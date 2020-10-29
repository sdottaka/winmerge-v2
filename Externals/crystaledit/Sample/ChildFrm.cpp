// ChildFrm.cpp : implementation of the CChildFrame class
//

#include "stdafx.h"
#include "Sample.h"

#include "ChildFrm.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, DpiAware::CDpiAwareWnd<CMDIChildWnd>)

BEGIN_MESSAGE_MAP(CChildFrame, DpiAware::CDpiAwareWnd<CMDIChildWnd>)
	//{{AFX_MSG_MAP(CChildFrame)
	ON_WM_SIZE()
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	// TODO: add member initialization code here
	
}

CChildFrame::~CChildFrame()
{
}

BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs
	CMainFrame* pMainFrame = static_cast<CMainFrame*>(AfxGetMainWnd());
	MDITileLayout::LayoutManager& layoutManager = pMainFrame->GetLayoutManager();
	if (!layoutManager.GetTileLayoutEnabled())
		return __super::PreCreateWindow(cs);
	__super::PreCreateWindow(cs);
	cs.style &= ~WS_CAPTION;
	CRect rcMain;
	::GetWindowRect(pMainFrame->m_hWndMDIClient, rcMain);
	CRect rc = layoutManager.GetDefaultOpenPaneRect();
	rc.left -= rcMain.left;
	rc.top -= rcMain.top;
	rc.right -= rcMain.left;
	rc.bottom -= rcMain.top;
	AdjustWindowRectEx(rc, cs.style, false, cs.dwExStyle);
	cs.x = rc.left;
	cs.y = rc.top;
	cs.cx = rc.Width();
	cs.cy = rc.Height();
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// CChildFrame diagnostics

#ifdef _DEBUG
void CChildFrame::AssertValid() const
{
	__super::AssertValid();
}

void CChildFrame::Dump(CDumpContext& dc) const
{
	__super::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CChildFrame message handlers

BOOL CChildFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{
	static_cast<CMainFrame*>(AfxGetMainWnd())->GetLayoutManager().NotifyChildOpened(this);
	return m_wndSplitter.Create(this, 2, 2, CSize(30, 30), pContext);
}

BOOL CChildFrame::DestroyWindow()
{
	static_cast<CMainFrame*>(AfxGetMainWnd())->GetLayoutManager().NotifyChildClosed(this);
	return __super::DestroyWindow();
}

void CChildFrame::OnSize(UINT nType, int cx, int cy)
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
	static_cast<CMainFrame*>(AfxGetMainWnd())->GetLayoutManager().NotifyChildResized(this);
}

