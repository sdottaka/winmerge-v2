// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  FileFiltersDlg.cpp
 *
 * @brief Implementation of FileFilters -dialog
 */

#include "stdafx.h"
#include "FileFiltersDlg.h"
#include <vector>
#include "UnicodeString.h"
#include "Merge.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "FileFilterMgr.h"
#include "FileFilterHelper.h"
#include "paths.h"
#include "SharedFilterDlg.h"
#include "TestFilterDlg.h"
#include "FileOrFolderSelect.h"

using std::vector;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/** @brief Template file used when creating new filefilter. */
static const TCHAR FILE_FILTER_TEMPLATE[] = _T("FileFilter.tmpl");

/** @brief Location for filters specific help to open. */
static const TCHAR FilterHelpLocation[] = _T("::/htmlhelp/Filters.html");

/////////////////////////////////////////////////////////////////////////////
// CFiltersDlg dialog
IMPLEMENT_DYNCREATE(FileFiltersDlg, CTrPropertyPage)

/**
 * @brief Constructor.
 */
FileFiltersDlg::FileFiltersDlg() : CTrPropertyPage(FileFiltersDlg::IDD)
{
	m_strCaption = theApp.LoadDialogCaption(m_lpszTemplateName).c_str();
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;
	m_psp.hIcon = AfxGetApp()->LoadIcon(IDI_FILEFILTER);
	m_psp.dwFlags |= PSP_USEHICON;
}

void FileFiltersDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(FileFiltersDlg)
	DDX_Control(pDX, IDC_FILTERFILE_LIST, m_listFilters);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(FileFiltersDlg, CTrPropertyPage)
	//{{AFX_MSG_MAP(FileFiltersDlg)
	ON_BN_CLICKED(IDC_FILTERFILE_EDITBTN, OnFiltersEditbtn)
	ON_NOTIFY(NM_DBLCLK, IDC_FILTERFILE_LIST, OnDblclkFiltersList)
	ON_WM_MOUSEMOVE()
	ON_BN_CLICKED(IDC_FILTERFILE_TEST_BTN, OnBnClickedFilterfileTestButton)
	ON_BN_CLICKED(IDC_FILTERFILE_NEWBTN, OnBnClickedFilterfileNewbutton)
	ON_BN_CLICKED(IDC_FILTERFILE_DELETEBTN, OnBnClickedFilterfileDelete)
	ON_COMMAND(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_FILTERFILE_LIST, OnLvnItemchangedFilterfileList)
	ON_NOTIFY(LVN_GETINFOTIP, IDC_FILTERFILE_LIST, OnInfoTip)
	ON_BN_CLICKED(IDC_FILTERFILE_INSTALL, OnBnClickedFilterfileInstall)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFiltersDlg message handlers

/**
 * @brief Set array of filters.
 * @param [in] fileFilters Array of filters to show in the dialog.
 * @note Call this before actually showing the dialog.
 */
void FileFiltersDlg::SetFilterArray(const vector<FileFilterInfo>& fileFilters)
{
	m_Filters = fileFilters;
}

/**
 * @brief Returns path (cont. filename) of selected filter
 * @return Full path to selected filter file.
 */
String FileFiltersDlg::GetSelected()
{
	return m_sFileFilterPath;
}

/**
 * @brief Set path of selected filter.
 * @param [in] Path for selected filter.
 * @note Call this before actually showing the dialog.
 */
void FileFiltersDlg::SetSelected(const String & selected)
{
	m_sFileFilterPath = selected;
}

/**
 * @brief Initialise listcontrol containing filters.
 */
