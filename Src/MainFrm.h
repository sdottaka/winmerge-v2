/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  MainFrm.h
 *
 * @brief Declaration file for CMainFrame
 *
 */
#pragma once

#include <vector>
#include <memory>
#include "MDITabBar.h"
#include "PathContext.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "utils/DpiAware.h"
#include "utils/MDITileLayout.h"

class BCMenu;
class CDirView;
class COpenDoc;
class CDirDoc;
class CMergeDoc;
class CHexMergeDoc;
class CMergeEditView;
class SyntaxColors;
class LineFiltersList;
class TempFile;
struct FileLocation;
class DropHandler;
class CMainFrame;
class CImgMergeFrame;

typedef std::shared_ptr<TempFile> TempFilePtr;

// typed lists (homogenous pointer lists)
typedef CTypedPtrList<CPtrList, COpenDoc *> OpenDocList;
typedef CTypedPtrList<CPtrList, CMergeDoc *> MergeDocList;
typedef CTypedPtrList<CPtrList, CDirDoc *> DirDocList;
typedef CTypedPtrList<CPtrList, CHexMergeDoc *> HexMergeDocList;

class PackingInfo;
class CLanguageSelect;

CMainFrame * GetMainFrame(); // access to the singleton main frame object

/**
 * @brief Frame class containing save-routines etc
 */
class CMainFrame : public DpiAware::CDpiAwareWnd<CMDIFrameWnd>
{
	friend CLanguageSelect;
	DECLARE_DYNAMIC(CMainFrame)
public:
	/**
	 * @brief Frame/View/Document types.
	 */
	enum FRAMETYPE
	{
		FRAME_FOLDER, /**< Folder compare frame. */
		FRAME_FILE, /**< File compare frame. */
		FRAME_HEXFILE, /**< Hex file compare frame. */
		FRAME_IMGFILE, /**< Image file compare frame. */
		FRAME_OTHER, /**< No frame? */
	};

	CMainFrame();

// Attributes
public:	
	bool m_bShowErrors; /**< Show folder compare error items? */
	LOGFONT m_lfDiff; /**< MergeView user-selected font */
	LOGFONT m_lfDir; /**< DirView user-selected font */
	static const TCHAR szClassName[];

// Operations
public:
	HMENU NewDirViewMenu();
	HMENU NewMergeViewMenu();
	HMENU NewHexMergeViewMenu();
	HMENU NewImgMergeViewMenu();
	HMENU NewOpenViewMenu();
	HMENU NewDefaultMenu(int ID = 0);
	HMENU GetScriptsSubmenu(HMENU mainMenu);
	HMENU GetPrediffersSubmenu(HMENU mainMenu);
	void UpdatePrediffersMenu();

	void FileNew(int nPanes);
	bool DoFileOpen(const PathContext *pFiles = nullptr,
		const DWORD dwFlags[] = nullptr, const String strDesc[] = nullptr, const String& sReportFile = _T(""), bool bRecurse = false, CDirDoc *pDirDoc = nullptr, String prediffer = _T(""), const PackingInfo * infoUnpacker = nullptr);
	bool ShowAutoMergeDoc(CDirDoc * pDirDoc, int nFiles, const FileLocation fileloc[],
		const DWORD dwFlags[], const String strDesc[], const String& sReportFile = _T(""), const PackingInfo * infoUnpacker = nullptr);
	bool ShowMergeDoc(CDirDoc * pDirDoc, int nFiles, const FileLocation fileloc[],
		const DWORD dwFlags[], const String strDesc[], const String& sReportFile = _T(""), const PackingInfo * infoUnpacker = nullptr);
	bool ShowHexMergeDoc(CDirDoc * pDirDoc, int nFiles, const FileLocation fileloc[],
		const DWORD dwFlags[], const String strDesc[], const String& sReportFile = _T(""), const PackingInfo * infoUnpacker = nullptr);
	bool ShowImgMergeDoc(CDirDoc * pDirDoc, int nFiles, const FileLocation fileloc[],
		const DWORD dwFlags[], const String strDesc[], const String& sReportFile = _T(""), const PackingInfo * infoUnpacker = nullptr);

