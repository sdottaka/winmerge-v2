/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirView.cpp
 *
 * @brief Main implementation file for CDirView
 */

#include "StdAfx.h"
#include "DirView.h"
#include "Constants.h"
#include "Merge.h"
#include "ClipBoard.h"
#include "DirActions.h"
#include "DirViewColItems.h"
#include "DirFrame.h"  // StatePane
#include "DirDoc.h"
#include "IMergeDoc.h"
#include "FileLocation.h"
#include "MainFrm.h"
#include "resource.h"
#include "FileTransform.h"
#include "SelectUnpackerDlg.h"
#include "paths.h"
#include "7zCommon.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "BCMenu.h"
#include "DirCmpReportDlg.h"
#include "DirCmpReport.h"
#include "DirCompProgressBar.h"
#include "CompareStatisticsDlg.h"
#include "LoadSaveCodepageDlg.h"
#include "ConfirmFolderCopyDlg.h"
#include "DirColsDlg.h"
#include "DirSelectFilesDlg.h"
#include "UniFile.h"
#include "ShellContextMenu.h"
#include "DiffItem.h"
#include "IListCtrlImpl.h"
#include "Merge7zFormatMergePluginImpl.h"
#include "FileOrFolderSelect.h"
#include "IntToIntMap.h"
#include "PatchTool.h"
#include "SyntaxColors.h"
#include <numeric>
#include <functional>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using std::swap;
using namespace std::placeholders;

/**
 * @brief Location for folder compare specific help to open.
 */
static TCHAR DirViewHelpLocation[] = _T("::/htmlhelp/Compare_dirs.html");

/**
 * @brief Limit (in seconds) to signal compare is ready for user.
 * If compare takes longer than this value (in seconds) we inform
 * user about it. Current implementation uses MessageBeep(IDOK).
 */
const int TimeToSignalCompare = 3;

// The resource ID constants/limits for the Shell context menu
const UINT LeftCmdFirst = 0x9000; // this should be greater than any of already defined command IDs
const UINT RightCmdLast = 0xffff; // maximum available value
const UINT LeftCmdLast = LeftCmdFirst + (RightCmdLast - LeftCmdFirst) / 3; // divide available range equally between two context menus
const UINT MiddleCmdFirst = LeftCmdLast + 1;
const UINT MiddleCmdLast = MiddleCmdFirst + (RightCmdLast - LeftCmdFirst) / 3;
const UINT RightCmdFirst = MiddleCmdLast + 1;

/////////////////////////////////////////////////////////////////////////////
// CDirView

enum { 
	COLUMN_REORDER = 99,
	STATUSBAR_UPDATE = 100
};

IMPLEMENT_DYNCREATE(CDirView, CListView)

CDirView::CDirView()
		: m_pList(nullptr)
		, m_nHiddenItems(0)
		, m_pCmpProgressBar(nullptr)
		, m_compareStart(0)
		, m_bTreeMode(false)
		, m_dirfilter(std::bind(&COptionsMgr::GetBool, GetOptionsMgr(), _1))
		, m_pShellContextMenuLeft(nullptr)
		, m_pShellContextMenuMiddle(nullptr)
		, m_pShellContextMenuRight(nullptr)
		, m_hCurrentMenu(nullptr)
		, m_pSavedTreeState(nullptr)
		, m_pColItems(nullptr)
		, m_nActivePane(-1)
{
	m_dwDefaultStyle &= ~LVS_TYPEMASK;
	// Show selection all the time, so user can see current item even when
	// focus is elsewhere (ie, on file edit window)
	m_dwDefaultStyle |= LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EDITLABELS;

	m_bTreeMode =  GetOptionsMgr()->GetBool(OPT_TREE_MODE);
	m_bExpandSubdirs = GetOptionsMgr()->GetBool(OPT_DIRVIEW_EXPAND_SUBDIRS);
	m_nEscCloses = GetOptionsMgr()->GetInt(OPT_CLOSE_WITH_ESC);
	Options::DirColors::Load(GetOptionsMgr(), m_cachedColors);
	m_bUseColors = GetOptionsMgr()->GetBool(OPT_DIRCLR_USE_COLORS);
}

CDirView::~CDirView()
{
}

BEGIN_MESSAGE_MAP(CDirView, DpiAware::CDpiAwareWnd<CListView>)
	ON_WM_CONTEXTMENU()
	//{{AFX_MSG_MAP(CDirView)
	ON_WM_LBUTTONDBLCLK()
	ON_COMMAND_RANGE(ID_L2R, ID_R2L, OnDirCopy)
	ON_UPDATE_COMMAND_UI_RANGE(ID_L2R, ID_R2L, OnUpdateDirCopy)
	ON_COMMAND(ID_DIR_COPY_LEFT_TO_RIGHT, (OnCtxtDirCopy<SIDE_LEFT, SIDE_RIGHT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_LEFT_TO_RIGHT, (OnUpdateCtxtDirCopy<SIDE_LEFT, SIDE_RIGHT>))
	ON_COMMAND(ID_DIR_COPY_LEFT_TO_MIDDLE, (OnCtxtDirCopy<SIDE_LEFT, SIDE_MIDDLE>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_LEFT_TO_MIDDLE, (OnUpdateCtxtDirCopy<SIDE_LEFT, SIDE_MIDDLE>))
	ON_COMMAND(ID_DIR_COPY_RIGHT_TO_LEFT, (OnCtxtDirCopy<SIDE_RIGHT, SIDE_LEFT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_RIGHT_TO_LEFT, (OnUpdateCtxtDirCopy<SIDE_RIGHT, SIDE_LEFT>))
	ON_COMMAND(ID_DIR_COPY_RIGHT_TO_MIDDLE, (OnCtxtDirCopy<SIDE_RIGHT, SIDE_MIDDLE>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_RIGHT_TO_MIDDLE, (OnUpdateCtxtDirCopy<SIDE_RIGHT, SIDE_MIDDLE>))
	ON_COMMAND(ID_DIR_COPY_MIDDLE_TO_LEFT, (OnCtxtDirCopy<SIDE_MIDDLE, SIDE_LEFT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_MIDDLE_TO_LEFT, (OnUpdateCtxtDirCopy<SIDE_MIDDLE, SIDE_LEFT>))
	ON_COMMAND(ID_DIR_COPY_MIDDLE_TO_RIGHT, (OnCtxtDirCopy<SIDE_MIDDLE, SIDE_RIGHT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_MIDDLE_TO_RIGHT, (OnUpdateCtxtDirCopy<SIDE_MIDDLE, SIDE_RIGHT>))
	ON_COMMAND(ID_DIR_DEL_LEFT, OnCtxtDirDel<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_LEFT, OnUpdateCtxtDirDel<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_DEL_RIGHT, OnCtxtDirDel<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_MIDDLE, OnUpdateCtxtDirDel<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_DEL_MIDDLE, OnCtxtDirDel<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_RIGHT, OnUpdateCtxtDirDel<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_DEL_BOTH, OnCtxtDirDelBoth)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_BOTH, OnUpdateCtxtDirDelBoth)
	ON_COMMAND(ID_DIR_DEL_ALL, OnCtxtDirDelBoth)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_ALL, OnUpdateCtxtDirDelBoth)
	ON_COMMAND(ID_DIR_OPEN_LEFT, OnCtxtDirOpen<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_LEFT, OnUpdateCtxtDirOpen<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_OPEN_LEFT_WITH, OnCtxtDirOpenWith<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_LEFT_WITH, OnUpdateCtxtDirOpenWith<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_OPEN_LEFT_PARENT_FOLDER, OnCtxtDirOpenParentFolder<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_LEFT_PARENT_FOLDER, OnUpdateCtxtDirOpenParentFolder<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_OPEN_MIDDLE, OnCtxtDirOpen<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_MIDDLE, OnUpdateCtxtDirOpen<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_OPEN_MIDDLE_WITH, OnCtxtDirOpenWith<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_MIDDLE_WITH, OnUpdateCtxtDirOpenWith<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_MIDDLE_PARENT_FOLDER, OnUpdateCtxtDirOpenParentFolder<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_OPEN_MIDDLE_PARENT_FOLDER, OnCtxtDirOpenParentFolder<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_OPEN_RIGHT, OnCtxtDirOpen<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_RIGHT, OnUpdateCtxtDirOpen<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_OPEN_RIGHT_WITH, OnCtxtDirOpenWith<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_RIGHT_WITH, OnUpdateCtxtDirOpenWith<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_OPEN_RIGHT_PARENT_FOLDER, OnCtxtDirOpenParentFolder<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_RIGHT_PARENT_FOLDER, OnUpdateCtxtDirOpenParentFolder<SIDE_RIGHT>)
	ON_COMMAND(ID_POPUP_OPEN_WITH_UNPACKER, OnCtxtOpenWithUnpacker)
	ON_UPDATE_COMMAND_UI(ID_POPUP_OPEN_WITH_UNPACKER, OnUpdateCtxtOpenWithUnpacker)
	ON_COMMAND(ID_DIR_OPEN_LEFT_WITHEDITOR, OnCtxtDirOpenWithEditor<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_LEFT_WITHEDITOR, OnUpdateCtxtDirOpenWithEditor<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_OPEN_MIDDLE_WITHEDITOR, OnCtxtDirOpenWithEditor<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_MIDDLE_WITHEDITOR, OnUpdateCtxtDirOpenWithEditor<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_OPEN_RIGHT_WITHEDITOR, OnCtxtDirOpenWithEditor<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_RIGHT_WITHEDITOR, OnUpdateCtxtDirOpenWithEditor<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_COPY_LEFT_TO_BROWSE, OnCtxtDirCopyTo<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_COPY_MIDDLE_TO_BROWSE, OnCtxtDirCopyTo<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_COPY_RIGHT_TO_BROWSE, OnCtxtDirCopyTo<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_LEFT_TO_BROWSE, OnUpdateCtxtDirCopyTo<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_MIDDLE_TO_BROWSE, OnUpdateCtxtDirCopyTo<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_RIGHT_TO_BROWSE, OnUpdateCtxtDirCopyTo<SIDE_RIGHT>)
	ON_WM_DESTROY()
	ON_WM_CHAR()
	ON_WM_KEYDOWN()
	ON_COMMAND(ID_FIRSTDIFF, OnFirstdiff)
	ON_UPDATE_COMMAND_UI(ID_FIRSTDIFF, OnUpdateFirstdiff)
	ON_COMMAND(ID_LASTDIFF, OnLastdiff)
	ON_UPDATE_COMMAND_UI(ID_LASTDIFF, OnUpdateLastdiff)
	ON_COMMAND(ID_NEXTDIFF, OnNextdiff)
	ON_UPDATE_COMMAND_UI(ID_NEXTDIFF, OnUpdateNextdiff)
	ON_COMMAND(ID_PREVDIFF, OnPrevdiff)
	ON_UPDATE_COMMAND_UI(ID_PREVDIFF, OnUpdatePrevdiff)
	ON_COMMAND(ID_CURDIFF, OnCurdiff)
	ON_UPDATE_COMMAND_UI(ID_CURDIFF, OnUpdateCurdiff)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateSave)
	ON_MESSAGE(MSG_UI_UPDATE, OnUpdateUIMessage)
	ON_COMMAND(ID_REFRESH, OnRefresh)
	ON_UPDATE_COMMAND_UI(ID_REFRESH, OnUpdateRefresh)
	ON_WM_TIMER()
	ON_UPDATE_COMMAND_UI(ID_STATUS_RIGHTDIR_RO, OnUpdateStatusRightRO)
	ON_UPDATE_COMMAND_UI(ID_STATUS_MIDDLEDIR_RO, OnUpdateStatusMiddleRO)
	ON_UPDATE_COMMAND_UI(ID_STATUS_LEFTDIR_RO, OnUpdateStatusLeftRO)
	ON_COMMAND(ID_FILE_LEFT_READONLY, OnReadOnly<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_FILE_LEFT_READONLY, OnUpdateReadOnly<SIDE_LEFT>)
	ON_COMMAND(ID_FILE_MIDDLE_READONLY, OnReadOnly<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_FILE_MIDDLE_READONLY, OnUpdateReadOnly<SIDE_MIDDLE>)
	ON_COMMAND(ID_FILE_RIGHT_READONLY, OnReadOnly<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_FILE_RIGHT_READONLY, OnUpdateReadOnly<SIDE_RIGHT>)
	ON_COMMAND(ID_TOOLS_CUSTOMIZECOLUMNS, OnCustomizeColumns)
	ON_COMMAND(ID_TOOLS_GENERATEREPORT, OnToolsGenerateReport)
	ON_COMMAND(ID_TOOLS_GENERATEPATCH, OnToolsGeneratePatch)
	ON_MESSAGE(MSG_GENERATE_FLIE_COMPARE_REPORT, OnGenerateFileCmpReport)
	ON_COMMAND(ID_DIR_ZIP_LEFT, OnCtxtDirZip<DirItemEnumerator::Left>)
	ON_COMMAND(ID_DIR_ZIP_MIDDLE, OnCtxtDirZip<DirItemEnumerator::Middle>)
	ON_COMMAND(ID_DIR_ZIP_RIGHT, OnCtxtDirZip<DirItemEnumerator::Right>)
	ON_COMMAND(ID_DIR_ZIP_BOTH, OnCtxtDirZip<DirItemEnumerator::Original | DirItemEnumerator::Altered | DirItemEnumerator::BalanceFolders>)
	ON_COMMAND(ID_DIR_ZIP_ALL, OnCtxtDirZip<DirItemEnumerator::Original | DirItemEnumerator::Altered | DirItemEnumerator::BalanceFolders>)
	ON_COMMAND(ID_DIR_ZIP_BOTH_DIFFS_ONLY, OnCtxtDirZip<DirItemEnumerator::Original | DirItemEnumerator::Altered | DirItemEnumerator::BalanceFolders | DirItemEnumerator::DiffsOnly>)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_LEFT, OnUpdateCtxtDirCopyTo<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_MIDDLE, OnUpdateCtxtDirCopyTo<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_RIGHT, OnUpdateCtxtDirCopyTo<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_BOTH, OnUpdateCtxtDirCopyBothTo)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_ALL, OnUpdateCtxtDirCopyBothTo)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_BOTH_DIFFS_ONLY, OnUpdateCtxtDirCopyBothDiffsOnlyTo)
	ON_COMMAND(ID_DIR_SHELL_CONTEXT_MENU_LEFT, OnCtxtDirShellContextMenu<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_SHELL_CONTEXT_MENU_MIDDLE, OnCtxtDirShellContextMenu<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_SHELL_CONTEXT_MENU_RIGHT, OnCtxtDirShellContextMenu<SIDE_RIGHT>)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateSelectAll)
	ON_COMMAND_RANGE(ID_PREDIFF_MANUAL, ID_PREDIFF_AUTO, OnPluginPredifferMode)
	ON_UPDATE_COMMAND_UI_RANGE(ID_PREDIFF_MANUAL, ID_PREDIFF_AUTO, OnUpdatePluginPredifferMode)
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_LEFT, OnCopyPathnames<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_MIDDLE, OnCopyPathnames<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_RIGHT, OnCopyPathnames<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_BOTH, OnCopyBothPathnames)
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_ALL, OnCopyBothPathnames)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_LEFT, OnUpdateCtxtDirCopy2<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_MIDDLE, OnUpdateCtxtDirCopy2<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_RIGHT, OnUpdateCtxtDirCopy2<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_BOTH, OnUpdateCtxtDirCopyBoth2)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_ALL, OnUpdateCtxtDirCopyBoth2)
	ON_COMMAND(ID_DIR_COPY_FILENAMES, OnCopyFilenames)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_FILENAMES, OnUpdateCopyFilenames)
	ON_COMMAND(ID_DIR_COPY_LEFT_TO_CLIPBOARD, OnCopyToClipboard<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_COPY_MIDDLE_TO_CLIPBOARD, OnCopyToClipboard<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_COPY_RIGHT_TO_CLIPBOARD, OnCopyToClipboard<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_COPY_BOTH_TO_CLIPBOARD, OnCopyBothToClipboard)
	ON_COMMAND(ID_DIR_COPY_ALL_TO_CLIPBOARD, OnCopyBothToClipboard)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_LEFT_TO_CLIPBOARD, OnUpdateCtxtDirCopy2<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_MIDDLE_TO_CLIPBOARD, OnUpdateCtxtDirCopy2<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_RIGHT_TO_CLIPBOARD, OnUpdateCtxtDirCopy2<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_BOTH_TO_CLIPBOARD, OnUpdateCtxtDirCopyBoth2)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_ALL_TO_CLIPBOARD, OnUpdateCtxtDirCopyBoth2)
	ON_COMMAND(ID_DIR_ITEM_RENAME, OnItemRename)
	ON_UPDATE_COMMAND_UI(ID_DIR_ITEM_RENAME, OnUpdateItemRename)
	ON_COMMAND(ID_DIR_HIDE_FILENAMES, OnHideFilenames)
	ON_COMMAND(ID_DIR_MOVE_LEFT_TO_BROWSE, OnCtxtDirMoveTo<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_LEFT_TO_BROWSE, OnUpdateCtxtDirMoveTo<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_MOVE_MIDDLE_TO_BROWSE, OnCtxtDirMoveTo<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_MIDDLE_TO_BROWSE, OnUpdateCtxtDirMoveTo<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_MOVE_RIGHT_TO_BROWSE, OnCtxtDirMoveTo<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_RIGHT_TO_BROWSE, OnUpdateCtxtDirMoveTo<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_HIDE_FILENAMES, OnUpdateHideFilenames)
	ON_WM_SIZE()
	ON_COMMAND(ID_MERGE_DELETE, OnDelete)
	ON_UPDATE_COMMAND_UI(ID_MERGE_DELETE, OnUpdateDelete)
	ON_COMMAND(ID_RESCAN, OnMarkedRescan)
	ON_UPDATE_COMMAND_UI(ID_STATUS_DIFFNUM, OnUpdateStatusNum)
	ON_COMMAND(ID_VIEW_SHOWHIDDENITEMS, OnViewShowHiddenItems)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWHIDDENITEMS, OnUpdateViewShowHiddenItems)
	ON_COMMAND(ID_MERGE_COMPARE, OnMergeCompare)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE, OnUpdateMergeCompare)
	ON_COMMAND(ID_MERGE_COMPARE_LEFT1_LEFT2, OnMergeCompare2<SELECTIONTYPE_LEFT1LEFT2>)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_LEFT1_LEFT2, OnUpdateMergeCompare2<SELECTIONTYPE_LEFT1LEFT2>)
	ON_COMMAND(ID_MERGE_COMPARE_RIGHT1_RIGHT2, OnMergeCompare2<SELECTIONTYPE_RIGHT1RIGHT2>)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_RIGHT1_RIGHT2, OnUpdateMergeCompare2<SELECTIONTYPE_RIGHT1RIGHT2>)
	ON_COMMAND(ID_MERGE_COMPARE_LEFT1_RIGHT2, OnMergeCompare2<SELECTIONTYPE_LEFT1RIGHT2>)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_LEFT1_RIGHT2, OnUpdateMergeCompare2<SELECTIONTYPE_LEFT1RIGHT2>)
	ON_COMMAND(ID_MERGE_COMPARE_LEFT2_RIGHT1, OnMergeCompare2<SELECTIONTYPE_LEFT2RIGHT1>)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_LEFT2_RIGHT1, OnUpdateMergeCompare2<SELECTIONTYPE_LEFT2RIGHT1>)
	ON_COMMAND(ID_MERGE_COMPARE_NONHORIZONTALLY, OnMergeCompareNonHorizontally)
	ON_COMMAND(ID_MERGE_COMPARE_XML, OnMergeCompareXML)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_XML, OnUpdateMergeCompare)
	ON_COMMAND_RANGE(ID_MERGE_COMPARE_HEX, ID_MERGE_COMPARE_IMAGE, OnMergeCompareAs)
	ON_UPDATE_COMMAND_UI_RANGE(ID_MERGE_COMPARE_HEX, ID_MERGE_COMPARE_IMAGE, OnUpdateMergeCompare)
	ON_COMMAND(ID_VIEW_TREEMODE, OnViewTreeMode)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TREEMODE, OnUpdateViewTreeMode)
	ON_COMMAND(ID_VIEW_EXPAND_ALLSUBDIRS, OnViewExpandAllSubdirs)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EXPAND_ALLSUBDIRS, OnUpdateViewExpandAllSubdirs)
	ON_COMMAND(ID_VIEW_COLLAPSE_ALLSUBDIRS, OnViewCollapseAllSubdirs)
	ON_UPDATE_COMMAND_UI(ID_VIEW_COLLAPSE_ALLSUBDIRS, OnUpdateViewCollapseAllSubdirs)
	ON_COMMAND(ID_VIEW_SWAPPANES, OnViewSwapPanes)
	ON_COMMAND(ID_VIEW_DIR_STATISTICS, OnViewCompareStatistics)
	ON_COMMAND(ID_OPTIONS_SHOWDIFFERENT, OnOptionsShowDifferent)
	ON_COMMAND(ID_OPTIONS_SHOWIDENTICAL, OnOptionsShowIdentical)
	ON_COMMAND(ID_OPTIONS_SHOWUNIQUELEFT, OnOptionsShowUniqueLeft)
	ON_COMMAND(ID_OPTIONS_SHOWUNIQUEMIDDLE, OnOptionsShowUniqueMiddle)
	ON_COMMAND(ID_OPTIONS_SHOWUNIQUERIGHT, OnOptionsShowUniqueRight)
	ON_COMMAND(ID_OPTIONS_SHOWBINARIES, OnOptionsShowBinaries)
	ON_COMMAND(ID_OPTIONS_SHOWSKIPPED, OnOptionsShowSkipped)
	ON_COMMAND(ID_OPTIONS_SHOWDIFFERENTLEFTONLY, OnOptionsShowDifferentLeftOnly)
	ON_COMMAND(ID_OPTIONS_SHOWDIFFERENTMIDDLEONLY, OnOptionsShowDifferentMiddleOnly)
	ON_COMMAND(ID_OPTIONS_SHOWDIFFERENTRIGHTONLY, OnOptionsShowDifferentRightOnly)
	ON_COMMAND(ID_OPTIONS_SHOWMISSINGLEFTONLY, OnOptionsShowMissingLeftOnly)
	ON_COMMAND(ID_OPTIONS_SHOWMISSINGMIDDLEONLY, OnOptionsShowMissingMiddleOnly)
	ON_COMMAND(ID_OPTIONS_SHOWMISSINGRIGHTONLY, OnOptionsShowMissingRightOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWDIFFERENT, OnUpdateOptionsShowdifferent)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWIDENTICAL, OnUpdateOptionsShowidentical)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWUNIQUELEFT, OnUpdateOptionsShowuniqueleft)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWUNIQUEMIDDLE, OnUpdateOptionsShowuniquemiddle)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWUNIQUERIGHT, OnUpdateOptionsShowuniqueright)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWBINARIES, OnUpdateOptionsShowBinaries)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWSKIPPED, OnUpdateOptionsShowSkipped)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWDIFFERENTLEFTONLY, OnUpdateOptionsShowDifferentLeftOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWDIFFERENTMIDDLEONLY, OnUpdateOptionsShowDifferentMiddleOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWDIFFERENTRIGHTONLY, OnUpdateOptionsShowDifferentRightOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWMISSINGLEFTONLY, OnUpdateOptionsShowMissingLeftOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWMISSINGMIDDLEONLY, OnUpdateOptionsShowMissingMiddleOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWMISSINGRIGHTONLY, OnUpdateOptionsShowMissingRightOnly)
	ON_COMMAND(ID_FILE_ENCODING, OnFileEncoding)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	//}}AFX_MSG_MAP
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnColumnClick)
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnItemChanged)
	ON_NOTIFY_REFLECT(LVN_BEGINLABELEDIT, OnBeginLabelEdit)
	ON_NOTIFY_REFLECT(LVN_ENDLABELEDIT, OnEndLabelEdit)
	ON_NOTIFY_REFLECT(NM_CLICK, OnClick)
	ON_NOTIFY_REFLECT(LVN_BEGINDRAG, OnBeginDrag)
 	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
	ON_BN_CLICKED(IDC_COMPARISON_STOP, OnBnClickedComparisonStop)
	ON_BN_CLICKED(IDC_COMPARISON_PAUSE, OnBnClickedComparisonPause)
	ON_BN_CLICKED(IDC_COMPARISON_CONTINUE, OnBnClickedComparisonContinue)
	ON_MESSAGE(WM_DPICHANGED_BEFOREPARENT, OnDpiChangedBeforeParent)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDirView diagnostics

