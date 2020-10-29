/**
 *  @file LineFiltersDlg.cpp
 *
 *  @brief Implementation of Line Filter dialog
 */ 

#include "stdafx.h"
#include "LineFiltersList.h"
#include "Merge.h"
#include "LineFiltersDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/** @brief Location for file compare specific help to open. */
static TCHAR FilterHelpLocation[] = _T("::/htmlhelp/Filters.html");

/////////////////////////////////////////////////////////////////////////////
// CPropLineFilter property page

IMPLEMENT_DYNAMIC(LineFiltersDlg, CTrPropertyPage)

/**
 * @brief Constructor.
 */
LineFiltersDlg::LineFiltersDlg()
: CTrPropertyPage(LineFiltersDlg::IDD)
, m_pList(nullptr)
{
	//{{AFX_DATA_INIT(LineFiltersDlg)
	m_bIgnoreRegExp = false;
	//}}AFX_DATA_INIT
	m_strCaption = theApp.LoadDialogCaption(m_lpszTemplateName).c_str();
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;
	m_psp.hIcon = AfxGetApp()->LoadIcon(IDI_LINEFILTER);
	m_psp.dwFlags |= PSP_USEHICON;
}

void LineFiltersDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(LineFiltersDlg)
	DDX_Check(pDX, IDC_IGNOREREGEXP, m_bIgnoreRegExp);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_LFILTER_LIST, m_filtersList);
}


BEGIN_MESSAGE_MAP(LineFiltersDlg, CTrPropertyPage)
	//{{AFX_MSG_MAP(LineFiltersDlg)
	ON_COMMAND(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_LFILTER_ADDBTN, OnBnClickedLfilterAddBtn)
	ON_BN_CLICKED(IDC_LFILTER_EDITBTN, OnBnClickedLfilterEditbtn)
	ON_BN_CLICKED(IDC_LFILTER_REMOVEBTN, OnBnClickedLfilterRemovebtn)
	ON_NOTIFY(LVN_ITEMACTIVATE, IDC_LFILTER_LIST, OnLvnItemActivateLfilterList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_LFILTER_LIST, OnLvnKeyDownLfilterList)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LFILTER_LIST, OnEndLabelEditLfilterList)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CPropLineFilter message handlers

/**
 * @brief Initialize the dialog.
 */
BOOL LineFiltersDlg::OnInitDialog()
{
	CTrPropertyPage::OnInitDialog();

	InitList();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/**
 * @brief Initialize the filter list in the dialog.
 * This function adds current line filters to the filter list.
 */
void LineFiltersDlg::InitList()
{
	// Show selection across entire row.
	// Also enable infotips.
	m_filtersList.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	String title = _("Regular expression");
	m_filtersList.InsertColumn(1, title.c_str(), LVCFMT_LEFT, PointToPixel(375));

	size_t count = m_pList->GetCount();
	for (size_t i = 0; i < count; i++)
	{
		const LineFilterItem &item = m_pList->GetAt(i);
		AddRow(item.filterStr, item.enabled);
	}
	m_filtersList.SetItemState(0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
	UpdateData(FALSE);
}

/**
 * @brief Open help from mainframe when user presses F1.
 */
void LineFiltersDlg::OnHelp()
{
	theApp.ShowHelp(FilterHelpLocation);
}

/**
 * @brief Add new row to the list control.
 * @param [in] Filter string to add.
 * @param [in] enabled Is filter enabled?
 * @return Index of added row.
 */
int LineFiltersDlg::AddRow(const String& filter /*= nullptr*/, bool enabled /*= false*/)
{
	int items = m_filtersList.GetItemCount();
	int ind = m_filtersList.InsertItem(items, filter.c_str());
	m_filtersList.SetCheck(ind, enabled);
	return ind;
}

/**
 * @brief Edit currently selected filter.
 */
void LineFiltersDlg::EditSelectedFilter()
{
	m_filtersList.SetFocus();
	int sel = m_filtersList.GetNextItem(-1, LVNI_SELECTED);
	if (sel > -1)
	{
		m_filtersList.EditLabel(sel);
	}
}

/**
 * @brief Called when Add-button is clicked.
 */
void LineFiltersDlg::OnBnClickedLfilterAddBtn()
{
	int ind = AddRow(_T(""));
	if (ind >= -1)
	{
		m_filtersList.SetItemState(ind, LVIS_SELECTED, LVIS_SELECTED);
		m_filtersList.EnsureVisible(ind, FALSE);
		EditSelectedFilter();
	}
}

/**
 * @brief Called when Edit button is clicked.
 */
void LineFiltersDlg::OnBnClickedLfilterEditbtn()
{
	EditSelectedFilter();
}

/**
 * @brief Save filters to list when exiting the dialog.
 */
void LineFiltersDlg::OnOK()
{
	m_pList->Empty();

	for (int i = 0; i < m_filtersList.GetItemCount(); i++)
	{
		String text = m_filtersList.GetItemText(i, 0);
		bool enabled = !!m_filtersList.GetCheck(i);

		m_pList->AddFilter(text, enabled);
	}

	CPropertyPage::OnClose();
}

/**
 * @brief Sets external filter list.
 * @param [in] list External filter list.
 */
void LineFiltersDlg::SetList(LineFiltersList * list)
{
	m_pList = list;
}

/**
 * @brief Called when Remove button is clicked.
 */
void LineFiltersDlg::OnBnClickedLfilterRemovebtn()
{
	int sel = m_filtersList.GetNextItem(-1, LVNI_SELECTED);
	if (sel != -1)
	{
		m_filtersList.DeleteItem(sel);
	}

	int newSel = min(m_filtersList.GetItemCount() - 1, sel);
	if (newSel >= -1)
	{
		m_filtersList.SetItemState(newSel, LVIS_SELECTED, LVIS_SELECTED);
		bool bPartialOk = false;
		m_filtersList.EnsureVisible(newSel, bPartialOk);
	}
}

/**
 * @brief Called when the user activates an item.
 */
void LineFiltersDlg::OnLvnItemActivateLfilterList(NMHDR *pNMHDR, LRESULT *pResult)
{
	EditSelectedFilter();
	*pResult = 0;
}

/**
 * @brief Called when a key has been pressed while the list has the focus.
 */
void LineFiltersDlg::OnLvnKeyDownLfilterList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMKEY pNMKey = reinterpret_cast<LPNMKEY>(pNMHDR);
	if (LOWORD(pNMKey->nVKey) == VK_F2)
	{
		EditSelectedFilter();
	}
	*pResult = 0;
}

/**
 * @brief Called when in-place editing has finished.
 */
void LineFiltersDlg::OnEndLabelEditLfilterList(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 1;
}
