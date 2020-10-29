// OpenFrm.cpp : implementation of the COpenFrame class
//
#include "stdafx.h"
#include "OpenFrm.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "MergeFrameCommon.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// COpenFrame

IMPLEMENT_DYNCREATE(COpenFrame, CMergeFrameCommon)

BEGIN_MESSAGE_MAP(COpenFrame, CMergeFrameCommon)
	//{{AFX_MSG_MAP(COpenFrame)
	ON_WM_ERASEBKGND()
	ON_WM_NCHITTEST()
	ON_WM_WINDOWPOSCHANGING()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


// COpenFrame construction/destruction

COpenFrame::COpenFrame()
{
	// TODO: add member initialization code here
}

COpenFrame::~COpenFrame()
{
}


BOOL COpenFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying the CREATESTRUCT cs
	if( !__super::PreCreateWindow(cs) )
		return FALSE;
	cs.style |= WS_CLIPCHILDREN;
	return TRUE;
}

BOOL COpenFrame::OnEraseBkgnd(CDC* pDC)
{
	CRect rect;
	GetClientRect(&rect);
	pDC->FillSolidRect(&rect, ::GetSysColor(COLOR_APPWORKSPACE));
	return TRUE;
}

LRESULT COpenFrame::OnNcHitTest(CPoint point)
{
	switch (LRESULT const ht = CMDIChildWnd::OnNcHitTest(point))
	{
	case HTTOP:
	case HTBOTTOM:
	case HTLEFT:
	case HTTOPLEFT:
	case HTBOTTOMLEFT:
		return HTCAPTION;
	case HTTOPRIGHT:
	case HTBOTTOMRIGHT:
		return HTRIGHT;
	default:
		return ht;
	}
}

void COpenFrame::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
	// Retain frame sizes during tile operations (tolerate overlapping)
	if ((lpwndpos->flags & (SWP_NOSIZE | SWP_NOOWNERZORDER)) == 0 &&
	    !(IsZoomed() || GetMainFrame()->GetLayoutManager().GetTileLayoutEnabled()))
	{
		if (CScrollView *const pView = static_cast<CScrollView*>(GetActiveView()))
		{
			CRect rc;
			pView->GetWindowRect(&rc);
			CalcWindowRect(&rc, CWnd::adjustOutside);
			lpwndpos->cx = rc.Width();
			lpwndpos->cy = rc.Height();
		}
	}
}

void COpenFrame::ActivateFrame(int nCmdShow) 
{
	__super::ActivateFrame(nCmdShow);
	if (!GetMainFrame()->GetLayoutManager().GetTileLayoutEnabled())
	{
		if (CView* const pView = GetActiveView())
		{
			WINDOWPLACEMENT wp;
			wp.length = sizeof wp;
			GetWindowPlacement(&wp);
			CRect rc;
			pView->GetWindowRect(&rc);
			CalcWindowRect(&rc, CWnd::adjustOutside);
			wp.rcNormalPosition.right = wp.rcNormalPosition.left + rc.Width();
			wp.rcNormalPosition.bottom = wp.rcNormalPosition.top + rc.Height();
			SetWindowPlacement(&wp);
			pView->ShowWindow(SW_SHOW);
		}
	}
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void COpenFrame::UpdateResources()
{
}

/**
 * @brief Save the window's position, free related resources, and destroy the window
 */
BOOL COpenFrame::DestroyWindow() 
{
	SaveWindowState();
	return __super::DestroyWindow();
}

// COpenFrame message handlers
