///////////////////////////////////////////////////////////////////////////
//  File:    memcombo.cpp
//  Version: 1.1.0.4
//  Updated: 19-Jul-1998
//
//  Copyright:  Ferdinand Prantl
//  E-mail:     prantl@ff.cuni.cz
//
//  Combo-box saving last typed expressions
//
//  You are free to use or modify this code to the following restrictions:
//  - Acknowledge me somewhere in your about box, simple "Parts of code by.."
//  will be enough. If you can't (or don't want to), contact me personally.
//  - LEAVE THIS HEADER INTACT
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "editreg.h"
#include "memcombo.h"
#include "../utils/registry.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define REMEMBER_COUNT  64

/////////////////////////////////////////////////////////////////////////////
// CMemComboBox

BEGIN_MESSAGE_MAP (CMemComboBox, CComboBox)
//{{AFX_MSG_MAP(CMemComboBox)
ON_CONTROL_REFLECT (CBN_SETFOCUS, OnSetfocus)
//}}AFX_MSG_MAP
END_MESSAGE_MAP ()

////////////////////////////////////////////////////////////////////////////////
// Methods

CMemComboBox::CMemComboBox () : m_bFirstFocus (true)
{
}

CMemComboBox::~CMemComboBox ()
{
}

/////////////////////////////////////////////////////////////////////////////
// CMemComboBox message handlers

CMap < CString, LPCTSTR, CString, LPCTSTR > CMemComboBox::groups;

void SetComboBoxHeight(CComboBox &Control)
{
  int      nHeight = Control.GetCount(), nMax = ::GetSystemMetrics(SM_CYSCREEN) - 48;
  CRect    rc;

  Control.GetClientRect(rc);
  Control.ClientToScreen(rc);
  nHeight = rc.Height() * nHeight + 16;
  if(rc.top + nHeight > nMax)
    nHeight = nMax - rc.top;
  Control.SetWindowPos(nullptr, 0, 0, rc.Width(), nHeight, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
}

void SetComboBoxWidth(CComboBox &Control, LPCTSTR lpszText = nullptr)
{
  int          cnt = Control.GetCount();

  if(!cnt)
    return;

  HMONITOR monitor = MonitorFromWindow(Control.m_hWnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO info{ sizeof MONITORINFO };
  GetMonitorInfo(monitor, &info);

  CClientDC      dc(&Control);
  CFont        *oldFont;
  int          width = 0, nMax = info.rcMonitor.right - 48;
  CRect        rc;
  CSize        size;

  CFont *pFont = Control.GetFont();
  oldFont = dc.SelectObject(pFont);
  if(lpszText != nullptr && *lpszText != 0) {
    size = dc.GetTextExtent(lpszText);
    width = size.cx;
  } else {
    CString        item;

    for(int i = 0; i < cnt; i++) {
      Control.GetLBText(i, item);
      size = dc.GetTextExtent(item);
      if(size.cx > width)
        width = size.cx;
    }
  }
  width += GetSystemMetrics(SM_CXVSCROLL) + 2 * GetSystemMetrics(SM_CXEDGE);
  Control.GetClientRect(rc);
  Control.ClientToScreen(rc);
  if(rc.left + width > nMax)
    width = nMax - rc.left;
  Control.SetDroppedWidth(width);
  dc.SelectObject(oldFont);
}

void CMemComboBox::FillCurrent ()
{
  CString strText;
  GetWindowText (strText);
  Fill (strText);
}

void CMemComboBox::
Fill (LPCTSTR text)
{
  if (text && *text)
    {
      int ol = GetCount ();
      int nPos = FindStringExact (-1, text);
      if (nPos  != CB_ERR)
        DeleteString (nPos);
      InsertString (0, text);
      int l = GetCount ();
      if (l > REMEMBER_COUNT)
        DeleteString (--l);
      if (ol != l)
        SetComboBoxHeight (*this);
      SetComboBoxWidth (*this);
      SetCurSel (0);
      if (!m_sGroup.IsEmpty ())
        {
          CString item, items;
          for (int i = 0; i < l; i++)
            {
              GetLBText (i, item);
              items += item + _T ('\n');
            }
          groups.SetAt (m_sGroup, items);
        }
    } 
}

void CMemComboBox::
LoadSettings ()
{
  CReg reg;
  if (reg.Open (HKEY_CURRENT_USER, REG_EDITPAD, KEY_READ))
    {
      static LPCTSTR name[] = { _T("FindText"), _T("ReplaceText") };
      CString value;

      for (int i = 0; i < sizeof (name) / sizeof (name[0]); i++)
        {
          if (reg.LoadString (name[i], value))
            {
              groups.SetAt (name[i], value);
            }
        }
    }
}

void CMemComboBox::
SaveSettings ()
{
  CReg reg;
  if (reg.Create (HKEY_CURRENT_USER, REG_EDITPAD, KEY_WRITE))
    {
      POSITION pos = groups.GetStartPosition ();
      CString name, value;

      while (pos)
        {
          groups.GetNextAssoc (pos, name, value);
          VERIFY (reg.SaveString (name, value));
        }
    }
}

void CMemComboBox::
OnSetfocus ()
{
  if (m_bFirstFocus && !m_sGroup.IsEmpty ())
  {
    m_bFirstFocus = false;
    // create the dropdown list
    CString items;
    if (groups.Lookup (m_sGroup, items))
    {
      int p;
      while ((p = items.Find (_T ('\n'))) != -1)
      {
        AddString (items.Left (p));
        items = items.Mid (p + 1);
      }
      SetComboBoxHeight (*this);
      SetComboBoxWidth (*this);
    }
    // we don't modify the windowText value as it may be initialized 
    // before the dialog is shown
  }
}


