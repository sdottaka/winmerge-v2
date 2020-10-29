/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  MergeStatusBar.h
 *
 * @brief Interface of the CMergeStatusBar class.
 *
 */
#pragma once

#include "MergeEditStatus.h"
#include "OptionsDiffColors.h"
#include "UnicodeString.h"
#include "utils/DpiAware.h"

class CMergeStatusBar : public DpiAware::CDpiAwareWnd<CStatusBar>
{
public : 
	CMergeStatusBar();
	~CMergeStatusBar();

	BOOL Create(CWnd* pParentWnd);
	void Resize(int widths[]);
	void SetPaneCount(int nPanes) { m_nPanes = nPanes; }
	void UpdateResources();
	IMergeEditStatus* GetIMergeEditStatus(int nPane) { return &m_status[nPane]; }
	void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

protected:

	//{{AFX_MSG(CEditorFilePathBar)
	afx_msg LRESULT OnDpiChangedBeforeParent(WPARAM, LPARAM);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP();

private:
	// this dialog uses custom edit boxes
	int m_nPanes;
	COLORSETTINGS m_cachedColors;
	bool m_bDiff[4];
	unsigned m_dispFlags[4];

protected:
	// Object that displays status line info for one side of a merge view
	class MergeStatus : public IMergeEditStatus
	{
	public:
		// ctr
		MergeStatus();
		// Implement MergeEditStatus
		void SetLineInfo(LPCTSTR szLine, int nColumn, int nColumns,
			int nChar, int nChars, LPCTSTR szEol, int nCodepage, bool bHasBom);
		void UpdateResources();
	protected:
		void Update();
	public:
		CMergeStatusBar* m_pWndStatusBar;
		int m_base; /**< 0 for left, 1 for right */
	private:
		String m_sLine;
		int m_nColumn; /**< Current column, tab-expanded */
		int m_nColumns; /**< Amount of columns, tab-expanded */
		int m_nChar; /**< Current char */
		int m_nChars; /**< Amount of chars in line */
		int m_nCodepage;
		bool m_bHasBom;
		String m_sEol;
		String m_sEolDisplay;
		String m_sCodepageName;
	};
	friend class MergeStatus; // MergeStatus accesses status bar
	MergeStatus m_status[3];
	CFont m_font;
};
