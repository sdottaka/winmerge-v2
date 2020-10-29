/////////////////////////////////////////////////////////////////////////
//
// CSizingControlBar            Version 2.44
//
// Created: Jan 24, 1998        Last Modified: March 31, 2002
//
// See the official site at www.datamekanix.com for documentation and
// the latest news.
//
/////////////////////////////////////////////////////////////////////////
// Copyright (C) 1998-2002 by Cristi Posea. All rights reserved.
//
// This code is free for personal and commercial use, providing this 
// notice remains intact in the source files and all eventual changes are
// clearly marked with comments.
//
// You must obtain the author's consent before you can include this code
// in a software library.
//
// No warrantee of any kind, express or implied, is included with this
// software; use at your own risk, responsibility for damages (if any) to
// anyone resulting from the use of this software rests entirely with the
// user.
//
// Send bug reports, bug fixes, enhancements, requests, flames, etc. to
// cristi@datamekanix.com or post them at the message board at the site.
//
// The sources and a short version of the docs are also available at
// www.codeproject.com . Look for a "Docking Windows" section and check
// the version to be sure you get the latest one ;)
//
// Hint: These classes are intended to be used as base classes. Do not
// simply add your code to these files - instead create a new class
// derived from one of CSizingControlBarXX classes and put there what
// you need. See CMyBar classes in the demo projects for examples.
// Modify this file only to fix bugs, and don't forget to send me a copy.
/////////////////////////////////////////////////////////////////////////
// Acknowledgements:
//  o   Thanks to Harlan R. Seymour for his continuous support during
//      development of this code.
//  o   Thanks to Dundas Software for the opportunity 
//      to test this code on real-life applications.
//  o   Some ideas for the gripper came from the CToolBarEx flat toolbar
//      by Joerg Koenig. Thanks, Joerg!
//  o   Thanks to Robert Wolpow for the code on which CDockContext based
//      dialgonal resizing is based.
//  o   Thanks to the following people for various bug fixes and/or
//      enhancements: Chris Maunder, Jakawan Ratiwanich, Udo Schaefer,
//      Anatoly Ivasyuk, Peter Hauptmann, DJ(?), Pat Kusbel, Aleksey
//      Malyshev.
//  o   And, of course, many thanks to all of you who used this code,
//      for the invaluable feedback I received.
/////////////////////////////////////////////////////////////////////////

// sizecbar.cpp : implementation file
//

#include "StdAfx.h"
#include "sizecbar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////
// CSizingControlBar

IMPLEMENT_DYNAMIC(CSizingControlBar, baseCSizingControlBar);

CSizingControlBar::CSizingControlBar()
{
}

CSizingControlBar::~CSizingControlBar()
{
}

BEGIN_MESSAGE_MAP(CSizingControlBar, DpiAware::CDpiAwareWnd<baseCSizingControlBar>)
    //{{AFX_MSG_MAP(CSizingControlBar)
    ON_WM_CREATE()
    ON_WM_PAINT()
    ON_WM_NCPAINT()
    ON_WM_NCCALCSIZE()
    ON_WM_WINDOWPOSCHANGING()
    ON_WM_CAPTURECHANGED()
    ON_WM_SETTINGCHANGE()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_NCLBUTTONDOWN()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_NCMOUSEMOVE()
    ON_WM_NCHITTEST()
    ON_WM_CLOSE()
    ON_WM_SIZE()
    //}}AFX_MSG_MAP
    ON_MESSAGE(WM_SETTEXT, OnSetText)
    ON_MESSAGE(WM_DPICHANGED_BEFOREPARENT, OnDpiChangedBeforeParent)
END_MESSAGE_MAP()

BOOL CSizingControlBar::Create(LPCTSTR lpszWindowName,
                               CWnd* pParentWnd, UINT nID,
                               DWORD dwStyle)
{
    // must have a parent
    ASSERT_VALID(pParentWnd);
    // cannot be both fixed and dynamic
    // (CBRS_SIZE_DYNAMIC is used for resizng when floating)
    ASSERT (!((dwStyle & CBRS_SIZE_FIXED) &&
              (dwStyle & CBRS_SIZE_DYNAMIC)));

    m_dwStyle = dwStyle & CBRS_ALL; // save the control bar styles

    // register and create the window - skip CControlBar::Create()
    CString wndclass = ::AfxRegisterWndClass(CS_DBLCLKS,
        ::LoadCursor(nullptr, IDC_ARROW),
        ::GetSysColorBrush(COLOR_BTNFACE), 0);

    dwStyle &= ~CBRS_ALL; // keep only the generic window styles
    dwStyle |= WS_CLIPCHILDREN; // prevents flashing
    if (!CWnd::Create(wndclass, lpszWindowName, dwStyle,
        CRect(0, 0, 0, 0), pParentWnd, nID))
        return FALSE;

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////
// CSizingControlBar operations
#if defined(_SCB_REPLACE_MINIFRAME) && !defined(_SCB_MINIFRAME_CAPTION)
void CSizingControlBar::EnableDocking(DWORD dwDockStyle)
{
    // must be CBRS_ALIGN_XXX or CBRS_FLOAT_MULTI only
    ASSERT((dwDockStyle & ~(CBRS_ALIGN_ANY|CBRS_FLOAT_MULTI)) == 0);
    // cannot have the CBRS_FLOAT_MULTI style
    ASSERT((dwDockStyle & CBRS_FLOAT_MULTI) == 0);
    // the bar must have CBRS_SIZE_DYNAMIC style
    ASSERT((m_dwStyle & CBRS_SIZE_DYNAMIC) != 0);

    m_dwDockStyle = dwDockStyle;
    if (m_pDockContext == nullptr)
        m_pDockContext = new CSCBDockContext(this);

    // permanently wire the bar's owner to its current parent
    if (m_hWndOwner == nullptr)
        m_hWndOwner = ::GetParent(m_hWnd);
}
#endif

/////////////////////////////////////////////////////////////////////////
// CSizingControlBar message handlers

int CSizingControlBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
    if (__super::OnCreate(lpCreateStruct) == -1)
        return -1;

    // query SPI_GETDRAGFULLWINDOWS system parameter
    // OnSettingChange() will update m_bDragShowContent
    BOOL bDragShowContent = m_bDragShowContent = false;
    ::SystemParametersInfo(SPI_GETDRAGFULLWINDOWS, 0,
        &bDragShowContent, 0);
    m_bDragShowContent = !!bDragShowContent;

    // uncomment this line if you want raised borders
//    m_dwSCBStyle |= SCBS_SHOWEDGES;

    return 0;
}


LRESULT CSizingControlBar::OnSetText(WPARAM wParam, LPARAM lParam)
{
    UNUSED_ALWAYS(wParam);

    LRESULT lResult = CWnd::Default();

    if (IsFloating() &&
        GetParentFrame()->IsKindOf(RUNTIME_CLASS(CMiniDockFrameWnd)))
    {
        m_pDockBar->SetWindowText((LPCTSTR) lParam); // update dockbar
        GetParentFrame()->DelayRecalcLayout(); // refresh miniframe
    }

    return lResult;
}

