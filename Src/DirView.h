/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 *  @file DirView.h
 *
 *  @brief Declaration of class CDirView
 */
#pragma once

/////////////////////////////////////////////////////////////////////////////
// CDirView view
#include <afxcview.h>
#include <memory>
#include <optional>
#include "OptionsDirColors.h"
#include "SortHeaderCtrl.h"
#include "UnicodeString.h"
#include "DirItemIterator.h"
#include "DirActions.h"
#include "utils/DpiAware.h"

class FileActionScript;

typedef enum { eMain, eContext } eMenuType;

class CDirDoc;
class CDirFrame;

class PackingInfo;
class PathContext;
class DirCompProgressBar;
class CompareStats;
struct DirColInfo;
class CLoadSaveCodepageDlg;
class CShellContextMenu;
class CDiffContext;
class DirViewColItems;
class DirItemEnumerator;
struct IListCtrl;

/**
 * @brief Position value for special items (..) in directory compare view.
 */
const uintptr_t SPECIAL_ITEM_POS = (uintptr_t)(reinterpret_cast<DIFFITEM *>( - 1L));

/** Default column width in directory compare */
const UINT DefColumnWidth = 111;

/**
 * @brief Directory compare results view.
 *
 * Directory compare view is list-view based, so it shows one result (for
 * folder or file, commonly called as 'item') in one line. User can select
 * visible columns, re-order columns, sort by column etc.
 *
 * Actual data is stored in CDiffContext in CDirDoc. Dircompare items and
 * CDiffContext items are linked by storing POSITION of CDiffContext item
 * as CDirView listitem key.
 */
class CDirView : public DpiAware::CDpiAwareWnd<CListView>
{
	friend struct FileCmpReport;
	friend DirItemEnumerator;
protected:
	CDirView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CDirView)

// Attributes
public:
	CDirDoc* GetDocument(); // non-debug version is inline
	// const version, for const methods to be able to call
	const CDirDoc * GetDocument() const { return const_cast<CDirView *>(this)->GetDocument(); }
	const CDiffContext& GetDiffContext() const;
	CDiffContext& GetDiffContext();

// Operations
public:
	CDirFrame * GetParentFrame();

	void StartCompare(CompareStats *pCompareStats);
	void Redisplay();
	void RedisplayChildren(DIFFITEM *diffpos, int level, UINT &index, int &alldiffs);
	void UpdateResources();
	void LoadColumnHeaderItems();
	DIFFITEM *GetItemKey(int idx) const;
	int GetItemIndex(DIFFITEM *key);
	// for populating list
	void DeleteItem(int sel, bool removeDIFFITEM = false);
	void DeleteAllDisplayItems();
	void SetFont(const LOGFONT & lf);

	void SortColumnsAppropriately();

	UINT GetSelectedCount() const;
	int GetFirstSelectedInd();
	void AddParentFolderItem(bool bEnable);
	void RefreshOptions();

	bool HasNextDiff();
	bool HasPrevDiff();
	void MoveToNextDiff();
	void MoveToPrevDiff();
	void OpenNextDiff();
	void OpenPrevDiff();
	void SetActivePane(int pane);

// Implementation types
private:

// Implementation in DirActions.cpp
private:
	void GetItemFileNames(int sel, String& strLeft, String& strRight) const;
	void GetItemFileNames(int sel, PathContext * paths) const;
	void FormatEncodingDialogDisplays(CLoadSaveCodepageDlg * dlg);
	DirActions MakeDirActions(DirActions::method_type func) const;
	DirActions MakeDirActions(DirActions::method_type2 func) const;
	Counts Count(DirActions::method_type2 func) const;
	void DoDirAction(DirActions::method_type func, const String& status_message);
	void DoDirActionTo(SIDE_TYPE stype, DirActions::method_type func, const String& status_message);
	void DoOpen(SIDE_TYPE stype);
	void DoOpenWith(SIDE_TYPE stype);
	void DoOpenWithEditor(SIDE_TYPE stype);
	void DoOpenParentFolder(SIDE_TYPE stype);
	void DoUpdateOpen(SELECTIONTYPE selectionType, CCmdUI* pCmdUI, bool openableForDir = true);
	void ConfirmAndPerformActions(FileActionScript & actions);
	void PerformActionList(FileActionScript & actions);
	void UpdateAfterFileScript(FileActionScript & actionList);
	void DoFileEncodingDialog();

// End DirActions.cpp
	void ReflectGetdispinfo(NMLVDISPINFO *);

