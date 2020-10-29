// SPDX-License-Identifier: BSL-1.0
// Copyright (c) 2020 Takashi Sawanaka
//
// Use, modification and distribution are subject to the 
// Boost Software License, Version 1.0. (See accompanying file 
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <afxwin.h>
#include <type_traits>
#include "mfc_templ_defines.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#define WM_DPICHANGED_BEFOREPARENT 0x02E2
#define WM_DPICHANGED_AFTERPARENT 0x02E3
#endif
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif

namespace DpiAware
{
	struct NONCLIENTMETRICS6 : public NONCLIENTMETRICS { int iPaddedBorderWidth; };

	using AdjustWindowRectExForDpiType = BOOL(__stdcall*)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi);
	using GetDpiForWindowType = UINT(__stdcall*)(HWND hwnd);
	using SystemParametersInfoForDpiType = BOOL(__stdcall*)(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi);
	using GetSystemMetricsForDpiType = int(__stdcall*)(int nIndex, UINT dpi);
	using OpenThemeDataForDpiType = HTHEME(__stdcall*)(HWND hwnd, LPCWSTR pszClassList, UINT dpi);
	using LoadIconWithScaleDownType = HRESULT(__stdcall*)(HINSTANCE hinst, PCWSTR pszName, int cx, int cy, HICON* phico);

	extern bool DpiAwareSupport;
	extern int DPIOnInit;

	extern AdjustWindowRectExForDpiType AdjustWindowRectExForDpi;
	extern GetDpiForWindowType GetDpiForWindow;
	extern SystemParametersInfoForDpiType SystemParametersInfoForDpi;
	extern GetSystemMetricsForDpiType GetSystemMetricsForDpi;
	extern OpenThemeDataForDpiType OpenThemeDataForDpi;
	extern LoadIconWithScaleDownType LoadIconWithScaleDown;

	void GetPointLogFont(LOGFONT& logFont, float point, const TCHAR* lfFaceName, int dpi);
	bool GetNonClientLogFont(LOGFONT& logFont, size_t memberOffset, int dpi);
	void UpdateAfxDataSysMetrics(int dpi);
	void ListView_UpdateColumnWidths(HWND hwnd, int olddpi, int newdpi);
	void Dialog_UpdateControlInnerWidths(HWND hwnd, int olddpi, int newdpi);
	CSize Dialog_CalcUpdatedSize(const TCHAR* pszFontFace, int nFontSize, const CSize& oldsize, int olddpi, int newdpi);
	CSize Dialog_GetSizeFromTemplate(const TCHAR* pTemplateID, const TCHAR* pszFaceName, int nFontSize);
	HIMAGELIST LoadShellImageList(int dpi);
	template <class T>
	T MulDivRect(const T* p, int nNumerator, int nDenominator)
	{
		T rc;
		rc.left   = MulDiv(p->left,   nNumerator, nDenominator);
		rc.top    = MulDiv(p->top,    nNumerator, nDenominator);
		rc.right  = MulDiv(p->right,  nNumerator, nDenominator);
		rc.bottom = MulDiv(p->bottom, nNumerator, nDenominator);
		return rc;
	}

	template<class Base>
	class CDpiAwareWnd : public Base
	{
	public:
		using this_type = CDpiAwareWnd;
		using base_type = Base;
		using Base::Base;
		
		int GetDpi() const { return m_dpi; }

		int PointToPixel(int point) const { return MulDiv(point, m_dpi, 72); }
		template<class T>
		int PointToPixel(T point) const { return static_cast<int>((point * m_dpi) / 72); }
		void UpdateDpi()
		{
			m_dpi = GetDpiForWindow(m_hWnd);
		}
		int GetSystemMetrics(int nIndex) { return GetSystemMetricsForDpi(nIndex, m_dpi); }
		BOOL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle)
		{
			return DpiAware::AdjustWindowRectExForDpi(lpRect, dwStyle, bMenu, dwExStyle, m_dpi);
		}

		virtual void CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType = CWnd::adjustBorder)
		{
			DWORD dwExStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
			if (nAdjustType == 0)
				dwExStyle &= ~WS_EX_CLIENTEDGE;
			DWORD dwStyle = (DWORD)GetWindowLong(m_hWnd, GWL_STYLE);
			DpiAware::AdjustWindowRectExForDpi(lpClientRect, dwStyle, FALSE, dwExStyle, m_dpi);
		}

		bool IsDifferentDpiFromSystemDpi() const
		{
			return DPIOnInit != m_dpi;
		}

		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct)
		{
			UpdateDpi();
			return __super::OnCreate(lpCreateStruct);
		}

		afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam)
		{
			int olddpi = m_dpi;
			UpdateDpi();
			return 0;
		}

		afx_msg LRESULT OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam)
		{
			UpdateDpi();
			return 0;
		}

		int m_dpi = DPIOnInit;

		BEGIN_MESSAGE_MAP_INLINE(this_type, base_type)
			ON_WM_CREATE()
			ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
			ON_MESSAGE(WM_DPICHANGED_BEFOREPARENT, OnDpiChangedBeforeParent)
		END_MESSAGE_MAP_INLINE()
	};

	template<class Base>
	class CDpiAwareDialog : public CDpiAwareWnd<Base>
	{
	public:
		using this_type = CDpiAwareDialog;
		using base_type = CDpiAwareWnd<Base>;
		using CDpiAwareWnd<Base>::CDpiAwareWnd;

		CRect m_rcInit;

		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct)
		{
			GetClientRect(m_rcInit);
			return __super::OnCreate(lpCreateStruct);
		}

		afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam)
		{
			int olddpi = m_dpi;
			bool bDynamicLayoutEnabled = IsDynamicLayoutEnabled();
			CSize sizeMin;
			if (bDynamicLayoutEnabled)
			{
//				sizeMin = GetDynamicLayout()->GetMinSize();
//				CRect rc = m_rcInit;
//				AdjustWindowRectEx(&rc, GetStyle(), false, GetExStyle());
//				SetWindowPos(nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
//				GetDynamicLayout()->Adjust();
				EnableDynamicLayout(FALSE);
			}
			UpdateDpi();
			Default();
			if (bDynamicLayoutEnabled)
			{
				/*
				CFont *pFont = GetFont();
				LOGFONT lfFont;
				pFont->GetLogFont(&lfFont);
				CSize size = Dialog_GetSizeFromTemplate(m_lpszTemplateName, lfFont.lfFaceName, MulDiv(abs(lfFont.lfHeight), 72, m_dpi));
				SetWindowPos(nullptr, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER);
				LoadDynamicLayoutResource(m_lpszTemplateName);
				*/
			}
			return 0;
		}

		afx_msg LRESULT OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam)
		{
			return OnDpiChanged(wParam, lParam);
		}

		BEGIN_MESSAGE_MAP_INLINE(this_type, base_type)
			ON_WM_CREATE()
			ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
			ON_MESSAGE(WM_DPICHANGED_BEFOREPARENT, OnDpiChangedBeforeParent)
		END_MESSAGE_MAP_INLINE()
	};
}

