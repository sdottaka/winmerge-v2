/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  MergeEditFrm.cpp
 *
 * @brief Implementation file for CMergeEditFrame
 *
 */

#include "stdafx.h"
#include "MergeEditFrm.h"
#include "Merge.h"
#include "MainFrm.h"
#include "MergeDoc.h"
#include "MergeEditView.h"
#include "LocationView.h"
#include "DiffViewBar.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SWAPPARAMS_IF(c, a, b) (c ? a : b), (c ? b : a)

/////////////////////////////////////////////////////////////////////////////
// CMergeEditFrame

IMPLEMENT_DYNCREATE(CMergeEditFrame, CMergeFrameCommon)

BEGIN_MESSAGE_MAP(CMergeEditFrame, CMergeFrameCommon)
	//{{AFX_MSG_MAP(CMergeEditFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_MDIACTIVATE()
	ON_WM_TIMER()
	ON_UPDATE_COMMAND_UI(ID_VIEW_DETAIL_BAR, OnUpdateControlBarMenu)
	ON_COMMAND_EX(ID_VIEW_DETAIL_BAR, OnBarCheck)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LOCATION_BAR, OnUpdateControlBarMenu)
	ON_COMMAND_EX(ID_VIEW_LOCATION_BAR, OnBarCheck)
	ON_COMMAND(ID_VIEW_SPLITVERTICALLY, OnViewSplitVertically)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SPLITVERTICALLY, OnUpdateViewSplitVertically)
	ON_MESSAGE(MSG_STORE_PANESIZES, OnStorePaneSizes)
	ON_WM_SIZE()
	ON_MESSAGE_VOID(WM_IDLEUPDATECMDUI, OnIdleUpdateCmdUI)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

#define IDT_SAVEPOSITION 2

/////////////////////////////////////////////////////////////////////////////
// CMergeEditFrame construction/destruction

/**
 * @brief Constructor.
 */
CMergeEditFrame::CMergeEditFrame()
: CMergeFrameCommon(IDI_EQUALTEXTFILE, IDI_NOTEQUALTEXTFILE)
, m_pwndDetailMergeEditSplitterView(nullptr)
{
	m_pMergeDoc = 0;
}

/**
 * Destructor.
 */
CMergeEditFrame::~CMergeEditFrame()
{
}