// Implementation in DirViewColHandler.cpp
public:
	void UpdateColumnNames();
	void SetColAlignments();
	// class CompareState is used to pass parameters to the PFNLVCOMPARE callback function.
	class CompareState
	{
	private:
		const DirViewColItems *const pColItems;
		const CDiffContext *const pCtxt;
		const int sortCol;
		const bool bSortAscending;
		const bool bTreeMode;
	public:
		CompareState(const CDiffContext *pCtxt, const DirViewColItems *pColItems, int sortCol, bool bSortAscending, bool bTreeMode);
		static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	} friend;
	void UpdateDiffItemStatus(UINT nIdx);
private:
	void InitiateSort();
	void NameColumn(const DirColInfo *col, int subitem);
	int AddNewItem(int i, DIFFITEM *diffpos, int iImage, int iIndent);
// End DirViewCols.cpp

private:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDirView)
public:
	virtual void OnInitialUpdate();
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	virtual BOOL OnChildNotify(UINT, WPARAM, LPARAM, LRESULT*);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CDirView();
	int GetFocusedItem();
	int GetFirstDifferentItem();
	int GetLastDifferentItem();
	int AddSpecialItems();
	std::vector<String>	GetCurrentColRegKeys();
	void OpenSpecialItems(DIFFITEM *pos1, DIFFITEM *pos2, DIFFITEM *pos3);