const bool CSizingControlBar::IsFloating() const
{
    return !IsHorzDocked() && !IsVertDocked();
}

const bool CSizingControlBar::IsHorzDocked() const
{
    return (m_nDockBarID == AFX_IDW_DOCKBAR_TOP ||
        m_nDockBarID == AFX_IDW_DOCKBAR_BOTTOM);
}

const bool CSizingControlBar::IsVertDocked() const
{
    return (m_nDockBarID == AFX_IDW_DOCKBAR_LEFT ||
        m_nDockBarID == AFX_IDW_DOCKBAR_RIGHT);
}

const bool CSizingControlBar::IsSideTracking() const
{
    // don't call this when not tracking
    ASSERT(m_bTracking && !IsFloating());

    return (m_htEdge == HTLEFT || m_htEdge == HTRIGHT) ?
        IsHorzDocked() : IsVertDocked();
}

CSize CSizingControlBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
    if (bStretch) // the bar is stretched (is not the child of a dockbar)
        if (bHorz)
            return CSize(32767, m_szHorz.cy);
        else
            return CSize(m_szVert.cx, 32767);

    // dirty cast - we need access to protected CDockBar members
    CSCBDockBar* pDockBar = static_cast<CSCBDockBar*>(m_pDockBar);

    // force imediate RecalcDelayShow() for all sizing bars on the row
    // with delayShow/delayHide flags set to avoid IsVisible() problems
    CSCBArray arrSCBars;
    GetRowSizingBars(arrSCBars);
    AFX_SIZEPARENTPARAMS layout;
    layout.hDWP = pDockBar->m_bLayoutQuery ?
        nullptr : ::BeginDeferWindowPos(static_cast<int>(arrSCBars.GetSize()));
    for (int i = 0; i < arrSCBars.GetSize(); i++)
        if (arrSCBars[i]->m_nStateFlags & (delayHide|delayShow))
            arrSCBars[i]->RecalcDelayShow(&layout);
    if (layout.hDWP != nullptr)
        ::EndDeferWindowPos(layout.hDWP);

    // get available length
    CRect rc = pDockBar->m_rectLayout;
    if (rc.IsRectEmpty())
        m_pDockSite->GetClientRect(&rc);
    int nLengthTotal = bHorz ? rc.Width() + 2 : rc.Height() - 2;

    if (IsVisible() && !IsFloating() &&
        m_bParentSizing && arrSCBars[0] == this)
        if (NegotiateSpace(nLengthTotal, (!!bHorz)))
            AlignControlBars();

    m_bParentSizing = false;

    if (bHorz)
        return CSize(max(m_szMinHorz.cx, m_szHorz.cx),
                     max(m_szMinHorz.cy, m_szHorz.cy));

    return CSize(max(m_szMinVert.cx, m_szVert.cx),
                 max(m_szMinVert.cy, m_szVert.cy));
}

CSize CSizingControlBar::CalcDynamicLayout(int nLength, DWORD dwMode)
{
    if (dwMode & (LM_HORZDOCK | LM_VERTDOCK)) // docked ?
    {
        if (nLength == -1)
            m_bParentSizing = true;

        return __super::CalcDynamicLayout(nLength, dwMode);
    }

    if (dwMode & LM_MRUWIDTH) return m_szFloat;
    if (dwMode & LM_COMMIT) return m_szFloat; // already committed

#ifndef _SCB_REPLACE_MINIFRAME
    // check for dialgonal resizing hit test
    int nHitTest = m_pDockContext->m_nHitTest;
    if (IsFloating() &&
        (nHitTest == HTTOPLEFT || nHitTest == HTBOTTOMLEFT ||
        nHitTest == HTTOPRIGHT || nHitTest == HTBOTTOMRIGHT))
    {
        CPoint ptCursor;
        ::GetCursorPos(&ptCursor);

        CRect rFrame, rBar;
        GetParentFrame()->GetWindowRect(&rFrame);
        GetWindowRect(&rBar);
        
        if (nHitTest == HTTOPLEFT || nHitTest == HTBOTTOMLEFT)
        {
            m_szFloat.cx = rFrame.left + rBar.Width() - ptCursor.x;
            m_pDockContext->m_rectFrameDragHorz.left = 
                min(ptCursor.x, rFrame.left + rBar.Width() - m_szMinFloat.cx);
        }

        if (nHitTest == HTTOPLEFT || nHitTest == HTTOPRIGHT)
        {
            m_szFloat.cy = rFrame.top + rBar.Height() - ptCursor.y;
            m_pDockContext->m_rectFrameDragHorz.top =
                min(ptCursor.y, rFrame.top + rBar.Height() - m_szMinFloat.cy);
        }

        if (nHitTest == HTTOPRIGHT || nHitTest == HTBOTTOMRIGHT)
            m_szFloat.cx = rBar.Width() + ptCursor.x - rFrame.right;

        if (nHitTest == HTBOTTOMLEFT || nHitTest == HTBOTTOMRIGHT)
            m_szFloat.cy = rBar.Height() + ptCursor.y - rFrame.bottom;
    }
    else
#endif //_SCB_REPLACE_MINIFRAME
        ((dwMode & LM_LENGTHY) ? m_szFloat.cy : m_szFloat.cx) = nLength;

    m_szFloat.cx = max(m_szFloat.cx, m_szMinFloat.cx);
    m_szFloat.cy = max(m_szFloat.cy, m_szMinFloat.cy);

    return m_szFloat;
}

void CSizingControlBar::OnWindowPosChanging(WINDOWPOS FAR* lpwndpos)
{
    // force non-client recalc if moved or resized
    lpwndpos->flags |= SWP_FRAMECHANGED;

    __super::OnWindowPosChanging(lpwndpos);

    // find on which side are we docked
    m_nDockBarID = GetParent()->GetDlgCtrlID();

    if (!IsFloating())
        if (lpwndpos->flags & SWP_SHOWWINDOW)
            m_bKeepSize = true;
}

/////////////////////////////////////////////////////////////////////////
// Mouse Handling
//
void CSizingControlBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (m_pDockBar != nullptr)
    {
        // start the drag
        ASSERT(m_pDockContext != nullptr);
        ClientToScreen(&point);
        m_pDockContext->StartDrag(point);
    }
    else
        CWnd::OnLButtonDown(nFlags, point);
}

void CSizingControlBar::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    if (m_pDockBar != nullptr)
    {
        // toggle docking
        ASSERT(m_pDockContext != nullptr);
        m_pDockContext->ToggleDocking();
    }
    else
        CWnd::OnLButtonDblClk(nFlags, point);
}

