/////////////////////////////////////////////////////////////////////////
//
// CSizingControlBarG           Version 2.44
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
/////////////////////////////////////////////////////////////////////////

// sizecbar.cpp : implementation file
//

#include "StdAfx.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////
// CSizingControlBarG

IMPLEMENT_DYNAMIC(CSizingControlBarG, baseCSizingControlBarG);

CSizingControlBarG::CSizingControlBarG()
{
}

CSizingControlBarG::~CSizingControlBarG()
{
}

BEGIN_MESSAGE_MAP(CSizingControlBarG, baseCSizingControlBarG)
    //{{AFX_MSG_MAP(CSizingControlBarG)
    ON_WM_NCLBUTTONUP()
    ON_WM_NCHITTEST()
    //}}AFX_MSG_MAP
    ON_MESSAGE(WM_SETTEXT, OnSetText)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////
// CSizingControlBarG message handlers

/////////////////////////////////////////////////////////////////////////
// Mouse Handling
//

void CSizingControlBarG::OnNcLButtonUp(UINT nHitTest, CPoint point)
{
    if (nHitTest == HTCLOSE)
        m_pDockSite->ShowControlBar(this, FALSE, FALSE); // hide

    __super::OnNcLButtonUp(nHitTest, point);
}

void CSizingControlBarG::NcCalcClient(LPRECT pRc, UINT nDockBarID)
{
    CRect rcBar(pRc); // save the bar rect
    // subtract edges
    __super::NcCalcClient(pRc, nDockBarID);

    if (!HasGripper())
        return;

    CRect rc(pRc); // the client rect as calculated by the base class

    bool bHorz = (nDockBarID == AFX_IDW_DOCKBAR_TOP) ||
                 (nDockBarID == AFX_IDW_DOCKBAR_BOTTOM);

    if (bHorz)
        rc.DeflateRect(PointToPixel(m_dblGripper), 0, 0, 0);
    else
        rc.DeflateRect(0, PointToPixel(m_dblGripper), 0, 0);

    // set position for the "x" (hide bar) button
    CPoint ptOrgBtn;
    if (bHorz)
        ptOrgBtn = CPoint(rc.left - PointToPixel(9.75), rc.top);
    else
        ptOrgBtn = CPoint(rc.right - PointToPixel(9.0), rc.top - PointToPixel(9.75));

    m_biHide.Move(ptOrgBtn - rcBar.TopLeft());

    *pRc = rc;
}

void CSizingControlBarG::NcPaintGripper(CDC* pDC, CRect rcClient)
{
    if (!HasGripper())
        return;

    // paints a simple "two raised lines" gripper
    // override this if you want a more sophisticated gripper
    auto PointToPixel = [dpi = GetDpi()](double point) { return static_cast<int>(point * dpi / 72); };
    CRect gripper = rcClient;
    CRect rcbtn(m_biHide.ptOrg, CSize(PointToPixel(m_biHide.dblBoxSize), PointToPixel(m_biHide.dblBoxSize)));
    bool bHorz = IsHorzDocked();

    gripper.DeflateRect(1, 1);
    if (bHorz)
    {   // gripper at left
        gripper.left -= PointToPixel(m_dblGripper);
        gripper.right = gripper.left + PointToPixel(2.25);
        gripper.top = rcbtn.bottom + PointToPixel(2.25);
    }
    else
    {   // gripper at top
        gripper.top -= PointToPixel(m_dblGripper);
        gripper.bottom = gripper.top + PointToPixel(2.25);
        gripper.right = rcbtn.left - PointToPixel(2.25);
    }

    pDC->Draw3dRect(gripper, ::GetSysColor(COLOR_BTNHIGHLIGHT),
        ::GetSysColor(COLOR_BTNSHADOW));

    gripper.OffsetRect(bHorz ? PointToPixel(2.25) : 0, bHorz ? 0 : PointToPixel(2.25));

    pDC->Draw3dRect(gripper, ::GetSysColor(COLOR_BTNHIGHLIGHT),
        ::GetSysColor(COLOR_BTNSHADOW));

    m_biHide.Paint(pDC);
}