#ifdef _DEBUG

CDirDoc* CDirView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDirDoc)));
	return (CDirDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CDirView message handlers

void CDirView::OnInitialUpdate()
{
	const int iconCX = [this]() {
		const int cx = GetSystemMetrics(SM_CXSMICON);
		if (cx < 24)
			return 16;
		if (cx < 32)
			return 24;
		if (cx < 48)
			return 32;
		return 48;
	}();
	const int iconCY = iconCX;
	__super::OnInitialUpdate();
	m_pList = &GetListCtrl();
	m_pIList.reset(new IListCtrlImpl(m_pList->m_hWnd));
	GetDocument()->SetDirView(this);
	m_pColItems.reset(new DirViewColItems(GetDocument()->m_nDirs));

	m_pList->SendMessage(CCM_SETUNICODEFORMAT, TRUE, 0);

	// Load user-selected font
	if (GetOptionsMgr()->GetBool(OPT_FONT_DIRCMP + OPT_FONT_USECUSTOM))
	{
		m_font.CreateFontIndirect(&GetMainFrame()->m_lfDir);
		CWnd::SetFont(&m_font, TRUE);
	}

	if (m_bUseColors)
		m_pList->SetBkColor(m_cachedColors.clrDirMargin);

	// Replace standard header with sort header
	HWND hWnd = ListView_GetHeader(m_pList->m_hWnd);
	if (hWnd != nullptr)
		m_ctlSortHeader.SubclassWindow(hWnd);

	// Load the icons used for the list view (to reflect diff status)
	// NOTE: these must be in the exactly the same order as in the `enum`
	// definition in the DirActions.h file (ref: DIFFIMG_LUNIQUE)
	VERIFY(m_imageList.Create(iconCX, iconCY, ILC_COLOR32 | ILC_MASK, 15, 1));
	int icon_ids[] = {
		IDI_LFILE, IDI_MFILE, IDI_RFILE,
		IDI_MRFILE, IDI_LRFILE, IDI_LMFILE,
		IDI_NOTEQUALFILE, IDI_EQUALFILE, IDI_FILE, 
		IDI_EQUALBINARY, IDI_BINARYDIFF,
		IDI_LFOLDER, IDI_MFOLDER, IDI_RFOLDER,
		IDI_MRFOLDER, IDI_LRFOLDER, IDI_LMFOLDER,
		IDI_FILESKIP, IDI_FOLDERSKIP,
		IDI_NOTEQUALFOLDER, IDI_EQUALFOLDER, IDI_FOLDER,
		IDI_COMPARE_ERROR,
		IDI_FOLDERUP, IDI_FOLDERUP_DISABLE,
		IDI_COMPARE_ABORTED,
		IDI_NOTEQUALTEXTFILE, IDI_EQUALTEXTFILE,
		IDI_NOTEQUALIMAGE, IDI_EQUALIMAGE, 
	};
	for (auto id : icon_ids)
		VERIFY(-1 != m_imageList.Add((HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, iconCX, iconCY, 0)));
	m_pList->SetImageList(&m_imageList, LVSIL_SMALL);

	// Load the icons used for the list view (expanded/collapsed state icons)
	VERIFY(m_imageState.Create(iconCX, iconCY, ILC_COLOR32 | ILC_MASK, 15, 1));
	for (auto id : { IDI_TREE_STATE_COLLAPSED, IDI_TREE_STATE_EXPANDED })
		VERIFY(-1 != m_imageState.Add((HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, iconCX, iconCY, 0)));

	// Restore column orders as they had them last time they ran
	m_pColItems->LoadColumnOrders(
		GetOptionsMgr()->GetString(GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_ORDERS : OPT_DIRVIEW3_COLUMN_ORDERS));

	// Display column headers (in appropriate order)
	ReloadColumns();

	// Show selection across entire row.u
	// Also allow user to rearrange columns via drag&drop of headers.
	// Also enable infotips.
	DWORD exstyle = LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_INFOTIP;
	m_pList->SetExtendedStyle(exstyle);
}

BOOL CDirView::PreCreateWindow(CREATESTRUCT& cs)
{
	__super::PreCreateWindow(cs);
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return TRUE;
}

/**
 * @brief Called before compare is started.
 * CDirDoc calls this function before new compare is started, so this
 * is good place to setup GUI for compare.
 * @param [in] pCompareStats Pointer to class having current compare stats.
 */
void CDirView::StartCompare(CompareStats *pCompareStats)
{
	if (m_pCmpProgressBar == nullptr)
		m_pCmpProgressBar.reset(new DirCompProgressBar());

	if (!::IsWindow(m_pCmpProgressBar->GetSafeHwnd()))
		m_pCmpProgressBar->Create(GetParentFrame());

	m_pCmpProgressBar->SetCompareStat(pCompareStats);
	m_pCmpProgressBar->StartUpdating();

	GetParentFrame()->ShowControlBar(m_pCmpProgressBar.get(), TRUE, FALSE);

	m_compareStart = clock();
}

/**
 * @brief Called when folder compare row is double-clicked with mouse.
 * Selected item is opened to folder or file compare.
 */
void CDirView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	LVHITTESTINFO lvhti;
	lvhti.pt = point;
	m_pList->SubItemHitTest(&lvhti);
	if (lvhti.iItem >= 0)
	{
		const DIFFITEM& di = GetDiffItem(lvhti.iItem);
		if (m_bTreeMode && GetDiffContext().m_bRecursive && di.diffcode.isDirectory())
		{
			if (di.customFlags & ViewCustomFlags::EXPANDED)
				CollapseSubdir(lvhti.iItem);
			else
				ExpandSubdir(lvhti.iItem);
		}
		else
		{
			CWaitCursor waitstatus;
			OpenSelection();
		}
	}
	__super::OnLButtonDblClk(nFlags, point);
}

/**
 * @brief Load or reload the columns (headers) of the list view
 */
void CDirView::ReloadColumns()
{
	LoadColumnHeaderItems();

	UpdateColumnNames();
	m_pColItems->LoadColumnWidths(
		GetOptionsMgr()->GetString(GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS),
		std::bind(&CListCtrl::SetColumnWidth, m_pList, _1, _2), GetDefColumnWidth());
	SetColAlignments();
}

/**
 * @brief Redisplay items in subfolder
 * @param [in] diffpos First item position in subfolder.
 * @param [in] level Indent level
 * @param [in,out] index Index of the item to be inserted.
 * @param [in,out] alldiffs Number of different items
 */
void CDirView::RedisplayChildren(DIFFITEM *diffpos, int level, UINT &index, int &alldiffs)
{
	const CDiffContext &ctxt = GetDiffContext();
	while (diffpos != nullptr)
	{
		DIFFITEM *curdiffpos = diffpos;
		const DIFFITEM &di = ctxt.GetNextSiblingDiffPosition(diffpos);

		if (di.diffcode.isResultDiff() || (!di.diffcode.existAll() && !di.diffcode.isResultFiltered()))
			++alldiffs;

		bool bShowable = IsShowable(ctxt, di, m_dirfilter);
		if (bShowable)
		{
			if (m_bTreeMode)
			{
				AddNewItem(index, curdiffpos, I_IMAGECALLBACK, level);
				index++;
				if (di.HasChildren())
				{
					m_pList->SetItemState(index - 1, INDEXTOSTATEIMAGEMASK((di.customFlags & ViewCustomFlags::EXPANDED) ? 2 : 1), LVIS_STATEIMAGEMASK);
					if (di.customFlags & ViewCustomFlags::EXPANDED)
						RedisplayChildren(ctxt.GetFirstChildDiffPosition(curdiffpos), level + 1, index, alldiffs);
				}
			}
			else
			{
				if (!ctxt.m_bRecursive || !di.diffcode.isDirectory() || !di.diffcode.existAll())
				{
					AddNewItem(index, curdiffpos, I_IMAGECALLBACK, 0);
					index++;
				}
				if (di.HasChildren())
				{
					RedisplayChildren(ctxt.GetFirstChildDiffPosition(curdiffpos), level + 1, index, alldiffs);
				}
			}
		}
	}
	m_firstDiffItem.reset();
	m_lastDiffItem.reset();
}

/**
 * @brief Redisplay folder compare view.
 * This function clears folder compare view and then adds
 * items from current compare to it.
 */
void CDirView::Redisplay()
{
	const CDirDoc *pDoc = GetDocument();
	const CDiffContext &ctxt = GetDiffContext();
	PathContext pathsParent;
	CImageList emptyImageList;

	UINT cnt = 0;
	// Disable redrawing while adding new items
	SetRedraw(FALSE);

	DeleteAllDisplayItems();

	m_pList->SetImageList((m_bTreeMode && ctxt.m_bRecursive) ? &m_imageState : &emptyImageList, LVSIL_STATE);

	// If non-recursive compare, add special item(s)
	if (!ctxt.m_bRecursive ||
		CheckAllowUpwardDirectory(ctxt, pDoc->m_pTempPathContext, pathsParent) == AllowUpwardDirectory::ParentIsTempPath)
	{
		cnt += AddSpecialItems();
	}

	int alldiffs = 0;
	DIFFITEM *diffpos = ctxt.GetFirstDiffPosition();
	RedisplayChildren(diffpos, 0, cnt, alldiffs);
	if (pDoc->m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPLETED)
		GetParentFrame()->SetLastCompareResult(alldiffs);
	SortColumnsAppropriately();
	SetRedraw(TRUE);
}

/**
 * @brief User right-clicked somewhere in this view
 */
void CDirView::OnContextMenu(CWnd*, CPoint point)
{
	if (GetListCtrl().GetItemCount() == 0)
		return;
	// Make sure window is active
	GetParentFrame()->ActivateFrame();

	int i = 0;
	if (point.x == -1 && point.y == -1)
	{
		//keystroke invocation
		CRect rect;
		GetClientRect(rect);
		ClientToScreen(rect);

		point = rect.TopLeft();
		point.Offset(5, 5);
	}
	else
	{
		// Check if user right-clicked on header
		// convert screen coordinates to client coordinates of listview
		CPoint insidePt = point;
		GetListCtrl().ScreenToClient(&insidePt);
		// TODO: correct for hscroll ?
		// Ask header control if click was on one of its header items
		HDHITTESTINFO hhti = { 0 };
		hhti.pt = insidePt;
		int col = static_cast<int>(GetListCtrl().GetHeaderCtrl()->SendMessage(HDM_HITTEST, 0, (LPARAM) & hhti));
		if (col >= 0)
		{
			// Presumably hhti.flags & HHT_ONHEADER is true
			HeaderContextMenu(point, m_pColItems->ColPhysToLog(col));
			return;
		}
		// bail out if point is not in any row
		LVHITTESTINFO lhti = { 0 };
		insidePt = point;
		ScreenToClient(&insidePt);
		lhti.pt = insidePt;
		i = GetListCtrl().HitTest(insidePt);
		TRACE(_T("i=%d\n"), i);
		if (i < 0)
			return;
	}

	ListContextMenu(point, i);
}

/**
 * @brief Format context menu string and disable item if it cannot be applied.
 */
static void NTAPI FormatContextMenu(BCMenu *pPopup, UINT uIDItem, int n1, int n2 = 0, int n3 = 0)
{
	CString s1, s2;
	pPopup->GetMenuText(uIDItem, s1, MF_BYCOMMAND);
	s2.FormatMessage(s1, NumToStr(n1).c_str(), NumToStr(n2).c_str(), NumToStr(n3).c_str());
	pPopup->SetMenuText(uIDItem, s2, MF_BYCOMMAND);
	if (n1 == 0)
	{
		pPopup->EnableMenuItem(uIDItem, MF_GRAYED);
	}
}

/**
 * @brief Toggle context menu item
 */
static void NTAPI CheckContextMenu(BCMenu *pPopup, UINT uIDItem, BOOL bCheck)
{
	if (bCheck)
		pPopup->CheckMenuItem(uIDItem, MF_CHECKED);
	else
		pPopup->CheckMenuItem(uIDItem, MF_UNCHECKED);
}

/**
 * @brief User right-clicked in listview rows
 */
