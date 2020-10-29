#include <StdAfx.h>
#include "Merge.h"
#include "TrDialogs.h"

IMPLEMENT_DYNAMIC(CTrDialog, CDialog)
IMPLEMENT_DYNAMIC(CTrPropertyPage, CPropertyPage)
IMPLEMENT_DYNAMIC(CTrDialogBar, CDialogBar)

BEGIN_MESSAGE_MAP(CTrDialog, DpiAware::CDpiAwareDialog<CDialog>)
	ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CTrPropertyPage, DpiAware::CDpiAwareDialog<CPropertyPage>)
	ON_MESSAGE(WM_DPICHANGED_BEFOREPARENT, OnDpiChangedBeforeParent)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CTrDialogBar, DpiAware::CDpiAwareDialog<CDialogBar>)
	ON_MESSAGE(WM_DPICHANGED_BEFOREPARENT, OnDpiChangedBeforeParent)
END_MESSAGE_MAP()

void DpiChangedImplHelper(HWND hwnd, int olddpi, int newdpi)
{
	theApp.ChangeDialogFont(hwnd, newdpi);
	DpiAware::Dialog_UpdateControlInnerWidths(hwnd, olddpi, newdpi);
}

void StaticDlgUtils::WildcardRemoveDuplicatePatterns(String& patterns)
{
	size_t i = 0, j = 0, k = 0;
	while ((j = patterns.find_first_of(L"; ", i)) != String::npos &&
		(k = patterns.find_last_of(L"; ", j) + 1) != patterns.length())
	{
		TCHAR const sep = patterns[j];
		patterns[j] = L'\0';
		if (PathMatchSpec(patterns.c_str() + i, patterns.c_str() + k))
		{
			patterns.erase(i, k - i);
		}
		else
		{
			patterns[j] = sep;
			i = k;
		}
	}
}

BOOL CTrDialog::OnInitDialog()
{
	theApp.TranslateDialog(m_hWnd);
	__super::OnInitDialog();
	return TRUE;
}

BOOL CTrPropertyPage::OnInitDialog()
{
	theApp.TranslateDialog(m_hWnd);
	__super::OnInitDialog();
	return TRUE;
}

BOOL CTrDialogBar::Create(CWnd* pParentWnd, LPCTSTR lpszTemplateName,
	UINT nStyle, UINT nID)
{
	BOOL bSucceeded = __super::Create(pParentWnd, lpszTemplateName, nStyle, nID);
	if (bSucceeded)
		theApp.TranslateDialog(m_hWnd);
	return bSucceeded;
}

BOOL CTrDialogBar::Create(CWnd* pParentWnd, UINT nIDTemplate,
	UINT nStyle, UINT nID)
{
	BOOL bSucceeded = __super::Create(pParentWnd, nIDTemplate, nStyle, nID);
	if (bSucceeded)
		theApp.TranslateDialog(m_hWnd);
	return bSucceeded;
}
