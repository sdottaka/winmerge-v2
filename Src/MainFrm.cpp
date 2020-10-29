/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  MainFrm.cpp
 *
 * @brief Implementation of the CMainFrame class
 */

#include "StdAfx.h"
#include "MainFrm.h"
#include <vector>
#include <afxinet.h>
#include <boost/range/mfc.hpp>
#include "Constants.h"
#include "Merge.h"
#include "FileFilterHelper.h"
#include "UnicodeString.h"
#include "BCMenu.h"
#include "OpenFrm.h"
#include "DirFrame.h"		// Include type information
#include "MergeEditFrm.h"
#include "HexMergeFrm.h"
#include "DirView.h"
#include "DirDoc.h"
#include "OpenDoc.h"
#include "MergeDoc.h"
#include "MergeEditView.h"
#include "HexMergeDoc.h"
#include "HexMergeView.h"
#include "ImgMergeFrm.h"
#include "LineFiltersList.h"
#include "ConflictFileParser.h"
#include "LineFiltersDlg.h"
#include "paths.h"
#include "Environment.h"
#include "PatchTool.h"
#include "Plugins.h"
#include "ConfigLog.h"
#include "7zCommon.h"
#include "Merge7zFormatMergePluginImpl.h"
#include "FileFiltersDlg.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "codepage_detect.h"
#include "unicoder.h"
#include "PreferencesDlg.h"
#include "FileOrFolderSelect.h"
#include "PluginsListDlg.h"
#include "stringdiffs.h"
#include "MergeCmdLineInfo.h"
#include "OptionsFont.h"
#include "JumpList.h"
#include "DropHandler.h"
#include "LanguageSelect.h"
#include "VersionInfo.h"
#include "Bitmap.h"
#include "CCrystalTextMarkers.h"
#include "utils/hqbitmap.h"

#include "WindowsManagerDialog.h"

using std::vector;
using boost::begin;
using boost::end;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static void LoadToolbarImageList(int orgImageWidth, int newImageHeight, UINT nIDResource, bool bGrayscale, CImageList& ImgList);
static CPtrList &GetDocList(CMultiDocTemplate *pTemplate);
template<class DocClass>
DocClass * GetMergeDocForDiff(CMultiDocTemplate *pTemplate, CDirDoc *pDirDoc, int nFiles, bool bMakeVisible = true);

/**
 * @brief A table associating menuitem id, icon and menus to apply.
 */
const CMainFrame::MENUITEM_ICON CMainFrame::m_MenuIcons[] = {
	{ ID_FILE_OPENCONFLICT,			IDB_FILE_OPENCONFLICT,			CMainFrame::MENU_ALL },
	{ ID_FILE_NEW3,                 IDB_FILE_NEW3,                  CMainFrame::MENU_ALL },
	{ ID_EDIT_COPY,					IDB_EDIT_COPY,					CMainFrame::MENU_ALL },
	{ ID_EDIT_CUT,					IDB_EDIT_CUT,					CMainFrame::MENU_ALL },
	{ ID_EDIT_PASTE,				IDB_EDIT_PASTE,					CMainFrame::MENU_ALL },
	{ ID_EDIT_FIND,					IDB_EDIT_SEARCH,				CMainFrame::MENU_ALL },
	{ ID_WINDOW_CASCADE,			IDB_WINDOW_CASCADE,				CMainFrame::MENU_ALL },
	{ ID_WINDOW_TILE_HORZ,			IDB_WINDOW_HORIZONTAL,			CMainFrame::MENU_ALL },
	{ ID_WINDOW_TILE_VERT,			IDB_WINDOW_VERTICAL,			CMainFrame::MENU_ALL },
	{ ID_FILE_CLOSE,				IDB_WINDOW_CLOSE,				CMainFrame::MENU_ALL },
	{ ID_WINDOW_CHANGE_PANE,		IDB_WINDOW_CHANGEPANE,			CMainFrame::MENU_ALL },
	{ ID_EDIT_WMGOTO,				IDB_EDIT_GOTO,					CMainFrame::MENU_ALL },
	{ ID_EDIT_REPLACE,				IDB_EDIT_REPLACE,				CMainFrame::MENU_ALL },
	{ ID_VIEW_SELECTFONT,			IDB_VIEW_SELECTFONT,			CMainFrame::MENU_ALL },
	{ ID_APP_EXIT,					IDB_FILE_EXIT,					CMainFrame::MENU_ALL },
	{ ID_HELP_CONTENTS,				IDB_HELP_CONTENTS,				CMainFrame::MENU_ALL },
	{ ID_EDIT_SELECT_ALL,			IDB_EDIT_SELECTALL,				CMainFrame::MENU_ALL },
	{ ID_TOOLS_FILTERS,				IDB_TOOLS_FILTERS,				CMainFrame::MENU_ALL },
	{ ID_TOOLS_CUSTOMIZECOLUMNS,	IDB_TOOLS_COLUMNS,				CMainFrame::MENU_ALL },
	{ ID_TOOLS_GENERATEPATCH,		IDB_TOOLS_GENERATEPATCH,		CMainFrame::MENU_ALL },
	{ ID_PLUGINS_LIST,				IDB_PLUGINS_LIST,				CMainFrame::MENU_ALL },
	{ ID_COPY_FROM_LEFT,			IDB_COPY_FROM_LEFT,				CMainFrame::MENU_ALL },
	{ ID_COPY_FROM_RIGHT,			IDB_COPY_FROM_RIGHT,			CMainFrame::MENU_ALL },
	{ ID_FILE_PRINT,				IDB_FILE_PRINT,					CMainFrame::MENU_FILECMP },
	{ ID_TOOLS_GENERATEREPORT,		IDB_TOOLS_GENERATEREPORT,		CMainFrame::MENU_FILECMP },
	{ ID_EDIT_TOGGLE_BOOKMARK,		IDB_EDIT_TOGGLE_BOOKMARK,		CMainFrame::MENU_FILECMP },
	{ ID_EDIT_GOTO_NEXT_BOOKMARK,	IDB_EDIT_GOTO_NEXT_BOOKMARK,	CMainFrame::MENU_FILECMP },
	{ ID_EDIT_GOTO_PREV_BOOKMARK,	IDB_EDIT_GOTO_PREV_BOOKMARK,	CMainFrame::MENU_FILECMP },
	{ ID_EDIT_CLEAR_ALL_BOOKMARKS,	IDB_EDIT_CLEAR_ALL_BOOKMARKS,	CMainFrame::MENU_FILECMP },
	{ ID_VIEW_ZOOMIN,				IDB_VIEW_ZOOMIN,				CMainFrame::MENU_FILECMP },
	{ ID_VIEW_ZOOMOUT,				IDB_VIEW_ZOOMOUT,				CMainFrame::MENU_FILECMP },
	{ ID_MERGE_COMPARE,				IDB_MERGE_COMPARE,				CMainFrame::MENU_FOLDERCMP },
	{ ID_MERGE_COMPARE_LEFT1_LEFT2,		IDB_MERGE_COMPARE_LEFT1_LEFT2,	CMainFrame::MENU_FOLDERCMP },
	{ ID_MERGE_COMPARE_RIGHT1_RIGHT2,	IDB_MERGE_COMPARE_RIGHT1_RIGHT2,CMainFrame::MENU_FOLDERCMP },
	{ ID_MERGE_COMPARE_LEFT1_RIGHT2,	IDB_MERGE_COMPARE_LEFT1_RIGHT2,	CMainFrame::MENU_FOLDERCMP },
	{ ID_MERGE_COMPARE_LEFT2_RIGHT1,	IDB_MERGE_COMPARE_LEFT2_RIGHT1,	CMainFrame::MENU_FOLDERCMP },
	{ ID_MERGE_DELETE,				IDB_MERGE_DELETE,				CMainFrame::MENU_FOLDERCMP },
	{ ID_TOOLS_GENERATEREPORT,		IDB_TOOLS_GENERATEREPORT,		CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_LEFT_TO_RIGHT,	IDB_LEFT_TO_RIGHT,				CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_LEFT_TO_MIDDLE,	IDB_LEFT_TO_MIDDLE,				CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_RIGHT_TO_LEFT,	IDB_RIGHT_TO_LEFT,				CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_RIGHT_TO_MIDDLE,	IDB_RIGHT_TO_MIDDLE,			CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_MIDDLE_TO_LEFT,	IDB_MIDDLE_TO_LEFT,				CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_MIDDLE_TO_RIGHT,	IDB_MIDDLE_TO_RIGHT,			CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_LEFT_TO_BROWSE,	IDB_LEFT_TO_BROWSE,				CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_MIDDLE_TO_BROWSE,	IDB_MIDDLE_TO_BROWSE,			CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_RIGHT_TO_BROWSE,	IDB_RIGHT_TO_BROWSE,			CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_MOVE_LEFT_TO_BROWSE,	IDB_MOVE_LEFT_TO_BROWSE,		CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_MOVE_MIDDLE_TO_BROWSE,	IDB_MOVE_MIDDLE_TO_BROWSE,		CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_MOVE_RIGHT_TO_BROWSE,	IDB_MOVE_RIGHT_TO_BROWSE,		CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_DEL_LEFT,				IDB_LEFT,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_DEL_MIDDLE,			IDB_MIDDLE,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_DEL_RIGHT,				IDB_RIGHT,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_DEL_BOTH,				IDB_BOTH,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_DEL_ALL,				IDB_ALL,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_PATHNAMES_LEFT,	IDB_LEFT,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_PATHNAMES_MIDDLE,	IDB_MIDDLE,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_PATHNAMES_RIGHT,	IDB_RIGHT,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_PATHNAMES_BOTH,	IDB_BOTH,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_PATHNAMES_ALL,	IDB_ALL,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_LEFT_TO_CLIPBOARD, IDB_LEFT,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_MIDDLE_TO_CLIPBOARD, IDB_MIDDLE,					CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_RIGHT_TO_CLIPBOARD, IDB_RIGHT,					CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_BOTH_TO_CLIPBOARD, IDB_BOTH,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_COPY_ALL_TO_CLIPBOARD, IDB_ALL,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_ZIP_LEFT,				IDB_LEFT,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_ZIP_MIDDLE,			IDB_MIDDLE,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_ZIP_RIGHT,				IDB_RIGHT,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_ZIP_BOTH,				IDB_BOTH,						CMainFrame::MENU_FOLDERCMP },
	{ ID_DIR_ZIP_ALL,				IDB_ALL,						CMainFrame::MENU_FOLDERCMP }
};


/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, DpiAware::CDpiAwareWnd<CMDIFrameWnd>)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_MENUCHAR()
	ON_WM_MEASUREITEM()
	ON_WM_INITMENUPOPUP()
	ON_WM_INITMENU()
	ON_WM_CREATE()
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_HELP_GNULICENSE, OnHelpGnulicense)
	ON_COMMAND(ID_OPTIONS, OnOptions)
	ON_COMMAND(ID_VIEW_SELECTFONT, OnViewSelectfont)
	ON_COMMAND(ID_VIEW_USEDEFAULTFONT, OnViewUsedefaultfont)
	ON_COMMAND(ID_HELP_CONTENTS, OnHelpContents)
	ON_WM_CLOSE()
	ON_COMMAND(ID_TOOLS_GENERATEPATCH, OnToolsGeneratePatch)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_COMMAND_RANGE(ID_UNPACK_MANUAL, ID_UNPACK_AUTO, OnPluginUnpackMode)
	ON_UPDATE_COMMAND_UI_RANGE(ID_UNPACK_MANUAL, ID_UNPACK_AUTO, OnUpdatePluginUnpackMode)
	ON_COMMAND_RANGE(ID_PREDIFFER_MANUAL, ID_PREDIFFER_AUTO, OnPluginPrediffMode)
	ON_UPDATE_COMMAND_UI_RANGE(ID_PREDIFFER_MANUAL, ID_PREDIFFER_AUTO, OnUpdatePluginPrediffMode)
	ON_UPDATE_COMMAND_UI(ID_RELOAD_PLUGINS, OnUpdateReloadPlugins)
	ON_COMMAND(ID_RELOAD_PLUGINS, OnReloadPlugins)
	ON_COMMAND(ID_HELP_GETCONFIG, OnSaveConfigData)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_NEW3, OnFileNew3)
	ON_COMMAND(ID_TOOLS_FILTERS, OnToolsFilters)
	ON_COMMAND(ID_VIEW_STATUS_BAR, OnViewStatusBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TAB_BAR, OnUpdateViewTabBar)
	ON_COMMAND(ID_VIEW_TAB_BAR, OnViewTabBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_RESIZE_PANES, OnUpdateResizePanes)
	ON_COMMAND(ID_VIEW_RESIZE_PANES, OnResizePanes)
	ON_COMMAND(ID_FILE_OPENPROJECT, OnFileOpenProject)
	ON_MESSAGE(WM_COPYDATA, OnCopyData)
	ON_MESSAGE(WM_USER+1, OnUser1)
	ON_COMMAND(ID_WINDOW_CLOSEALL, OnWindowCloseAll)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_CLOSEALL, OnUpdateWindowCloseAll)
	ON_COMMAND(ID_FILE_SAVEPROJECT, OnSaveProject)
	ON_WM_ACTIVATEAPP()
	ON_COMMAND_RANGE(ID_TOOLBAR_NONE, ID_TOOLBAR_HUGE, OnToolbarSize)
	ON_UPDATE_COMMAND_UI_RANGE(ID_TOOLBAR_NONE, ID_TOOLBAR_HUGE, OnUpdateToolbarSize)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
	ON_COMMAND(ID_HELP_RELEASENOTES, OnHelpReleasenotes)
	ON_COMMAND(ID_HELP_TRANSLATIONS, OnHelpTranslations)
	ON_COMMAND(ID_FILE_OPENCONFLICT, OnFileOpenConflict)
	ON_COMMAND(ID_PLUGINS_LIST, OnPluginsList)
	ON_UPDATE_COMMAND_UI(ID_STATUS_PLUGIN, OnUpdatePluginName)
	ON_NOTIFY(TBN_DROPDOWN, AFX_IDW_TOOLBAR, OnDiffOptionsDropDown)
	ON_COMMAND_RANGE(IDC_DIFF_WHITESPACE_COMPARE, IDC_DIFF_WHITESPACE_IGNOREALL, OnDiffWhitespace)
	ON_UPDATE_COMMAND_UI_RANGE(IDC_DIFF_WHITESPACE_COMPARE, IDC_DIFF_WHITESPACE_IGNOREALL, OnUpdateDiffWhitespace)
	ON_COMMAND(IDC_DIFF_CASESENSITIVE, OnDiffCaseSensitive)
	ON_UPDATE_COMMAND_UI(IDC_DIFF_CASESENSITIVE, OnUpdateDiffCaseSensitive)
	ON_COMMAND(IDC_DIFF_IGNOREEOL, OnDiffIgnoreEOL)
	ON_UPDATE_COMMAND_UI(IDC_DIFF_IGNOREEOL, OnUpdateDiffIgnoreEOL)
	ON_COMMAND(IDC_DIFF_IGNORECP, OnDiffIgnoreCP)
	ON_UPDATE_COMMAND_UI(IDC_DIFF_IGNORECP, OnUpdateDiffIgnoreCP)
	ON_COMMAND(IDC_RECURS_CHECK, OnIncludeSubfolders)
	ON_UPDATE_COMMAND_UI(IDC_RECURS_CHECK, OnUpdateIncludeSubfolders)
	ON_COMMAND_RANGE(ID_COMPMETHOD_FULL_CONTENTS, ID_COMPMETHOD_SIZE, OnCompareMethod)
	ON_UPDATE_COMMAND_UI_RANGE(ID_COMPMETHOD_FULL_CONTENTS, ID_COMPMETHOD_SIZE, OnUpdateCompareMethod)
	ON_COMMAND_RANGE(ID_MRU_FIRST, ID_MRU_LAST, OnMRUs)
	ON_UPDATE_COMMAND_UI(ID_MRU_FIRST, OnUpdateNoMRUs)
	ON_UPDATE_COMMAND_UI(ID_NO_MRU, OnUpdateNoMRUs)
	ON_COMMAND(ID_ACCEL_QUIT, &CMainFrame::OnAccelQuit)
	ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
	ON_COMMAND_EX(ID_WINDOW_ARRANGE, OnMDIWindowCmd)
	ON_COMMAND_EX(ID_WINDOW_CASCADE, OnMDIWindowCmd)
	ON_COMMAND_EX(ID_WINDOW_TILE_HORZ, OnMDIWindowCmd)
	ON_COMMAND_EX(ID_WINDOW_TILE_VERT, OnMDIWindowCmd)
	//}}AFX_MSG_MAP
	ON_MESSAGE(WMU_CHILDFRAMEADDED, &CMainFrame::OnChildFrameAdded)
	ON_MESSAGE(WMU_CHILDFRAMEREMOVED, &CMainFrame::OnChildFrameRemoved)
	ON_MESSAGE(WMU_CHILDFRAMEACTIVATE, &CMainFrame::OnChildFrameActivate)
	ON_MESSAGE(WMU_CHILDFRAMEACTIVATED, &CMainFrame::OnChildFrameActivated)
