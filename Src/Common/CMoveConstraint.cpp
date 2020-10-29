/*!
  \file    CMoveConstraint.cpp
  \author  Perry Rapp, Creator, 1998-2004
  \date    Created: 1998
  \date    Edited:  2006-09-23 (Kimmo Varis)

  \brief   Implementation of CMoveConstraint

*/
/* The MIT License
Copyright (c) 2001 Perry Rapp
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#include "StdAfx.h"
#include "CMoveConstraint.h"
#include "utils/DpiAware.h"
#include <afxtempl.h>       // MFC template collection classes
#include <afxext.h> // needed for CFormView

#ifdef _DEBUG
#define new DEBUG_NEW
#endif




namespace prdlg {

// from windowsx.h
#define GetWindowStyle(hwnd) ((DWORD)::GetWindowLong(hwnd, GWL_STYLE))
#define MapWindowRect(hwndFrom, hwndTo, lprc) \
                    MapWindowPoints((hwndFrom), (hwndTo), (POINT *)(lprc), 2)

// from wdj
static RECT getGripRect(HWND hwnd)
{
	RECT rc;
	GetClientRect(hwnd, &rc);
	const int dpi = DpiAware::GetDpiForWindow(hwnd);
	rc.left = rc.right - DpiAware::GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);
	rc.top = rc.bottom - DpiAware::GetSystemMetricsForDpi(SM_CYHSCROLL, dpi);
	return rc;
}

CMoveConstraint::EGRIP CMoveConstraint::c_defGrip = SG_NORMAL;

CMoveConstraint::Constraint::Constraint()
{
	Init();
}
CMoveConstraint::Constraint::Constraint(double fLeftX, double fExpandX, double fAboveY, double fExpandY, HWND hwndChild, CWnd * pWnd)
{
	Init();
	m_fLeftX = fLeftX;
	m_fExpandX = fExpandX;
	m_fAboveY = fAboveY;
	m_fExpandY = fExpandY;
	m_hwndChild = hwndChild;
	m_pWnd = pWnd;
}
void 
CMoveConstraint::Constraint::Init()
{
	m_fLeftX = 0;
	m_fExpandX = 0;
	m_fAboveY = 0;
	m_fExpandY = 0;
	m_hwndChild = nullptr;
	m_pWnd = nullptr;
	// m_rectChildOriginal
	m_hwndParent = nullptr;
}

CMoveConstraint::CMoveConstraint()
: m_bSubclassed(false)
, m_oldWndProc(nullptr)
, m_sRegistryValueName(_T("UnnamedWindow"))
, m_sRegistrySubkey(_T("LastWindowPos"))
{
	ClearMostData();
}

bool
CMoveConstraint::InitializeCurrentSize(HWND hwndDlg)
{
	ASSERT(!m_hwndDlg);
	if (!IsWindow(hwndDlg))
		return false;
	m_hwndDlg = hwndDlg;
	m_dpi = DpiAware::GetDpiForWindow(hwndDlg);

	GrabCurrentDimensionsAsOriginal(hwndDlg);
	return true;
}

void
CMoveConstraint::GrabCurrentDimensionsAsOriginal(HWND hwndDlg)
{
	// figure original size for resizing code
	GetClientRect(hwndDlg, m_rectDlgOriginal);
	CRect rect;
	GetWindowRect(hwndDlg, &rect);
	LOGFONT lf;
	CFont* pFont = CWnd::FromHandle(hwndDlg)->GetFont();
	pFont->GetLogFont(&lf);
	m_fontFace = lf.lfFaceName;
	m_fontSize = MulDiv(abs(lf.lfHeight), 72, m_dpi);

	// (min/max code)
	// remember original width & heighth in case a disallow function called
	// by default, set minimum size to original size
	m_nOrigX = m_nMinX = rect.Width();
	m_nOrigY = m_nMinY = rect.Height();

	m_bOriginalFetched = true;
}

void
CMoveConstraint::InitializeSpecificSize(HWND /*hwndDlg*/, int nWidth, int nHeight)
{
	// figure original size for resizing code
	m_rectDlgOriginal.left = 0;
	m_rectDlgOriginal.right = nWidth;
	m_rectDlgOriginal.top = 0;
	m_rectDlgOriginal.bottom = nHeight;

	// (min/max code)
	// remember original width & heighth in case a disallow function called
	// by default, set minimum size to original size
	m_nOrigX = m_nMinX = nWidth;
	m_nOrigY = m_nMinY = nHeight;
}