BOOL CMergeEditFrame::OnCreateClient( LPCREATESTRUCT /*lpcs*/,
	CCreateContext* pContext)
{
	m_wndSplitter.HideBorders(true);
	m_wndSplitter.Create(this, 2, 1, CSize(1, 1), pContext, WS_CHILD | WS_VISIBLE | 1/*SPLS_DYNAMIC_SPLIT*/);

	// Merge frame has also a dockable bar at the very left
	// This is not the client area, but we create it now because we want
	// to use the CCreateContext
	String sCaption = _("Location Pane");
	if (!m_wndLocationBar.Create(this, sCaption.c_str(), WS_CHILD | WS_VISIBLE, ID_VIEW_LOCATION_BAR))
	{
		TRACE0("Failed to create LocationBar\n");
		return FALSE;
	}

	CLocationView *pLocationView = new CLocationView;
	DWORD dwStyle = AFX_WS_DEFAULT_VIEW & ~WS_BORDER;
	pLocationView->Create(nullptr, nullptr, dwStyle, CRect(0,0,40,100), &m_wndLocationBar, 152, pContext);

	// Merge frame has also a dockable bar at the very bottom
	// This is not the client area, but we create it now because we want
	// to use the CCreateContext
	sCaption = _("Diff Pane");
	if (!m_wndDetailBar.Create(this, sCaption.c_str(), WS_CHILD | WS_VISIBLE, ID_VIEW_DETAIL_BAR))
	{
		TRACE0("Failed to create DiffViewBar\n");
		return FALSE;
	}

	m_pwndDetailMergeEditSplitterView = new CMergeEditSplitterView();
	m_pwndDetailMergeEditSplitterView->m_bDetailView = true;
	m_pwndDetailMergeEditSplitterView->Create(nullptr, nullptr, dwStyle, CRect(0,0,1,1), &m_wndDetailBar, ID_VIEW_DETAIL_BAR+1, pContext);

	// tell merge doc about these views
	m_pMergeDoc = dynamic_cast<CMergeDoc *>(pContext->m_pCurrentDoc);
	m_pMergeDoc->ForEachView([&](auto& pView) {
		pView->SetStatusInterface(m_wndStatusBar.GetIMergeEditStatus(pView->m_nThisPane));
	});
	m_pMergeDoc->SetLocationView(pLocationView);

	m_wndFilePathBar.SetPaneCount(m_pMergeDoc->m_nBuffers);
	m_wndFilePathBar.SetOnSetFocusCallback([&](int pane) {
		m_pMergeDoc->GetView(0, pane)->SetActivePane();
	});
	m_wndStatusBar.SetPaneCount(m_pMergeDoc->m_nBuffers);
	
	// Set frame window handles so we can post stage changes back
	pLocationView->SetFrameHwnd(GetSafeHwnd());
	m_wndLocationBar.SetFrameHwnd(GetSafeHwnd());
	m_wndDetailBar.SetFrameHwnd(GetSafeHwnd());

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMergeEditFrame message handlers

/**
* @brief Create the child frame, the splitter, the filename bar, the status bar,
* the diff dockable bar, and the four views
*
* @note  the panels layout is 
* <ul>
*  <li>   -----------
*  <li>		!  header !
*  <li>		!.........!
*  <li>		!.   .   .!
*  <li>		!. 1 . 2 .!
*  <li>		!.   .   .!
*  <li>		!.........!
*  <li>		!.........!
*  <li>		!  status !
*  <li>		-----------
*  <li>		!.........!
*  <li>		!.   3   .!
*  <li>		!.dockable!
*  <li>		! splitbar!
*  <li>		!.   4   .!
*  <li>		!.........!
*  <li>		-----------
* </ul>
*/
int CMergeEditFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;

	EnableDocking(CBRS_ALIGN_TOP|CBRS_ALIGN_BOTTOM|CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);

	RemoveBarBorder();

	// Merge frame has a header bar at top
	if (!m_wndFilePathBar.Create(this))
	{
		TRACE0("Failed to create dialog bar\n");
		return -1;      // fail to create
	}

	// Set filename bars inactive so colors get initialized
	m_wndFilePathBar.SetActive(0, false);
	m_wndFilePathBar.SetActive(1, false);
	m_wndFilePathBar.SetActive(2, false);

	// Merge frame also has a dockable bar at the very left
	// created in OnCreateClient 
	m_wndLocationBar.SetBarStyle(m_wndLocationBar.GetBarStyle() |
		CBRS_SIZE_DYNAMIC | CBRS_ALIGN_LEFT);
	m_wndLocationBar.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
	CRect rc{ 0, 0, 0, 0 };
	DockControlBar(&m_wndLocationBar, AFX_IDW_DOCKBAR_LEFT, &rc);

	// Merge frame also has a dockable bar at the very bottom
	// created in OnCreateClient 
	m_wndDetailBar.SetBarStyle(m_wndDetailBar.GetBarStyle() |
		CBRS_SIZE_DYNAMIC | CBRS_ALIGN_TOP);
	m_wndDetailBar.EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
	DockControlBar(&m_wndDetailBar, AFX_IDW_DOCKBAR_BOTTOM, &rc);

	// Merge frame also has a status bar at bottom, 
	// m_wndDetailBar is below, so we create this bar after m_wndDetailBar
	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}	

	// load docking positions and sizes
	CDockState pDockState;
	pDockState.LoadState(_T("Settings"));
	if (EnsureValidDockState(pDockState)) // checks for valid so won't ASSERT
		SetDockState(pDockState);
	// for the dimensions of the diff and location pane, use the CSizingControlBar loader
	m_wndLocationBar.LoadState(_T("Settings"));
	m_wndDetailBar.LoadState(_T("Settings"));

	return 0;
}