	void UpdateResources();
	void ClearStatusbarItemCount();
	void ApplyDiffOptions();
	void SelectFilter();
	void StartFlashing();
	bool AskCloseConfirmation();
	bool DoOpenConflict(const String& conflictFile, const String strDesc[] = nullptr, bool checked = false);
	FRAMETYPE GetFrameType(const CFrameWnd * pFrame) const;
	void UpdateDocTitle();
	void ReloadMenu();
	DropHandler *GetDropHandler() const { return m_pDropHandler; }
	const CTypedPtrArray<CPtrArray, CMDIChildWnd*>* GetChildArray() const { return &m_arrChild; }
	MDITileLayout::LayoutManager& GetLayoutManager() { return m_layoutManager; };

// Overrides
	virtual void GetMessageString(UINT nID, CString& rMessage) const;
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
public:
	virtual void ActivateFrame(int nCmdShow = -1);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnUpdateFrameTitle(BOOL bAddToTitle);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

	//}}AFX_VIRTUAL

// Implementation methods
protected:
	virtual ~CMainFrame();

// Public implementation data
public:
	bool m_bFirstTime; /**< If first time frame activated, get  pos from reg */

// Implementation data
protected:
	// control bar embedded members
	CStatusBar  m_wndStatusBar;
	CReBar m_wndReBar;
	CToolBar m_wndToolBar;
	CMDITabBar m_wndTabBar;
	CTypedPtrArray<CPtrArray, CMDIChildWnd*> m_arrChild;
	MDITileLayout::LayoutManager m_layoutManager;

	// Tweak MDI client window behavior
	class CMDIClient : public DpiAware::CDpiAwareWnd<CWnd>
	{
		static UINT_PTR const m_nRedrawTimer = 1612;
		virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_MDICREATE:
			case WM_MDIACTIVATE:
			{
				// To reduce flicker in maximized state, disable drawing while messing with MDI child frames
				BOOL bMaximized;
				HWND hwndActive = reinterpret_cast<HWND>(SendMessage(WM_MDIGETACTIVE, 0, reinterpret_cast<LPARAM>(&bMaximized)));
				if ((bMaximized || (message == WM_MDICREATE && !hwndActive)) &&
					SetTimer(m_nRedrawTimer, USER_TIMER_MINIMUM, nullptr))
				{
					SetRedraw(FALSE);
				}
				break;
			}
			case WM_TIMER:
				if (wParam == m_nRedrawTimer)
				{
					KillTimer(m_nRedrawTimer);
					SetRedraw(TRUE);
					RedrawWindow(NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE);
				}
				break;
			case WM_SIZE:
				if (AfxGetMainWnd())
					GetMainFrame()->GetLayoutManager().NotifyMainResized();
				break;
			case WM_DPICHANGED_BEFOREPARENT:
				UpdateDpi();
				break;
			}
			return CWnd::WindowProc(message, wParam, lParam);
		}
	} m_wndMDIClient;

	/** @brief Toolbar image table indexes. */
	enum TOOLBAR_IMAGES
	{
		TOOLBAR_IMAGES_ENABLED,
		TOOLBAR_IMAGES_DISABLED,
		TOOLBAR_IMAGES_COUNT
	};

	CImageList m_ToolbarImages[TOOLBAR_IMAGES_COUNT]; /**< Images for toolbar */

	enum
	{
		MENU_DEFAULT,
		MENU_MERGEVIEW,
		MENU_DIRVIEW,
		MENU_HEXMERGEVIEW,
		MENU_IMGMERGEVIEW,
		MENU_OPENVIEW,
		MENU_COUNT, // Add new items before this item
	};
	/**
	 * Menu frames - for which frame(s) the menu is.
	 */
	enum
	{
		MENU_MAINFRM = 0x000001,
		MENU_FILECMP = 0x000002,
		MENU_FOLDERCMP = 0x000004,
		MENU_ALL = MENU_MAINFRM | MENU_FILECMP | MENU_FOLDERCMP
	};

	/**
	 * A structure attaching a menu item, icon and menu types to apply to.
	 */
	struct MENUITEM_ICON
	{
		int menuitemID;   /**< Menu item's ID. */
		int iconResID;    /**< Icon's resource ID. */
		int menusToApply; /**< For which menus to apply. */
	};

	static const MENUITEM_ICON m_MenuIcons[];

	std::unique_ptr<BCMenu> m_pMenus[MENU_COUNT]; /**< Menus for different views */
	std::unique_ptr<BCMenu> m_pImageMenu;
	std::vector<TempFilePtr> m_tempFiles; /**< List of possibly needed temp files. */
	DropHandler *m_pDropHandler;


// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	afx_msg LRESULT OnMenuChar(UINT nChar, UINT nFlags, CMenu* pMenu) ;
	afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnFileOpen();
	afx_msg void OnHelpGnulicense();
	afx_msg void OnOptions();
	afx_msg void OnViewSelectfont();
	afx_msg void OnViewUsedefaultfont();
	afx_msg void OnHelpContents();
	afx_msg void OnClose();
	afx_msg void OnToolsGeneratePatch();
	afx_msg void OnDropFiles(const std::vector<String>& files);
	afx_msg void OnUpdatePluginUnpackMode(CCmdUI* pCmdUI);
	afx_msg void OnPluginUnpackMode(UINT nID);
	afx_msg void OnUpdatePluginPrediffMode(CCmdUI* pCmdUI);
	afx_msg void OnPluginPrediffMode(UINT nID);
	afx_msg void OnUpdateReloadPlugins(CCmdUI* pCmdUI);
	afx_msg void OnReloadPlugins();
	afx_msg void OnSaveConfigData();
	afx_msg void OnFileNew();
	afx_msg void OnFileNew3();
	afx_msg void OnToolsFilters();
	afx_msg void OnViewStatusBar();
	afx_msg void OnUpdateViewTabBar(CCmdUI* pCmdUI);
	afx_msg void OnViewTabBar();
	afx_msg void OnUpdateResizePanes(CCmdUI* pCmdUI);
	afx_msg void OnResizePanes();
	afx_msg void OnFileOpenProject();
	afx_msg LRESULT OnCopyData(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnUser1(WPARAM wParam, LPARAM lParam);
	afx_msg void OnWindowCloseAll();
	afx_msg void OnUpdateWindowCloseAll(CCmdUI* pCmdUI);
	afx_msg void OnSaveProject();
#if _MFC_VER > 0x0600
	afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
#else
	afx_msg void OnActivateApp(BOOL bActive, HTASK hTask);
#endif
	afx_msg void OnToolbarSize(UINT id);
	afx_msg void OnUpdateToolbarSize(CCmdUI* pCmdUI);
	afx_msg BOOL OnToolTipText(UINT, NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnHelpReleasenotes();
	afx_msg void OnHelpTranslations();
	afx_msg void OnFileOpenConflict();
	afx_msg void OnPluginsList();
	afx_msg void OnUpdatePluginName(CCmdUI* pCmdUI);
	afx_msg void OnDiffOptionsDropDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDiffWhitespace(UINT nID);
	afx_msg void OnUpdateDiffWhitespace(CCmdUI* pCmdUI);
	afx_msg void OnDiffCaseSensitive();
	afx_msg void OnUpdateDiffCaseSensitive(CCmdUI* pCmdUI);
	afx_msg void OnDiffIgnoreEOL();
	afx_msg void OnUpdateDiffIgnoreEOL(CCmdUI* pCmdUI);
	afx_msg void OnDiffIgnoreCP();
	afx_msg void OnUpdateDiffIgnoreCP(CCmdUI* pCmdUI);
	afx_msg void OnIncludeSubfolders();
	afx_msg void OnUpdateIncludeSubfolders(CCmdUI* pCmdUI);
	afx_msg void OnCompareMethod(UINT nID);
	afx_msg void OnUpdateCompareMethod(CCmdUI* pCmdUI);
	afx_msg void OnMRUs(UINT nID);
	afx_msg void OnUpdateNoMRUs(CCmdUI* pCmdUI);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnAccelQuit();
	afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
	afx_msg BOOL OnMDIWindowCmd(UINT nID);
	//}}AFX_MSG
	afx_msg LRESULT OnChildFrameAdded(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnChildFrameRemoved(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnChildFrameActivate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnChildFrameActivated(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

private:
	void addToMru(LPCTSTR szItem, LPCTSTR szRegSubKey, UINT nMaxItems = 20);
	OpenDocList &GetAllOpenDocs();
	MergeDocList &GetAllMergeDocs();
	DirDocList &GetAllDirDocs();
	HexMergeDocList &GetAllHexMergeDocs();
	std::list<CImgMergeFrame *> GetAllImgMergeFrames();
	void UpdateFont(FRAMETYPE frame);
	BOOL CreateToolbar();
	CMergeEditView * GetActiveMergeEditView();
	void LoadToolbarImages();
	HMENU NewMenu( int view, int ID );
};
