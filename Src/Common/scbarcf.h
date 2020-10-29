/////////////////////////////////////////////////////////////////////////
//
// CSizingControlBarCF          Version 2.44
// 
// Created: Dec 21, 1998        Last Modified: March 31, 2002
//
// See the official site at www.datamekanix.com for documentation and
// the latest news.
//
/////////////////////////////////////////////////////////////////////////
// Copyright (C) 1998-2002 by Cristi Posea. All rights reserved.
//
// This code is free for personal and commercial use, providing this 
// notice remains intact in the source files and all eventual changes are
// clearly marked with comments.
//
// You must obtain the author's consent before you can include this code
// in a software library.
//
// No warrantee of any kind, express or implied, is included with this
// software; use at your own risk, responsibility for damages (if any) to
// anyone resulting from the use of this software rests entirely with the
// user.
//
// Send bug reports, bug fixes, enhancements, requests, flames, etc. to
// cristi@datamekanix.com or post them at the message board at the site.
/////////////////////////////////////////////////////////////////////////
#pragma once
// scbarcf.h : header file
//

/////////////////////////////////////////////////////////////////////////
// CSizingControlBarCF

#include "utils/DpiAware.h"

#ifndef baseCSizingControlBarCF
#define baseCSizingControlBarCF CSizingControlBarG
#endif

class CSizingControlBarCF : public baseCSizingControlBarCF
{
    DECLARE_DYNAMIC(CSizingControlBarCF)

// Construction
public:
    CSizingControlBarCF();

// Overridables
    virtual void OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler) override;

// Implementation
protected:
    // implementation helpers
    virtual void NcPaintGripper(CDC* pDC, CRect rcClient) override;

protected:
    bool    m_bActive; // a child has focus
    CString m_sFontFace;

// Generated message map functions
protected:
    //{{AFX_MSG(CSizingControlBarCF)
    //}}AFX_MSG
    afx_msg LRESULT OnSetText(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()
};

