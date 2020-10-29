/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  MergeStatusBar.cpp
 *
 * @brief Implementation file for CMergeStatusBar class
 */

#include "stdafx.h"
#include "MergeStatusBar.h"
#include "charsets.h"
#include "unicoder.h"
#include "SyntaxColors.h"
#include "Merge.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/** @brief RO status panel width (point) */
static const UINT RO_PANEL_WIDTH = 20;
/** @brief Encoding status panel width (point) */
static const UINT ENCODING_PANEL_WIDTH = 90;
/** @brief EOL type status panel width (point) */
static const UINT EOL_PANEL_WIDTH = 30;

/**
 * @brief Statusbar pane indexes
 */
enum
{
	PANE_PANE0_INFO = 0,
	PANE_PANE0_ENCODING,
	PANE_PANE0_EOL,
	PANE_PANE0_RO,
	PANE_PANE1_INFO,
	PANE_PANE1_ENCODING,
	PANE_PANE1_EOL,
	PANE_PANE1_RO,
	PANE_PANE2_INFO,
	PANE_PANE2_ENCODING,
	PANE_PANE2_EOL,
	PANE_PANE2_RO,
};

const int nColumnsPerPane = PANE_PANE1_INFO - PANE_PANE0_INFO;

/**
 * @brief Bottom statusbar panels and indicators
 */
static UINT indicatorsBottom[] =
{
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
	ID_SEPARATOR,
};

BEGIN_MESSAGE_MAP(CMergeStatusBar, DpiAware::CDpiAwareWnd<CStatusBar>)
	ON_MESSAGE(WM_DPICHANGED_BEFOREPARENT, OnDpiChangedBeforeParent)
END_MESSAGE_MAP()

/**
 * @brief Constructor.
 */
CMergeStatusBar::CMergeStatusBar() : m_nPanes(2), m_bDiff{}, m_dispFlags{}
{
	for (int pane = 0; pane < sizeof(m_status) / sizeof(m_status[0]); pane++)
	{
		m_status[pane].m_pWndStatusBar = this;
		m_status[pane].m_base = PANE_PANE0_INFO + pane * nColumnsPerPane;
	}
	Options::DiffColors::Load(GetOptionsMgr(), m_cachedColors);
}

/**
 * @brief Destructor.
 */
CMergeStatusBar::~CMergeStatusBar()
{
}

BOOL CMergeStatusBar::Create(CWnd* pParentWnd)
{
	if (! __super::Create(pParentWnd))
		return FALSE;

	SetIndicators(indicatorsBottom, sizeof(indicatorsBottom) / sizeof(UINT));

	// Set text to read-only info panes
	// Text is hidden if file is writable
	String sText = _("RO");
	for (auto&& p : { PANE_PANE0_RO, PANE_PANE1_RO, PANE_PANE2_RO })
		SetPaneText(p, sText.c_str(), TRUE);

	for (int pane = 0; pane < 3; pane++)
	{
		SetPaneStyle(PANE_PANE0_INFO     + pane * nColumnsPerPane, SBPS_NORMAL);
		SetPaneStyle(PANE_PANE0_ENCODING + pane * nColumnsPerPane, SBPS_OWNERDRAW);
		SetPaneStyle(PANE_PANE0_EOL      + pane * nColumnsPerPane, SBPS_OWNERDRAW);
		SetPaneStyle(PANE_PANE0_RO       + pane * nColumnsPerPane, SBPS_NORMAL);
	}

	return TRUE;
};

void CMergeStatusBar::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	const int pbase = PANE_PANE0_INFO + (lpDrawItemStruct->itemID - PANE_PANE0_INFO) % nColumnsPerPane;
	const int pcur = (lpDrawItemStruct->itemID - PANE_PANE0_INFO) / nColumnsPerPane;
	if (pcur >= m_nPanes)
		return;
	std::vector<CString> ptext(m_nPanes);
	for (int pane = 0; pane < m_nPanes; ++pane)
		ptext[pane] = GetPaneText(pbase + pane * nColumnsPerPane);
	const bool diff = !std::equal(ptext.begin() + 1, ptext.end(), ptext.begin());

	if (!ptext[pcur].IsEmpty())
		m_dispFlags[pbase] |= 1 << pcur;
	const bool displayedAll = m_dispFlags[pbase] == static_cast<unsigned>((1 << m_nPanes) - 1);

	if (displayedAll && m_bDiff[pbase] != diff)
	{
		m_bDiff[pbase] = diff;
		for (int pane = 0; pane < m_nPanes; ++pane)
		{
			RECT rcColumn;
			GetItemRect(pbase + pane * nColumnsPerPane, &rcColumn);
			InvalidateRect(&rcColumn);
		}
		return;
	}

	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);
	if (displayedAll && diff)
	{
		dc.SetBkMode(OPAQUE);
		dc.SetTextColor(m_cachedColors.clrWordDiffText == -1 ?
			theApp.GetMainSyntaxColors()->GetColor(COLORINDEX_NORMALTEXT) : m_cachedColors.clrWordDiffText);
		dc.SetBkColor(m_cachedColors.clrWordDiff);
		dc.ExtTextOut(
			lpDrawItemStruct->rcItem.left, lpDrawItemStruct->rcItem.top,
			ETO_OPAQUE, &lpDrawItemStruct->rcItem, _T(""), nullptr );
	}
	else
	{
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(GetSysColor(COLOR_BTNTEXT));
	}
	dc.DrawText(ptext[pcur], &lpDrawItemStruct->rcItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	dc.Detach();
}

