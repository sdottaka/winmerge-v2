/*!
  \file    CMoveConstraint.h
  \author  Perry Rapp, Creator, 1998-2004
  \date    Created: 1998
  \date    Edited:  2006-09-23 (Kimmo Varis)

  \brief   Declaration of CMoveConstraint

*/
/* The MIT License
Copyright (c) 2001 Perry Rapp
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#pragma once

#include <afxtempl.h>

class CFormView;

namespace prdlg {

/*!
  \class CMoveConstraint
  \brief Code to resize controls on a dialog (formview, propertypage)

  HOW TO USE

  (a) add a member variable

	prdlg::CMoveConstraint m_constraint; 

  (b) add (1) Initialize & (2,3,4...) Constrain calls
      (to OnInitDialog for dialogs)
      (to InitialUpdate for views)
      (to OnCreate or OnCreateClient for frames)

  (c) optionally call any Allow or Disallow functions to adjust sizing potential (as in b)

  (d) call SetIsPropertyPage or Sheet if appropriate
       call SetScrollScale (for formview)

  Choose to (e1) Subclass or (e2) call WindowProc

  (e1) call Subclass - can call after Initialize calls
       unless original size is needed, eg FormViews,
		 in which case call Initialize from OnNcCreate

  (e2) override dialog's WindowProc and forward to constraint
    add code to WindowProc (copy following block) before the call to the parent WindowProc
	   use ClassWizard, or add to AFX_VIRTUAL block:
	   virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

	LRESULT lresult;
	if (m_constraint.WindowProc(m_hWnd, message, wParam, lParam, &lresult))
		return lresult;


   For frames & views, the GetMinMaxInfo is sent to the frame
   so the frame must have a constraint to control min & max

  EXAMPLE USE (in InitDialog)

  m_constraint.InitializeCurrentSize(this);
  m_constraint.ConstrainItem(IDC_EDIT, 0, 1, 0, 1); // fills up all x & y delta
  m_constraint.ConstrainItem(IDC_LABEL, 0, 0, 1, 0); // no expansion, but below 100% of height growth
  m_constraint.SubclassWnd(); // install subclassing

 */

class CMoveConstraint
{
public:
	enum EGRIP { SG_NONE, SG_NORMAL, SG_PARENTSTATE };
private:
	static EGRIP c_defGrip; // class-wide default sizing grip setting

public:
	static void SetDefGrip(EGRIP nDefGrip) { c_defGrip = nDefGrip; }

public:

	// add a MoveConstraint member to dialog or view
	CMoveConstraint();
	~CMoveConstraint();

	bool IsInitalized() { return m_hwndDlg!=nullptr; }

	// THIS IS THE USUAL ONE
	// call from InitDialog (dialogs) or InitialUpdate (views) or OnCreate (frames)
	// if you want the current size of the dialog
	// to be used for scaling the controls
	// fails if IsWindow fails
	bool InitializeCurrentSize(HWND hwndParent);
	bool InitializeCurrentSize(CWnd * pParent);

	// if you want to set, a priori, the size of the dialog to be assumed
	// the controls are scaled for in the template
	// for use when parent is maximized, and we need to know the design size (in pixels)
	// this size is what the initial controls are sized to fit
	void InitializeSpecificSize(HWND hwndParent, int nWidth, int nHeight);
	void InitializeSpecificSize(CWnd * pParent, int nWidth, int nHeight);

	// for FORMVIEWS, who lose their dialog size early
	// fails if no WM_SIZE came thru
	bool InitializeOriginalSize(CWnd * pParent);
	bool InitializeOriginalSize(HWND hwndParent);

	// force an update of controls (laying them out to current size)
	void UpdateSizes();

	// default is no height shrink, no width shrink
	// but allowed height growth & allowed width growth
	void AllowHeightShrink();
	void AllowWidthShrink();
	void DisallowHeightGrowth();
	void DisallowWidthGrowth();
	// if you know exactly how big you want to allow it (in pixels)
	// use -1 to not set an argument
	void SetMaxSizePixels(int nWidth, int nHeight);
	void SetMinSizePixels(int nWidth, int nHeight);
	void SetMinSizePercent(double fWidth, double fHeight);

	// for FORMVIEW, how much to reduce scrollbars when form shrinks
	void SetScrollScale(CFormView * pFormView, double fShrinkWidth, double fShrinkHeight);

	// call for each constrained child window (three equivalent versions for convenience)
	// fails if IsWindow fails (except with CWnd it is just deferred)
	bool Constrain(HWND hwndChild, double fLeftX, double fExpandX, double fAboveY, double fExpandY);
	void Constrain(CWnd * pWnd, double fLeftX, double fExpandX, double fAboveY, double fExpandY);
	bool ConstrainItem(int nId, double fLeftX, double fExpandX, double fAboveY, double fExpandY);