void CDirView::ListContextMenu(CPoint point, int /*i*/)
{
	CDirDoc* pDoc = GetDocument();
	BCMenu menu;
	VERIFY(menu.LoadMenu(IDR_POPUP_DIRVIEW));
	VERIFY(menu.LoadToolbar(IDR_MAINFRAME));
	theApp.TranslateMenu(menu.m_hMenu);

	// 1st submenu of IDR_POPUP_DIRVIEW is for item popup
	BCMenu *pPopup = static_cast<BCMenu*>(menu.GetSubMenu(0));
	ASSERT(pPopup != nullptr);

	if (pDoc->m_nDirs < 3)
	{
		pPopup->RemoveMenu(ID_DIR_COPY_LEFT_TO_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_MIDDLE_TO_LEFT, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_MIDDLE_TO_RIGHT, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_MIDDLE_TO_BROWSE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_RIGHT_TO_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_MOVE_MIDDLE_TO_BROWSE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_DEL_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_DEL_ALL, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_OPEN_MIDDLE, MF_BYCOMMAND);

		for (int i = 0; i < pPopup->GetMenuItemCount(); ++i)
		{
			if (pPopup->GetMenuItemID(i) == ID_DIR_HIDE_FILENAMES)
				pPopup->RemoveMenu(i + 3, MF_BYPOSITION);
		}

		pPopup->RemoveMenu(ID_DIR_OPEN_MIDDLE_WITHEDITOR, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_OPEN_MIDDLE_WITH, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_PATHNAMES_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_PATHNAMES_ALL, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_MIDDLE_TO_CLIPBOARD, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_ALL_TO_CLIPBOARD, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_ZIP_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_ZIP_ALL, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_SHELL_CONTEXT_MENU_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_MERGE_COMPARE_NONHORIZONTALLY, MF_BYCOMMAND);
	}
	else
	{
		pPopup->RemoveMenu(ID_DIR_COPY_PATHNAMES_BOTH, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_BOTH_TO_CLIPBOARD, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_ZIP_BOTH, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_DEL_BOTH, MF_BYCOMMAND);
		pPopup->RemoveMenu(2, MF_BYPOSITION); // Compare Non-horizontally
	}

	CMenu menuPluginsHolder;
	menuPluginsHolder.LoadMenu(IDR_POPUP_PLUGINS_SETTINGS);
	theApp.TranslateMenu(menuPluginsHolder.m_hMenu);
	String s = _("Plugin Settings");
	pPopup->AppendMenu(MF_SEPARATOR);
	pPopup->AppendMenu(MF_POPUP, static_cast<int>(reinterpret_cast<uintptr_t>(menuPluginsHolder.m_hMenu)), s.c_str());

	CFrameWnd *pFrame = GetTopLevelFrame();
	ASSERT(pFrame != nullptr);
	pFrame->m_bAutoMenuEnable = FALSE;
	// invoke context menu
	// this will invoke all the OnUpdate methods to enable/disable the individual items
	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
			AfxGetMainWnd());

	pFrame->m_bAutoMenuEnable = TRUE;
}

/**
 * @brief User right-clicked on specified logical column
 */
void CDirView::HeaderContextMenu(CPoint point, int /*i*/)
{
	BCMenu menu;
	VERIFY(menu.LoadMenu(IDR_POPUP_DIRVIEW));
	VERIFY(menu.LoadToolbar(IDR_MAINFRAME));
	theApp.TranslateMenu(menu.m_hMenu);
	// 2nd submenu of IDR_POPUP_DIRVIEW is for header popup
	BCMenu* pPopup = static_cast<BCMenu *>(menu.GetSubMenu(1));
	ASSERT(pPopup != nullptr);

	// invoke context menu
	// this will invoke all the OnUpdate methods to enable/disable the individual items
	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
			AfxGetMainWnd());
}

/**
 * @brief Gets Explorer's context menu for a group of selected files.
 *
 * @param [in] Side whether to get context menu for the files from the left or
 *   right side.
 * @retval true menu successfully retrieved.
 * @retval falsea an error occurred while retrieving the menu.
 */
bool CDirView::ListShellContextMenu(SIDE_TYPE stype)
{
	CShellContextMenu* shellContextMenu;
	switch (stype) {
	case SIDE_MIDDLE:
		shellContextMenu = m_pShellContextMenuMiddle.get(); break;
	case SIDE_RIGHT:
		shellContextMenu = m_pShellContextMenuRight.get(); break;
	default:
		shellContextMenu = m_pShellContextMenuLeft.get(); break;
	}
	shellContextMenu->Initialize();
	ApplyFolderNameAndFileName(SelBegin(), SelEnd(), stype, GetDiffContext(),
		[&](const String& path, const String& filename) { shellContextMenu->AddItem(path, filename); });
	return shellContextMenu->RequeryShellContextMenu();
}

/**
 * @brief User chose (main menu) Copy from right to left
 */
void CDirView::OnDirCopy(UINT id)
{
	bool to_right = (id == ID_L2R) ? true : false;
	if (GetDocument()->m_nDirs < 3)
	{
		if (to_right)
			DoDirAction(&DirActions::Copy<SIDE_LEFT, SIDE_RIGHT>, _("Copying files..."));
		else
			DoDirAction(&DirActions::Copy<SIDE_RIGHT, SIDE_LEFT>, _("Copying files..."));
	}
	else
	{
		if (to_right)
		{
			switch (m_nActivePane)
			{
			case 0:
				DoDirAction(&DirActions::Copy<SIDE_LEFT, SIDE_MIDDLE>, _("Copying files..."));
				break;
			case 1:
			case 2:
				DoDirAction(&DirActions::Copy<SIDE_MIDDLE, SIDE_RIGHT>, _("Copying files..."));
				break;
			}
		}
		else
		{
			switch (m_nActivePane)
			{
			case 0:
			case 1:
				DoDirAction(&DirActions::Copy<SIDE_MIDDLE, SIDE_LEFT>, _("Copying files..."));
				break;
			case 2:
				DoDirAction(&DirActions::Copy<SIDE_RIGHT, SIDE_MIDDLE>, _("Copying files..."));
				break;
			}
		}
	}
}

/// User chose (context men) Copy from right to left
template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
void CDirView::OnCtxtDirCopy()
{
	DoDirAction(&DirActions::Copy<srctype, dsttype>, _("Copying files..."));
}

/// User chose (context menu) Copy left to...
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirCopyTo()
{
	DoDirActionTo(stype, &DirActions::CopyTo<stype>, _("Copying files..."));
}

/// Update context menu Copy Right to Left item
template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
void CDirView::OnUpdateCtxtDirCopy(CCmdUI* pCmdUI)
{
	DoUpdateDirCopy<srctype, dsttype>(pCmdUI, eContext);
}

/// Update main menu Copy Right to Left item
void CDirView::OnUpdateDirCopy(CCmdUI* pCmdUI)
{
	bool to_right = pCmdUI->m_nID == ID_L2R ? true : false;
	if (GetDocument()->m_nDirs < 3)
	{
		if (to_right)
			DoUpdateDirCopy<SIDE_LEFT, SIDE_RIGHT>(pCmdUI, eContext);
		else
			DoUpdateDirCopy<SIDE_RIGHT, SIDE_LEFT>(pCmdUI, eContext);
	}
	else
	{
		if (to_right)
		{
			switch (m_nActivePane)
			{
			case 0:
				DoUpdateDirCopy<SIDE_LEFT, SIDE_MIDDLE>(pCmdUI, eContext);
				break;
			case 1:
			case 2:
				DoUpdateDirCopy<SIDE_MIDDLE, SIDE_RIGHT>(pCmdUI, eContext);
				break;
			}
		}
		else
		{
			switch (m_nActivePane)
			{
			case 0:
			case 1:
				DoUpdateDirCopy<SIDE_MIDDLE, SIDE_LEFT>(pCmdUI, eContext);
				break;
			case 2:
				DoUpdateDirCopy<SIDE_RIGHT, SIDE_MIDDLE>(pCmdUI, eContext);
				break;
			}
		}
	}
}

void CDirView::DoDirAction(DirActions::method_type func, const String& status_message)
{
	CWaitCursor waitstatus;

	try {
		// First we build a list of desired actions
		FileActionScript actionScript;
		DirItemWithIndexIterator begin(m_pIList.get(), -1, true);
		DirItemWithIndexIterator end;
		FileActionScript *rsltScript;
		rsltScript = std::accumulate(begin, end, &actionScript, MakeDirActions(func));
		ASSERT(rsltScript == &actionScript);
		// Now we prompt, and execute actions
		ConfirmAndPerformActions(actionScript);
	} catch (ContentsChangedException& e) {
		AfxMessageBox(e.m_msg.c_str(), MB_ICONWARNING);
	} catch (FileOperationException& e) {
		AfxMessageBox(e.m_msg.c_str(), MB_ICONWARNING);
	}
}

void CDirView::DoDirActionTo(SIDE_TYPE stype, DirActions::method_type func, const String& status_message)
{
	String destPath;
	String startPath(m_lastCopyFolder);
	String selectfolder_title;

	if (stype == SIDE_LEFT)
		selectfolder_title = _("Left side - select destination folder:");
	else if (stype == SIDE_MIDDLE)
		selectfolder_title = _("Middle side - select destination folder:");
	else if (stype == SIDE_RIGHT)
		selectfolder_title = _("Right side - select destination folder:");

	if (!SelectFolder(destPath, startPath.c_str(), selectfolder_title))
		return;

	m_lastCopyFolder = destPath;
	CWaitCursor waitstatus;

	try {
		// First we build a list of desired actions
		FileActionScript actionScript;
		actionScript.m_destBase = destPath;
		DirItemWithIndexIterator begin(m_pIList.get(), -1, true);
		DirItemWithIndexIterator end;
		FileActionScript *rsltScript;
		rsltScript = std::accumulate(begin, end, &actionScript, MakeDirActions(func));
		ASSERT(rsltScript == &actionScript);
		// Now we prompt, and execute actions
		ConfirmAndPerformActions(actionScript);
	} catch (ContentsChangedException& e) {
		AfxMessageBox(e.m_msg.c_str(), MB_ICONWARNING);
	}
}

// Confirm with user, then perform the action list
void CDirView::ConfirmAndPerformActions(FileActionScript & actionList)
{
	if (actionList.GetActionItemCount() == 0) // Not sure it is possible to get right-click menu without
		return;    // any selected items, but may as well be safe

	ASSERT(actionList.GetActionItemCount()>0); // Or else the update handler got it wrong

	// Set parent window so modality is correct and correct window gets focus
	// after dialogs.
	actionList.SetParentWindow(this->GetSafeHwnd());
	
	try {
		ConfirmActionList(GetDiffContext(), actionList);
	} catch (ConfirmationNeededException& e) {
		ConfirmFolderCopyDlg dlg;
		dlg.m_caption = e.m_caption;
		dlg.m_question = e.m_question;
		dlg.m_fromText = e.m_fromText;
		dlg.m_toText = e.m_toText;
		dlg.m_fromPath = e.m_fromPath;
		dlg.m_toPath = e.m_toPath;
		INT_PTR ans = dlg.DoModal();
		if (ans != IDOK && ans != IDYES)
			return;
	}
	PerformActionList(actionList);
}

/**
 * @brief Perform an array of actions
 * @note There can be only COPY or DELETE actions, not both!
 */
void CDirView::PerformActionList(FileActionScript & actionScript)
{
	// Check option and enable putting deleted items to Recycle Bin
	if (GetOptionsMgr()->GetBool(OPT_USE_RECYCLE_BIN))
		actionScript.UseRecycleBin(true);
	else
		actionScript.UseRecycleBin(false);

	actionScript.SetParentWindow(GetMainFrame()->GetSafeHwnd());

	theApp.AddOperation();
	bool succeeded = actionScript.Run();
	if (succeeded)
		UpdateAfterFileScript(actionScript);
	theApp.RemoveOperation();
	if (!succeeded && !actionScript.IsCanceled())
		throw FileOperationException(_T("File operation failed"));
}

/**
 * @brief Update results after running FileActionScript.
 * This functions is called after script is finished to update
 * results (including UI).
 * @param [in] actionlist Script that was run.
 */
void CDirView::UpdateAfterFileScript(FileActionScript & actionList)
{
	bool bItemsRemoved = false;
	int curSel = GetFirstSelectedInd();
	CDiffContext& ctxt = GetDiffContext();
	while (actionList.GetActionItemCount()>0)
	{
		// Start handling from tail of list, so removing items
		// doesn't invalidate our item indexes.
		FileActionItem act = actionList.RemoveTailActionItem();

		// Update doc (difflist)
		UPDATEITEM_TYPE updatetype = UpdateDiffAfterOperation(act, ctxt, GetDiffItem(act.context));
		if (updatetype == UPDATEITEM_REMOVE)
		{
			DeleteItem(act.context, true);
			bItemsRemoved = true;
		}
		else if (updatetype == UPDATEITEM_UPDATE)
			UpdateDiffItemStatus(act.context);
	}
	
	// Make sure selection is at sensible place if all selected items
	// were removed.
	if (bItemsRemoved)
	{
		UINT selected = GetSelectedCount();
		if (selected == 0)
		{
			if (curSel < 1)
				++curSel;
			MoveFocus(0, curSel - 1, selected);
		}
	}
}

Counts CDirView::Count(DirActions::method_type2 func) const
{
	return ::Count(SelBegin(), SelEnd(), MakeDirActions(func));
}

/// Should Copy to Left be enabled or disabled ? (both main menu & context menu use this)
template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
void CDirView::DoUpdateDirCopy(CCmdUI* pCmdUI, eMenuType menuType)
{
	Counts counts = Count(&DirActions::IsItemCopyableOnTo<srctype, dsttype>);
	pCmdUI->Enable(counts.count > 0);
	if (menuType == eContext)
		pCmdUI->SetText(FormatMenuItemString(srctype, dsttype, counts.count, counts.total).c_str());
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CDirView::UpdateResources()
{
	UpdateColumnNames();
	GetParentFrame()->UpdateResources();
}

/**
 * @brief User just clicked a column, so perform sort
 */
void CDirView::OnColumnClick(NMHDR *pNMHDR, LRESULT *pResult)
{
	// set sort parameters and handle ascending/descending
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*) pNMHDR;
	int oldSortColumn = GetOptionsMgr()->GetInt((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3);
	int sortcol = m_pColItems->ColPhysToLog(pNMListView->iSubItem);
	if (sortcol == oldSortColumn)
	{
		// Swap direction
		bool bSortAscending = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SORT_ASCENDING);
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SORT_ASCENDING, !bSortAscending);
	}
	else
	{
		GetOptionsMgr()->SaveOption((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3, sortcol);
		// most columns start off ascending, but not dates
		bool bSortAscending = m_pColItems->IsDefaultSortAscending(sortcol);
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SORT_ASCENDING, bSortAscending);
	}

	SortColumnsAppropriately();
	*pResult = 0;
}

void CDirView::SortColumnsAppropriately()
{
	int sortCol = GetOptionsMgr()->GetInt((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3);
	if (sortCol == -1 || sortCol >= m_pColItems->GetColCount())
		return;

	bool bSortAscending = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SORT_ASCENDING);
	m_ctlSortHeader.SetSortImage(m_pColItems->ColLogToPhys(sortCol), bSortAscending);
	//sort using static CompareFunc comparison function
	CompareState cs(&GetDiffContext(), m_pColItems.get(), sortCol, bSortAscending, m_bTreeMode);
	GetListCtrl().SortItems(cs.CompareFunc, reinterpret_cast<DWORD_PTR>(&cs));

	m_firstDiffItem.reset();
	m_lastDiffItem.reset();
}

/// Do any last minute work as view closes
void CDirView::OnDestroy()
{
	DeleteAllDisplayItems();

	{
		const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_ORDERS : OPT_DIRVIEW3_COLUMN_ORDERS;
		GetOptionsMgr()->SaveOption(keyname, m_pColItems->SaveColumnOrders());
	}
	{
		const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS;
		GetOptionsMgr()->SaveOption(keyname,
			m_pColItems->SaveColumnWidths(std::bind(&CListCtrl::GetColumnWidth, m_pList, _1)));
	}

	__super::OnDestroy();

	GetMainFrame()->ClearStatusbarItemCount();
}

/**
 * @brief Open selected item when user presses ENTER key.
 */
void CDirView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_RETURN)
	{
		int sel = GetFocusedItem();
		if (sel >= 0)
		{
			const DIFFITEM& di = GetDiffItem(sel);
			if (m_bTreeMode && GetDiffContext().m_bRecursive && di.diffcode.isDirectory())
			{
				if (di.customFlags & ViewCustomFlags::EXPANDED)
					CollapseSubdir(sel);
				else
					ExpandSubdir(sel);
			}
			else
			{
				CWaitCursor waitstatus;
				OpenSelection();
			}
		}
	}
	__super::OnChar(nChar, nRepCnt, nFlags);
}

/**
 * @brief Expand/collapse subfolder when "+/-" icon is clicked.
 */
void CDirView::OnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNM = (LPNMITEMACTIVATE)pNMHDR;
	LVHITTESTINFO lvhti;
	lvhti.pt = pNM->ptAction;
	m_pList->SubItemHitTest(&lvhti);
	if (lvhti.flags == LVHT_ONITEMSTATEICON)
	{
		const DIFFITEM &di = GetDiffItem(pNM->iItem);
		if (di.customFlags & ViewCustomFlags::EXPANDED)
			CollapseSubdir(pNM->iItem);
		else
			ExpandSubdir(pNM->iItem);
	}

	*pResult = 0;
}

/**
 * @brief Collapse subfolder
 * @param [in] sel Folder item index in listview.
 */
void CDirView::CollapseSubdir(int sel)
{
	DIFFITEM& dip = this->GetDiffItem(sel);
	if (!m_bTreeMode || !(dip.customFlags & ViewCustomFlags::EXPANDED) || !dip.HasChildren())
		return;

	m_pList->SetRedraw(FALSE);	// Turn off updating (better performance)

	dip.customFlags &= ~ViewCustomFlags::EXPANDED;
	m_pList->SetItemState(sel, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);

	int count = m_pList->GetItemCount();
	for (int i = sel + 1; i < count; i++)
	{
		const DIFFITEM& di = GetDiffItem(i);
		if (!di.IsAncestor(&dip))
			break;
		m_pList->DeleteItem(i--);
		count--;
	}

	m_pList->SetRedraw(TRUE);	// Turn updating back on
}

/**
 * @brief Expand subfolder
 * @param [in] sel Folder item index in listview.
 */
void CDirView::ExpandSubdir(int sel, bool bRecursive)
{
	DIFFITEM& dip = GetDiffItem(sel);
	if (!m_bTreeMode || (dip.customFlags & ViewCustomFlags::EXPANDED) || !dip.HasChildren())
		return;

	m_pList->SetRedraw(FALSE);	// Turn off updating (better performance)
	m_pList->SetItemState(sel, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);

	CDiffContext &ctxt = GetDiffContext();
	dip.customFlags |= ViewCustomFlags::EXPANDED;
	if (bRecursive)
		ExpandSubdirs(ctxt, dip);

	DIFFITEM *diffpos = ctxt.GetFirstChildDiffPosition(GetItemKey(sel));
	UINT indext = sel + 1;
	int alldiffs;
	RedisplayChildren(diffpos, dip.GetDepth() + 1, indext, alldiffs);

	SortColumnsAppropriately();

	m_pList->SetRedraw(TRUE);	// Turn updating back on
}

/**
 * @brief Open parent folder if possible.
 */