void FileFiltersDlg::InitList()
{
	// Show selection across entire row.
	// Also enable infotips.
	m_listFilters.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	String title = _("Name");
	m_listFilters.InsertColumn(0, title.c_str(), LVCFMT_LEFT, PointToPixel(112));
	title = _("Description");
	m_listFilters.InsertColumn(1, title.c_str(), LVCFMT_LEFT, PointToPixel(262));
	title = _("Location");
	m_listFilters.InsertColumn(2, title.c_str(), LVCFMT_LEFT, PointToPixel(262));

	title = _("<None>");
	m_listFilters.InsertItem(1, title.c_str());
	m_listFilters.SetItemText(0, 1, title.c_str());
	m_listFilters.SetItemText(0, 2, title.c_str());

	const int count = (int) m_Filters.size();

	for (int i = 0; i < count; i++)
	{
		AddToGrid(i);
	}
}

/**
 * @brief Select filter by index in the listview.
 * @param [in] index Index of filter to select.
 */
void FileFiltersDlg::SelectFilterByIndex(int index)
{
	m_listFilters.SetItemState(index, LVIS_SELECTED, LVIS_SELECTED);
	bool bPartialOk = false;
	m_listFilters.EnsureVisible(index, bPartialOk);
}

/**
 * @brief Called before dialog is shown.
 * @return Always TRUE.
 */
BOOL FileFiltersDlg::OnInitDialog()
{
	CTrPropertyPage::OnInitDialog();

	InitList();

	if (m_sFileFilterPath.empty())
	{
		SelectFilterByIndex(0);
		return TRUE;
	}

	int count = m_listFilters.GetItemCount();
	for (int i = 0; i < count; i++)
	{
		String desc = m_listFilters.GetItemText(i, 2);
		if (strutils::compare_nocase(desc, m_sFileFilterPath) == 0)
		{
			SelectFilterByIndex(i);
		}
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/**
 * @brief Add filter from filter-list index to dialog.
 * @param [in] filterIndex Index of filter to add.
 */
void FileFiltersDlg::AddToGrid(int filterIndex)
{
	const FileFilterInfo & filterinfo = m_Filters.at(filterIndex);
	const int item = filterIndex + 1;

	m_listFilters.InsertItem(item, filterinfo.name.c_str());
	m_listFilters.SetItemText(item, 1, filterinfo.description.c_str());
	m_listFilters.SetItemText(item, 2, filterinfo.fullpath.c_str());
}

/**
 * @brief Called when dialog is closed with "OK" button.
 */
void FileFiltersDlg::OnOK()
{
	int sel = m_listFilters.GetNextItem(-1, LVNI_SELECTED);
	m_sFileFilterPath = m_listFilters.GetItemText(sel, 2);

	CDialog::OnOK();
}

/**
 * @brief Open selected filter for editing.
 *
 * This opens selected file filter file for user to edit. Other WinMerge UI is
 * not (anymore) blocked during editing. We let user continue working with
 * WinMerge while editing filter(s). Before opening this dialog and before
 * doing directory compare we re-load changed filter files from disk. So we
 * always compare with latest saved filters.
 * @sa CMainFrame::OnToolsFilters()
 * @sa CDirDoc::Rescan()
 * @sa FileFilterHelper::ReloadUpdatedFilters()
 */
void FileFiltersDlg::OnFiltersEditbtn()
{
	int sel =- 1;

	sel = m_listFilters.GetNextItem(sel, LVNI_SELECTED);

	// Can't edit first "None"
	if (sel > 0)
	{
		String path = m_listFilters.GetItemText(sel, 2);
		EditFileFilter(path);
	}
}

/**
 * @brief Edit file filter in external editor.
 * @param [in] path Full path to file filter to edit.
 */
void FileFiltersDlg::EditFileFilter(const String& path)
{
	theApp.OpenFileToExternalEditor(path);
}

/**
 * @brief Edit selected filter when its double-clicked.
 * @param [in] pNMHDR List control item data.
 * @param [out] pResult Result of the action is returned in here.
 */
void FileFiltersDlg::OnDblclkFiltersList(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);

	OnFiltersEditbtn();
	*pResult = 0;
}

/**
 * @brief Is item in list the <None> item?
 * @param [in] item Item to test.
 * @return true if item is <None> item.
 */
bool FileFiltersDlg::IsFilterItemNone(int item) const
{
	String txtNone = _("<None>");
	String txt = m_listFilters.GetItemText(item, 0);

	return (strutils::compare_nocase(txt, txtNone) == 0);
}

/**
 * @brief Called when item state is changed.
 *
 * Disable Edit-button when "None" filter is selected.
 * @param [in] pNMHDR Listview item data.
 * @param [out] pResult Result of the action is returned in here.
 */
void FileFiltersDlg::OnLvnItemchangedFilterfileList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	// If item got selected
	if (pNMLV->uNewState & LVIS_SELECTED)
	{
		String txtNone = _("<None>");
		String txt = m_listFilters.GetItemText(pNMLV->iItem, 0);

		bool isNone = strutils::compare_nocase(txt, txtNone) == 0;

		EnableDlgItem(IDC_FILTERFILE_TEST_BTN, !isNone);
		EnableDlgItem(IDC_FILTERFILE_EDITBTN, !isNone);
		EnableDlgItem(IDC_FILTERFILE_DELETEBTN, !isNone);
	}
	*pResult = 0;
}

