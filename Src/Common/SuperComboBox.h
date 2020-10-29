#pragma once

// SuperComboBox.h : header file
//

#include <vector>
#include "UnicodeString.h"
#include "utils/DpiAware.h"

class DropHandler;

/////////////////////////////////////////////////////////////////////////////
// CSuperComboBox window

class CSuperComboBox : public DpiAware::CDpiAwareWnd<CComboBoxEx>
{
// Construction
public:
	CSuperComboBox();
	virtual ~CSuperComboBox();

// Attributes
protected:
	bool m_bInEditchange;
	bool m_bAutoComplete;
	bool m_bDoComplete;
	bool m_bHasImageList;

	int m_nMaxItems;
	bool m_bComboBoxEx;
	bool m_bExtendedFileNames;
	bool m_bCanBeEmpty;

	bool m_bMustUninitOLE;
	static std::map<int, HIMAGELIST> m_himlSystemMap;

	DropHandler *m_pDropHandler;

	std::vector<CString> m_sFullStateText;

public:

	enum
	{
		AUTO_COMPLETE_DISABLED		= 0,
		AUTO_COMPLETE_FILE_SYSTEM,
		AUTO_COMPLETE_RECENTLY_USED
	};

// Operations
	void SetAutoComplete(INT nSource);

public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSuperComboBox)
	public:
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    //}}AFX_VIRTUAL

// Implementation
public:
	void SetFileControlStates(bool bCanBeEmpty = false, int nMaxItems = -1);
	void SaveState(LPCTSTR szRegSubKey);
	void LoadState(LPCTSTR szRegSubKey);
	bool AttachSystemImageList();
	int AddString(LPCTSTR lpszItem);
	int InsertString(int nIndex, LPCTSTR lpszItem);
	int DeleteString(int nIndex);
	int FindString(int nStartAfter, LPCTSTR lpszString) const;
	int GetLBTextLen(int nIndex) const;
	void GetLBText(int nIndex, CString &rString) const;

	// Generated message map functions
protected:
	void ResetContent();

	virtual void PreSubclassWindow();

	//{{AFX_MSG(CSuperComboBox)
	afx_msg BOOL OnEditchange();
	afx_msg BOOL OnSetfocus();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDropFiles(const std::vector<String>& files);
	afx_msg void OnGetDispInfo(NMHDR *pNotifyStruct, LRESULT *pResult);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};
