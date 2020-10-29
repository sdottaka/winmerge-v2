// SPDX-License-Identifier: BSL-1.0
// Copyright (c) 2020 Takashi Sawanaka
//
// Use, modification and distribution are subject to the 
// Boost Software License, Version 1.0. (See accompanying file 
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "StdAfx.h"
#include "MDITileLayout.h"
#include "DpiAware.h"

namespace MDITileLayout
{
	bool Pane::Split(bool bHorizontal, double ratio)
	{
		if (IsSplitted())
			return false;
		m_pSplittedLayout.reset(new SplittedLayout(this, bHorizontal, ratio, m_wndList));
		m_wndList.clear();
		return true;
	}

	bool Pane::Combine()
	{
		if (IsSplitted())
		{
			if (m_pSplittedLayout->m_pPaneFirst->IsSplitted())
				m_pSplittedLayout->m_pPaneFirst->Combine();
			if (m_pSplittedLayout->m_pPaneSecond->IsSplitted())
				m_pSplittedLayout->m_pPaneSecond->Combine();
			for (auto& pWnd : m_pSplittedLayout->m_pPaneFirst->m_wndList)
				m_wndList.push_back(pWnd);
			for (auto& pWnd : m_pSplittedLayout->m_pPaneSecond->m_wndList)
				m_wndList.push_back(pWnd);
			m_pSplittedLayout.reset();
			return true;
		}
		if (m_pParentSplittedLayout == nullptr)
			return false;
		return m_pParentSplittedLayout->m_pParentPane->Combine();
	}

	void Pane::Tile(bool bHorizontal)
	{
		if (IsSplitted())
			Combine();
		size_t nCount = m_wndList.size();
		if (nCount <= 1)
			return;
		Split(bHorizontal, 1.0 / nCount);
		for (size_t i = 1; i < nCount; ++i)
			m_pSplittedLayout->m_pPaneFirst->MoveWindow(m_pSplittedLayout->m_pPaneSecond.get(), m_pSplittedLayout->m_pPaneFirst->m_wndList[1]);
		m_pSplittedLayout->m_pPaneSecond->Tile(bHorizontal);
	}

	CRect Pane::GetRect(const CRect& rcMainWnd) const
	{
		double left = 0, top = 0, width = 1.0, height = 1.0;
		for (auto *pPane = this;
			pPane->m_pParentSplittedLayout != nullptr;
			pPane = pPane->m_pParentSplittedLayout->m_pParentPane)
		{
			SplittedLayout* pParentSplittedLayout = pPane->m_pParentSplittedLayout;
			if (pParentSplittedLayout->m_pPaneFirst.get() == pPane)
			{
				if (pParentSplittedLayout->m_bHorizontal)
				{
					top *= pParentSplittedLayout->m_firstPaneSizeRatio;
					height *= pParentSplittedLayout->m_firstPaneSizeRatio;
				}
				else
				{
					left *= pParentSplittedLayout->m_firstPaneSizeRatio;
					width *= pParentSplittedLayout->m_firstPaneSizeRatio;
				}
			}
			else
			{
				if (pParentSplittedLayout->m_bHorizontal)
				{
					top = pParentSplittedLayout->m_firstPaneSizeRatio + top * (1.0 - pParentSplittedLayout->m_firstPaneSizeRatio);
					height *= (1.0 - pParentSplittedLayout->m_firstPaneSizeRatio);
				}
				else
				{
					left = pParentSplittedLayout->m_firstPaneSizeRatio + left * (1.0 - pParentSplittedLayout->m_firstPaneSizeRatio);
					width *= (1.0 - pParentSplittedLayout->m_firstPaneSizeRatio);
				}
			}
			
		}
		return CRect(
			static_cast<int>(rcMainWnd.left + rcMainWnd.Width() * left),
			static_cast<int>(rcMainWnd.top + rcMainWnd.Height() * top),
			static_cast<int>(rcMainWnd.left + rcMainWnd.Width() * (left + width)),
			static_cast<int>(rcMainWnd.top + rcMainWnd.Height() * (top + height)));
	}

	void Pane::SetDefaultOpenPane()
	{
		for (auto *pPane = this;
			pPane->m_pParentSplittedLayout != nullptr;
			pPane = pPane->m_pParentSplittedLayout->m_pParentPane)
		{
			SplittedLayout* pParentSplittedLayout = pPane->m_pParentSplittedLayout;
			pParentSplittedLayout->m_bDefaultOpenPaneIsFirst = (pParentSplittedLayout->m_pPaneFirst.get() == pPane);
		}
	}

