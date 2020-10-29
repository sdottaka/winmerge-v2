/** 
 * @file  MergeFrameCommon.cpp
 *
 * @brief Implementation file for CMergeFrameCommon
 *
 */
#include "StdAfx.h"
#include "MergeFrameCommon.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "utils/DpiAware.h"
#include "Merge.h"
#include "MainFrm.h"
#include <../src/mfc/afximpl.h>

IMPLEMENT_DYNCREATE(CMergeFrameCommon, CMDIChildWnd)

BEGIN_MESSAGE_MAP(CMergeFrameCommon, DpiAware::CDpiAwareWnd<CMDIChildWnd>)
	//{{AFX_MSG_MAP(CMergeFrameCommon)
	ON_WM_GETMINMAXINFO()
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_MDIACTIVATE()
//	ON_MESSAGE(WM_GETICON, OnGetIcon)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMergeFrameCommon::CMergeFrameCommon(int nIdenticalIcon, int nDifferentIcon)
	: m_hIdentical(nIdenticalIcon < 0 ? nullptr : AfxGetApp()->LoadIcon(nIdenticalIcon))
	, m_hDifferent(nDifferentIcon < 0 ? nullptr : AfxGetApp()->LoadIcon(nDifferentIcon))
	, m_bActivated(false)
	, m_nLastSplitPos{0}
{
	::PostMessage(AfxGetMainWnd()->GetSafeHwnd(), WMU_CHILDFRAMEADDED, 0, reinterpret_cast<LPARAM>(this));
	GetMainFrame()->GetLayoutManager().NotifyChildOpened(this);
}

CMergeFrameCommon::~CMergeFrameCommon()
{
	::PostMessage(AfxGetMainWnd()->GetSafeHwnd(), WMU_CHILDFRAMEREMOVED, 0, reinterpret_cast<LPARAM>(this));
	GetMainFrame()->GetLayoutManager().NotifyChildClosed(this);
}

BOOL CMergeFrameCommon::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs
	MDITileLayout::LayoutManager& layoutManager = GetMainFrame()->GetLayoutManager();
	if (!layoutManager.GetTileLayoutEnabled())
		return __super::PreCreateWindow(cs);
	__super::PreCreateWindow(cs);
	cs.style &= ~WS_CAPTION;
	CRect rcMain;
	::GetWindowRect(GetMainFrame()->m_hWndMDIClient, rcMain);
	CRect rc = layoutManager.GetDefaultOpenPaneRect();
	rc = layoutManager.AdjustChildRect(rcMain, rc, cs.style, WS_EX_WINDOWEDGE | WS_EX_MDICHILD, GetMainFrame()->GetDpi());
	rc.right -= rcMain.left;
	rc.bottom -= rcMain.top;
	rc.left -= rcMain.left;
	rc.top -= rcMain.top;
	cs.x = rc.left;
	cs.y = rc.top;
	cs.cx = rc.Width();
	cs.cy = rc.Height();
	return true;
}

void CMergeFrameCommon::ActivateFrame(int nCmdShow)
{
	if (!m_bActivated) 
	{
		m_bActivated = true;

		// get the active child frame, and a flag whether it is maximized
		BOOL bMaximized = FALSE;
		CMDIChildWnd * oldActiveFrame = GetMDIFrame()->MDIGetActive(&bMaximized);
		if (oldActiveFrame == nullptr)
			// for the first frame, get the restored/maximized state from the registry
			bMaximized = GetOptionsMgr()->GetBool(OPT_ACTIVE_FRAME_MAX);
		if (bMaximized)
			nCmdShow = SW_SHOWMAXIMIZED;
		else
			nCmdShow = SW_SHOWNORMAL;
	}

	__super::ActivateFrame(nCmdShow);
}

void CMergeFrameCommon::SaveWindowState()
{
	// If we are active, save the restored/maximized state
	// If we are not, do nothing and let the active frame do the job.
 	if (GetParentFrame()->GetActiveFrame() == this)
	{
		WINDOWPLACEMENT wp;
		wp.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(&wp);
		GetOptionsMgr()->SaveOption(OPT_ACTIVE_FRAME_MAX, (wp.showCmd == SW_MAXIMIZE));
	}
}

void CMergeFrameCommon::RemoveBarBorder()
{
	afxData.cxBorder2 = 0;
	afxData.cyBorder2 = 0;
	for (int i = 0; i < 4; ++i)
	{
		CControlBar* pBar = GetControlBar(i + AFX_IDW_DOCKBAR_TOP);
		pBar->SetBarStyle(pBar->GetBarStyle() & ~(CBRS_BORDER_ANY | CBRS_BORDER_3D));
	}
}

/**
 * @brief Reflect comparison result in window's icon.
 * @param nResult [in] Last comparison result which the application returns.
 */
void CMergeFrameCommon::SetLastCompareResult(int nResult)
{
	HICON hCurrent = GetIcon(FALSE);
	HICON hReplace = (nResult == 0) ? m_hIdentical : m_hDifferent;

	if (hCurrent != hReplace)
	{
		SetIcon(hReplace, TRUE);

		AfxGetMainWnd()->SetTimer(IDT_UPDATEMAINMENU, 500, nullptr);
	}

	theApp.SetLastCompareResult(nResult);
}

void CMergeFrameCommon::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	__super::OnGetMinMaxInfo(lpMMI);
	// [Fix for MFC 8.0 MDI Maximizing Child Window bug on Vista]
	// https://groups.google.com/forum/#!topic/microsoft.public.vc.mfc/iajCdW5DzTM
#if _MFC_VER >= 0x0800
	lpMMI->ptMaxTrackSize.x = max(lpMMI->ptMaxTrackSize.x, lpMMI->ptMaxSize.x);
	lpMMI->ptMaxTrackSize.y = max(lpMMI->ptMaxTrackSize.y, lpMMI->ptMaxSize.y);
#endif
}

void CMergeFrameCommon::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	if (nType == SIZE_MAXIMIZED && IsDifferentDpiFromSystemDpi())
	{
		// This is a workaround of the problem that the maximized MDI child window is in the wrong position when the DPI changes
		// I don't think MDI-related processing inside Windows fully supports per-monitor dpi awareness
		CRect rc;
		GetParent()->GetClientRect(rc);
		AdjustWindowRectEx(&rc, GetStyle(), FALSE, GetExStyle());
		SetWindowPos(nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
	}
	GetMainFrame()->GetLayoutManager().NotifyChildResized(this);
}

void CMergeFrameCommon::OnDestroy()
{
	// https://stackoverflow.com/questions/35553955/getting-rid-of-3d-look-of-mdi-frame-window
	CFrameWnd::OnDestroy();
}

void CMergeFrameCommon::OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd)
{
	// call the base class to let standard processing switch to
	// the top-level menu associated with this window
	__super::OnMDIActivate(bActivate, pActivateWnd, pDeactivateWnd);

	if (bActivate)
		::PostMessage(AfxGetMainWnd()->GetSafeHwnd(), WMU_CHILDFRAMEACTIVATED, 0, reinterpret_cast<LPARAM>(this));
}