void CMergeStatusBar::Resize(int widths[])
{
	// Set bottom statusbar panel widths
	// Kimmo - I don't know why 4 seems to be right for me
	int borderWidth = 4; // GetSystemMetrics(SM_CXEDGE);

	for (int pane = 0; pane < m_nPanes; pane++)
	{
		int paneWidth = widths[pane] - (PointToPixel(RO_PANEL_WIDTH + ENCODING_PANEL_WIDTH + EOL_PANEL_WIDTH) +
			(3 * borderWidth));
		if (paneWidth < borderWidth)
			paneWidth = borderWidth;

		SetPaneInfo(PANE_PANE0_INFO + pane * nColumnsPerPane, ID_STATUS_PANE0FILE_INFO + pane,
			SBPS_NORMAL, paneWidth);
		SetPaneInfo(PANE_PANE0_ENCODING + pane * nColumnsPerPane, ID_STATUS_PANE0FILE_ENCODING + pane,
			SBT_OWNERDRAW, PointToPixel(ENCODING_PANEL_WIDTH) - borderWidth);
		SetPaneInfo(PANE_PANE0_RO + pane * nColumnsPerPane, ID_STATUS_PANE0FILE_RO + pane,
			SBPS_NORMAL, PointToPixel(RO_PANEL_WIDTH) - borderWidth);
		SetPaneInfo(PANE_PANE0_EOL + pane * nColumnsPerPane, ID_STATUS_PANE0FILE_EOL + pane,
			SBT_OWNERDRAW, PointToPixel(EOL_PANEL_WIDTH) - borderWidth);
	}
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CMergeStatusBar::UpdateResources()
{
	for (int pane = 0; pane < m_nPanes; pane++)
		m_status[pane].UpdateResources();
}

/// Bridge class which implements the interface from crystal editor to frame status line display
CMergeStatusBar::MergeStatus::MergeStatus()
: m_nColumn(0)
, m_nColumns(0)
, m_nChar(0)
, m_nChars(0)
, m_nCodepage(-1)
, m_bHasBom(false)
, m_pWndStatusBar(nullptr)
, m_base(0)
{
}

/// Send status line info (about one side of merge view) to screen
void CMergeStatusBar::MergeStatus::Update()
{
	if (IsWindow(m_pWndStatusBar->m_hWnd))
	{
		CString strInfo, strEncoding;
		if (m_nChars == -1)
		{
			strInfo.Format(_("Line: %s").c_str(),
				m_sLine.c_str());
		}
		else if (m_sEolDisplay.empty())
		{
			strInfo.Format(_("Ln: %s  Col: %d/%d  Ch: %d/%d").c_str(),
				m_sLine.c_str(), m_nColumn, m_nColumns, m_nChar, m_nChars, m_nCodepage, m_sCodepageName.c_str());
		}
		else
		{
			strInfo.Format(_("Ln: %s  Col: %d/%d  Ch: %d/%d  EOL: %s").c_str(),
				m_sLine.c_str(), m_nColumn, m_nColumns, m_nChar, m_nChars, m_sEolDisplay.c_str(), m_nCodepage, m_sCodepageName.c_str());
		}

		if (m_nCodepage > 0)
			strEncoding.Format(_("%s").c_str(), m_sCodepageName.c_str());
		m_pWndStatusBar->SetPaneText(m_base, strInfo);
		m_pWndStatusBar->SetPaneText(m_base + 1, strEncoding);
	}
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CMergeStatusBar::MergeStatus::UpdateResources()
{
	Update();
}

/// Visible representation of eol
static String EolString(const String & sEol)
{
	if (sEol == _T("\r\n"))
	{
		return _("CRLF");
	}
	if (sEol == _T("\n"))
	{
		return _("LF");
	}
	if (sEol == _T("\r"))
	{
		return _("CR");
	}
	if (sEol.empty())
	{
		return _("None");
	}
	if (sEol == _T("hidden"))
		return _T("");
	return _T("?");
}

/// Receive status line info from crystal window and display
void CMergeStatusBar::MergeStatus::SetLineInfo(LPCTSTR szLine, int nColumn,
		int nColumns, int nChar, int nChars, LPCTSTR szEol, int nCodepage, bool bHasBom)
{
	if (m_sLine != szLine || m_nColumn != nColumn || m_nColumns != nColumns ||
		m_nChar != nChar || m_nChars != nChars || m_sEol != szEol != 0 || m_nCodepage != nCodepage || m_bHasBom != bHasBom)
	{
		USES_CONVERSION;
		m_sLine = szLine;
		m_nColumn = nColumn;
		m_nColumns = nColumns;
		m_nChar = nChar;
		m_nChars = nChars;
		m_sEol = szEol;
		m_sEolDisplay = EolString(m_sEol);
		if (m_nCodepage != nCodepage || m_bHasBom != bHasBom)
		{
			const char *pszCodepageName = GetEncodingNameFromCodePage(nCodepage);
			m_sCodepageName = pszCodepageName ? ucr::toTString(pszCodepageName) : _T("");
			if (bHasBom)
				m_sCodepageName += _T(" BOM");
		}
		m_nCodepage = nCodepage;
		m_bHasBom = bHasBom;
		Update();
	}
}

LRESULT CMergeStatusBar::OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam)
{
	__super::OnDpiChangedBeforeParent(wParam, lParam);
	
	if (m_font.m_hObject == GetFont()->m_hObject)
		m_font.DeleteObject();

	LOGFONT lfStatusFont;
	if (DpiAware::GetNonClientLogFont(lfStatusFont, offsetof(NONCLIENTMETRICS, lfStatusFont), GetDpi()))
		m_font.CreateFontIndirect(&lfStatusFont);

	SetFont(&m_font);
	return 0;
}