	Pane* Pane::GetDefaultOpenPane() const
	{
		if (!IsSplitted())
			return const_cast<Pane *>(this);
		if (m_pSplittedLayout->m_bDefaultOpenPaneIsFirst)
			return m_pSplittedLayout->m_pPaneFirst->GetDefaultOpenPane();
		return m_pSplittedLayout->m_pPaneSecond->GetDefaultOpenPane();
	}

	bool Pane::AddWindow(CWnd* pWnd)
	{
		if (IsSplitted())
			return false;
		m_wndList.push_back(pWnd);
		return true;
	}

	bool Pane::RemoveWindow(CWnd* pWnd)
	{
		auto it = std::remove(m_wndList.begin(), m_wndList.end(), pWnd);
		if (it == m_wndList.end())
			return false;
		m_wndList.erase(it);
		return true;
	}

	bool Pane::MoveWindow(Pane* pDstPane, CWnd* pWnd)
	{
		if (!RemoveWindow(pWnd))
			return false;
		return pDstPane->AddWindow(pWnd);
	}

	Pane* Pane::FindPaneByWindow(CWnd* pWnd) const
	{
		if (!IsSplitted())
		{
			auto it = std::find(m_wndList.begin(), m_wndList.end(), pWnd);
			if (it != m_wndList.end())
				return const_cast<Pane *>(this);
			return nullptr;
		}
		if (Pane *pPane = m_pSplittedLayout->m_pPaneFirst->FindPaneByWindow(pWnd))
			return pPane;
		return m_pSplittedLayout->m_pPaneSecond->FindPaneByWindow(pWnd);
	}

	void Pane::UpdateSizeRatio(const CRect& rcMainWnd, const CRect& rcChildWnd)
	{
		if (m_pParentSplittedLayout == nullptr)
			return;
		const int None = 0, Left = 1, Top = 2, Right = 4, Bottom = 8;
		int changedBorders = None;
		CRect rcChildWndOld = GetRect(rcMainWnd);
		if (rcChildWndOld.left != rcChildWnd.left)
			changedBorders |= Left;
		if (rcChildWndOld.top != rcChildWnd.top)
			changedBorders |= Top;
		if (rcChildWndOld.right != rcChildWnd.right)
			changedBorders |= Right;
		if (rcChildWndOld.bottom != rcChildWnd.bottom)
			changedBorders |= Bottom;
		if (changedBorders & Left)
		{
			for (auto* pPane = this;
				pPane->m_pParentSplittedLayout != nullptr;
				pPane = pPane->m_pParentSplittedLayout->m_pParentPane)
			{
				if (!pPane->m_pParentSplittedLayout->m_bHorizontal && pPane->m_pParentSplittedLayout->m_pPaneSecond.get() == pPane)
				{
					CRect rcParentWnd = pPane->m_pParentSplittedLayout->m_pParentPane->GetRect(rcMainWnd);
					if (rcChildWnd.Width() <= 0 || rcChildWnd.Width() >= rcParentWnd.Width())
					{
						pPane->m_pParentSplittedLayout->m_pParentPane->Combine();
						return;
					}
					pPane->m_pParentSplittedLayout->m_firstPaneSizeRatio = static_cast<double>(rcChildWnd.left - rcParentWnd.left) / rcParentWnd.Width();
					break;
				}
			}
		}
		if (changedBorders & Top)
		{
			for (auto* pPane = this;
				pPane->m_pParentSplittedLayout != nullptr;
				pPane = pPane->m_pParentSplittedLayout->m_pParentPane)
			{
				if (pPane->m_pParentSplittedLayout->m_bHorizontal && pPane->m_pParentSplittedLayout->m_pPaneSecond.get() == pPane)
				{
					CRect rcParentWnd = pPane->m_pParentSplittedLayout->m_pParentPane->GetRect(rcMainWnd);
					if (rcChildWnd.Height() <= 0 || rcChildWnd.Height() >= rcParentWnd.Height())
					{
						pPane->m_pParentSplittedLayout->m_pParentPane->Combine();
						return;
					}
					pPane->m_pParentSplittedLayout->m_firstPaneSizeRatio = static_cast<double>(rcChildWnd.top - rcParentWnd.top) / rcParentWnd.Height();
					break;
				}
			}
		}
		if (changedBorders & Right)
		{
			for (auto* pPane = this;
				pPane->m_pParentSplittedLayout != nullptr;
				pPane = pPane->m_pParentSplittedLayout->m_pParentPane)
			{
				if (!pPane->m_pParentSplittedLayout->m_bHorizontal && pPane->m_pParentSplittedLayout->m_pPaneFirst.get() == pPane)
				{
					CRect rcParentWnd = pPane->m_pParentSplittedLayout->m_pParentPane->GetRect(rcMainWnd);
					if (rcChildWnd.Width() <= 0 || rcChildWnd.Width() >= rcParentWnd.Width())
					{
						pPane->m_pParentSplittedLayout->m_pParentPane->Combine();
						return;
					}
					pPane->m_pParentSplittedLayout->m_firstPaneSizeRatio = static_cast<double>(rcChildWnd.right - rcParentWnd.left) / rcParentWnd.Width();
					break;
				}
			}
		}
		if (changedBorders & Bottom)
		{
			for (auto* pPane = this;
				pPane->m_pParentSplittedLayout != nullptr;
				pPane = pPane->m_pParentSplittedLayout->m_pParentPane)
			{
				if (pPane->m_pParentSplittedLayout->m_bHorizontal && pPane->m_pParentSplittedLayout->m_pPaneFirst.get() == pPane)
				{
					CRect rcParentWnd = pPane->m_pParentSplittedLayout->m_pParentPane->GetRect(rcMainWnd);
					if (rcChildWnd.Height() <= 0 || rcChildWnd.Height() >= rcParentWnd.Height())
					{
						pPane->m_pParentSplittedLayout->m_pParentPane->Combine();
						return;
					}
					pPane->m_pParentSplittedLayout->m_firstPaneSizeRatio = static_cast<double>(rcChildWnd.bottom - rcParentWnd.top) / rcParentWnd.Height();
					break;
				}
			}
		}
	}
	
