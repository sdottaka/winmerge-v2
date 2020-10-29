/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  EditorFilePathBar.h
 *
 * @brief Interface of the CEditorFilePathBar class.
 *
 */
#pragma once

#include "FilepathEdit.h"
#include "utils/DpiAware.h"
#include <functional>

/**
 * Interface to update the header data.
 */
class IHeaderBar
{
public:
	virtual String GetText(int pane) const = 0;
	virtual void SetText(int pane, const String& sString) = 0;
	virtual void SetActive(int pane, bool bActive) = 0;
	virtual void SetPaneCount(int nPanes) = 0;
	virtual void Resize() = 0;
	virtual void SetOnSetFocusCallback(const std::function<void(int)> callbackfunc) = 0;
};


/**
 * @brief A dialog bar with two controls for left/right path.
 * This class is a dialog bar for the both files path in the editor. 
 * The bar looks like a statusBar (font, height). The control
 * displays a tip for each path (as a tooltip). 
 */
class CEditorFilePathBar : public DpiAware::CDpiAwareWnd<CDialogBar>, public IHeaderBar
{
public : 
	CEditorFilePathBar();
	~CEditorFilePathBar();

	BOOL Create( CWnd* pParentWnd);
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);

// Dialog Data
	enum { IDD = IDD_EDITOR_HEADERBAR };
	
	void Resize() override;
	void Resize(int widths[]);
	void SetOnSetFocusCallback(const std::function<void(int)> callbackfunc) override;

	// Implement IFilepathHeaders
	void SetText(int pane, const String& sString) override;
	String GetText(int pane) const override;
	void SetActive(int pane, bool bActive) override;
	void SetPaneCount(int nPanes) override;

protected:
	//{{AFX_MSG(CEditorFilePathBar)
	afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pTTTStruct, LRESULT * pResult );
	afx_msg void OnSetFocusEdit(UINT id);
	afx_msg LRESULT OnDpiChangedBeforeParent(WPARAM, LPARAM);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP();

private:
	// this dialog uses custom edit boxes
	CFilepathEdit m_Edit[3]; /**< Edit controls. */
	CFont m_font; /**< Font for editcontrols */
	int m_nPanes;
	std::function<void(int)> m_callbackfunc;
};

inline void CEditorFilePathBar::SetPaneCount(int nPanes)
{
	m_nPanes = nPanes;
}

/** 
 * @brief Set callback function on EN_SETFOCUS notification
 */
inline void CEditorFilePathBar::SetOnSetFocusCallback(const std::function<void(int)> callbackfunc)
{
	m_callbackfunc = callbackfunc;
}

