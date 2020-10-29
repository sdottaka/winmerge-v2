// CSuperComboBox.cpp : implementation file
//

#include "StdAfx.h"
#include "SuperComboBox.h"
#include <vector>
#include "DropHandler.h"

// Wrap placement new to avoid the need to temporarily #undef new
template<typename T>
T &placement_cast(void *p)
{
	return *new(p) T;
}

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define DEF_MAXSIZE		20	// default maximum items to retain in each SuperComboBox


/////////////////////////////////////////////////////////////////////////////
// CSuperComboBox

std::map<int, HIMAGELIST> CSuperComboBox::m_himlSystemMap;

CSuperComboBox::CSuperComboBox()
	: m_pDropHandler(nullptr)
	, m_bInEditchange(false)
	, m_bDoComplete(false)
	, m_bAutoComplete(false)
	, m_bHasImageList(false)
	, m_bComboBoxEx(false)
	, m_bExtendedFileNames(false)
	, m_bCanBeEmpty(false)
	, m_nMaxItems(DEF_MAXSIZE)
{


	// Initialize OLE libraries if not yet initialized
	m_bMustUninitOLE = false;
	_AFX_THREAD_STATE* pState = AfxGetThreadState();
	if (!pState->m_bNeedTerm)
	{
		SCODE sc = ::OleInitialize(nullptr);
		if (FAILED(sc))
			AfxMessageBox(_T("OLE initialization failed. Make sure that the OLE libraries are the correct version"));
		else
			m_bMustUninitOLE = true;
	}
}

CSuperComboBox::~CSuperComboBox()
{
	// Uninitialize OLE support
	if (m_bMustUninitOLE)
		::OleUninitialize();
}

BEGIN_MESSAGE_MAP(CSuperComboBox, CComboBoxEx)
	//{{AFX_MSG_MAP(CSuperComboBox)
	ON_CONTROL_REFLECT_EX(CBN_EDITCHANGE, OnEditchange)
	ON_CONTROL_REFLECT_EX(CBN_SETFOCUS, OnSetfocus)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_DRAWITEM()
	ON_NOTIFY_REFLECT(CBEN_GETDISPINFO, OnGetDispInfo)
	ON_MESSAGE(WM_DPICHANGED_BEFOREPARENT, OnDpiChangedBeforeParent)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSuperComboBox message handlers

void CSuperComboBox::PreSubclassWindow()
{
	UpdateDpi();
	__super::PreSubclassWindow();
	m_pDropHandler = new DropHandler(std::bind(&CSuperComboBox::OnDropFiles, this, std::placeholders::_1));
	RegisterDragDrop(m_hWnd, m_pDropHandler);
	
	TCHAR szClassName[256];
	GetClassName(m_hWnd, szClassName, sizeof(szClassName)/sizeof(szClassName[0]));
	if (lstrcmpi(_T("ComboBoxEx32"), szClassName) == 0)
		m_bComboBoxEx = true;
}

/**
 * @brief Sets additional state for handling Extended Length file names.
 */
void CSuperComboBox::SetFileControlStates(bool bCanBeEmpty /*= false*/, int nMaxItems /*= -1*/)
{
	ASSERT(m_bComboBoxEx);

	m_bExtendedFileNames = true;
	m_bCanBeEmpty = bCanBeEmpty;
	if (nMaxItems > 0)
		m_nMaxItems = nMaxItems;
}

/**
 * @brief Adds a string to the list box of a combo box
 * @param lpszItem Pointer to the null-terminated string that is to be added. 
 */
int CSuperComboBox::AddString(LPCTSTR lpszItem)
{
	return InsertString(GetCount(), lpszItem);
}

/**
 * @brief Inserts a string into the list box of a combo box.
 * @param nIndex The zero-based index to the position in the list box that receives the string.
 * @param lpszItem Pointer to the null-terminated string that is to be added. 
 */