void CSizingControlBar::OnNcLButtonDown(UINT nHitTest, CPoint point) 
{
    UNUSED_ALWAYS(point);

    if (m_bTracking || IsFloating())
        return;

    if ((nHitTest >= HTSIZEFIRST) && (nHitTest <= HTSIZELAST))
        StartTracking(nHitTest, point); // sizing edge hit
}

void CSizingControlBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bTracking)
        StopTracking();

    __super::OnLButtonUp(nFlags, point);
}

void CSizingControlBar::OnRButtonDown(UINT nFlags, CPoint point)
{
    if (m_bTracking)
        StopTracking();

    __super::OnRButtonDown(nFlags, point);
}

void CSizingControlBar::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bTracking)
    {
        CPoint ptScreen = point;
        ClientToScreen(&ptScreen);

        OnTrackUpdateSize(ptScreen);
    }

    __super::OnMouseMove(nFlags, point);
}

void CSizingControlBar::OnCaptureChanged(CWnd *pWnd)
{
    if (m_bTracking && (pWnd != this))
        StopTracking();

    __super::OnCaptureChanged(pWnd);
}

void CSizingControlBar::OnNcCalcSize(BOOL bCalcValidRects,
                                     NCCALCSIZE_PARAMS FAR* lpncsp)
{
    UNUSED_ALWAYS(bCalcValidRects);

#ifndef _SCB_REPLACE_MINIFRAME
    // Enable diagonal resizing for floating miniframe
    if (IsFloating())
    {
        CFrameWnd* pFrame = GetParentFrame();
        if (pFrame != nullptr &&
            pFrame->IsKindOf(RUNTIME_CLASS(CMiniFrameWnd)))
        {
            DWORD dwStyle = ::GetWindowLong(pFrame->m_hWnd, GWL_STYLE);
            if ((dwStyle & MFS_4THICKFRAME) != 0)
            {
                pFrame->ModifyStyle(MFS_4THICKFRAME, 0); // clear
                GetParent()->ModifyStyle(0, WS_CLIPCHILDREN);
            }
        }
    }
#endif _SCB_REPLACE_MINIFRAME

    // compute the the client area
    m_dwSCBStyle &= ~SCBS_EDGEALL;

    // add resizing edges between bars on the same row
    if (!IsFloating() && m_pDockBar != nullptr)
    {
        CSCBArray arrSCBars;
        int nThis;
        GetRowSizingBars(arrSCBars, nThis);

        bool bHorz = IsHorzDocked();
        if (nThis > 0)
            m_dwSCBStyle |= bHorz ? SCBS_EDGELEFT : SCBS_EDGETOP;

        if (nThis < arrSCBars.GetUpperBound())
            m_dwSCBStyle |= bHorz ? SCBS_EDGERIGHT : SCBS_EDGEBOTTOM;
    }

    NcCalcClient(&lpncsp->rgrc[0], m_nDockBarID);
}

void CSizingControlBar::NcCalcClient(LPRECT pRc, UINT nDockBarID)
{
    CRect rc(pRc);

    rc.DeflateRect(3, 5, 3, 3);
    if (nDockBarID != AFX_IDW_DOCKBAR_FLOAT)
        rc.DeflateRect(2, 0, 2, 2);

    switch(nDockBarID)
    {
    case AFX_IDW_DOCKBAR_TOP:
        m_dwSCBStyle |= SCBS_EDGEBOTTOM;
        break;
    case AFX_IDW_DOCKBAR_BOTTOM:
        m_dwSCBStyle |= SCBS_EDGETOP;
        break;
    case AFX_IDW_DOCKBAR_LEFT:
        m_dwSCBStyle |= SCBS_EDGERIGHT;
        break;
    case AFX_IDW_DOCKBAR_RIGHT:
        m_dwSCBStyle |= SCBS_EDGELEFT;
        break;
    }

    // make room for edges only if they will be painted
    if (m_dwSCBStyle & SCBS_SHOWEDGES)
        rc.DeflateRect(
            (m_dwSCBStyle & SCBS_EDGELEFT)   != 0 ? m_cxEdge : 0,
            (m_dwSCBStyle & SCBS_EDGETOP)    != 0 ? m_cxEdge : 0,
            (m_dwSCBStyle & SCBS_EDGERIGHT)  != 0 ? m_cxEdge : 0,
            (m_dwSCBStyle & SCBS_EDGEBOTTOM) != 0 ? m_cxEdge : 0);

    *pRc = rc;
}

void CSizingControlBar::OnNcPaint()
{
    // get window DC that is clipped to the non-client area
    CWindowDC dc(this); // the HDC will be released by the destructor

    CRect rcClient, rcBar;
    GetClientRect(rcClient);
    ClientToScreen(rcClient);
    GetWindowRect(rcBar);
    rcClient.OffsetRect(-rcBar.TopLeft());
    rcBar.OffsetRect(-rcBar.TopLeft());

    CDC mdc;
    mdc.CreateCompatibleDC(&dc);
    
    CBitmap bm;
    bm.CreateCompatibleBitmap(&dc, rcBar.Width(), rcBar.Height());
    CBitmap* pOldBm = mdc.SelectObject(&bm);

    // draw borders in non-client area
    CRect rcDraw = rcBar;
    DrawBorders(&mdc, rcDraw);

    // erase the NC background
    mdc.FillRect(rcDraw, CBrush::FromHandle(
        (HBRUSH) GetClassLongPtr(m_hWnd, GCLP_HBRBACKGROUND)));

    if ((m_dwSCBStyle & SCBS_SHOWEDGES) != 0)
    {
        CRect rcEdge; // paint the sizing edges
        for (int i = 0; i < 4; i++)
            if (GetEdgeRect(rcBar, GetEdgeHTCode(i), rcEdge))
                mdc.Draw3dRect(rcEdge, ::GetSysColor(COLOR_BTNHIGHLIGHT),
                    ::GetSysColor(COLOR_BTNSHADOW));
    }

    NcPaintGripper(&mdc, rcClient);

    // client area is not our bussiness :)
    dc.IntersectClipRect(rcBar);
    dc.ExcludeClipRect(rcClient);

    dc.BitBlt(0, 0, rcBar.Width(), rcBar.Height(), &mdc, 0, 0, SRCCOPY);

    mdc.SelectObject(pOldBm);
}

void CSizingControlBar::NcPaintGripper(CDC* pDC, CRect rcClient)
{
    UNUSED_ALWAYS(pDC);
    UNUSED_ALWAYS(rcClient);
}

void CSizingControlBar::OnPaint()
{
    // overridden to skip border painting based on clientrect
    CPaintDC dc(this);
}

