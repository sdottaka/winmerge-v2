/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  OpenView.h
 *
 * @brief Declaration file for COpenView window
 *
 */

#pragma once

/////////////////////////////////////////////////////////////////////////////
// COpenView window
#include "SuperComboBox.h"
#include "FileTransform.h"
#include "PathContext.h"
#include "CMoveConstraint.h"
#include "TrDialogs.h"
#include <array>

class ProjectFile;
class COpenDoc;
class DropHandler;

/**
 * @brief The Open-View class.
 * The Open-View allows user to select paths to compare. In addition to
 * the two paths, there are controls for selecting filter and unpacker plugin.
 * If one of the paths is a project file, that project file is loaded,
 * overwriting possible other values in other dialog controls.
 * The dialog shows also a status of the selected paths (found/not found),
 * if enabled in the options (enabled by default).
 */
class COpenView : public DpiAware::CDpiAwareWnd<CFormView>, public DlgUtils<COpenView>
{
protected: // create from serialization only
	COpenView();
	DECLARE_DYNCREATE(COpenView)

public:
// Dialog Data
	//{{AFX_DATA(COpenView)
	enum { IDD = IDD_OPEN };
	CSuperComboBox	m_ctlExt;
	CSuperComboBox	m_ctlPath[3];
	String m_strPath[3];
	bool m_bReadOnly[3];
	PathContext m_files;
	bool	m_bRecurse;
	String	m_strExt;
	String	m_strUnpacker;
	//}}AFX_DATA

// other public data
	/// unpacker info
	std::array<DWORD, 3> m_dwFlags;
	PackingInfo m_infoHandler;

// Attributes
public:
	COpenDoc* GetDocument() const;

// Operations
public:
	void UpdateButtonStates();
	void UpdateResources();

// Implementation data
private:
	String m_strBrowsePath[3]; /**< Left/middle/right path from browse dialog. */
	CWinThread *m_pUpdateButtonStatusThread;
	ATL::CImage m_image; /**< Image loader/viewer for logo image */
	CSize m_sizeOrig;
	prdlg::CMoveConstraint m_constraint;
	CFont m_fontSwapButton;
	HICON const m_hIconRotate;
	HCURSOR const m_hCursorNo;
	std::array<bool, 3> m_bAutoCompleteReady;
	DropHandler *m_pDropHandler;
	int m_retryCount;
// Overrides
	public:
virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnInitialUpdate(); // called first time after construct
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);

// Implementation
public:
	virtual ~COpenView();

protected:
	void SetStatus(UINT msgID);
	void SetUnpackerStatus(UINT msgID);
	bool LoadProjectFile(const String &path);
	void TerminateThreadIfRunning();
	void TrimPaths();
	void LoadComboboxStates();
	void SaveComboboxStates();
	String AskProjectFileName(bool bOpen);

// Generated message map functions
protected:
	//{{AFX_MSG(COpenView)
	afx_msg void OnPathButton(UINT nID);
	afx_msg void OnOK();
	afx_msg void OnCancel();
	afx_msg void OnLoadProject();
	afx_msg void OnSaveProject();
	afx_msg void OnDropDownSaveProject(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSelchangePathCombo(UINT nID);
	afx_msg void OnSetfocusPathCombo(UINT id, NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDragBeginPathCombo(UINT id, NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSwapButton(int id1, int id2);
	template<int id1, int id2>
	afx_msg void OnSwapButton();
	afx_msg void OnEditEvent(UINT nID);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnSelectUnpacker();
	afx_msg void OnSelectFilter();
	afx_msg void OnOptions();
	afx_msg void OnDropDownOptions(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
	afx_msg void OnEditAction(int msg, WPARAM wParam, LPARAM LPARAM);
	template <int MSG, int WPARAM = 0, int LPARAM = 0>
	afx_msg void OnEditAction();
	afx_msg void OnHelp();
	afx_msg void OnDropFiles(const std::vector<String>& files);
	afx_msg LRESULT OnUpdateStatus(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in OpenView.cpp
inline COpenDoc* COpenView::GetDocument() const
   { return reinterpret_cast<COpenDoc*>(m_pDocument); }
#endif