void CDirView::OpenParentDirectory()
{
	CDirDoc *pDoc = GetDocument();
	PathContext pathsParent;
	switch (CheckAllowUpwardDirectory(GetDiffContext(), pDoc->m_pTempPathContext, pathsParent))
	{
	case AllowUpwardDirectory::ParentIsTempPath:
		pDoc->m_pTempPathContext = pDoc->m_pTempPathContext->DeleteHead();
		// fall through (no break!)
	case AllowUpwardDirectory::ParentIsRegularPath: 
	{
		DWORD dwFlags[3];
		for (int nIndex = 0; nIndex < pathsParent.GetSize(); ++nIndex)
			dwFlags[nIndex] = FFILEOPEN_NOMRU | (pDoc->GetReadOnly(nIndex) ? FFILEOPEN_READONLY : 0);
		GetMainFrame()->DoFileOpen(&pathsParent, dwFlags, nullptr, _T(""), GetDiffContext().m_bRecursive, (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? nullptr : pDoc);
	}
		// fall through (no break!)
	case AllowUpwardDirectory::No:
		break;
	default:
		LangMessageBox(IDS_INVALID_DIRECTORY, MB_ICONSTOP);
		break;
	}
}

/**
 * @brief Get one or two selected items
 *
 * Returns false if 0 or more than 3 items selecte
 */
bool CDirView::GetSelectedItems(int * sel1, int * sel2, int * sel3)
{
	*sel2 = -1;
	*sel3 = -1;
	*sel1 = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (*sel1 == -1)
		return false;
	*sel2 = m_pList->GetNextItem(*sel1, LVNI_SELECTED);
	if (*sel2 == -1)
		return true;
	*sel3 = m_pList->GetNextItem(*sel2, LVNI_SELECTED);
	if (*sel3 == -1)
		return true;
	int extra = m_pList->GetNextItem(*sel3, LVNI_SELECTED);
	return (extra == -1);
}

/**
 * @brief Open special items (parent folders etc).
 * @param [in] pos1 First item position.
 * @param [in] pos2 Second item position.
 */
void CDirView::OpenSpecialItems(DIFFITEM *pos1, DIFFITEM *pos2, DIFFITEM *pos3)
{
	if (pos2==nullptr && pos3==nullptr)
	{
		// Browse to parent folder(s) selected
		// SPECIAL_ITEM_POS is position for
		// special items, but there is currenly
		// only one (parent folder)
		OpenParentDirectory();
	}
	else
	{
		// Parent directory & something else selected
		// Not valid action
	}
}

/**
 * @brief Creates a pairing folder for unique folder item.
 * This function creates a pairing folder for unique folder item in
 * folder compare. This way user can browse into unique folder's
 * contents and don't necessarily need to copy whole folder structure.
 * @return true if user agreed and folder was created.
 */
static bool CreateFoldersPair(const PathContext& paths)
{
	bool created = false;
	for (const auto& path : paths)
	{
		if (!paths::DoesPathExist(path))
		{
			String message =
				strutils::format_string1( 
					_("The folder exists only in other side and cannot be opened.\n\nDo you want to create a matching folder:\n%1\nto the other side and open these folders?"),
					path);
			int res = AfxMessageBox(message.c_str(), MB_YESNO | MB_ICONWARNING | MB_DONT_ASK_AGAIN);
			if (res == IDYES)
				created = paths::CreateIfNeeded(path);
		}
	}
	return created;
}

void CDirView::Open(const PathContext& paths, DWORD dwFlags[3], PackingInfo * infoUnpacker)
{
	bool isdir = false;
	for (auto path : paths)
	{
		if (paths::DoesPathExist(path) == paths::IS_EXISTING_DIR)
			isdir = true;
	}
	CDirDoc * pDoc = GetDocument();
	if (isdir)
	{
		// Open subfolders
		// Don't add folders to MRU
		GetMainFrame()->DoFileOpen(&paths, dwFlags, nullptr, _T(""), GetDiffContext().m_bRecursive, (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? nullptr : pDoc);
	}
	else if (HasZipSupport() && std::count_if(paths.begin(), paths.end(), ArchiveGuessFormat) == paths.GetSize())
	{
		// Open archives, not adding paths to MRU
		GetMainFrame()->DoFileOpen(&paths, dwFlags, nullptr, _T(""), GetDiffContext().m_bRecursive, nullptr, _T(""), infoUnpacker);
	}
	else
	{
		// Regular file case

		// Binary attributes are set after files are unpacked
		// so after plugins such as the MS-Office plugins have had a chance to make them textual
		// We haven't done unpacking yet in this diff, but if a binary flag is already set,
		// then it was set in a previous diff after unpacking, so we trust it

		// Open identical and different files
		FileLocation fileloc[3];
		String strDesc[3];
		const String sUntitled[] = { _("Untitled left"), paths.GetSize() < 3 ? _("Untitled right") : _("Untitled middle"), _("Untitled right") };
		for (int i = 0; i < paths.GetSize(); ++i)
		{
			if (paths::DoesPathExist(paths[i]) == paths::DOES_NOT_EXIST)
				strDesc[i] = sUntitled[i];
			else
				fileloc[i].setPath(paths[i]);
		}

		GetMainFrame()->ShowAutoMergeDoc(pDoc, paths.GetSize(), fileloc,
			dwFlags, strDesc, _T(""), infoUnpacker);
	}
}

/**
 * @brief Open selected files or directories.
 *
 * Opens selected files to file compare. If comparing
 * directories non-recursively, then subfolders and parent
 * folder are opened too.
 *
 * This handles the case that one item is selected
 * and the case that two items are selected (one on each side)
 */
void CDirView::OpenSelection(SELECTIONTYPE selectionType /*= SELECTIONTYPE_NORMAL*/, PackingInfo * infoUnpacker /*= nullptr*/, bool openableForDir /*= true*/)
{
	Merge7zFormatMergePluginScope scope(infoUnpacker);
	CDirDoc * pDoc = GetDocument();
	const CDiffContext& ctxt = GetDiffContext();

	// First, figure out what was selected (store into pos1 & pos2)
	DIFFITEM *pos1 = nullptr, *pos2 = nullptr, *pos3 = nullptr;
	int sel1 = -1, sel2 = -1, sel3 = -1;
	if (!GetSelectedItems(&sel1, &sel2, &sel3))
	{
		// Must have 1 or 2 or 3 items selected
		// Not valid action
		return;
	}

	pos1 = GetItemKey(sel1);
	ASSERT(pos1 != nullptr);
	if (sel2 != -1)
	{
		pos2 = GetItemKey(sel2);
		ASSERT(pos2 != nullptr);
		if (sel3 != -1)
			pos3 = GetItemKey(sel3);
	}

	// Now handle the various cases of what was selected

	if (pos1 == (DIFFITEM *)SPECIAL_ITEM_POS)
	{
		OpenSpecialItems(pos1, pos2, pos3);
		return;
	}

	// Common variables which both code paths below are responsible for setting
	PathContext paths;
	const DIFFITEM *pdi[3] = {0}; // left & right items (di1==di2 if single selection)
	bool isdir = false; // set if we're comparing directories
	int nPane[3];
	String errmsg;
	bool success;
	if (pos2 && !pos3)
		success = GetOpenTwoItems(ctxt, selectionType, pos1, pos2, pdi,
				paths, sel1, sel2, isdir, nPane, errmsg, openableForDir);
	else if (pos2 && pos3)
		success = GetOpenThreeItems(ctxt, pos1, pos2, pos3, pdi,
				paths, sel1, sel2, sel3, isdir, nPane, errmsg, openableForDir);
	else
	{
		// Only one item selected, so perform diff on its sides
		success = GetOpenOneItem(ctxt, pos1, pdi, 
				paths, sel1, isdir, nPane, errmsg, openableForDir);
		if (isdir)
			CreateFoldersPair(paths);
	}
	if (!success)
	{
		if (!errmsg.empty())
			AfxMessageBox(errmsg.c_str(), MB_ICONSTOP);
		return;
	}

	// Now pathLeft, pathRight, di1, di2, and isdir are all set
	// We have two items to compare, no matter whether same or different underlying DirView item

	DWORD dwFlags[3];
	for (int nIndex = 0; nIndex < paths.GetSize(); nIndex++)
		dwFlags[nIndex] = FFILEOPEN_NOMRU | (pDoc->GetReadOnly(nPane[nIndex]) ? FFILEOPEN_READONLY : 0);

	Open(paths, dwFlags, infoUnpacker);
}

void CDirView::OpenSelectionAs(UINT id)
{
	CDirDoc * pDoc = GetDocument();
	const CDiffContext& ctxt = GetDiffContext();

	// First, figure out what was selected (store into pos1 & pos2 & pos3)
	DIFFITEM *pos1 = nullptr, *pos2 = nullptr, *pos3 = nullptr;
	int sel1 = -1, sel2 = -1, sel3 = -1;
	if (!GetSelectedItems(&sel1, &sel2, &sel3))
	{
		// Must have 1 or 2 or 3 items selected
		// Not valid action
		return;
	}

	pos1 = GetItemKey(sel1);
	ASSERT(pos1);
	if (sel2 != -1)
	{
		pos2 = GetItemKey(sel2);
		ASSERT(pos2 != nullptr);
		if (sel3 != -1)
			pos3 = GetItemKey(sel3);
	}

	// Now handle the various cases of what was selected

	if (pos1 == (DIFFITEM *)SPECIAL_ITEM_POS)
	{
		ASSERT(false);
		return;
	}

	// Common variables which both code paths below are responsible for setting
	PathContext paths;
	const DIFFITEM *pdi[3]; // left & right items (di1==di2 if single selection)
	bool isdir = false; // set if we're comparing directories
	int nPane[3];
	String errmsg;
	bool success;
	if (pos2 && !pos3)
		success = GetOpenTwoItems(ctxt, SELECTIONTYPE_NORMAL, pos1, pos2, pdi,
				paths, sel1, sel2, isdir, nPane, errmsg, false);
	else if (pos2 && pos3)
		success = GetOpenThreeItems(ctxt, pos1, pos2, pos3, pdi,
				paths, sel1, sel2, sel3, isdir, nPane, errmsg, false);
	else
	{
		// Only one item selected, so perform diff on its sides
		success = GetOpenOneItem(ctxt, pos1, pdi,
				paths, sel1, isdir, nPane, errmsg, false);
	}
	if (!success)
	{
		if (!errmsg.empty())
			AfxMessageBox(errmsg.c_str(), MB_ICONSTOP);
		return;
	}

	// Open identical and different files
	DWORD dwFlags[3] = { 0 };
	FileLocation fileloc[3];
	for (int pane = 0; pane < paths.GetSize(); pane++)
	{
		fileloc[pane].setPath(paths[pane]);
		dwFlags[pane] |= FFILEOPEN_NOMRU | (pDoc->GetReadOnly(nPane[pane]) ? FFILEOPEN_READONLY : 0);
	}
	if (id == ID_MERGE_COMPARE_HEX)
		GetMainFrame()->ShowHexMergeDoc(pDoc, paths.GetSize(), fileloc, dwFlags, nullptr);
	else
		GetMainFrame()->ShowImgMergeDoc(pDoc, paths.GetSize(), fileloc, dwFlags, nullptr);
}

/// User chose (context menu) delete left
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirDel()
{
	DoDirAction(&DirActions::DeleteOn<stype>, _("Deleting files..."));
}

/// User chose (context menu) delete both
void CDirView::OnCtxtDirDelBoth()
{
	DoDirAction(&DirActions::DeleteOnBoth, _("Deleting files..."));
}

/// Enable/disable Delete Left menu choice on context menu
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirDel(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemDeletableOn<stype>);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemString(stype, counts.count, counts.total).c_str());
}

/// Enable/disable Delete Both menu choice on context menu
void CDirView::OnUpdateCtxtDirDelBoth(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemDeletableOnBoth);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringAll(GetDocument()->m_nDirs, counts.count, counts.total).c_str());
}

/**
 * @brief Update "Copy | Right to..." item
 */
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirCopyTo(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemCopyableToOn<stype>);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringTo(stype, counts.count, counts.total).c_str());
}

void CDirView::OnUpdateCtxtDirCopyBothTo(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemCopyableBothToOn);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringAllTo(GetDocument()->m_nDirs, counts.count, counts.total).c_str());
}

void CDirView::OnUpdateCtxtDirCopyBothDiffsOnlyTo(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemNavigableDiff);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringDifferencesTo(counts.count, counts.total).c_str());
}
	
/**
 * @brief Update "Copy | Left/Right/Both " item
 */
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirCopy2(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemCopyableToOn<stype>);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemString(stype, counts.count, counts.total).c_str());
}

void CDirView::OnUpdateCtxtDirCopyBoth2(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemCopyableBothToOn);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringAll(GetDocument()->m_nDirs, counts.count, counts.total).c_str());
}

/**
 * @brief Get keydata associated with item in given index.
 * @param [in] idx Item's index to list in UI.
 * @return Key for item in given index.
 */
DIFFITEM *CDirView::GetItemKey(int idx) const
{
	return (DIFFITEM *) m_pList->GetItemData(idx);
}

// SetItemKey & GetItemKey encapsulate how the display list items
// are mapped to DiffItems, which in turn are DiffContext keys to the actual DIFFITEM data

/**
 * @brief Get DIFFITEM data for item.
 * This function returns DIFFITEM data for item in given index in GUI.
 * @param [in] sel Item's index in folder compare GUI list.
 * @return DIFFITEM for item.
 */
const DIFFITEM &CDirView::GetDiffItem(int sel) const
{
	CDirView * pDirView = const_cast<CDirView *>(this);
	return pDirView->GetDiffItem(sel);
}

/**
 * Given index in list control, get modifiable reference to its DIFFITEM data
 */
DIFFITEM &CDirView::GetDiffItem(int sel)
{
	DIFFITEM *diffpos = GetItemKey(sel);

	// If it is special item, return empty DIFFITEM
	if (diffpos == (DIFFITEM *)SPECIAL_ITEM_POS)
	{
		return *DIFFITEM::GetEmptyItem();
	}
	return GetDiffContext().GetDiffRefAt(diffpos);
}

void CDirView::DeleteItem(int sel, bool removeDIFFITEM)
{
	if (m_bTreeMode)
		CollapseSubdir(sel);
	if (removeDIFFITEM)
	{
		DIFFITEM *diffpos = GetItemKey(sel);
		m_pList->DeleteItem(sel);
		if (diffpos != (DIFFITEM *)SPECIAL_ITEM_POS)
		{
			if (diffpos->HasChildren())
				diffpos->RemoveChildren();
			diffpos->DelinkFromSiblings();
			delete diffpos;
		}
	}
	else
	{
		m_pList->DeleteItem(sel);
	}

	m_firstDiffItem.reset();
	m_lastDiffItem.reset();
}

void CDirView::DeleteAllDisplayItems()
{
	// item data are just positions (diffposes)
	// that is, they contain no memory needing to be freed
	m_pList->DeleteAllItems();

	m_firstDiffItem.reset();
	m_lastDiffItem.reset();
}

/**
 * @brief Given key, get index of item which has it stored.
 * This function searches from list in UI.
 */
int CDirView::GetItemIndex(DIFFITEM *key)
{
	LVFINDINFO findInfo;

	findInfo.flags = LVFI_PARAM;  // Search for itemdata
	findInfo.lParam = (LPARAM)key;
	return m_pList->FindItem(&findInfo);
}

/**
 * @brief Get the file names on both sides for specified item.
 * @note Return empty strings if item is special item.
 */
void CDirView::GetItemFileNames(int sel, String& strLeft, String& strRight) const
{
	DIFFITEM *diffpos = GetItemKey(sel);
	if (diffpos == (DIFFITEM *)SPECIAL_ITEM_POS)
	{
		strLeft.erase();
		strRight.erase();
	}
	else
	{
		const CDiffContext& ctxt = GetDiffContext();
		::GetItemFileNames(ctxt, ctxt.GetDiffAt(diffpos), strLeft, strRight);
	}
}

/**
 * @brief Get the file names on both sides for specified item.
 * @note Return empty strings if item is special item.
 */
void CDirView::GetItemFileNames(int sel, PathContext * paths) const
{
	DIFFITEM *diffpos = GetItemKey(sel);
	if (diffpos == (DIFFITEM *)SPECIAL_ITEM_POS)
	{
		for (int nIndex = 0; nIndex < GetDocument()->m_nDirs; nIndex++)
			paths->SetPath(nIndex, _T(""));
	}
	else
	{
		const CDiffContext& ctxt = GetDiffContext();
		*paths = ::GetItemFileNames(ctxt, ctxt.GetDiffAt(diffpos));
	}
}

/**
 * @brief Open selected file with registered application.
 * Uses shell file associations to open file with registered
 * application. We first try to use "Edit" action which should
 * open file to editor, since we are more interested editing
 * files than running them (scripts).
 * @param [in] stype Side of file to open.
 */
void CDirView::DoOpen(SIDE_TYPE stype)
{
	int sel = GetSingleSelectedItem();
	if (sel == -1) return;
	DirItemIterator dirBegin = SelBegin();
	String file = GetSelectedFileName(dirBegin, stype, GetDiffContext());
	if (file.empty()) return;
	HINSTANCE rtn = ShellExecute(::GetDesktopWindow(), _T("edit"), file.c_str(), 0, 0, SW_SHOWNORMAL);
	if (reinterpret_cast<uintptr_t>(rtn) == SE_ERR_NOASSOC)
		rtn = ShellExecute(::GetDesktopWindow(), _T("open"), file.c_str(), 0, 0, SW_SHOWNORMAL);
	if (reinterpret_cast<uintptr_t>(rtn) == SE_ERR_NOASSOC)
		DoOpenWith(stype);
}

/// Open with dialog for file on selected side
void CDirView::DoOpenWith(SIDE_TYPE stype)
{
	int sel = GetSingleSelectedItem();
	if (sel == -1) return;
	DirItemIterator dirBegin = SelBegin();
	String file = GetSelectedFileName(dirBegin, stype, GetDiffContext());
	if (file.empty()) return;
	CString sysdir;
	if (!GetSystemDirectory(sysdir.GetBuffer(MAX_PATH), MAX_PATH)) return;
	sysdir.ReleaseBuffer();
	CString arg = (CString)_T("shell32.dll,OpenAs_RunDLL ") + file.c_str();
	ShellExecute(::GetDesktopWindow(), 0, _T("RUNDLL32.EXE"), arg, sysdir, SW_SHOWNORMAL);
}

/// Open selected file  on specified side to external editor
void CDirView::DoOpenWithEditor(SIDE_TYPE stype)
{
	int sel = GetSingleSelectedItem();
	if (sel == -1) return;
	DirItemIterator dirBegin = SelBegin();
	String file = GetSelectedFileName(dirBegin, stype, GetDiffContext());
	if (file.empty()) return;

	theApp.OpenFileToExternalEditor(file);
}

void CDirView::DoOpenParentFolder(SIDE_TYPE stype)
{
	int sel = GetSingleSelectedItem();
	if (sel == -1) return;
	DirItemIterator dirBegin = SelBegin();
	String file = GetSelectedFileName(dirBegin, stype, GetDiffContext());
	if (file.empty()) return;
	String parentFolder = paths::GetParentPath(file);
	ShellExecute(::GetDesktopWindow(), _T("open"), parentFolder.c_str(), 0, 0, SW_SHOWNORMAL);
}

/// User chose (context menu) open left
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirOpen()
{
	DoOpen(stype);
}