NCHITTEST_RESULT CSizingControlBar::OnNcHitTest(CPoint point)
{
    CRect rcBar, rcEdge;
    GetWindowRect(rcBar);

    if (!IsFloating())
        for (int i = 0; i < 4; i++)
            if (GetEdgeRect(rcBar, GetEdgeHTCode(i), rcEdge))
                if (rcEdge.PtInRect(point))
                    return GetEdgeHTCode(i);

    return HTCLIENT;
}

void CSizingControlBar::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
    __super::OnSettingChange(uFlags, lpszSection);

    BOOL bDragShowContent = m_bDragShowContent = false;
    ::SystemParametersInfo(SPI_GETDRAGFULLWINDOWS, 0,
        &bDragShowContent, 0); // update
    m_bDragShowContent = !!bDragShowContent;
}

void CSizingControlBar::OnSize(UINT nType, int cx, int cy)
{
    UNUSED_ALWAYS(nType);
    
    if ((m_dwSCBStyle & SCBS_SIZECHILD) != 0)
    {
        // automatic child resizing - only one child is allowed
        CWnd* pWnd = GetWindow(GW_CHILD);
        if (pWnd != nullptr)
        {
            pWnd->MoveWindow(0, 0, cx, cy);
            ASSERT(pWnd->GetWindow(GW_HWNDNEXT) == nullptr);
        }
    }
}

void CSizingControlBar::OnClose()
{
    // do nothing: protection against accidentally destruction by the
    //   child control (i.e. if user hits Esc in a child editctrl)
}

/////////////////////////////////////////////////////////////////////////
// CSizingControlBar implementation helpers

void CSizingControlBar::StartTracking(UINT nHitTest, CPoint point)
{
    SetCapture();

    // make sure no updates are pending
    if (!m_bDragShowContent)
        RedrawWindow(nullptr, nullptr, RDW_ALLCHILDREN | RDW_UPDATENOW);

    m_htEdge = nHitTest;
    m_bTracking = true;

    bool bHorz = IsHorzDocked();
    bool bHorzTracking = m_htEdge == HTLEFT || m_htEdge == HTRIGHT;

    m_nTrackPosOld = bHorzTracking ? point.x : point.y;

    CRect rcBar, rcEdge;
    GetWindowRect(rcBar);
    GetEdgeRect(rcBar, m_htEdge, rcEdge);
    m_nTrackEdgeOfs = m_nTrackPosOld -
        (bHorzTracking ? rcEdge.CenterPoint().x : rcEdge.CenterPoint().y);
    
    CSCBArray arrSCBars;
    int nThis;
    GetRowSizingBars(arrSCBars, nThis);

    m_nTrackPosMin = m_nTrackPosMax = m_nTrackPosOld;
    if (!IsSideTracking())
    {
        // calc minwidth as the max minwidth of the sizing bars on row
        int nMinWidth = bHorz ? m_szMinHorz.cy : m_szMinVert.cx;
        for (int i = 0; i < arrSCBars.GetSize(); i++)
            nMinWidth = max(nMinWidth, bHorz ? 
                arrSCBars[i]->m_szMinHorz.cy :
                arrSCBars[i]->m_szMinVert.cx);
        int nExcessWidth = (bHorz ? m_szHorz.cy : m_szVert.cx) - nMinWidth;

        // the control bar cannot grow with more than the width of
        // remaining client area of the mainframe
        CRect rcT;
        m_pDockSite->RepositionBars(0, 0xFFFF, AFX_IDW_PANE_FIRST,
            reposQuery, &rcT, nullptr, TRUE);
        int nMaxWidth = bHorz ? rcT.Height() - 2 : rcT.Width() - 2;

        bool bTopOrLeft = m_htEdge == HTTOP || m_htEdge == HTLEFT;

        m_nTrackPosMin -= bTopOrLeft ? nMaxWidth : nExcessWidth;
        m_nTrackPosMax += bTopOrLeft ? nExcessWidth : nMaxWidth;
    }
    else
    {
        // side tracking:
        // max size is the actual size plus the amount the other
        // sizing bars can be decreased until they reach their minsize
        if (m_htEdge == HTBOTTOM || m_htEdge == HTRIGHT)
            nThis++;

        for (int i = 0; i < arrSCBars.GetSize(); i++)
        {
            CSizingControlBar* pBar = arrSCBars[i];

            int nExcessWidth = bHorz ? 
                pBar->m_szHorz.cx - pBar->m_szMinHorz.cx :
                pBar->m_szVert.cy - pBar->m_szMinVert.cy;

            if (i < nThis)
                m_nTrackPosMin -= nExcessWidth;
            else
                m_nTrackPosMax += nExcessWidth;
        }
    }

    OnTrackInvertTracker(); // draw tracker
}

void CSizingControlBar::StopTracking()
{
    OnTrackInvertTracker(); // erase tracker

    m_bTracking = false;
    ReleaseCapture();

    m_pDockSite->DelayRecalcLayout();
}

void CSizingControlBar::OnTrackUpdateSize(CPoint& point)
{
    ASSERT(!IsFloating());

    bool bHorzTrack = m_htEdge == HTLEFT || m_htEdge == HTRIGHT;

    int nTrackPos = bHorzTrack ? point.x : point.y;
    nTrackPos = max(m_nTrackPosMin, min(m_nTrackPosMax, nTrackPos));

    int nDelta = nTrackPos - m_nTrackPosOld;

    if (nDelta == 0)
        return; // no pos change

    OnTrackInvertTracker(); // erase tracker

    m_nTrackPosOld = nTrackPos;
    
    bool bHorz = IsHorzDocked();

    CSize sizeNew = bHorz ? m_szHorz : m_szVert;
    switch (m_htEdge)
    {
    case HTLEFT:    sizeNew -= CSize(nDelta, 0); break;
    case HTTOP:     sizeNew -= CSize(0, nDelta); break;
    case HTRIGHT:   sizeNew += CSize(nDelta, 0); break;
    case HTBOTTOM:  sizeNew += CSize(0, nDelta); break;
    }

    CSCBArray arrSCBars;
    int nThis;
    GetRowSizingBars(arrSCBars, nThis);

    if (!IsSideTracking())
        for (int i = 0; i < arrSCBars.GetSize(); i++)
        {
            CSizingControlBar* pBar = arrSCBars[i];
            // make same width (or height)
            (bHorz ? pBar->m_szHorz.cy : pBar->m_szVert.cx) =
                bHorz ? sizeNew.cy : sizeNew.cx;
        }
    else
    {
        int nGrowingBar = nThis;
        bool bBefore = m_htEdge == HTTOP || m_htEdge == HTLEFT;
        if (bBefore && nDelta > 0)
            nGrowingBar--;
        if (!bBefore && nDelta < 0)
            nGrowingBar++;
        if (nGrowingBar != nThis)
            bBefore = !bBefore;

        // nGrowing is growing
        nDelta = abs(nDelta);
        CSizingControlBar* pBar = arrSCBars[nGrowingBar];
        (bHorz ? pBar->m_szHorz.cx : pBar->m_szVert.cy) += nDelta;

        // the others are shrinking
        int nFirst = bBefore ? nGrowingBar - 1 : nGrowingBar + 1;
        int nLimit = bBefore ? -1 : static_cast<int>(arrSCBars.GetSize());

        for (int i = nFirst; nDelta != 0 && i != nLimit; i += (bBefore ? -1 : 1))
        {
            CSizingControlBar* pBar1 = arrSCBars[i];
                
            int nDeltaT = min(nDelta,
                (bHorz ? pBar1->m_szHorz.cx : pBar1->m_szVert.cy) -
                (bHorz ? pBar1->m_szMinHorz.cx : pBar1->m_szMinVert.cy));

            (bHorz ? pBar1->m_szHorz.cx : pBar1->m_szVert.cy) -= nDeltaT;
            nDelta -= nDeltaT;
        }
    }

    OnTrackInvertTracker(); // redraw tracker at new pos

    if (m_bDragShowContent)
        m_pDockSite->DelayRecalcLayout();
}