	SplittedLayout::SplittedLayout(Pane* pParentPane, bool bHorizontal, double ratio, std::vector<CWnd*>& wndList)
		: m_pParentPane(pParentPane)
		, m_bHorizontal(bHorizontal)
		, m_firstPaneSizeRatio(ratio)
		, m_pPaneFirst(new Pane(this))
		, m_pPaneSecond(new Pane(this))
	{
		m_pPaneFirst->m_wndList = wndList;
		m_pPaneSecond->SetDefaultOpenPane();
	}

	void LayoutManager::SetTileLayoutEnabled(bool bEnabled)
	{
		if (m_bEnabled == bEnabled)
			return;

		m_bEnabled = bEnabled;
		
		HWND hWndMDIActive = m_pMDIFrameWnd->MDIGetActive()->GetSafeHwnd();
		if (hWndMDIActive == nullptr)
			return;

		m_pMDIFrameWnd->MDICascade();

		for (CWnd* pWnd = m_pMDIFrameWnd->MDIGetActive()->GetParent()->GetTopWindow(); pWnd; pWnd = pWnd->GetNextWindow())
		{
			DWORD dwStyle = pWnd->GetStyle();
			if (m_bEnabled)
			{
				SetWindowLong(pWnd->m_hWnd, GWL_STYLE, dwStyle & ~(WS_CAPTION));
			}
			else
			{
				SetWindowLong(pWnd->m_hWnd, GWL_STYLE, dwStyle | WS_CAPTION);
				pWnd->ShowWindow(SW_HIDE);
				pWnd->ShowWindow(SW_SHOW);
			}
		}

		UpdateChildWindows();
	}

	void LayoutManager::Tile(bool bHorizontal)
	{
		m_pPane->Tile(bHorizontal);
		UpdateChildWindows();
	}

	bool LayoutManager::SplitActivePane(bool bHorizontal, double ratio)
	{
		bool result = false;
		if (Pane* pPane = GetActivePane())
			result = pPane->Split(bHorizontal, ratio);
		if (result)
			UpdateChildWindows();
		return result;
	}

	bool LayoutManager::CombineActivePane()
	{
		bool result = false;
		if (Pane* pPane = GetActivePane())
			result = pPane->Combine();
		if (result)
			UpdateChildWindows();
		return result;
	}

	void LayoutManager::SetActivePaneAsDefaultOpenPane()
	{
		if (Pane* pPane = GetActivePane())
			pPane->SetDefaultOpenPane();
	}

	CRect LayoutManager::GetDefaultOpenPaneRect() const
	{
		CRect rc = GetMainRect();
		if (Pane* pPane = m_pPane->GetDefaultOpenPane())
			return pPane->GetRect(rc);
		return rc;
	}

	void LayoutManager::NotifyMainResized()
	{
		UpdateChildWindows();
	}

	void LayoutManager::NotifyChildOpened(CWnd* pChildWnd)
	{
		if (Pane* pPane = m_pPane->GetDefaultOpenPane())
			pPane->AddWindow(pChildWnd);
	}

	void LayoutManager::NotifyChildClosed(CWnd* pChildWnd)
	{
		if (Pane* pPane = m_pPane->FindPaneByWindow(pChildWnd))
		{
			pPane->RemoveWindow(pChildWnd);
			if (pPane->GetWindowCount() == 0)
			{
				pPane->Combine();
				UpdateChildWindows();
			}
		}
	}

