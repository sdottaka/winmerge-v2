/** 
 * @file  MDITabBar.h
 *
 * @brief Declaration file for MDITabBar
 *
 */
#pragma once

#include "utils/DpiAware.h"

/**
 * @brief Class for Tab bar.
 */
class CMDITabBar : public DpiAware::CDpiAwareWnd<CControlBar>
{
	DECLARE_DYNAMIC(CMDITabBar)

private:
	enum {MDITABBAR_MINTITLELENGTH = 8, MDITABBAR_MAXTITLELENGTH = 64};

	bool m_bInSelchange;
	CMDIFrameWnd *m_pMainFrame;
	bool m_bMouseTracking;
	bool m_bCloseButtonDown;
	CRect m_rcCurrentCloseButtom;
	bool  m_bAutoMaxWidth;
	int   m_nDraggingTabItemIndex;
	CFont m_font;
	int m_cxSMIcon;

public:
	CMDITabBar() : m_bInSelchange(false), m_pMainFrame(nullptr), m_bMouseTracking(false), m_bCloseButtonDown(false), m_bAutoMaxWidth(true), m_nDraggingTabItemIndex(-1), m_cxSMIcon(0) {}
	virtual ~CMDITabBar() {}
	BOOL Create(CMDIFrameWnd* pParentWnd);
	void UpdateTabs();
	bool GetAutoMaxWidth() const { return m_bAutoMaxWidth; }
	void SetAutoMaxWidth(bool bAutoMaxWidth) { m_bAutoMaxWidth = bAutoMaxWidth; }

	virtual void OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler) {}
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);

	int GetItemCount() const
	{ ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, TCM_GETITEMCOUNT, 0, 0L); }
	BOOL GetItem(int nItem, TCITEM* pTabCtrlItem) const
	{ ASSERT(::IsWindow(m_hWnd)); return (BOOL)::SendMessage(m_hWnd, TCM_GETITEM, nItem, (LPARAM)pTabCtrlItem); }
	BOOL SetItem(int nItem, TCITEM* pTabCtrlItem)
	{ ASSERT(::IsWindow(m_hWnd)); return (BOOL)::SendMessage(m_hWnd, TCM_SETITEM, nItem, (LPARAM)pTabCtrlItem); }
	BOOL InsertItem(int nItem, TCITEM* pTabCtrlItem)
	{ ASSERT(::IsWindow(m_hWnd)); return (BOOL)::SendMessage(m_hWnd, TCM_INSERTITEM, nItem, (LPARAM)pTabCtrlItem); }
	BOOL DeleteItem(int nItem)
	{ ASSERT(::IsWindow(m_hWnd)); return (BOOL)::SendMessage(m_hWnd, TCM_DELETEITEM, nItem, 0L); }
	int GetCurSel() const
	{ ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, TCM_GETCURSEL, 0, 0L); }
	int SetCurSel(int nItem)
	{ ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, TCM_SETCURSEL, nItem, 0L); }
	int HitTest(TCHITTESTINFO* pHitTestInfo) const
	{ ASSERT(::IsWindow(m_hWnd)); return (int)::SendMessage(m_hWnd, TCM_HITTEST, 0, (LPARAM) pHitTestInfo); }
	BOOL GetItemRect(int nItem, LPRECT lpRect) const
	{ ASSERT(::IsWindow(m_hWnd)); return (BOOL)::SendMessage(m_hWnd, TCM_GETITEMRECT, (WPARAM)nItem, (LPARAM)lpRect); }

protected:
	//{{AFX_MSG(CMDITabBar)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg BOOL OnSelchange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg LRESULT OnDpiChangedBeforeParent(WPARAM, LPARAM);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	int determineIconSize() const;
	CRect GetCloseButtonRect(int nItem) const;
	int GetItemIndexFromPoint(CPoint pt) const;
	void SwapTabs(int nIndexA, int nIndexB);
};