/// User chose (context menu) open left with
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirOpenWith()
{
	DoOpenWith(stype);
}

/// User chose (context menu) open left with editor
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirOpenWithEditor()
{
	DoOpenWithEditor(stype);
}

/// User chose (context menu) open left parent folder
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirOpenParentFolder()
{
	DoOpenParentFolder(stype);
}

/// Update context menuitem "Open left | with editor"
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirOpenWithEditor(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemOpenableOnWith<stype>);
	pCmdUI->Enable(counts.count > 0 && counts.total == 1);
}

// return selected item index, or -1 if none or multiple
int CDirView::GetSingleSelectedItem() const
{
	int sel = -1, sel2 = -1;
	sel = m_pList->GetNextItem(sel, LVNI_SELECTED);
	if (sel == -1) return -1;
	sel2 = m_pList->GetNextItem(sel, LVNI_SELECTED);
	if (sel2 != -1) return -1;
	return sel;
}
// Enable/disable Open Left menu choice on context menu
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirOpen(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemOpenableOn<stype>);
	pCmdUI->Enable(counts.count > 0 && counts.total == 1);
}

// Enable/disable Open Left With menu choice on context menu
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirOpenWith(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemOpenableOnWith<stype>);
	pCmdUI->Enable(counts.count > 0 && counts.total == 1);
}

// Enable/disable Open Parent Folder menu choice on context menu
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirOpenParentFolder(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsParentFolderOpenable<stype>);
	pCmdUI->Enable(counts.count > 0 && counts.total == 1);
}

// Used for Open
void CDirView::DoUpdateOpen(SELECTIONTYPE selectionType, CCmdUI* pCmdUI, bool openableForDir /*= true*/)
{
	int sel1 = -1, sel2 = -1, sel3 = -1;
	if (!GetSelectedItems(&sel1, &sel2, &sel3))
	{
		// 0 items or more than 2 items seleted
		pCmdUI->Enable(FALSE);
		return;
	}
	if (sel2 == -1)
	{
		// One item selected
		if (selectionType != SELECTIONTYPE_NORMAL)
		{
			pCmdUI->Enable(FALSE);
			return;
		}
		if (!openableForDir)
		{
			const DIFFITEM& di1 = GetDiffItem(sel1);
			if (di1.diffcode.isDirectory())
			{
				pCmdUI->Enable(FALSE);
				return;
			}
		}
	}
	else if (sel3 == -1)
	{
		// Two items selected
		const DIFFITEM& di1 = GetDiffItem(sel1);
		const DIFFITEM& di2 = GetDiffItem(sel2);
		if (!AreItemsOpenable(GetDiffContext(), selectionType, di1, di2, openableForDir))
		{
			pCmdUI->Enable(FALSE);
			return;
		}
	}
	else
	{
		// Three items selected
		const DIFFITEM& di1 = GetDiffItem(sel1);
		const DIFFITEM& di2 = GetDiffItem(sel2);
		const DIFFITEM& di3 = GetDiffItem(sel3);
		if (selectionType != SELECTIONTYPE_NORMAL || !::AreItemsOpenable(GetDiffContext(), di1, di2, di3, openableForDir))
		{
			pCmdUI->Enable(FALSE);
			return;
		}
	}
	pCmdUI->Enable(TRUE);
}

/**
 * @brief Return count of selected items in folder compare.
 */
UINT CDirView::GetSelectedCount() const
{
	return m_pList->GetSelectedCount();
}

/**
 * @brief Return index of first selected item in folder compare.
 */
int CDirView::GetFirstSelectedInd()
{
	return m_pList->GetNextItem(-1, LVNI_SELECTED);
}

// Go to first diff
// If none or one item selected select found item
// This is used for scrolling to first diff too
void CDirView::OnFirstdiff()
{
	DirItemIterator it =
		std::find_if(Begin(), End(), MakeDirActions(&DirActions::IsItemNavigableDiff));
	if (it != End())
		MoveFocus(GetFirstSelectedInd(), it.m_sel, GetSelectedCount());
}

void CDirView::OnUpdateFirstdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetFirstDifferentItem() > -1);
}

// Go to last diff
// If none or one item selected select found item
void CDirView::OnLastdiff()
{
	DirItemIterator it =
		std::find_if(RevBegin(), RevEnd(), MakeDirActions(&DirActions::IsItemNavigableDiff));
	if (it != RevEnd())
		MoveFocus(GetFirstSelectedInd(), it.m_sel, GetSelectedCount());
}

void CDirView::OnUpdateLastdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetFirstDifferentItem() > -1);
}

bool CDirView::HasNextDiff()
{
	int lastDiff = GetLastDifferentItem();

	// Check if different files were found and
	// there is different item after focused item
	return (lastDiff > -1) && (GetFocusedItem() < lastDiff);
}

bool CDirView::HasPrevDiff()
{
	int firstDiff = GetFirstDifferentItem();

	// Check if different files were found and
	// there is different item before focused item
	return (firstDiff > -1) && (firstDiff < GetFocusedItem());
}

void CDirView::MoveToNextDiff()
{
	int currentInd = GetFocusedItem();
	DirItemIterator begin(m_pIList.get(), currentInd + 1);
	DirItemIterator it =
		std::find_if(begin, End(), MakeDirActions(&DirActions::IsItemNavigableDiff));
	if (it != End())
		MoveFocus(currentInd, it.m_sel, GetSelectedCount());
}

void CDirView::MoveToPrevDiff()
{
	int currentInd = GetFocusedItem();
	if (currentInd <= 0)
		return;
	DirItemIterator begin(m_pIList.get(), currentInd - 1, false, true);
	DirItemIterator it =
		std::find_if(begin, RevEnd(), MakeDirActions(&DirActions::IsItemNavigableDiff));
	if (it != RevEnd())
		MoveFocus(currentInd, it.m_sel, GetSelectedCount());
}

void CDirView::OpenNextDiff()
{
	MoveToNextDiff();
	int currentInd = GetFocusedItem();
	const DIFFITEM& dip = GetDiffItem(currentInd);
	if (!dip.diffcode.isDirectory())
	{
		OpenSelection();
	}
	else
	{
		GetParentFrame()->ActivateFrame();
	}
}

void CDirView::OpenPrevDiff()
{
	MoveToPrevDiff();
	int currentInd = GetFocusedItem();
	const DIFFITEM& dip = GetDiffItem(currentInd);
	if (!dip.diffcode.isDirectory())
	{
		OpenSelection();
	}
	else
	{
		GetParentFrame()->ActivateFrame();
	}
}

void CDirView::SetActivePane(int pane)
{
	if (m_nActivePane >= 0)
		GetParentFrame()->GetHeaderInterface()->SetActive(m_nActivePane, false);
	GetParentFrame()->GetHeaderInterface()->SetActive(pane, true);
	m_nActivePane = pane;
}

// Go to next diff
// If none or one item selected select found item
void CDirView::OnNextdiff()
{
	MoveToNextDiff();
}


void CDirView::OnUpdateNextdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(HasNextDiff());
}

// Go to prev diff
// If none or one item selected select found item
void CDirView::OnPrevdiff()
{
	MoveToPrevDiff();
}


void CDirView::OnUpdatePrevdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(HasPrevDiff());
}

void CDirView::OnCurdiff()
{
	const int count = m_pList->GetItemCount();
	bool found = false;
	int i = GetFirstSelectedInd();

	// No selection - no diff to go
	if (i == -1)
		i = count;

	while (i < count && !found)
	{
		UINT selected = m_pList->GetItemState(i, LVIS_SELECTED);
		UINT focused = m_pList->GetItemState(i, LVIS_FOCUSED);

		if (selected == LVIS_SELECTED && focused == LVIS_FOCUSED)
		{
			m_pList->EnsureVisible(i, FALSE);
			found = true;
		}
		i++;
	}
}

void CDirView::OnUpdateCurdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetFirstSelectedInd() > -1);
}

int CDirView::GetFocusedItem()
{
	return m_pList->GetNextItem(-1, LVNI_FOCUSED);
}

int CDirView::GetFirstDifferentItem()
{
	if (!m_firstDiffItem.has_value())
	{
		DirItemIterator it =
			std::find_if(Begin(), End(), MakeDirActions(&DirActions::IsItemNavigableDiff));
		m_firstDiffItem = it.m_sel;
	}
	return m_firstDiffItem.value();
}

int CDirView::GetLastDifferentItem()
{
	if (!m_lastDiffItem.has_value())
	{
		DirItemIterator it =
			std::find_if(RevBegin(), RevEnd(), MakeDirActions(&DirActions::IsItemNavigableDiff));
		m_lastDiffItem = it.m_sel;
	}
	return m_lastDiffItem.value();
}

/**
 * @brief Move focus to specified item (and selection if multiple items not selected)
 *
 * Moves the focus from item [currentInd] to item [i]
 * Additionally, if there are not multiple items selected,
 *  deselects item [currentInd] and selects item [i]
 */
void CDirView::MoveFocus(int currentInd, int i, int selCount)
{
	if (selCount <= 1)
	{
		// Not multiple items selected, so bring selection with us
		m_pList->SetItemState(currentInd, 0, LVIS_SELECTED);
		m_pList->SetItemState(currentInd, 0, LVIS_FOCUSED);
		m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}

	// Move focus to specified item
	// (this automatically defocuses old item)
	m_pList->SetItemState(i, LVIS_FOCUSED, LVIS_FOCUSED);
	m_pList->EnsureVisible(i, FALSE);
}

void CDirView::OnUpdateSave(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(FALSE);
}

CDirFrame * CDirView::GetParentFrame()
{
	// can't verify cast without introducing more coupling
	// (CDirView doesn't include DirFrame.h)
	return static_cast<CDirFrame *>(__super::GetParentFrame());
}

void CDirView::OnRefresh()
{
	m_pSavedTreeState.reset(SaveTreeState(GetDiffContext()));
	GetDocument()->Rescan();
}

BOOL CDirView::PreTranslateMessage(MSG* pMsg)
{
	// Handle special shortcuts here
	if (pMsg->message == WM_KEYDOWN)
	{
		if (!IsLabelEdit())
		{
			// Check if we got 'ESC pressed' -message
			if (pMsg->wParam == VK_ESCAPE)
			{
				if (m_pCmpProgressBar != nullptr)
				{
					OnBnClickedComparisonStop();
					return TRUE;
				}

				if (m_nEscCloses != 0)
				{
					AfxGetMainWnd()->PostMessage(WM_COMMAND, ID_FILE_CLOSE);
					return FALSE;
				}
			}
			// Check if we got 'DEL pressed' -message
			if (pMsg->wParam == VK_DELETE)
			{
				AfxGetMainWnd()->PostMessage(WM_COMMAND, ID_MERGE_DELETE);
				return FALSE;
			}
			int sel = GetFocusedItem();
			// Check if we got 'Backspace pressed' -message
			if (pMsg->wParam == VK_BACK)
			{
				if (!GetDiffContext().m_bRecursive)
				{
					OpenParentDirectory();
					return FALSE;
				}
				else if (m_bTreeMode && sel >= 0)
				{
					const DIFFITEM& di = GetDiffItem(sel);
					assert(di.HasParent());
					if (di.HasParent())
					{
						int i = GetItemIndex(di.GetParentLink());
						if (i >= 0)
							MoveFocus(sel, i, GetSelectedCount());
					}
				}
			}
			if (sel >= 0)
			{
				DIFFITEM& dip = this->GetDiffItem(sel);
				if (pMsg->wParam == VK_LEFT)
				{
					if (m_bTreeMode && GetDiffContext().m_bRecursive && (!(dip.customFlags & ViewCustomFlags::EXPANDED) || !dip.HasChildren()))
						PostMessage(WM_KEYDOWN, VK_BACK);
					else
						CollapseSubdir(sel);
					return TRUE;
				}
				if (pMsg->wParam == VK_SUBTRACT)
				{
					CollapseSubdir(sel);
					return TRUE;
				}
				if (pMsg->wParam == VK_RIGHT)
				{
					if (m_bTreeMode && GetDiffContext().m_bRecursive && dip.customFlags & ViewCustomFlags::EXPANDED && dip.HasChildren())
						PostMessage(WM_KEYDOWN, VK_DOWN);
					else
						ExpandSubdir(sel);
					return TRUE;
				}
				if (pMsg->wParam == VK_ADD)
				{
					ExpandSubdir(sel);
					return TRUE;
				}
				if (pMsg->wParam == VK_MULTIPLY)
				{
					ExpandSubdir(sel, true);
					return TRUE;
				}
			}
		}
		else
		{
			// ESC doesn't close window when user is renaming an item.
			if (pMsg->wParam == VK_ESCAPE)
			{
				m_bUserCancelEdit = true;

				// The edit control send LVN_ENDLABELEDIT when it loses focus,
				// so we use it to cancel the rename action.
				m_pList->SetFocus();

				// Stop the ESC before it reach the main frame which might
				// cause a program termination.
				return TRUE;
			}
		}
	}
	return __super::PreTranslateMessage(pMsg);
}

void CDirView::OnUpdateRefresh(CCmdUI* pCmdUI)
{
	UINT threadState = GetDocument()->m_diffThread.GetThreadState();
	pCmdUI->Enable(threadState != CDiffThread::THREAD_COMPARING);
}

/**
 * @brief Called when compare thread asks UI update.
 * @note Currently thread asks update after compare is ready
 * or aborted.
 */
LRESULT CDirView::OnUpdateUIMessage(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	CDirDoc * pDoc = GetDocument();
	ASSERT(pDoc != nullptr);

	if (wParam == CDiffThread::EVENT_COMPARE_COMPLETED)
	{
		// Close and destroy the dialog after compare
		if (m_pCmpProgressBar != nullptr)
			GetParentFrame()->ShowControlBar(m_pCmpProgressBar.get(), FALSE, FALSE);
		m_pCmpProgressBar.reset();

		pDoc->CompareReady();

		if (!pDoc->GetGeneratingReport())
			Redisplay();

		if (!pDoc->GetReportFile().empty())
		{
			OnToolsGenerateReport();
			pDoc->SetReportFile(_T(""));
		}

		if (GetOptionsMgr()->GetBool(OPT_SCROLL_TO_FIRST))
			OnFirstdiff();
		else
			MoveFocus(0, 0, 0);

		// If compare took more than TimeToSignalCompare seconds, notify user
		clock_t elapsed = clock() - m_compareStart;
		GetParentFrame()->SetStatus(
			strutils::format(_("Elapsed time: %ld ms"), elapsed).c_str()
		);
		if (elapsed > TimeToSignalCompare * CLOCKS_PER_SEC)
			MessageBeep(IDOK);
		GetMainFrame()->StartFlashing();
	}
	else if (wParam == CDiffThread::EVENT_COMPARE_PROGRESSED)
	{
		InvalidateRect(nullptr, FALSE);
	}
	else if (wParam == CDiffThread::EVENT_COLLECT_COMPLETED)
	{
		if (m_pSavedTreeState != nullptr)
		{
			RestoreTreeState(GetDiffContext(), m_pSavedTreeState.get());
			m_pSavedTreeState.reset();
			Redisplay();
		}
		else
		{
			if (m_bExpandSubdirs)
				OnViewExpandAllSubdirs();
			else
				Redisplay();
		}
	}

	return 0; // return value unused
}


BOOL CDirView::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	NMHDR * hdr = reinterpret_cast<NMHDR *>(lParam);
	if (hdr->code == HDN_ENDDRAG)
		return OnHeaderEndDrag((LPNMHEADER)hdr, pResult);
	if (hdr->code == HDN_BEGINDRAG)
		return OnHeaderBeginDrag((LPNMHEADER)hdr, pResult);

	return __super::OnNotify(wParam, lParam, pResult);
}

BOOL CDirView::OnChildNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (uMsg == WM_NOTIFY)
	{
		NMHDR *pNMHDR = (NMHDR *)lParam;
		switch (pNMHDR->code)
		{
		case LVN_GETDISPINFO:
			ReflectGetdispinfo((NMLVDISPINFO *)lParam);
			return TRUE;
		case LVN_GETINFOTIPW:
		case LVN_GETINFOTIPA:
			return TRUE;
		}
	}
	return __super::OnChildNotify(uMsg, wParam, lParam, pResult);
}

/**
 * @brief User is starting to drag a column header
 */
bool CDirView::OnHeaderBeginDrag(LPNMHEADER hdr, LRESULT* pResult)
{
	// save column widths before user reorders them
	// so we can reload them on the end drag
	const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS;
	GetOptionsMgr()->SaveOption(keyname,
		m_pColItems->SaveColumnWidths(std::bind(&CListCtrl::GetColumnWidth, m_pList, _1)).c_str());
	return true;
}

/**
 * @brief User just finished dragging a column header
 */
bool CDirView::OnHeaderEndDrag(LPNMHEADER hdr, LRESULT* pResult)
{
	int src = hdr->iItem;
	int dest = hdr->pitem->iOrder;
	bool allowDrop = true;
	*pResult = allowDrop ? FALSE : TRUE;
	if (allowDrop && src != dest && dest != -1)
	{
		m_pColItems->MoveColumn(src, dest);
		InitiateSort();
	}
	return true;
}

LRESULT CDirView::OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam)
{
	int olddpi = m_dpi;
	__super::OnDpiChangedBeforeParent(wParam, lParam);
	DpiAware::ListView_UpdateColumnWidths(m_hWnd, olddpi, m_dpi);
	return 0;
}

/**
 * @brief Remove any windows reordering of columns
 */
void CDirView::FixReordering()
{
	LVCOLUMN lvcol;
	lvcol.mask = LVCF_ORDER;
	lvcol.fmt = 0;
	lvcol.cx = 0;
	lvcol.pszText = nullptr;
	lvcol.iSubItem = 0;
	for (int i = 0; i < m_pColItems->GetColCount(); ++i)
	{
		lvcol.iOrder = i;
		GetListCtrl().SetColumn(i, &lvcol);
	}
}

/** @brief Add columns to display, loading width & order from registry. */
void CDirView::LoadColumnHeaderItems()
{
	bool dummyflag = false;

	CHeaderCtrl * h = m_pList->GetHeaderCtrl();
	if (h->GetItemCount())
	{
		dummyflag = true;
		while (m_pList->GetHeaderCtrl()->GetItemCount() > 1)
			m_pList->DeleteColumn(1);
	}

	for (int i = 0; i < m_pColItems->GetDispColCount(); ++i)
	{
		LVCOLUMN lvc;
		lvc.mask = LVCF_FMT + LVCF_SUBITEM + LVCF_TEXT;
		lvc.fmt = LVCFMT_LEFT;
		lvc.cx = 0;
		lvc.pszText = _T("text");
		lvc.iSubItem = i;
		m_pList->InsertColumn(i, &lvc);
	}
	if (dummyflag)
		m_pList->DeleteColumn(1);

}

void CDirView::SetFont(const LOGFONT & lf)
{
	m_font.DeleteObject();
	m_font.CreateFontIndirect(&lf);
	CWnd::SetFont(&m_font);
}