/**
 * @brief Called before infotip is shown to get infotip text.
 * @param [in] pNMHDR Listview item data.
 * @param [out] pResult Result of the action is returned in here.
 */
void FileFiltersDlg::OnInfoTip(NMHDR * pNMHDR, LRESULT * pResult)
{
	LVHITTESTINFO lvhti = {0};
	NMLVGETINFOTIP * pInfoTip = reinterpret_cast<NMLVGETINFOTIP*>(pNMHDR);
	ASSERT(pInfoTip != nullptr);

	// Get subitem under mouse cursor
	lvhti.pt = m_ptLastMousePos;
	m_listFilters.SubItemHitTest(&lvhti);

	if (lvhti.iSubItem > 1)
	{
		// Check that we are over icon or label
		if ((lvhti.flags & LVHT_ONITEMICON) || (lvhti.flags & LVHT_ONITEMLABEL))
		{
			// Set item text to tooltip
			String strText = m_listFilters.GetItemText(lvhti.iItem, lvhti.iSubItem);
			_tcscpy_s(pInfoTip->pszText, pInfoTip->cchTextMax, strText.c_str());
		}
	}
}

/**
 * @brief Track mouse position for showing tooltips.
 * @param [in] nFlags Mouse movement flags.
 * @param [in] point Current mouse position.
 */
void FileFiltersDlg::OnMouseMove(UINT nFlags, CPoint point) 
{
	m_ptLastMousePos = point;
	CDialog::OnMouseMove(nFlags, point);
}

/**
 * @brief Called when user presses "Test" button.
 *
 * Asks filename for new filter from user (using standard
 * file picker dialog) and copies template file to that
 * name. Opens new filterfile for editing.
 * @todo (At least) Warn if user puts filter to outside
 * filter directories?
 */
void FileFiltersDlg::OnBnClickedFilterfileTestButton()
{
	UpdateData(TRUE);

	int sel = m_listFilters.GetNextItem(-1, LVNI_SELECTED);
	if (sel == -1)
		return;
	if (IsFilterItemNone(sel))
		return;
	
	m_sFileFilterPath = m_listFilters.GetItemText(sel, 2);

	// Ensure filter is up-to-date (user probably just edited it)
	theApp.m_pGlobalFileFilter->ReloadUpdatedFilters();

	FileFilterMgr *pMgr = theApp.m_pGlobalFileFilter->GetManager();
	FileFilter * pFileFilter = pMgr->GetFilterByPath(m_sFileFilterPath);
	if (pFileFilter == nullptr)
		return;

	CTestFilterDlg dlg(this, pFileFilter, pMgr);
	dlg.DoModal();
}