NCHITTEST_RESULT CSizingControlBarG::OnNcHitTest(CPoint point)
{
    CRect rcBar;
    GetWindowRect(rcBar);

    LRESULT nRet = __super::OnNcHitTest(point);
    if (nRet != HTCLIENT)
        return nRet;

    auto PointToPixel = [dpi = GetDpi()](double point) { return static_cast<int>(point * dpi / 72); };
    CRect rc(m_biHide.ptOrg, CSize(PointToPixel(m_biHide.dblBoxSize), PointToPixel(m_biHide.dblBoxSize)));
    rc.OffsetRect(rcBar.TopLeft());
    if (rc.PtInRect(point))
        return HTCLOSE;

    return HTCLIENT;
}

/////////////////////////////////////////////////////////////////////////
// CSizingControlBarG implementation helpers

void CSizingControlBarG::OnUpdateCmdUI(CFrameWnd* pTarget,
                                      BOOL bDisableIfNoHndler)
{
    UNUSED_ALWAYS(bDisableIfNoHndler);
    UNUSED_ALWAYS(pTarget);

    if (!HasGripper())
        return;

    bool bNeedPaint = false;

    CPoint pt;
    ::GetCursorPos(&pt);
    bool bHit = (OnNcHitTest(pt) == HTCLOSE);
    bool bLButtonDown = (::GetKeyState(VK_LBUTTON) < 0);

    bool bWasPushed = m_biHide.bPushed;
    m_biHide.bPushed = bHit && bLButtonDown;

    bool bWasRaised = m_biHide.bRaised;
    m_biHide.bRaised = bHit && !bLButtonDown;

    bNeedPaint |= (m_biHide.bPushed ^ bWasPushed) ||
                  (m_biHide.bRaised ^ bWasRaised);

    if (bNeedPaint)
        SendMessage(WM_NCPAINT);
}

/////////////////////////////////////////////////////////////////////////
// CSCBButton

CSCBButton::CSCBButton()
{
    bRaised = false;
    bPushed = false;
}

void CSCBButton::Paint(CDC* pDC)
{
    const int dpi = DpiAware::GetDpiForWindow(AfxGetMainWnd()->m_hWnd);
    auto PointToPixel = [dpi](double point) { return static_cast<int>(point * dpi / 72); };
    CRect rc(ptOrg, CSize(PointToPixel(dblBoxSize), PointToPixel(dblBoxSize)));

    if (bPushed)
        pDC->Draw3dRect(rc, ::GetSysColor(COLOR_BTNSHADOW),
            ::GetSysColor(COLOR_BTNHIGHLIGHT));
    else
        if (bRaised)
            pDC->Draw3dRect(rc, ::GetSysColor(COLOR_BTNHIGHLIGHT),
                ::GetSysColor(COLOR_BTNSHADOW));

    COLORREF clrOldTextColor = pDC->GetTextColor();
    pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
    int nPrevBkMode = pDC->SetBkMode(TRANSPARENT);
    CFont font;
    LOGFONT lf;
    DpiAware::GetPointLogFont(lf, 6, _T("Marlett"), dpi);
    font.CreateFontIndirect(&lf);
    CFont* oldfont = pDC->SelectObject(&font);

    pDC->TextOut(ptOrg.x + PointToPixel(1.5), ptOrg.y + PointToPixel(1.5), CString(_T("r"))); // x-like

    pDC->SelectObject(oldfont);
    pDC->SetBkMode(nPrevBkMode);
    pDC->SetTextColor(clrOldTextColor);
}

bool CSizingControlBarG::HasGripper() const
{
#if defined(_SCB_MINIFRAME_CAPTION) || !defined(_SCB_REPLACE_MINIFRAME)
    // if the miniframe has a caption, don't display the gripper
    if (IsFloating())
        return false;
#endif //_SCB_MINIFRAME_CAPTION

    return true;
}