void CSizingControlBar::OnTrackInvertTracker()
{
    ASSERT(m_bTracking);

    if (m_bDragShowContent)
        return; // don't show tracker if DragFullWindows is on

    bool bHorz = IsHorzDocked();
    CRect rc, rcBar, rcDock, rcFrame;
    GetWindowRect(rcBar);
    m_pDockBar->GetWindowRect(rcDock);
    m_pDockSite->GetWindowRect(rcFrame);
    VERIFY(GetEdgeRect(rcBar, m_htEdge, rc));
    if (!IsSideTracking())
        rc = bHorz ? 
            CRect(rcDock.left + 1, rc.top, rcDock.right - 1, rc.bottom) :
            CRect(rc.left, rcDock.top + 1, rc.right, rcDock.bottom - 1);

    bool bHorzTracking = m_htEdge == HTLEFT || m_htEdge == HTRIGHT;
    int nOfs = m_nTrackPosOld - m_nTrackEdgeOfs;
    nOfs -= bHorzTracking ? rc.CenterPoint().x : rc.CenterPoint().y;
    rc.OffsetRect(bHorzTracking ? nOfs : 0, bHorzTracking ? 0 : nOfs);
    rc.OffsetRect(-rcFrame.TopLeft());

    CDC *pDC = m_pDockSite->GetDCEx(nullptr,
        DCX_WINDOW | DCX_CACHE | DCX_LOCKWINDOWUPDATE);
    CBrush* pBrush = CDC::GetHalftoneBrush();
    CBrush* pBrushOld = pDC->SelectObject(pBrush);

    pDC->PatBlt(rc.left, rc.top, rc.Width(), rc.Height(), PATINVERT);
    
    pDC->SelectObject(pBrushOld);
    m_pDockSite->ReleaseDC(pDC);
}

bool CSizingControlBar::GetEdgeRect(CRect rcWnd, UINT nHitTest,
                                    CRect& rcEdge)
{
    rcEdge = rcWnd;
    if ((m_dwSCBStyle & SCBS_SHOWEDGES) != 0)
        rcEdge.DeflateRect(1, 1);
    bool bHorz = IsHorzDocked();

    switch (nHitTest)
    {
    case HTLEFT:
        if ((m_dwSCBStyle & SCBS_EDGELEFT) == 0) 
			return false;
        rcEdge.right = rcEdge.left + m_cxEdge;
        rcEdge.DeflateRect(0, bHorz ? m_cxEdge: 0);
        break;
    case HTTOP:
        if ((m_dwSCBStyle & SCBS_EDGETOP) == 0) 
			return false;
        rcEdge.bottom = rcEdge.top + m_cxEdge;
        rcEdge.DeflateRect(bHorz ? 0 : m_cxEdge, 0);
        break;
    case HTRIGHT:
        if ((m_dwSCBStyle & SCBS_EDGERIGHT) == 0) 
			return false;
        rcEdge.left = rcEdge.right - m_cxEdge;
        rcEdge.DeflateRect(0, bHorz ? m_cxEdge: 0);
        break;
    case HTBOTTOM:
        if ((m_dwSCBStyle & SCBS_EDGEBOTTOM) == 0) 
			return false;
        rcEdge.top = rcEdge.bottom - m_cxEdge;
        rcEdge.DeflateRect(bHorz ? 0 : m_cxEdge, 0);
        break;
    default:
        ASSERT(false); // invalid hit test code
    }
    return true;
}

UINT CSizingControlBar::GetEdgeHTCode(int nEdge)
{
    if (nEdge == 0) return HTLEFT;
    if (nEdge == 1) return HTTOP;
    if (nEdge == 2) return HTRIGHT;
    if (nEdge == 3) return HTBOTTOM;
    ASSERT(false); // invalid edge code
    return HTNOWHERE;
}

void CSizingControlBar::GetRowInfo(int& nFirst, int& nLast, int& nThis)
{
    ASSERT_VALID(m_pDockBar); // verify bounds

    nThis = m_pDockBar->FindBar(this);
    ASSERT(nThis != -1);

    int i, nBars = static_cast<int>(m_pDockBar->m_arrBars.GetSize());

    // find the first and the last bar in row
    for (nFirst = -1, i = nThis - 1; i >= 0 && nFirst == -1; i--)
        if (m_pDockBar->m_arrBars[i] == nullptr)
            nFirst = i + 1;
    for (nLast = -1, i = nThis + 1; i < nBars && nLast == -1; i++)
        if (m_pDockBar->m_arrBars[i] == nullptr)
            nLast = i - 1;

    ASSERT((nLast != -1) && (nFirst != -1));
}

void CSizingControlBar::GetRowSizingBars(CSCBArray& arrSCBars)
{
    int nThis; // dummy
    GetRowSizingBars(arrSCBars, nThis);
}

void CSizingControlBar::GetRowSizingBars(CSCBArray& arrSCBars, int& nThis)
{
    arrSCBars.RemoveAll();

    int nFirstT, nLastT, nThisT;
    GetRowInfo(nFirstT, nLastT, nThisT);

    nThis = -1;
    for (int i = nFirstT; i <= nLastT; i++)
    {
        CSizingControlBar* pBar =
            static_cast<CSizingControlBar*>(m_pDockBar->m_arrBars[i]);
        if (HIWORD(pBar) == 0) continue; // placeholder
        if (!pBar->IsVisible()) continue;
        if (pBar->IsKindOf(RUNTIME_CLASS(CSizingControlBar)))
        {
            if (pBar == this)
                nThis = static_cast<int>(arrSCBars.GetSize());

            arrSCBars.Add(pBar);
        }
    }
}