bool
CMoveConstraint::InitializeOriginalSize(HWND hwndDlg)
{
	ASSERT(hwndDlg != nullptr && m_hwndDlg == nullptr);
	m_hwndDlg = hwndDlg;
	m_dpi = DpiAware::GetDpiForWindow(hwndDlg);

	return m_nOrigX != 0; // if 0, we didn't get WM_SIZE so we don't know the original size
}

bool
CMoveConstraint::InitializeOriginalSize(CWnd * pParent)
{
	ASSERT(pParent != nullptr);
	return InitializeOriginalSize(pParent->m_hWnd);
}


void
CMoveConstraint::InitializeSpecificSize(CWnd * pDlg, int nWidth, int nHeight)
{
	ASSERT(pDlg != nullptr);
	InitializeSpecificSize(pDlg->m_hWnd, nWidth, nHeight);
}

bool
CMoveConstraint::InitializeCurrentSize(CWnd * pDlg)
{
	ASSERT(pDlg != nullptr);
	return InitializeCurrentSize(pDlg->m_hWnd);
}

void
CMoveConstraint::UpdateSizes()
{
	Resize(m_hwndDlg, SIZE_RESTORED);
}

void
CMoveConstraint::AllowHeightShrink()
{
	m_nMinY = 0;
}
void
CMoveConstraint::AllowWidthShrink()
{
	m_nMinX = 0;
}
void
CMoveConstraint::DisallowHeightGrowth()
{
	m_nMaxY = m_nOrigY;
}
void
CMoveConstraint::DisallowWidthGrowth()
{
	m_nMaxX = m_nOrigX;
}

void
CMoveConstraint::SetMaxSizePixels(int nWidth, int nHeight)
{
	if (nWidth != -1)
		m_nMaxX = nWidth;
	if (nHeight != -1)
		m_nMaxY = nHeight;
}

void
CMoveConstraint::SetMinSizePixels(int nWidth, int nHeight)
{
	if (nWidth != -1)
		m_nMinX = nWidth;
	if (nHeight != -1)
		m_nMinY = nHeight;
}

void
CMoveConstraint::SetMinSizePercent(double fWidth, double fHeight)
{
	if (fWidth >= 0)
		m_nMinX = (int)(fWidth * m_nOrigX);
	if (fHeight >= 0)
		m_nMinY = (int)(fHeight * m_nOrigY);
}

void
CMoveConstraint::SetScrollScale(CFormView * pFormView, double fShrinkWidth, double fShrinkHeight)
{
	m_fShrinkHeight = fShrinkHeight;
	m_fShrinkWidth = fShrinkWidth;
	m_pFormView = pFormView;
	CSize size = pFormView->GetTotalSize();
	m_nOrigScrollX = size.cx;
	m_nOrigScrollY = size.cy;
}

CMoveConstraint::~CMoveConstraint()
{
	ClearMostData();
}


void
CMoveConstraint::ClearMostData()
{
	// clears everything but m_bSubclassed
	// this is called from constructor, OnDestroy, and destructor
	// so it can't assume any numerical variables have sane values
	m_hwndDlg=nullptr;
	// m_rectDlgOriginal
	m_nOrigX=0;
	m_nOrigY=0;
	m_nGrip=c_defGrip;
	m_bOriginalFetched=false;
	m_nMinX=0;
	m_nMinY=0;
	m_nMaxX=0;
	m_nMaxY=0;
	m_nDelayed=0;
	m_dpi=0;
	// this specifically does NOT touch m_bSubclassed, as a subclass may still be in use
	m_pFormView=nullptr;
	m_nOrigScrollX=0;
	m_nOrigScrollY=0;
	m_fShrinkWidth=0;
	m_fShrinkHeight=0;
	m_bPropertyPage=false;
	m_bPropertySheet=false;
	m_ConstraintList.RemoveAll();
	m_bPersistent=false;
	m_bConstrainNonChildren = false;
}