int CSuperComboBox::InsertString(int nIndex, LPCTSTR lpszItem)
{
	if (m_bComboBoxEx)
	{
		CString sShortName;		// scoped to remain valid for calling __super::InsertItem()
		if (m_bExtendedFileNames)
		{
			if (nIndex >= static_cast<int>(m_sFullStateText.size()))
				m_sFullStateText.resize(nIndex + 10);
			sShortName = m_sFullStateText[nIndex] = lpszItem;

			const int nPartLen = 72;
			if (sShortName.GetLength() > (nPartLen*2+8)) 
			{
				if (sShortName.Left(4) == _T("\\\\?\\"))
					sShortName.Delete(0, 4);
				else
				if (sShortName.Left(8) == _T("\\\\?\\UNC\\"))
					sShortName.Delete(1, 6);
				CString sL = sShortName.Left(nPartLen);
				int nL = sL.ReverseFind(_T('\\'));
				if (nL > 0) sL = sL.Left(nL+1);

				CString sR = sShortName.Right(nPartLen);
				int nR = sR.Find(_T('\\'));
				if (nR > 0) sR = sR.Right(sR.GetLength() - nR);

				sShortName = sL + _T(" ... ") + sR;
				lpszItem = (LPCTSTR)sShortName;
			}
		}
		COMBOBOXEXITEM cbitem = {0};
		cbitem.mask = CBEIF_TEXT |
			(m_bHasImageList ? CBEIF_IMAGE|CBEIF_SELECTEDIMAGE : 0);
		cbitem.pszText = (LPTSTR)lpszItem;
		cbitem.cchTextMax = (int)_tcslen(lpszItem);
		cbitem.iItem = nIndex;
		cbitem.iImage = I_IMAGECALLBACK;
		cbitem.iSelectedImage = I_IMAGECALLBACK;
		return __super::InsertItem(&cbitem);
	}
	else
	{
		return CComboBox::InsertString(nIndex, lpszItem);
	}
}

int CSuperComboBox::DeleteString(int nIndex)
{
	if (m_bComboBoxEx && m_bExtendedFileNames &&
	    nIndex >= 0 && nIndex < static_cast<int>(m_sFullStateText.size()))
	{
		m_sFullStateText.erase(m_sFullStateText.begin() + nIndex);
	}
	return __super::DeleteString(nIndex);
}

int CSuperComboBox::FindString(int nStartAfter, LPCTSTR lpszString) const
{
	
	if (m_bComboBoxEx)
	{
		ASSERT(m_bExtendedFileNames);
		CString sSearchString = lpszString;
		int nSearchStringLen = sSearchString.GetLength();
		if (nSearchStringLen <= 0)
			return CB_ERR;
		int nLimit = static_cast<int>(m_sFullStateText.size());
		for (int i = nStartAfter+1; i < nLimit; i++)
		{
			CString sListString = m_sFullStateText[i];
			int nListStringLen = sListString.GetLength();
			if (nSearchStringLen <= nListStringLen && sSearchString.CompareNoCase(sListString.Left(nSearchStringLen))==0)
				return i;
		}
		return CB_ERR;
	}
	else
	{
		return CComboBox::FindString(nStartAfter, lpszString);
	}
}

/**
 * @brief Gets the system image list and attaches the image list to a combo box control.
 */
bool CSuperComboBox::AttachSystemImageList()
{
	HIMAGELIST hImageList = nullptr;
	ASSERT(m_bComboBoxEx);
	if (m_himlSystemMap.find(m_dpi) == m_himlSystemMap.end())
	{
		hImageList = DpiAware::LoadShellImageList(m_dpi);
		if (hImageList == nullptr)
			return false;
		m_himlSystemMap.emplace(m_dpi, hImageList);
	}
	else
		hImageList = m_himlSystemMap[m_dpi];
	SetImageList(CImageList::FromHandle(hImageList));
	m_bHasImageList = true;
	return true;
}

