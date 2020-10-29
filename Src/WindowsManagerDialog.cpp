// WindowsManagerDialog.cpp : implementation file
//

#include "stdafx.h"
#include "Merge.h"
#include "WindowsManagerDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWindowsManagerDialog dialog

CWindowsManagerDialog::CWindowsManagerDialog(CWnd* pParent/* = NULL*/)
	: CDialog(CWindowsManagerDialog::IDD, pParent)
	,m_bAutoCleanup(FALSE)
	,m_pFrame(NULL)
	,m_pIL(NULL)
{
	//{{AFX_DATA_INIT(CWindowsManagerDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CWindowsManagerDialog::~CWindowsManagerDialog()
{
	if (NULL != m_pIL->GetSafeHandle())
		m_pIL->DeleteImageList();
	if (NULL != m_pIL)
		delete m_pIL;
}

void CWindowsManagerDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWindowsManagerDialog)
	// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_LIST_FILE, m_List);
}

BEGIN_MESSAGE_MAP(CWindowsManagerDialog, CDialog)
	//{{AFX_MSG_MAP(CWindowsManagerDialog)
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_WM_ACTIVATE()
	ON_WM_DESTROY()
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST_FILE, &CWindowsManagerDialog::OnNMCustomdrawListFile)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_FILE, &CWindowsManagerDialog::OnNMDblclkListFile)
	//}}AFX_MSG_MAP
	ON_MESSAGE(WMU_ISOPEN, &CWindowsManagerDialog::OnIsOpen)
	ON_MESSAGE(WMU_SELECTNEXT, &CWindowsManagerDialog::OnSelectNext)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWindowsManagerDialog message handlers

BOOL CWindowsManagerDialog::PreTranslateMessage(MSG* pMsg)
{
	// TODO: Add your specialized code here and/or call the base class

	if ((WM_KEYUP == pMsg->message && VK_CONTROL == pMsg->wParam) || 
		(WM_KEYDOWN == pMsg->message && VK_ESCAPE == pMsg->wParam))
	{
		PostMessage(WM_CLOSE);
	}

	if (WM_KEYDOWN == pMsg->message && VK_TAB == pMsg->wParam)
	{
		PostMessage(WMU_SELECTNEXT, (GetAsyncKeyState(VK_SHIFT) < 0) ? 1 : 0, 0);
	}

	return CDialog::PreTranslateMessage(pMsg);
}

BOOL CWindowsManagerDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO: Add extra initialization here

	m_List.SetExtendedStyle(LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);

	if (NULL == m_pIL)
		m_pIL = new CImageList();

	m_pIL->DeleteImageList();
	m_pIL->Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 1);
	m_List.SetImageList(m_pIL, LVSIL_SMALL);
	m_List.SetBkColor(WMD_LISTCOLOR_BKG);
	m_List.SetTextBkColor(WMD_LISTCOLOR_BKG);

	PopulateList();

	if(m_List.GetItemCount() > 0)
		m_List.SetItemState((1 == m_List.GetItemCount()) ? 0 : 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	AdjustSize();
	CenterWindow(m_pFrame);	// please initialize the parent window before creating dialog

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CWindowsManagerDialog::PopulateList()
{
	if (m_List.GetItemCount() <= 0)
	{
		CRect rect;
		m_List.GetWindowRect(rect);
		m_List.InsertColumn(0, _T("Files"), LVCFMT_LEFT, rect.Width());
	}

	m_List.DeleteAllItems();

	CString sText;
	const CTypedPtrArray<CPtrArray, CMDIChildWnd*>* pArrChild = m_pFrame->GetChildArray();
	for (int i = 0; i < pArrChild->GetSize(); ++i)
	{
		HICON hIcon = pArrChild->GetAt(i)->GetIcon(TRUE);
		if (NULL == hIcon)
			hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
		m_pIL->Add(hIcon);
		sText = pArrChild->GetAt(i)->GetActiveDocument()->GetPathName();
		if(sText.IsEmpty())
			pArrChild->GetAt(i)->GetWindowText(sText);
		m_List.InsertItem(i, sText, m_pIL->GetImageCount() - 1);
		m_List.SetItemData(i, (DWORD_PTR)pArrChild->GetAt(i));
	}
}
// adjust size to listctrl column and dialog
void CWindowsManagerDialog::AdjustSize()
{
	CRect rect;
	m_List.GetItemRect(0, rect, LVIR_ICON);

	const int nImgWidth = rect.right - rect.left;
	const int nSpaceWidth = m_List.GetStringWidth(_T(" "));
	const int nLeftMargin = GetSystemMetrics(SM_CXFRAME) * 2 + nSpaceWidth * 4;

	int nMaxWidth = -1;

	CString sText;
	CRect _rc(0, 0, 0, 0);
	for (int i = 0; i < m_List.GetItemCount(); ++i)
	{
		sText = m_List.GetItemText(i, 0);
		int nWidth = m_List.GetStringWidth(sText);
		if (nWidth > nMaxWidth)
			nMaxWidth = nWidth;
		_rc.bottom += rect.bottom - rect.top;
	}

	_rc.right = nMaxWidth + nImgWidth + nLeftMargin + 2 * ::GetSystemMetrics(SM_CYVSCROLL);
	m_List.SetColumnWidth(0, _rc.right - ::GetSystemMetrics(SM_CYFRAME));

	//if the tasklist exceeds the height of the display, leave some space at the bottom
	if (_rc.bottom > ::GetSystemMetrics(SM_CYSCREEN) - 50)
	{
		_rc.bottom = ::GetSystemMetrics(SM_CYSCREEN) - 50;
		m_List.SetColumnWidth(0, _rc.right - ::GetSystemMetrics(SM_CYFRAME) - ::GetSystemMetrics(SM_CYVSCROLL));
	}
	// Task List's border is 1px smaller than ::GetSystemMetrics(SM_CYFRAME) returns
	_rc.bottom += ::GetSystemMetrics(SM_CYFRAME);
	MoveWindow(_rc, FALSE);
}

void CWindowsManagerDialog::SetParentWnd(CWnd* pWnd)
{
	ASSERT(NULL != pWnd);
	m_pFrame = DYNAMIC_DOWNCAST(CMainFrame, pWnd);
}

void CWindowsManagerDialog::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here

	if(NULL != m_List.GetSafeHwnd() && cx > 0 && cy > 0)
		m_List.MoveWindow(0, 0, cx, cy);
}