void
CMoveConstraint::
InitializeChildConstraintData(HWND hwndDlg, Constraint & constraint)
{
	HWND hwndChild = constraint.m_hwndChild;
	ASSERT(IsWindow(hwndChild));
	constraint.m_hwndParent = GetParent(hwndChild);
	if (!m_bConstrainNonChildren && constraint.m_hwndParent != hwndDlg)
	{
		// this is all predicated on the children being real Windows children
		// because of the use of client coordinates in ::SetWindowPos down in Resize() below
		// altho the truth is, the scaling is linear, so it will work with any windows
		// but if you want to do that, you have to call 
		//  m_constraint.ConstrainNonWindows();
		// to suppress this debug ASSERT
		ASSERT(false); // this is not a child of the dialog
	}
	GetWindowRect(hwndChild, constraint.m_rectChildOriginal);
	CWnd * wndParent = CWnd::FromHandle(constraint.m_hwndParent);
	wndParent->ScreenToClient(constraint.m_rectChildOriginal);
}

bool
CMoveConstraint::
Constrain(HWND hwndChild, double fLeftX, double fExpandX, double fAboveY, double fExpandY)
{
	return DoConstrain(0, hwndChild, fLeftX, fExpandX, fAboveY, fExpandY);
}

bool
CMoveConstraint::
DoConstrain(CWnd * pWnd, HWND hwndChild, double fLeftX, double fExpandX, double fAboveY, double fExpandY)
{
	Constraint constraint(fLeftX, fExpandX, fAboveY, fExpandY, hwndChild, pWnd);

	if (m_hwndDlg && IsWindow(m_hwndDlg) && hwndChild != nullptr && IsWindow(hwndChild))
	{
		InitializeChildConstraintData(m_hwndDlg, constraint);
	}
	else
	{
		if (pWnd == nullptr) // only CWnds can be deferred
			return false;
		m_nDelayed++;
	}

	ConstraintList & constraintList = m_ConstraintList;
	constraintList.AddTail(constraint);
	return true;
}


void
CMoveConstraint::
Constrain(CWnd * pWnd, double fLeftX, double fExpandX, double fAboveY, double fExpandY)
{
	ASSERT(pWnd != nullptr);
	DoConstrain(pWnd, pWnd->m_hWnd, fLeftX, fExpandX, fAboveY, fExpandY);
}

bool
CMoveConstraint::
ConstrainItem(int nId, double fLeftX, double fExpandX, double fAboveY, double fExpandY)
{
	if (m_hwndDlg == nullptr || !IsWindow(m_hwndDlg))
		return false;
	HWND hwnd = GetDlgItem(m_hwndDlg, nId);
	return DoConstrain(0, hwnd, fLeftX, fExpandX, fAboveY, fExpandY);
}

/**
 * This is the window proc callback that works with the CSubclass module.
 */
LRESULT CALLBACK
CMoveConstraint::ConstraintWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	void * data = GetProp(hwnd, _T("CMoveConstraintData"));
	CMoveConstraint * constraint = reinterpret_cast<CMoveConstraint *>(data);

	LRESULT lresult;
	if (constraint->WindowProc(hwnd, msg, wParam, lParam, &lresult))
		return lresult;

	return CallWindowProc(constraint->m_oldWndProc, hwnd, msg, wParam, lParam);
}
bool
CMoveConstraint::SubclassWnd()
{
	void * data = reinterpret_cast<void *>(this);
	// this will return false if this window/wndproc combination has already
	// been established (subclassed)
	m_bSubclassed = true;
	m_oldWndProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(m_hwndDlg, GWLP_WNDPROC));
	SetWindowLongPtr(m_hwndDlg, GWLP_WNDPROC, (__int3264)(LONG_PTR)(CMoveConstraint::ConstraintWndProc));
	SetProp(m_hwndDlg, _T("CMoveConstraintData"), data);
	return m_bSubclassed;
}
bool
CMoveConstraint::UnSubclassWnd()
{
	if (!m_bSubclassed)
		return false;
	SetWindowLongPtr(m_hwndDlg, GWLP_WNDPROC, (__int3264)(LONG_PTR)(m_oldWndProc));
	RemoveProp(m_hwndDlg, _T("CMoveConstraintData"));
	m_oldWndProc = nullptr;
	m_bSubclassed = false;
	return true;
}