/**
 * @brief We must use this function before a call to SetDockState
 *
 * @note Without this, SetDockState will assert or crash if a bar from the 
 * CDockState is missing in the current CMergeEditFrame.
 * The bars are identified with their ID. This means the missing bar bug is triggered
 * when we run WinMerge after changing the ID of a bar. 
 */
bool CMergeEditFrame::EnsureValidDockState(CDockState& state) 
{
	for (int i = (int) state.m_arrBarInfo.GetSize()-1 ; i >= 0; i--) 
	{
		bool barIsCorrect = true;
		CControlBarInfo* pInfo = (CControlBarInfo*)state.m_arrBarInfo[i];
		if (pInfo == nullptr) 
			barIsCorrect = false;
		else
		{
			if (! pInfo->m_bFloating) 
			{
				pInfo->m_pBar = GetControlBar(pInfo->m_nBarID);
				if (pInfo->m_pBar == nullptr) 
					barIsCorrect = false; //toolbar id's probably changed	
			}
		}

		if (! barIsCorrect)
			state.m_arrBarInfo.RemoveAt(i);
	}
	return true;
}

void CMergeEditFrame::ActivateFrame(int nCmdShow) 
{
	CMergeFrameCommon::ActivateFrame(nCmdShow);
}

BOOL CMergeEditFrame::DestroyWindow() 
{
	SavePosition();
	SaveActivePane();
	SaveWindowState();
	return CMergeFrameCommon::DestroyWindow();
}

/**
 * @brief Save coordinates of the frame, splitters, and bars
 *
 * @note Do not save the maximized/restored state here. We are interested
 * in the state of the active frame, and maybe this frame is not active
 */
void CMergeEditFrame::SavePosition()
{
	// save the bars layout
	// save docking positions and sizes
	CDockState m_pDockState;
	GetDockState(m_pDockState);
	m_pDockState.SaveState(_T("Settings"));
	// for the dimensions of the diff pane, use the CSizingControlBar save
	m_wndLocationBar.SaveState(_T("Settings"));
	m_wndDetailBar.SaveState(_T("Settings"));
}

void CMergeEditFrame::SaveActivePane()
{
	for (int iRowParent = 0; iRowParent < m_wndSplitter.GetRowCount(); ++iRowParent)
	{
		int iRow, iCol;
		auto& splitterWnd = static_cast<CMergeEditSplitterView*>(m_wndSplitter.GetPane(iRowParent, 0))->m_wndSplitter;
		splitterWnd.GetActivePane(&iRow, &iCol);
		if (iRow >= 0 || iCol >= 0)
			GetOptionsMgr()->SaveOption(OPT_ACTIVE_PANE, max(iRow, iCol));
	}
}

void CMergeEditFrame::OnClose() 
{
	// clean up pointers.
	CMergeFrameCommon::OnClose();

	GetMainFrame()->ClearStatusbarItemCount();
}

/// update splitting position for panels 1/2 and headerbar and statusbar 
void CMergeEditFrame::UpdateHeaderSizes()
{
	if(!::IsWindow(m_wndFilePathBar.m_hWnd) || !::IsWindow(m_wndSplitter.m_hWnd))
		return;
	
	int w[3];
	int pane;
	CMergeDoc * pDoc = GetMergeDoc();
	auto& wndSplitter = GetMergeEditSplitterWnd(0);
	if (wndSplitter.GetColumnCount() > 1)
	{
		for (pane = 0; pane < wndSplitter.GetColumnCount(); pane++)
		{
			int wmin;
			wndSplitter.GetColumnInfo(pane, w[pane], wmin);
			if (w[pane]<1) w[pane]=1; // Perry 2003-01-22 (I don't know why this happens)
		}
	}
	else
	{
		CRect rect;
		wndSplitter.GetWindowRect(&rect);
		for (pane = 0; pane < pDoc->m_nBuffers; pane++)
		{
			w[pane] = rect.Width() /  pDoc->m_nBuffers;
		}
	}

	if (!std::equal(m_nLastSplitPos, m_nLastSplitPos + pDoc->m_nBuffers - 1, w))
	{
		std::copy_n(w, pDoc->m_nBuffers - 1, m_nLastSplitPos);

		// resize controls in header dialog bar
		m_wndFilePathBar.Resize(w);

		m_wndStatusBar.Resize(w);
	}
}