void CSuperComboBox::LoadState(LPCTSTR szRegSubKey)
{
	ResetContent();

	int cnt = AfxGetApp()->GetProfileInt(szRegSubKey, _T("Count"), 0);
	int idx = 0;
	for (int i=0; i < cnt && idx < m_nMaxItems; i++)
	{
		CString s,s2;
		s2.Format(_T("Item_%d"), i);
		s = AfxGetApp()->GetProfileString(szRegSubKey, s2);
		if (FindStringExact(-1, s) == CB_ERR && !s.IsEmpty())
		{
			AddString(s);
			idx++;
		}
	}
	if (idx > 0)
	{
		bool bIsEmpty = (m_bCanBeEmpty ? (AfxGetApp()->GetProfileInt(szRegSubKey, _T("Empty"), FALSE) == TRUE) : false);
		if (bIsEmpty)
		{
			SetCurSel(-1);
		}
		else
		{
			SetCurSel(0);
			if (m_bExtendedFileNames)
				GetEditCtrl()->SetWindowText(m_sFullStateText[0]);
		}
	}
}

void CSuperComboBox::GetLBText(int nIndex, CString &rString) const
{
	ASSERT(::IsWindow(m_hWnd));

	if (m_bExtendedFileNames)
	{
		rString = m_sFullStateText[nIndex];
	}
	else
	{
		__super::GetLBText(nIndex, rString.GetBufferSetLength(GetLBTextLen(nIndex)));
		rString.ReleaseBuffer();
	}
}

int CSuperComboBox::GetLBTextLen(int nIndex) const
{
	if (m_bExtendedFileNames)
	{
		return m_sFullStateText[nIndex].GetLength();
	}
	else
	{
		return __super::GetLBTextLen(nIndex);
	}
}

/** 
 * @brief Saves strings in combobox.
 * This function saves strings in combobox, in editbox and in dropdown.
 * Whitespace characters are stripped from begin and end of the strings
 * before saving. Empty strings are not saved. So strings which have only
 * whitespace characters aren't save either.
 * @param [in] szRegSubKey Registry subkey where to save strings.
 * @param [in] bCanBeEmpty
 * @param [in] nMaxItems Max number of strings to save.
 */
void CSuperComboBox::SaveState(LPCTSTR szRegSubKey)
{
	CString strItem;
	if (m_bComboBoxEx)
		GetEditCtrl()->GetWindowText(strItem);
	else
		GetWindowText(strItem);
	strItem.TrimLeft();
	strItem.TrimRight();

	int idx = 0;
	if (!strItem.IsEmpty())
	{
		AfxGetApp()->WriteProfileString(szRegSubKey, _T("Item_0"), strItem);
		idx=1;
	}

	int cnt = GetCount();
	for (int i=0; i < cnt && idx < m_nMaxItems; i++)
	{		
		CString s;
		GetLBText(i, s);
		s.TrimLeft();
		s.TrimRight();
		if (s != strItem && !s.IsEmpty())
		{
			CString s2;
			s2.Format(_T("Item_%d"), idx);
			AfxGetApp()->WriteProfileString(szRegSubKey, s2, s);
			idx++;
		}
	}
	AfxGetApp()->WriteProfileInt(szRegSubKey, _T("Count"), idx);
	
	if (m_bCanBeEmpty)
		AfxGetApp()->WriteProfileInt(szRegSubKey, _T("Empty"), strItem.IsEmpty());
}

BOOL CSuperComboBox::OnEditchange()
{
	if (m_bHasImageList)
	{
		// Trigger a WM_WINDOWPOSCHANGING to help the client area receive an update trough WM_DRAWITEM
		SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE);
	}

	// bail if not auto completing 
	if (!m_bDoComplete) 
		return FALSE;
	
	int length = GetWindowTextLength();

	// bail if no text
	if (length <= 0) 
		return FALSE;
	
	if (m_bInEditchange)
		return FALSE;
	m_bInEditchange = true;

	// Get the text in the edit box
	CString s;
	GetWindowText(s);
	
	// get the current selection
	DWORD sel = GetEditSel();
	int start = (short)LOWORD(sel), end = (short)HIWORD(sel);
	
	// look for the string that is prefixed by the typed text
	int idx = FindString(-1, s);
	if (idx != CB_ERR)
	{
		// set the new string
		CString strNew;
		GetLBText(idx, strNew);
		SetWindowText(strNew);
	}
	
	// select the text after our typing
	if (sel == CB_ERR || end >= length)
	{
		start = length;
		end = -1;
	}

	// get the caret back in the right spot
	if (m_bComboBoxEx)
		GetEditCtrl()->SetSel(start, end);
	else
		CComboBox::SetEditSel(start, end);  

	m_bInEditchange = false;

	return FALSE;
}