/**
 * Check if we have any pending constraints not yet added to constraint list
 * because they were set before the dialog was created (so initial sizes 
 * could not yet be captured).
 */
void
CMoveConstraint::CheckDeferredChildren()
{
	if (m_nDelayed == 0)
		return;
	ConstraintList & constraintList = m_ConstraintList;
	for (POSITION pos=constraintList.GetHeadPosition(); pos != nullptr; constraintList.GetNext(pos))
	{
		Constraint & constraint = constraintList.GetAt(pos);
		if (constraint.m_hwndChild  != nullptr)
			continue;
		ASSERT(constraint.m_pWnd != nullptr);
		if (constraint.m_pWnd->m_hWnd != nullptr)
		{
			constraint.m_hwndChild = constraint.m_pWnd->m_hWnd;
			InitializeChildConstraintData(m_hwndDlg, constraint);
			m_nDelayed--;
		}
	}
}

/**
 * Custom handling for WM_SIZE
 * Apply all resize constraints from the constraint list.
 */
void
CMoveConstraint::Resize(HWND hWnd, UINT nType)
{
// optional - could use BeginDeferWindowPos, DeferWindowPos, EndDeferWindowPos
// 2000/10/07 - haven't tried them
// but I don't think they'd matter because I use SWP_NOREDRAW in the loop

	if (nType == SIZE_MINIMIZED) return;

	if (m_hwndDlg == nullptr && hWnd != nullptr && !m_bOriginalFetched)
	{
		// if early subclass or wndproc
		// grab early dimensions, in case we want them later (eg, property sheet)
		GrabCurrentDimensionsAsOriginal(hWnd);
		return;
	}

	if (m_hwndDlg == nullptr || !IsWindow(m_hwndDlg))
		return;

	CRect rectParentCurrent;
	GetClientRect(m_hwndDlg, rectParentCurrent);

	// compute delta from original size (all fractions based on this)
	int nDeltaWidth = (rectParentCurrent.right - m_rectDlgOriginal.right);
	int nDeltaHeight = (rectParentCurrent.bottom - m_rectDlgOriginal.bottom);

	ConstraintList & constraintList = m_ConstraintList;
	for (POSITION pos=constraintList.GetHeadPosition(); pos != nullptr; constraintList.GetNext(pos))
	{
		Constraint & constraint = constraintList.GetAt(pos);
		if (constraint.m_hwndChild == nullptr)
			continue;

		CRect rectChildCurrent;
		// get the screen & client coordinates of the child
		::GetWindowRect(constraint.m_hwndChild, &rectChildCurrent);
		CWnd * wndParent = CWnd::FromHandle(constraint.m_hwndParent);
		wndParent->ScreenToClient(&rectChildCurrent);

		int nDelta;

		nDelta = nDeltaWidth;
		rectChildCurrent.left = (int)(nDelta * constraint.m_fLeftX) + constraint.m_rectChildOriginal.left;
		rectChildCurrent.right = (int)(nDelta * (constraint.m_fLeftX + constraint.m_fExpandX)) + constraint.m_rectChildOriginal.right;

		nDelta = nDeltaHeight;
		rectChildCurrent.top = (int)(nDelta * constraint.m_fAboveY) + constraint.m_rectChildOriginal.top;
		rectChildCurrent.bottom = (int)(nDelta * (constraint.m_fAboveY + constraint.m_fExpandY)) + constraint.m_rectChildOriginal.bottom;

		SetWindowPos(constraint.m_hwndChild, nullptr, rectChildCurrent.left, rectChildCurrent.top
			, rectChildCurrent.Width(), rectChildCurrent.Height(), SWP_NOZORDER+SWP_NOREDRAW);
	}

	if (m_pFormView != nullptr)
	{
		// ignore growth
		//if (nDeltaWidth > 0)
		//	nDeltaWidth = 0;
		//if (nDeltaHeight > 0)
		//	nDeltaHeight = 0;
		CSize size;
		size.cx = (int)(m_nOrigScrollX + nDeltaWidth * m_fShrinkWidth);
		size.cy = (int)(m_nOrigScrollY + nDeltaHeight * m_fShrinkHeight);
		m_pFormView->SetScrollSizes(MM_TEXT, size);
	}

	InvalidateRect(m_hwndDlg, nullptr, TRUE);
	UpdateWindow(m_hwndDlg);
}