IHeaderBar * CMergeEditFrame::GetHeaderInterface()
{
	return &m_wndFilePathBar;
}

void CMergeEditFrame::UpdateAutoPaneResize()
{
	auto& wndSplitter = GetMergeEditSplitterWnd(0);
	wndSplitter.AutoResizePanes(GetOptionsMgr()->GetBool(OPT_RESIZE_PANES));
}

void CMergeEditFrame::UpdateSplitter()
{
	for (int iRow = 0; iRow < m_wndSplitter.GetRowCount(); ++iRow)
		GetMergeEditSplitterWnd(iRow).RecalcLayout();
	m_wndSplitter.RecalcLayout();
	m_pwndDetailMergeEditSplitterView->m_wndSplitter.RecalcLayout();
}

/**
 * @brief Synchronize control with splitter position,
 */
void CMergeEditFrame::OnIdleUpdateCmdUI()
{
	UpdateHeaderSizes();
	CMergeFrameCommon::OnIdleUpdateCmdUI();
}

void CMergeEditFrame::OnTimer(UINT_PTR nIDEvent) 
{
	if (nIDEvent == IDT_SAVEPOSITION)
	{
		SavePosition();
		KillTimer(IDT_SAVEPOSITION);
	}
	else
	{
		UpdateHeaderSizes();
	}
	CMergeFrameCommon::OnTimer(nIDEvent);
}

void CMergeEditFrame::OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd)
{
	CMergeFrameCommon::OnMDIActivate(bActivate, pActivateWnd, pDeactivateWnd);

	CMergeDoc *pDoc = GetMergeDoc();
	if (bActivate && pDoc != nullptr)
		this->GetParentFrame()->PostMessage(WM_USER+1);
	return;
}

/**
 * @brief Split panes vertically
 */
void CMergeEditFrame::OnViewSplitVertically() 
{
	auto& wndSplitter = GetMergeEditSplitterWnd(0);
	bool bSplitVertically = (wndSplitter.GetColumnCount() != 1);
	bSplitVertically = !bSplitVertically; // toggle
	GetOptionsMgr()->SaveOption(OPT_SPLIT_HORIZONTALLY, !bSplitVertically);
	for (int iRow = 0; iRow < m_wndSplitter.GetRowCount(); ++iRow)
		GetMergeEditSplitterWnd(iRow).FlipSplit();
	m_pwndDetailMergeEditSplitterView->m_wndSplitter.FlipSplit();
}

/**
 * @brief Update "Split Vertically" UI items
 */
void CMergeEditFrame::OnUpdateViewSplitVertically(CCmdUI* pCmdUI) 
{
	auto& wndSplitter = GetMergeEditSplitterWnd(0);
	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck((wndSplitter.GetColumnCount() != 1));
}

/// Document commanding us to close
void CMergeEditFrame::CloseNow()
{
	SavePosition(); // Save settings before closing!
	SaveActivePane();
	MDIActivate();
	MDIDestroy();
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CMergeEditFrame::UpdateResources()
{
	m_wndStatusBar.UpdateResources();
	m_wndLocationBar.UpdateResources();
	m_wndDetailBar.UpdateResources();
}

/**
 * @brief Save pane sizes and positions when one of panes requests it.
 */
LRESULT CMergeEditFrame::OnStorePaneSizes(WPARAM wParam, LPARAM lParam)
{
	KillTimer(IDT_SAVEPOSITION);
	SetTimer(IDT_SAVEPOSITION, 300, nullptr);
	return 0;
}

void CMergeEditFrame::OnSize(UINT nType, int cx, int cy) 
{
	__super::OnSize(nType, cx, cy);
	
	UpdateHeaderSizes();
}