END_MESSAGE_MAP()

/**
 * @brief MainFrame statusbar panels/indicators
 */
static UINT StatusbarIndicators[] =
{
	ID_SEPARATOR,           // Plugin name
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,           // Merge mode
	ID_SEPARATOR,           // Diff number
	ID_INDICATOR_CAPS,      // Caps Lock
	ID_INDICATOR_NUM,       // Num Lock
	ID_INDICATOR_OVR,       // Insert
};

/**
  * @brief Return a const reference to a CMultiDocTemplate's list of documents.
  */
static CPtrList &GetDocList(CMultiDocTemplate *pTemplate)
{
	struct Template : public CMultiDocTemplate
	{
	public:
		using CMultiDocTemplate::m_docList;
	};
	return static_cast<struct Template *>(pTemplate)->m_docList;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

/**
 * @brief MainFrame constructor. Loads settings from registry.
 * @todo Preference for logging?
 */
CMainFrame::CMainFrame()
: m_bFirstTime(true)
, m_pDropHandler(nullptr)
, m_bShowErrors(false)
, m_lfDiff(Options::Font::Load(GetOptionsMgr(), OPT_FONT_FILECMP))
, m_lfDir(Options::Font::Load(GetOptionsMgr(), OPT_FONT_DIRCMP))
, m_layoutManager(this)
{
	m_layoutManager.SetTileLayoutEnabled(true);
}

CMainFrame::~CMainFrame()
{
	GetOptionsMgr()->SaveOption(OPT_TABBAR_AUTO_MAXWIDTH, m_wndTabBar.GetAutoMaxWidth());
	strdiff::Close();

	m_arrChild.RemoveAll();
}

const TCHAR CMainFrame::szClassName[] = _T("WinMergeWindowClassW");

/**
 * @brief Change MainFrame window class name
 *        see http://support.microsoft.com/kb/403825/ja
 */
BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	WNDCLASS wndcls;
	BOOL bRes = __super::PreCreateWindow(cs);
	HINSTANCE hInst = AfxGetInstanceHandle();
	// see if the class already exists
	if (!::GetClassInfo(hInst, szClassName, &wndcls))
	{
		// get default stuff
		::GetClassInfo(hInst, cs.lpszClass, &wndcls);
		// register a new class
		wndcls.lpszClassName = szClassName;
		wndcls.hIcon = ::LoadIcon(hInst, MAKEINTRESOURCE(IDR_MAINFRAME));
		::RegisterClass(&wndcls);
	}
	cs.lpszClass = szClassName;
	return bRes;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_wndMDIClient.SubclassWindow(m_hWndMDIClient);
	m_wndMDIClient.UpdateDpi();

	if (!CreateToolbar())
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}
	
	if (!m_wndTabBar.Create(this))
	{
		TRACE0("Failed to create tab bar\n");
		return -1;      // fail to create
	}
	m_wndTabBar.SetAutoMaxWidth(GetOptionsMgr()->GetBool(OPT_TABBAR_AUTO_MAXWIDTH));

	if (!GetOptionsMgr()->GetBool(OPT_SHOW_TABBAR))
		__super::ShowControlBar(&m_wndTabBar, false, 0);

	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}
	theApp.SetIndicators(m_wndStatusBar, StatusbarIndicators,
			static_cast<int>(std::size(StatusbarIndicators)));

	m_wndStatusBar.SetPaneInfo(0, 0, SBPS_STRETCH | SBPS_NOBORDERS, 0);
	m_wndStatusBar.SetPaneInfo(1, ID_STATUS_PLUGIN, 0, PointToPixel(225));
	m_wndStatusBar.SetPaneInfo(2, ID_STATUS_MERGINGMODE, 0, PointToPixel(75)); 
	m_wndStatusBar.SetPaneInfo(3, ID_STATUS_DIFFNUM, 0, PointToPixel(112)); 

	if (!GetOptionsMgr()->GetBool(OPT_SHOW_STATUSBAR))
		__super::ShowControlBar(&m_wndStatusBar, false, 0);

	m_pDropHandler = new DropHandler(std::bind(&CMainFrame::OnDropFiles, this, std::placeholders::_1));
	RegisterDragDrop(m_hWnd, m_pDropHandler);

	m_wndMDIClient.ModifyStyleEx(WS_EX_CLIENTEDGE, 0);

	return 0;
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	__super::OnTimer(nIDEvent);

	if (nIDEvent == IDT_UPDATEMAINMENU)
	{
		KillTimer(nIDEvent);

		BOOL bMaximized;
		MDIGetActive(&bMaximized);

		// When MDI maximized the window icon is drawn on the menu bar, so we
		// need to notify it that our icon has changed.
		if (bMaximized)
			DrawMenuBar();

		OnUpdateFrameTitle(FALSE);
	}
}

void CMainFrame::OnDestroy(void)
{
	if (m_pDropHandler != nullptr)
		RevokeDragDrop(m_hWnd);
}

static HMENU GetSubmenu(HMENU mainMenu, UINT nIDFirstMenuItem, bool bFirstSubmenu)
{
	int i;
	for (i = 0 ; i < ::GetMenuItemCount(mainMenu) ; i++)
		if (::GetMenuItemID(::GetSubMenu(mainMenu, i), 0) == nIDFirstMenuItem)
			break;
	HMENU menu = ::GetSubMenu(mainMenu, i);

	if (!bFirstSubmenu)
	{
		// look for last submenu
		for (i = ::GetMenuItemCount(menu) ; i >= 0  ; i--)
			if (::GetSubMenu(menu, i) != nullptr)
				return ::GetSubMenu(menu, i);
	}
	else
	{
		// look for first submenu
		for (i = 0 ; i < ::GetMenuItemCount(menu) ; i++)
			if (::GetSubMenu(menu, i) != nullptr)
				return ::GetSubMenu(menu, i);
	}

	// error, submenu not found
	return nullptr;
}

/** 
 * @brief Find the scripts submenu from the main menu
 * As now this is the first submenu in "Edit" menu
 * We find the "Edit" menu by looking for a menu 
 *  starting with ID_EDIT_UNDO.
 */
HMENU CMainFrame::GetScriptsSubmenu(HMENU mainMenu)
{
	return GetSubmenu(mainMenu, ID_PLUGINS_LIST, false);
}

/**
 * @brief Find the scripts submenu from the main menu
 * As now this is the first submenu in "Plugins" menu
 * We find the "Plugins" menu by looking for a menu 
 *  starting with ID_UNPACK_MANUAL.
 */
HMENU CMainFrame::GetPrediffersSubmenu(HMENU mainMenu)
{
	return GetSubmenu(mainMenu, ID_PLUGINS_LIST, true);
}

/**
 * @brief Create a new menu for the view..
 * @param [in] view Menu view either MENU_DEFAULT, MENU_MERGEVIEW or MENU_DIRVIEW.
 * @param [in] ID Menu's resource ID.
 * @return Menu for the view.
 */
HMENU CMainFrame::NewMenu(int view, int ID)
{
	int menu_view;
	if (m_pMenus[view] == nullptr)
	{
		m_pMenus[view].reset(new BCMenu());
		if (m_pMenus[view] == nullptr)
			return nullptr;
	}

	switch (view)
	{
	case MENU_MERGEVIEW:
		menu_view = MENU_FILECMP;
		break;
	case MENU_DIRVIEW:
		menu_view = MENU_FOLDERCMP;
		break;
	case MENU_DEFAULT:
	default:
		menu_view = MENU_MAINFRM;
		break;
	};

	if (!m_pMenus[view]->LoadMenu(ID))
	{
		ASSERT(false);
		return nullptr;
	}

	if (view == MENU_IMGMERGEVIEW)
	{
		m_pImageMenu.reset(new BCMenu);
		m_pImageMenu->LoadMenu(MAKEINTRESOURCE(IDR_POPUP_IMGMERGEVIEW));
		m_pMenus[view]->InsertMenu(4, MF_BYPOSITION | MF_POPUP, (UINT_PTR)m_pImageMenu->GetSubMenu(0)->m_hMenu, const_cast<TCHAR *>(LoadResString(IDS_IMAGE_MENU).c_str())); 
	}

	// Load bitmaps to menuitems
	for (auto& menu_icon: m_MenuIcons)
	{
		if (menu_view == (menu_icon.menusToApply & menu_view))
		{
			m_pMenus[view]->ModifyODMenu(nullptr, menu_icon.menuitemID, menu_icon.iconResID);
		}
	}

	m_pMenus[view]->LoadToolbar(IDR_MAINFRAME);

	theApp.TranslateMenu(m_pMenus[view]->m_hMenu);

	return (m_pMenus[view]->Detach());

}
/** 
* @brief Create new default (CMainFrame) menu.
*/
HMENU CMainFrame::NewDefaultMenu(int ID /*=0*/)
{
	if (ID == 0)
		ID = IDR_MAINFRAME;
	return NewMenu( MENU_DEFAULT, ID );
}

/**
 * @brief Create new File compare (CMergeEditView) menu.
 */
HMENU CMainFrame::NewMergeViewMenu()
{
	return NewMenu( MENU_MERGEVIEW, IDR_MERGEDOCTYPE);
}

/**
 * @brief Create new Dir compare (CDirView) menu
 */
HMENU CMainFrame::NewDirViewMenu()
{
	return NewMenu(MENU_DIRVIEW, IDR_DIRDOCTYPE );
}

/**
 * @brief Create new File compare (CHexMergeView) menu.
 */
HMENU CMainFrame::NewHexMergeViewMenu()
{
	return NewMenu( MENU_HEXMERGEVIEW, IDR_MERGEDOCTYPE);
}

/**
 * @brief Create new Image compare (CImgMergeView) menu.
 */
HMENU CMainFrame::NewImgMergeViewMenu()
{
	return NewMenu( MENU_IMGMERGEVIEW, IDR_MERGEDOCTYPE);
}

/**
 * @brief Create new File compare (COpenView) menu.
 */
HMENU CMainFrame::NewOpenViewMenu()
{
	return NewMenu( MENU_OPENVIEW, IDR_MAINFRAME);
}

/**
 * @brief This handler ensures that the popup menu items are drawn correctly.
 */
void CMainFrame::OnMeasureItem(int nIDCtl,
	LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
	bool setflag = false;
	if (lpMeasureItemStruct->CtlType == ODT_MENU)
	{
		if (IsMenu(reinterpret_cast<HMENU>(static_cast<uintptr_t>(lpMeasureItemStruct->itemID))))
		{
			CMenu* cmenu =
				CMenu::FromHandle(reinterpret_cast<HMENU>(static_cast<uintptr_t>(lpMeasureItemStruct->itemID)));

			if (m_pMenus[MENU_DEFAULT]->IsMenu(cmenu))
			{
				m_pMenus[MENU_DEFAULT]->MeasureItem(lpMeasureItemStruct);
				setflag = true;
			}
		}
	}

	if (!setflag)
		__super::OnMeasureItem(nIDCtl, lpMeasureItemStruct);
}

/**
 * @brief This handler ensures that keyboard shortcuts work.
 */
