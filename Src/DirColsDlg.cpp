/** 
 * @file  DirColsDlg.cpp
 *
 * @brief Implementation file for CDirColsDlg
 *
 * @date  Created: 2003-08-19
 */


#include "stdafx.h"
#include "DirColsDlg.h"
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CDirColsDlg dialog

/**
 * @brief Default dialog constructor.
 * @param [in] pParent Dialog's parent component (window).
 */
CDirColsDlg::CDirColsDlg(CWnd* pParent /*= nullptr*/)
	: CTrDialog(CDirColsDlg::IDD, pParent)
	, m_bReset(false)
{
}

/**
 * @brief Handle dialog data exchange between controls and variables.
 */
void CDirColsDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDirColsDlg)
	DDX_Control(pDX, IDC_COLDLG_LIST, m_listColumns);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDirColsDlg, CTrDialog)
	//{{AFX_MSG_MAP(CDirColsDlg)
	ON_BN_CLICKED(IDC_UP, OnUp)
	ON_BN_CLICKED(IDC_DOWN, OnDown)
	ON_BN_CLICKED(IDC_COLDLG_DEFAULTS, OnDefaults)
	//}}AFX_MSG_MAP
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_COLDLG_LIST, OnLvnItemchangedColdlgList)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDirColsDlg message handlers

/**
 * @brief Initialize listcontrol in dialog.
 */
void CDirColsDlg::InitList()
{
	// Show selection across entire row.
	// Also enable infotips.
	m_listColumns.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	m_listColumns.InsertColumn(0, _T(""), LVCFMT_LEFT, PointToPixel(148));
}

/**
 * @brief Dialog initialisation. Load column lists.
 */
