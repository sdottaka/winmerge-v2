/** 
 * @file PreferencesDlg.cpp
 *
 * @brief Implementation file for CPreferencesDlg
 *
 * @note This code originates from AbstractSpoon / TodoList
 * (http://www.abstractspoon.com/) but is modified to use in
 * WinMerge.
 */

#include "StdAfx.h"
#include "PreferencesDlg.h"
#include "resource.h"
#include "UnicodeString.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "SyntaxColors.h"
#include "Merge.h"
#include "paths.h"
#include "FileOrFolderSelect.h"
#include "OptionsSyntaxColors.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/**
 * @brief Location for file compare specific help to open.
 */
static TCHAR OptionsHelpLocation[] = _T("::/htmlhelp/Configuration.html");

/////////////////////////////////////////////////////////////////////////////
// CPreferencesDlg dialog

const TCHAR PATHDELIM = '>';

CPreferencesDlg::CPreferencesDlg(COptionsMgr *regOptions, SyntaxColors *colors,
		UINT nMenuID /*= 0*/, CWnd* pParent /*= nullptr*/)   // standard constructor
: CTrDialog(IDD_PREFERENCES, pParent)
, m_pOptionsMgr(regOptions)
, m_pSyntaxColors(colors)
, m_pageGeneral(regOptions)
, m_pageCompare(regOptions)
, m_pageColorSchemes(regOptions)
, m_pageMergeColors(regOptions)
, m_pageTextColors(regOptions, colors)
, m_pageSyntaxColors(regOptions, colors)
, m_pageMarkerColors(regOptions, colors)
, m_pageDirColors(regOptions)
, m_pageArchive(regOptions)
, m_pageCodepage(regOptions)
, m_pageEditor(regOptions)
, m_pageSystem(regOptions)
, m_pageBackups(regOptions)
, m_pageShell(regOptions)
, m_pageCompareFolder(regOptions)
, m_pageCompareTable(regOptions)
, m_pageCompareBinary(regOptions)
, m_pageCompareImage(regOptions)
{
	UNREFERENCED_PARAMETER(nMenuID);
}

CPreferencesDlg::~CPreferencesDlg()
{
}

void CPreferencesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPreferencesDlg)
	DDX_Control(pDX, IDC_TREEOPT_PAGES, m_tcPages);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPreferencesDlg, CTrDialog)
	//{{AFX_MSG_MAP(CPreferencesDlg)
	ON_WM_SIZE()
	ON_COMMAND(ID_HELP, OnHelpButton)
	ON_BN_CLICKED(IDC_TREEOPT_HELP, OnHelpButton)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREEOPT_PAGES, OnSelchangedPages)
	ON_BN_CLICKED(IDC_TREEOPT_IMPORT, OnImportButton)
	ON_BN_CLICKED(IDC_TREEOPT_EXPORT, OnExportButton)
	ON_MESSAGE(WM_APP + IDC_COLOR_SCHEMES, OnColorSchemeChanged)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPreferencesDlg message handlers