void CWindowsManagerDialog::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	CDialog::OnActivate(nState, pWndOther, bMinimized);

	if (WA_INACTIVE == nState)
		PostMessage(WM_CLOSE);
}

void CWindowsManagerDialog::OnClose()
{
	// TODO: Add your message handler code here and/or call default

	if (m_bAutoCleanup)
		DestroyWindow();
	else
		CDialog::OnClose();
}

void CWindowsManagerDialog::OnOK()
{
	// TODO: Add extra validation here

	if (m_bAutoCleanup)
		DestroyWindow();
	else
		CDialog::OnOK();
}

void CWindowsManagerDialog::OnCancel()
{
	// TODO: Add extra cleanup here

	if (m_bAutoCleanup)
		DestroyWindow();
	else
		CDialog::OnCancel();
}

void CWindowsManagerDialog::PostNcDestroy()
{
	// TODO: Add your specialized code here and/or call the base class
	CDialog::PostNcDestroy();

	if (m_bAutoCleanup)
		delete this;
}

void CWindowsManagerDialog::OnDestroy()
{
	CDialog::OnDestroy();

	// TODO: Add your message handler code here

	const int nIndex = m_List.GetNextItem(-1, LVNI_SELECTED);
	if (nIndex >= 0 && nIndex < m_List.GetItemCount())
		::PostMessage(AfxGetMainWnd()->GetSafeHwnd(), WMU_CHILDFRAMEACTIVATE, 0, (LPARAM)m_List.GetItemData(nIndex));
}

LRESULT CWindowsManagerDialog::OnIsOpen(WPARAM wParam, LPARAM lParam)
{
	return 1;
}

LRESULT CWindowsManagerDialog::OnSelectNext(WPARAM wParam, LPARAM lParam)
{
	int nIndex = m_List.GetNextItem(-1, LVNI_SELECTED);
	if (wParam)		// reverse order
	{
		if (0 == nIndex)
			nIndex = m_List.GetItemCount() - 1;
		else
			nIndex--;
	}
	else
	{
		if (nIndex == m_List.GetItemCount() - 1)
			nIndex = 0;
		else
			nIndex++;
	}

	m_List.SetItemState(nIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	m_List.EnsureVisible(nIndex, FALSE);

	return 1;
}

void CWindowsManagerDialog::OnNMCustomdrawListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
//	LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	// TODO: Add your control notification handler code here

	switch (pLVCD->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
		if (CDIS_FOCUS == (pLVCD->nmcd.uItemState & CDIS_FOCUS))
		{
			pLVCD->nmcd.uItemState = CDIS_DEFAULT;
			pLVCD->clrTextBk = WMD_LISTCOLOR_BKGSEL;
		}
		*pResult = CDRF_DODEFAULT;
		break;
	default:
		*pResult = CDRF_DODEFAULT;
		break;
	}
}

void CWindowsManagerDialog::OnNMDblclkListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;

	PostMessage(WM_CLOSE);
}