/** @brief Fire off a resort of the data, to take place when things stabilize. */
void CDirView::InitiateSort()
{
	PostMessage(WM_TIMER, COLUMN_REORDER);
}

void CDirView::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == COLUMN_REORDER)
	{
		// Remove the windows reordering, as we're doing it ourselves
		FixReordering();
		// Now redraw screen
		UpdateColumnNames();
		m_pColItems->LoadColumnWidths(
			GetOptionsMgr()->GetString(GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS),
			std::bind(&CListCtrl::SetColumnWidth, m_pList, _1, _2), GetDefColumnWidth());
		Redisplay();
	}
	else if (nIDEvent == STATUSBAR_UPDATE)
	{
		KillTimer(STATUSBAR_UPDATE);
		int items = GetSelectedCount();
		String msg = (items == 1) ? _("1 item selected") : strutils::format_string1(_("%1 items selected"), strutils::to_str(items));
		GetParentFrame()->SetStatus(msg.c_str());
	}
	
	__super::OnTimer(nIDEvent);
}

/**
 * @brief Change left-side readonly-status
 */
template<SIDE_TYPE stype>
void CDirView::OnReadOnly()
{
	const int index = SideToIndex(GetDiffContext(), stype);
	bool bReadOnly = GetDocument()->GetReadOnly(index);
	GetDocument()->SetReadOnly(index, !bReadOnly);
}

/**
 * @brief Update left-readonly menu item
 */
template<SIDE_TYPE stype>
void CDirView::OnUpdateReadOnly(CCmdUI* pCmdUI)
{
	const int index = SideToIndex(GetDiffContext(), stype);
	bool bReadOnly = GetDocument()->GetReadOnly(index);
	if (stype != SIDE_MIDDLE)
	{
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(bReadOnly);
	}
	else
	{
		pCmdUI->Enable(GetDocument()->m_nDirs > 2);
		pCmdUI->SetCheck(bReadOnly && GetDocument()->m_nDirs > 2);
	}
}

/**
 * @brief Update left-side readonly statusbar item
 */
void CDirView::OnUpdateStatusLeftRO(CCmdUI* pCmdUI)
{
	bool bROLeft = GetDocument()->GetReadOnly(0);
	pCmdUI->Enable(bROLeft);
}

/**
 * @brief Update middle readonly statusbar item
 */
void CDirView::OnUpdateStatusMiddleRO(CCmdUI* pCmdUI)
{
	bool bROMiddle = GetDocument()->GetReadOnly(1);
	pCmdUI->Enable(bROMiddle && GetDocument()->m_nDirs > 2);
}

/**
 * @brief Update right-side readonly statusbar item
 */
void CDirView::OnUpdateStatusRightRO(CCmdUI* pCmdUI)
{
	bool bRORight = GetDocument()->GetReadOnly(GetDocument()->m_nDirs - 1);
	pCmdUI->Enable(bRORight);
}

/**
 * @brief Open dialog to customize dirview columns
 */
void CDirView::OnCustomizeColumns()
{
	// Located in DirViewColHandler.cpp
	OnEditColumns();
	const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_ORDERS : OPT_DIRVIEW3_COLUMN_ORDERS;
	GetOptionsMgr()->SaveOption(keyname, m_pColItems->SaveColumnOrders());
}

void CDirView::OnCtxtOpenWithUnpacker()
{
	int sel = -1;
	sel = m_pList->GetNextItem(sel, LVNI_SELECTED);
	if (sel != -1)
	{
		// let the user choose a handler
		CSelectUnpackerDlg dlg(GetDiffItem(sel).diffFileInfo[0].filename, this);
		// create now a new infoUnpacker to initialize the manual/automatic flag
		PackingInfo infoUnpacker(PLUGIN_AUTO);
		dlg.SetInitialInfoHandler(&infoUnpacker);

		if (dlg.DoModal() == IDOK)
		{
			infoUnpacker = dlg.GetInfoHandler();
			OpenSelection(SELECTIONTYPE_NORMAL, &infoUnpacker, false);
		}
	}

}

void CDirView::OnUpdateCtxtOpenWithUnpacker(CCmdUI* pCmdUI)
{
	if (!GetOptionsMgr()->GetBool(OPT_PLUGINS_ENABLED))
	{
		pCmdUI->Enable(FALSE);
		return;
	}

	// we need one selected file, existing on both side
	if (m_pList->GetSelectedCount() != 1)
		pCmdUI->Enable(FALSE);
	else
	{
		int sel = -1;
		sel = m_pList->GetNextItem(sel, LVNI_SELECTED);
		const DIFFITEM& di = GetDiffItem(sel);
		if (di.diffcode.isDirectory())
		{
			pCmdUI->Enable(FALSE);
			return;
		}
		pCmdUI->Enable(IsItemDeletableOnBoth(GetDiffContext(), di));
	}
}

/**
 * @brief Fill string list with current dirview column registry key names
 */
std::vector<String> CDirView::GetCurrentColRegKeys()
{
	std::vector<String> colKeys;
	int nphyscols = GetListCtrl().GetHeaderCtrl()->GetItemCount();
	for (int col = 0; col < nphyscols; ++col)
	{
		int logcol = m_pColItems->ColPhysToLog(col);
		colKeys.push_back(m_pColItems->GetColRegValueNameBase(logcol));
	}
	return colKeys;
}

struct FileCmpReport: public IFileCmpReport
{
	explicit FileCmpReport(CDirView *pDirView) : m_pDirView(pDirView) {}
	~FileCmpReport() override {}
	bool operator()(REPORT_TYPE nReportType, IListCtrl *pList, int nIndex, const String &sDestDir, String &sLinkPath) override
	{
		const CDiffContext& ctxt = m_pDirView->GetDiffContext();
		const DIFFITEM &di = m_pDirView->GetDiffItem(nIndex);
		
		String sLinkFullPath = paths::ConcatPath(ctxt.GetLeftPath(), di.diffFileInfo[0].GetFile());

		if (di.diffcode.isDirectory() || !IsItemNavigableDiff(ctxt, di) || IsArchiveFile(sLinkFullPath))
		{
			sLinkPath.clear();
			return false;
		}

		sLinkPath = di.diffFileInfo[0].GetFile();

		strutils::replace(sLinkPath, _T("\\"), _T("_"));
		sLinkPath += _T(".html");
		String sReportPath = paths::ConcatPath(sDestDir, sLinkPath);
		bool completed = false;

		m_pDirView->MoveFocus(m_pDirView->GetFirstSelectedInd(), nIndex, m_pDirView->GetSelectedCount());
		m_pDirView->PostMessage(MSG_GENERATE_FLIE_COMPARE_REPORT,
			reinterpret_cast<WPARAM>(sReportPath.c_str()), 
			reinterpret_cast<LPARAM>(&completed));

		while (!completed)
		{
			MSG msg;
			while (::PeekMessage(&msg, nullptr, NULL, NULL, PM_NOREMOVE))
			{
				if (!AfxGetApp()->PumpMessage())
					break;
			}
			Sleep(5);
		}

		return true;
	}
private:
	FileCmpReport();
	CDirView *m_pDirView;
};

LRESULT CDirView::OnGenerateFileCmpReport(WPARAM wParam, LPARAM lParam)
{
	OpenSelection();
	CFrameWnd * pFrame = GetMainFrame()->GetActiveFrame();
	IMergeDoc * pMergeDoc = dynamic_cast<IMergeDoc *>(pFrame->GetActiveDocument());
	if (pMergeDoc == nullptr)
		pMergeDoc = dynamic_cast<IMergeDoc *>(pFrame);

	auto *pReportFileName = reinterpret_cast<const TCHAR *>(wParam);
	bool *pCompleted = reinterpret_cast<bool *>(lParam);
	if (pMergeDoc != nullptr)
	{
		pMergeDoc->GenerateReport(pReportFileName);
		pMergeDoc->CloseNow();
	}
	MSG msg;
	while (::PeekMessage(&msg, nullptr, NULL, NULL, PM_NOREMOVE))
		if (!AfxGetApp()->PumpMessage())
			break;
	GetMainFrame()->OnUpdateFrameTitle(FALSE);
	*pCompleted = true;
	return 0;
}

/**
 * @brief Generate report from dir compare results.
 */
void CDirView::OnToolsGenerateReport()
{
	CDirDoc *pDoc = GetDocument();
	DirCmpReportDlg dlg;
	dlg.LoadSettings();
	dlg.m_sReportFile = pDoc->GetReportFile();
	if (dlg.m_sReportFile.empty() && dlg.DoModal() != IDOK)
		return;

	pDoc->SetGeneratingReport(true);
	const CDiffContext& ctxt = GetDiffContext();

	PathContext paths = ctxt.GetNormalizedPaths();

	// If inside archive, convert paths
	if (pDoc->IsArchiveFolders())
	{
		for (int i = 0; i < paths.GetSize(); i++)
			pDoc->ApplyDisplayRoot(i, paths[i]);
	}

	DirCmpReport *pReport = new DirCmpReport(GetCurrentColRegKeys());
	pReport->SetRootPaths(paths);
	pReport->SetColumns(m_pColItems->GetDispColCount());
	pReport->SetFileCmpReport(new FileCmpReport(this));
	pReport->SetList(new IListCtrlImpl(m_pList->m_hWnd));
	pReport->SetReportType(dlg.m_nReportType);
	pReport->SetReportFile(dlg.m_sReportFile);
	pReport->SetCopyToClipboard(dlg.m_bCopyToClipboard);
	pReport->SetIncludeFileCmpReport(dlg.m_bIncludeFileCmpReport);
	pDoc->SetReport(pReport);
	pDoc->Rescan();
}

/**
 * @brief Generate patch from files selected.
 *
 * Creates a patch from selected files in active directory compare, or
 * active file compare. Files in file compare must be saved before
 * creating a patch.
 */
void CDirView::OnToolsGeneratePatch()
{
	CPatchTool patcher;
	const CDiffContext& ctxt = GetDiffContext();

	// Get selected items from folder compare
	bool bValidFiles = true;
	for (DirItemIterator it = SelBegin(); bValidFiles && it != SelEnd(); ++it)
	{
		const DIFFITEM &item = *it;
		if (item.diffcode.isBin())
		{
			LangMessageBox(IDS_CANNOT_CREATE_BINARYPATCH, MB_ICONWARNING |
				MB_DONT_DISPLAY_AGAIN, IDS_CANNOT_CREATE_BINARYPATCH);
			bValidFiles = false;
		}

		if (bValidFiles)
		{
			// Format full paths to files (leftFile/rightFile)
			String leftFile = item.getFilepath(0, ctxt.GetNormalizedPath(0));
			if (!leftFile.empty())
				leftFile = paths::ConcatPath(leftFile, item.diffFileInfo[0].filename);
			String rightFile = item.getFilepath(1, ctxt.GetNormalizedPath(1));
			if (!rightFile.empty())
				rightFile = paths::ConcatPath(rightFile, item.diffFileInfo[1].filename);

			// Format relative paths to files in folder compare
			String leftpatch = item.diffFileInfo[0].path;
			if (!leftpatch.empty())
				leftpatch += _T("/");
			leftpatch += item.diffFileInfo[0].filename;
			String rightpatch = item.diffFileInfo[1].path;
			if (!rightpatch.empty())
				rightpatch += _T("/");
			rightpatch += item.diffFileInfo[1].filename;
			patcher.AddFiles(leftFile, leftpatch, rightFile, rightpatch);
		}
	}

	patcher.CreatePatch();
}

/**
 * @brief Add special items for non-recursive compare
 * to directory view.
 *
 * Currently only special item is ".." for browsing to
 * parent folders.
 * @return number of items added to view
 */
int CDirView::AddSpecialItems()
{
	CDirDoc *pDoc = GetDocument();
	int retVal = 0;
	bool bEnable = true;
	PathContext pathsParent;
	switch (CheckAllowUpwardDirectory(GetDiffContext(), pDoc->m_pTempPathContext, pathsParent))
	{
	case AllowUpwardDirectory::No:
		bEnable = false;
		// fall through
	default:
		AddParentFolderItem(bEnable);
		retVal = 1;
		// fall through
	case AllowUpwardDirectory::Never:
		break;
	}
	return retVal;
}

/**
 * @brief Add "Parent folder" ("..") item to directory view
 */
void CDirView::AddParentFolderItem(bool bEnable)
{
	AddNewItem(0, (DIFFITEM *)SPECIAL_ITEM_POS, bEnable ? DIFFIMG_DIRUP : DIFFIMG_DIRUP_DISABLE, 0);
}

template <int flag>
void CDirView::OnCtxtDirZip()
{
	if (!HasZipSupport())
	{
		LangMessageBox(IDS_NO_ZIP_SUPPORT, MB_ICONINFORMATION);
		return;
	}

	DirItemEnumerator
	(
		this, LVNI_SELECTED | flag
	).CompressArchive();
}

void CDirView::ShowShellContextMenu(SIDE_TYPE stype)
{
	CShellContextMenu *pContextMenu = nullptr;
	switch (stype)
	{
	case SIDE_LEFT:
		if (m_pShellContextMenuLeft == nullptr)
			m_pShellContextMenuLeft.reset(new CShellContextMenu(LeftCmdFirst, LeftCmdLast));
		pContextMenu = m_pShellContextMenuLeft.get();
		break;
	case SIDE_MIDDLE:
		if (m_pShellContextMenuMiddle == nullptr)
			m_pShellContextMenuMiddle.reset(new CShellContextMenu(MiddleCmdFirst, MiddleCmdLast));
		pContextMenu = m_pShellContextMenuMiddle.get();
		break;
	case SIDE_RIGHT:
		if (m_pShellContextMenuRight == nullptr)
			m_pShellContextMenuRight.reset(new CShellContextMenu(RightCmdFirst, RightCmdLast));
		pContextMenu = m_pShellContextMenuRight.get();
		break;
	}
	if (pContextMenu!=nullptr && ListShellContextMenu(stype))
	{
		CPoint point;
		GetCursorPos(&point);
		HWND hWnd = GetSafeHwnd();
		CFrameWnd *pFrame = GetTopLevelFrame();
		ASSERT(pFrame != nullptr);
		BOOL bAutoMenuEnableOld = pFrame->m_bAutoMenuEnable;
		pFrame->m_bAutoMenuEnable = FALSE;
		BOOL nCmd = TrackPopupMenu(pContextMenu->GetHMENU(), TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, 0, hWnd, nullptr);
		if (nCmd)
			pContextMenu->InvokeCommand(nCmd, hWnd);
		pContextMenu->ReleaseShellContextMenu();
		pFrame->m_bAutoMenuEnable = bAutoMenuEnableOld;
	}
}

template <SIDE_TYPE stype>
void CDirView::OnCtxtDirShellContextMenu()
{
	ShowShellContextMenu(stype);
}

/**
 * @brief Select all visible items in dir compare
 */
void CDirView::OnSelectAll()
{
	// While the user is renaming an item, select all the edited text.
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->SetSel(pEdit->GetWindowTextLength());
	}
	else
	{
		int selCount = m_pList->GetItemCount();

		for (int i = 0; i < selCount; i++)
		{
			// Don't select special items (SPECIAL_ITEM_POS)
			DIFFITEM *diffpos = GetItemKey(i);
			if (diffpos != (DIFFITEM *)SPECIAL_ITEM_POS)
				m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
		}
	}
}

/**
 * @brief Update "Select All" item
 */
void CDirView::OnUpdateSelectAll(CCmdUI* pCmdUI)
{
	bool bEnable = (!IsLabelEdit()) || (m_pList->GetItemCount() > 0);
	pCmdUI->Enable(bEnable);
}

/**
 * @brief Handle clicks in plugin context view in list
 */
void CDirView::OnPluginPredifferMode(UINT nID)
{
	ApplyPluginPrediffSetting(SelBegin(), SelEnd(), GetDiffContext(), 
		(nID == ID_PREDIFF_AUTO) ? PLUGIN_AUTO : PLUGIN_MANUAL);
}

/**
 * @brief Updates just before displaying plugin context view in list
 */
void CDirView::OnUpdatePluginPredifferMode(CCmdUI* pCmdUI)
{
	// 2004-04-03, Perry
	// CMainFrame::OnUpdatePluginUnpackMode handles this for global unpacking
	// and is the template to copy, but here, this is a bit tricky
	// as a group of files may be selected
	// and they may not all have the same setting
	// so I'm not trying this right now

	pCmdUI->Enable(GetOptionsMgr()->GetBool(OPT_PLUGINS_ENABLED));

	BCMenu *pPopup = static_cast<BCMenu*>(pCmdUI->m_pSubMenu);
	if (pPopup == nullptr)
		return;

	std::pair<int, int> counts = CountPredifferYesNo(SelBegin(), SelEnd(), GetDiffContext());

	CheckContextMenu(pPopup, ID_PREDIFF_AUTO, (counts.first > 0));
	CheckContextMenu(pPopup, ID_PREDIFF_MANUAL, (counts.second > 0));
}

/**
 * @brief Refresh cached options.
 */
void CDirView::RefreshOptions()
{
	m_nEscCloses = GetOptionsMgr()->GetInt(OPT_CLOSE_WITH_ESC);
	m_bExpandSubdirs = GetOptionsMgr()->GetBool(OPT_DIRVIEW_EXPAND_SUBDIRS);
	Options::DirColors::Load(GetOptionsMgr(), m_cachedColors);
	m_bUseColors = GetOptionsMgr()->GetBool(OPT_DIRCLR_USE_COLORS);
	m_pList->SetBkColor(m_bUseColors ? m_cachedColors.clrDirMargin : GetSysColor(COLOR_WINDOW));
	Invalidate();
}

/**
 * @brief Copy selected item left side paths (containing filenames) to clipboard.
 */
template<SIDE_TYPE stype>
void CDirView::OnCopyPathnames()
{
	std::list<String> list;
	CopyPathnames(SelBegin(), SelEnd(), std::back_inserter(list), stype, GetDiffContext());
	PutToClipboard(strutils::join(list.begin(), list.end(), _T("\r\n")), GetMainFrame()->GetSafeHwnd());
}

void CDirView::OnCopyBothPathnames()
{
	std::list<String> list;
	CopyBothPathnames(SelBegin(), SelEnd(), std::back_inserter(list), GetDiffContext());
	PutToClipboard(strutils::join(list.begin(), list.end(), _T("\r\n")), GetMainFrame()->GetSafeHwnd());
}

/**
 * @brief Copy selected item filenames to clipboard.
 */
void CDirView::OnCopyFilenames()
{
	std::list<String> list;
	CopyFilenames(SelBegin(), SelEnd(), std::back_inserter(list));
	PutToClipboard(strutils::join(list.begin(), list.end(), _T("\r\n")), GetMainFrame()->GetSafeHwnd());
}

/**
 * @brief Enable/Disable dirview Copy Filenames context menu item.
 */
void CDirView::OnUpdateCopyFilenames(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(Count(&DirActions::IsItemFile).count > 0);
}