BOOL CPreferencesDlg::OnInitDialog()
{
	CTrDialog::OnInitDialog();

	m_tcPages.SetIndent(0);

	// Second parameter is 'path', page's parent page(s) and caption.
	// '>' is used as path separator.
	// For example "General" creates top-level "General" page
	// and "General>Colors" creates "Colors" sub-page for "General"
	AddPage(&m_pageGeneral, IDS_OPTIONSPG_GENERAL);
	AddPage(&m_pageCompare, IDS_OPTIONSPG_COMPARE, IDS_OPTIONSPG_GENCOMPARE);
	AddPage(&m_pageCompareFolder, IDS_OPTIONSPG_COMPARE, IDS_OPTIONSPG_FOLDERCOMPARE);
	AddPage(&m_pageCompareTable, IDS_OPTIONSPG_COMPARE, IDS_OPTIONSPG_TABLECOMPARE);
	AddPage(&m_pageCompareBinary, IDS_OPTIONSPG_COMPARE, IDS_OPTIONSPG_BINARYCOMPARE);
	AddPage(&m_pageCompareImage, IDS_OPTIONSPG_COMPARE, IDS_OPTIONSPG_IMAGECOMPARE);
	AddPage(&m_pageEditor, IDS_OPTIONSPG_EDITOR);
	AddPage(&m_pageColorSchemes, IDS_OPTIONSPG_COLORS, IDS_OPTIONSPG_COLOR_SCHEMES);
	AddPage(&m_pageMergeColors, IDS_OPTIONSPG_COLORS, IDS_OPTIONSPG_MERGECOLORS);
	AddPage(&m_pageSyntaxColors, IDS_OPTIONSPG_COLORS, IDS_OPTIONSPG_SYNTAXCOLORS);
	AddPage(&m_pageTextColors, IDS_OPTIONSPG_COLORS, IDS_OPTIONSPG_TEXTCOLORS);
	AddPage(&m_pageMarkerColors, IDS_OPTIONSPG_COLORS, IDS_OPTIONSPG_MARKERCOLORS);
	AddPage(&m_pageDirColors, IDS_OPTIONSPG_COLORS, IDS_OPTIONSPG_DIRCOLORS);
	AddPage(&m_pageArchive, IDS_OPTIONSPG_ARCHIVE);
	AddPage(&m_pageSystem, IDS_OPTIONSPG_SYSTEM);
	AddPage(&m_pageBackups, IDS_OPTIONSPG_BACKUPS);
	AddPage(&m_pageCodepage, IDS_OPTIONSPG_CODEPAGE);
	AddPage(&m_pageShell, IDS_OPTIONSPG_SHELL);

	ReadOptions();
	
	CRect rPPHost;
	GetDlgItem(IDC_TREEOPT_HOSTFRAME)->GetWindowRect(rPPHost);
	ScreenToClient(rPPHost);

	if (m_pphost.Create(rPPHost, this))
		SetActivePage(AfxGetApp()->GetProfileInt(_T("Settings"), _T("OptStartPage"), 0));
 
	// setup handler for resizing this dialog	
	m_constraint.InitializeCurrentSize(this);
	m_constraint.DisallowHeightGrowth();
	m_constraint.SubclassWnd(); // install subclassing
	m_constraint.LoadPosition(_T("ResizeableDialogs"), _T("OptionsDlg"), false); // persist size via registry
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CPreferencesDlg::OnOK()
{
	CDialog::OnOK();
	m_pphost.OnOK();

	SaveOptions();

	AfxGetApp()->WriteProfileInt(_T("Settings"), _T("OptStartPage"), m_pphost.GetActiveIndex());
}

void CPreferencesDlg::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

	if (CWnd *pPPHostWnd = GetDlgItem(IDC_TREEOPT_HOSTFRAME))
	{
		CRect rPPHost;
		pPPHostWnd->GetWindowRect(rPPHost);
		ScreenToClient(rPPHost);
		if (m_pphost.m_hWnd)
			m_pphost.MoveWindow(&rPPHost);
	}
}
	
void CPreferencesDlg::OnHelpButton() 
{
	theApp.ShowHelp(OptionsHelpLocation);
}

void CPreferencesDlg::AddPage(CPropertyPage* pPage, UINT nResourceID)
{
	String sPath = theApp.LoadString(nResourceID);
	AddPage(pPage, sPath.c_str());
}

void CPreferencesDlg::AddPage(CPropertyPage* pPage, UINT nTopHeading, UINT nSubHeading)
{
	String sPath = theApp.LoadString(nTopHeading);
	sPath += _T(">");
	sPath += theApp.LoadString(nSubHeading);
	AddPage(pPage, sPath.c_str());
}

