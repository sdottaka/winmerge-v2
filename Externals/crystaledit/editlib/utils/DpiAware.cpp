// SPDX-License-Identifier: BSL-1.0
// Copyright (c) 2020 Takashi Sawanaka
//
// Use, modification and distribution are subject to the 
// Boost Software License, Version 1.0. (See accompanying file 
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "StdAfx.h"
#include "DpiAware.h"
#include <commoncontrols.h>
#include <../src/mfc/afximpl.h>

namespace DpiAware
{
	GetDpiForWindowType GetDpiForWindow = nullptr;
	SystemParametersInfoForDpiType SystemParametersInfoForDpi = nullptr;
	GetSystemMetricsForDpiType GetSystemMetricsForDpi = nullptr;
	OpenThemeDataForDpiType OpenThemeDataForDpi = nullptr;
	AdjustWindowRectExForDpiType AdjustWindowRectExForDpi = nullptr;
	LoadIconWithScaleDownType LoadIconWithScaleDown = nullptr;

	int DPIOnInit = 0;

	bool DpiAwareSupport = []()
	{
		DPIOnInit = MulDiv(USER_DEFAULT_SCREEN_DPI, ::GetSystemMetrics(SM_CXSMICON), 16);
		HMODULE hLibraryUser32 = GetModuleHandleW(L"user32.dll");
		if (hLibraryUser32)
		{
			AdjustWindowRectExForDpi = reinterpret_cast<AdjustWindowRectExForDpiType>(GetProcAddress(hLibraryUser32, "AdjustWindowRectExForDpi"));
			GetDpiForWindow = reinterpret_cast<GetDpiForWindowType>(GetProcAddress(hLibraryUser32, "GetDpiForWindow"));
			SystemParametersInfoForDpi = reinterpret_cast<SystemParametersInfoForDpiType>(GetProcAddress(hLibraryUser32, "SystemParametersInfoForDpi"));
			GetSystemMetricsForDpi = reinterpret_cast<GetSystemMetricsForDpiType>(GetProcAddress(hLibraryUser32, "GetSystemMetricsForDpi"));
		}
		HMODULE hLibraryComctl32 = GetModuleHandleW(L"comctl32.dll");
		if (hLibraryComctl32)
		{
			LoadIconWithScaleDown = reinterpret_cast<LoadIconWithScaleDownType>(GetProcAddress(hLibraryComctl32, "LoadIconWithScaleDown"));
		}
		HMODULE hLibraryUxTheme = GetModuleHandleW(L"uxtheme.dll");
		if (hLibraryUxTheme)
		{
			OpenThemeDataForDpi = reinterpret_cast<OpenThemeDataForDpiType>(GetProcAddress(hLibraryUxTheme, "OpenThemeDataForDpi"));
		}
		if (!AdjustWindowRectExForDpi)
			AdjustWindowRectExForDpi = [](LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi) -> BOOL
				{ return AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle); };
		if (!GetDpiForWindow)
			GetDpiForWindow = [](HWND hwnd) -> UINT { return CClientDC(CWnd::FromHandle(hwnd)).GetDeviceCaps(LOGPIXELSX); };
		if (!SystemParametersInfoForDpi)
			SystemParametersInfoForDpi = [](UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi) -> BOOL { return SystemParametersInfo(uiAction, uiParam, pvParam, fWinIni); };
		if (!GetSystemMetricsForDpi)
			GetSystemMetricsForDpi = [](int nIndex, UINT dpi) -> int { return GetSystemMetrics(nIndex); };
		if (!LoadIconWithScaleDown)
			LoadIconWithScaleDown = [](HINSTANCE hinst, PCWSTR pszName, int cx, int cy, HICON *phico) -> HRESULT
			  { *phico = LoadIconW(hinst, pszName); return *phico != nullptr ? S_OK : E_FAIL; };
		if (!OpenThemeDataForDpi)
		{
			OpenThemeDataForDpi = [](HWND hwnd, LPCWSTR pszClassList, UINT dpi) -> HTHEME { return OpenThemeData(hwnd, pszClassList); };
			return false;
		}
		return true;
	}();

	void GetPointLogFont(LOGFONT& logFont, float point, const TCHAR* lfFaceName, int dpi)
	{
		LOGFONT lfv{};
		lfv.lfHeight = static_cast<long>(-point * dpi / 72.0f);
		lfv.lfCharSet = DEFAULT_CHARSET;
		_tcscpy_s(lfv.lfFaceName, lfFaceName);
		logFont = lfv;
	}

	bool GetNonClientLogFont(LOGFONT& logFont, size_t memberOffset, int dpi)
	{
		DpiAware::NONCLIENTMETRICS6 ncm{};
		ncm.cbSize = sizeof(NONCLIENTMETRICS);
		if (DpiAware::DpiAwareSupport)
			ncm.cbSize = sizeof DpiAware::NONCLIENTMETRICS6;
		if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, ncm.cbSize, static_cast<NONCLIENTMETRICS *>(&ncm), 0, dpi))
			return false;
		memcpy(&logFont, (char*)&ncm + memberOffset, sizeof(LOGFONT));
		return true;
	}

	void UpdateAfxDataSysMetrics(int dpi)
	{
		afxData.cxIcon = GetSystemMetricsForDpi(SM_CXICON, dpi);
		afxData.cyIcon = GetSystemMetricsForDpi(SM_CYICON, dpi);
		afxData.cxVScroll = GetSystemMetricsForDpi(SM_CXVSCROLL, dpi) + AFX_CX_BORDER;
		afxData.cyHScroll = GetSystemMetricsForDpi(SM_CYHSCROLL, dpi) + AFX_CY_BORDER;
		afxData.cxPixelsPerInch = dpi;
		afxData.cyPixelsPerInch = dpi;
	}

	void TreeView_UpdateIndent(HWND hwnd, int olddpi, int newdpi)
	{
		int indent = TreeView_GetIndent(hwnd);
		TreeView_SetIndent(hwnd, MulDiv(indent, newdpi, olddpi));
	}

	void ListView_UpdateColumnWidths(HWND hwnd, int olddpi, int newdpi)
	{
		HWND hwndHeader = ListView_GetHeader(hwnd);
		const int nColumns = Header_GetItemCount(hwndHeader);
		for (int i = 0; i < nColumns; ++i)
		{
			const int nColumnWidth = MulDiv(ListView_GetColumnWidth(hwnd, i), newdpi, olddpi);
			ListView_SetColumnWidth(hwnd, i, nColumnWidth);
		}
	}

	void Dialog_UpdateControlInnerWidths(HWND hwnd, int olddpi, int newdpi)
	{
		struct Dpis { int olddpi, newdpi; HWND hwndParent; } dpis{ olddpi, newdpi, hwnd };
		auto enumfunc = [](HWND hwnd, LPARAM lParam) -> BOOL
		{
			const Dpis *pdpis = (const Dpis *)lParam;
			TCHAR name[256];
			GetClassName(hwnd, name, sizeof(name) / sizeof(TCHAR));
			if (_tcsicmp(name, _T("SysListView32")) == 0 && pdpis->hwndParent == GetParent(hwnd))
				ListView_UpdateColumnWidths(hwnd, pdpis->olddpi, pdpis->newdpi);
			else if (_tcsicmp(name, _T("SysTreeView32")) == 0 && pdpis->hwndParent == GetParent(hwnd))
				TreeView_UpdateIndent(hwnd, pdpis->olddpi, pdpis->newdpi);
			return TRUE;
		};
		EnumChildWindows(hwnd, enumfunc, (LPARAM)&dpis);
	}

	CSize Dialog_GetSizeFromTemplate(const TCHAR *pTemplateID, const TCHAR *pszFaceName, int nFontSize)
	{
		CDialogTemplate tmpl;
		CSize size;
		tmpl.Load(pTemplateID);
		tmpl.SetFont(pszFaceName, nFontSize);
		tmpl.GetSizeInPixels(&size);
		return size;
	}

	CSize Dialog_CalcUpdatedSize(const TCHAR *pszFontFace, int nFontSize, const CSize& oldsize, int olddpi, int newdpi)
	{
		UINT cxSysChar[2], cySysChar[2];
		LOGFONT lf[2] = {};
		CClientDC dc(nullptr);
		lf[0].lfHeight = -MulDiv(nFontSize, olddpi, 72);
		lf[0].lfWeight = FW_NORMAL;
		lf[0].lfCharSet = DEFAULT_CHARSET;
		_tcscpy_s(lf[0].lfFaceName, pszFontFace);
		lf[1] = lf[0];
		lf[1].lfHeight = -MulDiv(nFontSize, newdpi, 72);

		for (int i = 0; i < 2; ++i)
		{
			CFont font;
			font.CreateFontIndirect(&lf[i]);
			HFONT hFontOld = (HFONT)SelectObject(dc, font);
			TEXTMETRIC tm;
			dc.GetTextMetrics(&tm);
			cySysChar[i] = tm.tmHeight + tm.tmExternalLeading;
			SIZE size;
			GetTextExtentPoint32(dc, _T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"), 52, &size);
			cxSysChar[i] = (size.cx + 26) / 52;
			dc.SelectObject(hFontOld);
		}

		int newcx = MulDiv(oldsize.cx, cxSysChar[1], cxSysChar[0]);
		int newcy = MulDiv(oldsize.cy, cySysChar[1], cySysChar[0]);
		return { newcx, newcy };
	}

	HIMAGELIST LoadShellImageList(int dpi)
	{
		SHFILEINFO sfi{};
		if (dpi == DpiAware::DPIOnInit)
		{
			return (HIMAGELIST)SHGetFileInfo(_T(""), 0,
				&sfi, sizeof(sfi), SHGFI_SMALLICON | SHGFI_SYSICONINDEX);
		}
		else
		{
			int cxsmicon = ::GetSystemMetrics(SM_CXSMICON);
			int iconsize = MulDiv(16, dpi, USER_DEFAULT_SCREEN_DPI);
			int size = SHIL_EXTRALARGE;
			if (iconsize < cxsmicon * 2)
				size = SHIL_SMALL;
			else if (iconsize < cxsmicon * 3)
				size = SHIL_LARGE;
			IImageList *pImageList = nullptr;
			if (FAILED(SHGetImageList(size, IID_IImageList, (void**)&pImageList)))
				return nullptr;
			CComQIPtr<IImageList2> pImageList2(pImageList);
			if (pImageList2)
				pImageList2->Resize(iconsize, iconsize);
			return IImageListToHIMAGELIST(pImageList);
		}
	}
}

