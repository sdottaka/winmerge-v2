// SPDX-License-Identifier: BSL-1.0
// Copyright (c) 2020 Takashi Sawanaka
//
// Use, modification and distribution are subject to the 
// Boost Software License, Version 1.0. (See accompanying file 
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <afxwin.h>
#include <memory>
#include <vector>

namespace MDITileLayout
{
	class SplittedLayout;

	class Pane
	{
		friend SplittedLayout;
	public:
		Pane(SplittedLayout* pParentSplittedLayout) : m_pParentSplittedLayout(pParentSplittedLayout) {}
		bool Split(bool bHorizontal, double ratio);
		bool Combine();
		void Tile(bool bHorizontal);
		CRect GetRect(const CRect& rcMainWnd) const;
		bool IsSplitted() const { return m_pSplittedLayout != nullptr; }
		void SetDefaultOpenPane();
		Pane* GetDefaultOpenPane() const;
		bool AddWindow(CWnd* pWnd);
		bool RemoveWindow(CWnd* pWnd);
		size_t GetWindowCount() const { return m_wndList.size(); }
		bool MoveWindow(Pane *pDstPane, CWnd* pWnd);
		Pane* FindPaneByWindow(CWnd* pWnd) const;
		void UpdateSizeRatio(const CRect& rcMainWnd, const CRect& rcChildWnd);

	protected:
		std::vector<CWnd*> m_wndList;
		std::unique_ptr<SplittedLayout> m_pSplittedLayout;
		SplittedLayout* m_pParentSplittedLayout;
	};

	class SplittedLayout
	{
		friend Pane;
	public:
		SplittedLayout(Pane* pParentPane, bool bHorizontal, double ratio, std::vector<CWnd*>& wndList);

	protected:
		Pane* m_pParentPane = nullptr;
		std::unique_ptr<Pane> m_pPaneFirst;
		std::unique_ptr<Pane> m_pPaneSecond;
		bool m_bDefaultOpenPaneIsFirst = false;
		bool m_bHorizontal = false;
		double m_firstPaneSizeRatio = 0.5;
	};

	class LayoutManager
	{
	public:
		LayoutManager(CMDIFrameWnd* pMDIFrameWnd) : m_pMDIFrameWnd(pMDIFrameWnd), m_pPane(new Pane(nullptr)) {}
		void  SetTileLayoutEnabled(bool bEnabled);
		bool  GetTileLayoutEnabled() const { return m_bEnabled; };
		void  Tile(bool bHorizontal);
		bool  SplitActivePane(bool bHorizontal, double ratio);
		bool  CombineActivePane();
		void  SetActivePaneAsDefaultOpenPane();
		CRect GetDefaultOpenPaneRect() const;
		void  NotifyMainResized();
		void  NotifyChildOpened(CWnd* pChlidWnd);
		void  NotifyChildClosed(CWnd* pChlidWnd);
		void  NotifyChildResized(CWnd* pChlidWnd);
		CRect AdjustChildRect(const CRect& rcMain, const CRect& rc, DWORD dwStyle, DWORD dwExStyle, int dpi) const;

	protected:
		Pane* FindPaneByPosition(CPoint& pt) const;
		Pane* GetActivePane() const;
		void  UpdateChildWindows();
		CRect GetMainRect() const;
		CRect GetChildRect(CWnd* pChildWnd, const CRect& rcMain) const;

		bool m_bInResizing = false;
		bool m_bEnabled = false;
		std::unique_ptr<Pane> m_pPane;
		CMDIFrameWnd* m_pMDIFrameWnd = nullptr;
	};
}