void CPreferencesDlg::AddPage(CPropertyPage* pPage, LPCTSTR szPath)
{
	CString sPath(szPath);

	if (m_pphost.AddPage(pPage))
	{
		HTREEITEM htiParent = TVI_ROOT; // default
		int nFind = sPath.Find(PATHDELIM);

		while (nFind != -1)
		{
			CString sParent = sPath.Left(nFind);
			sPath = sPath.Mid(nFind + 1);

			// see if parent already exists
			HTREEITEM htiParentParent = htiParent;
			htiParent = m_tcPages.GetChildItem(htiParentParent);

			while (htiParent != nullptr)
			{
				if (sParent.CompareNoCase(m_tcPages.GetItemText(htiParent)) == 0)
					break;

				htiParent = m_tcPages.GetNextItem(htiParent, TVGN_NEXT);
			}

			if (htiParent == nullptr)
				htiParent = m_tcPages.InsertItem(sParent, htiParentParent);

			nFind = sPath.Find(PATHDELIM);
		}

		HTREEITEM hti = m_tcPages.InsertItem(sPath, htiParent); // whatever's left
		m_tcPages.EnsureVisible(hti);

		// map both ways
		m_tcPages.SetItemData(hti, static_cast<DWORD_PTR>(reinterpret_cast<uintptr_t>(pPage)));
		m_mapPP2HTI[(void*)pPage] = (void*)hti;
	}
}

void CPreferencesDlg::OnSelchangedPages(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UNREFERENCED_PARAMETER(pNMHDR);
	HTREEITEM htiSel = m_tcPages.GetSelectedItem();

	while (m_tcPages.ItemHasChildren(htiSel))
		htiSel = m_tcPages.GetChildItem(htiSel);

	CPropertyPage* pPage = (CPropertyPage*)m_tcPages.GetItemData(htiSel);
	ASSERT (pPage != nullptr);

	if (pPage != nullptr)
	{
		m_pphost.SetActivePage(pPage, false);

		// update caption
		String sCaption = strutils::format_string1(_("Options (%1)"), (LPCTSTR)GetItemPath(htiSel));
		SetWindowText(sCaption.c_str());
	}

	m_tcPages.SetFocus();
	
	*pResult = 0;
}

void CPreferencesDlg::SetActivePage(int nPage)
{
	m_pphost.SetActivePage(nPage, false);

	// synchronize tree
	CPropertyPage* pPage = m_pphost.GetActivePage();
	HTREEITEM hti = nullptr;

	if (m_mapPP2HTI.Lookup(pPage, (void*&)hti) && hti != nullptr)
		m_tcPages.SelectItem(hti);
}

CString CPreferencesDlg::GetItemPath(HTREEITEM hti)
{
	CString sPath = m_tcPages.GetItemText(hti);

	hti = m_tcPages.GetParentItem(hti);
	while (hti != nullptr)
	{
		sPath = m_tcPages.GetItemText(hti) + _T(" > ") + sPath;
		hti = m_tcPages.GetParentItem(hti);
	}

	return sPath;
}

/**
 * @brief Read options from storage to UI controls.
 * @param [in] bUpdate If `true` UpdateData() is called
 */
void CPreferencesDlg::ReadOptions(bool bUpdate)
{
	m_pageGeneral.ReadOptions();
	m_pageColorSchemes.ReadOptions();
	m_pageMergeColors.ReadOptions();
	m_pageTextColors.ReadOptions();
	m_pageSyntaxColors.ReadOptions();
	m_pageMarkerColors.ReadOptions();
	m_pageDirColors.ReadOptions();
	m_pageSystem.ReadOptions();
	m_pageCompare.ReadOptions();
	m_pageCompareFolder.ReadOptions();
	m_pageCompareTable.ReadOptions();
	m_pageCompareBinary.ReadOptions();
	m_pageCompareImage.ReadOptions();
	m_pageEditor.ReadOptions();
	m_pageCodepage.ReadOptions();
	m_pageArchive.ReadOptions();
	m_pageBackups.ReadOptions();
	m_pageShell.ReadOptions();

	if (bUpdate)
	{
		SafeUpdatePage(&m_pageGeneral, false);
		SafeUpdatePage(&m_pageColorSchemes, false);
		SafeUpdatePage(&m_pageMergeColors, false);
		SafeUpdatePage(&m_pageTextColors, false);
		SafeUpdatePage(&m_pageSyntaxColors, false);
		SafeUpdatePage(&m_pageMarkerColors, false);
		SafeUpdatePage(&m_pageDirColors, false);
		SafeUpdatePage(&m_pageSystem, false);
		SafeUpdatePage(&m_pageCompare, false);
		SafeUpdatePage(&m_pageCompareFolder, false);
		SafeUpdatePage(&m_pageCompareTable, false);
		SafeUpdatePage(&m_pageCompareBinary, false);
		SafeUpdatePage(&m_pageCompareImage, false);
		SafeUpdatePage(&m_pageEditor, false);
		SafeUpdatePage(&m_pageCodepage, false);
		SafeUpdatePage(&m_pageArchive, false);
		SafeUpdatePage(&m_pageBackups, false);
		SafeUpdatePage(&m_pageShell, false);
	}
}

