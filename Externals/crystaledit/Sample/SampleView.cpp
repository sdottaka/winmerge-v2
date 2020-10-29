// SampleView.cpp : implementation of the CSampleView class
//

#include "stdafx.h"
#include "Sample.h"

#include "SampleDoc.h"
#include "SampleView.h"
#include "editcmd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSampleView

IMPLEMENT_DYNCREATE(CSampleView, CCrystalEditView)

BEGIN_MESSAGE_MAP(CSampleView, CCrystalEditView)
	//{{AFX_MSG_MAP(CSampleView)
	ON_WM_CONTEXTMENU()
	//}}AFX_MSG_MAP
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CCrystalEditView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CCrystalEditView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CCrystalEditView::OnFilePrintPreview)
	ON_COMMAND(ID_VIEW_SELMARGIN, OnSelMargin)
	ON_COMMAND(ID_VIEW_TOPMARGIN, OnTopMargin)
	ON_COMMAND(ID_VIEW_WORDWRAP, OnWordWrap)
	ON_COMMAND(ID_VIEW_WHITESPACE, OnViewWhitespace)
	ON_UPDATE_COMMAND_UI(ID_VIEW_WHITESPACE, OnUpdateViewWhitespace)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSampleView construction/destruction

CSampleView::CSampleView()
{
	// TODO: add construction code here
	SetParser(&m_xParser);
}

CSampleView::~CSampleView()
{
}

BOOL CSampleView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CCrystalEditView::PreCreateWindow(cs);
}


/////////////////////////////////////////////////////////////////////////////
// CSampleView diagnostics

#ifdef _DEBUG
void CSampleView::AssertValid() const
{
	CCrystalEditView::AssertValid();
}

void CSampleView::Dump(CDumpContext& dc) const
{
	CCrystalEditViewEx::Dump(dc);
}

CSampleDoc* CSampleView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSampleDoc)));
	return static_cast<CSampleDoc*>(m_pDocument);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CSampleView message handlers

CCrystalTextBuffer *CSampleView::LocateTextBuffer()
{
	return &GetDocument()->m_xTextBuffer;
}

void CSampleView::OnInitialUpdate() 
{
	CCrystalEditViewEx::OnInitialUpdate();

	LOGFONT lf{ GetDocument()->m_lf };
	lf.lfHeight = MulDiv(lf.lfHeight, m_dpi, 96);
	SetFont(lf);
	SetColorContext(GetDocument()->m_pSyntaxColors);
	SetMarkersContext(GetDocument()->m_pMarkers);
	if (GetDocument()->m_xTextBuffer.GetTableEditing())
	{
		SetTopMargin(true);
		AutoFitColumn();
	}
}

void CSampleView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	AfxMessageBox(_T("Build your own context menu!"));
}

void CSampleView::OnSelMargin()
{
	GetDocument()->ForEachView([](CSampleView* pView) {
		pView->SetSelectionMargin(!pView->GetSelectionMargin());
	});
}

void CSampleView::OnTopMargin()
{
	GetDocument()->ForEachView([](CSampleView* pView) {
		pView->SetTopMargin(!pView->GetTopMargin());
	});
}

void CSampleView::OnWordWrap()
{
	GetDocument()->ForEachView([](CSampleView* pView) {
		pView->SetWordWrapping(!pView->GetWordWrapping());
	});
}

void CSampleView::OnViewWhitespace()
{
	GetDocument()->ForEachView([](CSampleView* pView) {
		bool bViewTabs = pView->GetViewTabs();
		pView->SetViewTabs(!bViewTabs);
		pView->SetViewEols(!bViewTabs, true);
	});
}

void CSampleView::OnUpdateViewWhitespace(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetViewTabs());
}