/**
 * Custom handling for WM_GETMINMAXINFO.
 * Enforce any resizing limitations.
 */
void
CMoveConstraint::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI)
{
	// views don't get WM_GETMINMAXINFO, but dialogs & frames do
	if (m_hwndDlg == nullptr)
		return;
	if (m_nMinX)
		lpMMI->ptMinTrackSize.x = m_nMinX;
	if (m_nMinY)
		lpMMI->ptMinTrackSize.y = m_nMinY;
	if (m_nMaxX)
		lpMMI->ptMaxTrackSize.x = m_nMaxX;
	if (m_nMaxY)
		lpMMI->ptMaxTrackSize.y = m_nMaxY;
}

bool
CMoveConstraint::PaintGrip()
{
	if (m_nGrip == SG_NONE) return false;
	if (m_hwndDlg == nullptr) return false;
	HWND hw = (m_nGrip == SG_PARENTSTATE) ? GetParent(m_hwndDlg) : m_hwndDlg;
	return !IsZoomed(hw) && !IsIconic(hw);
}

/**
 * Custom handling for WM_NCHITTEST
 * We paint the sizing grip if the mouse is in the lower right hand corner.
 */
bool
CMoveConstraint::OnNcHitTest(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT * plresult)
{
	// views don't get WM_NCHITTEST, but dialogs & frames do
	if (m_hwndDlg == nullptr)
		return false;
	if (m_nMinY == m_nMaxY)
	{
		LRESULT nRet = CallWindowProc(m_oldWndProc, m_hwndDlg, msg, wParam, lParam);
		switch(nRet)
		{
		case HTBOTTOMLEFT:
		case HTTOPLEFT:
			*plresult = HTLEFT;
			return true;
		case HTBOTTOMRIGHT:
		case HTTOPRIGHT:
			*plresult = HTRIGHT;
			return true;
		case HTBOTTOM:
		case HTTOP:
			*plresult = HTBORDER;
			return true;
		}
		return false;
	}
	if (m_nMinX == m_nMaxX)
	{
		LRESULT nRet = CallWindowProc(m_oldWndProc, m_hwndDlg, msg, wParam, lParam);
		switch(nRet)
		{
		case HTBOTTOMLEFT:
		case HTBOTTOMRIGHT:
			*plresult = HTBOTTOM;
			return true;
		case HTTOPLEFT:
		case HTTOPRIGHT:
			*plresult = HTTOP;
			return true;
		case HTLEFT:
		case HTRIGHT:
			*plresult = HTBORDER;
			return true;
		}
		return false;
	}
	if (!PaintGrip())
		return false;

	// check for size grip
	int x = (int)(short)LOWORD(lParam);
	int y = (int)(short)HIWORD(lParam);
	int cx,cy;
	RECT rc = getGripRect(m_hwndDlg);
	MapWindowRect(m_hwndDlg, HWND_DESKTOP, &rc);
	RECT rc2;
	GetWindowRect(m_hwndDlg, &rc2);
	cx = x-rc.left;
	cy = y-rc.top;
	if (0 < cx && 0 < cy && (rc.right - rc.left < cy+cy))
	{
		*plresult = HTBOTTOMRIGHT;
		return true;
	}
	return false;
}

