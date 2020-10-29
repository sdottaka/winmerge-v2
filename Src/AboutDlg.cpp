// SPDX-License-Identifier: GPL-2.0-or-later
/** 
 * @file  AboutDlg.cpp
 *
 * @brief Implementation of the About-dialog.
 *
 */

#include "stdafx.h"
#include "AboutDlg.h"
#include "TrDialogs.h"
#include "Bitmap.h"
#include "resource.h" // IDD_ABOUTBOX

 // https://www.gnu.org/graphics/gnu-ascii.html
 // Copyright (c) 2001 Free Software Foundation, Inc.
 // Converted with https://tomeko.net/online_tools/cpp_text_escape.php
static const char gnu_ascii[] =
"  ,           ,\n"
" /             \\\n"
"((__-^^-,-^^-__))\n"
" `-_---' `---_-'\n"
"  `--|o` 'o|--'\n"
"     \\  `  /\n"
"      ): :(\n"
"      :o_o:\n"
"       \"-\"";

/** 
 * @brief About-dialog class.
 * 
 * Shows About-dialog bitmap and draws version number and other
 * texts into it.
 */
class CAboutDlg::Impl : public CTrDialog
{
public:
	explicit Impl(CAboutDlg *p, CWnd* pParent = nullptr);

// Dialog Data
	//{{AFX_DATA(CAboutDlg::Impl)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg::Impl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg::Impl)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOpenContributors();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnBnClickedWWW(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);

private:
	void RecreateResources();

	CAboutDlg *const m_p;
	ATL::CImage m_image;
	CFont m_font;
	CFont m_font_gnu_ascii;
};

BEGIN_MESSAGE_MAP(CAboutDlg::Impl, CTrDialog)
	//{{AFX_MSG_MAP(CAboutDlg::Impl)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_OPEN_CONTRIBUTORS, OnBnClickedOpenContributors)
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_NOTIFY(NM_CLICK, IDC_WWW, OnBnClickedWWW)
	ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

CAboutDlg::Impl::Impl(CAboutDlg *p, CWnd* pParent /*= nullptr*/)
	: CTrDialog(CAboutDlg::Impl::IDD)
	, m_p(p)
{
}

void CAboutDlg::Impl::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg::Impl)
	DDX_Text(pDX, IDC_COMPANY, m_p->m_info.copyright);
	DDX_Text(pDX, IDC_VERSION, m_p->m_info.version);
	//}}AFX_DATA_MAP
}

/** 
 * @brief Read version info from resource to dialog.
 */
BOOL CAboutDlg::Impl::OnInitDialog() 
{
	CTrDialog::OnInitDialog();

	if (!LoadImageFromResource(m_image, MAKEINTRESOURCE(IDR_SPLASH), _T("IMAGE")))
	{
		// FIXME: LoadImageFromResource() seems to fail when running on Wine 5.0.
	}

	RecreateResources();

	::SetDlgItemTextA(m_hWnd, IDC_GNU_ASCII, gnu_ascii);

	String link;
	GetDlgItemText(IDC_WWW, link);
	link = _T("<a href=\"") + m_p->m_info.website + _T("\">") + link + _T("</a>");
	SetDlgItemText(IDC_WWW, link);	

	UpdateData(FALSE);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

HBRUSH CAboutDlg::Impl::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (nCtlColor == CTLCOLOR_STATIC)
	{
		if (pWnd->GetDlgCtrlID() == IDC_GNU_ASCII)
			pDC->SetTextColor(RGB(128, 128, 128));
		pDC->SetBkMode(TRANSPARENT);
		return (HBRUSH)GetStockObject(NULL_BRUSH);
	}
	return CTrDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CAboutDlg::Impl::OnEraseBkgnd(CDC* pDC)
{
	if (!m_image)
		return FALSE;
	CRect rc, rcCompany;
	GetClientRect(&rc);
	GetDlgItem(IDC_COMPANY)->GetWindowRect(&rcCompany);
	ScreenToClient(&rcCompany);
	rc.top = rcCompany.bottom;
	pDC->FillSolidRect(&rc, GetSysColor(COLOR_BTNFACE));
	rc.bottom = rc.top;
	rc.top = 0;
	m_image.Draw(pDC->m_hDC, rc, Gdiplus::InterpolationModeBicubic);
	return TRUE;
}

/**
 * @brief Show contributors list.
 * Opens Contributors.txt into notepad.
 */
void CAboutDlg::Impl::OnBnClickedOpenContributors()
{
	int tmp = 0;
	m_p->m_onclick_contributers.notify(m_p, tmp);
}

void CAboutDlg::Impl::OnBnClickedWWW(NMHDR *pNMHDR, LRESULT *pResult)
{
	PNMLINK pNMLink = (PNMLINK)pNMHDR;
	ShellExecute(nullptr, _T("open"), pNMLink->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
}

LRESULT CAboutDlg::Impl::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
	CTrDialog::OnDpiChanged(wParam, lParam);
	RecreateResources();
	Invalidate();
	return 0;
}

void CAboutDlg::Impl::RecreateResources()
{
	if (!GetDlgItem(IDC_VERSION))
		return;
	
	const int dpi = GetDpi();

	LOGFONT lfv{};
	DpiAware::GetPointLogFont(lfv, 10, _T("Tahoma"), dpi);
	lfv.lfWeight = FW_NORMAL;
	m_font.DeleteObject();
	m_font.CreateFontIndirect(&lfv);

	LOGFONT lf{};
	DpiAware::GetPointLogFont(lf, 14, _T("Courier New"), dpi);
	lf.lfWeight = FW_BOLD;
	m_font_gnu_ascii.DeleteObject();
	m_font_gnu_ascii.CreateFontIndirect(&lf);

	GetDlgItem(IDC_VERSION)->SetFont(&m_font);
	GetDlgItem(IDC_GNU_ASCII)->SetFont(&m_font_gnu_ascii);
}

CAboutDlg::CAboutDlg() : m_pimpl(new CAboutDlg::Impl(this)) {}
CAboutDlg::~CAboutDlg() {}
int CAboutDlg::DoModal() { return static_cast<int>(m_pimpl->DoModal()); }