	void LayoutManager::NotifyChildResized(CWnd* pChildWnd)
	{
		if (m_bInResizing)
			return;
		Pane* pPane = m_pPane->FindPaneByWindow(pChildWnd);
		if (pPane == nullptr)
			return;
		CRect rcMain = GetMainRect();
		CRect rcChild = GetChildRect(pChildWnd, rcMain);
		pPane->UpdateSizeRatio(rcMain, rcChild);

		UpdateChildWindows();
	}

	Pane* LayoutManager::FindPaneByPosition(CPoint& pt) const
	{
		CRect rc = GetMainRect();
		for (CWnd* pWnd = m_pMDIFrameWnd->MDIGetActive()->GetParent()->GetTopWindow(); pWnd; pWnd = pWnd->GetNextWindow())
		{
			if (Pane* pPane = m_pPane->FindPaneByWindow(pWnd))
			{
				CRect rcChild = pPane->GetRect(rc);
				if (rcChild.left <= pt.x && pt.x < rcChild.right &&
					rcChild.top <= pt.y && pt.y < rcChild.bottom)
					return pPane;
			}
		}
		return nullptr;
	}

	Pane* LayoutManager::GetActivePane() const
	{
		if (CWnd* pWnd = m_pMDIFrameWnd->MDIGetActive())
			return m_pPane->FindPaneByWindow(pWnd);
		return nullptr;
	}

	CRect LayoutManager::AdjustChildRect(const CRect& rcMain, const CRect& rc, DWORD dwStyle, DWORD dwExStyle, int dpi) const
	{
		CRect rcChild = rc;
		CRect rcChildOrg = rcChild;
		DpiAware::AdjustWindowRectExForDpi(rcChild, dwStyle, false, dwExStyle, dpi);
		if (rcChildOrg.right < rcMain.right)
			rcChild.right = rcChildOrg.right;
		if (rcChildOrg.bottom < rcMain.bottom)
			rcChild.bottom = rcChildOrg.bottom;
		return rcChild;
	}

	void LayoutManager::UpdateChildWindows()
	{
		if (!m_bEnabled || m_pMDIFrameWnd->MDIGetActive()->GetSafeHwnd() == nullptr)
			return;
		m_bInResizing = true;
		CRect rc = GetMainRect();
		CWnd* pWndMDIClient = CWnd::FromHandle(m_pMDIFrameWnd->m_hWndMDIClient);
		const int dpi = DpiAware::GetDpiForWindow(m_pMDIFrameWnd->m_hWndMDIClient);
		CRect rcMainMargin;
		DpiAware::AdjustWindowRectExForDpi(rcMainMargin, pWndMDIClient->GetStyle(), false, pWndMDIClient->GetExStyle(), dpi);
		HDWP hdwp = BeginDeferWindowPos(8);
		for (CWnd* pWnd = m_pMDIFrameWnd->MDIGetActive()->GetParent()->GetTopWindow(); pWnd; pWnd = pWnd->GetNextWindow())
		{
			if (Pane* pPane = m_pPane->FindPaneByWindow(pWnd))
			{
				CRect rcChild = AdjustChildRect(rc, pPane->GetRect(rc), pWnd->GetStyle(), pWnd->GetExStyle(), dpi);
				DeferWindowPos(hdwp, pWnd->m_hWnd, nullptr,
					rcChild.left - rc.left + rcMainMargin.left,
					rcChild.top - rc.top + rcMainMargin.top,
					rcChild.Width(), rcChild.Height(), SWP_NOZORDER);
			}
		}
		EndDeferWindowPos(hdwp);
		m_bInResizing = false;
	}

	CRect LayoutManager::GetMainRect() const
	{
		CRect rc;
		CWnd *pWnd = CWnd::FromHandle(m_pMDIFrameWnd->m_hWndMDIClient);
		pWnd->GetWindowRect(rc);
		return rc;
	}

	CRect LayoutManager::GetChildRect(CWnd* pChildWnd, const CRect& rcMain) const
	{
		const int dpi = DpiAware::GetDpiForWindow(pChildWnd->m_hWnd);
		CRect rc, rcOuter;
		pChildWnd->GetWindowRect(rc);
		DpiAware::AdjustWindowRectExForDpi(rcOuter, pChildWnd->GetStyle(), false, pChildWnd->GetExStyle(), dpi);
		CRect rcChild { rc.left - rcOuter.left, rc.top - rcOuter.top, rc.right - rcOuter.right, rc.bottom - rcOuter.bottom};
		if (rc.right < rcMain.right)
			rcChild.right = rc.right;
		if (rc.bottom < rcMain.bottom)
			rcChild.bottom = rc.bottom;
		return rcChild;
	}
}