bool CSizingControlBar::NegotiateSpace(int nLengthTotal, bool bHorz)
{
    ASSERT(bHorz == IsHorzDocked());

    int nFirst, nLast, nThis;
    GetRowInfo(nFirst, nLast, nThis);

    int nLengthAvail = nLengthTotal;
    int nLengthActual = 0;
    int nLengthMin = 2;
    int nWidthMax = 0;
    CSizingControlBar* pBar;

    int i=0;
    for (i = nFirst; i <= nLast; i++)
    {
        pBar = static_cast<CSizingControlBar*>(m_pDockBar->m_arrBars[i]);
        if (HIWORD(pBar) == 0) continue; // placeholder
        if (!pBar->IsVisible()) continue;
        bool bIsSizingBar = 
            !!pBar->IsKindOf(RUNTIME_CLASS(CSizingControlBar));

        int nLengthBar; // minimum length of the bar
        if (bIsSizingBar)
            nLengthBar = bHorz ? pBar->m_szMinHorz.cx - 2 :
                pBar->m_szMinVert.cy - 2;
        else
        {
            CRect rcBar;
            pBar->GetWindowRect(&rcBar);
            nLengthBar = bHorz ? rcBar.Width() - 2 : rcBar.Height() - 2;
        }

        nLengthMin += nLengthBar;
        if (nLengthMin > nLengthTotal)
        {
            // split the row after fixed bar
            if (i < nThis)
            {
                m_pDockBar->m_arrBars.InsertAt(i + 1,
                    (CControlBar*) nullptr);
                return false;
            }
            
            // only this sizebar remains on the row, adjust it to minsize
            if (i == nThis)
            {
                if (bHorz)
                    m_szHorz.cx = m_szMinHorz.cx;
                else
                    m_szVert.cy = m_szMinVert.cy;

                return true; // the dockbar will split the row for us
            }

            // we have enough bars - go negotiate with them
            m_pDockBar->m_arrBars.InsertAt(i, (CControlBar*) nullptr);
            nLast = i - 1;
            break;
        }

        if (bIsSizingBar)
        {
            nLengthActual += bHorz ? pBar->m_szHorz.cx - 2 : 
                pBar->m_szVert.cy - 2;
            nWidthMax = max(nWidthMax, bHorz ? pBar->m_szHorz.cy :
                pBar->m_szVert.cx);
        }
        else
            nLengthAvail -= nLengthBar;
    }

    CSCBArray arrSCBars;
    GetRowSizingBars(arrSCBars);
    int nNumBars = static_cast<int>(arrSCBars.GetSize());
    int nDelta = nLengthAvail - nLengthActual;

    // return faster when there is only one sizing bar per row (this one)
    if (nNumBars == 1)
    {
        ASSERT(arrSCBars[0] == this);

        if (nDelta == 0)
            return true;
        
        m_bKeepSize = false;
        (bHorz ? m_szHorz.cx : m_szVert.cy) += nDelta;

        return true;
    }

    // make all the bars the same width
    for (i = 0; i < nNumBars; i++)
        if (bHorz)
            arrSCBars[i]->m_szHorz.cy = nWidthMax;
        else
            arrSCBars[i]->m_szVert.cx = nWidthMax;

    // distribute the difference between the bars,
    // but don't shrink them below their minsizes
    while (nDelta != 0)
    {
        int nDeltaOld = nDelta;
        for (i = 0; i < nNumBars; i++)
        {
            pBar = arrSCBars[i];
            int nLMin = bHorz ?
                pBar->m_szMinHorz.cx : pBar->m_szMinVert.cy;
            int nL = bHorz ? pBar->m_szHorz.cx : pBar->m_szVert.cy;

            if ((nL == nLMin) && (nDelta < 0) || // already at min length
                pBar->m_bKeepSize) // or wants to keep its size
                continue;

            // sign of nDelta
            int nDelta2 = (nDelta == 0) ? 0 : ((nDelta < 0) ? -1 : 1);

            (bHorz ? pBar->m_szHorz.cx : pBar->m_szVert.cy) += nDelta2;
            nDelta -= nDelta2;
            if (nDelta == 0) break;
        }
        // clear m_bKeepSize flags
        if ((nDeltaOld == nDelta) || (nDelta == 0))
            for (i = 0; i < nNumBars; i++)
                arrSCBars[i]->m_bKeepSize = false;
    }

    return true;
}

void CSizingControlBar::AlignControlBars()
{
    int nFirst, nLast, nThis;
    GetRowInfo(nFirst, nLast, nThis);

    bool bHorz = IsHorzDocked();
    bool bNeedRecalc = false;
    int nAlign = bHorz ? -2 : 0;

    CRect rc, rcDock;
    m_pDockBar->GetWindowRect(&rcDock);

    for (int i = nFirst; i <= nLast; i++)
    {
        CSizingControlBar* pBar =
            static_cast<CSizingControlBar*>(m_pDockBar->m_arrBars[i]);
        if (HIWORD(pBar) == 0) continue; // placeholder
        if (!pBar->IsVisible()) continue;

        pBar->GetWindowRect(&rc);
        rc.OffsetRect(-rcDock.TopLeft());

        if (pBar->IsKindOf(RUNTIME_CLASS(CSizingControlBar)))
            rc = CRect(rc.TopLeft(),
                bHorz ? pBar->m_szHorz : pBar->m_szVert);

        if ((bHorz ? rc.left : rc.top) != nAlign)
        {
            if (!bHorz)
                rc.OffsetRect(0, nAlign - rc.top - 2);
            else if (m_nDockBarID == AFX_IDW_DOCKBAR_TOP)
                rc.OffsetRect(nAlign - rc.left, -2);
            else
                rc.OffsetRect(nAlign - rc.left, 0);
            pBar->MoveWindow(rc);
            bNeedRecalc = true;
        }
        nAlign += (bHorz ? rc.Width() : rc.Height()) - 2;
    }

    if (bNeedRecalc)
        m_pDockSite->DelayRecalcLayout();
}

void CSizingControlBar::OnUpdateCmdUI(CFrameWnd* pTarget,
                                      BOOL bDisableIfNoHndler)
{
    UNUSED_ALWAYS(bDisableIfNoHndler);
    UNUSED_ALWAYS(pTarget);
}