LRESULT CMainFrame::OnMenuChar(UINT nChar, UINT nFlags, 
	CMenu* pMenu) 
{
	LRESULT lresult;
	if(m_pMenus[MENU_DEFAULT]->IsMenu(pMenu))
		lresult=BCMenu::FindKeyboardShortcut(nChar, nFlags, pMenu);
	else
		lresult=__super::OnMenuChar(nChar, nFlags, pMenu);
	return lresult;
}

/**
 * @brief This handler updates the menus from time to time.
 */
void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu) 
{
	__super::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);
	
	if (!bSysMenu)
	{
		if (BCMenu::IsMenu(pPopupMenu))
		{
			BCMenu::UpdateMenu(pPopupMenu);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

void CMainFrame::OnFileOpen() 
{
	DoFileOpen();
}

/**
 * @brief Check for BOM, and also, if bGuessEncoding, try to deduce codepage
 *
 * Unpacks info from FileLocation & delegates all work to codepage_detect module
 */
static void
FileLocationGuessEncodings(FileLocation & fileloc, int iGuessEncoding)
{
	fileloc.encoding = GuessCodepageEncoding(fileloc.filepath, iGuessEncoding);
}

bool CMainFrame::ShowAutoMergeDoc(CDirDoc * pDirDoc,
	int nFiles, const FileLocation ifileloc[],
	const DWORD dwFlags[], const String strDesc[], const String& sReportFile /*= _T("")*/,
	const PackingInfo * infoUnpacker /*= nullptr*/)
{
	if (sReportFile.empty() && pDirDoc->CompareFilesIfFilesAreLarge(nFiles, ifileloc))
		return false;

	FileFilterHelper filterImg, filterBin;
	filterImg.UseMask(true);
	filterImg.SetMask(GetOptionsMgr()->GetString(OPT_CMP_IMG_FILEPATTERNS));
	filterBin.UseMask(true);
	filterBin.SetMask(GetOptionsMgr()->GetString(OPT_CMP_BIN_FILEPATTERNS));
	for (int pane = 0; pane < nFiles; ++pane)
	{
		if (filterImg.includeFile(ifileloc[pane].filepath) && CImgMergeFrame::IsLoadable())
			return ShowImgMergeDoc(pDirDoc, nFiles, ifileloc, dwFlags, strDesc, sReportFile, infoUnpacker);
		else if (filterBin.includeFile(ifileloc[pane].filepath) && CHexMergeView::IsLoadable())
			return ShowHexMergeDoc(pDirDoc, nFiles, ifileloc, dwFlags, strDesc, sReportFile, infoUnpacker);
	}
	return ShowMergeDoc(pDirDoc, nFiles, ifileloc, dwFlags, strDesc, sReportFile, infoUnpacker);
}

std::array<bool, 3> GetROFromFlags(int nFiles, const DWORD dwFlags[])
{
	std::array<bool, 3> bRO = { false, false, false };
	for (int pane = 0; pane < nFiles; pane++)
	{
		if (dwFlags)
			bRO[pane] = ((dwFlags[pane] & FFILEOPEN_READONLY) > 0);
	}
	return bRO;
}

int GetActivePaneFromFlags(int nFiles, const DWORD dwFlags[])
{
	int nActivePane = -1;
	for (int pane = 0; pane < nFiles; ++pane)
	{
		if (dwFlags && (dwFlags[pane] & FFILEOPEN_SETFOCUS))
			nActivePane = pane;
	}
	return nActivePane;
}

/**
 * @brief Creates new MergeDoc instance and shows documents.
 * @param [in] pDirDoc Dir compare document to create a new Merge document for.
 * @param [in] ifilelocLeft Left side file location info.
 * @param [in] ifilelocRight Right side file location info.
 * @param [in] dwLeftFlags Left side flags.
 * @param [in] dwRightFlags Right side flags.
 * @param [in] infoUnpacker Plugin info.
 * @return success/failure
 */
bool CMainFrame::ShowMergeDoc(CDirDoc * pDirDoc,
	int nFiles, const FileLocation ifileloc[],
	const DWORD dwFlags[], const String strDesc[], const String& sReportFile /*= _T("")*/,
	const PackingInfo * infoUnpacker /*= nullptr*/)
{
	if (m_pMenus[MENU_MERGEVIEW] == nullptr)
		theApp.m_pDiffTemplate->m_hMenuShared = NewMergeViewMenu();
	CMergeDoc * pMergeDoc = GetMergeDocForDiff<CMergeDoc>(theApp.m_pDiffTemplate, pDirDoc, nFiles, false);

	// Make local copies, so we can change encoding if we guess it below
	FileLocation fileloc[3];
	std::copy_n(ifileloc, nFiles, fileloc);

	ASSERT(pMergeDoc != nullptr);		// must ASSERT to get an answer to the question below ;-)
	if (pMergeDoc == nullptr)
		return false; // when does this happen ?

	// if an unpacker is selected, it must be used during LoadFromFile
	// MergeDoc must memorize it for SaveToFile
	// Warning : this unpacker may differ from the pDirDoc one
	// (through menu : "Plugins"->"Open with unpacker")
	pMergeDoc->SetUnpacker(infoUnpacker);

	// detect codepage
	int iGuessEncodingType = GetOptionsMgr()->GetInt(OPT_CP_DETECT);
	for (int pane = 0; pane < nFiles; pane++)
	{
		if (fileloc[pane].encoding.m_unicoding == -1)
			fileloc[pane].encoding.m_unicoding = ucr::NONE;
		if (fileloc[pane].encoding.m_unicoding == ucr::NONE && fileloc[pane].encoding.m_codepage == -1)
		{
			FileLocationGuessEncodings(fileloc[pane], iGuessEncodingType);
		}
	}

	// Note that OpenDocs() takes care of closing compare window when needed.
	bool bResult = pMergeDoc->OpenDocs(nFiles, fileloc, GetROFromFlags(nFiles, dwFlags).data(), strDesc);
	if (bResult)
	{
		if (CMergeEditFrame *pFrame = pMergeDoc->GetParentFrame())
			if (!pFrame->IsActivated())
				pFrame->InitialUpdateFrame(pMergeDoc, true);
	}
	else
	{
		return false;
	}

	for (int pane = 0; pane < nFiles; pane++)
	{
		if (dwFlags)
		{
			bool bModified = (dwFlags[pane] & FFILEOPEN_MODIFIED) > 0;
			if (bModified)
			{
				pMergeDoc->m_ptBuf[pane]->SetModified(true);
				pMergeDoc->UpdateHeaderPath(pane);
			}
			if (dwFlags[pane] & FFILEOPEN_AUTOMERGE)
			{
				pMergeDoc->DoAutoMerge(pane);
			}
		}
	}

	pMergeDoc->MoveOnLoad(GetActivePaneFromFlags(nFiles, dwFlags));

	if (!sReportFile.empty())
		pMergeDoc->GenerateReport(sReportFile);

	return true;
}

bool CMainFrame::ShowHexMergeDoc(CDirDoc * pDirDoc, int nFiles, const FileLocation fileloc[],
	const DWORD dwFlags[], const String strDesc[], const String& sReportFile /*= _T("")*/,
	const PackingInfo * infoUnpacker /*= nullptr*/)
{
	if (m_pMenus[MENU_HEXMERGEVIEW] == nullptr)
		theApp.m_pHexMergeTemplate->m_hMenuShared = NewHexMergeViewMenu();
	CHexMergeDoc *pHexMergeDoc = GetMergeDocForDiff<CHexMergeDoc>(theApp.m_pHexMergeTemplate, pDirDoc, nFiles);
	if (pHexMergeDoc == nullptr)
		return false;

	if (!pHexMergeDoc->OpenDocs(nFiles, fileloc, GetROFromFlags(nFiles, dwFlags).data(), strDesc))
		return false;

	pHexMergeDoc->MoveOnLoad(GetActivePaneFromFlags(nFiles, dwFlags));
	
	if (!sReportFile.empty())
		pHexMergeDoc->GenerateReport(sReportFile);

	return true;
}

bool CMainFrame::ShowImgMergeDoc(CDirDoc * pDirDoc, int nFiles, const FileLocation fileloc[],
	const DWORD dwFlags[], const String strDesc[], const String& sReportFile /*= _T("")*/,
	const PackingInfo * infoUnpacker /*= nullptr*/)
{
	CImgMergeFrame *pImgMergeFrame = new CImgMergeFrame();
	if (!CImgMergeFrame::menu.m_hMenu)
		CImgMergeFrame::menu.m_hMenu = NewImgMergeViewMenu();
	pImgMergeFrame->SetSharedMenu(CImgMergeFrame::menu.m_hMenu);

	pImgMergeFrame->SetDirDoc(pDirDoc);
	pDirDoc->AddMergeDoc(pImgMergeFrame);
		
	if (!pImgMergeFrame->OpenDocs(nFiles, fileloc, GetROFromFlags(nFiles, dwFlags).data(), strDesc, this))
		return ShowMergeDoc(pDirDoc, nFiles, fileloc, dwFlags, strDesc, sReportFile, infoUnpacker);

	for (int pane = 0; pane < nFiles; pane++)
	{
		if (dwFlags && (dwFlags[pane] & FFILEOPEN_AUTOMERGE))
			pImgMergeFrame->DoAutoMerge(pane);
	}

	pImgMergeFrame->MoveOnLoad(GetActivePaneFromFlags(nFiles, dwFlags));

	if (!sReportFile.empty())
		pImgMergeFrame->GenerateReport(sReportFile);

	return true;
}

/**
 * @brief Show GNU licence information in notepad (local file) or in Web Browser
 */
void CMainFrame::OnHelpGnulicense() 
{
	const String spath = paths::ConcatPath(env::GetProgPath(), LicenseFile);
	theApp.OpenFileOrUrl(spath.c_str(), LicenceUrl);
}

/**
 * @brief Opens Options-dialog and saves changed options
 */
void CMainFrame::OnOptions() 
{
	// Using singleton shared syntax colors
	CPreferencesDlg dlg(GetOptionsMgr(), theApp.GetMainSyntaxColors());
	INT_PTR rv = dlg.DoModal();

	if (rv == IDOK)
	{
		LANGID lang = static_cast<LANGID>(GetOptionsMgr()->GetInt(OPT_SELECTED_LANGUAGE));
		if (lang != theApp.m_pLangDlg->GetLangId())
		{
			theApp.m_pLangDlg->SetLanguage(lang, true);
	
			// Update status bar inicator texts
			theApp.SetIndicators(m_wndStatusBar, 0, 0);
	
			// Update the current menu
			ReloadMenu();
	
			// update the title text of the document
			UpdateDocTitle();

			UpdateResources();
		}

		// Set new temporary path
		theApp.SetupTempPath();

		// Set new filterpath
		String filterPath = GetOptionsMgr()->GetString(OPT_FILTER_USERPATH);
		theApp.m_pGlobalFileFilter->SetUserFilterPath(filterPath);

		CCrystalTextView::RENDERING_MODE nRenderingMode = static_cast<CCrystalTextView::RENDERING_MODE>(GetOptionsMgr()->GetInt(OPT_RENDERING_MODE));
		CCrystalTextView::SetRenderingModeDefault(nRenderingMode);

		theApp.UpdateCodepageModule();

		strdiff::SetBreakChars(GetOptionsMgr()->GetString(OPT_BREAK_SEPARATORS).c_str());

		// make an attempt at rescanning any open diff sessions
		ApplyDiffOptions();

		// Update all dirdoc settings
		for (auto pDirDoc : GetAllDirDocs())
			pDirDoc->RefreshOptions();
		for (auto pOpenDoc : GetAllOpenDocs())
			pOpenDoc->RefreshOptions();
		for (auto pMergeDoc : GetAllHexMergeDocs())
			pMergeDoc->RefreshOptions();
	}
}

static bool AddToRecentDocs(const PathContext& paths, const unsigned flags[], bool recurse, const String& filter)
{
	String params, title;
	for (int nIndex = 0; nIndex < paths.GetSize(); ++nIndex)
	{
		if (flags && (flags[nIndex] & FFILEOPEN_READONLY))
		{
			switch (nIndex)
			{
			case 0: params += _T("/wl "); break;
			case 1: params += paths.GetSize() == 2 ? _T("/wr ") : _T("/wm "); break;
			case 2:	params += _T("/wr "); break;
			}
		}
		params += _T("\"") + paths[nIndex] + _T("\" ");

		String path = paths[nIndex];
		paths::normalize(path);
		title += paths::FindFileName(path);
		if (nIndex < paths.GetSize() - 1)
			title += _T(" - ");
	}
	if (recurse)
		params += _T("/r ");
	if (!filter.empty())
		params += _T("/f \"") + filter + _T("\" ");

	Concurrent::CreateTask([params, title](){
			CoInitialize(nullptr);
			JumpList::AddToRecentDocs(_T(""), params, title, params, 0);
			CoUninitialize();
			return 0;
		});
	return true;
}
/**
 * @brief Begin a diff: open dirdoc if it is directories, else open a mergedoc for editing.
 * @param [in] pszLeft Left-side path.
 * @param [in] pszRight Right-side path.
 * @param [in] dwLeftFlags Left-side flags.
 * @param [in] dwRightFlags Right-side flags.
 * @param [in] bRecurse Do we run recursive (folder) compare?
 * @param [in] pDirDoc Dir compare document to use.
 * @param [in] prediffer Prediffer plugin name.
 * @return `true` if opening files and compare succeeded, `false` otherwise.
 */
bool CMainFrame::DoFileOpen(const PathContext * pFiles /*= nullptr*/,
	const DWORD dwFlags[] /*= nullptr*/, const String strDesc[] /*= nullptr*/, const String& sReportFile /*= T("")*/, bool bRecurse /*= false*/, CDirDoc *pDirDoc/*= nullptr*/,
	String prediffer /*= _T("")*/, const PackingInfo *infoUnpacker /*= nullptr*/)
{
	if (pDirDoc != nullptr && !pDirDoc->CloseMergeDocs())
		return false;

	FileTransform::g_UnpackerMode = static_cast<PLUGIN_MODE>(GetOptionsMgr()->GetInt(OPT_PLUGINS_UNPACKER_MODE));
	FileTransform::g_PredifferMode = static_cast<PLUGIN_MODE>(GetOptionsMgr()->GetInt(OPT_PLUGINS_PREDIFFER_MODE));

	Merge7zFormatMergePluginScope scope(infoUnpacker);

	PathContext tFiles;
	if (pFiles != nullptr)
		tFiles = *pFiles;
	bool bRO[3] = {0};
	if (dwFlags)
	{
		bRO[0] = (dwFlags[0] & FFILEOPEN_READONLY) != 0;
		bRO[1] = (dwFlags[1] & FFILEOPEN_READONLY) != 0;
		bRO[2] = (dwFlags[2] & FFILEOPEN_READONLY) != 0;
	};

	// pop up dialog unless arguments exist (and are compatible)
	paths::PATH_EXISTENCE pathsType = paths::GetPairComparability(tFiles, IsArchiveFile);
	if (pathsType == paths::DOES_NOT_EXIST)
	{
		if (m_pMenus[MENU_OPENVIEW] == nullptr)
			theApp.m_pOpenTemplate->m_hMenuShared = NewOpenViewMenu();
		COpenDoc *pOpenDoc = static_cast<COpenDoc *>(theApp.m_pOpenTemplate->CreateNewDocument());
		if (dwFlags)
		{
			pOpenDoc->m_dwFlags[0] = dwFlags[0];
			pOpenDoc->m_dwFlags[1] = dwFlags[1];
			pOpenDoc->m_dwFlags[2] = dwFlags[2];
		}
		pOpenDoc->m_files = tFiles;
		pOpenDoc->m_bRecurse = bRecurse;
		if (infoUnpacker)
			pOpenDoc->m_infoHandler = *infoUnpacker;
		CFrameWnd *pFrame = theApp.m_pOpenTemplate->CreateNewFrame(pOpenDoc, nullptr);
		theApp.m_pOpenTemplate->InitialUpdateFrame(pFrame, pOpenDoc);
		return true;
	}
	else
	{
		// Add trailing '\' for directories if its missing
		if (pathsType == paths::IS_EXISTING_DIR)
		{
			if (!paths::EndsWithSlash(tFiles[0]) && !IsArchiveFile(tFiles[0]))
				tFiles[0] = paths::AddTrailingSlash(tFiles[0]);
			if (!paths::EndsWithSlash(tFiles[1]) && !IsArchiveFile(tFiles[1]))
				tFiles[1] = paths::AddTrailingSlash(tFiles[1]);
			if (tFiles.GetSize() == 3 && !paths::EndsWithSlash(tFiles[2]) && !IsArchiveFile(tFiles[1]))
				tFiles[2] = paths::AddTrailingSlash(tFiles[2]);
		}

		//save the MRU left and right files.
		if (dwFlags)
		{
			if (!(dwFlags[0] & FFILEOPEN_NOMRU))
				addToMru(tFiles[0].c_str(), _T("Files\\Left"));
			if (!(dwFlags[1] & FFILEOPEN_NOMRU))
				addToMru(tFiles[1].c_str(), _T("Files\\Right"));
			if (tFiles.GetSize() == 3 && !(dwFlags[2] & FFILEOPEN_NOMRU))
				addToMru(tFiles[2].c_str(), _T("Files\\Option"));
		}
	}

	CTempPathContext *pTempPathContext = nullptr;
	if (pathsType == paths::IS_EXISTING_DIR)
	{
		DecompressResult res= DecompressArchive(m_hWnd, tFiles);
		if (res.pTempPathContext)
		{
			pathsType = res.pathsType;
			tFiles = res.files;
			pTempPathContext = res.pTempPathContext;
		}
	}

	// Determine if we want a new dirview open, now that we know if it was
	// an archive. Don't open a new dirview if we are comparing files.
	if (pDirDoc == nullptr)
	{
		if (pathsType == paths::IS_EXISTING_DIR)
		{
			CDirDoc::m_nDirsTemp = tFiles.GetSize();
			if (m_pMenus[MENU_DIRVIEW] == nullptr)
				theApp.m_pDirTemplate->m_hMenuShared = NewDirViewMenu();
			pDirDoc = static_cast<CDirDoc*>(theApp.m_pDirTemplate->OpenDocumentFile(nullptr));
		}
		else
		{
			pDirDoc = static_cast<CDirDoc*>(theApp.m_pDirTemplate->CreateNewDocument());
		}
	}

	// open the diff
	if (pathsType == paths::IS_EXISTING_DIR)
	{
		if (pDirDoc != nullptr)
		{
			// Anything that can go wrong inside InitCompare() will yield an
			// exception. There is no point in checking return value.
			pDirDoc->InitCompare(tFiles, bRecurse, pTempPathContext);

			pDirDoc->SetReportFile(sReportFile);
			pDirDoc->SetDescriptions(strDesc);
			pDirDoc->SetTitle(nullptr);
			for (int nIndex = 0; nIndex < tFiles.GetSize(); nIndex++)
				pDirDoc->SetReadOnly(nIndex, bRO[nIndex]);

			pDirDoc->Rescan();
		}
	}
	else
	{		
		FileLocation fileloc[3];

		for (int nPane = 0; nPane < tFiles.GetSize(); nPane++)
			fileloc[nPane].setPath(tFiles[nPane]);

		if (!prediffer.empty())
		{
			String strBothFilenames = strutils::join(tFiles.begin(), tFiles.end(), _T("|"));
			pDirDoc->GetPluginManager().SetPrediffer(strBothFilenames, prediffer);
		}

		ShowAutoMergeDoc(pDirDoc, tFiles.GetSize(), fileloc, dwFlags, strDesc, sReportFile,
				infoUnpacker);
	}

	if (pFiles != nullptr && (!dwFlags || !(dwFlags[0] & FFILEOPEN_NOMRU)))
	{
		String filter = GetOptionsMgr()->GetString(OPT_FILEFILTER_CURRENT);
		AddToRecentDocs(*pFiles, (unsigned *)dwFlags, bRecurse, filter);
	}

	return true;
}

void CMainFrame::UpdateFont(FRAMETYPE frame)
{
	if (frame == FRAME_FOLDER)
	{
		for (auto pDoc : GetAllDirDocs())
		{
			if (pDoc != nullptr)
			{
				CDirView *pView = pDoc->GetMainView();
				if (pView != nullptr)
					pView->SetFont(m_lfDir);
			}
		}
	}
	else
	{
		for (auto pDoc : GetAllMergeDocs())
		{
			CMergeDoc *pMergeDoc = dynamic_cast<CMergeDoc *>(pDoc);
			if (pMergeDoc != nullptr)
				for (auto& pView: pMergeDoc->GetViewList())
					pView->SetFont(m_lfDiff);
		}
	}
}

/**
 * @brief Select font for Merge/Dir view
 * 
 * Shows font selection dialog to user, sets current font and saves
 * selected font properties to registry. Selects fon type to active
 * view (Merge/Dir compare). If there is no open views, then font
 * is selected for Merge view (for example user may want to change to
 * unicode font before comparing files).
 */
void CMainFrame::OnViewSelectfont() 
{
	FRAMETYPE frame = GetFrameType(GetActiveFrame());
	CHOOSEFONT cf = { sizeof CHOOSEFONT };
	LOGFONT *lf = nullptr;
	cf.Flags = CF_INITTOLOGFONTSTRUCT|CF_FORCEFONTEXIST|CF_SCREENFONTS;
	if (frame == FRAME_FILE)
		cf.Flags |= CF_FIXEDPITCHONLY; // Only fixed-width fonts for merge view

	// CF_FIXEDPITCHONLY = 0x00004000L
	// in case you are a developer and want to disable it to test with, eg, a Chinese capable font
	if (frame == FRAME_FOLDER)
		lf = &m_lfDir;
	else
		lf = &m_lfDiff;

	cf.lpLogFont = lf;

	if (ChooseFont(&cf))
	{
		Options::Font::Save(GetOptionsMgr(), frame == FRAME_FOLDER ? OPT_FONT_DIRCMP : OPT_FONT_FILECMP, lf, true);
		UpdateFont(frame);
	}
}

/**
 * @brief Use default font for active view type
 *
 * Disable user-selected font for active view type (Merge/Dir compare).
 * If there is no open views, then Merge view font is changed.
 */
void CMainFrame::OnViewUsedefaultfont() 
{
	FRAMETYPE frame = GetFrameType(GetActiveFrame());

	if (frame == FRAME_FOLDER)
	{
		Options::Font::Reset(GetOptionsMgr(), OPT_FONT_DIRCMP);
		m_lfDir = Options::Font::Load(GetOptionsMgr(), OPT_FONT_DIRCMP);
		Options::Font::Save(GetOptionsMgr(), OPT_FONT_DIRCMP, &m_lfDir, false);
	}
	else
	{
		Options::Font::Reset(GetOptionsMgr(), OPT_FONT_FILECMP);
		m_lfDiff = Options::Font::Load(GetOptionsMgr(), OPT_FONT_FILECMP);
		Options::Font::Save(GetOptionsMgr(), OPT_FONT_FILECMP, &m_lfDiff, false);
	}

	UpdateFont(frame);
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CMainFrame::UpdateResources()
{
	m_wndStatusBar.SetPaneText(0, theApp.LoadString(AFX_IDS_IDLEMESSAGE).c_str());

	for (auto pDoc : GetAllDirDocs())
		pDoc->UpdateResources();
	for (auto pDoc : GetAllMergeDocs())
		pDoc->UpdateResources();
	for (auto pDoc : GetAllOpenDocs())
		pDoc->UpdateResources();
	for (auto pFrame: GetAllImgMergeFrames())
		pFrame->UpdateResources();
}

/**
 * @brief Open WinMerge help.
 *
 * If local HTMLhelp file is found, open it, otherwise open HTML page from web.
 */
void CMainFrame::OnHelpContents()
{
	theApp.ShowHelp();
}

/**
 * @brief Handle translation of default messages on the status bar
 */
void CMainFrame::GetMessageString(UINT nID, CString& rMessage) const
{
	// load appropriate string
	const String s = theApp.LoadString(nID);
	if (s.length() > 0)
		AfxExtractSubString(rMessage, s.c_str(), 0);
}

void CMainFrame::ActivateFrame(int nCmdShow) 
{
	if (!m_bFirstTime)
	{
		__super::ActivateFrame(nCmdShow);
		return;
	}

	m_bFirstTime = false;

	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(&wp);
	wp.rcNormalPosition.left=theApp.GetProfileInt(_T("Settings"), _T("MainLeft"),0);
	wp.rcNormalPosition.top=theApp.GetProfileInt(_T("Settings"), _T("MainTop"),0);
	wp.rcNormalPosition.right=theApp.GetProfileInt(_T("Settings"), _T("MainRight"),0);
	wp.rcNormalPosition.bottom=theApp.GetProfileInt(_T("Settings"), _T("MainBottom"),0);
	if (nCmdShow != SW_MINIMIZE && theApp.GetProfileInt(_T("Settings"), _T("MainMax"), FALSE))
		wp.showCmd = SW_MAXIMIZE;
	else
		wp.showCmd = nCmdShow;

	CRect dsk_rc,rc(wp.rcNormalPosition);

	dsk_rc.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	dsk_rc.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	dsk_rc.right = dsk_rc.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
	dsk_rc.bottom = dsk_rc.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
	if (rc.Width() != 0 && rc.Height() != 0)
	{
		// Ensure top-left corner is on visible area,
		// 20 points margin is added to prevent "lost" window
		CPoint ptTopLeft(rc.TopLeft());
		ptTopLeft += CPoint(20, 20);

		if (dsk_rc.PtInRect(ptTopLeft))
			SetWindowPlacement(&wp);
		else
			__super::ActivateFrame(nCmdShow);
	}
	else
		__super::ActivateFrame(nCmdShow);
}

/**
 * @brief Called when mainframe is about to be closed.
 * This function is called when mainframe is to be closed (not for
 * file/compare windows.
 */
void CMainFrame::OnClose()
{
	if (theApp.GetActiveOperations())
		return;

	// Check if there are multiple windows open and ask for closing them
	bool bAskClosing = GetOptionsMgr()->GetBool(OPT_ASK_MULTIWINDOW_CLOSE);
	if (bAskClosing)
	{
		bool quit = AskCloseConfirmation();
		if (!quit)
			return;
	}

	// Save last selected filter
	String filter = theApp.m_pGlobalFileFilter->GetFilterNameOrMask();
	GetOptionsMgr()->SaveOption(OPT_FILEFILTER_CURRENT, filter);

	// save main window position
	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(&wp);
	theApp.WriteProfileInt(_T("Settings"), _T("MainLeft"),wp.rcNormalPosition.left);
	theApp.WriteProfileInt(_T("Settings"), _T("MainTop"),wp.rcNormalPosition.top);
	theApp.WriteProfileInt(_T("Settings"), _T("MainRight"),wp.rcNormalPosition.right);
	theApp.WriteProfileInt(_T("Settings"), _T("MainBottom"),wp.rcNormalPosition.bottom);
	theApp.WriteProfileInt(_T("Settings"), _T("MainMax"), (wp.showCmd == SW_MAXIMIZE));

	for (auto pFrame: GetAllImgMergeFrames())
	{
		if (!pFrame->CloseNow())
			return;
	}

	__super::OnClose();
}

/**
 * @brief Utility function to update CSuperComboBox format MRU
 */
void CMainFrame::addToMru(LPCTSTR szItem, LPCTSTR szRegSubKey, UINT nMaxItems)
{
	std::vector<CString> list;
	CString s;
	UINT cnt = AfxGetApp()->GetProfileInt(szRegSubKey, _T("Count"), 0);
	list.push_back(szItem);
	for (UINT i=0 ; i<cnt; ++i)
	{
		s = AfxGetApp()->GetProfileString(szRegSubKey, strutils::format(_T("Item_%d"), i).c_str());
		if (s != szItem)
			list.push_back(s);
	}
	cnt = list.size() > nMaxItems ? nMaxItems : static_cast<UINT>(list.size());
	for (UINT i=0 ; i<cnt; ++i)
		AfxGetApp()->WriteProfileString(szRegSubKey, strutils::format(_T("Item_%d"), i).c_str(), list[i]);
	// update count
	AfxGetApp()->WriteProfileInt(szRegSubKey, _T("Count"), cnt);
}

void CMainFrame::ApplyDiffOptions() 
{
	for (auto pMergeDoc : GetAllMergeDocs())
	{
		// Re-read MergeDoc settings (also updates view settings)
		// and rescan using new options
		pMergeDoc->RefreshOptions();
		pMergeDoc->FlushAndRescan(true);
	}
}

/// Get list of OpenDocs (documents underlying edit sessions)
OpenDocList &CMainFrame::GetAllOpenDocs()
{
	return static_cast<OpenDocList &>(GetDocList(theApp.m_pOpenTemplate));
}

/// Get list of MergeDocs (documents underlying edit sessions)
MergeDocList &CMainFrame::GetAllMergeDocs()
{
	return static_cast<MergeDocList &>(GetDocList(theApp.m_pDiffTemplate));
}

/// Get list of DirDocs (documents underlying a scan)
DirDocList &CMainFrame::GetAllDirDocs()
{
	return static_cast<DirDocList &>(GetDocList(theApp.m_pDirTemplate));
}

/// Get list of HexMergeDocs (documents underlying edit sessions)
HexMergeDocList &CMainFrame::GetAllHexMergeDocs()
{
	return static_cast<HexMergeDocList &>(GetDocList(theApp.m_pHexMergeTemplate));
}

std::list<CImgMergeFrame *> CMainFrame::GetAllImgMergeFrames()
{
	std::list<CImgMergeFrame *> list;
	// Close Non-Document/View frame with confirmation
	CMDIChildWnd *pChild = static_cast<CMDIChildWnd *>(CWnd::FromHandle(m_hWndMDIClient)->GetWindow(GW_CHILD));
	while (pChild != nullptr)
	{
		CMDIChildWnd *pNextChild = static_cast<CMDIChildWnd *>(pChild->GetWindow(GW_HWNDNEXT));
		if (GetFrameType(pChild) == FRAME_IMGFILE)
			list.push_back(static_cast<CImgMergeFrame *>(pChild));
		pChild = pNextChild;
	}
	return list;
}

/**
 * @brief Obtain a merge doc to display a difference in files.
 * @return Pointer to CMergeDoc to use. 
 */
template<class DocClass>
DocClass * GetMergeDocForDiff(CMultiDocTemplate *pTemplate, CDirDoc *pDirDoc, int nFiles, bool bMakeVisible)
{
	// Create a new merge doc
	DocClass::m_nBuffersTemp = nFiles;
	DocClass *pMergeDoc = static_cast<DocClass*>(pTemplate->OpenDocumentFile(nullptr, bMakeVisible));
	if (pMergeDoc != nullptr)
	{
		pDirDoc->AddMergeDoc(pMergeDoc);
		pMergeDoc->SetDirDoc(pDirDoc);
	}
	return pMergeDoc;
}

// Clear the item count in the main status pane
void CMainFrame::ClearStatusbarItemCount()
{
	m_wndStatusBar.SetPaneText(2, _T(""));
}

/**
 * @brief Generate patch from files selected.
 *
 * Creates a patch from selected files in active directory compare, or
 * active file compare. Files in file compare must be saved before
 * creating a patch.
 */
void CMainFrame::OnToolsGeneratePatch()
{
	CPatchTool patcher;
	patcher.CreatePatch();
}

void CMainFrame::OnDropFiles(const std::vector<String>& dropped_files)
{
	PathContext tFiles(dropped_files);
	const size_t fileCount = tFiles.GetSize();

	bool recurse = GetOptionsMgr()->GetBool(OPT_CMP_INCLUDE_SUBDIRS);
	// Do a reverse comparison with the current 'Include Subfolders' settings when pressing Control key
	if (!!::GetAsyncKeyState(VK_CONTROL))
		recurse = !recurse;

	// If user has <Shift> pressed with one file selected,
	// assume it is an archive and set filenames to same
	if (::GetAsyncKeyState(VK_SHIFT) < 0 && fileCount == 1)
	{
		tFiles.SetRight(tFiles[0]);
	}

	// Check if they dropped a project file
	DWORD dwFlags[3] = {FFILEOPEN_NONE, FFILEOPEN_NONE, FFILEOPEN_NONE};
	if (fileCount == 1)
	{
		if (theApp.IsProjectFile(tFiles[0]))
		{
			theApp.LoadAndOpenProjectFile(tFiles[0]);
			return;
		}
		if (IsConflictFile(tFiles[0]))
		{
			DoOpenConflict(tFiles[0], nullptr, true);
			return;
		}
	}

	DoFileOpen(&tFiles, dwFlags, nullptr, _T(""), recurse);
}

void CMainFrame::OnPluginUnpackMode(UINT nID )
{
	switch (nID)
	{
	case ID_UNPACK_MANUAL:
		FileTransform::g_UnpackerMode = PLUGIN_MANUAL;
		break;
	case ID_UNPACK_AUTO:
		FileTransform::g_UnpackerMode = PLUGIN_AUTO;
		break;
	}
	GetOptionsMgr()->SaveOption(OPT_PLUGINS_UNPACKER_MODE, FileTransform::g_UnpackerMode);
}

void CMainFrame::OnUpdatePluginUnpackMode(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetOptionsMgr()->GetBool(OPT_PLUGINS_ENABLED));

	if (pCmdUI->m_nID == ID_UNPACK_MANUAL)
		pCmdUI->SetRadio(PLUGIN_MANUAL == FileTransform::g_UnpackerMode);
	if (pCmdUI->m_nID == ID_UNPACK_AUTO)
		pCmdUI->SetRadio(PLUGIN_AUTO == FileTransform::g_UnpackerMode);
}
void CMainFrame::OnPluginPrediffMode(UINT nID )
{
	switch (nID)
	{
	case ID_PREDIFFER_MANUAL:
		FileTransform::g_PredifferMode = PLUGIN_MANUAL;
		break;
	case ID_PREDIFFER_AUTO:
		FileTransform::g_PredifferMode = PLUGIN_AUTO;
		break;
	}
	PrediffingInfo infoPrediffer;
	for (auto pMergeDoc : GetAllMergeDocs())
		pMergeDoc->SetPrediffer(&infoPrediffer);
	for (auto pDirDoc : GetAllDirDocs())
		pDirDoc->GetPluginManager().SetPrediffSettingAll(FileTransform::g_PredifferMode);
	GetOptionsMgr()->SaveOption(OPT_PLUGINS_PREDIFFER_MODE, FileTransform::g_PredifferMode);
}

void CMainFrame::OnUpdatePluginPrediffMode(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetOptionsMgr()->GetBool(OPT_PLUGINS_ENABLED));

	if (pCmdUI->m_nID == ID_PREDIFFER_MANUAL)
		pCmdUI->SetRadio(PLUGIN_MANUAL == FileTransform::g_PredifferMode);
	if (pCmdUI->m_nID == ID_PREDIFFER_AUTO)
		pCmdUI->SetRadio(PLUGIN_AUTO == FileTransform::g_PredifferMode);
}
/**
 * @brief Called when "Reload Plugins" item is updated
 */
void CMainFrame::OnUpdateReloadPlugins(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetOptionsMgr()->GetBool(OPT_PLUGINS_ENABLED));
}