	void SetIsPropertyPage() { m_bPropertyPage = true; SetSizeGrip(SG_NONE); }
	void SetIsPropertySheet() { m_bPropertySheet = true; }
	void SetIsView() { SetSizeGrip(SG_PARENTSTATE); }

	// If you plan to constrain windows that are not your children
	// (eg, grandchildren), call this
	void ConstrainNonChildren() { m_bConstrainNonChildren = true; }

	// embedded windows, such as property pages or formviews, should set the size grip to none
	void SetSizeGrip(EGRIP nGrip) { m_nGrip = nGrip; }
	// either Subclass or call a WindowProc
	bool SubclassWnd();
	bool UnSubclassWnd();

	// call after all constraints established
	// always loads size, may also set position
	void LoadPosition(LPCTSTR szKeyName, LPCTSTR szValueName, bool position);
	void LoadPosition(LPCTSTR szValueName, bool position);
	void Persist(bool saving, bool position);

	// for use when children hadn't been created yet at initialization time
	// so their constraints had to be buffered to be initialized later
	// (this is used by property pages & property sheets automatically, at PSN_SETACTIVE)
	void CheckDeferredChildren();
	int GetDeferredCount() { return m_nDelayed; }

	// see usage section above
	bool WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT * plresult);

// ToolTips
	void SetTip(int id, LPCTSTR szTip);
	void SetTip(int id, int nResourceId);

// Implementation methods
protected:
	struct Constraint;
	void GrabCurrentDimensionsAsOriginal(HWND hwndParent);
	bool DoConstrain(CWnd * pWnd, HWND hwndChild, double fLeftX, double fExpandX, double fAboveY, double fExpandY);
	void InitializeChildConstraintData(HWND hwndParent, Constraint & constraint);
	// handle WM_SIZE
	void Resize(HWND hWnd, UINT nType);
	// handle WM_GETMINMAXINFO
	void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI );
	// handle WM_NCHITTEST
	bool OnNcHitTest(UINT message, WPARAM wParam, LPARAM lParam, LRESULT * plresult);
	// handle WM_NOTIFY/TTN_NEEDTEXT combination
	bool OnTtnNeedText(TOOLTIPTEXT * pTTT, LRESULT * plresult);
	bool PaintGrip();
	void ClearMostData();
	// handle WM_DESTROY
	void OnDestroy();


	static LRESULT CALLBACK ConstraintWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
	

// Implementation types
protected:
	struct tip { CString m_sText; int m_nResourceId; tip() : m_nResourceId(0) { } };

	struct Constraint
	{
		double m_fLeftX;
		double m_fExpandX;
		double m_fAboveY;
		double m_fExpandY;
		HWND m_hwndChild;
		CWnd * m_pWnd;
		CRect m_rectChildOriginal;
		HWND m_hwndParent; // normally is the dialog
		Constraint();
		Constraint(double fLeftX, double fExpandX, double fAboveY, double fExpandY, HWND hwndChild, CWnd * pWnd);
		void Init();
	};

	typedef CList<Constraint, Constraint &> ConstraintList;


// Implementation data
private:
	HWND m_hwndDlg; // parent of controls - could be FormView or PropertyPage or whatever as well
	CRect m_rectDlgOriginal;
	String m_fontFace;
	double m_fontSize;
	int m_nOrigX;
	int m_nOrigY;
	EGRIP m_nGrip;
	bool m_bOriginalFetched;
	int m_nMinX;
	int m_nMinY;
	int m_nMaxX;
	int m_nMaxY;
	int m_nDelayed; // CWnds without HWND
	int m_dpi;
	bool m_bSubclassed;
	WNDPROC m_oldWndProc;
// formview stuff
	CFormView * m_pFormView;
	// formview original scrollbars
	int m_nOrigScrollX;
	int m_nOrigScrollY;
	// formview - how much scrollbars shrink
	double m_fShrinkWidth;
	double m_fShrinkHeight;
	// special handling for PSN_SETACTIVE
	bool m_bPropertyPage;
	bool m_bPropertySheet;
// constraints
	ConstraintList m_ConstraintList;
	bool m_bConstrainNonChildren;
	bool m_bPersistent; // whether to save position to registry
	CString m_sRegistrySubkey; // defaults to _T("LastWindowPos")
	CString m_sRegistryValueName; // should be name of window
// tooltips
	CMap<UINT_PTR, UINT_PTR, tip, tip&> m_tips;
};

} // namespace