/**
 * @brief Called when user presses "New..." button.
 *
 * Asks filename for new filter from user (using standard
 * file picker dialog) and copies template file to that
 * name. Opens new filterfile for editing.
 * @todo (At least) Warn if user puts filter to outside
 * filter directories?
 * @todo Can global filter path be empty (I think not - Kimmo).
 */
void FileFiltersDlg::OnBnClickedFilterfileNewbutton()
{
	String globalPath = theApp.m_pGlobalFileFilter->GetGlobalFilterPathWithCreate();
	String userPath = theApp.m_pGlobalFileFilter->GetUserFilterPathWithCreate();

	if (globalPath.empty() && userPath.empty())
	{
		AfxMessageBox(
			_("User's filter file folder is not defined!\n\nPlease select filter folder in Options/System.").c_str(), MB_ICONSTOP);
		return;
	}

	// Format path to template file
	String templatePath = paths::ConcatPath(globalPath, FILE_FILTER_TEMPLATE);

	if (paths::DoesPathExist(templatePath) != paths::IS_EXISTING_FILE)
	{
		String msg = strutils::format_string2(
			_("Cannot find file filter template file!\n\nPlease copy file %1 to WinMerge/Filters -folder:\n%2."),
			FILE_FILTER_TEMPLATE, templatePath);
		AfxMessageBox(msg.c_str(), MB_ICONERROR);
		return;
	}

	String path = globalPath.empty() ? userPath : globalPath;

	if (!globalPath.empty() && !userPath.empty())
	{
		CSharedFilterDlg dlg(
			GetOptionsMgr()->GetBool(OPT_FILEFILTER_SHARED) ? 
				CSharedFilterDlg::SHARED : CSharedFilterDlg::PRIVATE);
		if (dlg.DoModal() != IDOK)
			return;
		GetOptionsMgr()->SaveOption(OPT_FILEFILTER_SHARED, (dlg.GetSelectedFilterType() == CSharedFilterDlg::SHARED));
		path = dlg.GetSelectedFilterType() == CSharedFilterDlg::SHARED ? globalPath : userPath;
	}

	if (path.length())
		path = paths::AddTrailingSlash(path);
	
	String s;
	if (SelectFile(GetSafeHwnd(), s, false, path.c_str(), _("Select filename for new filter"),
		_("File Filters (*.flt)|*.flt|All Files (*.*)|*.*||")))
	{
		// Fix file extension
		TCHAR file[_MAX_FNAME] = {0};
		TCHAR ext[_MAX_EXT] = {0};
		TCHAR dir[_MAX_DIR] = {0};
		TCHAR drive[_MAX_DRIVE] = {0};
		_tsplitpath_s(s.c_str(), drive, _MAX_DRIVE, dir, _MAX_DIR, file, _MAX_FNAME, ext, _MAX_EXT);
		if (_tcslen(ext) == 0)
		{
			s += FileFilterExt;
		}
		else if (_tcsicmp(ext, FileFilterExt) != 0)
		{
			s = drive;
			s += dir;
			s += file;
			s += FileFilterExt;
		}

		// Open-dialog asks about overwriting, so we can overwrite filter file
		// user has already allowed it.
		if (!CopyFile(templatePath.c_str(), s.c_str(), FALSE))
		{
			String msg = strutils::format_string1(
				_( "Cannot copy filter template file to filter folder:\n%1\n\nPlease make sure the folder exists and is writable."),
				templatePath);
			AfxMessageBox(msg.c_str(), MB_ICONERROR);
			return;
		}
		EditFileFilter(s);
		FileFilterMgr *pMgr = theApp.m_pGlobalFileFilter->GetManager();
		int retval = pMgr->AddFilter(s);
		if (retval == FILTER_OK)
		{
			// Remove all from filterslist and re-add so we can update UI
			String selected;
			theApp.m_pGlobalFileFilter->LoadAllFileFilters();
			m_Filters = theApp.m_pGlobalFileFilter->GetFileFilters(selected);

			UpdateFiltersList();
		}
	}
}