void CMainFrame::OnReloadPlugins()
{
	// delete all script interfaces
	// (interfaces will be created again automatically when WinMerge needs them)
	CAllThreadsScripts::GetActiveSet()->FreeAllScripts();

	// update the editor scripts submenu
	HMENU scriptsSubmenu = GetScriptsSubmenu(m_hMenuDefault);
	if (scriptsSubmenu != nullptr)
		CMergeEditView::createScriptsSubmenu(scriptsSubmenu);
	UpdatePrediffersMenu();
}

/** @brief Return active merge edit view, if can figure it out/is available */
CMergeEditView * CMainFrame::GetActiveMergeEditView()
{
	// NB: GetActiveDocument does not return the Merge Doc 
	//     even when the merge edit view is in front
	// NB: CMergeEditFrame::GetActiveView returns `nullptr` when location view active
	// So we have this rather complicated logic to try to get a merge edit view
	// We look at the front child window, which should be a frame
	// and we can get a MergeEditView from it, if it is a CMergeEditFrame
	// (DirViews use a different frame type)
	CMergeEditFrame * pFrame = dynamic_cast<CMergeEditFrame *>(GetActiveFrame());
	if (pFrame == nullptr) return nullptr;
	// Try to get the active MergeEditView (ie, left or right)
	if (pFrame->GetActiveView() != nullptr && pFrame->GetActiveView()->IsKindOf(RUNTIME_CLASS(CMergeEditView)))
	{
		return dynamic_cast<CMergeEditView *>(pFrame->GetActiveView());
	}
	return pFrame->GetMergeDoc()->GetActiveMergeView();
}