/**
 * @brief Copy selected item left side to clipboard.
 */
template<SIDE_TYPE stype>
void CDirView::OnCopyToClipboard()
{
	std::list<String> list;
	CopyPathnames(SelBegin(), SelEnd(), std::back_inserter(list), stype, GetDiffContext());
	PutFilesToClipboard(list, GetMainFrame()->GetSafeHwnd());
}

/**
 * @brief Copy selected item both side to clipboard.
 */
void CDirView::OnCopyBothToClipboard()
{
	std::list<String> list;
	CopyBothPathnames(SelBegin(), SelEnd(), std::back_inserter(list), GetDiffContext());
	PutFilesToClipboard(list, GetMainFrame()->GetSafeHwnd());
}

/**
 * @brief Rename a selected item on both sides.
 *
 */
void CDirView::OnItemRename()
{
	ASSERT(1 == m_pList->GetSelectedCount());
	int nSelItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	ASSERT(-1 != nSelItem);
	m_pList->EditLabel(nSelItem);
}

/**
 * @brief Enable/Disable dirview Rename context menu item.
 *
 */
void CDirView::OnUpdateItemRename(CCmdUI* pCmdUI)
{
	bool bEnabled = (1 == m_pList->GetSelectedCount());
	pCmdUI->Enable(bEnabled && SelBegin() != SelEnd());
}

/**
 * @brief hide selected item filenames (removes them from the ListView)
 */
void CDirView::OnHideFilenames()
{
	m_pList->SetRedraw(FALSE);	// Turn off updating (better performance)
	DirItemIterator it;
	while ((it = SelRevBegin()) != SelRevEnd())
	{
		DIFFITEM &di = *it;
		SetItemViewFlag(di, ViewCustomFlags::HIDDEN, ViewCustomFlags::VISIBILITY);
		DeleteItem(it.m_sel);
		m_nHiddenItems++;
	}
	m_pList->SetRedraw(TRUE);	// Turn updating back on
}

/**
 * @brief update menu item
 */
void CDirView::OnUpdateHideFilenames(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pList->GetSelectedCount() != 0);
}

/// User chose (context menu) Move left to...
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirMoveTo()
{
	DoDirActionTo(stype, &DirActions::MoveTo<stype>, _("Moving files..."));
}

/**
 * @brief Update "Move | Left to..." item
 */
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirMoveTo(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemMovableToOn<stype>);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringTo(stype, counts.count, counts.total).c_str());
}

/**
 * @brief Update title after window is resized.
 */
void CDirView::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	GetDocument()->SetTitle(nullptr);
}

/**
 * @brief Called when user selects 'Delete' from 'Merge' menu.
 */
void CDirView::OnDelete()
{
	DoDirAction(&DirActions::DeleteOnEitherOrBoth, _("Deleting files..."));
}

/**
 * @brief Enables/disables 'Delete' item in 'Merge' menu.
 */
void CDirView::OnUpdateDelete(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(Count(&DirActions::IsItemDeletableOnEitherOrBoth).count > 0);
}

/**
 * @brief Called when item state is changed.
 *
 * Show count of selected items in statusbar.
 */
void CDirView::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	// If item's selected state changed
	if ((pNMListView->uOldState & LVIS_SELECTED) !=
			(pNMListView->uNewState & LVIS_SELECTED))
	{
		if ((pNMListView->iItem % 5000) > 0)
			SetTimer(STATUSBAR_UPDATE, 100, nullptr);
		else
			OnTimer(STATUSBAR_UPDATE);
	}
	*pResult = 0;
}

/**
 * @brief Called before user start to item label edit.
 *
 * Disable label edit if initiated from a user double-click.
 */
afx_msg void CDirView::OnBeginLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = (SelBegin() == SelEnd());

	// If label edit is allowed.
	if (*pResult == FALSE)
	{
		const NMLVDISPINFO *pdi = (NMLVDISPINFO*)pNMHDR;
		ASSERT(pdi != nullptr);

		// Locate the edit box on the right column in case the user changed the
		// column order.
		const int nColPos = m_pColItems->ColLogToPhys(0);

		// Get text from the "File Name" column.
		CString sText = m_pList->GetItemText(pdi->item.iItem, nColPos);
		ASSERT(!sText.IsEmpty());

		// Keep only left file name (separated by '|'). This form occurs
		// when two files exists with same name but not in same case.
		int nPos = sText.Find('|');
		if (-1 != nPos)
		{
			sText = sText.Left(nPos);
		}

		// Set the edit control with the updated text.
		CEdit *pEdit = m_pList->GetEditControl();
		ASSERT(pEdit != nullptr);
		pEdit->SetWindowText(sText);

		m_bUserCancelEdit = false;
	}
}

/**
 * @brief Called when user done with item label edit.
 *
 */
afx_msg void CDirView::OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = FALSE;

	// We can't use the normal condition of pszText==`nullptr` to know if the
	// user cancels editing when file names had different case (e.g.
	// "file.txt|FILE.txt"). The edit text was changed to "file.txt" and
	// if the user accept it as the new file name, pszText is `nullptr`.

	if (!m_bUserCancelEdit)
	{
		CEdit *pEdit = m_pList->GetEditControl();
		ASSERT(pEdit != nullptr);

		CString sText;
		pEdit->GetWindowText(sText);

		if (!sText.IsEmpty())
		{
			try {
				DirItemIterator it(m_pIList.get(), reinterpret_cast<NMLVDISPINFO *>(pNMHDR)->item.iItem);
				*pResult = DoItemRename(it, GetDiffContext(), String(sText));
			} catch (ContentsChangedException& e) {
				AfxMessageBox(e.m_msg.c_str(), MB_ICONWARNING);
			}
		}
	}
}

/**
 * @brief Called when item is marked for rescan.
 * This function marks selected items for rescan and rescans them.
 */
void CDirView::OnMarkedRescan()
{
	std::for_each(SelBegin(), SelEnd(), MarkForRescan);
	if (std::distance(SelBegin(), SelEnd()) > 0)
	{
		m_pSavedTreeState.reset(SaveTreeState(GetDiffContext()));
		GetDocument()->SetMarkedRescan();
		GetDocument()->Rescan();
	}
}

/**
 * @brief Called to update the item count in the status bar
 */
void CDirView::OnUpdateStatusNum(CCmdUI* pCmdUI)
{
	String s; // text to display

	int count = m_pList->GetItemCount();
	int focusItem = GetFocusedItem();

	if (focusItem == -1)
	{
		// No item has focus
		// "Items: %1"
		s = strutils::format_string1(_("Items: %1"), strutils::to_str(count));
	}
	else
	{
		// Don't show number to special items
		DIFFITEM *pos = GetItemKey(focusItem);
		if (pos != (DIFFITEM *)SPECIAL_ITEM_POS)
		{
			// If compare is non-recursive reduce special items count
			bool bRecursive = GetDiffContext().m_bRecursive;
			if (!bRecursive)
			{
				--focusItem;
				--count;
			}
			// "Item %1 of %2"
			s = strutils::format_string2(_("Item %1 of %2"), 
					strutils::to_str(focusItem + 1), strutils::to_str(count));
		}
	}
	pCmdUI->SetText(s.c_str());
}

/**
 * @brief Show all hidden items.
 */
void CDirView::OnViewShowHiddenItems()
{
	SetItemViewFlag(GetDiffContext(), ViewCustomFlags::VISIBLE, ViewCustomFlags::VISIBILITY);
	m_nHiddenItems = 0;
	Redisplay();
}

/**
 * @brief Enable/Disable 'Show hidden items' menuitem.
 */
void CDirView::OnUpdateViewShowHiddenItems(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_nHiddenItems > 0);
}

/**
 * @brief Toggle Tree Mode
 */
void CDirView::OnViewTreeMode()
{
	m_bTreeMode = !m_bTreeMode;
	m_dirfilter.tree_mode = m_bTreeMode;
	GetOptionsMgr()->SaveOption(OPT_TREE_MODE, m_bTreeMode); // reverse
	Redisplay();
}

/**
 * @brief Check/Uncheck 'Tree Mode' menuitem.
 */
void CDirView::OnUpdateViewTreeMode(CCmdUI* pCmdUI)
{
	// Don't show Tree Mode as 'checked' if the
	// menu item is greyed out (disabled).  Its very confusing.
	if( GetDocument()->GetDiffContext().m_bRecursive ) {
		pCmdUI->SetCheck(m_bTreeMode);
		pCmdUI->Enable(TRUE);
	} else {
		pCmdUI->SetCheck(FALSE);
		pCmdUI->Enable(FALSE);
	}
}

/**
 * @brief Expand all subfolders
 */
void CDirView::OnViewExpandAllSubdirs()
{
	ExpandAllSubdirs(GetDiffContext());
	Redisplay();
}

/**
 * @brief Update "Expand All Subfolders" item
 */
void CDirView::OnUpdateViewExpandAllSubdirs(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bTreeMode && GetDiffContext().m_bRecursive);
}

/**
 * @brief Collapse all subfolders
 */
void CDirView::OnViewCollapseAllSubdirs()
{
	CollapseAllSubdirs(GetDiffContext());
	Redisplay();
}

/**
 * @brief Update "Collapse All Subfolders" item
 */
void CDirView::OnUpdateViewCollapseAllSubdirs(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bTreeMode && GetDiffContext().m_bRecursive);
}

void CDirView::OnViewSwapPanes()
{
	GetDocument()->Swap(0, GetDocument()->m_nDirs - 1);
	Redisplay();
}

/**
 * @brief Show/Hide different files/directories
 */
void CDirView::OnOptionsShowDifferent() 
{
	m_dirfilter.show_different = !m_dirfilter.show_different;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT, m_dirfilter.show_different);
	Redisplay();
}

/**
 * @brief Show/Hide identical files/directories
 */
void CDirView::OnOptionsShowIdentical() 
{
	m_dirfilter.show_identical = !m_dirfilter.show_identical;
	GetOptionsMgr()->SaveOption(OPT_SHOW_IDENTICAL, m_dirfilter.show_identical);
	Redisplay();
}

/**
 * @brief Show/Hide left-only files/directories
 */
void CDirView::OnOptionsShowUniqueLeft() 
{
	m_dirfilter.show_unique_left = !m_dirfilter.show_unique_left;
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_LEFT, m_dirfilter.show_unique_left);
	Redisplay();
}

/**
 * @brief Show/Hide middle-only files/directories
 */
void CDirView::OnOptionsShowUniqueMiddle() 
{
	m_dirfilter.show_unique_middle = !m_dirfilter.show_unique_middle;
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_MIDDLE, m_dirfilter.show_unique_middle);
	Redisplay();
}

/**
 * @brief Show/Hide right-only files/directories
 */
void CDirView::OnOptionsShowUniqueRight() 
{
	m_dirfilter.show_unique_right = !m_dirfilter.show_unique_right;
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_RIGHT, m_dirfilter.show_unique_right);
	Redisplay();
}

/**
 * @brief Show/Hide binary files
 */
void CDirView::OnOptionsShowBinaries()
{
	m_dirfilter.show_binaries = !m_dirfilter.show_binaries;
	GetOptionsMgr()->SaveOption(OPT_SHOW_BINARIES, m_dirfilter.show_binaries);
	Redisplay();
}

/**
 * @brief Show/Hide skipped files/directories
 */
void CDirView::OnOptionsShowSkipped()
{
	m_dirfilter.show_skipped = !m_dirfilter.show_skipped;
	GetOptionsMgr()->SaveOption(OPT_SHOW_SKIPPED, m_dirfilter.show_skipped);
	Redisplay();
}

/**
 * @brief Show/Hide different files/folders (Middle and right are identical)
 */
void CDirView::OnOptionsShowDifferentLeftOnly() 
{
	m_dirfilter.show_different_left_only = !m_dirfilter.show_different_left_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_LEFT_ONLY, m_dirfilter.show_different_left_only);
	Redisplay();
}

/**
 * @brief Show/Hide different files/folders (Left and right are identical)
 */
void CDirView::OnOptionsShowDifferentMiddleOnly() 
{
	m_dirfilter.show_different_middle_only = !m_dirfilter.show_different_middle_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_MIDDLE_ONLY, m_dirfilter.show_different_middle_only);
	Redisplay();
}

/**
 * @brief Show/Hide different files/folders (Left and middle are identical)
 */
void CDirView::OnOptionsShowDifferentRightOnly() 
{
	m_dirfilter.show_different_right_only = !m_dirfilter.show_different_right_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_RIGHT_ONLY, m_dirfilter.show_different_right_only);
	Redisplay();
}

/**
 * @brief Show/Hide missing left only files/folders
 */
void CDirView::OnOptionsShowMissingLeftOnly() 
{
	m_dirfilter.show_missing_left_only = !m_dirfilter.show_missing_left_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_MISSING_LEFT_ONLY, m_dirfilter.show_missing_left_only);
	Redisplay();
}

/**
 * @brief Show/Hide missing middle only files/folders
 */
void CDirView::OnOptionsShowMissingMiddleOnly() 
{
	m_dirfilter.show_missing_middle_only = !m_dirfilter.show_missing_middle_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_MISSING_MIDDLE_ONLY, m_dirfilter.show_missing_middle_only);
	Redisplay();
}

/**
 * @brief Show/Hide missing right only files/folders
 */
void CDirView::OnOptionsShowMissingRightOnly() 
{
	m_dirfilter.show_missing_right_only = !m_dirfilter.show_missing_right_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_MISSING_RIGHT_ONLY, m_dirfilter.show_missing_right_only);
	Redisplay();
}

void CDirView::OnUpdateOptionsShowdifferent(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_different);
}

void CDirView::OnUpdateOptionsShowidentical(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_identical);
}

void CDirView::OnUpdateOptionsShowuniqueleft(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_unique_left);
}

void CDirView::OnUpdateOptionsShowuniquemiddle(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_unique_middle);
}

void CDirView::OnUpdateOptionsShowuniqueright(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_unique_right);
}

void CDirView::OnUpdateOptionsShowBinaries(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_binaries);
}

void CDirView::OnUpdateOptionsShowSkipped(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_dirfilter.show_skipped);
}

void CDirView::OnUpdateOptionsShowDifferentLeftOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_different_left_only);
}

void CDirView::OnUpdateOptionsShowDifferentMiddleOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_different_middle_only);
}

void CDirView::OnUpdateOptionsShowDifferentRightOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_different_right_only);
}

void CDirView::OnUpdateOptionsShowMissingLeftOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_missing_left_only);
}

void CDirView::OnUpdateOptionsShowMissingMiddleOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_missing_middle_only);
}

void CDirView::OnUpdateOptionsShowMissingRightOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_missing_right_only);
}

void CDirView::OnMergeCompare()
{
	CWaitCursor waitstatus;
	OpenSelection();
}

template<SELECTIONTYPE seltype>
void CDirView::OnMergeCompare2()
{
	CWaitCursor waitstatus;
	OpenSelection(seltype);
}

void CDirView::OnMergeCompareNonHorizontally()
{
	int sel1, sel2, sel3;
	if (!GetSelectedItems(&sel1, &sel2, &sel3))
		return;
	DirSelectFilesDlg dlg;
	if (sel1 != -1)
		dlg.m_pdi[0] = &GetDiffItem(sel1);
	if (sel2 != -1)
		dlg.m_pdi[1] = &GetDiffItem(sel2);
	if (sel3 != -1)
		dlg.m_pdi[2] = &GetDiffItem(sel3);
	if (dlg.DoModal() == IDOK && dlg.m_selectedButtons.size() > 0)
	{
		CDirDoc *pDoc = GetDocument();
		DWORD dwFlags[3] = {};
		PathContext paths;
		for (int nIndex = 0; nIndex < static_cast<int>(dlg.m_selectedButtons.size()); ++nIndex)
		{
			int n = dlg.m_selectedButtons[nIndex];
			dwFlags[nIndex] = FFILEOPEN_NOMRU | (pDoc->GetReadOnly(n % 3) ? FFILEOPEN_READONLY : 0);
			if (dlg.m_pdi[n / 3])
				paths.SetPath(nIndex, GetItemFileName(pDoc->GetDiffContext(), *dlg.m_pdi[n / 3], n % 3));
		}
		if (paths.GetSize() == 1)
			paths.SetRight(_T(""));
		Open(paths, dwFlags);
	}
}

void CDirView::OnMergeCompareXML()
{
	CWaitCursor waitstatus;
	PackingInfo packingInfo(PLUGIN_BUILTIN_XML);
	OpenSelection(SELECTIONTYPE_NORMAL, &packingInfo, false);
}

void CDirView::OnMergeCompareAs(UINT nID)
{
	CWaitCursor waitstatus;
	OpenSelectionAs(nID);
}

void CDirView::OnUpdateMergeCompare(CCmdUI *pCmdUI)
{
	bool openableForDir = (pCmdUI->m_nID != ID_MERGE_COMPARE_XML &&
						   pCmdUI->m_nID != ID_MERGE_COMPARE_HEX &&
						   pCmdUI->m_nID != ID_MERGE_COMPARE_IMAGE);

	DoUpdateOpen(SELECTIONTYPE_NORMAL, pCmdUI, openableForDir);
}

template<SELECTIONTYPE seltype>
void CDirView::OnUpdateMergeCompare2(CCmdUI *pCmdUI)
{
	DoUpdateOpen(seltype, pCmdUI);
}

void CDirView::OnViewCompareStatistics()
{
	CompareStatisticsDlg dlg(GetDocument()->GetCompareStats());
	dlg.DoModal();
}

/**
 * @brief Count left & right files, and number with editable text encoding
 * @param nLeft [out]  #files on left side selected
 * @param nLeftAffected [out]  #files on left side selected which can have text encoding changed
 * @param nRight [out]  #files on right side selected
 * @param nRightAffected [out]  #files on right side selected which can have text encoding changed
 *
 * Affected files include all except unicode files
 */
void CDirView::FormatEncodingDialogDisplays(CLoadSaveCodepageDlg * dlg)
{
	IntToIntMap currentCodepages = CountCodepages(SelBegin(), SelEnd(), GetDiffContext());

	Counts left, middle, right;
	left = Count(&DirActions::IsItemEditableEncoding<SIDE_LEFT>);
	if (GetDocument()->m_nDirs > 2)
		middle = Count(&DirActions::IsItemEditableEncoding<SIDE_MIDDLE>);
	right = Count(&DirActions::IsItemEditableEncoding<SIDE_RIGHT>);

	// Format strings such as "25 of 30 Files Affected"
	String sLeftAffected = FormatFilesAffectedString(left.count, left.total);
	String sMiddleAffected = (GetDocument()->m_nDirs < 3) ? _T("") : FormatFilesAffectedString(middle.count, middle.total);
	String sRightAffected = FormatFilesAffectedString(right.count, right.total);
	dlg->SetLeftRightAffectStrings(sLeftAffected, sMiddleAffected, sRightAffected);
	int codepage = currentCodepages.FindMaxKey();
	dlg->SetCodepages(codepage);
}