/**
 * @brief Write options from UI to storage.
 */
void CPreferencesDlg::SaveOptions()
{
	m_pageGeneral.WriteOptions();
	m_pageSystem.WriteOptions();
	m_pageCompare.WriteOptions();
	m_pageCompareFolder.WriteOptions();
	m_pageCompareTable.WriteOptions();
	m_pageCompareBinary.WriteOptions();
	m_pageCompareImage.WriteOptions();
	m_pageEditor.WriteOptions();
	m_pageColorSchemes.WriteOptions();
	m_pageMergeColors.WriteOptions();
	m_pageTextColors.WriteOptions();
	m_pageSyntaxColors.WriteOptions();
	m_pageMarkerColors.WriteOptions();
	m_pageDirColors.WriteOptions();
	m_pageCodepage.WriteOptions();
	m_pageArchive.WriteOptions();
	m_pageBackups.WriteOptions();
	m_pageShell.WriteOptions();
}

/**
 * @brief Imports options from file.
 */
void CPreferencesDlg::OnImportButton()
{
	String s;
	if (SelectFile(GetSafeHwnd(), s, true, nullptr, _("Select file for import"), _("Options files (*.ini)|*.ini|All Files (*.*)|*.*||")))
	{
		if (m_pOptionsMgr->ImportOptions(s) == COption::OPT_OK)
		{
			Options::SyntaxColors::Load(m_pOptionsMgr, m_pSyntaxColors);
			ReadOptions(true);
			LangMessageBox(IDS_OPT_IMPORT_DONE, MB_ICONINFORMATION);
		}
		else
			LangMessageBox(IDS_OPT_IMPORT_ERR, MB_ICONWARNING);
	}
}

/**
 * @brief Exports options to file.
 */
void CPreferencesDlg::OnExportButton()
{
	String settingsFile;
	if (SelectFile(GetSafeHwnd(), settingsFile, false, nullptr, _("Select file for export"),
		_("Options files (*.ini)|*.ini|All Files (*.*)|*.*||")))
	{
		// Add settings file extension if it is missing
		// So we allow 'filename.otherext' but add extension for 'filename'
		if (paths::FindExtension(settingsFile).empty())
			settingsFile += _T(".ini");

		// Save all new settings before exporting
		m_pphost.UpdatePagesData();
		SaveOptions();

		if (m_pOptionsMgr->ExportOptions(settingsFile, true) == COption::OPT_OK)
			LangMessageBox(IDS_OPT_EXPORT_DONE, MB_ICONINFORMATION);
		else
			LangMessageBox(IDS_OPT_EXPORT_ERR, MB_ICONWARNING);
	}
}

LRESULT CPreferencesDlg::OnColorSchemeChanged(WPARAM wParams, LPARAM lParam)
{
	Options::SyntaxColors::Load(m_pOptionsMgr, m_pSyntaxColors);
	ReadOptions(true);
	return 0;
}

/**
 * @brief Do a safe UpdateData call for propertypage.
 * This function does safe UpdateData call for given propertypage. As it is,
 * all propertypages may not have been yet initialized properly, so we must
 * have some care when calling updateData for them.
 * @param [in] pPage Propertypage to update.
 * @param bSaveAndValidate UpdateData direction parameter.
 */
void CPreferencesDlg::SafeUpdatePage(CPropertyPage* pPage, bool bSaveAndValidate)
{
	if (pPage->GetSafeHwnd() != nullptr)
		pPage->UpdateData(bSaveAndValidate);
}