void CSizingControlBar::LoadState(LPCTSTR lpszProfileName)
{
    ASSERT_VALID(this);
    ASSERT(GetSafeHwnd() != nullptr); // must be called after Create()

#if defined(_SCB_REPLACE_MINIFRAME) && !defined(_SCB_MINIFRAME_CAPTION)
    // compensate the caption miscalculation in CFrameWnd::SetDockState()
    CDockState state;
    state.LoadState(lpszProfileName);

    UINT nID = GetDlgCtrlID();
    for (int i = 0; i < state.m_arrBarInfo.GetSize(); i++)
    {
        CControlBarInfo* pInfo = (CControlBarInfo*)state.m_arrBarInfo[i];
        ASSERT(pInfo != nullptr);
        if (!pInfo->m_bFloating)
            continue;
        
        // this is a floating dockbar - check the ID array
        for (int j = 0; j < pInfo->m_arrBarID.GetSize(); j++)
            if ((DWORD) pInfo->m_arrBarID[j] == nID)
            {
                // found this bar - offset origin and save settings
                pInfo->m_pointPos.x++;
                pInfo->m_pointPos.y +=
                    GetSystemMetrics(SM_CYSMCAPTION) + 1;
                pInfo->SaveState(lpszProfileName, i);
            }
    }
#endif //_SCB_REPLACE_MINIFRAME && !_SCB_MINIFRAME_CAPTION

    CWinApp* pApp = AfxGetApp();

    TCHAR szSection[256];
    wsprintf(szSection, _T("%s-SCBar-%d"), lpszProfileName,
        GetDlgCtrlID());

    m_szHorz.cx = max(m_szMinHorz.cx, (int) pApp->GetProfileInt(
        szSection, _T("sizeHorzCX"), m_szHorz.cx));
    m_szHorz.cy = max(m_szMinHorz.cy, (int) pApp->GetProfileInt(
        szSection, _T("sizeHorzCY"), m_szHorz.cy));

    m_szVert.cx = max(m_szMinVert.cx, (int) pApp->GetProfileInt(
        szSection, _T("sizeVertCX"), m_szVert.cx));
    m_szVert.cy = max(m_szMinVert.cy, (int) pApp->GetProfileInt(
        szSection, _T("sizeVertCY"), m_szVert.cy));

    m_szFloat.cx = max(m_szMinFloat.cx, (int) pApp->GetProfileInt(
        szSection, _T("sizeFloatCX"), m_szFloat.cx));
    m_szFloat.cy = max(m_szMinFloat.cy, (int) pApp->GetProfileInt(
        szSection, _T("sizeFloatCY"), m_szFloat.cy));
}

void CSizingControlBar::SaveState(LPCTSTR lpszProfileName)
{
    // place your SaveState or GlobalSaveState call in
    // CMainFrame's OnClose() or DestroyWindow(), not in OnDestroy()
    ASSERT_VALID(this);
    ASSERT(GetSafeHwnd() != nullptr);

    CWinApp* pApp = AfxGetApp();

    TCHAR szSection[256];
    wsprintf(szSection, _T("%s-SCBar-%d"), lpszProfileName,
        GetDlgCtrlID());

    pApp->WriteProfileInt(szSection, _T("sizeHorzCX"), m_szHorz.cx);
    pApp->WriteProfileInt(szSection, _T("sizeHorzCY"), m_szHorz.cy);

    pApp->WriteProfileInt(szSection, _T("sizeVertCX"), m_szVert.cx);
    pApp->WriteProfileInt(szSection, _T("sizeVertCY"), m_szVert.cy);

    pApp->WriteProfileInt(szSection, _T("sizeFloatCX"), m_szFloat.cx);
    pApp->WriteProfileInt(szSection, _T("sizeFloatCY"), m_szFloat.cy);
}

void CSizingControlBar::GlobalLoadState(CFrameWnd* pFrame,
                                        LPCTSTR lpszProfileName)
{
    POSITION pos = pFrame->m_listControlBars.GetHeadPosition();
    while (pos != nullptr)
    {
        CSizingControlBar* pBar = 
            static_cast<CSizingControlBar*>(pFrame->m_listControlBars.GetNext(pos));
        ASSERT(pBar != nullptr);
        if (pBar->IsKindOf(RUNTIME_CLASS(CSizingControlBar)))
            pBar->LoadState(lpszProfileName);
    }
}

void CSizingControlBar::GlobalSaveState(CFrameWnd* pFrame,
                                        LPCTSTR lpszProfileName)
{
    POSITION pos = pFrame->m_listControlBars.GetHeadPosition();
    while (pos != nullptr)
    {
        CSizingControlBar* pBar =
            static_cast<CSizingControlBar*>(pFrame->m_listControlBars.GetNext(pos));
        ASSERT(pBar != nullptr);
        if (pBar->IsKindOf(RUNTIME_CLASS(CSizingControlBar)))
            pBar->SaveState(lpszProfileName);
    }
}

LRESULT CSizingControlBar::OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam)
{
    int olddpi = m_dpi;

    __super::OnDpiChangedBeforeParent(wParam, lParam);

    for (auto& size : { &m_szHorz, &m_szVert, &m_szFloat })
    {
        size->cx = MulDiv(size->cx, m_dpi, olddpi);
        size->cy = MulDiv(size->cy, m_dpi, olddpi);
    }

    m_pDockSite->DelayRecalcLayout();

    return 0;
}

#ifdef _SCB_REPLACE_MINIFRAME
#ifndef _SCB_MINIFRAME_CAPTION
/////////////////////////////////////////////////////////////////////////////
// CSCBDockContext Drag Operations

static void AdjustRectangle(CRect& rect, CPoint pt)
{
    int nXOffset = (pt.x < rect.left) ? (pt.x - rect.left) :
                    (pt.x > rect.right) ? (pt.x - rect.right) : 0;
    int nYOffset = (pt.y < rect.top) ? (pt.y - rect.top) :
                    (pt.y > rect.bottom) ? (pt.y - rect.bottom) : 0;
    rect.OffsetRect(nXOffset, nYOffset);
}

void CSCBDockContext::StartDrag(CPoint pt)
{
    ASSERT_VALID(m_pBar);
    m_bDragging = TRUE;

    InitLoop();

    ASSERT((m_pBar->m_dwStyle & CBRS_SIZE_DYNAMIC) != 0);

    // get true bar size (including borders)
    CRect rect;
    m_pBar->GetWindowRect(rect);
    m_ptLast = pt;
    CSize sizeHorz = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_HORZDOCK);
    CSize sizeVert = m_pBar->CalcDynamicLayout(0, LM_VERTDOCK);
    CSize sizeFloat = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_MRUWIDTH);

    m_rectDragHorz = CRect(rect.TopLeft(), sizeHorz);
    m_rectDragVert = CRect(rect.TopLeft(), sizeVert);

    // calculate frame dragging rectangle
    m_rectFrameDragHorz = CRect(rect.TopLeft(), sizeFloat);

#ifdef _MAC
    CMiniFrameWnd::CalcBorders(&m_rectFrameDragHorz,
        WS_THICKFRAME, WS_EX_FORCESIZEBOX);
#else
    CMiniFrameWnd::CalcBorders(&m_rectFrameDragHorz, WS_THICKFRAME);
#endif
    m_rectFrameDragHorz.DeflateRect(2, 2);
    m_rectFrameDragVert = m_rectFrameDragHorz;
    
    // adjust rectangles so that point is inside
    AdjustRectangle(m_rectDragHorz, pt);
    AdjustRectangle(m_rectDragVert, pt);
    AdjustRectangle(m_rectFrameDragHorz, pt);
    AdjustRectangle(m_rectFrameDragVert, pt);

    // initialize tracking state and enter tracking loop
    m_dwOverDockStyle = CanDock();
    Move(pt);   // call it here to handle special keys
    Track();
}
#endif //_SCB_MINIFRAME_CAPTION

