/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  DirFrame.cpp
 *
 * @brief Implementation file for CDirFrame
 *
 */

#include "stdafx.h"
#include "DirFrame.h"
#include "Merge.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/**
 * @brief Statusbar pane indexes
 */
enum
{
	PANE_COMPMETHOD = 1,
	PANE_FILTER,
	PANE_LEFT_RO,
	PANE_MIDDLE_RO,
	PANE_RIGHT_RO,
};

/**
 * @brief Width of compare method name pane in statusbar
 */
const int COMPMETHOD_PANEL_WIDTH = 100;
/**
 * @brief Width of filter name pane in statusbar
 */
const int FILTER_PANEL_WIDTH = 100;

/**
 * @brief Bottom statusbar panels and indicators
 */
static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
};

/**
 * @brief RO status panel width
 */
static UINT RO_PANEL_WIDTH = 30;

/////////////////////////////////////////////////////////////////////////////
// CDirFrame

IMPLEMENT_DYNCREATE(CDirFrame, CMergeFrameCommon)

CDirFrame::CDirFrame()
: CMergeFrameCommon(IDI_EQUALFOLDER, IDI_NOTEQUALFOLDER)
{
}

CDirFrame::~CDirFrame()
{
}

BEGIN_MESSAGE_MAP(CDirFrame, CMergeFrameCommon)
	//{{AFX_MSG_MAP(CDirFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDirFrame message handlers

/**
 * @brief Create statusbar
 */
int CDirFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;

	EnableDocking(CBRS_ALIGN_TOP);

	// Dir frame has a header bar at top
	if (!m_wndFilePathBar.Create(this))
	{
		TRACE0("Failed to create header bar\n");
		return -1;      // fail to create
	}	

	// Directory frame has a status bar
	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}	
	
	String sText = _("RO");
	m_wndStatusBar.SetPaneInfo(0, 0, SBPS_STRETCH | SBPS_NOBORDERS, 0);
	m_wndStatusBar.SetPaneInfo(PANE_COMPMETHOD, ID_STATUS_FILTER, 0, PointToPixel(COMPMETHOD_PANEL_WIDTH));
	m_wndStatusBar.SetPaneInfo(PANE_FILTER, ID_STATUS_FILTER, 0, PointToPixel(FILTER_PANEL_WIDTH));
	m_wndStatusBar.SetPaneInfo(PANE_LEFT_RO, ID_STATUS_LEFTDIR_RO, 0, PointToPixel(RO_PANEL_WIDTH));
	m_wndStatusBar.SetPaneInfo(PANE_MIDDLE_RO, ID_STATUS_MIDDLEDIR_RO, 0, PointToPixel(RO_PANEL_WIDTH));
	m_wndStatusBar.SetPaneInfo(PANE_RIGHT_RO, ID_STATUS_RIGHTDIR_RO, 0, PointToPixel(RO_PANEL_WIDTH));
	m_wndStatusBar.SetPaneText(PANE_LEFT_RO, sText.c_str(), TRUE); 
	m_wndStatusBar.SetPaneText(PANE_MIDDLE_RO, sText.c_str(), TRUE); 
	m_wndStatusBar.SetPaneText(PANE_RIGHT_RO, sText.c_str(), TRUE);

	// load docking positions and sizes
	CDockState dockState;
	dockState.LoadState(_T("Settings-DirFrame"));
	SetDockState(dockState);

	return 0;
}

/**
 * @brief Set statusbar text
 */
void CDirFrame::SetStatus(LPCTSTR szStatus)
{
	m_wndStatusBar.SetPaneText(0, szStatus);
}

/**
 * @brief Set current compare method name to statusbar
 * @param [in] nCompMethod compare method to show
 */
void CDirFrame::SetCompareMethodStatusDisplay(int nCompMethod)
{
	m_wndStatusBar.SetPaneText(PANE_COMPMETHOD, LoadResString(IDS_COMPMETHOD_FULL_CONTENTS + nCompMethod).c_str());
}

/**
 * @brief Set active filter name to statusbar
 * @param [in] szFilter Filtername to show
 */
void CDirFrame::SetFilterStatusDisplay(LPCTSTR szFilter)
{
	m_wndStatusBar.SetPaneText(PANE_FILTER, szFilter);
}

/**
 * @brief Restore maximized state of directory compare window
 */
void CDirFrame::ActivateFrame(int nCmdShow) 
{
	__super::ActivateFrame(nCmdShow);
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CDirFrame::UpdateResources()
{
}

void CDirFrame::OnClose() 
{	
	__super::OnClose();
}

/**
 * @brief Save maximized state before destroying window
 */
BOOL CDirFrame::DestroyWindow() 
{
	// save docking positions and sizes
	CDockState dockState;
	GetDockState(dockState);
	dockState.SaveState(_T("Settings-DirFrame"));
	SaveWindowState();
	return __super::DestroyWindow();
}

void CDirFrame::OnSize(UINT nType, int cx, int cy) 
{
	__super::OnSize(nType, cx, cy);
	
	m_wndFilePathBar.Resize();
}