void CMainFrame::UpdatePrediffersMenu()
{
	CMenu* menu = GetMenu();
	if (menu == nullptr)
	{
		return;
	}

	HMENU hMainMenu = menu->m_hMenu;
	HMENU prediffersSubmenu = GetPrediffersSubmenu(hMainMenu);
	if (prediffersSubmenu != nullptr)
	{
		CMergeEditView * pEditView = GetActiveMergeEditView();
		if (pEditView != nullptr)
			pEditView->createPrediffersSubmenu(prediffersSubmenu);
		else
		{
			// no view or dir view : display an empty submenu
			int i = GetMenuItemCount(prediffersSubmenu);
			while (i --)
				::DeleteMenu(prediffersSubmenu, 0, MF_BYPOSITION);
			::AppendMenu(prediffersSubmenu, MF_SEPARATOR, 0, nullptr);
		}
	}
}

/**
 * @brief Save WinMerge configuration and info to file
 */
void CMainFrame::OnSaveConfigData()
{
	CConfigLog configLog;
	String sError;

	if (configLog.WriteLogFile(sError))
	{
		String sFileName = configLog.GetFileName();
		theApp.OpenFileToExternalEditor(sFileName);
	}
	else
	{
		String sFileName = configLog.GetFileName();
		String msg = strutils::format_string2(_("Cannot open file\n%1\n\n%2"), sFileName, sError);
		AfxMessageBox(msg.c_str(), MB_OK | MB_ICONSTOP);
	}
}

/**
 * @brief Open two new empty docs, 'Scratchpads'
 * 
 * Allows user to open two empty docs, to paste text to
 * compare from clipboard.
 * @note File filenames are set emptys and filedescriptors
 * are loaded from resource.
 * @sa CMergeDoc::OpenDocs()
 * @sa CMergeDoc::TrySaveAs()
 */
void CMainFrame::FileNew(int nPanes) 
{
	CDirDoc *pDirDoc = static_cast<CDirDoc*>(theApp.m_pDirTemplate->CreateNewDocument());
	
	// Load emptyfile descriptors and open empty docs
	// Use default codepage
	DWORD dwFlags[3] = {0, 0, 0};
	String strDesc[3];
	FileLocation fileloc[3];
	if (nPanes == 2)
	{
		strDesc[0] = _("Untitled left");
		strDesc[1] = _("Untitled right");
		fileloc[0].encoding.SetCodepage(ucr::getDefaultCodepage());
		fileloc[1].encoding.SetCodepage(ucr::getDefaultCodepage());
		ShowMergeDoc(pDirDoc, 2, fileloc, dwFlags, strDesc);
	}
	else
	{
		strDesc[0] = _("Untitled left");
		strDesc[1] = _("Untitled middle");
		strDesc[2] = _("Untitled right");
		fileloc[0].encoding.SetCodepage(ucr::getDefaultCodepage());
		fileloc[1].encoding.SetCodepage(ucr::getDefaultCodepage());
		fileloc[2].encoding.SetCodepage(ucr::getDefaultCodepage());
		ShowMergeDoc(pDirDoc, 3, fileloc, dwFlags, strDesc);
	}
}

/**
 * @brief Open two new empty docs, 'Scratchpads'
 * 
 * Allows user to open two empty docs, to paste text to
 * compare from clipboard.
 * @note File filenames are set emptys and filedescriptors
 * are loaded from resource.
 * @sa CMergeDoc::OpenDocs()
 * @sa CMergeDoc::TrySaveAs()
 */
void CMainFrame::OnFileNew() 
{
	FileNew(2);
}

void CMainFrame::OnFileNew3() 
{
	FileNew(3);
}

/**
 * @brief Open Filters dialog
 */