BOOL CSuperComboBox::OnSetfocus()
{
	if (m_bHasImageList)
		GetEditCtrl()->SetModify(FALSE);

	return FALSE;
}

BOOL CSuperComboBox::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
		int nVirtKey = (int) pMsg->wParam;
		// If Shift+Del pressed when dropdown is open, delete selected item
		// from dropdown list
		if (GetAsyncKeyState(VK_SHIFT))
		{
			if (GetDroppedState() && nVirtKey == VK_DELETE)
			{
				int cursel = GetCurSel();
				if (cursel != CB_ERR)
				{
					DeleteString(cursel);
					if (cursel >= GetCount())
						cursel = GetCount() - 1;
					if (cursel >= 0)
						SetCurSel(cursel);
				}
				return FALSE; // No need to further handle this message
			}
		}
		if (m_bAutoComplete)
		{
			m_bDoComplete = true;

			if (nVirtKey == VK_DELETE || nVirtKey == VK_BACK)
					m_bDoComplete = false;
		}
    }

    return __super::PreTranslateMessage(pMsg);
}

void CSuperComboBox::SetAutoComplete(INT nSource)
{
	switch (nSource)
	{
		case AUTO_COMPLETE_DISABLED:
			m_bAutoComplete = false;
			break;

		case AUTO_COMPLETE_FILE_SYSTEM:
		{
			// Disable the build-in auto-completion and use the Windows
			// shell functionality.
			m_bAutoComplete = false;

			// ComboBox's edit control is alway 1001.
			CWnd *pWnd = m_bComboBoxEx ? this->GetEditCtrl() : GetDlgItem(1001);
			ASSERT(pWnd != nullptr);
			SHAutoComplete(pWnd->m_hWnd, SHACF_FILESYSTEM);
			break;
		}

		case AUTO_COMPLETE_RECENTLY_USED:
			m_bAutoComplete = true;
			break;

		default:
			ASSERT(!"Unknown AutoComplete source.");
			m_bAutoComplete = false;
	}
}

void CSuperComboBox::ResetContent()
{
	if (m_bExtendedFileNames)
	{
		m_sFullStateText.resize(m_nMaxItems);
		for (int i = 0; i < m_nMaxItems; i++)
		{
			m_sFullStateText[i] = _T("");
		}
	}
	__super::ResetContent();
}

int CSuperComboBox::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	m_pDropHandler = new DropHandler(std::bind(&CSuperComboBox::OnDropFiles, this, std::placeholders::_1));
	RegisterDragDrop(m_hWnd, m_pDropHandler);
	return 0;
}

void CSuperComboBox::OnDestroy(void)
{
	if (m_pDropHandler != nullptr)
		RevokeDragDrop(m_hWnd);
}

/////////////////////////////////////////////////////////////////////////////
//
//	OnDropFiles code from CDropEdit
//	Copyright 1997 Chris Losinger
//
//	shortcut expansion code modified from :
//	CShortcut, 1996 Rob Warner
//
void CSuperComboBox::OnDropFiles(const std::vector<String>& files)
{
	GetParent()->SendMessage(WM_COMMAND, GetDlgCtrlID() +
		(CBN_EDITUPDATE << 16), (LPARAM)m_hWnd);
	SetWindowText(files[0].c_str());
	GetParent()->SendMessage(WM_COMMAND, GetDlgCtrlID() +
		(CBN_EDITCHANGE << 16), (LPARAM)m_hWnd);
}

static DWORD WINAPI SHGetFileInfoThread(LPVOID pParam)
{
	CString &sPath = reinterpret_cast<CString &>(pParam);
	SHFILEINFO sfi = {0};
	// If SHGetFileInfo() fails, intentionally leave sfi.iIcon as 0 (indicating
	// a file of inspecific type) so as to not obstruct CBEIF_DI_SETITEM logic.
	if (!sPath.IsEmpty())
		SHGetFileInfo(sPath, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX);
	sPath.~CString();
	return sfi.iIcon;
}