/**
 * @brief Display file encoding dialog to user & handle user's choices
 *
 * This handles DirView invocation, so multiple files may be affected
 */
void CDirView::DoFileEncodingDialog()
{
	CLoadSaveCodepageDlg dlg(GetDocument()->m_nDirs);
	// set up labels about what will be affected
	FormatEncodingDialogDisplays(&dlg);
	dlg.EnableSaveCodepage(false); // disallow setting a separate codepage for saving

	// Invoke dialog
	if (dlg.DoModal() != IDOK)
		return;

	bool affected[3];
	affected[0] = dlg.DoesAffectLeft();
	affected[1] = dlg.DoesAffectMiddle();
	affected[SideToIndex(GetDiffContext(), SIDE_RIGHT)] = dlg.DoesAffectRight();

	ApplyCodepage(SelBegin(), SelEnd(), GetDiffContext(), affected, dlg.GetLoadCodepage());

	m_pList->InvalidateRect(nullptr);
	m_pList->UpdateWindow();

	// TODO: We could loop through any active merge windows belonging to us
	// and see if any of their files are affected
	// but, if they've been edited, we cannot throw away the user's work?
}

/**
 * @brief Display file encoding dialog & handle user's actions
 */
void CDirView::OnFileEncoding()
{
	DoFileEncodingDialog();
}

/** @brief Open help from mainframe when user presses F1*/
void CDirView::OnHelp()
{
	theApp.ShowHelp(DirViewHelpLocation);
}

/**
 * @brief true while user is editing a file name.
 */
bool CDirView::IsLabelEdit() const
{
	return (m_pList->GetEditControl() != nullptr);
}

/**
 * @brief Allow edit "Paste" when renaming an item.
 */
void CDirView::OnEditCopy()
{
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->Copy();
	}
}

/**
 * @brief Allow edit "Cut" when renaming an item.
 */
void CDirView::OnEditCut()
{
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->Cut();
	}
}

/**
* @brief Allow edit "Paste" when renaming an item.
 */
void CDirView::OnEditPaste()
{
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->Paste();
	}
}

/**
 * @brief Allow edit "Undo" when renaming an item.
 */
void CDirView::OnEditUndo()
{
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->Undo();
	}
}

/**
 * @brief Update the tool bar's "Undo" icon. It should be enabled when
 * renaming an item and undo is possible.
 */
void CDirView::OnUpdateEditUndo(CCmdUI* pCmdUI)
{
	CEdit *pEdit = m_pList->GetEditControl();
	pCmdUI->Enable(pEdit && pEdit->CanUndo());
}
/**
 * @brief Returns CShellContextMenu object that owns given HMENU.
 *
 * @param [in] hMenu Handle to the menu to check ownership of.
 * @return Either m_pShellContextMenuLeft, m_pShellContextMenuRight
 *   or `nullptr` if hMenu is not owned by these two.
 */
CShellContextMenu* CDirView::GetCorrespondingShellContextMenu(HMENU hMenu) const
{
	CShellContextMenu* pMenu = nullptr;
	if (m_pShellContextMenuLeft!=nullptr && hMenu == m_pShellContextMenuLeft->GetHMENU())
		pMenu = m_pShellContextMenuLeft.get();
	else if (m_pShellContextMenuMiddle!=nullptr && hMenu == m_pShellContextMenuMiddle->GetHMENU())
		pMenu = m_pShellContextMenuMiddle.get();
	else if (m_pShellContextMenuRight!=nullptr && hMenu == m_pShellContextMenuRight->GetHMENU())
		pMenu = m_pShellContextMenuRight.get();

	return pMenu;
}

/**
 * @brief Handle messages related to correct menu working.
 *
 * We need to requery shell context menu each time we switch from context menu
 * for one side to context menu for other side. Here we check whether we need to
 * requery and call ShellContextMenuHandleMenuMessage.
 */
LRESULT CDirView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	while (message == WM_INITMENUPOPUP)
	{
		HMENU hMenu = (HMENU)wParam;
		if (CShellContextMenu* pMenu = GetCorrespondingShellContextMenu(hMenu))
		{
			if (m_hCurrentMenu != hMenu)
			{
				// re-query context menu once more, because if context menu was queried for right
				// group of files and we are showing menu for left group (or vice versa) menu will
				// be shown incorrectly
				// also, if context menu was last queried for right group of files and we are
				// invoking command for left command will be executed for right group (the last
				// group that menu was requested for)
				// may be a "feature" of Shell

				pMenu->RequeryShellContextMenu();
				m_hCurrentMenu = hMenu;
			}
		}
		break;
	}

	CShellContextMenu* pMenu = GetCorrespondingShellContextMenu(m_hCurrentMenu);

	if (pMenu != nullptr)
	{
		LRESULT res = 0;
		pMenu->HandleMenuMessage(message, wParam, lParam, res);
	}

	return __super::WindowProc(message, wParam, lParam);
}

/**
 * @brief Implement background item coloring
 */
void CDirView::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if (!m_bUseColors) {
		return;
	}

	LPNMLISTVIEW pNM = (LPNMLISTVIEW)pNMHDR;
	*pResult = CDRF_DODEFAULT;

	if (pNM->hdr.code == NM_CUSTOMDRAW)
	{
		LPNMLVCUSTOMDRAW lpC = (LPNMLVCUSTOMDRAW)pNMHDR;

		if (lpC->nmcd.dwDrawStage == CDDS_PREPAINT)
		{
			*pResult =  CDRF_NOTIFYITEMDRAW;
			return;
		}

		if (lpC->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
		{
			*pResult = CDRF_NOTIFYITEMDRAW;
			return;
		}

		if (lpC->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM ))
		{
			GetColors (static_cast<int>(lpC->nmcd.dwItemSpec), lpC->iSubItem, lpC->clrTextBk, lpC->clrText);
		}
	}
}

void CDirView::OnBnClickedComparisonStop()
{
	if (m_pCmpProgressBar != nullptr)
		m_pCmpProgressBar->EndUpdating();
	GetDocument()->AbortCurrentScan();
}

void CDirView::OnBnClickedComparisonPause()
{
	if (m_pCmpProgressBar != nullptr)
		m_pCmpProgressBar->SetPaused(true);
	GetDocument()->PauseCurrentScan();
}

void CDirView::OnBnClickedComparisonContinue()
{
	if (m_pCmpProgressBar != nullptr)
		m_pCmpProgressBar->SetPaused(false);
	GetDocument()->ContinueCurrentScan();
}

/**
 * @brief Populate colors for items in view, depending on difference status
 */
void CDirView::GetColors (int nRow, int nCol, COLORREF& clrBk, COLORREF& clrText) const
{
	const DIFFITEM& di = GetDiffItem (nRow);

	if (di.isEmpty())
	{
		clrText = theApp.GetMainSyntaxColors()->GetColor(COLORINDEX_NORMALTEXT);
		clrBk = theApp.GetMainSyntaxColors()->GetColor(COLORINDEX_BKGND);
	}
	else if (di.diffcode.isResultFiltered())
	{
		clrText = m_cachedColors.clrDirItemFilteredText;
		clrBk = m_cachedColors.clrDirItemFiltered;
	}
	else if (!IsItemExistAll(GetDiffContext(), di))
	{
		clrText = m_cachedColors.clrDirItemNotExistAllText;
		clrBk = m_cachedColors.clrDirItemNotExistAll;
	}
	else if (di.diffcode.isResultDiff())
	{
		clrText = m_cachedColors.clrDirItemDiffText;
		clrBk = m_cachedColors.clrDirItemDiff;
	}
	else if (di.diffcode.isResultSame())
	{
		clrText = m_cachedColors.clrDirItemEqualText;
		clrBk = m_cachedColors.clrDirItemEqual;
	}
	else
	{
		clrText = theApp.GetMainSyntaxColors()->GetColor(COLORINDEX_NORMALTEXT);
		clrBk = theApp.GetMainSyntaxColors()->GetColor(COLORINDEX_BKGND);
	}
}

void CDirView::OnSearch()
{
	CDirDoc *pDoc = GetDocument();
	m_pList->SetRedraw(FALSE);	// Turn off updating (better performance)
	int nRows = m_pList->GetItemCount();
	for (int currRow = nRows - 1; currRow >= 0; currRow--)
	{
		DIFFITEM *pos = GetItemKey(currRow);
		if (pos == (DIFFITEM *)SPECIAL_ITEM_POS)
			continue;

		bool bFound = false;
		DIFFITEM &di = GetDiffItem(currRow);
		PathContext paths;
		for (int i = 0; i < pDoc->m_nDirs; i++)
		{
			if (di.diffcode.exists(i) && !di.diffcode.isDirectory())
			{
				GetItemFileNames(currRow, &paths);
				UniMemFile ufile;
				if (!ufile.OpenReadOnly(paths[i]))
					continue;

				ufile.SetUnicoding(di.diffFileInfo[i].encoding.m_unicoding);
				ufile.SetBom(di.diffFileInfo[i].encoding.m_bom);
				ufile.SetCodepage(di.diffFileInfo[i].encoding.m_codepage);

				ufile.ReadBom();

				String line;
				for (;;)
				{
					bool lossy = false;
					if (!ufile.ReadString(line, &lossy))
						break;
					
					if (_tcsstr(line.c_str(), _T("DirView")))
					{
						bFound = true;
						break;
					}
				}

				ufile.Close();
				if (bFound)
					break;
			}
		}
		if (!bFound)
		{
			SetItemViewFlag(di, ViewCustomFlags::HIDDEN, ViewCustomFlags::VISIBILITY);
			DeleteItem(currRow);
			m_nHiddenItems++;
		}
	}
	m_pList->SetRedraw(TRUE);	// Turn updating back on
}

/**
 * @brief Drag files/directories from folder compare listing view.
 */
void CDirView::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult) 
{
	COleDataSource *DropData = new COleDataSource();

	std::list<String> list;
	CopyPathnamesForDragAndDrop(SelBegin(), SelEnd(), std::back_inserter(list), GetDiffContext());
	String filesForDroping = strutils::join(list.begin(), list.end(), _T("\n")) + _T("\n");

	CSharedFile file(GMEM_DDESHARE | GMEM_MOVEABLE | GMEM_ZEROINIT);
	file.Write(filesForDroping.data(), static_cast<unsigned>((filesForDroping.length() + 1) * sizeof(TCHAR)));
	
	HGLOBAL hMem = GlobalReAlloc(file.Detach(), (filesForDroping.length() + 1) * sizeof(TCHAR), 0);
	if (hMem != nullptr) 
	{
		DropData->CacheGlobalData(CF_UNICODETEXT, hMem);
		DROPEFFECT de = DropData->DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_MOVE, nullptr);
	}

	*pResult = 0;
}

/// Assign column name, using string resource & current column ordering
void CDirView::NameColumn(const DirColInfo *col, int subitem)
{
	int phys = m_pColItems->ColLogToPhys(subitem);
	if (phys>=0)
	{
		String s = tr(col->idNameContext, col->idName);
		LV_COLUMN lvc;
		lvc.mask = LVCF_TEXT;
		lvc.pszText = const_cast<LPTSTR>(s.c_str());
		m_pList->SetColumn(phys, &lvc);
	}
}

/// Load column names from string table
void CDirView::UpdateColumnNames()
{
	int ncols = m_pColItems->GetColCount();
	for (int i=0; i<ncols; ++i)
	{
		const DirColInfo* col = m_pColItems->GetDirColInfo(i);
		NameColumn(col, i);
	}
}

/**
 * @brief Set alignment of columns.
 */
void CDirView::SetColAlignments()
{
	int ncols = m_pColItems->GetColCount();
	for (int i=0; i<ncols; ++i)
	{
		const DirColInfo * col = m_pColItems->GetDirColInfo(i);
		LVCOLUMN lvc;
		lvc.mask = LVCF_FMT;
		lvc.fmt = col->alignment;
		m_pList->SetColumn(m_pColItems->ColLogToPhys(i), &lvc);
	}
}

CDirView::CompareState::CompareState(const CDiffContext *pCtxt, const DirViewColItems *pColItems, int sortCol, bool bSortAscending, bool bTreeMode)
: pCtxt(pCtxt)
, pColItems(pColItems)
, sortCol(sortCol)
, bSortAscending(bSortAscending)
, bTreeMode(bTreeMode)
{
}

/// Compare two specified rows during a sort operation (windows callback)
int CALLBACK CDirView::CompareState::CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CompareState *pThis = reinterpret_cast<CompareState*>(lParamSort);
	// Sort special items always first in dir view
	if (lParam1 == -1)
		return -1;
	if (lParam2 == -1)
		return 1;

	DIFFITEM *diffposl = (DIFFITEM *)lParam1;
	DIFFITEM *diffposr = (DIFFITEM *)lParam2;
	const DIFFITEM &ldi = pThis->pCtxt->GetDiffAt(diffposl);
	const DIFFITEM &rdi = pThis->pCtxt->GetDiffAt(diffposr);
	// compare 'left' and 'right' parameters as appropriate
	int retVal = pThis->pColItems->ColSort(pThis->pCtxt, pThis->sortCol, ldi, rdi, pThis->bTreeMode);
	// return compare result, considering sort direction
	return pThis->bSortAscending ? retVal : -retVal;
}

/// Add new item to list view
int CDirView::AddNewItem(int i, DIFFITEM *diffpos, int iImage, int iIndent)
{
	LV_ITEM lvItem;
	lvItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE | LVIF_INDENT;
	lvItem.iItem = i;
	lvItem.iIndent = iIndent;
	lvItem.iSubItem = 0;
	lvItem.pszText = LPSTR_TEXTCALLBACK;
	lvItem.lParam = (LPARAM)diffpos;
	lvItem.iImage = iImage;
	return GetListCtrl().InsertItem(&lvItem);
}

/**
 * @brief Update listview display of details for specified row
 * @note Customising shownd data should be done here
 */
void CDirView::UpdateDiffItemStatus(UINT nIdx)
{
	GetListCtrl().RedrawItems(nIdx, nIdx);
}

static String rgDispinfoText[2]; // used in function below

/**
 * @brief Allocate a text buffer to assign to NMLVDISPINFO::item::pszText
 * Quoting from SDK Docs:
 *	If the LVITEM structure is receiving item text, the pszText and cchTextMax
 *	members specify the address and size of a buffer. You can either copy text to
 *	the buffer or assign the address of a string to the pszText member. In the
 *	latter case, you must not change or delete the string until the corresponding
 *	item text is deleted or two additional LVN_GETDISPINFO messages have been sent.
 */
static LPTSTR NTAPI AllocDispinfoText(const String &s)
{
	static int i = 0;
	LPCTSTR pszText = (rgDispinfoText[i] = s).c_str();
	i ^= 1;
	return (LPTSTR)pszText;
}

/**
 * @brief Respond to LVN_GETDISPINFO message
 */
void CDirView::ReflectGetdispinfo(NMLVDISPINFO *pParam)
{
	int nIdx = pParam->item.iItem;
	int i = m_pColItems->ColPhysToLog(pParam->item.iSubItem);
	DIFFITEM *key = GetItemKey(nIdx);
	if (key == (DIFFITEM *)SPECIAL_ITEM_POS)
	{
		if (m_pColItems->IsColName(i))
		{
			pParam->item.pszText = _T("..");
		}
		return;
	}
	if (!GetDocument()->HasDiffs())
		return;
	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (pParam->item.mask & LVIF_TEXT)
	{
		String s = m_pColItems->ColGetTextToDisplay(&ctxt, i, di);
		pParam->item.pszText = AllocDispinfoText(s);
	}
	if (pParam->item.mask & LVIF_IMAGE)
	{
		pParam->item.iImage = GetColImage(di);
	}
}

/**
 * @brief User examines & edits which columns are displayed in dirview, and in which order
 */
void CDirView::OnEditColumns()
{
	CDirColsDlg dlg;
	// List all the currently displayed columns
	for (int col=0; col<GetListCtrl().GetHeaderCtrl()->GetItemCount(); ++col)
	{
		int l = m_pColItems->ColPhysToLog(col);
		dlg.AddColumn(m_pColItems->GetColDisplayName(l), m_pColItems->GetColDescription(l), l, col);
	}
	// Now add all the columns not currently displayed
	int l=0;
	for (l=0; l<m_pColItems->GetColCount(); ++l)
	{
		if (m_pColItems->ColLogToPhys(l)==-1)
		{
			dlg.AddColumn(m_pColItems->GetColDisplayName(l), m_pColItems->GetColDescription(l), l);
		}
	}

	// Add default order of columns for resetting to defaults
	for (l = 0; l < m_pColItems->GetColCount(); ++l)
	{
		int phy = m_pColItems->GetColDefaultOrder(l);
		dlg.AddDefColumn(m_pColItems->GetColDisplayName(l), l, phy);
	}

	if (dlg.DoModal() != IDOK)
		return;

	const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS;
	GetOptionsMgr()->SaveOption(keyname,
		(dlg.m_bReset ? m_pColItems->ResetColumnWidths(GetDefColumnWidth()) :
		                m_pColItems->SaveColumnWidths(std::bind(&CListCtrl::GetColumnWidth, m_pList, _1))));

	// Reset our data to reflect the new data from the dialog
	const CDirColsDlg::ColumnArray & cols = dlg.GetColumns();
	m_pColItems->ClearColumnOrders();
	const int sortColumn = GetOptionsMgr()->GetInt((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3);
	std::vector<int> colorder(m_pColItems->GetColCount(), -1);
	for (CDirColsDlg::ColumnArray::const_iterator iter = cols.begin();
		iter != cols.end(); ++iter)
	{
		int log = iter->log_col; 
		int phy = iter->phy_col;
		colorder[log] = phy;

		// If sorted column was hidden, reset sorting
		if (log == sortColumn && phy < 0)
		{
			GetOptionsMgr()->Reset((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3);
			GetOptionsMgr()->Reset(OPT_DIRVIEW_SORT_ASCENDING);
		}
	}

	m_pColItems->SetColumnOrdering(&colorder[0]);

	if (m_pColItems->GetDispColCount() < 1)
	{
		// Ignore them if they didn't leave a column showing
		m_pColItems->ResetColumnOrdering();
	}
	else
	{
		ReloadColumns();
		Redisplay();
	}
}

DirActions CDirView::MakeDirActions(DirActions::method_type func) const
{
	const CDirDoc *pDoc = GetDocument();
	return DirActions(pDoc->GetDiffContext(), pDoc->GetReadOnly(), func);
}

DirActions CDirView::MakeDirActions(DirActions::method_type2 func) const
{
	const CDirDoc *pDoc = GetDocument();
	return DirActions(pDoc->GetDiffContext(), pDoc->GetReadOnly(), nullptr, func);
}

const CDiffContext& CDirView::GetDiffContext() const
{
	return GetDocument()->GetDiffContext();
}

CDiffContext& CDirView::GetDiffContext()
{
	return GetDocument()->GetDiffContext();
}