void CMainFrame::OnToolsFilters()
{
	String title = _("Filters");
	CPropertySheet sht(title.c_str());
	LineFiltersDlg lineFiltersDlg;
	FileFiltersDlg fileFiltersDlg;
	std::unique_ptr<LineFiltersList> lineFilters(new LineFiltersList());
	String selectedFilter;
	const String origFilter = theApp.m_pGlobalFileFilter->GetFilterNameOrMask();
	sht.AddPage(&fileFiltersDlg);
	sht.AddPage(&lineFiltersDlg);
	sht.m_psh.dwFlags |= PSH_NOAPPLYNOW; // Hide 'Apply' button since we don't need it

	// Make sure all filters are up-to-date
	theApp.m_pGlobalFileFilter->ReloadUpdatedFilters();

	fileFiltersDlg.SetFilterArray(theApp.m_pGlobalFileFilter->GetFileFilters(selectedFilter));
	fileFiltersDlg.SetSelected(selectedFilter);
	const bool lineFiltersEnabledOrig = GetOptionsMgr()->GetBool(OPT_LINEFILTER_ENABLED);
	lineFiltersDlg.m_bIgnoreRegExp = lineFiltersEnabledOrig;

	lineFilters->CloneFrom(theApp.m_pLineFilters.get());
	lineFiltersDlg.SetList(lineFilters.get());

	if (sht.DoModal() == IDOK)
	{
		String strNone = _("<None>");
		String path = fileFiltersDlg.GetSelected();
		if (path.find(strNone) != String::npos)
		{
			// Don't overwrite mask we already have
			if (!theApp.m_pGlobalFileFilter->IsUsingMask())
			{
				String sFilter(_T("*.*"));
				theApp.m_pGlobalFileFilter->SetFilter(sFilter);
				GetOptionsMgr()->SaveOption(OPT_FILEFILTER_CURRENT, sFilter);
			}
		}
		else
		{
			theApp.m_pGlobalFileFilter->SetFileFilterPath(path);
			theApp.m_pGlobalFileFilter->UseMask(false);
			String sFilter = theApp.m_pGlobalFileFilter->GetFilterNameOrMask();
			GetOptionsMgr()->SaveOption(OPT_FILEFILTER_CURRENT, sFilter);
		}
		bool linefiltersEnabled = lineFiltersDlg.m_bIgnoreRegExp;
		GetOptionsMgr()->SaveOption(OPT_LINEFILTER_ENABLED, linefiltersEnabled);

		// Check if compare documents need rescanning
		bool bFileCompareRescan = false;
		bool bFolderCompareRescan = false;
		CFrameWnd * pFrame = GetActiveFrame();
		FRAMETYPE frame = GetFrameType(pFrame);
		if (frame == FRAME_FILE)
		{
			if (lineFiltersEnabledOrig != linefiltersEnabled ||
					!theApp.m_pLineFilters->Compare(lineFilters.get()))
			{
				bFileCompareRescan = true;
			}
		}
		else if (frame == FRAME_FOLDER)
		{
			const String newFilter = theApp.m_pGlobalFileFilter->GetFilterNameOrMask();
			if (lineFiltersEnabledOrig != linefiltersEnabled || 
					!theApp.m_pLineFilters->Compare(lineFilters.get()) || origFilter != newFilter)
			{
				int res = LangMessageBox(IDS_FILTERCHANGED, MB_ICONWARNING | MB_YESNO);
				if (res == IDYES)
					bFolderCompareRescan = true;
			}
		}

		// Save new filters before (possibly) rescanning
		theApp.m_pLineFilters->CloneFrom(lineFilters.get());
		theApp.m_pLineFilters->SaveFilters();

		if (bFileCompareRescan)
		{
			for (auto pMergeDoc : GetAllMergeDocs())
				pMergeDoc->FlushAndRescan(true);
		}
		else if (bFolderCompareRescan)
		{
			for (auto pDirDoc : GetAllDirDocs())
				pDirDoc->Rescan();
		}
	}
}

/**
 * @brief Open Filters dialog.
 */
void CMainFrame::SelectFilter()
{
	OnToolsFilters();
}

/**
 * @brief Closes application with ESC.
 *
 * Application is closed if:
 * - 'Close Windows with ESC' option is enabled and
 *    there is no open document windows
 * - '-e' commandline switch is given
 */
BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	// Check if we got 'ESC pressed' -message
	if ((pMsg->message == WM_KEYDOWN) && (pMsg->wParam == VK_ESCAPE))
	{
		if (theApp.m_bEscShutdown && m_wndTabBar.GetItemCount() <= 1)
		{
			AfxGetMainWnd()->SendMessage(WM_COMMAND, ID_APP_EXIT);
			return TRUE;
		}
		else if (GetOptionsMgr()->GetInt(OPT_CLOSE_WITH_ESC) == 1 && m_wndTabBar.GetItemCount() == 0)
		{
			AfxGetMainWnd()->PostMessage(WM_COMMAND, ID_APP_EXIT);
			return FALSE;
		}
	}

	if (WM_KEYDOWN == pMsg->message && VK_TAB == pMsg->wParam && GetAsyncKeyState(VK_CONTROL) < 0 && m_arrChild.GetSize() > 1)
	{
		CWindowsManagerDialog* pDlg = new CWindowsManagerDialog;
		pDlg->Create(CWindowsManagerDialog::IDD, this);
		pDlg->ShowWindow(SW_SHOW);
		return TRUE;
	}

	return __super::PreTranslateMessage(pMsg);
}

/**
 * @brief Show/hide statusbar.
 */
void CMainFrame::OnViewStatusBar()
{
	bool bShow = !GetOptionsMgr()->GetBool(OPT_SHOW_STATUSBAR);
	GetOptionsMgr()->SaveOption(OPT_SHOW_STATUSBAR, bShow);

	__super::ShowControlBar(&m_wndStatusBar, bShow, 0);
}

/**
 * @brief Updates "Show Tabbar" menuitem.
 */
void CMainFrame::OnUpdateViewTabBar(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_SHOW_TABBAR));
}

/**
 * @brief Show/hide tabbar.
 */
void CMainFrame::OnViewTabBar()
{
	bool bShow = !GetOptionsMgr()->GetBool(OPT_SHOW_TABBAR);
	GetOptionsMgr()->SaveOption(OPT_SHOW_TABBAR, bShow);

	__super::ShowControlBar(&m_wndTabBar, bShow, 0);
}

/**
 * @brief Updates "Automatically Resize Panes" menuitem.
 */
void CMainFrame::OnUpdateResizePanes(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_RESIZE_PANES));
}


/**
 * @brief Enable/disable automatic pane resizing.
 */
void CMainFrame::OnResizePanes()
{
	bool bResize = !GetOptionsMgr()->GetBool(OPT_RESIZE_PANES);
	GetOptionsMgr()->SaveOption(OPT_RESIZE_PANES, bResize);
	// TODO: Introduce a common merge frame superclass?
	CFrameWnd *pActiveFrame = GetActiveFrame();
	if (CMergeEditFrame *pFrame = DYNAMIC_DOWNCAST(CMergeEditFrame, pActiveFrame))
	{
		pFrame->UpdateAutoPaneResize();
		if (bResize)
			pFrame->UpdateSplitter();
	}
	else if (CHexMergeFrame *pFrame1 = DYNAMIC_DOWNCAST(CHexMergeFrame, pActiveFrame))
	{
		pFrame1->UpdateAutoPaneResize();
		if (bResize)
			pFrame1->UpdateSplitter();
	}
}

/**
 * @brief Open project-file.
 */
void CMainFrame::OnFileOpenProject()
{
	String sFilepath;
	
	// get the default projects path
	String strProjectPath = GetOptionsMgr()->GetString(OPT_PROJECTS_PATH);
	if (!SelectFile(GetSafeHwnd(), sFilepath, true, strProjectPath.c_str(), _T(""),
			_("WinMerge Project Files (*.WinMerge)|*.WinMerge||")))
		return;
	
	strProjectPath = paths::GetParentPath(sFilepath);
	// store this as the new project path
	GetOptionsMgr()->SaveOption(OPT_PROJECTS_PATH, strProjectPath);

	theApp.LoadAndOpenProjectFile(sFilepath);
}

/**
 * @brief Receive command line from another instance.
 *
 * This function receives command line when only single-instance
 * is allowed. New instance tried to start sends its command line
 * to here so we can open paths it was meant to.
 */
LRESULT CMainFrame::OnCopyData(WPARAM wParam, LPARAM lParam)
{
	COPYDATASTRUCT *pCopyData = (COPYDATASTRUCT*)lParam;
	LPCTSTR pchData = (LPCTSTR)pCopyData->lpData;
	// Bail out if data isn't zero-terminated
	DWORD cchData = pCopyData->cbData / sizeof(TCHAR);
	if (cchData == 0 || pchData[cchData - 1] != _T('\0'))
		return FALSE;
	ReplyMessage(TRUE);
	MergeCmdLineInfo cmdInfo(pchData);
	theApp.ApplyCommandLineConfigOptions(cmdInfo);
	theApp.ParseArgsAndDoOpen(cmdInfo, this);
	return TRUE;
}

LRESULT CMainFrame::OnUser1(WPARAM wParam, LPARAM lParam)
{
	CFrameWnd * pFrame = GetActiveFrame();
	if (pFrame != nullptr)
	{
		IMergeDoc *pMergeDoc = dynamic_cast<IMergeDoc *>(pFrame->GetActiveDocument());
		if (pMergeDoc == nullptr)
			pMergeDoc = dynamic_cast<IMergeDoc *>(pFrame);
		if (pMergeDoc != nullptr)
			pMergeDoc->CheckFileChanged();
	}
	return 0;
}

/**
 * @brief Close all open windows.
 * 
 * Asks about saving unsaved files and then closes all open windows.
 */
void CMainFrame::OnWindowCloseAll()
{
	CMDIChildWnd *pChild = MDIGetActive();
	while (pChild != nullptr)
	{
		CDocument* pDoc;
		if ((pDoc = pChild->GetActiveDocument()) != nullptr)
		{
			if (!pDoc->SaveModified())
				return;
			pDoc->OnCloseDocument();
		}
		else if (GetFrameType(pChild) == FRAME_IMGFILE)
		{
			if (!static_cast<CImgMergeFrame *>(pChild)->CloseNow())
				return;
		}
		else
		{
			pChild->DestroyWindow();
		}
		pChild = MDIGetActive();
	}
	return;
}

/**
 * @brief Enables Window/Close All item if there are open windows.
 */ 
void CMainFrame::OnUpdateWindowCloseAll(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(MDIGetActive() != nullptr);
}

/**
 * @brief Access to the singleton main frame (where we have some globals)
 */
CMainFrame * GetMainFrame()
{
	CWnd * mainwnd = AfxGetMainWnd();
	ASSERT(mainwnd != nullptr);
	CMainFrame *pMainframe = dynamic_cast<CMainFrame*>(mainwnd);
	ASSERT(pMainframe != nullptr);
	return pMainframe;
}

/**
 * @brief Opens dialog for user to Load, edit and save project files.
 * This dialog gets current compare paths and filter (+other properties
 * possible in project files) and initializes the dialog with them.
 */
void CMainFrame::OnSaveProject()
{
	if (m_pMenus[MENU_OPENVIEW] == nullptr)
		theApp.m_pOpenTemplate->m_hMenuShared = NewOpenViewMenu();
	COpenDoc *pOpenDoc = static_cast<COpenDoc *>(theApp.m_pOpenTemplate->CreateNewDocument());

	PathContext paths;
	CFrameWnd * pFrame = GetActiveFrame();
	FRAMETYPE frame = GetFrameType(pFrame);

	if (frame == FRAME_FILE)
	{
		CMergeDoc * pMergeDoc = static_cast<CMergeDoc *>(pFrame->GetActiveDocument());
		pOpenDoc->m_files = pMergeDoc->m_filePaths;
		for (int pane = 0; pane < pOpenDoc->m_files.GetSize(); ++pane)
			pOpenDoc->m_dwFlags[pane] = FFILEOPEN_PROJECT | (pMergeDoc->m_ptBuf[pane]->GetReadOnly() ? FFILEOPEN_PROJECT : 0);
		pOpenDoc->m_bRecurse = GetOptionsMgr()->GetBool(OPT_CMP_INCLUDE_SUBDIRS);
		pOpenDoc->m_strExt = theApp.m_pGlobalFileFilter->GetFilterNameOrMask();
	}
	else if (frame == FRAME_FOLDER)
	{
		// Get paths currently in compare
		const CDirDoc * pDoc = static_cast<const CDirDoc*>(pFrame->GetActiveDocument());
		const CDiffContext& ctxt = pDoc->GetDiffContext();

		// Set-up the dialog
		for (int pane = 0; pane < ctxt.GetCompareDirs(); ++pane)
		{
			pOpenDoc->m_dwFlags[pane] = FFILEOPEN_PROJECT | (pDoc->GetReadOnly(pane) ? FFILEOPEN_READONLY : 0);
			pOpenDoc->m_files.SetPath(pane, paths::AddTrailingSlash(ctxt.GetNormalizedPath(pane)));
		}
		pOpenDoc->m_bRecurse = ctxt.m_bRecursive;
		pOpenDoc->m_strExt = static_cast<FileFilterHelper *>(ctxt.m_piFilterGlobal)->GetFilterNameOrMask();
	}

	CFrameWnd *pOpenFrame = theApp.m_pOpenTemplate->CreateNewFrame(pOpenDoc, nullptr);
	theApp.m_pOpenTemplate->InitialUpdateFrame(pOpenFrame, pOpenDoc);
}

/** 
 * @brief Start flashing window if window is inactive.
 */
void CMainFrame::StartFlashing()
{
	CWnd * activeWindow = GetActiveWindow();
	if (activeWindow != this)
		FlashWindowEx(FLASHW_ALL | FLASHW_TIMERNOFG, 3, 0);
}

#if _MFC_VER > 0x0600
void CMainFrame::OnActivateApp(BOOL bActive, DWORD dwThreadID)
#else
void CMainFrame::OnActivateApp(BOOL bActive, HTASK hTask)
#endif
{
#if _MFC_VER > 0x0600
	__super::OnActivateApp(bActive, dwThreadID);
#else
	__super::OnActivateApp(bActive, hTask);
#endif

	CFrameWnd * pFrame = GetActiveFrame();
	if (pFrame != nullptr)
	{
		IMergeDoc *pMergeDoc = dynamic_cast<IMergeDoc *>(pFrame->GetActiveDocument());
		if (pMergeDoc == nullptr)
			pMergeDoc = dynamic_cast<IMergeDoc *>(pFrame);
		if (pMergeDoc != nullptr)
			PostMessage(WM_USER+1);
	}
}

BOOL CMainFrame::CreateToolbar()
{
	if (!m_wndToolBar.CreateEx(this) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		return FALSE;
	}

	if (!m_wndReBar.Create(this, RBS_BANDBORDERS,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_ALIGN_TOP))
	{
		return FALSE;
	}

	VERIFY(m_wndToolBar.ModifyStyle(0, TBSTYLE_FLAT|TBSTYLE_TRANSPARENT));

	// Remove this if you don't want tool tips or a resizable toolbar
	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |
		CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
	m_wndToolBar.GetToolBarCtrl().SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS);

	m_wndReBar.AddBar(&m_wndToolBar);

	LoadToolbarImages();

	UINT nID, nStyle;
	int iImage;
	int index = m_wndToolBar.GetToolBarCtrl().CommandToIndex(ID_OPTIONS);
	m_wndToolBar.GetButtonInfo(index, nID, nStyle, iImage);
	nStyle |= TBSTYLE_DROPDOWN;
	m_wndToolBar.SetButtonInfo(index, nID, nStyle, iImage);

	if (!GetOptionsMgr()->GetBool(OPT_SHOW_TOOLBAR))
	{
		__super::ShowControlBar(&m_wndToolBar, false, 0);
	}

	return TRUE;
}