// Implementation data
protected:
	CSortHeaderCtrl m_ctlSortHeader;
	CImageList m_imageList;
	CImageList m_imageState;
	CListCtrl* m_pList;
	std::unique_ptr<IListCtrl> m_pIList;
	int m_nEscCloses; /**< Cached value for option for ESC closing window */
	bool m_bExpandSubdirs;
	CFont m_font; /**< User-selected font */
	UINT m_nHiddenItems; /**< Count of items we have hidden */
	bool m_bTreeMode; /**< `true` if tree mode is on*/
	DirViewFilterSettings m_dirfilter;
	std::unique_ptr<DirCompProgressBar> m_pCmpProgressBar;
	clock_t m_compareStart; /**< Starting process time of the compare */
	bool m_bUserCancelEdit; /**< `true` if the user cancels rename */
	String m_lastCopyFolder; /**< Last Copy To -target folder. */

	std::optional<int> m_firstDiffItem;
	std::optional<int> m_lastDiffItem;
	DIRCOLORSETTINGS m_cachedColors; /**< Cached color settings */
	bool m_bUseColors;

	std::unique_ptr<CShellContextMenu> m_pShellContextMenuLeft; /**< Shell context menu for group of left files */
	std::unique_ptr<CShellContextMenu> m_pShellContextMenuMiddle; /**< Shell context menu for group of middle files */
	std::unique_ptr<CShellContextMenu> m_pShellContextMenuRight; /**< Shell context menu for group of right files */
	HMENU m_hCurrentMenu; /**< Current shell context menu (either left or right) */
	std::unique_ptr<DirViewTreeState> m_pSavedTreeState;
	std::unique_ptr<DirViewColItems> m_pColItems;
	int m_nActivePane;

	// Generated message map functions
	afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	//{{AFX_MSG(CDirView)
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnDirCopy(UINT id);
	template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
	afx_msg void OnCtxtDirCopy();
	afx_msg void OnUpdateDirCopy(CCmdUI* pCmdUI);
	template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
	afx_msg void OnUpdateCtxtDirCopy(CCmdUI* pCmdUI);
	template<SIDE_TYPE stype>
	afx_msg void OnCtxtDirDel();
	template<SIDE_TYPE stype>
	afx_msg void OnUpdateCtxtDirDel(CCmdUI* pCmdUI);
	afx_msg void OnCtxtDirDelBoth();
	afx_msg void OnUpdateCtxtDirDelBoth(CCmdUI* pCmdUI);
	template<SIDE_TYPE stype>
	afx_msg void OnCtxtDirOpen();
	template<SIDE_TYPE stype>
	afx_msg void OnUpdateCtxtDirOpen(CCmdUI* pCmdUI);
	template<SIDE_TYPE stype>
	afx_msg void OnCtxtDirOpenWith();
	template<SIDE_TYPE stype>
	afx_msg void OnUpdateCtxtDirOpenWith(CCmdUI* pCmdUI);
	template<SIDE_TYPE stype>
	afx_msg void OnCtxtDirOpenWithEditor();
	template<SIDE_TYPE stype>
	afx_msg void OnUpdateCtxtDirOpenWithEditor(CCmdUI* pCmdUI);
	template<SIDE_TYPE stype>
	afx_msg void OnCtxtDirOpenParentFolder();
	template<SIDE_TYPE stype>
	afx_msg void OnUpdateCtxtDirOpenParentFolder(CCmdUI* pCmdUI);
	template<SIDE_TYPE stype>
	afx_msg void OnCtxtDirCopyTo();
	template<SIDE_TYPE stype>
	afx_msg void OnUpdateCtxtDirCopyTo(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCtxtDirCopyBothTo(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCtxtDirCopyBothDiffsOnlyTo(CCmdUI* pCmdUI);
	template<SIDE_TYPE stype>
	afx_msg void OnUpdateCtxtDirCopy2(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCtxtDirCopyBoth2(CCmdUI* pCmdUI);
	afx_msg void OnDestroy();
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFirstdiff();
	afx_msg void OnUpdateFirstdiff(CCmdUI* pCmdUI);
	afx_msg void OnLastdiff();
	afx_msg void OnUpdateLastdiff(CCmdUI* pCmdUI);
	afx_msg void OnNextdiff();
	afx_msg void OnUpdateNextdiff(CCmdUI* pCmdUI);
	afx_msg void OnPrevdiff();
	afx_msg void OnUpdatePrevdiff(CCmdUI* pCmdUI);
	afx_msg void OnCurdiff();
	afx_msg void OnUpdateCurdiff(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSave(CCmdUI* pCmdUI);
	afx_msg LRESULT OnUpdateUIMessage(WPARAM wParam, LPARAM lParam);
	afx_msg void OnRefresh();
	afx_msg void OnUpdateRefresh(CCmdUI* pCmdUI);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnEditColumns();
	template<SIDE_TYPE stype>
	afx_msg void OnReadOnly();
	template<SIDE_TYPE stype>
	afx_msg void OnUpdateReadOnly(CCmdUI* pCmdUI);
	afx_msg void OnUpdateStatusLeftRO(CCmdUI* pCmdUI);
	afx_msg void OnUpdateStatusMiddleRO(CCmdUI* pCmdUI);
	afx_msg void OnUpdateStatusRightRO(CCmdUI* pCmdUI);
	afx_msg void OnCustomizeColumns();
	afx_msg void OnCtxtOpenWithUnpacker();
	afx_msg void OnUpdateCtxtOpenWithUnpacker(CCmdUI* pCmdUI);
	afx_msg void OnToolsGenerateReport();
	afx_msg LRESULT OnGenerateFileCmpReport(WPARAM wParam, LPARAM lParam);
	afx_msg void OnToolsGeneratePatch();
	template<int flag>
	afx_msg void OnCtxtDirZip();
	template<SIDE_TYPE stype>
	afx_msg void OnCtxtDirShellContextMenu();
	afx_msg void OnSelectAll();
	afx_msg void OnUpdateSelectAll(CCmdUI* pCmdUI);
	afx_msg void OnPluginPredifferMode(UINT nID);
	afx_msg void OnUpdatePluginPredifferMode(CCmdUI* pCmdUI);
	template<SIDE_TYPE side>
	afx_msg void OnCopyPathnames();
	afx_msg void OnCopyBothPathnames();
	afx_msg void OnCopyFilenames();
	afx_msg void OnUpdateCopyFilenames(CCmdUI* pCmdUI);
	template<SIDE_TYPE side>
	afx_msg void OnCopyToClipboard();
	afx_msg void OnCopyBothToClipboard();
	afx_msg void OnItemRename();
	afx_msg void OnUpdateItemRename(CCmdUI* pCmdUI);
	afx_msg void OnHideFilenames();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	template<SIDE_TYPE stype>
	afx_msg void OnCtxtDirMoveTo();
	template<SIDE_TYPE stype>
	afx_msg void OnUpdateCtxtDirMoveTo(CCmdUI* pCmdUI);
	afx_msg void OnUpdateHideFilenames(CCmdUI* pCmdUI);
	afx_msg void OnDelete();
	afx_msg void OnUpdateDelete(CCmdUI* pCmdUI);
	afx_msg void OnMarkedRescan();
	afx_msg void OnUpdateStatusNum(CCmdUI* pCmdUI);
	afx_msg void OnViewShowHiddenItems();
	afx_msg void OnUpdateViewShowHiddenItems(CCmdUI* pCmdUI);
	afx_msg void OnViewTreeMode();
	afx_msg void OnUpdateViewTreeMode(CCmdUI* pCmdUI);
	afx_msg void OnViewExpandAllSubdirs();
	afx_msg void OnUpdateViewExpandAllSubdirs(CCmdUI* pCmdUI);
	afx_msg void OnViewCollapseAllSubdirs();
	afx_msg void OnUpdateViewCollapseAllSubdirs(CCmdUI* pCmdUI);
	afx_msg void OnViewSwapPanes();
	afx_msg void OnOptionsShowDifferent();
	afx_msg void OnOptionsShowIdentical();
	afx_msg void OnOptionsShowUniqueLeft();
	afx_msg void OnOptionsShowUniqueMiddle();
	afx_msg void OnOptionsShowUniqueRight();
	afx_msg void OnOptionsShowBinaries();
	afx_msg void OnOptionsShowSkipped();
	afx_msg void OnOptionsShowDifferentLeftOnly();
	afx_msg void OnOptionsShowDifferentMiddleOnly();
	afx_msg void OnOptionsShowDifferentRightOnly();
	afx_msg void OnOptionsShowMissingLeftOnly();
	afx_msg void OnOptionsShowMissingMiddleOnly();
	afx_msg void OnOptionsShowMissingRightOnly();
	afx_msg void OnUpdateOptionsShowdifferent(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowidentical(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowuniqueleft(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowuniquemiddle(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowuniqueright(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowBinaries(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowSkipped(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowDifferentLeftOnly(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowDifferentMiddleOnly(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowDifferentRightOnly(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowMissingLeftOnly(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowMissingMiddleOnly(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsShowMissingRightOnly(CCmdUI* pCmdUI);
	afx_msg void OnMergeCompare();
	template<SELECTIONTYPE seltype>
	afx_msg void OnMergeCompare2();
	afx_msg void OnMergeCompareNonHorizontally();
	afx_msg void OnMergeCompareXML();
	afx_msg void OnMergeCompareAs(UINT nID);
	afx_msg void OnUpdateMergeCompare(CCmdUI *pCmdUI);
	template<SELECTIONTYPE seltype>
	afx_msg void OnUpdateMergeCompare2(CCmdUI *pCmdUI);
	afx_msg void OnViewCompareStatistics();
	afx_msg void OnFileEncoding();
	afx_msg void OnHelp();
	afx_msg void OnEditCopy();
	afx_msg void OnEditCut();
	afx_msg void OnEditPaste();
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
	afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBeginLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSearch();
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBnClickedComparisonStop();
	afx_msg void OnBnClickedComparisonPause();
	afx_msg void OnBnClickedComparisonContinue();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	bool OnHeaderBeginDrag(LPNMHEADER hdr, LRESULT* pResult);
	bool OnHeaderEndDrag(LPNMHEADER hdr, LRESULT* pResult);
	LRESULT OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam);

private:
	void Open(const PathContext& paths, DWORD dwFlags[3], PackingInfo * infoUnpacker = nullptr);
	void OpenSelection(SELECTIONTYPE selectionType = SELECTIONTYPE_NORMAL, PackingInfo * infoUnpacker = nullptr, bool openableForDir = true);
	void OpenSelectionAs(UINT id);
	bool GetSelectedItems(int * sel1, int * sel2, int * sel3);
	void OpenParentDirectory();
	template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
	void DoUpdateDirCopy(CCmdUI* pCmdUI, eMenuType menuType);
	const DIFFITEM &GetDiffItem(int sel) const;
	DIFFITEM &GetDiffItem(int sel);
	int GetSingleSelectedItem() const;
	void MoveFocus(int currentInd, int i, int selCount);

	void FixReordering();
	void HeaderContextMenu(CPoint point, int i);
	void ListContextMenu(CPoint point, int i);
	bool ListShellContextMenu(SIDE_TYPE side);
	void ShowShellContextMenu(SIDE_TYPE side);
	CShellContextMenu* GetCorrespondingShellContextMenu(HMENU hMenu) const;
	void ReloadColumns();
	bool IsLabelEdit() const;
	void CollapseSubdir(int sel);
	void ExpandSubdir(int sel, bool bRecursive = false);
	void GetColors(int nRow, int nCol, COLORREF& clrBk, COLORREF& clrText) const;
	int GetDefColumnWidth() const { return PointToPixel(DefColumnWidth); };

public:
	DirItemIterator Begin() const { return DirItemIterator(m_pIList.get()); }
	DirItemIterator End() const { return DirItemIterator(); }
	DirItemIterator RevBegin() const { return DirItemIterator(m_pIList.get(), -1, false, true); }
	DirItemIterator RevEnd() const { return DirItemIterator(); }
	DirItemIterator SelBegin() const { return DirItemIterator(m_pIList.get(), -1, true); }
	DirItemIterator SelEnd() const { return DirItemIterator(); }
	DirItemIterator SelRevBegin() const { return DirItemIterator(m_pIList.get(), -1, true, true); }
	DirItemIterator SelRevEnd() const { return DirItemIterator(); }
};

#ifndef _DEBUG  // debug version in DirView.cpp
inline CDirDoc* CDirView::GetDocument()
{ return reinterpret_cast<CDirDoc*>(m_pDocument); }
#endif