BOOL CDirColsDlg::OnInitDialog() 
{
	CTrDialog::OnInitDialog();
	InitList();
	LoadLists();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/**
 * @brief Load listboxes on screen from column array.
 */
void CDirColsDlg::LoadLists()
{
	for (ColumnArray::iterator iter = m_cols.begin(); iter != m_cols.end(); ++iter)
	{
		const column & c = *iter;
		int x = m_listColumns.InsertItem(m_listColumns.GetItemCount(),
			c.name.c_str());
		m_listColumns.SetItemData(x, c.log_col);
		if (c.phy_col >= 0)
			m_listColumns.SetCheck(x, TRUE);
	}
	SortArrayToLogicalOrder();
	
	// Set first item to selected state
	SelectItem(0);
}

/**
 * @brief Select item in list.
 * @param [in] index Index of item to select.
 */
void CDirColsDlg::SelectItem(int index)
{
	m_listColumns.SetItemState(index, LVIS_SELECTED, LVIS_SELECTED);
}

/**
 * @brief Load listboxes on screen from defaults column array
 */
void CDirColsDlg::LoadDefLists()
{
	for (ColumnArray::iterator iter = m_defCols.begin(); iter != m_defCols.end(); ++iter)
	{
		const column & c = *iter;
		int x = m_listColumns.InsertItem(m_listColumns.GetItemCount(),
			c.name.c_str());
		m_listColumns.SetItemData(x, c.log_col);
		if (c.phy_col >= 0)
			m_listColumns.SetCheck(x, TRUE);
	}
	SortArrayToLogicalOrder();
}
/**
 * @brief Sort m_cols so that it is in logical column order.
 */
void CDirColsDlg::SortArrayToLogicalOrder()
{
	std::sort(m_cols.begin(), m_cols.end(), &CompareColumnsByLogicalOrder);
}

/**
 * @brief Compare column order of two columns.
 * @param [in] el1 First column to compare.
 * @param [in] el2 Second column to compare.
 * @return Column order.
 */
bool CDirColsDlg::CompareColumnsByLogicalOrder( const column & el1, const column & el2 )
{
   return el1.log_col < el2.log_col;
}

/**
 * @brief Move column name in the list.
 * @param [in] index Index of column to move.
 * @param [in] newIndex New index for the column to move.
 */
void CDirColsDlg::MoveItem(int index, int newIndex)
{
	// Get current column data
	String text =  m_listColumns.GetItemText(index, 0);
	bool checked = !!m_listColumns.GetCheck(index);
	UINT state = m_listColumns.GetItemState(index, LVIS_SELECTED);
	DWORD_PTR data = m_listColumns.GetItemData(index);

	// Delete column
	m_listColumns.DeleteItem(index);
	
	// Insert column in new index
	m_listColumns.InsertItem(newIndex, text.c_str());
	m_listColumns.SetItemState(newIndex, state, LVIS_SELECTED);
	m_listColumns.SetItemData(newIndex, data);
	if (checked)
		m_listColumns.SetCheck(newIndex);
}

/**
 * @brief Move selected items one position up or down.
 * @param [in] bUp If `true` items are moved up,
 *  if `false` items are moved down.
 */
void CDirColsDlg::MoveSelectedItems(bool bUp)
{
	int firstInd = -1;
	POSITION pos = m_listColumns.GetFirstSelectedItemPosition();

	while (pos != nullptr)
	{
		int ind = m_listColumns.GetNextSelectedItem(pos);
		int newInd = bUp ? ind - 1: ind + 1;
		
		// Remember first item
		if (firstInd == -1)
			firstInd = newInd;
		MoveItem(ind, newInd);
	}
	m_listColumns.EnsureVisible(firstInd, FALSE);
}

/**
 * @brief Move selected items up in list.
 */
void CDirColsDlg::OnUp()
{
	MoveSelectedItems(true);
}

/**
 * @brief Move selected items down (in show list)
 */
void CDirColsDlg::OnDown() 
{
	MoveSelectedItems(false);
}

/**
 * @brief Move hidden columns as last items in the list.
 */
void CDirColsDlg::SanitizeOrder()
{
	// Find last visible column.
	int i = m_listColumns.GetItemCount() - 1;
	for ( ; i >= 0; i--)
	{
		if (m_listColumns.GetCheck(i))
			break;
	}

	// Move all hidden columns below last visible column.
	for (int j = i; j >= 0; j--)
	{
		if (!m_listColumns.GetCheck(j))
		{
			MoveItem(j, i);
			i--;
		}
	}
}

/**
 * @brief User clicked ok, so we update m_cols and close.
 */
void CDirColsDlg::OnOK() 
{
	SanitizeOrder();

	for (int i = 0; i < m_listColumns.GetItemCount(); i++)
	{
		bool checked = !!m_listColumns.GetCheck(i);
		DWORD_PTR data = m_listColumns.GetItemData(i);
		column & col1 = m_cols[data];
		if (checked)
			col1.phy_col = i;
		else
			col1.phy_col = -1;
	}

	CTrDialog::OnOK();
}

/**
 * @brief Empty lists and load default columns and order.
 */
void CDirColsDlg::OnDefaults()
{
	m_listColumns.DeleteAllItems();
	m_bReset = true;
	LoadDefLists();
}

/**
 * @brief Update description when selected item changes.
 */
void CDirColsDlg::OnLvnItemchangedColdlgList(NMHDR *pNMHDR, LRESULT *pResult)
{
	POSITION pos = m_listColumns.GetFirstSelectedItemPosition();

	if (pos != nullptr)
	{
		int ind = m_listColumns.GetNextSelectedItem(pos);
		DWORD_PTR data = m_listColumns.GetItemData(ind);

		ColumnArray::size_type j;
		for (j = 0; j < m_cols.size(); j++)
		{
			if (static_cast<DWORD_PTR>(m_cols[j].log_col) == data)
				break;
		}
		SetDlgItemText(IDC_COLDLG_DESC, m_cols[j].desc);

		// Disable Up/Down -buttons when first/last items are selected.
		EnableDlgItem(IDC_UP, ind != 0);
		EnableDlgItem(IDC_DOWN,
			ind != m_listColumns.GetItemCount() - static_cast<int>(m_listColumns.GetSelectedCount()));
	}
	*pResult = 0;
}