/////////////////////////////////////////////////////////////////////////////
// CSCBMiniDockFrameWnd

IMPLEMENT_DYNCREATE(CSCBMiniDockFrameWnd, baseCSCBMiniDockFrameWnd);

BEGIN_MESSAGE_MAP(CSCBMiniDockFrameWnd, baseCSCBMiniDockFrameWnd)
    //{{AFX_MSG_MAP(CSCBMiniDockFrameWnd)
    ON_WM_NCLBUTTONDOWN()
    ON_WM_GETMINMAXINFO()
    ON_WM_WINDOWPOSCHANGING()
    ON_WM_SIZE()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CSCBMiniDockFrameWnd::Create(CWnd* pParent, DWORD dwBarStyle)
{
    // set m_bInRecalcLayout to avoid flashing during creation
    // RecalcLayout will be called once something is docked
    m_bInRecalcLayout = TRUE;

    DWORD dwStyle = WS_POPUP|WS_CAPTION|WS_SYSMENU|MFS_MOVEFRAME|
        MFS_4THICKFRAME|MFS_SYNCACTIVE|MFS_BLOCKSYSMENU|
        FWS_SNAPTOBARS;

    if (dwBarStyle & CBRS_SIZE_DYNAMIC)
        dwStyle &= ~MFS_MOVEFRAME;

    DWORD dwExStyle = 0;
#ifdef _MAC
    if (dwBarStyle & CBRS_SIZE_DYNAMIC)
        dwExStyle |= WS_EX_FORCESIZEBOX;
    else
        dwStyle &= ~(MFS_MOVEFRAME|MFS_4THICKFRAME);
#endif

    if (!CMiniFrameWnd::CreateEx(dwExStyle,
        nullptr, &afxChNil, dwStyle, rectDefault, pParent))
    {
        m_bInRecalcLayout = FALSE;
        return FALSE;
    }
    dwStyle = dwBarStyle & (CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT) ?
        CBRS_ALIGN_LEFT : CBRS_ALIGN_TOP;
    dwStyle |= dwBarStyle & CBRS_FLOAT_MULTI;
    CMenu* pSysMenu = GetSystemMenu(FALSE);
    //pSysMenu->DeleteMenu(SC_SIZE, MF_BYCOMMAND);
    CString strHide;
    if (strHide.LoadString(AFX_IDS_HIDE))
    {
        pSysMenu->DeleteMenu(SC_CLOSE, MF_BYCOMMAND);
        pSysMenu->AppendMenu(MF_STRING|MF_ENABLED, SC_CLOSE, strHide);
    }

    // must initially create with parent frame as parent
    if (!m_wndDockBar.Create(pParent, WS_CHILD | WS_VISIBLE | dwStyle,
        AFX_IDW_DOCKBAR_FLOAT))
    {
        m_bInRecalcLayout = FALSE;
        return FALSE;
    }

    // set parent to CMiniDockFrameWnd
    m_wndDockBar.SetParent(this);
    m_bInRecalcLayout = FALSE;

    return TRUE;
}

void CSCBMiniDockFrameWnd::OnNcLButtonDown(UINT nHitTest, CPoint point)
{
    if (nHitTest == HTCAPTION || nHitTest == HTCLOSE)
    {
        baseCSCBMiniDockFrameWnd::OnNcLButtonDown(nHitTest, point);
        return;
    }

    if (GetSizingControlBar() != nullptr)
        CMiniFrameWnd::OnNcLButtonDown(nHitTest, point);
    else
        baseCSCBMiniDockFrameWnd::OnNcLButtonDown(nHitTest, point);
}

CSizingControlBar* CSCBMiniDockFrameWnd::GetSizingControlBar()
{
    CWnd* pWnd = GetWindow(GW_CHILD); // get the dockbar
    if (pWnd == nullptr)
        return nullptr;
    
    pWnd = pWnd->GetWindow(GW_CHILD); // get the controlbar
    if (pWnd == nullptr)
        return nullptr;

    if (!pWnd->IsKindOf(RUNTIME_CLASS(CSizingControlBar)))
        return nullptr;

    return static_cast<CSizingControlBar*>(pWnd);
}

void CSCBMiniDockFrameWnd::OnSize(UINT nType, int cx, int cy) 
{
    CSizingControlBar* pBar = GetSizingControlBar();
    if ((pBar != nullptr) && (GetStyle() & MFS_4THICKFRAME) == 0
        && pBar->IsVisible() &&
        cx + 4 >= pBar->m_szMinFloat.cx &&
        cy + 4 >= pBar->m_szMinFloat.cy)
        pBar->m_szFloat = CSize(cx + 4, cy + 4);

    baseCSCBMiniDockFrameWnd::OnSize(nType, cx, cy);
}

void CSCBMiniDockFrameWnd::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI)
{
    baseCSCBMiniDockFrameWnd::OnGetMinMaxInfo(lpMMI);

    CSizingControlBar* pBar = GetSizingControlBar();
    if (pBar != nullptr)
    {
        CRect r(CPoint(0, 0), pBar->m_szMinFloat - CSize(4, 4));
#ifndef _SCB_MINIFRAME_CAPTION
        CMiniFrameWnd::CalcBorders(&r, WS_THICKFRAME);
#else
        CMiniFrameWnd::CalcBorders(&r, WS_THICKFRAME|WS_CAPTION);
#endif //_SCB_MINIFRAME_CAPTION
        lpMMI->ptMinTrackSize.x = r.Width();
        lpMMI->ptMinTrackSize.y = r.Height();
    }
}

void CSCBMiniDockFrameWnd::OnWindowPosChanging(WINDOWPOS FAR* lpwndpos) 
{
    if ((GetStyle() & MFS_4THICKFRAME) != 0)
    {
        CSizingControlBar* pBar = GetSizingControlBar();
        if (pBar != nullptr)
        {
            lpwndpos->flags |= SWP_NOSIZE; // don't size this time
            // prevents flicker
            pBar->m_pDockBar->ModifyStyle(0, WS_CLIPCHILDREN);

            // enable diagonal resizing
            DWORD dwStyleRemove = MFS_4THICKFRAME;
#ifndef _SCB_MINIFRAME_CAPTION
            // remove caption
            dwStyleRemove |= WS_SYSMENU|WS_CAPTION;
#endif
            ModifyStyle(dwStyleRemove, 0);

            DelayRecalcLayout();
            pBar->PostMessage(WM_NCPAINT);
        }
    }

    CMiniFrameWnd::OnWindowPosChanging(lpwndpos);
}

#endif //_SCB_REPLACE_MINIFRAME