/**
 * @brief Delete selected filter.
 */
void FileFiltersDlg::OnBnClickedFilterfileDelete()
{
	String path;
	int sel =- 1;

	sel = m_listFilters.GetNextItem(sel, LVNI_SELECTED);

	// Can't delete first "None"
	if (sel > 0)
	{
		path = m_listFilters.GetItemText(sel, 2);

		String sConfirm = strutils::format_string1(_("Are you sure you want to delete\n\n%1 ?"), path);
		int res = AfxMessageBox(sConfirm.c_str(), MB_ICONWARNING | MB_YESNO);
		if (res == IDYES)
		{
			if (DeleteFile(path.c_str()))
			{
				FileFilterMgr *pMgr = theApp.m_pGlobalFileFilter->GetManager();
				pMgr->RemoveFilter(path);
				
				// Remove all from filterslist and re-add so we can update UI
				String selected;
				m_Filters = theApp.m_pGlobalFileFilter->GetFileFilters(selected);

				UpdateFiltersList();
			}
			else
			{
				String msg = strutils::format_string1(
					_("Failed to delete the filter file:\n%1\n\nMaybe the file is read-only?"),
					path);
				AfxMessageBox(msg.c_str(), MB_ICONSTOP);
			}
		}
	}
}

/**
 * @brief Update filters to list.
 */
void FileFiltersDlg::UpdateFiltersList()
{
	int count = (int) m_Filters.size();

	m_listFilters.DeleteAllItems();

	String title = _("<None>");
	m_listFilters.InsertItem(1, title.c_str());
	m_listFilters.SetItemText(0, 1, title.c_str());
	m_listFilters.SetItemText(0, 2, title.c_str());

	for (int i = 0; i < count; i++)
	{
		AddToGrid(i);
	}
}

/**
 * @brief Open help from mainframe when user presses F1
 */
void FileFiltersDlg::OnHelp()
{
	theApp.ShowHelp(FilterHelpLocation);
}

/**
 * @brief Install new filter.
 * This function is called when user selects "Install" button from GUI.
 * Function allows easy installation of new filters for user. For example
 * when user has downloaded filter file from net. First we ask user to
 * select filter to install. Then we copy selected filter to private
 * filters folder.
 */
void FileFiltersDlg::OnBnClickedFilterfileInstall()
{
	String s;
	String path;
	String userPath = theApp.m_pGlobalFileFilter->GetUserFilterPathWithCreate();

	if (SelectFile(GetSafeHwnd(), s, true, path.c_str(),_("Locate filter file to install"),
		_("File Filters (*.flt)|*.flt|All Files (*.*)|*.*||")))
	{
		userPath = paths::ConcatPath(userPath, paths::FindFileName(s));
		if (!CopyFile(s.c_str(), userPath.c_str(), TRUE))
		{
			// If file already exists, ask from user
			// If user wants to, overwrite existing filter
			if (paths::DoesPathExist(userPath) == paths::IS_EXISTING_FILE)
			{
				int res = LangMessageBox(IDS_FILEFILTER_OVERWRITE, MB_YESNO |
					MB_ICONWARNING);
				if (res == IDYES)
				{
					if (!CopyFile(s.c_str(), userPath.c_str(), FALSE))
					{
						LangMessageBox(IDS_FILEFILTER_INSTALLFAIL, MB_ICONSTOP);
					}
				}
			}
			else
			{
				LangMessageBox(IDS_FILEFILTER_INSTALLFAIL, MB_ICONSTOP);
			}
		}
		else
		{
			FileFilterMgr *pMgr = theApp.m_pGlobalFileFilter->GetManager();
			pMgr->AddFilter(userPath);

			// Remove all from filterslist and re-add so we can update UI
			String selected;
			m_Filters = theApp.m_pGlobalFileFilter->GetFileFilters(selected);

			UpdateFiltersList();
		}
	}
}