/** @brief Load toolbar images from the resource. */
void CMainFrame::LoadToolbarImages()
{
	const int toolbarNewImgSize = MulDiv(16, GetSystemMetrics(SM_CXSMICON), 16) * (1 + GetOptionsMgr()->GetInt(OPT_TOOLBAR_SIZE));
	const int toolbarOrgImgSize = toolbarNewImgSize <= 20 ? 16 : 32;
	CToolBarCtrl& BarCtrl = m_wndToolBar.GetToolBarCtrl();

	m_ToolbarImages[TOOLBAR_IMAGES_ENABLED].Detach();
	m_ToolbarImages[TOOLBAR_IMAGES_DISABLED].Detach();
	CSize sizeButton(0, 0);

	LoadToolbarImageList(toolbarOrgImgSize, toolbarNewImgSize,
		toolbarOrgImgSize <= 16 ? IDB_TOOLBAR_ENABLED : IDB_TOOLBAR_ENABLED32,
		false, m_ToolbarImages[TOOLBAR_IMAGES_ENABLED]);
	LoadToolbarImageList(toolbarOrgImgSize, toolbarNewImgSize,
		toolbarOrgImgSize <= 16 ? IDB_TOOLBAR_ENABLED : IDB_TOOLBAR_ENABLED32,
		true, m_ToolbarImages[TOOLBAR_IMAGES_DISABLED]);

	sizeButton = CSize(toolbarNewImgSize + 8, toolbarNewImgSize + 8);

	BarCtrl.SetButtonSize(sizeButton);
	if (CImageList *pImgList = BarCtrl.SetImageList(&m_ToolbarImages[TOOLBAR_IMAGES_ENABLED]))
		pImgList->DeleteImageList();
	if (CImageList *pImgList = BarCtrl.SetDisabledImageList(&m_ToolbarImages[TOOLBAR_IMAGES_DISABLED]))
		pImgList->DeleteImageList();

	// resize the rebar.
	REBARBANDINFO rbbi = { sizeof REBARBANDINFO };
	rbbi.fMask = RBBIM_CHILDSIZE;
	rbbi.cyMinChild = sizeButton.cy;
	m_wndReBar.GetReBarCtrl().SetBandInfo(0, &rbbi);
}


/**
 * @brief Load a transparent 32-bit color image list.
 */
static void LoadHiColImageList(UINT nIDResource, int nWidth, int nHeight, int nNewWidth, int nNewHeight, int nCount, bool bGrayscale, CImageList& ImgList)
{
	CBitmap bm;
	bm.Attach(LoadBitmapAndConvertTo32bit(AfxGetInstanceHandle(), nIDResource, nNewWidth * nCount, nNewHeight, bGrayscale, RGB(0xff, 0, 0xff)));

	VERIFY(ImgList.Create(nNewWidth, nNewHeight, ILC_COLOR32, nCount, 0));
	int nIndex = ImgList.Add(&bm, nullptr);
	ASSERT(-1 != nIndex);
}

/**
 * @brief Load toolbar image list.
 */
static void LoadToolbarImageList(int orgImageWidth, int newImageWidth, UINT nIDResource, bool bGrayscale, CImageList& ImgList)
{
	const int ImageCount = 22;
	const int orgImageHeight = orgImageWidth - 1;
	const int newImageHeight = newImageWidth - 1;
	LoadHiColImageList(nIDResource, orgImageWidth, orgImageHeight, newImageWidth, newImageHeight, ImageCount, bGrayscale, ImgList);
}

/**
 * @brief Called when the document title is modified.
 */
void CMainFrame::OnUpdateFrameTitle(BOOL bAddToTitle)
{
	CFrameWnd::OnUpdateFrameTitle(bAddToTitle);
	
	if (m_wndTabBar.m_hWnd != nullptr)
		m_wndTabBar.UpdateTabs();
}

/** @brief Show none/small/big/huge toolbar. */
void CMainFrame::OnToolbarSize(UINT id)
{
	if (id == ID_TOOLBAR_NONE)
	{
		GetOptionsMgr()->SaveOption(OPT_SHOW_TOOLBAR, false);
		__super::ShowControlBar(&m_wndToolBar, false, 0);
	}
	else
	{
		GetOptionsMgr()->SaveOption(OPT_SHOW_TOOLBAR, true);
		GetOptionsMgr()->SaveOption(OPT_TOOLBAR_SIZE, id - ID_TOOLBAR_SMALL);

		LoadToolbarImages();

		__super::ShowControlBar(&m_wndToolBar, true, 0);
	}
}

/** @brief Show none/small/big/huge toolbar. */
void CMainFrame::OnUpdateToolbarSize(CCmdUI *pCmdUI)
{
	if (!GetOptionsMgr()->GetBool(OPT_SHOW_TOOLBAR))
		pCmdUI->SetRadio(pCmdUI->m_nID == ID_TOOLBAR_NONE);
	else
		pCmdUI->SetRadio((pCmdUI->m_nID - ID_TOOLBAR_SMALL) == static_cast<UINT>(GetOptionsMgr()->GetInt(OPT_TOOLBAR_SIZE)));
}

/** @brief Lang aware version of CFrameWnd::OnToolTipText() */
BOOL CMainFrame::OnToolTipText(UINT, NMHDR* pNMHDR, LRESULT* pResult)
{
	ASSERT(pNMHDR->code == TTN_NEEDTEXTA || pNMHDR->code == TTN_NEEDTEXTW);

	// need to handle both ANSI and UNICODE versions of the message
	TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
	TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
	String strFullText;
	CString strTipText;
	UINT_PTR nID = pNMHDR->idFrom;
	if (pNMHDR->code == TTN_NEEDTEXTA && (pTTTA->uFlags & TTF_IDISHWND) ||
		pNMHDR->code == TTN_NEEDTEXTW && (pTTTW->uFlags & TTF_IDISHWND))
	{
		// idFrom is actually the HWND of the tool
		nID = ::GetDlgCtrlID((HWND)nID);
	}

	if (nID != 0) // will be zero on a separator
	{
		strFullText = theApp.LoadString(static_cast<UINT>(nID));
		// don't handle the message if no string resource found
		if (strFullText.empty())
			return FALSE;

		// this is the command id, not the button index
		AfxExtractSubString(strTipText, strFullText.c_str(), 1, '\n');
	}
	if (pNMHDR->code == TTN_NEEDTEXTA)
		_wcstombsz(pTTTA->szText, strTipText, static_cast<ULONG>(std::size(pTTTA->szText)));
	else
		lstrcpyn(pTTTW->szText, strTipText, static_cast<int>(std::size(pTTTW->szText)));
	*pResult = 0;

	// bring the tooltip window above other popup windows
	::SetWindowPos(pNMHDR->hwndFrom, HWND_TOP, 0, 0, 0, 0,
		SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOMOVE|SWP_NOOWNERZORDER);

	return TRUE;    // message was handled
}

/**
 * @brief Ask user for close confirmation when closing the mainframe.
 * This function asks if user wants to close multiple open windows when user
 * selects (perhaps accidentally) to close WinMerge (application).
 * @return true if user agreeds to close all windows.
 */
bool CMainFrame::AskCloseConfirmation()
{
	const DirDocList &dirdocs = GetAllDirDocs();
	const MergeDocList &mergedocs = GetAllMergeDocs();

	int ret = IDYES;
	const size_t count = dirdocs.GetCount() + mergedocs.GetCount();
	if (count > 1)
	{
		// Check that we don't have one empty dirdoc + mergedoc situation.
		// That happens since we open "hidden" dirdoc for every file compare.
		if (dirdocs.GetCount() == 1)
		{
			CDirDoc *pDoc = dirdocs.GetHead();
			if (!pDoc->HasDiffs())
				return true;
		}
		ret = LangMessageBox(IDS_CLOSEALL_WINDOWS, MB_YESNO | MB_ICONWARNING);
	}
	return (ret == IDYES);
}

/**
 * @brief Shows the release notes for user.
 * This function opens release notes HTML document into browser.
 */
void CMainFrame::OnHelpReleasenotes()
{
	const String sPath = paths::ConcatPath(env::GetProgPath(), RelNotes);
	ShellExecute(nullptr, _T("open"), sPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

/**
 * @brief Shows the translations page.
 * This function opens translations page URL into browser.
 */
void CMainFrame::OnHelpTranslations()
{
	ShellExecute(nullptr, _T("open"), TranslationsUrl, nullptr, nullptr, SW_SHOWNORMAL);
}

/**
 * @brief Called when user selects File/Open Conflict...
 */
void CMainFrame::OnFileOpenConflict()
{
	String conflictFile;
	if (SelectFile(GetSafeHwnd(), conflictFile))
	{
		DoOpenConflict(conflictFile);
	}
}

/**
 * @brief Select and open conflict file for resolving.
 * This function lets user to select conflict file to resolve.
 * Then we parse conflict file to two files to "merge" and
 * save resulting file over original file.
 *
 * Set left-side file read-only as it is the repository file which cannot
 * be modified anyway. Right-side file is user's file which is set as
 * modified by default so user can just save it and accept workspace
 * file as resolved file.
 * @param [in] conflictFile Full path to conflict file to open.
 * @param [in] checked If true, do not check if it really is project file.
 * @return `true` if conflict file was opened for resolving.
 */
bool CMainFrame::DoOpenConflict(const String& conflictFile, const String strDesc[] /*= nullptr*/, bool checked /*= false*/)
{
	bool conflictCompared = false;

	if (!checked)
	{
		bool confFile = IsConflictFile(conflictFile);
		if (!confFile)
		{
			String message = strutils::format_string1(_("The file\n%1\nis not a conflict file."), conflictFile);
			AfxMessageBox(message.c_str(), MB_ICONSTOP);
			return false;
		}
	}

	// Create temp files and put them into the list,
	// from where they get deleted when MainFrame is deleted.
	String ext = paths::FindExtension(conflictFile);
	TempFilePtr wTemp(new TempFile());
	String workFile = wTemp->Create(_T("confw_"), ext);
	m_tempFiles.push_back(wTemp);
	TempFilePtr vTemp(new TempFile());
	String revFile = vTemp->Create(_T("confv_"), ext);
	m_tempFiles.push_back(vTemp);
	TempFilePtr bTemp(new TempFile());
	String baseFile = vTemp->Create(_T("confb_"), ext);
	m_tempFiles.push_back(bTemp);

	// Parse conflict file into two files.
	bool inners, threeWay;
	int iGuessEncodingType = GetOptionsMgr()->GetInt(OPT_CP_DETECT);
	bool success = ParseConflictFile(conflictFile, workFile, revFile, baseFile, iGuessEncodingType, inners, threeWay);

	if (success)
	{
		// Open two parsed files to WinMerge, telling WinMerge to
		// save over original file (given as third filename).
		theApp.m_strSaveAsPath = conflictFile;
		if (!threeWay)
		{
			String strDesc2[2] = { 
				(strDesc && !strDesc[0].empty()) ? strDesc[0] : _("Theirs File"),
				(strDesc && !strDesc[2].empty()) ? strDesc[2] : _("Mine File") };
			DWORD dwFlags[2] = {FFILEOPEN_READONLY | FFILEOPEN_NOMRU, FFILEOPEN_NOMRU | FFILEOPEN_MODIFIED};
			PathContext tmpPathContext(revFile, workFile);
			conflictCompared = DoFileOpen(&tmpPathContext, dwFlags, strDesc2);
		}
		else
		{
			String strDesc3[3] = {
				(strDesc && !strDesc[0].empty()) ? strDesc[0] : _("Base File"),
				(strDesc && !strDesc[1].empty()) ? strDesc[1] : _("Theirs File"),
				(strDesc && !strDesc[2].empty()) ? strDesc[2] : _("Mine File") };
			PathContext tmpPathContext(baseFile, revFile, workFile);
			DWORD dwFlags[3] = {FFILEOPEN_READONLY | FFILEOPEN_NOMRU, FFILEOPEN_READONLY | FFILEOPEN_NOMRU, FFILEOPEN_NOMRU | FFILEOPEN_MODIFIED};
			conflictCompared = DoFileOpen(&tmpPathContext, dwFlags, strDesc3);
		}
	}
	else
	{
		LangMessageBox(IDS_ERROR_CONF_RESOLVE, MB_ICONSTOP);
	}
	return conflictCompared;
}

/**
 * @brief Get type of frame (File/Folder compare).
 * @param [in] pFrame Pointer to frame to check.
 * @return FRAMETYPE of the given frame.
*/
CMainFrame::FRAMETYPE CMainFrame::GetFrameType(const CFrameWnd * pFrame) const
{
	bool bMergeFrame = !!pFrame->IsKindOf(RUNTIME_CLASS(CMergeEditFrame));
	bool bHexMergeFrame = !!pFrame->IsKindOf(RUNTIME_CLASS(CHexMergeFrame));
	bool bImgMergeFrame = !!pFrame->IsKindOf(RUNTIME_CLASS(CImgMergeFrame));
	bool bDirFrame = !!pFrame->IsKindOf(RUNTIME_CLASS(CDirFrame));

	if (bMergeFrame)
		return FRAME_FILE;
	else if (bHexMergeFrame)
		return FRAME_HEXFILE;
	else if (bImgMergeFrame)
		return FRAME_IMGFILE;
	else if (bDirFrame)
		return FRAME_FOLDER;
	else
		return FRAME_OTHER;
}

/**
 * @brief Show the plugins list dialog.
 */
void CMainFrame::OnPluginsList()
{
	PluginsListDlg dlg;
	dlg.DoModal();
}

void CMainFrame::OnDiffOptionsDropDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTOOLBAR pToolBar = reinterpret_cast<LPNMTOOLBAR>(pNMHDR);
	ClientToScreen(&(pToolBar->rcButton));
	BCMenu menu;
	VERIFY(menu.LoadMenu(IDR_POPUP_DIFF_OPTIONS));
	theApp.TranslateMenu(menu.m_hMenu);
	CMenu* pPopup = menu.GetSubMenu(0);
	if (pPopup != nullptr)
	{
		pPopup->TrackPopupMenu(TPM_RIGHTALIGN | TPM_RIGHTBUTTON, 
			pToolBar->rcButton.right, pToolBar->rcButton.bottom, this);
	}
	*pResult = 0;
}
void CMainFrame::OnDiffWhitespace(UINT nID)
{
	GetOptionsMgr()->SaveOption(OPT_CMP_IGNORE_WHITESPACE, nID - IDC_DIFF_WHITESPACE_COMPARE);
	ApplyDiffOptions();
}

void CMainFrame::OnUpdateDiffWhitespace(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio((pCmdUI->m_nID - IDC_DIFF_WHITESPACE_COMPARE) == static_cast<UINT>(GetOptionsMgr()->GetInt(OPT_CMP_IGNORE_WHITESPACE)));
	pCmdUI->Enable();
}

void CMainFrame::OnDiffCaseSensitive()
{
	GetOptionsMgr()->SaveOption(OPT_CMP_IGNORE_CASE, !GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_CASE));
	ApplyDiffOptions();
}