void
CMoveConstraint::OnDestroy()
{
	if (m_bPersistent)
		Persist(true, true);
	UnSubclassWnd();
	// the one variable that CANNOT safely be cleared now is m_bSubclassed
	// because the subclass is almost certainly not yet removed
	// (the subclass calls us to let us do destroy processing, before 
	//  removing itself)
	ClearMostData();
}

/**
 * Custom handling of the TTN_NEEDTEXT notification message
 */
bool
CMoveConstraint::OnTtnNeedText(TOOLTIPTEXT * pTTT, LRESULT * plresult)
{
	UINT_PTR id = pTTT->hdr.idFrom;
	UINT uflags = pTTT->uFlags;
	if (uflags & TTF_IDISHWND)
		id = GetDlgCtrlID((HWND)id);
	tip ti;
	if (m_tips.Lookup(id, ti))
	{
		if (ti.m_nResourceId)
		{
			pTTT->lpszText = MAKEINTRESOURCE(ti.m_nResourceId);
			pTTT->hinst = AfxGetResourceHandle();
		}
		else
		{
			pTTT->lpszText = (LPTSTR)(LPCTSTR)ti.m_sText;
		}
		*plresult = true; // return `true` from original window proc
		return true; // stop processing this message
	}
	return false;
}

/**
 * Main window proc of this subclassing library
 * Set plresult to set the return value of the real window proc which called us.
 * Return true to prevent any further handling of this message.
 */
