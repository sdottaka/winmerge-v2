/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  Merge.h
 *
 * @brief main header file for the MERGE application
 *
 */
#pragma once

#define WMU_CHILDFRAMEADDED						(WM_APP + 10)
#define WMU_CHILDFRAMEREMOVED					(WM_APP + 11)
#define WMU_CHILDFRAMEACTIVATE					(WM_APP + 12)
#define WMU_CHILDFRAMEACTIVATED					(WM_APP + 13)
#define IDT_UPDATEMAINMENU 1

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include <memory>
#include "MergeCmdLineInfo.h"
#include "resource.h"       // main symbols

struct FileFilter;
class FileFilterHelper;
class CAssureScriptsForThread;
class CMainFrame;
class CLanguageSelect;
class MergeCmdLineInfo;
class ProjectFile;
class COptionsMgr;
class LineFiltersList;
class SyntaxColors;
class CCrystalTextMarkers;
class FilterCommentsManager;

/////////////////////////////////////////////////////////////////////////////
// CMergeApp:
// See Merge.cpp for the implementation of this class
//

enum { IDLE_TIMER = 9754 };

/** 
 * @brief WinMerge application class
 */
class CMergeApp : public CWinApp
{
public:
	bool m_bNeedIdleTimer;
	CMultiDocTemplate* m_pOpenTemplate;
	CMultiDocTemplate* m_pDiffTemplate;
	CMultiDocTemplate* m_pHexMergeTemplate;
	CMultiDocTemplate* m_pDirTemplate;
	std::unique_ptr<CLanguageSelect> m_pLangDlg;
	std::unique_ptr<FileFilterHelper> m_pGlobalFileFilter;
	std::unique_ptr<SyntaxColors> m_pSyntaxColors; /**< Syntax color container */
	std::unique_ptr<CCrystalTextMarkers> m_pMarkers; /**< Marker container */
	String m_strSaveAsPath; /**< "3rd path" where output saved if given */
	bool m_bEscShutdown; /**< If commandline switch -e given ESC closes appliction */
	SyntaxColors * GetMainSyntaxColors() { return m_pSyntaxColors.get(); }
	CCrystalTextMarkers * GetMainMarkers() const { return m_pMarkers.get(); }
	MergeCmdLineInfo::ExitNoDiff m_bExitIfNoDiff; /**< Exit if files are identical? */
	std::unique_ptr<LineFiltersList> m_pLineFilters; /**< List of linefilters */
	std::unique_ptr<FilterCommentsManager> m_pFilterCommentsManager;

	WORD GetLangId() const;
	void SetIndicators(CStatusBar &, const UINT *, int) const;
	void TranslateMenu(HMENU) const;
	void TranslateDialog(HWND) const;
	void ChangeDialogFont(HWND hwnd, int dpi) const;
	String LoadString(UINT) const;
	bool TranslateString(const std::string&, String&) const; 
	std::wstring LoadDialogCaption(LPCTSTR) const;

	CMergeApp();
	~CMergeApp();

public:
	void AddToRecentProjectsMRU(LPCTSTR sPathName);
	void SetNeedIdleTimer();
	void SetLastCompareResult(int nResult) { m_nLastCompareResult = nResult; }

	COptionsMgr * GetMergeOptionsMgr() { return static_cast<COptionsMgr *> (m_pOptions.get()); }
	FileFilterHelper * GetGlobalFileFilter() { return m_pGlobalFileFilter.get(); }
	void ShowHelp(LPCTSTR helpLocation = nullptr);
	void OpenFileToExternalEditor(const String& file, int nLineNumber = 1);
	void OpenFileOrUrl(LPCTSTR szFile, LPCTSTR szUrl);
	bool CreateBackup(bool bFolder, const String& pszPath);
	int HandleReadonlySave(String& strSavePath, bool bMultiFile, bool &bApplyToAll);
	bool GetMergingMode() const;
	void SetMergingMode(bool bMergingMode);
	void SetupTempPath();
	bool IsReallyIdle() const;

	virtual UINT GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault) override;
	virtual BOOL WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue) override;
	virtual CString GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault = NULL) override;
	virtual BOOL WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue) override;

// Implementation
protected:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMergeApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual int DoMessageBox(LPCTSTR lpszPrompt, UINT nType, UINT nIDPrompt);
	virtual BOOL OnIdle(LONG lCount);
	//}}AFX_VIRTUAL

	void InitializeFileFilters();
	bool ParseArgsAndDoOpen(MergeCmdLineInfo& cmdInfo, CMainFrame* pMainFrame);
	void UpdateDefaultCodepage(int cpDefaultMode, int cpCustomCodepage);
	void UpdateCodepageModule();
	void ApplyCommandLineConfigOptions(MergeCmdLineInfo & cmdInfo);

	// End MergeArgs.cpp

	bool LoadProjectFile(const String& sProject, ProjectFile &project);
	bool SaveProjectFile(const String& sProject, const ProjectFile &project);
	bool LoadAndOpenProjectFile(const String& sFilepath, const String& sReportFile = _T(""));
	bool IsProjectFile(const String& filepath) const;

	//@{
	/**
	 * @name Active operations counter.
	 * These functions implement counter for active operations. We need to
	 * track active operations during whose user cannot exit the application.
	 * E.g. copying files in folder compare is such an operation.
	 */
	/**
	 * Increment the active operation counter.
	 */
	void AddOperation() { InterlockedIncrement(&m_nActiveOperations); }
	/**
	 * Decrement the active operation counter.
	 */
	void RemoveOperation()
	{
		ASSERT(m_nActiveOperations > 0);
		InterlockedDecrement( &m_nActiveOperations);
	}
	/**
	 * Get the active operations count.
	 */
	LONG GetActiveOperations() const { return m_nActiveOperations; }
	//@}

	CFont *GetGUIFont(int dpi) const;

	//{{AFX_MSG(CMergeApp)
	afx_msg BOOL OnOpenRecentFile(UINT nID);
	afx_msg void OnAppAbout();
	afx_msg void OnHelp();
	afx_msg void OnMergingMode();
	afx_msg void OnUpdateMergingMode(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMergingStatus(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	std::unique_ptr<COptionsMgr> m_pOptions;
	CAssureScriptsForThread * m_mainThreadScripts;
	int m_nLastCompareResult;
	bool m_bNonInteractive;
	LONG m_nActiveOperations; /**< Active operations count. */
	bool m_bMergingMode; /**< Merging or Edit mode */
	mutable std::map<int, CFont> m_mapFontGUI;
	ATL::CImage m_imageForInitializingGdiplus;
};

extern CMergeApp theApp;

/** 
 * @brief Set flag so that application will broadcast notification at next
 * idle time (via WM_TIMER id=IDLE_TIMER)
 */
inline void CMergeApp::SetNeedIdleTimer()
{
	m_bNeedIdleTimer = true; 
}