void CMainFrame::OnUpdateDiffCaseSensitive(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(!GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_CASE));
	pCmdUI->Enable();
}

void CMainFrame::OnDiffIgnoreEOL()
{
	GetOptionsMgr()->SaveOption(OPT_CMP_IGNORE_EOL, !GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_EOL));
	ApplyDiffOptions();
}

void CMainFrame::OnUpdateDiffIgnoreEOL(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_EOL));
	pCmdUI->Enable();
}

void CMainFrame::OnDiffIgnoreCP()
{
	GetOptionsMgr()->SaveOption(OPT_CMP_IGNORE_CODEPAGE, !GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_CODEPAGE));
	ApplyDiffOptions();
}

void CMainFrame::OnUpdateDiffIgnoreCP(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_CODEPAGE));
	pCmdUI->Enable();
}

void CMainFrame::OnIncludeSubfolders()
{
	GetOptionsMgr()->SaveOption(OPT_CMP_INCLUDE_SUBDIRS, !GetOptionsMgr()->GetBool(OPT_CMP_INCLUDE_SUBDIRS));
	// Update all dirdoc settings
	for (auto pDirDoc : GetAllDirDocs())
		pDirDoc->RefreshOptions();
	for (auto pOpenDoc : GetAllOpenDocs())
		pOpenDoc->RefreshOptions();
}

void CMainFrame::OnUpdateIncludeSubfolders(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetOptionsMgr()->GetBool(OPT_CMP_INCLUDE_SUBDIRS));
	pCmdUI->Enable();
}

void CMainFrame::OnCompareMethod(UINT nID)
{ 
	GetOptionsMgr()->SaveOption(OPT_CMP_METHOD, nID - ID_COMPMETHOD_FULL_CONTENTS);
}

void CMainFrame::OnUpdateCompareMethod(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio((pCmdUI->m_nID - ID_COMPMETHOD_FULL_CONTENTS) == static_cast<UINT>(GetOptionsMgr()->GetInt(OPT_CMP_METHOD)));
	pCmdUI->Enable();
}

void CMainFrame::OnMRUs(UINT nID)
{
	std::vector<JumpList::Item> mrus = JumpList::GetRecentDocs(GetOptionsMgr()->GetInt(OPT_MRU_MAX));
	const size_t idx = nID - ID_MRU_FIRST;
	if (idx < mrus.size())
	{
		MergeCmdLineInfo cmdInfo((_T("\"") + mrus[idx].path + _T("\" ") + mrus[idx].params).c_str());
		theApp.ParseArgsAndDoOpen(cmdInfo, this);
	}
}

void CMainFrame::OnUpdateNoMRUs(CCmdUI* pCmdUI)
{
	// append the MRU submenu
	HMENU hMenu = GetSubmenu(AfxGetMainWnd()->GetMenu()->m_hMenu, ID_FILE_NEW, false);
	if (hMenu == nullptr)
		return;
	
	// empty the menu
	size_t i = ::GetMenuItemCount(hMenu);
	while (i --)
		::DeleteMenu(hMenu, 0, MF_BYPOSITION);

	std::vector<JumpList::Item> mrus = JumpList::GetRecentDocs(GetOptionsMgr()->GetInt(OPT_MRU_MAX));

	if (mrus.size() == 0)
	{
		// no script : create a <empty> entry
		::AppendMenu(hMenu, MF_STRING, ID_NO_EDIT_SCRIPTS, theApp.LoadString(IDS_NO_EDIT_SCRIPTS).c_str());
	}
	else
	{
		// or fill in the submenu with the scripts names
		int ID = ID_MRU_FIRST;	// first ID in menu
		for (i = 0 ; i < mrus.size() ; i++, ID++)
			::AppendMenu(hMenu, MF_STRING, ID, 
				((i < 9 ? strutils::format(_T("&%d "), i+1) : strutils::format(_T("&%c "), 'a' + i - 9)) 
					+ mrus[i].title).c_str());
	}

	pCmdUI->Enable(true);
}

/**
 * @brief Update plugin name
 * @param [in] pCmdUI UI component to update.
 */
void CMainFrame::OnUpdatePluginName(CCmdUI* pCmdUI)
{
	pCmdUI->SetText(_T(""));
}

void CMainFrame::ReloadMenu()
{
	// set the menu of the main frame window
	UINT idMenu = IDR_MAINFRAME;
	CMergeApp *pApp = dynamic_cast<CMergeApp *> (AfxGetApp());
	CMainFrame * pMainFrame = dynamic_cast<CMainFrame *> ((CFrameWnd*)pApp->m_pMainWnd);
	HMENU hNewDefaultMenu = pMainFrame->NewDefaultMenu(idMenu);
	HMENU hNewMergeMenu = pMainFrame->NewMergeViewMenu();
	HMENU hNewImgMergeMenu = pMainFrame->NewImgMergeViewMenu();
	HMENU hNewDirMenu = pMainFrame->NewDirViewMenu();
	if (hNewDefaultMenu != nullptr && hNewMergeMenu != nullptr && hNewDirMenu != nullptr)
	{
		// Note : for Windows98 compatibility, use FromHandle and not Attach/Detach
		CMenu * pNewDefaultMenu = CMenu::FromHandle(hNewDefaultMenu);
		CMenu * pNewMergeMenu = CMenu::FromHandle(hNewMergeMenu);
		CMenu * pNewImgMergeMenu = CMenu::FromHandle(hNewImgMergeMenu);
		CMenu * pNewDirMenu = CMenu::FromHandle(hNewDirMenu);

		CWnd *pFrame = CWnd::FromHandle(::GetWindow(pMainFrame->m_hWndMDIClient, GW_CHILD));
		while (pFrame != nullptr)
		{
			if (pFrame->IsKindOf(RUNTIME_CLASS(CMergeEditFrame)))
				static_cast<CMergeEditFrame *>(pFrame)->SetSharedMenu(hNewMergeMenu);
			if (pFrame->IsKindOf(RUNTIME_CLASS(CHexMergeFrame)))
				static_cast<CHexMergeFrame *>(pFrame)->SetSharedMenu(hNewMergeMenu);
			if (pFrame->IsKindOf(RUNTIME_CLASS(CImgMergeFrame)))
				static_cast<CImgMergeFrame *>(pFrame)->SetSharedMenu(hNewImgMergeMenu);
			else if (pFrame->IsKindOf(RUNTIME_CLASS(COpenFrame)))
				static_cast<COpenFrame *>(pFrame)->SetSharedMenu(hNewDefaultMenu);
			else if (pFrame->IsKindOf(RUNTIME_CLASS(CDirFrame)))
				static_cast<CDirFrame *>(pFrame)->SetSharedMenu(hNewDirMenu);
			pFrame = pFrame->GetNextWindow();
		}

		CFrameWnd *pActiveFrame = pMainFrame->GetActiveFrame();
		if (pActiveFrame != nullptr)
		{
			if (pActiveFrame->IsKindOf(RUNTIME_CLASS(CMergeEditFrame)))
				pMainFrame->MDISetMenu(pNewMergeMenu, nullptr);
			else if (pActiveFrame->IsKindOf(RUNTIME_CLASS(CHexMergeFrame)))
				pMainFrame->MDISetMenu(pNewMergeMenu, nullptr);
			else if (pActiveFrame->IsKindOf(RUNTIME_CLASS(CImgMergeFrame)))
				pMainFrame->MDISetMenu(pNewImgMergeMenu, nullptr);
			else if (pActiveFrame->IsKindOf(RUNTIME_CLASS(CDirFrame)))
				pMainFrame->MDISetMenu(pNewDirMenu, nullptr);
			else
				pMainFrame->MDISetMenu(pNewDefaultMenu, nullptr);
		}
		else
			pMainFrame->MDISetMenu(pNewDefaultMenu, nullptr);

		// Don't delete the old menu
		// There is a bug in BCMenu or in Windows98 : the new menu does not
		// appear correctly if we destroy the old one
		//			if (pOldDefaultMenu != nullptr)
		//				pOldDefaultMenu->DestroyMenu();
		//			if (pOldMergeMenu != nullptr)
		//				pOldMergeMenu->DestroyMenu();
		//			if (pOldDirMenu = nullptr)
		//				pOldDirMenu->DestroyMenu();

		// m_hMenuDefault is used to redraw the main menu when we close a child frame
		// if this child frame had a different menu
		pMainFrame->m_hMenuDefault = hNewDefaultMenu;
		pApp->m_pOpenTemplate->m_hMenuShared = hNewDefaultMenu;
		pApp->m_pDiffTemplate->m_hMenuShared = hNewMergeMenu;
		pApp->m_pDirTemplate->m_hMenuShared = hNewDirMenu;

		// force redrawing the menu bar
		pMainFrame->DrawMenuBar();
	}
}

void CMainFrame::UpdateDocTitle()
{
	CDocManager* pDocManager = AfxGetApp()->m_pDocManager;
	POSITION posTemplate = pDocManager->GetFirstDocTemplatePosition();
	ASSERT(posTemplate != nullptr);

	while (posTemplate != nullptr)
	{
		CDocTemplate* pTemplate = pDocManager->GetNextDocTemplate(posTemplate);

		ASSERT(pTemplate != nullptr);

		for (auto pDoc : GetDocList(static_cast<CMultiDocTemplate *>(pTemplate)))
		{
			static_cast<CDocument *>(const_cast<void *>(pDoc))->SetTitle(nullptr);
			((CFrameWnd*)AfxGetApp()->m_pMainWnd)->OnUpdateFrameTitle(TRUE);
		}
	}
}

void CMainFrame::OnAccelQuit()
{
	// TODO: Add your command handler code here

	SendMessage(WM_CLOSE);
}

LRESULT CMainFrame::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
	int olddpi = m_dpi;

	__super::OnDpiChanged(wParam, lParam);

	DpiAware::UpdateAfxDataSysMetrics(m_dpi);
	BCMenu::ReopenTheme(m_dpi);

	m_lfDiff.lfHeight = MulDiv(m_lfDiff.lfHeight, m_dpi, olddpi);
	m_lfDir.lfHeight = MulDiv(m_lfDir.lfHeight, m_dpi, olddpi);
	
	UpdateFont(FRAME_FILE);
	UpdateFont(FRAME_FOLDER);

	LoadToolbarImages();

	const RECT* pRect = reinterpret_cast<RECT*>(lParam);
	SetWindowPos(nullptr, pRect->left, pRect->top,
		pRect->right - pRect->left,
		pRect->bottom - pRect->top, SWP_NOZORDER | SWP_NOACTIVATE);

	return 0;
}

LRESULT CMainFrame::OnChildFrameAdded(WPARAM wParam, LPARAM lParam)
{
	for (int i = 0; i < m_arrChild.GetSize(); ++i)
	{
		if (reinterpret_cast<CMDIChildWnd*>(lParam) == m_arrChild.GetAt(i))
		{
			return 0;
		}
	}

	m_arrChild.InsertAt(0, (CMDIChildWnd*)lParam);

	return 1;
}

LRESULT CMainFrame::OnChildFrameRemoved(WPARAM wParam, LPARAM lParam)
{
	for (int i = 0; i < m_arrChild.GetSize(); ++i)
	{
		if (reinterpret_cast<CMDIChildWnd*>(lParam) == m_arrChild.GetAt(i))
		{
			m_arrChild.RemoveAt(i);
			break;
		}
	}

	return 1;
}

LRESULT CMainFrame::OnChildFrameActivate(WPARAM wParam, LPARAM lParam)
{
	for (int i = 0; i < m_arrChild.GetSize(); ++i)
	{
		if (reinterpret_cast<CMDIChildWnd*>(lParam) == m_arrChild.GetAt(i))
		{
			CMDIChildWnd* pMDIChild = m_arrChild.GetAt(i);
			if (pMDIChild->IsIconic())
				pMDIChild->ShowWindow(SW_RESTORE);
			MDIActivate(pMDIChild);
			break;
		}
	}

	return 1;
}
// put lParam as index 0 in m_arrChild
LRESULT CMainFrame::OnChildFrameActivated(WPARAM wParam, LPARAM lParam)
{
	for (int i = 0; i < m_arrChild.GetSize(); ++i)
	{
		if (reinterpret_cast<CMDIChildWnd*>(lParam) == m_arrChild.GetAt(i))
		{
			m_arrChild.RemoveAt(i);
			break;
		}
	}

	m_arrChild.InsertAt(0, (CMDIChildWnd*)lParam);

	return 1;
}

BOOL CMainFrame::OnMDIWindowCmd(UINT nID)
{
	switch (nID)
	{
	case ID_WINDOW_TILE_HORZ:
	case ID_WINDOW_TILE_VERT:
	{
		bool bHorizontal = (nID == ID_WINDOW_TILE_HORZ);
		m_layoutManager.SetTileLayoutEnabled(true);
		m_layoutManager.Tile(bHorizontal);
		break;
	}
	case ID_WINDOW_CASCADE:
		m_layoutManager.SetTileLayoutEnabled(false);
		__super::OnMDIWindowCmd(nID);
		break;
	}
	return 0;
}