bool
CMoveConstraint::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT * plresult)
{
	if (m_bPropertyPage)
	{
		// pass PSN_SETACTIVE to parent (property sheet)
		if (m_hwndDlg && (WM_NOTIFY == msg) && (PSN_SETACTIVE == (((NMHDR *)lParam)->code)))
		{
			HWND hwndParent = GetParent(m_hwndDlg);
			NMHDR nmhdr = { hwndParent, 0, PSN_SETACTIVE };
			SendMessage(hwndParent, WM_NOTIFY, 0, (LPARAM)&nmhdr);
		}
	}
	if (m_bPropertySheet)
	{
		if ((WM_NOTIFY == msg) && (PSN_SETACTIVE == (((NMHDR *)lParam)->code)))
		{
			CheckDeferredChildren();
		}
	}
	if (WM_GETMINMAXINFO == msg) {
		OnGetMinMaxInfo(reinterpret_cast<LPMINMAXINFO>(lParam));
	} else if (WM_SIZE == msg) { // 2000/03/10 - tried WM_EXITSIZEMOVE - no help for property page problem
		Resize(hWnd, (UINT)wParam /* x & y in lParam */);
	} else if (WM_PAINT == msg && PaintGrip()) {
		CPaintDC dc(CWnd::FromHandle(hWnd));
		RECT rc = getGripRect(hWnd);
		dc.DrawFrameControl(&rc, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
	} else if (WM_NCHITTEST == msg && !IsIconic(hWnd) && !IsZoomed(hWnd)) {
		if (OnNcHitTest(msg, wParam, lParam, plresult))
			return true;
	} else if (WM_DESTROY == msg) {
		OnDestroy();
	} else if (msg==WM_NOTIFY && TTN_NEEDTEXT==((NMHDR*)lParam)->code) {
		if (OnTtnNeedText((TOOLTIPTEXT*)lParam, plresult))
			return true;
	} else if (WM_DPICHANGED == msg) {
		const int olddpi = m_dpi;
		m_dpi = HIWORD(wParam);
		const int oldnonclientsx = m_nOrigX - m_rectDlgOriginal.Width();
		const int oldnonclientsy = m_nOrigY - m_rectDlgOriginal.Height();
		CRect rc, rcClient;
		GetWindowRect(m_hwndDlg, &rc);
		GetClientRect(m_hwndDlg, &rcClient);
		const int newnonclientsx = rc.Width() - rcClient.Width();
		const int newnonclientsy = rc.Height() - rcClient.Height();
		const RECT* pRect = reinterpret_cast<RECT*>(lParam);
		CSize sizeOld = { m_rectDlgOriginal.right, m_rectDlgOriginal.bottom };
		CSize sizeNew = DpiAware::Dialog_CalcUpdatedSize(m_fontFace.c_str(), m_fontSize, sizeOld, olddpi, m_dpi);
		m_rectDlgOriginal.right = sizeNew.cx;
		m_rectDlgOriginal.bottom = sizeNew.cx;
		for (auto& pval : { &m_nMaxX, &m_nMinX, &m_nOrigX })
		{
			if (*pval)
				*pval = MulDiv(*pval - oldnonclientsx, sizeNew.cx, sizeOld.cx) + newnonclientsx;
		}
		for (auto& pval : { &m_nMaxY, &m_nMinY, &m_nOrigY })
		{
			if (*pval)
				*pval = MulDiv(*pval - oldnonclientsy, sizeNew.cy, sizeOld.cy) + newnonclientsy;
		}
		ConstraintList & constraintList = m_ConstraintList;
		for (POSITION pos=constraintList.GetHeadPosition(); pos != nullptr; constraintList.GetNext(pos))
		{
			Constraint & constraint = constraintList.GetAt(pos);
			for (auto& pval :
				{ &constraint.m_rectChildOriginal.left, &constraint.m_rectChildOriginal.right })
				*pval = MulDiv(*pval, sizeNew.cx, sizeOld.cx);
			for (auto& pval :
				{ &constraint.m_rectChildOriginal.top, &constraint.m_rectChildOriginal.bottom })
				*pval = MulDiv(*pval, sizeNew.cy, sizeOld.cy);
		}
	}

	return false;
}

/**
 * Save size (& optionally position) in registry
 */
void
CMoveConstraint::LoadPosition(LPCTSTR szKeyName, LPCTSTR szValueName, bool position)
{
	m_sRegistrySubkey = szKeyName;
	LoadPosition(szValueName, position);
}

/**
 * Save size (& optionally position) in registry
 */
void
CMoveConstraint::LoadPosition(LPCTSTR szValueName, bool position)
{
	m_sRegistryValueName = szValueName;
	m_bPersistent=true;
	Persist(false, position);
}

void
CMoveConstraint::Persist(bool saving, bool position)
{
	LPCTSTR szSection = m_sRegistrySubkey;
	if (saving)
	{
		CString str;
		RECT rc;
		GetWindowRect(m_hwndDlg, &rc);
		str.Format(_T("%d,%d,%d,%d"), rc.left, rc.top, rc.right, rc.bottom);
		AfxGetApp()->WriteProfileString(szSection, m_sRegistryValueName, str);
	}
	else
	{
		RECT wprc;
		CString str = AfxGetApp()->GetProfileString(szSection, m_sRegistryValueName);
		GetWindowRect(m_hwndDlg, &wprc);
		if (m_pFormView != nullptr)
			CWnd::FromHandle(m_hwndDlg)->GetParent()->ScreenToClient(&wprc);
		CRect rc;
		int ct=_stscanf_s(str, _T("%d,%d,%d,%d"), &rc.left, &rc.top, &rc.right, &rc.bottom);
		if (ct==4)
		{
			if (position)
			{
				wprc.left = rc.left;
				wprc.top = rc.top;
			}
			int width = rc.Width();
			int height = rc.Height();
			if (m_nMinX && m_nMinX > width)  width = m_nMinX;
			if (m_nMaxX && m_nMaxX < width)  width = m_nMaxX;
			if (m_nMinY && m_nMinY > height) height = m_nMinY;
			if (m_nMaxY && m_nMaxY < height) height = m_nMaxY;
			wprc.right = wprc.left + width;
			wprc.bottom = wprc.top + height;
			SetWindowPos(m_hwndDlg, nullptr, 
				wprc.left, wprc.top, wprc.right - wprc.left, wprc.bottom - wprc.top,
				SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}
}


void
CMoveConstraint::SetTip(int id, LPCTSTR szTip)
{
	tip ti;
	ti.m_sText = szTip;
	m_tips[id] = ti;
}

void
CMoveConstraint::SetTip(int id, int nResourceId)
{
	tip ti;
	ti.m_nResourceId = nResourceId;
	m_tips[id] = ti;
}


} // namespace