static int GetFileTypeIconIndex(LPVOID pParam)
{
	CString &sText = reinterpret_cast<CString &>(pParam);
	DWORD dwIconIndex = 0;
	bool isNetworkDrive = false;
	if (sText.GetLength() >= 2 && (sText[1] == L'\\'))
	{
		if (sText.GetLength() > 4 && sText.Left(4) == L"\\\\?\\")
			if (sText.GetLength() > 8 && sText.Left(8) == L"\\\\?\\UNC\\")
				isNetworkDrive = true;
			else
				isNetworkDrive = false;	// Just a Long File Name indicator
		else
			isNetworkDrive = true;
	}
	else
	if (sText.GetLength() >= 3 && GetDriveType(sText.Left(3)) == DRIVE_REMOTE)
		isNetworkDrive = true;	// Drive letter, but mapped to Remote UNC device.

	// Unless execution drops into the final else block,
	// SHGetFileInfoThread() takes ownership of, and will eventually trash sText
	if (!isNetworkDrive)
	{
		dwIconIndex = SHGetFileInfoThread(pParam);
	}
	else
	if (HANDLE hThread = CreateThread(nullptr, 0, SHGetFileInfoThread, pParam, 0, nullptr))
	{
		// The path is a network path. 
		// Try to get the index of a system image list icon, with 1-sec timeout.

		DWORD dwResult = WaitForSingleObject(hThread, 1000);
		if (dwResult == WAIT_OBJECT_0)
		{
			GetExitCodeThread(hThread, &dwIconIndex);
		}
		CloseHandle(hThread);
	}
	else
	{
		// Ownership of sText was retained, so trash it here
		sText.~CString();
	}
	return static_cast<int>(dwIconIndex);
}

void CSuperComboBox::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if ((lpDrawItemStruct->itemState & ODS_COMBOBOXEDIT) != 0 && m_bHasImageList)
	{
		LPVOID pvText;
		CString &sText = placement_cast<CString>(&pvText);
		CEdit *const pEdit = GetEditCtrl();
		if (!pEdit->GetModify() || GetFocus() != pEdit)
			GetWindowText(sText);
		int iIcon = GetFileTypeIconIndex(pvText);
		ImageList_DrawEx(m_himlSystemMap[m_dpi], iIcon, lpDrawItemStruct->hDC,
			lpDrawItemStruct->rcItem.left, lpDrawItemStruct->rcItem.top,
			0, 0, GetSysColor(COLOR_WINDOW), CLR_NONE, ILD_NORMAL);
		return;
	}
	__super::OnDrawItem(nIDCtl, lpDrawItemStruct);
}

/**
 * @brief A message handler for CBEN_GETDISPINFO message
 */
void CSuperComboBox::OnGetDispInfo(NMHDR *pNotifyStruct, LRESULT *pResult)
{
	NMCOMBOBOXEX *pDispInfo = (NMCOMBOBOXEX *)pNotifyStruct;
	if (pDispInfo && pDispInfo->ceItem.pszText && m_bHasImageList)
	{
		pDispInfo->ceItem.mask |= CBEIF_DI_SETITEM;
		LPVOID pvText;
		GetLBText(static_cast<int>(pDispInfo->ceItem.iItem), placement_cast<CString>(&pvText));
		int iIcon = GetFileTypeIconIndex(pvText);
		pDispInfo->ceItem.iImage = iIcon;
		pDispInfo->ceItem.iSelectedImage = iIcon;
	}
	*pResult = 0;
}

LRESULT CSuperComboBox::OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam)
{
	__super::OnDpiChangedBeforeParent(wParam, lParam);
	if (m_bHasImageList)
	{
		if (m_himlSystemMap.find(m_dpi) == m_himlSystemMap.end())
		{
			HIMAGELIST hImageList = DpiAware::LoadShellImageList(m_dpi);
			if (!hImageList)
				return 0;
			m_himlSystemMap.emplace(m_dpi, hImageList);
		}
		SetImageList(CImageList::FromHandle(m_himlSystemMap[m_dpi]));
	}
	return 0;
}
