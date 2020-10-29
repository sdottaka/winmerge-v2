////////////////////////////////////////////////////////////////////////////
//  File:       ccrystaltextview.cpp
//  Version:    1.2.0.5
//  Created:    29-Dec-1998
//
//  Author:     Stcherbatchenko Andrei
//  E-mail:     windfall@gmx.de
//
//  Implementation of the CCrystalTextView class, a part of Crystal Edit -
//  syntax coloring text editor.
//
//  You are free to use or modify this code to the following restrictions:
//  - Acknowledge me somewhere in your about box, simple "Parts of code by.."
//  will be enough. If you can't (or don't want to), contact me personally.
//  - LEAVE THIS HEADER INTACT
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//  17-Feb-99
//  FIX:    missing UpdateCaret() in CCrystalTextView::SetFont
//  FIX:    missing UpdateCaret() in CCrystalTextView::RecalcVertScrollBar
//  FIX:    mistype in CCrystalTextView::RecalcPageLayouts + instead of +=
//  FIX:    removed condition 'm_nLineHeight < 20' in
//      CCrystalTextView::CalcLineCharDim(). This caused painting defects
//      when using very small fonts.
//
//  FEATURE:    Some experiments with smooth scrolling, controlled by
//      m_bSmoothScroll member variable, by default turned off.
//      See ScrollToLine function for implementation details.
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//  21-Feb-99
//      Paul Selormey, James R. Twine
//  +   FEATURE: description for Undo/Redo actions
//  +   FEATURE: multiple MSVC-like bookmarks
//  +   FEATURE: 'Disable backspace at beginning of line' option
//  +   FEATURE: 'Disable drag-n-drop editing' option
//
//  +   FIX:  ResetView() now virtual
//  +   FEATURE: Added OnEditOperation() virtual: base for auto-indent,
//      smart indent etc.
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//  19-Jul-99
//      Ferdinand Prantl:
//  +   FEATURE: regular expressions, go to line and things ...
//  +   FEATURE: plenty of syntax highlighting definitions
//  +   FEATURE: corrected bug in syntax highlighting C comments
//  +   FEATURE: extended registry support for saving settings
//  +   FEATURE: some other things I've forgotten ...
//
//  ... it's being edited very rapidly so sorry for non-commented
//        and maybe "ugly" code ...
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//  01-Jun-99 to 31-Aug-99
//      Sven Wiegand (search for "//BEGIN SW" to find my changes):
//
//  + FEATURE: support for language switching on the fly with class 
//          CCrystalParser
//  +   FEATURE: word wrapping
//  + FIX:  Setting m_nIdealCharPos, when choosing cursor position by mouse
//  + FIX:  Backward search
//  + FEATURE: incremental search
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//  24-Oct-99
//      Sven Wiegand
//
//  + FIX: Opening large files won't crash anymore and will go very fast
//         (removed call to RecalcVertScrollBar() in WrapLineCached())
//  + FIX: Problems with repainting and cursor-position by resizing window 
//         fixed by adding call to ScrollToSubLine() in OnSize().
//  + FEATURE: Supporting [Return] to exit incremental-search-mode
//           (see OnChar())
///////////////////////////////////////////////////////////////////////////////

/**
 * @file  ccrystaltextview.cpp
 *
 * @brief Implementation of the CCrystalTextView class
 */

#include "StdAfx.h"
#include <vector>
#include <algorithm>
#include <malloc.h>
#include <imm.h> /* IME */
#include "editcmd.h"
#include "editreg.h"
#include "ccrystaltextview.h"
#include "ccrystaltextbuffer.h"
#include "ccrystaltextmarkers.h"
#include "ViewableWhitespace.h"
#include "SyntaxColors.h"
#include "renderers/ccrystalrendererdirectwrite.h"
#include "renderers/ccrystalrenderergdi.h"
#include "dialogs/cfindtextdlg.h"
#include "dialogs/ctextmarkerdlg.h"
#include "dialogs/gotodlg.h"
#include "utils/fpattern.h"
#include "utils/filesup.h"
#include "utils/registry.h"
#include "utils/string_util.h"
#include "utils/wcwidth.h"
#include "utils/icu.hpp"

using std::vector;
using CrystalLineParser::TEXTBLOCK;

// Escaped character constants in range 0x80-0xFF are interpreted in current codepage
// Using C locale gets us direct mapping to Unicode codepoints
#pragma setlocale("C")

#ifndef __AFXPRIV_H__
#pragma message("Include <afxpriv.h> in your stdafx.h to avoid this message")
#include <afxpriv.h>
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// The vcruntime.h version of _countf() gives syntax errors starting with VS 15.7.2,
// but only with `CCrystalTextView::m_SourceDefs` (which is local to this .cpp file), 
// and only for X64 compilations (Win32 is ok, probably because no alignment issues
// are involved).  I think that this could be related to C++17 compliance issues.
// This patch reverts to a 'traditional' definition of _countf(), a pre-existing 
// part of the CCrystalTextView package.
#undef _countof
#ifndef _countof
#define _countof(array) (sizeof(array)/sizeof(array[0]))
#endif

#define DEFAULT_PRINT_MARGIN        1000    //  10 millimeters

#ifndef WM_MOUSEHWHEEL
#  define WM_MOUSEHWHEEL 0x20e
#endif

/** @brief Width of revision marks. */
const UINT MARGIN_REV_WIDTH = 3;

/** @brief Color of unsaved line revision mark (dark yellow). */
const COLORREF UNSAVED_REVMARK_CLR = RGB(0xD7, 0xD7, 0x00);
/** @brief Color of saved line revision mark (green). */
const COLORREF SAVED_REVMARK_CLR = RGB(0x00, 0xFF, 0x00);

#define SMOOTH_SCROLL_FACTOR        6

#define ICON_INDEX_WRAPLINE         15

////////////////////////////////////////////////////////////////////////////
// CCrystalTextView

LOGFONT CCrystalTextView::m_LogFont;

IMPLEMENT_DYNCREATE (CCrystalTextView, CView)

HINSTANCE CCrystalTextView::s_hResourceInst = nullptr;
CCrystalTextView::RENDERING_MODE CCrystalTextView::s_nRenderingModeDefault = RENDERING_MODE_GDI;

static ptrdiff_t FindStringHelper(LPCTSTR pszLineBegin, size_t nLineLength, LPCTSTR pszFindWhere, LPCTSTR pszFindWhat, DWORD dwFlags, int &nLen, RxNode *&rxnode, RxMatchRes *rxmatch);

BEGIN_MESSAGE_MAP (CCrystalTextView, DpiAware::CDpiAwareWnd<CView>)
//{{AFX_MSG_MAP(CCrystalTextView)
ON_WM_DESTROY ()
ON_WM_ERASEBKGND ()
ON_WM_SIZE ()
ON_WM_VSCROLL ()
ON_WM_SETCURSOR ()
ON_WM_LBUTTONDOWN ()
ON_WM_SETFOCUS ()
ON_WM_HSCROLL ()
ON_WM_LBUTTONUP ()
ON_WM_MOUSEMOVE ()
ON_WM_TIMER ()
ON_WM_KILLFOCUS ()
ON_WM_LBUTTONDBLCLK ()
ON_COMMAND (ID_EDIT_COPY, OnEditCopy)
ON_UPDATE_COMMAND_UI (ID_EDIT_COPY, OnUpdateEditCopy)
ON_COMMAND (ID_EDIT_SELECT_ALL, OnEditSelectAll)
ON_WM_RBUTTONDOWN ()
ON_WM_SYSCOLORCHANGE ()
ON_WM_CREATE ()
ON_COMMAND (ID_EDIT_FIND, OnEditFind)
ON_COMMAND (ID_EDIT_REPEAT, OnEditRepeat)
ON_UPDATE_COMMAND_UI (ID_EDIT_REPEAT, OnUpdateEditRepeat)
ON_COMMAND (ID_EDIT_MARK, OnEditMark)
ON_WM_MOUSEWHEEL ()
ON_WM_MOUSEHWHEEL ()
ON_MESSAGE (WM_IME_STARTCOMPOSITION, OnImeStartComposition) /* IME */
//}}AFX_MSG_MAP
ON_COMMAND (ID_EDIT_CHAR_LEFT, OnCharLeft)
ON_COMMAND (ID_EDIT_EXT_CHAR_LEFT, OnExtCharLeft)
ON_COMMAND (ID_EDIT_CHAR_RIGHT, OnCharRight)
ON_COMMAND (ID_EDIT_EXT_CHAR_RIGHT, OnExtCharRight)
ON_COMMAND (ID_EDIT_WORD_LEFT, OnWordLeft)
ON_COMMAND (ID_EDIT_EXT_WORD_LEFT, OnExtWordLeft)
ON_COMMAND (ID_EDIT_WORD_RIGHT, OnWordRight)
ON_COMMAND (ID_EDIT_EXT_WORD_RIGHT, OnExtWordRight)
ON_COMMAND (ID_EDIT_LINE_UP, OnLineUp)
ON_COMMAND (ID_EDIT_EXT_LINE_UP, OnExtLineUp)
ON_COMMAND (ID_EDIT_LINE_DOWN, OnLineDown)
ON_COMMAND (ID_EDIT_EXT_LINE_DOWN, OnExtLineDown)
ON_COMMAND (ID_EDIT_SCROLL_UP, ScrollUp)
ON_COMMAND (ID_EDIT_SCROLL_DOWN, ScrollDown)
ON_COMMAND (ID_EDIT_PAGE_UP, OnPageUp)
ON_COMMAND (ID_EDIT_EXT_PAGE_UP, OnExtPageUp)
ON_COMMAND (ID_EDIT_PAGE_DOWN, OnPageDown)
ON_COMMAND (ID_EDIT_EXT_PAGE_DOWN, OnExtPageDown)
ON_COMMAND (ID_EDIT_LINE_END, OnLineEnd)
ON_COMMAND (ID_EDIT_EXT_LINE_END, OnExtLineEnd)
ON_COMMAND (ID_EDIT_HOME, OnHome)
ON_COMMAND (ID_EDIT_EXT_HOME, OnExtHome)
ON_COMMAND (ID_EDIT_TEXT_BEGIN, OnTextBegin)
ON_COMMAND (ID_EDIT_EXT_TEXT_BEGIN, OnExtTextBegin)
ON_COMMAND (ID_EDIT_TEXT_END, OnTextEnd)
ON_COMMAND (ID_EDIT_EXT_TEXT_END, OnExtTextEnd)
//  Standard printing commands
ON_COMMAND (ID_FILE_PAGE_SETUP, OnFilePageSetup)
ON_COMMAND (ID_FILE_PRINT, __super::OnFilePrint)
ON_COMMAND (ID_FILE_PRINT_DIRECT, __super::OnFilePrint)
ON_COMMAND (ID_FILE_PRINT_PREVIEW, __super::OnFilePrintPreview)
//  Status
ON_UPDATE_COMMAND_UI (ID_EDIT_INDICATOR_CRLF, OnUpdateIndicatorCRLF)
ON_UPDATE_COMMAND_UI (ID_EDIT_INDICATOR_POSITION, OnUpdateIndicatorPosition)
//  Bookmarks
ON_COMMAND_RANGE (ID_EDIT_TOGGLE_BOOKMARK0, ID_EDIT_TOGGLE_BOOKMARK9, OnToggleBookmark)
ON_COMMAND_RANGE (ID_EDIT_GO_BOOKMARK0, ID_EDIT_GO_BOOKMARK9, OnGoBookmark)
ON_COMMAND (ID_EDIT_CLEAR_BOOKMARKS, OnClearBookmarks)
// More Bookmarks
ON_COMMAND (ID_EDIT_TOGGLE_BOOKMARK, OnToggleBookmark)
ON_COMMAND (ID_EDIT_GOTO_NEXT_BOOKMARK, OnNextBookmark)
ON_COMMAND (ID_EDIT_GOTO_PREV_BOOKMARK, OnPrevBookmark)
ON_COMMAND (ID_EDIT_CLEAR_ALL_BOOKMARKS, OnClearAllBookmarks)
ON_UPDATE_COMMAND_UI (ID_EDIT_GOTO_NEXT_BOOKMARK, OnUpdateNextBookmark)
ON_UPDATE_COMMAND_UI (ID_EDIT_GOTO_PREV_BOOKMARK, OnUpdatePrevBookmark)
ON_UPDATE_COMMAND_UI (ID_EDIT_CLEAR_ALL_BOOKMARKS, OnUpdateClearAllBookmarks)
// Ferdi's source type chnages
ON_COMMAND_RANGE (ID_SOURCE_PLAIN, ID_SOURCE_TEX, OnSourceType)
ON_UPDATE_COMMAND_UI_RANGE (ID_SOURCE_PLAIN, ID_SOURCE_TEX, OnUpdateSourceType)
ON_COMMAND (ID_EDIT_MATCHBRACE, OnMatchBrace)
ON_UPDATE_COMMAND_UI (ID_EDIT_MATCHBRACE, OnUpdateMatchBrace)
ON_COMMAND (ID_EDIT_GOTO, OnEditGoTo)
ON_UPDATE_COMMAND_UI (ID_VIEW_TOGGLE_SRC_HDR, OnUpdateToggleSourceHeader)
ON_COMMAND (ID_VIEW_TOGGLE_SRC_HDR, OnToggleSourceHeader)
ON_UPDATE_COMMAND_UI (ID_VIEW_TOPMARGIN, OnUpdateTopMargin)
ON_COMMAND (ID_VIEW_TOPMARGIN, OnTopMargin)
ON_UPDATE_COMMAND_UI (ID_VIEW_SELMARGIN, OnUpdateSelMargin)
ON_COMMAND (ID_VIEW_SELMARGIN, OnSelMargin)
ON_UPDATE_COMMAND_UI (ID_VIEW_WORDWRAP, OnUpdateWordWrap)
ON_COMMAND (ID_VIEW_WORDWRAP, OnWordWrap)
ON_COMMAND (ID_FORCE_REDRAW, OnForceRedraw)
  //BEGIN SW
  // incremental search
  ON_COMMAND(ID_EDIT_FIND_INCREMENTAL_FORWARD, OnEditFindIncrementalForward)
  ON_COMMAND(ID_EDIT_FIND_INCREMENTAL_BACKWARD, OnEditFindIncrementalBackward)
  ON_UPDATE_COMMAND_UI(ID_EDIT_FIND_INCREMENTAL_FORWARD, OnUpdateEditFindIncrementalForward)
  ON_UPDATE_COMMAND_UI(ID_EDIT_FIND_INCREMENTAL_BACKWARD, OnUpdateEditFindIncrementalBackward)
  //END SW
  ON_COMMAND (ID_EDIT_TOGGLE_COLUMNSELECTION, OnToggleColumnSelection)
  ON_MESSAGE (WM_DPICHANGED_BEFOREPARENT, OnDpiChangedBeforeParent)
END_MESSAGE_MAP ()

#define EXPAND_PRIMITIVE(impl, func)    \
void CCrystalTextView::On##func() { m_bRectangularSelection = false; impl(false); }  \
void CCrystalTextView::OnExt##func() { impl(true); }
EXPAND_PRIMITIVE (MoveLeft, CharLeft)
EXPAND_PRIMITIVE (MoveRight, CharRight)
EXPAND_PRIMITIVE (MoveWordLeft, WordLeft)
EXPAND_PRIMITIVE (MoveWordRight, WordRight)
EXPAND_PRIMITIVE (MoveUp, LineUp)
EXPAND_PRIMITIVE (MoveDown, LineDown)
EXPAND_PRIMITIVE (MovePgUp, PageUp)
EXPAND_PRIMITIVE (MovePgDn, PageDown)
EXPAND_PRIMITIVE (MoveHome, Home)
EXPAND_PRIMITIVE (MoveEnd, LineEnd)
EXPAND_PRIMITIVE (MoveCtrlHome, TextBegin)
EXPAND_PRIMITIVE (MoveCtrlEnd, TextEnd)
#undef EXPAND_PRIMITIVE

/////////////////////////////////////////////////////////////////////////////
// CCrystalTextView construction/destruction

bool CCrystalTextView::
DoSetTextType (CrystalLineParser::TextDefinition *def)
{
  m_CurSourceDef = def;
  SetFlags (def->flags);

// Do not set these
// EOL is determined from file, tabsize and viewtabs are
// global WinMerge settings, selection margin is not needed
// and wordwrapping must be false always
#if 0
  SetWordWrapping ((def->flags & SRCOPT_WORDWRAP) != false);
  SetSelectionMargin ((def->flags & SRCOPT_SELMARGIN) != false);
  SetTabSize (def->tabsize);
  SetViewTabs ((def->flags & SRCOPT_SHOWTABS) != false);
  int nEoln;
  if (def->flags & SRCOPT_EOLNDOS)
    {
      nEoln = 0;
    }
  else if (def->flags & SRCOPT_EOLNUNIX)
    {
      nEoln = 1;
    }
  else if (def->flags & SRCOPT_EOLNMAC)
    {
      nEoln = 2;
    }
  else /* eoln auto */
    {
      nEoln = -1;
    }
  SetCRLFMode (nEoln);
#endif
  return true;
}

bool CCrystalTextView::
SetTextType (LPCTSTR pszExt)
{
  m_CurSourceDef = CrystalLineParser::m_SourceDefs;

  CrystalLineParser::TextDefinition *def = CrystalLineParser::GetTextType (pszExt);

  return SetTextType (def);
}

bool CCrystalTextView::
SetTextType (CrystalLineParser::TextType enuType)
{
  CrystalLineParser::TextDefinition *def;

  m_CurSourceDef = def = CrystalLineParser::m_SourceDefs;
  for (int i = 0; i < _countof (CrystalLineParser::m_SourceDefs); i++, def++)
    {
      if (def->type == enuType)
        {
          return SetTextType (def);
        }
    }
  return false;
}

bool CCrystalTextView::
SetTextType (CrystalLineParser::TextDefinition *def)
{
  if (def)
    if (m_CurSourceDef != def)
      return DoSetTextType (def);
    else
      return true;
  return false;
}

void CCrystalTextView::
LoadSettings ()
{
  CrystalLineParser::TextDefinition *def = CrystalLineParser::m_SourceDefs;
  bool bFontLoaded;
  CReg reg;
  if (reg.Open (HKEY_CURRENT_USER, REG_EDITPAD, KEY_READ))
    {
      reg.LoadNumber (_T ("DefaultEncoding"), (DWORD*) &CCrystalTextBuffer::m_nDefaultEncoding);
      for (int i = 0; i < _countof (CrystalLineParser::m_SourceDefs); i++, def++)
        {
          CReg reg1;
          if (reg1.Open (reg.hKey, def->name, KEY_READ))
            {
              reg1.LoadString (_T ("Extensions"), def->exts, _countof (def->exts));
              reg1.LoadNumber (_T ("Flags"), &def->flags);
//              reg1.LoadNumber (_T ("TabSize"), &def->tabsize);
              reg1.LoadString (_T ("OpenComment"), def->opencomment, _countof (def->opencomment));
              reg1.LoadString (_T ("CloseComment"), def->closecomment, _countof (def->closecomment));
              reg1.LoadString (_T ("CommentLine"), def->commentline, _countof (def->commentline));
              reg1.LoadNumber (_T ("DefaultEncoding"), &def->encoding);
            }
        }
      bFontLoaded = reg.LoadBinary (_T ("LogFont"), (LPBYTE) &m_LogFont, sizeof (m_LogFont));
    }
  else
    bFontLoaded = false;
  if (!bFontLoaded)
    {
      CWindowDC dc (CWnd::GetDesktopWindow ());
      NONCLIENTMETRICS info;
      info.cbSize = sizeof(info);
      SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(info), &info, 0);
      memcpy (&m_LogFont, &info.lfMessageFont, sizeof (LOGFONT));
      m_LogFont.lfHeight = -MulDiv (11, dc.GetDeviceCaps (LOGPIXELSY), 72);
      m_LogFont.lfWeight = FW_NORMAL;
      m_LogFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
      m_LogFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
      m_LogFont.lfQuality = DEFAULT_QUALITY;
      m_LogFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
      _tcscpy_s (m_LogFont.lfFaceName, _T ("Courier New"));
    }
}

void CCrystalTextView::
SaveSettings ()
{
  CrystalLineParser::TextDefinition *def = CrystalLineParser::m_SourceDefs;
  CReg reg;
  if (reg.Create (HKEY_CURRENT_USER, REG_EDITPAD, KEY_WRITE))
    {
      VERIFY (reg.SaveNumber (_T ("DefaultEncoding"), (DWORD) CCrystalTextBuffer::m_nDefaultEncoding));
      for (int i = 0; i < _countof (CrystalLineParser::m_SourceDefs); i++, def++)
        {
          CReg reg1;
          if (reg1.Create (reg.hKey, def->name, KEY_WRITE))
            {
              VERIFY (reg1.SaveString (_T ("Extensions"), def->exts));
              VERIFY (reg1.SaveNumber (_T ("Flags"), def->flags));
//              VERIFY (reg1.SaveNumber (_T ("TabSize"), def->tabsize));
              VERIFY (reg1.SaveString (_T ("OpenComment"), def->opencomment));
              VERIFY (reg1.SaveString (_T ("CloseComment"), def->closecomment));
              VERIFY (reg1.SaveString (_T ("CommentLine"), def->commentline));
              VERIFY (reg1.SaveNumber (_T ("DefaultEncoding"), def->encoding));
            }
        }
      VERIFY (reg.SaveBinary (_T ("LogFont"), (LPBYTE) &m_LogFont, sizeof (m_LogFont)));
    }
}

CCrystalTextView::CCrystalTextView ()
: m_nScreenChars(-1)
, m_pFindTextDlg(nullptr)
, m_CurSourceDef(nullptr)
, m_dwLastDblClickTime(0)
, m_rxnode(nullptr)
, m_pszMatched(nullptr)
, m_bTopMargin(false)
, m_bSelMargin(true)
, m_bViewLineNumbers(false)
, m_bWordWrap(false)
, m_bHideLines(false)
, m_bLastSearch(false)
, m_bBookmarkExist(false)
, m_bSingle(false) // needed to be set in descendat classes
, m_bRememberLastPos(false)
, m_pColors(nullptr)
, m_nLastLineIndexCalculatedSubLineIndex(-1)
, m_hAccel(nullptr)
, m_pTextBuffer(nullptr)
, m_pCacheBitmap(nullptr)
, m_pszLastFindWhat(nullptr)
, m_dwLastSearchFlags(0)
, m_bMultipleSearch(false)
, m_bCursorHidden(false)
, m_nLineHeight(0)
, m_nCharWidth(0)
, m_bViewTabs(false)
, m_bViewEols(false)
, m_bDistinguishEols(false)
, m_dwFlags(0)
, m_nScreenLines(0)
, m_pMarkers(nullptr)
, m_panSubLines(new CArray<int, int>())
, m_panSubLineIndexCache(new CArray<int, int>())
, m_pstrIncrementalSearchString(new CString)
, m_pstrIncrementalSearchStringOld(new CString)
, m_ParseCookies(new vector<DWORD>)
, m_pnActualLineLength(new vector<int>)
, m_nIdealCharPos(0)
, m_bFocused(false)
, m_lfBaseFont{}
, m_lfSavedBaseFont{}
, m_pParser(nullptr)
, m_pPrintFont(nullptr)
#ifdef _UNICODE
, m_bChWidthsCalculated{}
, m_iChDoubleWidthFlags{}
#endif
, m_bPreparingToDrag(false)
, m_bDraggingText(false)
, m_bDragSelection(false)
, m_bWordSelection(false)
, m_bLineSelection(false)
, m_bRectangularSelection(false)
, m_bColumnSelection(false)
, m_nDragSelTimer(0)
, m_bOverrideCaret(false)
, m_nLastFindWhatLen(0)
, m_nPrintPages(0)
, m_nPrintLineHeight(0)
, m_bPrintFooter(false)
, m_bPrintHeader(false)
, m_bPrinting(false)
, m_nTopLine(0)
, m_nOffsetChar(0)
, m_nTopSubLine(0)
, m_bSmoothScroll(false)
, m_bVertScrollBarLocked(false)
, m_bHorzScrollBarLocked(false)
, m_bShowInactiveSelection(false)
, m_bDisableDragAndDrop(false)
, m_bIncrementalSearchForward(false)
, m_bIncrementalSearchBackward(false)
, m_bIncrementalFound(false)
, m_rxmatch{}
, m_nRenderingMode(s_nRenderingModeDefault)
, m_pCrystalRendererSaved(nullptr)
, m_nColumnResizing(-1)
{
#ifdef _WIN64
  if (m_nRenderingMode == RENDERING_MODE_GDI)
    m_pCrystalRenderer.reset(new CCrystalRendererGDI());
  else
    m_pCrystalRenderer.reset(new CCrystalRendererDirectWrite(m_nRenderingMode));
#else
  m_pCrystalRenderer.reset(new CCrystalRendererGDI());
#endif

  m_panSubLines->SetSize( 0, 4096 );
  m_panSubLineIndexCache->SetSize( 0, 4096 );

  //END SW
  CCrystalTextView::ResetView ();
  SetTextType (CrystalLineParser::SRC_PLAIN);
}

CCrystalTextView::~CCrystalTextView ()
{
  ASSERT (m_hAccel == nullptr);
  ASSERT (m_pCacheBitmap == nullptr);
  ASSERT (m_pTextBuffer == nullptr);   //  Must be correctly detached

  delete m_pFindTextDlg;

  free (m_pszLastFindWhat);
  m_pszLastFindWhat=nullptr;

  RxFree (m_rxnode);
  m_rxnode = nullptr;

  free(m_pszMatched); // Allocated by _tcsdup()
  m_pszMatched = nullptr;

  //BEGIN SW
  delete m_panSubLines;
  m_panSubLines = nullptr;

  delete m_panSubLineIndexCache;
  m_panSubLineIndexCache = nullptr;

  delete m_pstrIncrementalSearchString;
  m_pstrIncrementalSearchString = nullptr;

  delete m_pstrIncrementalSearchStringOld;
  m_pstrIncrementalSearchStringOld = nullptr;

  //END SW
  ASSERT(m_ParseCookies != nullptr);
  delete m_ParseCookies;
  m_ParseCookies = nullptr;
  ASSERT(m_pnActualLineLength != nullptr);
  delete m_pnActualLineLength;
  m_pnActualLineLength = nullptr;
  if (m_pMarkers != nullptr)
    m_pMarkers->DeleteView(this);
}

BOOL CCrystalTextView::
PreCreateWindow (CREATESTRUCT & cs)
{
  CWnd *pParentWnd = CWnd::FromHandlePermanent (cs.hwndParent);
  if (pParentWnd == nullptr || !pParentWnd->IsKindOf (RUNTIME_CLASS (CSplitterWnd)))
    {
      //  View must always create its own scrollbars,
      //  if only it's not used within splitter
      //BEGIN SW
      if( GetTextLayoutMode () == TEXTLAYOUT_WORDWRAP )
        // we do not need a horizontal scroll bar, if we wrap the lines
        cs.style|= WS_VSCROLL;
      else
        cs.style |= (WS_HSCROLL | WS_VSCROLL);
      /*ORIGINAL
      cs.style |= (WS_HSCROLL | WS_VSCROLL);
      */
      //END SW
    }
  cs.lpszClass = AfxRegisterWndClass (CS_DBLCLKS);
  return __super::PreCreateWindow (cs);
}


/////////////////////////////////////////////////////////////////////////////
// CCrystalTextView drawing

void CCrystalTextView::
GetSelection (CPoint & ptStart, CPoint & ptEnd)
{
  PrepareSelBounds ();
  ptStart = m_ptDrawSelStart;
  ptEnd = m_ptDrawSelEnd;
}

bool CCrystalTextView::
GetColumnSelection (int nLineIndex, int & nSelBegin, int & nSelEnd)
{
  int nSelTop, nSelBottom;
  if (m_ptDrawSelStart.y < m_ptDrawSelEnd.y)
    {
      nSelTop = m_ptDrawSelStart.y;
      nSelBottom = m_ptDrawSelEnd.y;
    }
  else
    {
      nSelTop = m_ptDrawSelEnd.y;
      nSelBottom = m_ptDrawSelStart.y;
    }

  if (nSelTop > nLineIndex || nLineIndex > nSelBottom)
    {
      nSelBegin = 0;
      nSelEnd = 0;
      return false;
    }
  else
    {
      int nStartCharPos = CalculateActualOffset (m_ptDrawSelStart.y, m_ptDrawSelStart.x, true);
      int nEndCharPos = CalculateActualOffset (m_ptDrawSelEnd.y, m_ptDrawSelEnd.x, true);
      int nLeftCharPos, nRightCharPos;
      if (nStartCharPos > nEndCharPos)
        {
          nLeftCharPos = nEndCharPos;
          nRightCharPos = nStartCharPos;
        }
      else
        {
          nLeftCharPos = nStartCharPos;
          nRightCharPos = nEndCharPos;
        }
      if (nRightCharPos < m_nIdealCharPos)
        nRightCharPos = m_nIdealCharPos;
      nSelBegin = ApproxActualOffset (nLineIndex, nLeftCharPos);
      nSelEnd = ApproxActualOffset (nLineIndex, nRightCharPos);
      return true;
    }
}

void CCrystalTextView::
GetFullySelectedLines(int & firstLine, int & lastLine)
{
  CPoint ptStart;
  CPoint ptEnd;
  GetSelection(ptStart, ptEnd);

  if (ptStart.x == 0)
    firstLine = ptStart.y;
  else
    firstLine = ptStart.y + 1;
  if (ptEnd.x == GetLineLength(ptEnd.y))
    lastLine = ptEnd.y;
  else
    lastLine = ptEnd.y-1;
}

CCrystalTextBuffer *CCrystalTextView::
LocateTextBuffer ()
{
  return nullptr;
}

/**
 * @brief : Get the line length, for cursor movement 
 *
 * @note : there are at least 4 line lengths :
 * - number of characters (memory, no EOL)
 * - number of characters (memory, with EOL)
 * - number of characters for cursor position (tabs are expanded, no EOL)
 * - number of displayed characters (tabs are expanded, with EOL)
 * Corresponding functions :
 * - GetLineLength
 * - GetFullLineLength
 * - GetLineActualLength
 * - ExpandChars (returns the line to be displayed as a CString)
 */
int CCrystalTextView::
GetLineActualLength (int nLineIndex)
{
  const int nLineCount = GetLineCount ();
  ASSERT (nLineCount > 0);
  ASSERT (nLineIndex >= 0 && nLineIndex < nLineCount);
  if (!m_pnActualLineLength->size())
    {
      m_pnActualLineLength->assign(nLineCount, -1);
    }

  if ((*m_pnActualLineLength)[nLineIndex] != - 1)
    return (*m_pnActualLineLength)[nLineIndex];

  //  Actual line length is not determined yet, let's calculate a little
  int nActualLength = 0;
  int nLength = GetLineLength (nLineIndex);
  if (nLength > 0)
    {
      LPCTSTR pszChars = GetLineChars (nLineIndex);
      const int nTabSize = GetTabSize ();
      auto pIterChar = ICUBreakIterator::getCharacterBreakIterator(pszChars, nLength);
      switch ( GetTextLayoutMode ())
        {
          case TEXTLAYOUT_TABLE_NOWORDWRAP:
          case TEXTLAYOUT_TABLE_WORDWRAP:
            {
              int nColumnCount =  m_pTextBuffer->GetColumnCount (nLineIndex);
              int nColumnTotalWidth = 0;
              bool bInQuote = false;
              const int sep = m_pTextBuffer->GetFieldDelimiter ();
              const int quote = m_pTextBuffer->GetFieldEnclosure ();
              for (int i = 0, nColumn = 0; i < nLength; i = pIterChar->next())
                {
                  TCHAR c = pszChars[i];
                  if (!bInQuote && c == sep)
                    {
                      nColumnTotalWidth += m_pTextBuffer->GetColumnWidth (nColumn++);
                      nActualLength = nColumnTotalWidth;
                    }
                  else
                    {
                      if (c == quote)
                        bInQuote = !bInQuote;
                      if (c == '\t')
                        nActualLength ++;
                      else
                        nActualLength += GetCharCellCountFromChar (pszChars + i);
                      if (nColumn < nColumnCount && nActualLength > nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn))
                        nActualLength = nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn);
                    }
                }
            }
            break;
          default:
            {
              for (int i = 0; i < nLength; i = pIterChar->next())
                {
                  TCHAR c = pszChars[i];
                  if (c == _T('\t'))
                    nActualLength += (nTabSize - nActualLength % nTabSize);
                  else
                    nActualLength += GetCharCellCountFromChar(pszChars + i);
                }
            }
        }
    }

  (*m_pnActualLineLength)[nLineIndex] = nActualLength;
  return nActualLength;
}

void CCrystalTextView::
ScrollToChar (int nNewOffsetChar, bool bNoSmoothScroll /*= false*/, bool bTrackScrollBar /*= true*/ )
{
  //BEGIN SW
  // no horizontal scrolling, when word wrapping is enabled
  if (GetTextLayoutMode () == TEXTLAYOUT_WORDWRAP)
    return;
  //END SW

  //  For now, ignoring bNoSmoothScroll and m_bSmoothScroll
  if (m_nOffsetChar != nNewOffsetChar)
    {
      int nScrollChars = m_nOffsetChar - nNewOffsetChar;
      m_nOffsetChar = nNewOffsetChar;
      CRect rcScroll;
      GetClientRect (&rcScroll);
      rcScroll.left += GetMarginWidth ();
      ScrollWindow (nScrollChars * GetCharWidth (), 0, &rcScroll, &rcScroll);
      UpdateWindow ();
      if (bTrackScrollBar)
        RecalcHorzScrollBar (true);
    }
}

/**
 * @brief Scroll view to given line.
 * Scrolls view so that given line is first line in the view. We limit
 * scrolling so that there is only one empty line visible after the last
 * line at max. So we don't allow user to scroll last line being at top or
 * even at middle of the screen. This is how many editors behave and I
 * (Kimmo) think it is good for us too.
 * @param [in] nNewTopSubLine New top line for view.
 * @param [in] bNoSmoothScroll if true don't use smooth scrolling.
 * @param [in] bTrackScrollBar if true scrollbar is updated after scroll.
 */
void CCrystalTextView::ScrollToSubLine( int nNewTopSubLine, 
                  bool bNoSmoothScroll /*= false*/, bool bTrackScrollBar /*= true*/ )
{
  if (m_nTopSubLine != nNewTopSubLine)
    {
      CRect rcScroll;
      GetClientRect (&rcScroll);
      rcScroll.top += GetTopMarginHeight ();

      if (bNoSmoothScroll || ! m_bSmoothScroll)
        {
          // Limit scrolling so that we show one empty line at end of file
          const int nScreenLines = GetScreenLines();
          const int nLineCount = GetSubLineCount();
          if (nNewTopSubLine > (nLineCount - nScreenLines))
            {
              nNewTopSubLine = nLineCount - nScreenLines;
              if (nNewTopSubLine < 0)
                nNewTopSubLine = 0;
            }

          const int nScrollLines = m_nTopSubLine - nNewTopSubLine;
          m_nTopSubLine = nNewTopSubLine;
          // OnDraw() uses m_nTopLine to determine topline
          int dummy;
          GetLineBySubLine(m_nTopSubLine, m_nTopLine, dummy);
          ScrollWindow(0, nScrollLines * GetLineHeight(), nullptr, rcScroll);
          UpdateWindow();
          if (bTrackScrollBar)
            {
              RecalcVertScrollBar(true);
              RecalcHorzScrollBar();
            }
        }
      else
        {
          // Do smooth scrolling
          int nLineHeight = GetLineHeight();
          if (m_nTopSubLine > nNewTopSubLine)
            {
              int nIncrement = (m_nTopSubLine - nNewTopSubLine) / SMOOTH_SCROLL_FACTOR + 1;
              while (m_nTopSubLine != nNewTopSubLine)
                {
                  int nTopSubLine = m_nTopSubLine - nIncrement;
                  if (nTopSubLine < nNewTopSubLine)
                    nTopSubLine = nNewTopSubLine;
                  const int nScrollLines = nTopSubLine - m_nTopSubLine;
                  m_nTopSubLine = nTopSubLine;
                  ScrollWindow(0, - nLineHeight * nScrollLines, nullptr, rcScroll);
                  UpdateWindow();
                  if (bTrackScrollBar)
                    {
                      RecalcVertScrollBar(true);
                      RecalcHorzScrollBar();
                    }
                }
            }
          else
            {
              int nIncrement = (nNewTopSubLine - m_nTopSubLine) / SMOOTH_SCROLL_FACTOR + 1;
              while (m_nTopSubLine != nNewTopSubLine)
                {
                  int nTopSubLine = m_nTopSubLine + nIncrement;
                  if (nTopSubLine > nNewTopSubLine)
                    nTopSubLine = nNewTopSubLine;
                  const int nScrollLines = nTopSubLine - m_nTopSubLine;
                  m_nTopSubLine = nTopSubLine;
                  ScrollWindow(0, - nLineHeight * nScrollLines, rcScroll, rcScroll);
                  UpdateWindow();
                  if (bTrackScrollBar)
                    {
                      RecalcVertScrollBar(true);
                      RecalcHorzScrollBar();
                    }
                }
            }
        }
      int nDummy;
      GetLineBySubLine( m_nTopSubLine, m_nTopLine, nDummy );
    }
}

void CCrystalTextView::
ScrollToLine (int nNewTopLine, bool bNoSmoothScroll /*= false*/, bool bTrackScrollBar /*= true*/ )
{
  if( m_nTopLine != nNewTopLine )
    ScrollToSubLine( GetSubLineIndex( nNewTopLine ), bNoSmoothScroll, bTrackScrollBar );
}

/** Append szadd to string str, and advance position curpos */
static void AppendStringAdv(CString & str, int & curpos, LPCTSTR szadd)
{
  str += szadd;
  curpos += (int) _tcslen(szadd);
}

/** Append escaped control char to string str, and advance position curpos */
static void AppendEscapeAdv(CString & str, int & curpos, TCHAR c)
{
  int curlen = str.GetLength();
  LPTSTR szadd = str.GetBufferSetLength(curlen + 3) + curlen;
  curpos += wsprintf(szadd, _T("\t%02X"), static_cast<int>(c));
}

int CCrystalTextView::
ExpandChars (int nLineIndex, int nOffset, int nCount, CString & line, int nActualOffset)
{
  line.Empty();
  // Request whitespace characters for codepage ACP
  // because that is the codepage used by ExtTextOut
  const ViewableWhitespaceChars * lpspc = GetViewableWhitespaceChars(GetACP(), m_nRenderingMode != RENDERING_MODE_GDI);

  if (nCount <= 0)
    {
      return 0;
    }

  const int nTabSize = GetTabSize ();

  LPCTSTR pszChars = GetLineChars(nLineIndex);
  pszChars += nOffset;
  int nLength = nCount;

  for (int i = 0; i < nLength; i++)
    {
      TCHAR c = pszChars[i];
      if (c == _T('\t'))
        nCount += nTabSize - 1;
      else if (c >= _T('\x00') && c <= _T('\x1F') && c != _T('\r') && c != _T('\n'))
        nCount += 2;
    }

  // Preallocate line buffer, to avoid reallocations as we add characters
  line.GetBuffer(nCount + 1); // at least this many characters
  line.ReleaseBuffer(0);
  int nCurPos = 0;
  const bool bTableEditing = m_pTextBuffer->GetTableEditing ();

  if (nCount > nLength || m_bViewTabs || m_bViewEols)
    {
      auto pIterChar = ICUBreakIterator::getCharacterBreakIterator(pszChars, nLength);
      for (int i = 0, next = 0; i < nLength; i = next)
        {
          next = pIterChar->next();
          if (pszChars[i] == _T('\t'))
            {
              int nSpaces = bTableEditing ? 1 : (nTabSize - (nActualOffset + nCurPos) % nTabSize);
              if (m_bViewTabs)
                {
                  AppendStringAdv(line, nCurPos, lpspc->c_tab);
                  nSpaces--;
                }
              while (nSpaces > 0)
                {
                  line += _T(' ');
                  nCurPos++;
                  nSpaces--;
                }
            }
          else  if (pszChars[i] == ' ' && m_bViewTabs)
            AppendStringAdv(line, nCurPos, lpspc->c_space);
          else if (pszChars[i] == '\r' || pszChars[i] == '\n')
            {
              if (m_bViewEols)
                {
                  if (pszChars[i] == '\n' && !m_bDistinguishEols && i+nOffset>0 && pszChars[i-1] == '\r')
                    {
                      // Ignore \n after \r
                    }
                  else
                    {
                      if (pszChars[i] == '\r' && i < nLength - 1 && pszChars[i+1] == '\n' && m_bDistinguishEols)
                        {
                          AppendStringAdv(line, nCurPos, lpspc->c_eol);
                          i++;
                        }
                      else if (pszChars[i] == '\r' && m_bDistinguishEols)
                        AppendStringAdv(line, nCurPos, lpspc->c_cr);
                      else if (pszChars[i] == '\n' && m_bDistinguishEols)
                        AppendStringAdv(line, nCurPos, lpspc->c_lf);
                      else
                        {
                          AppendStringAdv(line, nCurPos, lpspc->c_eol);
                        }
                    }
                }
            }
          else if (pszChars[i] >= _T('\x00') && pszChars[i] <= _T('\x1F'))
            {
              AppendEscapeAdv(line, nCurPos, pszChars[i]);
            }
          else
            {
              nCurPos += GetCharCellCountFromChar(pszChars + i);
              for (; i < next; ++i)
                line += pszChars[i];
            }
        }
    }
  else
    {
      auto pIterChar = ICUBreakIterator::getCharacterBreakIterator(pszChars, nLength);
      for (int i1=0, next=0; i1<nLength; i1 = next)
        {
          next = pIterChar->next();
          nCurPos += GetCharCellCountFromChar(pszChars + i1);
          for (; i1 < next; ++i1)
            line += pszChars[i1];
        }
    }
  return nCurPos;
}

int CCrystalTextView::
ExpandCharsTableEditingNoWrap(int nLineIndex, int nOffset, int nCount, CString& line, int nActualOffset)
{
  if (m_pTextBuffer == nullptr || nCount <= 0)
    return 0;

  LPCTSTR pszChars = GetLineChars(nLineIndex);
  line.Empty();
  // Request whitespace characters for codepage ACP
  // because that is the codepage used by ExtTextOut
  const ViewableWhitespaceChars * lpspc = GetViewableWhitespaceChars(GetACP(), m_nRenderingMode != RENDERING_MODE_GDI);

  int nLength = nCount;
  int nColumn = 0;
  int nColumnCount = 0;
  int nCurColumn = -1;
  int nColumnTotalWidth = 0;
  int nColumnBegin = 0;
  int nColumnTotalWidthBegin = 0;
  const int nLineLength = GetFullLineLength (nLineIndex);
  const int nTabSize = GetTabSize ();
  const int sep = m_pTextBuffer->GetFieldDelimiter ();
  const int quote = m_pTextBuffer->GetFieldEnclosure ();
  const int eollen = nLineLength - GetLineLength (nLineIndex);
  bool bInQuote = false;
  bool bInQuoteBegin = false;

  for (int i = 0; i < nLineLength; i++)
    {
      TCHAR c = pszChars[i];
      if (i == nOffset)
        {
          nColumnBegin = nColumn;
          nColumnTotalWidthBegin = nColumnTotalWidth;
          bInQuoteBegin = bInQuote;
        }
      if (nLineIndex == m_ptCursorPos.y && i == m_ptCursorPos.x)
        nCurColumn = nColumn;
      if (!bInQuote && c == sep)
        {
          nColumnTotalWidth += m_pTextBuffer->GetColumnWidth (nColumn++);
          nCount = nColumnTotalWidth;
        }
      else if (c == '\t')
        nCount += 1 - 1;
      else if (c == quote)
        {
          bInQuote = !bInQuote;
        }
      else if (c >= '\x00' && c <= '\x1F' && c != '\r' && c != '\n')
        nCount += 3 - 1;
    }
  nColumnCount = nColumn + 1;

  // Preallocate line buffer, to avoid reallocations as we add characters
  line.GetBuffer (nCount + 1); // at least this many characters
  line.ReleaseBuffer (0);
  int nCurPos = nActualOffset;

  pszChars += nOffset;

  int curColumnTextCellWidth = 0;
  bool beforeCursorPos = (nLineIndex == m_ptCursorPos.y && nOffset < m_ptCursorPos.x);
  CString curColumnText;
  std::vector<std::pair<int, int>> curColumnByteLenCellWidth;
  nColumn = nColumnBegin;
  nColumnTotalWidth = nColumnTotalWidthBegin;
  bInQuote = bInQuoteBegin;
  auto pIterChar = ICUBreakIterator::getCharacterBreakIterator (pszChars, nLength);
  auto nextColumnDistance = [&](int nCurPos)
    {
      return (nColumn == nColumnCount - 1) ? INT_MAX : nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn) - nCurPos;
    };
  auto appendChars = [&](int i, int next, int pos, CString& text, int& textwidth)
    {
      TCHAR c = pszChars[i];
      if ((c == '\r' || c == '\n') && i >= nLineLength - nOffset - eollen)
        {
          if (m_bViewEols)
            {
              if (c == '\n' && !m_bDistinguishEols && i+nOffset>0 && pszChars[i-1] == '\r')
                {
                  // Ignore \n after \r
                }
              else
                {
                  int prevtextwidth = textwidth;
                  if (c == '\r' && i < nLength - 1 && pszChars[i+1] == '\n' && m_bDistinguishEols)
                    AppendStringAdv (text, textwidth, lpspc->c_eol);
                  else if (c == '\r' && m_bDistinguishEols)
                    AppendStringAdv (text, textwidth, lpspc->c_cr);
                  else if (c == '\n' && m_bDistinguishEols)
                    {
                      if (i == 0 || pszChars[i - 1] != '\r')
                        AppendStringAdv (text, textwidth, lpspc->c_lf);
                    }
                  else
                    AppendStringAdv (text, textwidth, lpspc->c_eol);
                  if (textwidth - prevtextwidth > 0)
                    curColumnByteLenCellWidth.push_back ({ textwidth - prevtextwidth, textwidth - prevtextwidth});
                }
            }
        }
      else if (c == '\t')
        {
          int nSpaces = 1;
          if (sep != '\t' || bInQuote)
            {
              nSpaces = 1;
              if (nSpaces > nextColumnDistance (pos))
                  nSpaces = nextColumnDistance (pos);
            }
          if (nSpaces >= 1 && m_bViewTabs)
            {
              curColumnByteLenCellWidth.push_back ({ 1, 1 });
              AppendStringAdv (text, textwidth, lpspc->c_tab);
              nSpaces--;
            }
          while (nSpaces > 0)
            {
              curColumnByteLenCellWidth.push_back ({ 1, 1 });
              textwidth++;
              text += ' ';
              nSpaces--;
            }
        }
      else if (c == ' ' && m_bViewTabs)
        {
          curColumnByteLenCellWidth.push_back ({ 1, 1 });
          AppendStringAdv (text, textwidth, lpspc->c_space);
       }
      else if (c >= '\x00' && c <= '\x1F')
        {
          curColumnByteLenCellWidth.push_back ({ 3, 3 });
          AppendEscapeAdv (text, textwidth, c);
          if (c == '\r' && pszChars[i + 1] == '\n')
            AppendEscapeAdv (text, textwidth, pszChars[i + 1]);
        }
      else
        {
          int nLen = GetCharCellCountFromChar (pszChars + i);
          curColumnByteLenCellWidth.push_back ({ nLen, next - i });
          textwidth += nLen;
          for (; i < next; ++i)
            text += pszChars[i];
        }
      if (!bInQuote && c == sep)
        {
          int nSpaces = nextColumnDistance (pos + 1);
          while (nSpaces > 0)
            {
              text += ' ';
              ++textwidth;
              --nSpaces;
            }
          nColumnTotalWidth += m_pTextBuffer->GetColumnWidth (nColumn);
          ++nColumn;
        }
    };
  for (int i = 0, next = 0; i < nLength; i = next)
    {
      next = pIterChar->next ();
      const TCHAR c = pszChars[i];
      if (c == quote)
        bInQuote = !bInQuote;
      int nLen = GetCharCellCountFromChar (pszChars + i);
      if (nColumn == nCurColumn && beforeCursorPos)
        {
          appendChars (i, next, nCurPos + curColumnTextCellWidth, curColumnText, curColumnTextCellWidth);
          if (next + nOffset == m_ptCursorPos.x || next >= nLength)
            {
              beforeCursorPos = false;
              if (curColumnTextCellWidth > nextColumnDistance (nCurPos))
                {
                  for (size_t k = 0; k < curColumnByteLenCellWidth.size () && curColumnTextCellWidth > nextColumnDistance (nCurPos); ++k)
                    {
                      curColumnTextCellWidth -= curColumnByteLenCellWidth[k].first;
                      curColumnText = curColumnText.Mid (curColumnByteLenCellWidth[k].second);
                    }
                  int nSpaces = nextColumnDistance (nCurPos) - curColumnTextCellWidth;
                  if (nSpaces > 0)
                    {
                      CString spaces (' ', nSpaces);
                      curColumnText.Insert (0, spaces);
                      curColumnTextCellWidth = m_pTextBuffer->GetColumnWidth (nColumn);
                    }
                }
              line += curColumnText;
              nCurPos += curColumnTextCellWidth;
            }
        }
      else
        {
          if (nLen <= nextColumnDistance (nCurPos))
            {
              appendChars (i, next, nCurPos, line, nCurPos);
              curColumnByteLenCellWidth.clear ();
            }
          else
            {
              int nSpaces = nextColumnDistance (nCurPos);
              while (nSpaces > 0)
                {
                  line += ' ';
                  ++nCurPos;
                  --nSpaces;
                }
              if (!bInQuote && c == sep)
                {
                  nColumnTotalWidth += m_pTextBuffer->GetColumnWidth (nColumn);
                  ++nColumn;
                }
            }
        }
    }
  return nCurPos - nActualOffset;
}


/**
 * @brief Draw a chunk of text (one color, one line, full or part of line)
 *
 * @note In ANSI build, this routine is buggy for multibytes or double-width characters
 */
void CCrystalTextView::
DrawLineHelperImpl (CPoint & ptOrigin, const CRect & rcClip, int nColorIndex,
                    int nBgColorIndex, COLORREF crText, COLORREF crBkgnd, int nLineIndex, int nOffset, int nCount, int &nActualOffset)
{
  ASSERT (nCount >= 0);
  if (nCount > 0)
    {
      CString line;
      if (GetTextLayoutMode () == TEXTLAYOUT_TABLE_NOWORDWRAP)
        nActualOffset += ExpandCharsTableEditingNoWrap (nLineIndex, nOffset, nCount, line, nActualOffset);
      else
        nActualOffset += ExpandChars (nLineIndex, nOffset, nCount, line, nActualOffset);
      const int lineLen = line.GetLength();
      const int nCharWidth = GetCharWidth();
      const int nCharWidthNarrowed = nCharWidth / 2;
      const int nCharWidthWidened = nCharWidth * 2 - nCharWidthNarrowed;
      const int nLineHeight = GetLineHeight();
      auto pIterChar = ICUBreakIterator::getCharacterBreakIterator((LPCTSTR)line, lineLen);

      // i the character index, from 0 to lineLen-1
      int i = 0;

      // Pass if the text begins after the right end of the clipping region
      if (ptOrigin.x < rcClip.right)
        {
          // Because ExtTextOut is buggy when ptOrigin.x < - 4095 * charWidth
          // or when nCount >= 4095
          // and because this is not well documented,
          // we decide to do the left & right clipping here
          
          // Update the position after the left clipped characters
          // stop for i = first visible character, at least partly
          const int clipLeft = rcClip.left - nCharWidth * 2;
          for ( ; i < lineLen; i = pIterChar->next())
            {
              int pnWidthsCurrent = GetCharCellCountFromChar(static_cast<const TCHAR *>(line) + i) * nCharWidth;
              ptOrigin.x += pnWidthsCurrent;
              if (ptOrigin.x >= clipLeft)
                {
                  ptOrigin.x -= pnWidthsCurrent;
                  break;
                }
            }

          //
#ifdef _DEBUG
          //CSize sz = pdc->GetTextExtent(line, nCount);
          //ASSERT(sz.cx == m_nCharWidth * nCount);
#endif
           
          if (i < lineLen)
            {
              // We have to draw some characters
              int ibegin = i;
              int nSumWidth = 0;

              int nWidth = rcClip.right - ptOrigin.x;

              // Table of charwidths as CCrystalEditor thinks they are
              // Seems that CrystalEditor's and ExtTextOut()'s charwidths aren't
              // same with some fonts and text is drawn only partially
              // if this table is not used.
              vector<int> nWidths(nWidth / nCharWidth * 2 + 2);
              bool bdisphex = false;
              for (int next = i; i < lineLen && nSumWidth < nWidth ; i = next)
                {
                  if (line[i] == '\t') // Escape sequence leadin?
                    {
                      bdisphex = true;
                      // Substitute a space narrowed to half the width of a character cell.
                      line.SetAt(i, ' ');
                      size_t idx = i - ibegin;
                      if (idx >= nWidths.size())
                        nWidths.resize(nWidths.size() * 2);
                      nSumWidth += nWidths[idx] = nCharWidthNarrowed;
                      // 1st hex digit has normal width.
                      idx = pIterChar->next() - ibegin;
                      if (idx >= nWidths.size())
                        nWidths.resize(nWidths.size() * 2);
                      nSumWidth += nWidths[idx] = nCharWidth;
                      // 2nd hex digit is padded by half the width of a character cell.
                      idx = pIterChar->next() - ibegin;
                      if (idx >= nWidths.size())
                        nWidths.resize(nWidths.size() * 2);
                      nSumWidth += nWidths[idx] = nCharWidthWidened;
                    }
                  else
                    {
                      size_t idx = i - ibegin;
                      if (idx >= nWidths.size())
                        nWidths.resize(nWidths.size() * 2);
                      nSumWidth += nWidths[idx] = GetCharCellCountFromChar(static_cast<const TCHAR *>(line) + i) * nCharWidth;
                    }
                  next = pIterChar->next();
                }
              int nCount1 = i - ibegin;

              if (ptOrigin.x + nSumWidth > rcClip.left)
                {
                  if (crText == CLR_NONE || nColorIndex & COLORINDEX_APPLYFORCE)
                    m_pCrystalRenderer->SetTextColor(GetColor(nColorIndex));
                  else
                    m_pCrystalRenderer->SetTextColor(crText);
                  if (crBkgnd == CLR_NONE || nBgColorIndex & COLORINDEX_APPLYFORCE)
                    m_pCrystalRenderer->SetBkColor(GetColor(nBgColorIndex));
                  else
                    m_pCrystalRenderer->SetBkColor(crBkgnd);

                  m_pCrystalRenderer->SwitchFont(GetItalic(nColorIndex), GetBold(nColorIndex));
                  // we are sure to have less than 4095 characters because all the chars are visible
                  RECT rcIntersect;
                  RECT rcTextBlock = {ptOrigin.x, ptOrigin.y, ptOrigin.x + nSumWidth + 2, ptOrigin.y + nLineHeight};
                  IntersectRect(&rcIntersect, &rcClip, &rcTextBlock);
                  m_pCrystalRenderer->DrawText(ptOrigin.x, ptOrigin.y, rcIntersect, LPCTSTR(line) + ibegin, nCount1, &nWidths[0]);
                  if (bdisphex)
                    {
                      // Draw rounded rectangles around control characters
                      m_pCrystalRenderer->PushAxisAlignedClip(rcClip);
                      int x = ptOrigin.x;
                      for (int j = 0 ; j < nCount1 ; ++j)
                        {
                          // Assume narrowed space is converted escape sequence leadin.
                          if (line[ibegin + j] == ' ' && nWidths[j] < nCharWidth)
                            {
                              m_pCrystalRenderer->DrawRoundRectangle(x + 2, ptOrigin.y + 1,
                                                                     x + 3 * nCharWidth - 2, ptOrigin.y + nLineHeight - 1,
                                                                     nCharWidth / 2, nLineHeight / 2);
                            }
                          x += nWidths[j];
                        }
                      m_pCrystalRenderer->PopAxisAlignedClip();
                    }
                }

              // Update the final position after the visible characters
              ptOrigin.x += nSumWidth;

            }
        }
      // Update the final position after the right clipped characters
      for ( ; i < lineLen; i = pIterChar->next())
        {
          ptOrigin.x += GetCharCellCountFromChar(static_cast<const TCHAR *>(line) + i) * nCharWidth;
        }
    }
}

void CCrystalTextView::
DrawLineHelper (CPoint & ptOrigin, const CRect & rcClip, int nColorIndex, int nBgColorIndex, 
                COLORREF crText, COLORREF crBkgnd, int nLineIndex, int nOffset, int nCount, int &nActualOffset, CPoint ptTextPos)
{
  if (nCount > 0)
    {
      if (m_bFocused || m_bShowInactiveSelection)
        {
          int nSelBegin = 0, nSelEnd = 0;
          if ( !m_bRectangularSelection )
            {
              if (m_ptDrawSelStart.y > ptTextPos.y)
                {
                  nSelBegin = nCount;
                }
              else if (m_ptDrawSelStart.y == ptTextPos.y)
                {
                  nSelBegin = m_ptDrawSelStart.x - ptTextPos.x;
                  if (nSelBegin < 0)
                    nSelBegin = 0;
                  if (nSelBegin > nCount)
                    nSelBegin = nCount;
                }
              if (m_ptDrawSelEnd.y > ptTextPos.y)
                {
                  nSelEnd = nCount;
                }
              else if (m_ptDrawSelEnd.y == ptTextPos.y)
                {
                  nSelEnd = m_ptDrawSelEnd.x - ptTextPos.x;
                  if (nSelEnd < 0)
                    nSelEnd = 0;
                  if (nSelEnd > nCount)
                    nSelEnd = nCount;
                }
            }
          else
            {
              int nSelLeft, nSelRight;
              GetColumnSelection (ptTextPos.y, nSelLeft, nSelRight);
              nSelBegin = nSelLeft - ptTextPos.x;
              nSelEnd = nSelRight - ptTextPos.x;
              if (nSelBegin < 0) nSelBegin = 0;
              if (nSelBegin > nCount) nSelBegin = nCount;
              if (nSelEnd < 0) nSelEnd = 0;
              if (nSelEnd > nCount) nSelEnd = nCount;
            }

          ASSERT (nSelBegin >= 0 && nSelBegin <= nCount);
          ASSERT (nSelEnd >= 0 && nSelEnd <= nCount);
          ASSERT (nSelBegin <= nSelEnd);

          //  Draw part of the text before selection
          if (nSelBegin > 0)
            {
              DrawLineHelperImpl (ptOrigin, rcClip, nColorIndex, nBgColorIndex, crText, crBkgnd, nLineIndex, nOffset, nSelBegin, nActualOffset);
            }
          if (nSelBegin < nSelEnd)
            {
              DrawLineHelperImpl (ptOrigin, rcClip,
                                  nColorIndex & ~COLORINDEX_APPLYFORCE,
                                  nBgColorIndex & ~COLORINDEX_APPLYFORCE,
                                  GetColor (COLORINDEX_SELTEXT),
                                  GetColor (COLORINDEX_SELBKGND),
                                  nLineIndex,
                                  nOffset + nSelBegin, nSelEnd - nSelBegin, nActualOffset);
            }
          if (nSelEnd < nCount)
            {
              DrawLineHelperImpl (ptOrigin, rcClip, nColorIndex, nBgColorIndex, crText, crBkgnd, nLineIndex, nOffset + nSelEnd, nCount - nSelEnd, nActualOffset);
            }
        }
      else
        {
          DrawLineHelperImpl (ptOrigin, rcClip, nColorIndex, nBgColorIndex, crText, crBkgnd, nLineIndex, nOffset, nCount, nActualOffset);
        }
    }
}

void CCrystalTextView::
GetLineColors (int nLineIndex, COLORREF & crBkgnd,
               COLORREF & crText, bool & bDrawWhitespace)
{
  DWORD dwLineFlags = GetLineFlags (nLineIndex);
  bDrawWhitespace = true;
  crText = RGB (255, 255, 255);
  if (dwLineFlags & LF_EXECUTION)
    {
      crBkgnd = RGB (0, 128, 0);
      return;
    }
  if (dwLineFlags & LF_BREAKPOINT)
    {
      crBkgnd = RGB (255, 0, 0);
      return;
    }
  if (dwLineFlags & LF_INVALID_BREAKPOINT)
    {
      crBkgnd = RGB (128, 128, 0);
      return;
    }
  crBkgnd = CLR_NONE;
  crText = CLR_NONE;
  bDrawWhitespace = false;
}

DWORD CCrystalTextView::
GetParseCookie (int nLineIndex)
{
  const int nLineCount = GetLineCount ();
  if (m_ParseCookies->size() == 0)
    {
      // must be initialized to invalid value (DWORD) -1
      m_ParseCookies->assign(nLineCount, static_cast<DWORD>(-1));
    }

  if (nLineIndex < 0)
    return 0;
  if ((*m_ParseCookies)[nLineIndex] != - 1)
    return (*m_ParseCookies)[nLineIndex];

  int L = nLineIndex;
  while (L >= 0 && (*m_ParseCookies)[L] == - 1)
    L--;
  L++;

  int nBlocks;
  while (L <= nLineIndex)
    {
      DWORD dwCookie = 0;
      if (L > 0)
        dwCookie = (*m_ParseCookies)[L - 1];
      ASSERT (dwCookie != - 1);
      (*m_ParseCookies)[L] = ParseLine (dwCookie, GetLineChars(L), GetLineLength(L), nullptr, nBlocks);
      ASSERT ((*m_ParseCookies)[L] != - 1);
      L++;
    }

  return (*m_ParseCookies)[nLineIndex];
}

std::vector<TEXTBLOCK> CCrystalTextView::
GetAdditionalTextBlocks (int nLineIndex)
{
  return {};
}

//BEGIN SW
void CCrystalTextView::WrapLine( int nLineIndex, int nMaxLineWidth, std::vector<int> *anBreaks, int &nBreaks )
{
  // There must be a parser attached to this view
  if( m_pParser == nullptr )
    return;

  m_pParser->WrapLine( nLineIndex, nMaxLineWidth, anBreaks, nBreaks );
}


void CCrystalTextView::WrapLineCached( 
                    int nLineIndex, int nMaxLineWidth, std::vector<int> *anBreaks, int &nBreaks )
{
  if( !GetLineVisible (nLineIndex) )
    {
      nBreaks = -1;
      return;
    }

  // If the word wrap is not active, there is no breaks in the line
  if( !m_bWordWrap )
    {
      nBreaks = 0;
      return;
    }

  // word wrap is active
  if( nLineIndex < m_panSubLines->GetSize() && !anBreaks && (*m_panSubLines)[nLineIndex] > -1 )
    // return cached data
    nBreaks = (*m_panSubLines)[nLineIndex] - 1;
  else
    {
      // recompute line wrap
      nBreaks = 0;
      WrapLine( nLineIndex, nMaxLineWidth, anBreaks, nBreaks );

      // cache data
      ASSERT( nBreaks > -1 );
      m_panSubLines->SetAtGrow( nLineIndex, nBreaks + 1 );

      // RecalcVertScrollBar();
    }
}


void CCrystalTextView::InvalidateLineCache( int nLineIndex1, int nLineIndex2 /*= -1*/ )
{
  // invalidate cached sub line index
  InvalidateSubLineIndexCache( nLineIndex1 );

  // invalidate cached sub line count

  if( nLineIndex2 == -1 && nLineIndex1 < m_panSubLines->GetSize() )
    for( int i = nLineIndex1; i < m_panSubLines->GetSize(); i++ )
      (*m_panSubLines)[i] = -1;
  else
    {
      if( nLineIndex1 > nLineIndex2 )
        {
          int	nStorage = nLineIndex1;
          nLineIndex1 = nLineIndex2;
          nLineIndex2 = nStorage;
        }

      if( nLineIndex1 >= m_panSubLines->GetSize() )
        return;

      if( nLineIndex2 >= m_panSubLines->GetSize() )
        nLineIndex2 = (int) m_panSubLines->GetUpperBound();

      for( int i = nLineIndex1; i <= nLineIndex2; i++ )
        if( i >= 0 && i < m_panSubLines->GetSize() )
          (*m_panSubLines)[i] = -1;
    }
}

/**
 * @brief Invalidate sub line index cache from the specified index to the end of file.
 * @param [in] nLineIndex Index of the first line to invalidate 
 */
void CCrystalTextView::InvalidateSubLineIndexCache( int nLineIndex )
{
  if (m_nLastLineIndexCalculatedSubLineIndex > nLineIndex)
    m_nLastLineIndexCalculatedSubLineIndex = nLineIndex - 1;
}

/**
 * @brief Invalidate items related screen size.
 */
void CCrystalTextView::InvalidateScreenRect(bool bInvalidateView)
{
  if (m_pCacheBitmap != nullptr)
    {
      delete m_pCacheBitmap;
      m_pCacheBitmap = nullptr;
    }
  m_nScreenChars = -1;
  m_nScreenLines = -1;
  InvalidateLineCache(0, -1);
  if (bInvalidateView)
    {
      Invalidate();
      m_nTopSubLine = GetSubLineIndex(m_nTopLine);
      RecalcVertScrollBar ();
      RecalcHorzScrollBar ();
      UpdateCaret ();
    }
}

void CCrystalTextView::DrawScreenLine( CPoint &ptOrigin, const CRect &rcClip,
         const std::vector<TEXTBLOCK>& blocks, int &nActualItem, 
         COLORREF crText, COLORREF crBkgnd, bool bDrawWhitespace,
         int nLineIndex, int nOffset, int nCount, int &nActualOffset, CPoint ptTextPos )
{
  CPoint	originalOrigin = ptOrigin;
  CPoint	ptOriginZeroWidthBlock;
  CRect		frect = rcClip;
  const int nLineLength = GetViewableLineLength( ptTextPos.y );
  const int nLineHeight = GetLineHeight();
  int nBgColorIndexZeorWidthBlock = COLORINDEX_NONE;
  bool bPrevZeroWidthBlock = false;
  static const int ZEROWIDTHBLOCK_WIDTH = 2;

  frect.top = ptOrigin.y;
  frect.bottom = frect.top + nLineHeight;

  int nBlockSize = static_cast<int>(blocks.size());
  ASSERT( nActualItem < nBlockSize );

  if( nBlockSize > 0 && nActualItem < nBlockSize - 1 && 
    blocks[nActualItem + 1].m_nCharPos >= nOffset && 
    blocks[nActualItem + 1].m_nCharPos <= nOffset + nCount )
    {
      ASSERT(blocks[nActualItem].m_nCharPos >= 0 &&
         blocks[nActualItem].m_nCharPos <= nLineLength);

      size_t I=0;
      for (I = nActualItem; I < blocks.size() - 1 &&
        blocks[I + 1].m_nCharPos <= nOffset + nCount; I ++)
        {
          const TEXTBLOCK& blk = blocks[I];
          ASSERT(blk.m_nCharPos >= 0 && blk.m_nCharPos <= nLineLength);

          int nOffsetToUse = (nOffset > blk.m_nCharPos) ?
             nOffset : blk.m_nCharPos;
          if (blocks[I + 1].m_nCharPos - nOffsetToUse > 0)
            {
              int nOldActualOffset = nActualOffset;
              DrawLineHelper(ptOrigin, rcClip, blk.m_nColorIndex, blk.m_nBgColorIndex, crText, crBkgnd, nLineIndex,
                (nOffset > blk.m_nCharPos)? nOffset : blk.m_nCharPos, 
                blocks[I + 1].m_nCharPos - nOffsetToUse,
                nActualOffset, CPoint( nOffsetToUse, ptTextPos.y ));
              if (bPrevZeroWidthBlock)
                {
                  CRect rcClipZeroWidthBlock(ptOriginZeroWidthBlock.x, rcClip.top, ptOriginZeroWidthBlock.x + ZEROWIDTHBLOCK_WIDTH, rcClip.bottom);
                  DrawLineHelper(ptOriginZeroWidthBlock, rcClipZeroWidthBlock, blk.m_nColorIndex, nBgColorIndexZeorWidthBlock, crText, crBkgnd, nLineIndex,
                      (nOffset > blk.m_nCharPos)? nOffset : blk.m_nCharPos, 
                      blocks[I + 1].m_nCharPos - nOffsetToUse,
                      nOldActualOffset, CPoint( nOffsetToUse, ptTextPos.y ));
                  bPrevZeroWidthBlock = false;
                }
            }
          else
            {
              if (!bPrevZeroWidthBlock && (blk.m_nCharPos < nOffset + nCount || nOffset + nCount == nLineLength))
                {
                  int nBgColorIndex = blk.m_nBgColorIndex;
                  COLORREF clrBkColor;
                  if (IsInsideSelBlock (CPoint{nOffsetToUse, ptTextPos.y}))
                    clrBkColor = GetColor(COLORINDEX_SELBKGND);
                  else if (crBkgnd == CLR_NONE || nBgColorIndex & COLORINDEX_APPLYFORCE)
                    clrBkColor = GetColor(nBgColorIndex);
                  else
                    clrBkColor = crBkgnd;
                  CRect rc(ptOrigin.x, ptOrigin.y, ptOrigin.x + ZEROWIDTHBLOCK_WIDTH, ptOrigin.y + GetLineHeight());
                  m_pCrystalRenderer->SetBkColor(clrBkColor);
                  m_pCrystalRenderer->FillRectangle(rc);
                  ptOriginZeroWidthBlock = ptOrigin;
                  nBgColorIndexZeorWidthBlock = blk.m_nBgColorIndex;
                  bPrevZeroWidthBlock = true;
                }
            }
          if (ptOrigin.x > rcClip.right)
            {
              if (GetTextLayoutMode () == TEXTLAYOUT_TABLE_WORDWRAP)
                {
                  while (I < blocks.size () - 1 && blocks[I + 1].m_nCharPos <= nOffset + nCount)
                    I++;
                }
              break;
            }
        }

      nActualItem = static_cast<int>(I);

      const TEXTBLOCK& blk = blocks[nActualItem];
      ASSERT(blk.m_nCharPos >= 0 &&
        blk.m_nCharPos <= nLineLength);

      if (nOffset + nCount - blk.m_nCharPos > 0)
        {
          int nOldActualOffset = nActualOffset;
          DrawLineHelper(ptOrigin, rcClip, blk.m_nColorIndex, blk.m_nBgColorIndex,
                  crText, crBkgnd, nLineIndex, blk.m_nCharPos,
                  nOffset + nCount - blk.m_nCharPos,
                  nActualOffset, CPoint(blk.m_nCharPos, ptTextPos.y));
          if (bPrevZeroWidthBlock)
            {
              CRect rcClipZeroWidthBlock(ptOriginZeroWidthBlock.x, rcClip.top, ptOriginZeroWidthBlock.x + ZEROWIDTHBLOCK_WIDTH, rcClip.bottom);
              DrawLineHelper(ptOriginZeroWidthBlock, rcClipZeroWidthBlock, blk.m_nColorIndex, nBgColorIndexZeorWidthBlock,
                  crText, crBkgnd, nLineIndex, blk.m_nCharPos,
                  nOffset + nCount - blk.m_nCharPos,
                  nOldActualOffset, CPoint(blk.m_nCharPos, ptTextPos.y));
              bPrevZeroWidthBlock = false;
            }
        }
      else
        {
          if (!bPrevZeroWidthBlock && (blk.m_nCharPos < nOffset + nCount || nOffset + nCount == nLineLength))
            {
              int nBgColorIndex = blk.m_nBgColorIndex;
              COLORREF clrBkColor;
              if (IsInsideSelBlock (CPoint{blk.m_nCharPos, ptTextPos.y}))
                clrBkColor = GetColor(COLORINDEX_SELBKGND);
              else if (crBkgnd == CLR_NONE || nBgColorIndex & COLORINDEX_APPLYFORCE)
                clrBkColor = GetColor(nBgColorIndex);
              else
                clrBkColor = crBkgnd;
              CRect rc(ptOrigin.x, ptOrigin.y, ptOrigin.x + ZEROWIDTHBLOCK_WIDTH, ptOrigin.y + GetLineHeight());
              m_pCrystalRenderer->SetBkColor(clrBkColor);
              m_pCrystalRenderer->FillRectangle(rc);
              bPrevZeroWidthBlock = true;
            }
        }
    }
  else
    {
      DrawLineHelper(
              ptOrigin, rcClip, blocks[nActualItem].m_nColorIndex, blocks[nActualItem].m_nBgColorIndex,
              crText, crBkgnd, nLineIndex, nOffset, nCount, nActualOffset, ptTextPos);
    }

  // Draw space on the right of the text

  frect.left = ptOrigin.x + (bPrevZeroWidthBlock ? ZEROWIDTHBLOCK_WIDTH : 0);

  if ((m_bFocused || m_bShowInactiveSelection) 
    && !m_bRectangularSelection
    && IsInsideSelBlock(CPoint(nLineLength, ptTextPos.y)) 
    && (nOffset + nCount) == nLineLength )
    {
      if (frect.left >= rcClip.left)
        {
          const int nCharWidth = GetCharWidth();
          CRect rc(frect.left, frect.top, frect.left + nCharWidth, frect.bottom);
          m_pCrystalRenderer->SetBkColor(GetColor(COLORINDEX_SELBKGND));
          m_pCrystalRenderer->FillRectangle(rc);
          frect.left += nCharWidth;
        }
    }
  if (frect.left < rcClip.left)
    frect.left = rcClip.left;

  if (frect.right > frect.left)
    {
      m_pCrystalRenderer->SetBkColor(bDrawWhitespace ? crBkgnd : GetColor(COLORINDEX_WHITESPACE));
      m_pCrystalRenderer->FillRectangle(frect);
    }

  // set origin to beginning of next screen line
  ptOrigin.x = originalOrigin.x;
  ptOrigin.y+= nLineHeight;
}
//END SW

class IntArray : public CArray<int, int>
{
public:
  explicit IntArray(int len) { SetSize(len); }
};

std::vector<TEXTBLOCK> CCrystalTextView::
MergeTextBlocks (const std::vector<TEXTBLOCK>& blocks1, const std::vector<TEXTBLOCK>& blocks2) const
{
  size_t i, j, k;

  std::vector<TEXTBLOCK> mergedBlocks(blocks1.size() + blocks2.size());

  for (i = 0, j = 0, k = 0; ; k++)
    {
      if (i >= blocks1.size() && j >= blocks2.size())
        {
          break;
        }
      else if ((i < blocks1.size()&& j < blocks2.size()) &&
          (blocks1[i].m_nCharPos == blocks2[j].m_nCharPos))
        {
          mergedBlocks[k].m_nCharPos = blocks2[j].m_nCharPos;
          if (blocks2[j].m_nColorIndex == COLORINDEX_NONE)
            mergedBlocks[k].m_nColorIndex = blocks1[i].m_nColorIndex;
          else
            mergedBlocks[k].m_nColorIndex = blocks2[j].m_nColorIndex;
          if (blocks2[j].m_nBgColorIndex == COLORINDEX_NONE)
            mergedBlocks[k].m_nBgColorIndex = blocks1[i].m_nBgColorIndex;
          else
            mergedBlocks[k].m_nBgColorIndex = blocks2[j].m_nBgColorIndex;
          i++;
          j++;
        }
      else if (j >= blocks2.size() || (i < blocks1.size() &&
          blocks1[i].m_nCharPos < blocks2[j].m_nCharPos))
        {
          mergedBlocks[k].m_nCharPos = blocks1[i].m_nCharPos;
          if (blocks2.size() == 0 || blocks2[j - 1].m_nColorIndex == COLORINDEX_NONE)
            mergedBlocks[k].m_nColorIndex = blocks1[i].m_nColorIndex;
          else
            mergedBlocks[k].m_nColorIndex = blocks2[j - 1].m_nColorIndex;
          if (blocks2.size() == 0 || blocks2[j - 1].m_nBgColorIndex == COLORINDEX_NONE)
            mergedBlocks[k].m_nBgColorIndex = blocks1[i].m_nBgColorIndex;
          else
            mergedBlocks[k].m_nBgColorIndex = blocks2[j - 1].m_nBgColorIndex;
          i++;
        }
      else if (i >= blocks1.size() || (j < blocks2.size() && blocks1[i].m_nCharPos > blocks2[j].m_nCharPos))
        {
          mergedBlocks[k].m_nCharPos = blocks2[j].m_nCharPos;
          if (i > 0 && blocks2[j].m_nColorIndex == COLORINDEX_NONE)
            mergedBlocks[k].m_nColorIndex = blocks1[i - 1].m_nColorIndex;
          else
            mergedBlocks[k].m_nColorIndex = blocks2[j].m_nColorIndex;
          if (i > 0 && blocks2[j].m_nBgColorIndex == COLORINDEX_NONE)
            mergedBlocks[k].m_nBgColorIndex = blocks1[i - 1].m_nBgColorIndex;
          else
            mergedBlocks[k].m_nBgColorIndex = blocks2[j].m_nBgColorIndex;
          j++;
        }
    }

  j = 0;
  for (i = 0; i < k; ++i)
    {
      if (i == 0 ||
          (mergedBlocks[i - 1].m_nColorIndex   != mergedBlocks[i].m_nColorIndex ||
           mergedBlocks[i - 1].m_nBgColorIndex != mergedBlocks[i].m_nBgColorIndex))
        {
          mergedBlocks[j] = mergedBlocks[i];
          ++j;
        }
    }

  mergedBlocks.resize(j);
  return mergedBlocks;
}

std::vector<TEXTBLOCK>
CCrystalTextView::GetMarkerTextBlocks(int nLineIndex) const
{
  std::vector<TEXTBLOCK> allblocks;
  if (!m_pMarkers->GetEnabled())
    return allblocks;

  int nLength = GetViewableLineLength (nLineIndex);

  for (const auto& marker : m_pMarkers->GetMarkers())
    {
      if (!marker.second.bVisible)
        continue;
      int nBlocks = 0;
      std::vector<TEXTBLOCK> blocks((nLength + 1) * 3); // be aware of nLength == 0
      blocks[0].m_nCharPos = 0;
      blocks[0].m_nColorIndex = COLORINDEX_NONE;
      blocks[0].m_nBgColorIndex = COLORINDEX_NONE;
      ++nBlocks;
      const TCHAR *pszChars = GetLineChars(nLineIndex);
      int nLineLength = GetLineLength(nLineIndex);
      if (pszChars != nullptr)
        {
          RxNode *node = nullptr;
          for (const TCHAR *p = pszChars; p < pszChars + nLineLength; )
            {
              RxMatchRes matches;
              int nMatchLen = 0;
              size_t nPos = ::FindStringHelper(pszChars, nLineLength, p, marker.second.sFindWhat, marker.second.dwFlags | FIND_NO_WRAP, nMatchLen, node, &matches);
              if (nPos == -1)
                break;
              if (nLineLength < static_cast<int>((p - pszChars) + nPos) + nMatchLen)
                nMatchLen = static_cast<int>(nLineLength - (p - pszChars));
              ASSERT(((p - pszChars) + nPos) < INT_MAX);
              blocks[nBlocks].m_nCharPos = static_cast<int>((p - pszChars) + nPos);
              blocks[nBlocks].m_nBgColorIndex = marker.second.nBgColorIndex | COLORINDEX_APPLYFORCE;
              blocks[nBlocks].m_nColorIndex = COLORINDEX_NONE;
              ++nBlocks;
              ASSERT(((p - pszChars) + nPos + nMatchLen) < INT_MAX);
              blocks[nBlocks].m_nCharPos = static_cast<int>((p - pszChars) + nPos + nMatchLen);
              blocks[nBlocks].m_nBgColorIndex = COLORINDEX_NONE;
              blocks[nBlocks].m_nColorIndex = COLORINDEX_NONE;
              ++nBlocks;
              p += nPos + (nMatchLen == 0 ? 1 : nMatchLen);
            }
          RxFree (node);
          blocks.resize(nBlocks);
          allblocks = MergeTextBlocks(allblocks, blocks);
        }
    }

  return allblocks;
}

std::vector<TEXTBLOCK>
CCrystalTextView::GetTextBlocks(int nLineIndex)
{
  int nLength = GetViewableLineLength (nLineIndex);

  //  Parse the line
  DWORD dwCookie = GetParseCookie(nLineIndex - 1);
  std::vector<TEXTBLOCK> blocks((nLength + 1) * 3); // be aware of nLength == 0
  int nBlocks = 0;
  // insert at least one textblock of normal color at the beginning
  blocks[0].m_nCharPos = 0;
  blocks[0].m_nColorIndex = COLORINDEX_NORMALTEXT;
  blocks[0].m_nBgColorIndex = COLORINDEX_BKGND;
  nBlocks++;
  (*m_ParseCookies)[nLineIndex] = ParseLine(dwCookie, GetLineChars(nLineIndex), GetLineLength(nLineIndex), blocks.data(), nBlocks);
  ASSERT((*m_ParseCookies)[nLineIndex] != -1);
  blocks.resize(nBlocks);
  
  return MergeTextBlocks(blocks, 
          (m_pMarkers && m_pMarkers->GetEnabled() && m_pMarkers->GetMarkers().size() > 0) ?
          MergeTextBlocks(GetAdditionalTextBlocks(nLineIndex), GetMarkerTextBlocks(nLineIndex)) :
          GetAdditionalTextBlocks(nLineIndex));
}

void CCrystalTextView::
DrawSingleLine (const CRect & rc, int nLineIndex)
{
  const int nCharWidth = GetCharWidth();
  ASSERT (nLineIndex >= -1 && nLineIndex < GetLineCount ());

  if (nLineIndex == -1)
    {
      //  Draw line beyond the text
      m_pCrystalRenderer->FillSolidRectangle (rc, GetColor (COLORINDEX_WHITESPACE));
      return;
    }

  //  Acquire the background color for the current line
  bool bDrawWhitespace = false;
  COLORREF crBkgnd, crText;
  GetLineColors (nLineIndex, crBkgnd, crText, bDrawWhitespace);

  int nLength = GetViewableLineLength (nLineIndex);

  std::vector<TEXTBLOCK> blocks = GetTextBlocks(nLineIndex);

  int nActualItem = 0;
  int nActualOffset = 0;
  // Wrap the line
  std::vector<int> anBreaks(nLength);
  int nBreaks = 0;

  WrapLineCached( nLineIndex, GetScreenChars(), &anBreaks, nBreaks );

  //  Draw the line text
  CPoint origin (rc.left - m_nOffsetChar * nCharWidth, rc.top);
  if (crBkgnd != CLR_NONE)
    m_pCrystalRenderer->SetBkColor (crBkgnd);
  if (crText != CLR_NONE)
    m_pCrystalRenderer->SetTextColor (crText);

  const TextLayoutMode layoutMode = GetTextLayoutMode ();
  if (layoutMode == TEXTLAYOUT_TABLE_WORDWRAP)
    {
      anBreaks.push_back (-nLength);
      CPoint originOrg = origin;
      DrawScreenLine(
        origin, rc,
        blocks, nActualItem,
        crText, crBkgnd, bDrawWhitespace,
        nLineIndex, 0, abs(anBreaks[0]),
        nActualOffset, CPoint( 0, nLineIndex ) );
      int nColumn = 0;
      for( int i = 0, j = 0; i < static_cast<int> (anBreaks.size ()) - 1; i++, j++ )
        {
          if (anBreaks[i] < 0)
            {
              if (j < nBreaks)
                {
                  CRect frect( origin.x, originOrg.y + (j + 1) * GetLineHeight (),
                    origin.x + m_pTextBuffer->GetColumnWidth (nColumn) * nCharWidth, rc.bottom );
                  if (frect.left < rc.left)
                    frect.left = rc.left;
                  if (frect.right > rc.left)
                    m_pCrystalRenderer->FillSolidRectangle (frect, crBkgnd == CLR_NONE ? GetColor(COLORINDEX_WHITESPACE) : crBkgnd);
                }
              origin.y = originOrg.y;
              origin.x += m_pTextBuffer->GetColumnWidth (nColumn++) * nCharWidth;
              j = -1;
            }
          DrawScreenLine(
            origin, rc,
            blocks, nActualItem,
            crText, crBkgnd, bDrawWhitespace,
            nLineIndex, abs(anBreaks[i]), abs(anBreaks[i + 1]) - abs(anBreaks[i]),
            nActualOffset, CPoint( abs(anBreaks[i]), nLineIndex ) );
        }
    }
  else if (layoutMode == TEXTLAYOUT_WORDWRAP && nBreaks > 0)
    {
      // Draw all the screen lines of the wrapped line
      ASSERT( anBreaks[0] < nLength );

      // draw start of line to first break
      DrawScreenLine(
        origin, rc,
        blocks, nActualItem,
        crText, crBkgnd, bDrawWhitespace,
        nLineIndex, 0, anBreaks[0], nActualOffset, CPoint( 0, nLineIndex ) );

      // draw from first break to last break
      int i=0;
      for( i = 0; i < nBreaks - 1; i++ )
        {
          ASSERT( anBreaks[i] >= 0 && anBreaks[i] < nLength );
          DrawScreenLine(
            origin, rc,
            blocks, nActualItem,
            crText, crBkgnd, bDrawWhitespace,
            nLineIndex, anBreaks[i], anBreaks[i + 1] - anBreaks[i],
            nActualOffset, CPoint( anBreaks[i], nLineIndex ) );
        }

      // draw from last break till end of line
      DrawScreenLine(
        origin, rc,
        blocks, nActualItem,
        crText, crBkgnd, bDrawWhitespace,
        nLineIndex, anBreaks[i], nLength - anBreaks[i],
        nActualOffset, CPoint( anBreaks[i], nLineIndex ) );
    }
  else
    DrawScreenLine(
      origin, rc,
      blocks, nActualItem,
      crText, crBkgnd, bDrawWhitespace,
      nLineIndex, 0, nLength, nActualOffset, CPoint(0, nLineIndex));

  // Draw empty sublines
  int nEmptySubLines = GetEmptySubLines(nLineIndex);
  if (nEmptySubLines > 0)
    {
      CRect frect = rc;
      frect.top = frect.bottom - nEmptySubLines * GetLineHeight();
      m_pCrystalRenderer->FillSolidRectangle(frect, crBkgnd == CLR_NONE ? GetColor(COLORINDEX_WHITESPACE) : crBkgnd);
    }
}

/**
 * @brief Escape special characters
 * @param [in]      strText The text to escape
 * @param [in, out] bLastCharSpace Whether last char processed was white space
 * @param [in, out] nNonbreakChars The number of non-break characters in the text
 * @param [in]      nScreenChars   The maximum number of characters to display per line
 * @return The escaped text
 */
static CString
EscapeHTML (const CString & strText, bool & bLastCharSpace, int & nNonbreakChars, int nScreenChars)
{
  CString strHTML;
  int len = strText.GetLength ();
  for (int i = 0; i < len; ++i)
    {
      TCHAR ch = strText[i];
      switch (ch)
        {
          case '&':
            strHTML += _T("&amp;");
            bLastCharSpace = false;
            nNonbreakChars++;
            break;
          case '<':
            strHTML += _T("&lt;");
            bLastCharSpace = false;
            nNonbreakChars++;
            break;
          case '>':
            strHTML += _T("&gt;");
            bLastCharSpace = false;
            nNonbreakChars++;
            break;
          case 0xB7:
          case 0xBB:
            strHTML += ch;
            strHTML += _T("<wbr>");
            bLastCharSpace = false;
            nNonbreakChars = 0;
            break;
          case ' ':
            if (i == 0 || bLastCharSpace)
              {
                strHTML += _T("&nbsp;");
                bLastCharSpace = false;
              }
            else
              {
                strHTML += _T(" ");
                bLastCharSpace = true;
              }
            nNonbreakChars = 0;
            break;
          default:
            strHTML += ch;
            bLastCharSpace = false;
            nNonbreakChars++;
        }
#ifndef _UNICODE
      if (IsDBCSLeadByte (ch))
        strHTML += strText[++i];
#endif
      if ((nNonbreakChars % nScreenChars) == nScreenChars - 1)
        {
          strHTML += _T("<wbr>");
          nNonbreakChars = 0;
        }
    }

  return strHTML;
}

// Make a CString from printf-style args (single call version of CString::Format)
static CString Fmt(LPCTSTR fmt, ...)
{
  CString str;
  va_list args;
  va_start(args, fmt);
  str.FormatV(fmt, args);
  va_end(args);
  return str;
}

/**
 * @brief Return all the styles necessary to render this view as HTML code
 * @return The HTML styles
 */
CString CCrystalTextView::
GetHTMLStyles ()
{
  int arColorIndices[] = {
    COLORINDEX_NORMALTEXT,
    COLORINDEX_SELTEXT,
    COLORINDEX_KEYWORD,
    COLORINDEX_FUNCNAME,
    COLORINDEX_COMMENT,
    COLORINDEX_NUMBER,
    COLORINDEX_OPERATOR,
    COLORINDEX_STRING,
    COLORINDEX_PREPROCESSOR,
    COLORINDEX_HIGHLIGHTTEXT1,
    COLORINDEX_HIGHLIGHTTEXT2,
    COLORINDEX_USER1,
    COLORINDEX_USER2,
  };
  int arBgColorIndices[] = {
    COLORINDEX_BKGND,
    COLORINDEX_SELBKGND,
    COLORINDEX_HIGHLIGHTBKGND1,
    COLORINDEX_HIGHLIGHTBKGND2,
    COLORINDEX_HIGHLIGHTBKGND3,
    COLORINDEX_HIGHLIGHTBKGND4,
  };

  CString strStyles;
  for (int f = 0; f < sizeof(arColorIndices)/sizeof(int); f++)
    {
      int nColorIndex = arColorIndices[f];
      for (int b = 0; b < sizeof(arBgColorIndices)/sizeof(int); b++)
        {
          int nBgColorIndex = arBgColorIndices[b];
          COLORREF clr;

          strStyles += Fmt (_T(".sf%db%d {"), nColorIndex, nBgColorIndex);
          clr = GetColor (nColorIndex);
          strStyles += Fmt (_T("color: #%02x%02x%02x; "), GetRValue (clr), GetGValue (clr), GetBValue (clr));
          clr = GetColor (nBgColorIndex);
          strStyles += Fmt (_T("background-color: #%02x%02x%02x; "), GetRValue (clr), GetGValue (clr), GetBValue (clr));
          if (GetBold (nColorIndex))
            strStyles += _T("font-weight: bold; ");
          if (GetItalic (nColorIndex))
            strStyles += _T("font-style: italic; ");
          strStyles += _T("}\n");
        }
    }
  COLORREF clrSelMargin = GetColor(COLORINDEX_SELMARGIN);
  COLORREF clrNormalText = GetColor(COLORINDEX_NORMALTEXT);
  strStyles += Fmt(_T(".ln {text-align: right; word-break: normal; color: #%02x%02x%02x; background-color: #%02x%02x%02x;}\n"),
    GetRValue(clrNormalText), GetGValue(clrNormalText), GetBValue(clrNormalText),
    GetRValue(clrSelMargin), GetGValue(clrSelMargin), GetBValue(clrSelMargin));
  return strStyles;
}

/**
 * @brief Return the HTML attribute associated with the specified colors
 * @param [in] nColorIndex   Index of text color
 * @param [in] nBgColorIndex Index of background color
 * @param [in] crText        Base text color
 * @param [in] crBkgnd       Base background color
 * @return The HTML attribute
 */
CString CCrystalTextView::
GetHTMLAttribute (int nColorIndex, int nBgColorIndex, COLORREF crText, COLORREF crBkgnd)
{
  CString strAttr;
  COLORREF clr;

  if ((crText == CLR_NONE || (nColorIndex & COLORINDEX_APPLYFORCE)) && 
      (crBkgnd == CLR_NONE || (nBgColorIndex & COLORINDEX_APPLYFORCE)))
    return Fmt(_T("class=\"sf%db%d\""), nColorIndex & ~COLORINDEX_APPLYFORCE, nBgColorIndex & ~COLORINDEX_APPLYFORCE);

  if (crText == CLR_NONE || (nColorIndex & COLORINDEX_APPLYFORCE))
    clr = GetColor (nColorIndex);
  else
    clr = crText;
  strAttr += Fmt (_T("style=\"color: #%02x%02x%02x; "), GetRValue (clr), GetGValue (clr), GetBValue (clr));

  if (crBkgnd == CLR_NONE || (nBgColorIndex & COLORINDEX_APPLYFORCE))
    clr = GetColor (nBgColorIndex);
  else
    clr = crBkgnd;
  strAttr += Fmt (_T("background-color: #%02x%02x%02x; "), GetRValue (clr), GetGValue (clr), GetBValue (clr));

  if (GetBold (nColorIndex))
    strAttr += _T("font-weight: bold; ");
  if (GetItalic (nColorIndex))
    strAttr += _T("font-style: italic; ");

  strAttr += _T("\"");

  return strAttr;
}

/**
 * @brief Retrieve the html version of the line
 * @param [in] nLineIndex  Index of line in view
 * @param [in] pszTag      The HTML tag to enclose the line
 * @return The html version of the line
 */
CString CCrystalTextView::
GetHTMLLine (int nLineIndex, LPCTSTR pszTag)
{
  ASSERT (nLineIndex >= -1 && nLineIndex < GetLineCount ());

  int nLength = GetViewableLineLength (nLineIndex);

  //  Acquire the background color for the current line
  bool bDrawWhitespace = false;
  COLORREF crBkgnd, crText;
  GetLineColors (nLineIndex, crBkgnd, crText, bDrawWhitespace);

  std::vector<TEXTBLOCK> blocks = GetTextBlocks(nLineIndex);

  CString strHTML;
  CString strExpanded;
  size_t i;
  int nNonbreakChars = 0;
  bool bLastCharSpace = false;
  const int nScreenChars = 40; //  GetScreenChars();

  strHTML += _T("<");
  strHTML += pszTag;
  strHTML += _T(" ");
  strHTML += GetHTMLAttribute (COLORINDEX_NORMALTEXT, COLORINDEX_BKGND, crText, crBkgnd);
  strHTML += _T("><code>");

  auto MakeSpan = [&](const TEXTBLOCK& block, const CString& strExpanded) {
    CString strHTML;
    strHTML += _T("<span ");
    strHTML += GetHTMLAttribute (block.m_nColorIndex, block.m_nBgColorIndex, crText, crBkgnd);
    strHTML += _T(">");
    strHTML += EscapeHTML (strExpanded, bLastCharSpace, nNonbreakChars, nScreenChars);
    strHTML += _T("</span>");
    return strHTML;
  };

  for (i = 0; i < blocks.size() - 1; i++)
    {
      ExpandChars (nLineIndex, blocks[i].m_nCharPos, blocks[i + 1].m_nCharPos - blocks[i].m_nCharPos, strExpanded, 0);
      if (!strExpanded.IsEmpty())
        strHTML += MakeSpan(blocks[i], strExpanded);
    }
  if (blocks.size() > 0)
    {
      ExpandChars (nLineIndex, blocks[i].m_nCharPos, nLength - blocks[i].m_nCharPos, strExpanded, 0);
      if (!strExpanded.IsEmpty())
        strHTML += MakeSpan(blocks[i], strExpanded);
      if (strExpanded.Compare (CString (' ', strExpanded.GetLength())) == 0)
        strHTML += _T("&nbsp;");
    }
  strHTML += _T("</code></");
  strHTML += pszTag;
  strHTML += _T(">");

  return strHTML;
}

COLORREF CCrystalTextView::
GetColor (int nColorIndex)
{
  if (m_pColors != nullptr)
    {
      nColorIndex &= ~COLORINDEX_APPLYFORCE;
      return m_pColors->GetColor(nColorIndex);
    }
  else
    return RGB(0, 0, 0);
}

DWORD CCrystalTextView::
GetLineFlags (int nLineIndex) const
{
  if (m_pTextBuffer == nullptr)
    return 0;
  return m_pTextBuffer->GetLineFlags (nLineIndex);
}

void CCrystalTextView::
DrawTopMargin (const CRect& rect)
{
  if (!m_bTopMargin)
    return;

  auto getColumnName = [](int nColumn) -> CString
    {
      CString columnName;
      for (int i = 0; ; ++i)
        {
          TCHAR c = 'A' + (nColumn % 26) - (i == 0 ? 0 : 1);
          columnName.Insert (0, c);
          nColumn /= 26;
          if (nColumn == 0)
            break;
        }
      return columnName;
    };

  m_pCrystalRenderer->SetBkColor (GetColor (COLORINDEX_SELMARGIN));
  m_pCrystalRenderer->FillRectangle (rect);
  m_pCrystalRenderer->SetTextColor (GetColor (COLORINDEX_NORMALTEXT));
  if (m_pTextBuffer->GetTableEditing ())
    {
      const int nCharWidth = GetCharWidth ();
      const int nMarginWidth = GetMarginWidth ();
      CString columnNames;
      for (int nColumn = 0, x = nMarginWidth - m_nOffsetChar * nCharWidth; x < rect.Width (); ++nColumn)
        {
          int nColumnWidth = m_pTextBuffer->GetColumnWidth (nColumn);
          CString columnName = getColumnName (nColumn);
          int columnNameLen = columnName.GetLength ();
          if (nColumnWidth < columnNameLen)
            columnNames += columnName.Right (nColumnWidth);
          else
            {
              int leftspaces = (nColumnWidth - columnNameLen) / 2;
              columnNames += CString (' ', leftspaces) + columnName + CString (' ', nColumnWidth - leftspaces - columnNameLen);
            }
          x += nColumnWidth * nCharWidth;
        }
      columnNames = columnNames.Mid (m_nOffsetChar).Left(rect.Width () / nCharWidth + 1);

      std::vector<int> nWidths (columnNames.GetLength (), nCharWidth);
      m_pCrystalRenderer->SwitchFont (false, false);
      m_pCrystalRenderer->DrawText (nMarginWidth, 0, rect, columnNames, columnNames.GetLength (), nWidths.data ());
    }
  else
    m_pCrystalRenderer->DrawRuler (GetMarginWidth (), 0, rect.Width (), rect.Height (), GetCharWidth (), m_nOffsetChar);
}

/**
 * @brief Draw selection margin.
 * @param [in] pdc         Pointer to draw context.
 * @param [in] rect        The rectangle to draw.
 * @param [in] nLineIndex  Index of line in view.
 * @param [in] nLineNumber Line number to display. if -1, it's not displayed.
 */
void CCrystalTextView::
DrawMargin (const CRect & rect, int nLineIndex, int nLineNumber)
{
  if (!m_bSelMargin && !m_bViewLineNumbers)
    m_pCrystalRenderer->SetBkColor(GetColor (COLORINDEX_BKGND));
  else
    m_pCrystalRenderer->SetBkColor(GetColor (COLORINDEX_SELMARGIN));
  m_pCrystalRenderer->FillRectangle(rect);

  if (m_bViewLineNumbers && nLineNumber > 0)
    {
      m_pCrystalRenderer->SetTextColor(GetColor(COLORINDEX_NORMALTEXT));
      m_pCrystalRenderer->DrawMarginLineNumber(rect.right, rect.top, nLineNumber);
    }

  // Draw line revision mark (or background) whenever we have valid lineindex
  COLORREF clrRevisionMark = GetColor(COLORINDEX_WHITESPACE);
  if (nLineIndex >= 0 && m_pTextBuffer != nullptr)
    {
      // get line revision marks color
      DWORD dwRevisionNumber = m_pTextBuffer->GetLineRevisionNumber(nLineIndex);
      if (dwRevisionNumber > 0)
        {
          if (m_pTextBuffer->m_dwRevisionNumberOnSave < dwRevisionNumber)
            clrRevisionMark = UNSAVED_REVMARK_CLR;
          else
            clrRevisionMark = SAVED_REVMARK_CLR;
        }
    }

  // draw line revision marks
  CRect rc(rect.right - MARGIN_REV_WIDTH, rect.top, rect.right, rect.bottom);
  m_pCrystalRenderer->FillSolidRectangle (rc, clrRevisionMark);

  if (!m_bSelMargin)
    return;

  int nImageIndex = -1;
  if (nLineIndex >= 0)
    {
      DWORD dwLineFlags = GetLineFlags (nLineIndex);
      static const DWORD adwFlags[] =
        {
          LF_EXECUTION,
          LF_BREAKPOINT,
          LF_COMPILATION_ERROR,
          LF_BOOKMARK (1),
          LF_BOOKMARK (2),
          LF_BOOKMARK (3),
          LF_BOOKMARK (4),
          LF_BOOKMARK (5),
          LF_BOOKMARK (6),
          LF_BOOKMARK (7),
          LF_BOOKMARK (8),
          LF_BOOKMARK (9),
          LF_BOOKMARK (0),
          LF_BOOKMARKS,
          LF_INVALID_BREAKPOINT
        };
      for (int I = 0; I < sizeof (adwFlags) / sizeof (adwFlags[0]); I++)
        {
          if ((dwLineFlags & adwFlags[I]) != 0)
            {
              nImageIndex = I;
              break;
            }
        }
    }
  if (nImageIndex >= 0)
    {
      const int iconsize = GetMarginIconSize();
      m_pCrystalRenderer->DrawMarginIcon(
        rect.left + 2, rect.top + (GetLineHeight() - iconsize) / 2, nImageIndex, iconsize);
    }

  // draw wrapped-line-icon
  if (nLineNumber > 0)
    {
      const int iconsize = GetMarginIconSize();
      int nBreaks = 0;
      WrapLineCached( nLineIndex, GetScreenChars(), nullptr, nBreaks );
      for (int i = 0; i < nBreaks; i++)
        {
          m_pCrystalRenderer->DrawMarginIcon(
              rect.right - iconsize, rect.top + (GetLineHeight()
              - iconsize) / 2 + (i+1) * GetLineHeight(), ICON_INDEX_WRAPLINE, iconsize);
        }
    }
}

bool CCrystalTextView::
IsInsideSelBlock (CPoint ptTextPos)
{
  PrepareSelBounds();
  ASSERT_VALIDTEXTPOS (ptTextPos);
  if (ptTextPos.y < m_ptDrawSelStart.y)
    return false;
  if (ptTextPos.y > m_ptDrawSelEnd.y)
    return false;
  if (m_bRectangularSelection)
    return ptTextPos.x >= m_ptDrawSelStart.x && ptTextPos.x < m_ptDrawSelEnd.x;
  if (ptTextPos.y < m_ptDrawSelEnd.y && ptTextPos.y > m_ptDrawSelStart.y)
    return true;
  if (m_ptDrawSelStart.y < m_ptDrawSelEnd.y)
    {
      if (ptTextPos.y == m_ptDrawSelEnd.y)
        return ptTextPos.x < m_ptDrawSelEnd.x;
      ASSERT (ptTextPos.y == m_ptDrawSelStart.y);
      return ptTextPos.x >= m_ptDrawSelStart.x;
    }
  ASSERT (m_ptDrawSelStart.y == m_ptDrawSelEnd.y);
  return ptTextPos.x >= m_ptDrawSelStart.x && ptTextPos.x < m_ptDrawSelEnd.x;
}

bool CCrystalTextView::
IsInsideSelection (const CPoint & ptTextPos)
{
  PrepareSelBounds ();
  return IsInsideSelBlock (ptTextPos);
}

/**
 * @brief : class the selection extremities in ascending order
 *
 * @note : Updates m_ptDrawSelStart and m_ptDrawSelEnd
 * This function must be called before reading these values
 */
void CCrystalTextView::
PrepareSelBounds ()
{
  if (m_ptSelStart.y < m_ptSelEnd.y ||
        (m_ptSelStart.y == m_ptSelEnd.y && m_ptSelStart.x < m_ptSelEnd.x))
    {
      m_ptDrawSelStart = m_ptSelStart;
      m_ptDrawSelEnd = m_ptSelEnd;
    }
  else
    {
      m_ptDrawSelStart = m_ptSelEnd;
      m_ptDrawSelEnd = m_ptSelStart;
    }
}

void CCrystalTextView::
OnDraw (CDC * pdc)
{
  // We use the same GDI objects for both drawing and printing.
  // So when printing is in progress, we do nothing in this function.
  if (m_bPrinting)
    return;

  CRect rcClient;
  GetClientRect (rcClient);

  if (m_pTextBuffer == nullptr)
    {
      m_pCrystalRenderer->BindDC(*pdc, rcClient);
      m_pCrystalRenderer->BeginDraw();
      m_pCrystalRenderer->SetBkColor(GetSysColor(COLOR_WINDOW));
      m_pCrystalRenderer->FillRectangle(rcClient);
      m_pCrystalRenderer->EndDraw();
      return;
    }

  const int nLineCount = GetLineCount ();
  const int nLineHeight = GetLineHeight ();
  PrepareSelBounds ();

  // if the private arrays (m_ParseCookies and m_pnActualLineLength) 
  // are defined, check they are in phase with the text buffer
  if (m_ParseCookies->size())
    ASSERT(m_ParseCookies->size() == static_cast<size_t>(nLineCount));
  if (m_pnActualLineLength->size())
    ASSERT(m_pnActualLineLength->size() == static_cast<size_t>(nLineCount));

  CDC cacheDC;
  VERIFY (cacheDC.CreateCompatibleDC (pdc));
  if (m_pCacheBitmap == nullptr)
    {
      m_pCacheBitmap = new CBitmap;
      VERIFY(m_pCacheBitmap->CreateCompatibleBitmap(pdc, rcClient.Width(), rcClient.Height()));
    }
  CBitmap *pOldBitmap = cacheDC.SelectObject (m_pCacheBitmap);

  // initialize rects
  int nSubLineOffset = GetSubLineIndex( m_nTopLine ) - m_nTopSubLine;
  int nCursorY = TextToClient (m_ptCursorPos).y;

  CRect rcLine;
  CRect rcTopMargin(rcClient.left, rcClient.top, rcClient.right, rcClient.top + GetTopMarginHeight());
  rcLine = rcClient;
  rcLine.top = rcTopMargin.bottom + nSubLineOffset * nLineHeight;
  CRect rcMargin (rcLine.left, rcLine.top, rcLine.left + GetMarginWidth (), rcLine.top + nLineHeight);
  rcLine.left = rcMargin.right;

  m_pCrystalRenderer->BindDC(cacheDC, rcClient);
  m_pCrystalRenderer->BeginDraw();

  int nLastLineBottom = 0;
  int nCurrentLine = m_nTopLine;
  while (rcLine.top < rcClient.bottom)
    {
      int nSubLines = 1;
      if( nCurrentLine < nLineCount /*&& GetLineLength( nCurrentLine ) > nMaxLineChars*/ )
        nSubLines = GetSubLines(nCurrentLine);

      rcLine.bottom = rcLine.top + nSubLines * nLineHeight;
      rcMargin.bottom = rcLine.bottom;

      CRect rcMarginAndLine(rcClient.left, rcLine.top, rcClient.right, rcLine.bottom);
      if (pdc->RectVisible(rcMarginAndLine))
        {
          if (nCurrentLine < nLineCount && GetLineVisible (nCurrentLine))
            {
              DrawMargin (rcMargin, nCurrentLine, nCurrentLine + 1);
              DrawSingleLine (rcLine, nCurrentLine);
              if (nCurrentLine+1 < nLineCount && !GetLineVisible (nCurrentLine + 1))
                m_pCrystalRenderer->DrawBoundaryLine (rcMargin.left, rcLine.right, rcMargin.bottom-1);
              if (m_pTextBuffer->GetTableEditing ())
                m_pCrystalRenderer->DrawGridLine (rcMargin.left, rcMargin.bottom-1, rcLine.right, rcMargin.bottom-1);
              if (nCurrentLine == m_ptCursorPos.y)
                m_pCrystalRenderer->DrawLineCursor (rcMargin.left, rcLine.right, 
                  nCursorY + nLineHeight - 1, 1);
              nLastLineBottom = rcMargin.bottom;
            }
          else
            {
              DrawMargin (rcMargin, -1, -1);
              DrawSingleLine (rcLine, -1);
            }
        }

      nCurrentLine++;
      rcLine.top = rcLine.bottom;
      rcMargin.top = rcLine.top;
    }

  if (pdc->RectVisible (rcTopMargin))
    DrawTopMargin (rcTopMargin);

  if (m_pTextBuffer->GetTableEditing ())
    {
      int nCharWidth = GetCharWidth ();
      int nMarginWidth = GetMarginWidth ();
      for (int nColumn = 0, x = nMarginWidth - m_nOffsetChar * nCharWidth;
           x < rcClient.Width();
           x += m_pTextBuffer->GetColumnWidth (nColumn++) * nCharWidth)
        {
          if (x >= nMarginWidth && nColumn > 0)
            m_pCrystalRenderer->DrawGridLine (x, rcClient.top, x, nLastLineBottom);
        }
    }

  m_pCrystalRenderer->EndDraw();

  VERIFY (pdc->BitBlt (rcClient.left, rcClient.top, rcClient.Width (),
          rcClient.Height (), &cacheDC, 0, 0, SRCCOPY));

  cacheDC.SelectObject (pOldBitmap);
  cacheDC.DeleteDC ();
}

void CCrystalTextView::
ResetView ()
{
  // m_bWordWrap = false;
  m_nTopLine = 0;
  m_nTopSubLine = 0;
  m_nOffsetChar = 0;
  m_nLineHeight = -1;
  m_nCharWidth = -1;
  m_nScreenLines = -1;
  m_nScreenChars = -1;
  m_nIdealCharPos = -1;
  m_ptAnchor.x = 0;
  m_ptAnchor.y = 0;
  InvalidateLineCache( 0, -1 );
  m_ParseCookies->clear();
  m_pnActualLineLength->clear();
  m_ptCursorPos.x = 0;
  m_ptCursorPos.y = 0;
  m_ptSelStart = m_ptSelEnd = m_ptCursorPos;
  if (m_bDragSelection)
    {
      ReleaseCapture ();
      KillTimer (m_nDragSelTimer);
    }
  m_bDragSelection = false;
  m_bVertScrollBarLocked = false;
  m_bHorzScrollBarLocked = false;
  if (::IsWindow (m_hWnd))
    UpdateCaret ();
  m_bShowInactiveSelection = true; // FP: reverted because I like it
  m_bPrintHeader = false;
  m_bPrintFooter = true;

  m_bMultipleSearch = false;    // More search

}

void CCrystalTextView::
UpdateCaret ()
{
  ASSERT_VALIDTEXTPOS (m_ptCursorPos);
  if (m_bFocused && !m_bCursorHidden &&
        CalculateActualOffset (m_ptCursorPos.y, m_ptCursorPos.x) >= m_nOffsetChar)
    {
      int nCaretHeight = GetLineVisible(m_ptCursorPos.y) ? GetLineHeight () : 0;
      if (m_bOverrideCaret)  //UPDATE
        CreateSolidCaret(GetCharWidth(), nCaretHeight);
      else
        CreateSolidCaret (2, nCaretHeight);

      SetCaretPos (TextToClient (m_ptCursorPos));
      ShowCaret ();
      UpdateCompositionWindowPos(); /* IME */
    }
  else
    {
      HideCaret ();
    }
  OnUpdateCaret();
}

void CCrystalTextView::
OnUpdateCaret()
{
}

int CCrystalTextView::
GetCRLFMode ()
{
  if (m_pTextBuffer != nullptr)
    {
      return m_pTextBuffer->GetCRLFMode ();
    }
  return -1;
}

void CCrystalTextView::
SetCRLFMode (CRLFSTYLE nCRLFMode)
{
  if (m_pTextBuffer != nullptr)
    {
      m_pTextBuffer->SetCRLFMode (nCRLFMode);
    }
}

int CCrystalTextView::
GetTabSize ()
{
  if (m_pTextBuffer == nullptr)
    return 4;

  return m_pTextBuffer->GetTabSize();

}

void CCrystalTextView::
SetTabSize (int nTabSize)
{
  ASSERT (nTabSize >= 0 && nTabSize <= 64);
  if (m_pTextBuffer == nullptr)
    return;

  if (m_pTextBuffer->GetTabSize() != nTabSize)
    {
      m_pTextBuffer->SetTabSize( nTabSize );

      m_pnActualLineLength->clear();
      RecalcHorzScrollBar ();
      Invalidate ();
      UpdateCaret ();
    }
}

void CCrystalTextView::
CalcLineCharDim ()
{
  CSize szCharExt = m_pCrystalRenderer->GetCharWidthHeight();
  m_nLineHeight = szCharExt.cy;
  if (m_nLineHeight < 1)
    m_nLineHeight = 1;
  m_nCharWidth = szCharExt.cx;
}

int CCrystalTextView::
GetLineHeight ()
{
  if (m_nLineHeight == -1)
    CalcLineCharDim ();
  return m_nLineHeight;
}

int CCrystalTextView::GetSubLines( int nLineIndex )
{
  // get a number of lines this wrapped lines contains
  int nBreaks = 0;
  WrapLineCached( nLineIndex, GetScreenChars(), nullptr, nBreaks );

  return GetEmptySubLines(nLineIndex) + nBreaks + 1;
}

bool CCrystalTextView::IsEmptySubLineIndex( int nSubLineIndex )
{
  int nLineIndex;
  int dummy;
  GetLineBySubLine(nSubLineIndex, nLineIndex, dummy);
  int nSubLineIndexNextLine = GetSubLineIndex(nLineIndex) + GetSubLines(nLineIndex);
  if (nSubLineIndexNextLine - GetEmptySubLines(nLineIndex) <= nSubLineIndex && nSubLineIndex < nSubLineIndexNextLine)
    return true;
  else
    return false;
}

int CCrystalTextView::CharPosToPoint( int nLineIndex, int nCharPos, CPoint &charPoint, int* pnColumn )
{
  // if we do not wrap lines, y is allways 0 and x is equl to nCharPos
  if (!m_bWordWrap)
    {
      charPoint.x = nCharPos;
      charPoint.y = 0;
    }

  // line is wrapped
  vector<int> anBreaks(GetLineLength (nLineIndex) + 1);
  int nBreaks = 0;

  WrapLineCached (nLineIndex, GetScreenChars(), &anBreaks, nBreaks);

  if (GetTextLayoutMode () == TEXTLAYOUT_TABLE_WORDWRAP)
    {
      int nColumn = 0;
      int j = 0;
      size_t i = 0;
      for (; i < anBreaks.size () && abs (anBreaks[i]) <= nCharPos ; ++i)
        {
          if (anBreaks[i] < 0)
            {
              nColumn++;
              j = 0;
            }
          else
            ++j;
        }
      charPoint.x = (i > 0) ? nCharPos - abs (anBreaks[i - 1]) : nCharPos;
      charPoint.y = j;
      if (pnColumn)
        *pnColumn = nColumn;

      return (i > 0)? abs (anBreaks[i - 1]) : 0;
    }
  else
    {
      int i = (nBreaks <= 0) ? -1 : nBreaks - 1;
      for (; i >= 0 && nCharPos < anBreaks[i]; i--)
          ; // Empty loop!

      charPoint.x = (i >= 0)? nCharPos - anBreaks[i] : nCharPos;
      charPoint.y = i + 1;

      int nReturnVal = (i >= 0)? anBreaks[i] : 0;

      return nReturnVal;
    }
}

/** Does character introduce a multicharacter character? */
static inline bool IsLeadByte(TCHAR ch)
{
#ifdef UNICODE
  return false;
#else
  return _getmbcp() && IsDBCSLeadByte(ch);
#endif
}

int CCrystalTextView::CursorPointToCharPos( int nLineIndex, const CPoint &curPoint )
{
  // calculate char pos out of point
  const int nLength = GetLineLength( nLineIndex );
  const int nScreenChars = GetScreenChars();
  LPCTSTR	szLine = GetLineChars( nLineIndex );

  // wrap line
  vector<int> anBreaks(nLength + 1);
  int	nBreaks = 0;

  WrapLineCached( nLineIndex, nScreenChars, &anBreaks, nBreaks );

  // find char pos that matches cursor position
  int nXPos = 0;
  int nYPos = 0;
  int	nCurPos = 0;
  const int nTabSize = GetTabSize();

  int nIndex=0, nPrevIndex = 0;
  auto pIterChar = ICUBreakIterator::getCharacterBreakIterator(szLine, nLength);
  switch (GetTextLayoutMode ())
    {
      case TEXTLAYOUT_TABLE_NOWORDWRAP:
        {
          int nColumnCount = m_pTextBuffer->GetColumnCount (nLineIndex);
          int nColumnTotalWidth = 0;
          int nColumn = 0;
          bool bInQuote = false;
          const int sep = m_pTextBuffer->GetFieldDelimiter ();
          const int quote = m_pTextBuffer->GetFieldEnclosure ();
          for( nIndex = 0; nIndex < nLength; nIndex = pIterChar->next())
            {
              int nOffset;
              if (!bInQuote && szLine[nIndex] == sep)
                {
                  nColumnTotalWidth += m_pTextBuffer->GetColumnWidth (nColumn++);
                  nOffset = nColumnTotalWidth - nXPos;
                }
              else
                {
                  if (szLine[nIndex] == quote)
                    bInQuote = !bInQuote;
                  if (szLine[nIndex] == '\t')
                    nOffset = 1;
                  else
                    nOffset = GetCharCellCountFromChar (szLine + nIndex);
                  if (nColumn < nColumnCount && nCurPos + nOffset > nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn))
                    nOffset = nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn) - nXPos;
                }
              nXPos += nOffset;
              nCurPos += nOffset;

              if( nXPos > curPoint.x && nYPos == curPoint.y )
                break;
              else if( nYPos > curPoint.y )
                {
                  nIndex = nPrevIndex;
                  break;
                }

              nPrevIndex = nIndex;
            }
        }
        break;
      case TEXTLAYOUT_TABLE_WORDWRAP:
        {
          int i = 0;
          int nColumn = 0;
          int nColumnSumWidth = 0;
          int nColumnCurPoint = INT_MAX;
          if (curPoint.x < m_pTextBuffer->GetColumnWidth (0))
            nColumnCurPoint = 0;
          for( nIndex = 0; nIndex < nLength; nIndex = pIterChar->next ())
            {
              if (i < static_cast<int>(anBreaks.size ()) && nIndex == abs (anBreaks[i]))
                {
                  if (anBreaks[i++] < 0)
                    {
                      nYPos = 0;
                      nColumnSumWidth += m_pTextBuffer->GetColumnWidth (nColumn++);
                      nXPos = nColumnSumWidth;
                      if (nColumnSumWidth <= curPoint.x && curPoint.x < nColumnSumWidth + m_pTextBuffer->GetColumnWidth (nColumn))
                        nColumnCurPoint = nColumn;
                    }
                  else
                    {
                      nXPos = nColumnSumWidth;
                      nYPos++;
                    }
                }

              int nOffset;
              if (szLine[nIndex] == '\t')
                nOffset = 1;
              else
                nOffset = GetCharCellCountFromChar (szLine + nIndex);
              nXPos += nOffset;
              nCurPos += nOffset;

              if( nXPos > curPoint.x && nYPos == curPoint.y )
                break;
              else if( nColumnCurPoint < nColumn && nPrevIndex != 0)
                {
                  nIndex = nPrevIndex;
                  break;
                }
              else if ( nYPos == curPoint.y)
                nPrevIndex = nIndex;
            }
          if (nIndex == nLength && nYPos != curPoint.y)
            nIndex = nPrevIndex;
        }
        break;
      default:
        {
          for( nIndex = 0; nIndex < nLength; nIndex = pIterChar->next())
            {
              if( nBreaks > 0 && nYPos < static_cast<int>(anBreaks.size ()) && nIndex == anBreaks[nYPos] )
                {
                  nXPos = 0;
                  nYPos++;
                }

              int nOffset;
              if (szLine[nIndex] == _T('\t'))
                nOffset = nTabSize - nCurPos % nTabSize;
              else
                nOffset = GetCharCellCountFromChar(szLine + nIndex);
              nXPos += nOffset;
              nCurPos += nOffset;

              if( nXPos > curPoint.x && nYPos == curPoint.y )
                break;
              else if( nYPos > curPoint.y )
                {
                  nIndex = nPrevIndex;
                  break;
                }

              nPrevIndex = nIndex;
            }
        }
    }
  return nIndex;
}

void CCrystalTextView::SubLineCursorPosToTextPos( const CPoint &subLineCurPos, CPoint &textPos )
{
  // Get line breaks
  int	nSubLineOffset, nLine;

  GetLineBySubLine( subLineCurPos.y, nLine, nSubLineOffset );

  // compute cursor-position
  textPos.x = CursorPointToCharPos( nLine, CPoint( subLineCurPos.x, nSubLineOffset ) );
  textPos.y = nLine;
}

/**
 * @brief Calculate last character position in (sub)line.
 * @param [in] nLineIndex Linenumber to check.
 * @param [in] nSublineOffset Subline index in wrapped line.
 * @return Position of the last character.
 */
int CCrystalTextView::SubLineEndToCharPos(int nLineIndex, int nSubLineOffset)
{
  const int nLength = GetLineLength(nLineIndex);

  // if word wrapping is disabled, the end is equal to the length of the line -1
  if (!m_bWordWrap)
    return nLength;

  // wrap line
  vector<int> anBreaks(nLength + 1);
  int nBreaks = 0;

  WrapLineCached(nLineIndex, GetScreenChars(), &anBreaks, nBreaks);

  if (GetTextLayoutMode() == TEXTLAYOUT_TABLE_WORDWRAP)
    {
      int nBreakLast = -1;
      for (int i = 0, j = 1; i < static_cast<int>(anBreaks.size ()); ++i, ++j)
        {
          if (anBreaks[i] < 0)
            j = 0;
          if (j == nSubLineOffset)
            nBreakLast = i;
        }
      if (nBreakLast < static_cast<int>(anBreaks.size ()) - 1)
        return abs (anBreaks[nBreakLast + 1]) - 1;
      return nLength;
    }

  // if there is no break inside the line or the given subline is the last
  // one in this line...
  if (nBreaks <= 0 || nSubLineOffset == nBreaks)
    {
      return nLength;
    }

  // compute character position for end of subline
  ASSERT(nSubLineOffset >= 0 && nSubLineOffset <= nBreaks);

  int nReturnVal = anBreaks[nSubLineOffset] - 1;

  return nReturnVal;
}

/** 
 * @brief Calculate first character position in (sub)line.
 * @param [in] nLineIndex Linenumber to check.
 * @param [in] nSublineOffset Subline index in wrapped line.
 * @return Position of the first character.
 */
int CCrystalTextView::SubLineHomeToCharPos(int nLineIndex, int nSubLineOffset)
{
  // if word wrapping is disabled, the start is 0
  if (!m_bWordWrap || nSubLineOffset == 0)
    return 0;

  // wrap line
  int nLength = GetLineLength(nLineIndex);
  vector<int> anBreaks(nLength + 1);
  int nBreaks = 0;

  WrapLineCached(nLineIndex, GetScreenChars(), &anBreaks, nBreaks);

  if (GetTextLayoutMode() == TEXTLAYOUT_TABLE_WORDWRAP)
    {
      for (int i = 0, j = 1; i < static_cast<int>(anBreaks.size ()); ++i, ++j)
        {
          if (anBreaks[i] < 0)
            j = 0;
          if (j == nSubLineOffset)
            return abs (anBreaks[i]);
        }
      return 0;
    }

  // if there is no break inside the line...
  if (nBreaks == 0)
    {
      return 0;
    }

  // compute character position for end of subline
  ASSERT(nSubLineOffset > 0 && nSubLineOffset <= nBreaks);

  int nReturnVal = anBreaks[nSubLineOffset - 1];

  return nReturnVal;
}
//END SW

int CCrystalTextView::
GetCharWidth ()
{
  if (m_nCharWidth == -1)
    CalcLineCharDim ();
  return m_nCharWidth;
}

int CCrystalTextView::
GetMaxLineLength (int nTopLine, int nLines)
{
  int nMaxLineLength = 0;
  const int nLineCount = (std::min)(nTopLine + nLines, GetLineCount ());
  for (int I = nTopLine; I < nLineCount; I++)
    {
      int nActualLength = GetLineActualLength (I);
      if (nMaxLineLength < nActualLength)
        nMaxLineLength = nActualLength;
    }
  return nMaxLineLength;
}

CCrystalTextView *CCrystalTextView::
GetSiblingView (int nRow, int nCol)
{
  CSplitterWnd *pSplitter = GetParentSplitter (this, false);
  if (pSplitter == nullptr)
    return nullptr;
  CWnd *pWnd = CWnd::FromHandlePermanent (
                 ::GetDlgItem (pSplitter->m_hWnd, pSplitter->IdFromRowCol (nRow, nCol)));
  if (pWnd == nullptr || !pWnd->IsKindOf (RUNTIME_CLASS (CCrystalTextView)))
    return nullptr;
  return static_cast<CCrystalTextView *>(pWnd);
}

void CCrystalTextView::
GoToLine (int nLine, bool bRelative)
{
  int nLines = m_pTextBuffer->GetLineCount () - 1;
  CPoint ptCursorPos = GetCursorPos ();
  if (bRelative)
    {
      nLine += ptCursorPos.y;
    }
  if (nLine)
    {
      nLine--;
    }
  if (nLine > nLines)
    {
      nLine = nLines;
    }
  if (nLine >= 0)
    {
      int nChars = m_pTextBuffer->GetLineLength (nLine);
      if (nChars)
        {
          nChars--;
        }
      if (ptCursorPos.x > nChars)
        {
          ptCursorPos.x = nChars;
        }
      if (ptCursorPos.x >= 0)
        {
          ptCursorPos.y = nLine;
          ASSERT_VALIDTEXTPOS (ptCursorPos);
          SetAnchor (ptCursorPos);
          SetSelection (ptCursorPos, ptCursorPos);
          SetCursorPos (ptCursorPos);
          EnsureVisible (ptCursorPos);
        }
    }
}

void CCrystalTextView::
OnInitialUpdate ()
{
  __super::OnInitialUpdate ();
  CString sDoc = GetDocument ()->GetPathName (), sExt = GetExt (sDoc);
  if (!sExt.IsEmpty())
    SetTextType (sExt);
  AttachToBuffer (nullptr);

  CSplitterWnd *pSplitter = GetParentSplitter (this, false);
  if (pSplitter != nullptr)
    {
      //  See CSplitterWnd::IdFromRowCol() implementation
      int nRow = (GetDlgCtrlID () - AFX_IDW_PANE_FIRST) / 16;
      int nCol = (GetDlgCtrlID () - AFX_IDW_PANE_FIRST) % 16;
      ASSERT (nRow >= 0 && nRow < pSplitter->GetRowCount ());
      ASSERT (nCol >= 0 && nCol < pSplitter->GetColumnCount ());

      if (nRow > 0)
        {
          CCrystalTextView *pSiblingView = GetSiblingView (0, nCol);
          if (pSiblingView != nullptr && pSiblingView != this)
            {
              m_nOffsetChar = pSiblingView->m_nOffsetChar;
              ASSERT (m_nOffsetChar >= 0 && m_nOffsetChar <= GetMaxLineLength (m_nTopLine, GetScreenLines()));
            }
        }

      if (nCol > 0)
        {
          CCrystalTextView *pSiblingView = GetSiblingView (nRow, 0);
          if (pSiblingView != nullptr && pSiblingView != this)
            {
              m_nTopLine = pSiblingView->m_nTopLine;
              ASSERT (m_nTopLine >= 0 && m_nTopLine < GetLineCount ());
            }
        }
    }
  SetFont (m_LogFont);
  if (m_bRememberLastPos && !sDoc.IsEmpty ())
    {
      DWORD dwLastPos[3];
      CString sKey = REG_EDITPAD;
      sKey += _T ("\\Remembered");
      CReg reg;
      if (reg.Open (HKEY_CURRENT_USER, sKey, KEY_READ) &&
        reg.LoadBinary (sDoc, (LPBYTE) dwLastPos, sizeof (dwLastPos)))
        {
          CPoint ptCursorPos;
          ptCursorPos.x = dwLastPos[1];
          ptCursorPos.y = dwLastPos[2];
          if (IsValidTextPosY (ptCursorPos))
            {
              if (!IsValidTextPosX (ptCursorPos))
                ptCursorPos.x = 0;
              ASSERT_VALIDTEXTPOS (ptCursorPos);
              SetCursorPos (ptCursorPos);
              SetSelection (ptCursorPos, ptCursorPos);
              SetAnchor (ptCursorPos);
              EnsureVisible (ptCursorPos);
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// CCrystalTextView printing

void CCrystalTextView::
OnPrepareDC (CDC * pDC, CPrintInfo * pInfo /*= nullptr*/)
{
  __super::OnPrepareDC (pDC, pInfo);

  if (pInfo != nullptr)
    {
      pInfo->m_bContinuePrinting = true;
      if (m_nPrintPages != 0 && (int) pInfo->m_nCurPage > m_nPrintPages)
        pInfo->m_bContinuePrinting = false;
    }
}

BOOL CCrystalTextView::
OnPreparePrinting (CPrintInfo * pInfo)
{
  return DoPreparePrinting (pInfo);
}

void CCrystalTextView::
GetPrintHeaderText (int nPageNum, CString & text)
{
  ASSERT (m_bPrintHeader);
  text = _T ("");
}

void CCrystalTextView::
GetPrintFooterText (int nPageNum, CString & text)
{
  ASSERT (m_bPrintFooter);
  text.Format (_T ("Page %d/%d"), nPageNum, m_nPrintPages);
}

void CCrystalTextView::
PrintHeader (CDC * pdc, int nPageNum)
{
  CRect rcHeader = m_rcPrintArea;
  rcHeader.bottom = rcHeader.top;
  rcHeader.top -= (m_nPrintLineHeight + m_nPrintLineHeight / 2);

  CString text;
  GetPrintHeaderText (nPageNum, text);
  if (!text.IsEmpty ())
    pdc->DrawText (text, &rcHeader, DT_CENTER | DT_NOPREFIX | DT_TOP | DT_SINGLELINE);
}

void CCrystalTextView::
PrintFooter (CDC * pdc, int nPageNum)
{
  CRect rcFooter = m_rcPrintArea;
  rcFooter.top = rcFooter.bottom;
  rcFooter.bottom += (m_nPrintLineHeight + m_nPrintLineHeight / 2);

  CString text;
  GetPrintFooterText (nPageNum, text);
  if (!text.IsEmpty ())
    pdc->DrawText (text, &rcFooter, DT_CENTER | DT_NOPREFIX | DT_BOTTOM | DT_SINGLELINE);
}

/**
* @brief Retrieves the print margins
* @param nLeft   [out] Left margin
* @param nTop    [out] Top margin
* @param nRight  [out] right margin
* @param nBottom [out] Bottom margin
*/
void CCrystalTextView::
GetPrintMargins (long & nLeft, long & nTop, long & nRight, long & nBottom)
{
  nLeft = DEFAULT_PRINT_MARGIN;
  nTop = DEFAULT_PRINT_MARGIN;
  nRight = DEFAULT_PRINT_MARGIN;
  nBottom = DEFAULT_PRINT_MARGIN;
  CReg reg;
  if (reg.Open (HKEY_CURRENT_USER, REG_EDITPAD, KEY_READ))
    {
      DWORD dwTemp;
      if (reg.LoadNumber (_T ("PageLeft"), &dwTemp))
        nLeft = dwTemp;
      if (reg.LoadNumber (_T ("PageRight"), &dwTemp))
        nRight = dwTemp;
      if (reg.LoadNumber (_T ("PageTop"), &dwTemp))
        nTop = dwTemp;
      if (reg.LoadNumber (_T ("PageBottom"), &dwTemp))
        nBottom = dwTemp;
    }
}

void CCrystalTextView::
RecalcPageLayouts (CDC * pdc, CPrintInfo * pInfo)
{
  m_ptPageArea = pInfo->m_rectDraw;
  m_ptPageArea.NormalizeRect ();

  m_nPrintLineHeight = pdc->GetTextExtent (_T ("X")).cy;

  m_rcPrintArea = m_ptPageArea;
  CSize szTopLeft, szBottomRight;
  CWinApp *pApp = AfxGetApp ();
  ASSERT (pApp != nullptr);
  GetPrintMargins (szTopLeft.cx, szTopLeft.cy, szBottomRight.cx, szBottomRight.cy);
  pdc->HIMETRICtoLP (&szTopLeft);
  pdc->HIMETRICtoLP (&szBottomRight);
  m_rcPrintArea.left += szTopLeft.cx;
  m_rcPrintArea.right -= szBottomRight.cx;
  m_rcPrintArea.top += szTopLeft.cy;
  m_rcPrintArea.bottom -= szBottomRight.cy;
  if (m_bPrintHeader)
    m_rcPrintArea.top += m_nPrintLineHeight + m_nPrintLineHeight / 2;
  if (m_bPrintFooter)
    m_rcPrintArea.bottom -= m_nPrintLineHeight + m_nPrintLineHeight / 2;

  InvalidateLineCache (0, -1);

  m_nScreenChars = (m_rcPrintArea.Width () - GetMarginWidth (pdc)) / GetCharWidth ();
  m_nScreenLines = m_rcPrintArea.Height () / GetLineHeight ();
}

void CCrystalTextView::
OnBeginPrinting (CDC * pdc, CPrintInfo * pInfo)
{
  ASSERT (m_pPrintFont == nullptr);
  LOGFONT lf = m_lfBaseFont;
  CDC *pDisplayDC = GetDC ();
  lf.lfHeight = MulDiv (lf.lfHeight, pdc->GetDeviceCaps (LOGPIXELSY), pDisplayDC->GetDeviceCaps (LOGPIXELSY));
  lf.lfWidth = MulDiv (lf.lfWidth, pdc->GetDeviceCaps (LOGPIXELSX), pDisplayDC->GetDeviceCaps (LOGPIXELSX));
  ReleaseDC (pDisplayDC);

  m_pCrystalRendererSaved = m_pCrystalRenderer.release();
  m_pCrystalRenderer.reset(new CCrystalRendererGDI());

  m_pPrintFont = new CFont;
  if (!m_pPrintFont->CreateFontIndirect (&lf))
    {
      delete m_pPrintFont;
      m_pPrintFont = nullptr;
      return;
    }

  GetFont (m_lfSavedBaseFont);
  m_pPrintFont->GetLogFont (&lf);
  SetFont(lf);

  m_nPrintPages = 0;
  m_bPrinting = true;
}

void CCrystalTextView::
OnEndPrinting (CDC * pdc, CPrintInfo * pInfo)
{
  if (m_pCrystalRendererSaved)
    {
      m_pCrystalRenderer.reset(m_pCrystalRendererSaved);
      m_pCrystalRendererSaved = nullptr;
    }
  if (m_pPrintFont != nullptr)
    {
      delete m_pPrintFont;
      m_pPrintFont = nullptr;
      SetFont(m_lfSavedBaseFont);
    }
  m_nPrintPages = 0;
  m_nPrintLineHeight = 0;
  m_bPrinting = false;
}

void CCrystalTextView::
OnPrint (CDC * pdc, CPrintInfo * pInfo)
{
  pdc->SelectObject (m_pPrintFont);

  const COLORREF defaultLineColor = RGB(0,0,0);
  const COLORREF defaultBgColor = RGB(255,255,255);

  RecalcPageLayouts (pdc, pInfo);

  m_nPrintPages = (GetSubLineCount () + GetScreenLines () - 1) / GetScreenLines ();

  ASSERT (pInfo->m_nCurPage >= 1 && (int) pInfo->m_nCurPage <= m_nPrintPages);

  int nTopSubLine = (pInfo->m_nCurPage - 1) * GetScreenLines ();
  int nEndSubLine = nTopSubLine + GetScreenLines () - 1;
  if (nEndSubLine >= GetSubLineCount ())
    nEndSubLine = GetSubLineCount () - 1;
  int nSubLines;
  int nTopLine, nEndLine;
  GetLineBySubLine (nTopSubLine, nTopLine, nSubLines);
  GetLineBySubLine (nEndSubLine, nEndLine, nSubLines);

  TRACE (_T ("Printing page %d of %d, lines %d - %d\n"), 
        pInfo->m_nCurPage, m_nPrintPages, nTopLine, nEndLine);

  m_pCrystalRenderer->BindDC(*pdc, m_rcPrintArea);
  m_pCrystalRenderer->BeginDraw();

  m_pCrystalRenderer->SetTextColor(defaultLineColor);
  m_pCrystalRenderer->SetBkColor(defaultBgColor);

  if (m_bPrintHeader)
    {
      PrintHeader (pdc, pInfo->m_nCurPage);
    }

  if (m_bPrintFooter)
    {
      PrintFooter (pdc, pInfo->m_nCurPage);
    }

  // set clipping region
  // see http://support.microsoft.com/kb/128334
  CRect rectClip = m_rcPrintArea;
  rectClip.right = rectClip.left + GetMarginWidth (pdc) + GetScreenChars () * GetCharWidth ();
  rectClip.bottom = rectClip.top + GetScreenLines () * GetLineHeight ();
  if (!!pdc->IsKindOf (RUNTIME_CLASS (CPreviewDC)))
    {
      CPreviewDC *pPrevDC = (CPreviewDC *)pdc;

      pPrevDC->PrinterDPtoScreenDP (&rectClip.TopLeft ());
      pPrevDC->PrinterDPtoScreenDP (&rectClip.BottomRight ());

      CPoint ptOrg;
      ::GetViewportOrgEx (pdc->m_hDC,&ptOrg);
      rectClip += ptOrg;
    }
  CRgn rgn;
  rgn.CreateRectRgnIndirect (&rectClip);
  pdc->SelectClipRgn (&rgn);

  // print lines
  CRect rcMargin;
  CRect rcLine = m_rcPrintArea;
  int nLineHeight = GetLineHeight ();
  rcLine.bottom = rcLine.top + nLineHeight;
  rcMargin = rcLine;
  rcMargin.right = rcMargin.left + GetMarginWidth (pdc);
  rcLine.left = rcMargin.right;

  int nSubLineOffset = GetSubLineIndex (nTopLine) - nTopSubLine;
  if( nSubLineOffset < 0 )
    {
      rcLine.OffsetRect( 0, nSubLineOffset * nLineHeight );
    }

  int nLineCount = GetLineCount();
  int nCurrentLine;
  for (nCurrentLine = nTopLine; nCurrentLine <= nEndLine; nCurrentLine++)
    {
      rcLine.bottom = rcLine.top + GetSubLines (nCurrentLine) * nLineHeight;
      rcMargin.bottom = rcLine.bottom;

      if (nCurrentLine < nLineCount && GetLineVisible (nCurrentLine))
        {
          DrawMargin (rcMargin, nCurrentLine, nCurrentLine + 1);
          DrawSingleLine (rcLine, nCurrentLine);
          if (nCurrentLine+1 < nLineCount && !GetLineVisible (nCurrentLine + 1))
            m_pCrystalRenderer->DrawBoundaryLine (rcMargin.left, rcLine.right, rcMargin.bottom-1);
        }

      rcLine.top = rcLine.bottom;
      rcMargin.top = rcLine.bottom;
    }

  m_pCrystalRenderer->EndDraw();

  pdc->SelectClipRgn (nullptr);
}


/////////////////////////////////////////////////////////////////////////////
// CCrystalTextView message handlers

int CCrystalTextView::
GetLineCount ()
{
  if (m_pTextBuffer == nullptr)
    return 1;                   //  Single empty line

  int nLineCount = m_pTextBuffer->GetLineCount ();
  ASSERT (nLineCount > 0);
  return nLineCount;
}

//BEGIN SW
int CCrystalTextView::GetSubLineCount()
{
  const int nLineCount = GetLineCount();

  // if we do not wrap words, number of sub lines is
  // equal to number of lines
  if( !m_bWordWrap && !m_bHideLines )
    return nLineCount;

  // calculate number of sub lines
  if (nLineCount <= 0)
    return 0;
  return CCrystalTextView::GetSubLineIndex( nLineCount - 1 ) + GetSubLines( nLineCount - 1 );
}

int CCrystalTextView::GetSubLineIndex( int nLineIndex )
{
  // if we do not wrap words, subline index of this line is equal to its index
  if( !m_bWordWrap && !m_bHideLines )
    return nLineIndex;

  // calculate subline index of the line
  int	nSubLineCount = 0;
  int nLineCount = GetLineCount();

  if( nLineIndex >= nLineCount )
    nLineIndex = nLineCount - 1;

  // return cached subline index of the line if it is already cached.
  if (nLineIndex <= m_nLastLineIndexCalculatedSubLineIndex)
    return (*m_panSubLineIndexCache)[nLineIndex];

  // calculate subline index of the line and cache it.
  if (m_nLastLineIndexCalculatedSubLineIndex >= 0)
    nSubLineCount = (*m_panSubLineIndexCache)[m_nLastLineIndexCalculatedSubLineIndex];
  else
    {
      m_nLastLineIndexCalculatedSubLineIndex = 0;
      m_panSubLineIndexCache->SetAtGrow( 0, 0 );
    }

// TODO: Rethink this, it is very time consuming
  for( int i = m_nLastLineIndexCalculatedSubLineIndex; i < nLineIndex; i++ )
    {
      m_panSubLineIndexCache->SetAtGrow( i, nSubLineCount);
      nSubLineCount+= GetSubLines( i );
    }
  m_panSubLineIndexCache->SetAtGrow( nLineIndex, nSubLineCount);
  m_nLastLineIndexCalculatedSubLineIndex = nLineIndex;

  return nSubLineCount;
}

// See comment in the header file
void CCrystalTextView::GetLineBySubLine(int nSubLineIndex, int &nLine, int &nSubLine)
{
  if (GetSubLineCount() == 0)
    {
      nLine = 0;
      nSubLine = 0;
      return;
    }

  ASSERT( nSubLineIndex < GetSubLineCount() );

  // if we do not wrap words, nLine is equal to nSubLineIndex and nSubLine is allways 0
  if ( !m_bWordWrap && !m_bHideLines )
    {
      nLine = nSubLineIndex;
      nSubLine = 0;
      return;
    }

  // compute result
  const int nLineCount = GetLineCount();

  // binary search
  int base = 0, i = 0, nSubLineIndex2 = 0;
  for (int lim = nLineCount; lim != 0; lim >>= 1)
    {
      i = base + (lim >> 1);
      nSubLineIndex2 = GetSubLineIndex(i);
      if (nSubLineIndex >= nSubLineIndex2 && nSubLineIndex < nSubLineIndex2 + GetSubLines(i))
        break;
      else if (nSubLineIndex2 <= nSubLineIndex)	/* key > p: move right */
        {
          base = i + 1;
          lim--;
        } /* else move left */
    }

  ASSERT(i < nLineCount);
  nLine = i;
  nSubLine = nSubLineIndex - nSubLineIndex2;
}

int CCrystalTextView::
GetLineLength (int nLineIndex) const
{
  if (m_pTextBuffer == nullptr)
    return 0;
  return m_pTextBuffer->GetLineLength (nLineIndex);
}

int CCrystalTextView::
GetFullLineLength (int nLineIndex) const
{
  if (m_pTextBuffer == nullptr)
    return 0;
  return m_pTextBuffer->GetFullLineLength (nLineIndex);
}

// How many bytes of line are displayed on-screen?
int CCrystalTextView::
GetViewableLineLength (int nLineIndex) const
{
  if (m_bViewEols)
    return GetFullLineLength(nLineIndex);
  else
    return GetLineLength(nLineIndex);
}

LPCTSTR CCrystalTextView::
GetLineChars (int nLineIndex) const
{
  if (m_pTextBuffer == nullptr)
    return nullptr;
  return m_pTextBuffer->GetLineChars (nLineIndex);
}

/** 
 * @brief Reattach buffer after deleting/inserting ghost lines :
 * 
 * @note no need to reinitialize the horizontal scrollbar
 * no need to reset the editor options (m_bOvrMode, m_bLastReplace)
 */
void CCrystalTextView::
ReAttachToBuffer (CCrystalTextBuffer * pBuf /*= nullptr*/ )
{
  if (m_pTextBuffer != nullptr)
    m_pTextBuffer->RemoveView (this);
  if (pBuf == nullptr)
    {
      pBuf = LocateTextBuffer ();
      //  ...
    }
  m_pTextBuffer = pBuf;
  if (m_pTextBuffer != nullptr)
    m_pTextBuffer->AddView (this);
  // don't reset CCrystalEditView options
  CCrystalTextView::ResetView ();

  //  Init scrollbars arrows
  CScrollBar *pVertScrollBarCtrl = GetScrollBarCtrl (SB_VERT);
  if (pVertScrollBarCtrl != nullptr)
    pVertScrollBarCtrl->EnableScrollBar (GetScreenLines () >= GetLineCount ()?
                                         ESB_DISABLE_BOTH : ESB_ENABLE_BOTH);
  //  Update vertical scrollbar only
  RecalcVertScrollBar ();
}

/** 
 * @brief Attach buffer (maybe for the first time)
 * initialize the view and initialize both scrollbars
 */
void CCrystalTextView::
AttachToBuffer (CCrystalTextBuffer * pBuf /*= nullptr*/ )
{
  if (m_pTextBuffer != nullptr)
    m_pTextBuffer->RemoveView (this);
  if (pBuf == nullptr)
    {
      pBuf = LocateTextBuffer ();
      //  ...
    }
  m_pTextBuffer = pBuf;
  if (m_pTextBuffer != nullptr)
    m_pTextBuffer->AddView (this);
  ResetView ();

  //  Init scrollbars
  CScrollBar *pVertScrollBarCtrl = GetScrollBarCtrl (SB_VERT);
  if (pVertScrollBarCtrl != nullptr)
    pVertScrollBarCtrl->EnableScrollBar (GetScreenLines () >= GetLineCount ()?
                                         ESB_DISABLE_BOTH : ESB_ENABLE_BOTH);
  CScrollBar *pHorzScrollBarCtrl = GetScrollBarCtrl (SB_HORZ);
  if (pHorzScrollBarCtrl != nullptr)
    pHorzScrollBarCtrl->EnableScrollBar (GetScreenChars () >= GetMaxLineLength (m_nTopLine, GetScreenLines())?
                                         ESB_DISABLE_BOTH : ESB_ENABLE_BOTH);

  //  Update scrollbars
  RecalcVertScrollBar ();
  RecalcHorzScrollBar ();
}

void CCrystalTextView::
DetachFromBuffer ()
{
  if (m_pTextBuffer != nullptr)
    {
      m_pTextBuffer->RemoveView (this);
      m_pTextBuffer = nullptr;
      // don't reset CCrystalEditView options
      CCrystalTextView::ResetView ();
    }
}

int CCrystalTextView::
GetScreenLines ()
{
  if (m_nScreenLines == -1)
    {
      CRect rect;
      GetClientRect (&rect);
      m_nScreenLines = (rect.Height () - GetTopMarginHeight ()) / GetLineHeight ();
    }
  return m_nScreenLines;
}

bool CCrystalTextView::
GetItalic (int nColorIndex)
{
  // WINMERGE - since italic text has problems,
  // lets disable it. E.g. "_" chars disappear and last
  // char may be cropped.
  return false;

  // return nColorIndex == COLORINDEX_COMMENT;
}

bool CCrystalTextView::
GetBold (int nColorIndex)
{
  if (m_pColors  != nullptr)
    {
      nColorIndex &= ~COLORINDEX_APPLYFORCE;
      return m_pColors->GetBold(nColorIndex);
    }
  else
    return false;
}

int CCrystalTextView::
GetScreenChars ()
{
  if (m_nScreenChars == -1)
    {
      CRect rect;
      GetClientRect (&rect);
      m_nScreenChars = (rect.Width () - GetMarginWidth ()) / GetCharWidth ();
    }
  return m_nScreenChars;
}

void CCrystalTextView::
OnDestroy ()
{
  DetachFromBuffer ();
  m_hAccel = nullptr;

  __super::OnDestroy ();

  if (m_pCacheBitmap != nullptr)
    {
      delete m_pCacheBitmap;
      m_pCacheBitmap = nullptr;
    }
}

BOOL CCrystalTextView::
OnEraseBkgnd (CDC * pdc)
{
  UNREFERENCED_PARAMETER(pdc);
  return true;
}

void CCrystalTextView::
OnSize (UINT nType, int cx, int cy)
{
  __super::OnSize (nType, cx, cy);

  //BEGIN SW
  // get char position of top left visible character with old cached word wrap
  CPoint	topPos;
  SubLineCursorPosToTextPos( CPoint( 0, m_nTopSubLine ), topPos );
  //END SW

  //BEGIN SW
  // we have to recompute the line wrapping
  InvalidateScreenRect(false);

  // compute new top sub line
  CPoint	topSubLine;
  CharPosToPoint( topPos.y, topPos.x, topSubLine );
  m_nTopSubLine = GetSubLineIndex(topPos.y) + topSubLine.y;

  ScrollToSubLine( m_nTopSubLine );

  // set caret to right position
  UpdateCaret();
  //END SW

  RecalcVertScrollBar (false, false);
  RecalcHorzScrollBar (false, false);
}

void CCrystalTextView::
UpdateSiblingScrollPos (bool bHorz)
{
  CSplitterWnd *pSplitterWnd = GetParentSplitter (this, false);
  if (pSplitterWnd != nullptr)
    {
      //  See CSplitterWnd::IdFromRowCol() implementation for details
      int nCurrentRow = (GetDlgCtrlID () - AFX_IDW_PANE_FIRST) / 16;
      int nCurrentCol = (GetDlgCtrlID () - AFX_IDW_PANE_FIRST) % 16;
      ASSERT (nCurrentRow >= 0 && nCurrentRow < pSplitterWnd->GetRowCount ());
      ASSERT (nCurrentCol >= 0 && nCurrentCol < pSplitterWnd->GetColumnCount ());

      if (bHorz)
        {
          int nCols = pSplitterWnd->GetColumnCount ();
          for (int nCol = 0; nCol < nCols; nCol++)
            {
              if (nCol != nCurrentCol)  //  We don't need to update ourselves
                {
                  CCrystalTextView *pSiblingView = GetSiblingView (nCurrentRow, nCol);
                  if (pSiblingView != nullptr)
                    pSiblingView->OnUpdateSibling (this, false);
                }
            }
        }
      else
        {
          int nRows = pSplitterWnd->GetRowCount ();
          for (int nRow = 0; nRow < nRows; nRow++)
            {
              if (nRow != nCurrentRow)  //  We don't need to update ourselves
                {
                  CCrystalTextView *pSiblingView = GetSiblingView (nRow, nCurrentCol);
                  if (pSiblingView != nullptr)
                    pSiblingView->OnUpdateSibling (this, false);
                }
            }
        }
    }
}

void CCrystalTextView::
OnUpdateSibling (CCrystalTextView * pUpdateSource, bool bHorz)
{
  if (pUpdateSource != this)
    {
      ASSERT (pUpdateSource != nullptr);
      ASSERT_KINDOF (CCrystalTextView, pUpdateSource);
      if (bHorz)
        {
          ASSERT (pUpdateSource->m_nTopLine >= 0);
          ASSERT (pUpdateSource->m_nTopLine < GetLineCount ());
          if (pUpdateSource->m_nTopLine != m_nTopLine)
            {
              ScrollToLine (pUpdateSource->m_nTopLine, true, false);
              UpdateCaret ();
            }
        }
      else
        {
          ASSERT (pUpdateSource->m_nOffsetChar >= 0);
          ASSERT (pUpdateSource->m_nOffsetChar < GetMaxLineLength (m_nTopLine, GetScreenLines()));
          if (pUpdateSource->m_nOffsetChar != m_nOffsetChar)
            {
              ScrollToChar (pUpdateSource->m_nOffsetChar, true, false);
              UpdateCaret ();
            }
        }
    }
}

void CCrystalTextView::
RecalcVertScrollBar (bool bPositionOnly /*= false*/, bool bRedraw /*= true */)
{
  SCROLLINFO si = {0};
  si.cbSize = sizeof (si);
  if (bPositionOnly)
    {
      si.fMask = SIF_POS;
      si.nPos = m_nTopSubLine;
    }
  else
    {
      const int nScreenLines = GetScreenLines();
      if( m_nTopSubLine > 0 && nScreenLines >= GetSubLineCount() )
        {
          m_nTopLine = 0;
          Invalidate ();
          UpdateCaret ();
        }
      si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
      si.nMin = 0;
      si.nMax = GetSubLineCount() - 1;
      si.nPage = nScreenLines;
      si.nPos = m_nTopSubLine;
    }
  VERIFY (SetScrollInfo (SB_VERT, &si, bRedraw));
}

void CCrystalTextView::
OnVScroll (UINT nSBCode, UINT nPos, CScrollBar * pScrollBar)
{
  __super::OnVScroll (nSBCode, nPos, pScrollBar);

  // Note we cannot use nPos because of its 16-bit nature
  SCROLLINFO si = {0};
  si.cbSize = sizeof (si);
  si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
  VERIFY (GetScrollInfo (SB_VERT, &si));

  // Get the minimum and maximum scroll-bar positions.
  int nMinPos = si.nMin;
  int nMaxPos = si.nMax;

  // Get the current position of scroll box.
  int nCurPos = si.nPos;

  bool bDisableSmooth = true;
  switch (nSBCode)
    {
    case SB_TOP:			// Scroll to top.
      nCurPos = nMinPos;
      bDisableSmooth = false;
      break;

    case SB_BOTTOM:			// Scroll to bottom.
      nCurPos = nMaxPos;
      bDisableSmooth = false;
      break;

    case SB_LINEUP:			// Scroll one line up.
      if (nCurPos > nMinPos)
        nCurPos--;
      break;

    case SB_LINEDOWN:		// Scroll one line down.
      if (nCurPos < nMaxPos)
        nCurPos++;
      break;

    case SB_PAGEUP:			// Scroll one page up.
      nCurPos = max(nMinPos, nCurPos - (int) si.nPage + 1);
      bDisableSmooth = false;
      break;

    case SB_PAGEDOWN:		// Scroll one page down.
      nCurPos = min(nMaxPos, nCurPos + (int) si.nPage - 1);
      bDisableSmooth = false;
      break;

    case SB_THUMBPOSITION:		// Scroll to absolute position. nPos is the position
      nCurPos = si.nTrackPos;	// of the scroll box at the end of the drag operation.
      break;

    case SB_THUMBTRACK:			// Drag scroll box to specified position. nPos is the
      nCurPos = si.nTrackPos;	// position that the scroll box has been dragged to.
      break;
    }
  ScrollToSubLine(nCurPos, bDisableSmooth);
}

void CCrystalTextView::
RecalcHorzScrollBar (bool bPositionOnly /*= false*/, bool bRedraw /*= true */)
{
  SCROLLINFO si = {0};
  si.cbSize = sizeof (si);

  const int nScreenChars = GetScreenChars();
  
  if (GetTextLayoutMode () == TEXTLAYOUT_WORDWRAP)
    {
      if (m_nOffsetChar > nScreenChars)
        {
          m_nOffsetChar = 0;
          UpdateCaret ();
        }

      // Disable horizontal scroll bar
      si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
      SetScrollInfo (SB_HORZ, &si);
      return;
    }

  const int nMaxLineLen = GetMaxLineLength (m_nTopLine, GetScreenLines());

  if (bPositionOnly)
    {
      si.fMask = SIF_POS;
      si.nPos = m_nOffsetChar;
    }
  else
    {
      if (nScreenChars >= nMaxLineLen && m_nOffsetChar > 0)
        {
          m_nOffsetChar = 0;
          Invalidate ();
          UpdateCaret ();
        }
      si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
      si.nMin = 0;

      // Horiz scroll limit to longest line + one screenwidth 
      si.nMax = nMaxLineLen + nScreenChars;
      si.nPage = nScreenChars;
      si.nPos = m_nOffsetChar;
    }
  VERIFY (SetScrollInfo (SB_HORZ, &si, bRedraw));
}

void CCrystalTextView::
OnHScroll (UINT nSBCode, UINT nPos, CScrollBar * pScrollBar)
{
  // Default handler not needed
  //__super::OnHScroll (nSBCode, nPos, pScrollBar);

  //  Again, we cannot use nPos because it's 16-bit
  SCROLLINFO si = {0};
  si.cbSize = sizeof (si);
  si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
  VERIFY (GetScrollInfo (SB_HORZ, &si));

  // Get the minimum and maximum scroll-bar positions.
  int nMinPos = si.nMin;
  int nMaxPos = si.nMax;

  // Get the current position of scroll box.
  int nCurPos = si.nPos;

  switch (nSBCode)
    {
    case SB_LEFT:             // Scroll to far left.
      nCurPos = nMinPos;
      break;

    case SB_RIGHT:            // Scroll to far right.
      nCurPos = nMaxPos;
      break;

    case SB_ENDSCROLL:        // End scroll.
      break;

    case SB_LINELEFT:         // Scroll left.
      if (nCurPos > nMinPos)
        nCurPos--;
      break;

    case SB_LINERIGHT:        // Scroll right.
      if (nCurPos < nMaxPos)
        nCurPos++;
      break;

    case SB_PAGELEFT:         // Scroll one page left.
      nCurPos = max(nMinPos, nCurPos - (int) si.nPage + 1);
      break;

    case SB_PAGERIGHT:        // Scroll one page right.
      nCurPos = min(nMaxPos, nCurPos + (int) si.nPage - 1);
      break;

    case SB_THUMBPOSITION:    // Scroll to absolute position. The current position is 
      nCurPos = si.nTrackPos; // specified by the nPos parameter.
      break;

    case SB_THUMBTRACK:       // Drag scroll box to specified position. The current 
      nCurPos = si.nTrackPos; // position is specified by the nPos parameter
                              // The SB_THUMBTRACK scroll-bar code typically is used by applications that give 
                              // some feedback while the scroll box is being dragged.
      break;
    }
  ScrollToChar (nCurPos, true);
  // This is needed, but why ? OnVScroll don't need to call UpdateCaret
  UpdateCaret ();
}

BOOL CCrystalTextView::
OnSetCursor (CWnd * pWnd, UINT nHitTest, UINT message)
{
  if (nHitTest == HTCLIENT)
    {
      CPoint pt;
      ::GetCursorPos (&pt);
      ScreenToClient (&pt);
      if (pt.y < GetTopMarginHeight ())
        {
          const int nColumnResizing = ClientToColumnResizing (pt.x);
          ::SetCursor (::LoadCursor (nullptr, nColumnResizing >= 0 ? IDC_SIZEWE : IDC_ARROW));
        }
      else if (pt.x < GetMarginWidth ())
        {
          ::SetCursor (::LoadCursor (AfxGetInstanceHandle (), MAKEINTRESOURCE (IDR_MARGIN_CURSOR)));
        }
      else
        {
          CPoint ptText = ClientToText (pt);
          PrepareSelBounds ();
          if (IsInsideSelBlock (ptText))
            {
              //  [JRT]:  Support For Disabling Drag and Drop...
              if (!m_bDisableDragAndDrop)   // If Drag And Drop Not Disabled

                ::SetCursor (::LoadCursor (nullptr, IDC_ARROW));     // Set To Arrow Cursor

            }
          else
            ::SetCursor (::LoadCursor (nullptr, IDC_IBEAM));
        }
      return true;
    }
  return __super::OnSetCursor (pWnd, nHitTest, message);
}

int CCrystalTextView::
ClientToIdealTextPos (int x)
{
  int nPos;
  if (x > GetMarginWidth ())
    nPos = m_nOffsetChar + (x - GetMarginWidth ()) / GetCharWidth ();
  else
    nPos = 0;
  return nPos;
}

/** 
 * @brief Converts client area point to text position.
 * @param [in] point Client area point.
 * @return Text position (line index, char index in line).
 * @note For gray selection area char index is 0.
 */
CPoint CCrystalTextView::
ClientToText (const CPoint & point)
{
  //BEGIN SW
  const int nSubLineCount = GetSubLineCount();
  const int nLineCount = GetLineCount();

  CPoint pt;
  pt.y = m_nTopSubLine + (point.y - GetTopMarginHeight ()) / GetLineHeight();
  if (pt.y >= nSubLineCount)
    pt.y = nSubLineCount - 1;
  if (pt.y < 0)
    pt.y = 0;

  int nLine;
  int nSubLineOffset;

  GetLineBySubLine( pt.y, nLine, nSubLineOffset );
  pt.y = nLine;

  LPCTSTR pszLine = nullptr;
  int nLength = 0;
  vector<int> anBreaks(1);
  int nBreaks = 0;

  if (pt.y >= 0 && pt.y < nLineCount)
    {
      nLength = GetLineLength( pt.y );
      anBreaks.resize(nLength + 1);
      pszLine = GetLineChars(pt.y);
      WrapLineCached( pt.y, GetScreenChars(), &anBreaks, nBreaks );

      if (nBreaks > nSubLineOffset && GetTextLayoutMode () == TEXTLAYOUT_WORDWRAP)
        nLength = anBreaks[nSubLineOffset] - 1;
    }

  // Char index for margin area is 0
  int nPos = ClientToIdealTextPos (point.x);
  int nIndex = 0;
  int nCurPos = 0;
  int n = 0;
  int i = 0;
  const int nTabSize = GetTabSize();

  auto pIterChar = ICUBreakIterator::getCharacterBreakIterator(pszLine, nLength);
  switch (GetTextLayoutMode ())
    {
      case TEXTLAYOUT_TABLE_NOWORDWRAP:
        {
          int nColumnCount = m_pTextBuffer->GetColumnCount (nLine);
          int nColumnTotalWidth = 0;
          int nColumn = 0;
          bool bInQuote = false;
          const int sep = m_pTextBuffer->GetFieldDelimiter ();
          const int quote = m_pTextBuffer->GetFieldEnclosure ();
          while (nIndex < nLength)
            {
              int nOffset;
              if (!bInQuote && pszLine[nIndex] == sep)
                {
                  nColumnTotalWidth += m_pTextBuffer->GetColumnWidth (nColumn++);
                  nOffset = nColumnTotalWidth - nCurPos;
                }
              else
                {
                  if (pszLine[nIndex] == quote)
                    bInQuote = !bInQuote;
                  if (pszLine[nIndex] == '\t')
                    nOffset = 1;
                  else
                    nOffset = GetCharCellCountFromChar (pszLine + nIndex);
                  if (nColumn < nColumnCount && nCurPos + nOffset > nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn))
                    nOffset = nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn) - nCurPos;
                }
              n += nOffset;
              nCurPos += nOffset;

              if (n > nPos && i == nSubLineOffset)
                break;

              nIndex = pIterChar->next ();
            }
        }
        break;
      case TEXTLAYOUT_TABLE_WORDWRAP:
        {
          int j = 0;
          int nColumn = 0;
          int nColumnSumWidth = 0;
          int nColumnCurPoint = INT_MAX;
          int nPrevIndex = 0;
          if (nPos < m_pTextBuffer->GetColumnWidth (0))
            nColumnCurPoint = 0;
          while (nIndex < nLength)
            {
              if (i < static_cast<int>(anBreaks.size()) && nIndex == abs(anBreaks[i]))
                {
                  if (anBreaks[i++] < 0)
                    {
                      j = 0;
                      nColumnSumWidth += m_pTextBuffer->GetColumnWidth (nColumn++);
                      n = nColumnSumWidth;
                      if (nColumnSumWidth <= nPos && nPos < nColumnSumWidth + m_pTextBuffer->GetColumnWidth (nColumn))
                        nColumnCurPoint = nColumn;
                    }
                  else
                    {
                      n = nColumnSumWidth;
                      j++;
                    }
                }
      
              int nOffset;
              if (pszLine[nIndex] == '\t')
                nOffset = 1;
              else
                nOffset = GetCharCellCountFromChar(pszLine + nIndex);
              n += nOffset;
              nCurPos += nOffset;

              if (n > nPos && j == nSubLineOffset)
                break;
              else if( nColumnCurPoint < nColumn && nPrevIndex != 0)
                {
                  nIndex = nPrevIndex;
                  break;
                }
              else if ( j == nSubLineOffset)
                nPrevIndex = nIndex;

              nIndex = pIterChar->next();
            }
          if (nIndex == nLength && j != nSubLineOffset)
            nIndex = nPrevIndex;
        }
        break;
      default:
        {
          while (nIndex < nLength)
            {
              if (nBreaks && i < static_cast<int>(anBreaks.size ()) && nIndex == anBreaks[i])
                {
                  n = 0;
                  i++;
                }

              int nOffset;
              if (pszLine[nIndex] == '\t')
                nOffset = nTabSize - nCurPos % nTabSize;
              else
                nOffset = GetCharCellCountFromChar(pszLine + nIndex);
              n += nOffset;
              nCurPos += nOffset;

              if (n > nPos && i == nSubLineOffset)
                break;

              nIndex = pIterChar->next();
            }
        }
        break;
    }

  ASSERT(nIndex >= 0 && nIndex <= nLength);
  pt.x = nIndex;
  return pt;
}

int CCrystalTextView::
ClientToColumn (int x)
{
  CRect rcClient;
  GetClientRect (&rcClient);
  int nCharWidth = GetCharWidth ();
  int nMarginWidth = GetMarginWidth ();
  for (int nColumn = 0, columnleft = nMarginWidth - m_nOffsetChar * nCharWidth;
      columnleft < rcClient.Width ();
      columnleft += m_pTextBuffer->GetColumnWidth (nColumn++) * nCharWidth)
    {
      if (columnleft <= x && x < columnleft + m_pTextBuffer->GetColumnWidth (nColumn) * nCharWidth)
        return nColumn;
    }
  return -1;
}

int CCrystalTextView::
ClientToColumnResizing (int x)
{
  const int nColumn = ClientToColumn (x);
  const int nColumnL = ClientToColumn (x - 4);
  const int nColumnR = ClientToColumn (x + 4);
  if (nColumn != nColumnL || nColumn != nColumnR)
    {
      return (nColumn != nColumnL) ? nColumnL : nColumn;
    }
  return -1;
}
#ifdef _DEBUG
void CCrystalTextView::
AssertValidTextPos (const CPoint & point)
{
  if (GetLineCount () > 0)
    {
      ASSERT (m_nTopLine >= 0 && m_nOffsetChar >= 0);
      ASSERT (point.y >= 0 && point.y < GetLineCount ());
      ASSERT (point.x >= 0 && point.x <= GetViewableLineLength (point.y));
    }
}
#endif

bool CCrystalTextView::
IsValidTextPos (const CPoint &point)
{
  return GetLineCount () > 0 && m_nTopLine >= 0 && m_nOffsetChar >= 0 &&
    point.y >= 0 && point.y < GetLineCount () && point.x >= 0 && point.x <= GetLineLength (point.y);
}

bool CCrystalTextView::
IsValidTextPosX (const CPoint &point)
{
  return GetLineCount () > 0 && m_nTopLine >= 0 && m_nOffsetChar >= 0 &&
    point.y >= 0 && point.y < GetLineCount () && point.x >= 0 && point.x <= GetLineLength (point.y);
}

bool CCrystalTextView::
IsValidTextPosY (const CPoint &point)
{
  return GetLineCount () > 0 && m_nTopLine >= 0 && m_nOffsetChar >= 0 &&
    point.y >= 0 && point.y < GetLineCount ();
}

CPoint CCrystalTextView::
TextToClient (const CPoint & point)
{
  ASSERT_VALIDTEXTPOS (point);
  LPCTSTR pszLine = GetLineChars (point.y);

  int nColumnIndex = 0;
  CPoint pt;
  //BEGIN SW
  CPoint	charPoint;
  int nSubLineStart = CharPosToPoint( point.y, point.x, charPoint, &nColumnIndex );
  charPoint.y+= GetSubLineIndex( point.y );

  // compute y-position
  pt.y = (charPoint.y - m_nTopSubLine) * GetLineHeight() + GetTopMarginHeight ();

  // if pt.x is null, we know the result
  if( charPoint.x == 0 && nColumnIndex == 0)
    {
      pt.x = GetMarginWidth();
      return pt;
    }

  // we have to calculate x-position
  int	nPreOffset = 0;
  /*ORIGINAL
  pt.y = (point.y - m_nTopLine) * GetLineHeight();
  */
  //END SW
  pt.x = 0;
  int nTabSize = GetTabSize ();
  auto pIterChar = ICUBreakIterator::getCharacterBreakIterator(pszLine, point.x);
  switch (GetTextLayoutMode ())
    {
      case TEXTLAYOUT_TABLE_NOWORDWRAP:
        {
          int nColumnCount = m_pTextBuffer->GetColumnCount (point.y);
          int nColumnTotalWidth = 0;
          int nColumn = 0;
          bool bInQuote = false;
          const int sep = m_pTextBuffer->GetFieldDelimiter ();
          const int quote = m_pTextBuffer->GetFieldEnclosure ();
          for (int nIndex = 0, nTabs = 0; nIndex < point.x; nIndex = pIterChar->next())
            {
              if (!bInQuote && pszLine[nIndex] == sep)
                {
                  nColumnTotalWidth += m_pTextBuffer->GetColumnWidth (nColumn++);
                  pt.x = nColumnTotalWidth;
                }
              else
                {
                  if (pszLine[nIndex] == quote)
                    bInQuote = !bInQuote;
                  if (pszLine[nIndex] == _T ('\t'))
                    pt.x ++;
                  else
                    pt.x += GetCharCellCountFromChar(pszLine + nIndex);
                  if (nColumn < nColumnCount && pt.x > nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn))
                    pt.x = nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn);
                }
            }
          pt.x = (pt.x - m_nOffsetChar) * GetCharWidth () + GetMarginWidth ();
          return pt;
        }
        break;
      case TEXTLAYOUT_TABLE_WORDWRAP:
        {
          pt.x = 0;
          for (int i = 0; i < nColumnIndex; ++i)
              pt.x += m_pTextBuffer->GetColumnWidth (i);
          for (int nIndex = 0; nIndex < point.x; nIndex = pIterChar->next())
            {
              if( nIndex >= nSubLineStart )
                {
                  if (pszLine[nIndex] == '\t')
                    pt.x ++;
                  else
                    pt.x += GetCharCellCountFromChar (pszLine + nIndex);
                }
            }
          pt.x = (pt.x - m_nOffsetChar) * GetCharWidth () + GetMarginWidth ();
          return pt;
        }
        break;
      default:
        {
          for (int nIndex = 0; nIndex < point.x; nIndex = pIterChar->next())
            {
              //BEGIN SW
              if( nIndex == nSubLineStart )
                nPreOffset = pt.x;
              //END SW
              if (pszLine[nIndex] == _T ('\t'))
                pt.x += (nTabSize - pt.x % nTabSize);
              else
                pt.x += GetCharCellCountFromChar(pszLine + nIndex);
            }
          //BEGIN SW
          pt.x-= nPreOffset;
          //END SW
      
          pt.x = (pt.x - m_nOffsetChar) * GetCharWidth () + GetMarginWidth ();
          return pt;
        }
    }
}

int CCrystalTextView::
ColumnToClient (int nColumn)
{
  CRect rcClient;
  GetClientRect (&rcClient);
  int nCharWidth = GetCharWidth ();
  int columnleft = GetMarginWidth () - m_nOffsetChar * nCharWidth;
  for (int nColumn2 = 0; nColumn2 != nColumn && columnleft < rcClient.Width ();
      columnleft += m_pTextBuffer->GetColumnWidth (nColumn2++) * nCharWidth)
      ;
  return columnleft;
}

void CCrystalTextView::
InvalidateLines (int nLine1, int nLine2, bool bInvalidateMargin /*= false*/ )
{
  bInvalidateMargin = true;
  const int nTopMarginHeight = GetTopMarginHeight ();
  const int nLineHeight = GetLineHeight();
  if (nLine2 == -1)
    {
      CRect rcInvalid;
      GetClientRect (&rcInvalid);
      if (!bInvalidateMargin)
        rcInvalid.left += GetMarginWidth ();
      //BEGIN SW
      rcInvalid.top = (GetSubLineIndex( nLine1 ) - m_nTopSubLine) * nLineHeight + nTopMarginHeight;
      /*ORIGINAL
      rcInvalid.top = (nLine1 - m_nTopLine) * GetLineHeight();
      */
      //END SW
      InvalidateRect (&rcInvalid, false);
    }
  else
    {
      if (nLine2 < nLine1)
        {
          int nTemp = nLine1;
          nLine1 = nLine2;
          nLine2 = nTemp;
        }
      CRect rcInvalid;
      GetClientRect (&rcInvalid);
      if (!bInvalidateMargin)
        rcInvalid.left += GetMarginWidth ();
      //BEGIN SW
      rcInvalid.top = (GetSubLineIndex( nLine1 ) - m_nTopSubLine) * nLineHeight + nTopMarginHeight;
      rcInvalid.bottom = (GetSubLineIndex( nLine2 ) - m_nTopSubLine + GetSubLines( nLine2 )) * nLineHeight + nTopMarginHeight;
      /*ORIGINAL
      rcInvalid.top = (nLine1 - m_nTopLine) * GetLineHeight();
      rcInvalid.bottom = (nLine2 - m_nTopLine + 1) * GetLineHeight();
      */
      //END SW
      InvalidateRect (&rcInvalid, false);
    }
}

void CCrystalTextView::
SetSelection (const CPoint & ptStart, const CPoint & ptEnd, bool bUpdateView /* = true */)
{
  ASSERT_VALIDTEXTPOS (ptStart);
  ASSERT_VALIDTEXTPOS (ptEnd);
  if (m_ptSelStart == ptStart && !m_bRectangularSelection)
    {
      if (m_ptSelEnd != ptEnd)
        InvalidateLines (ptEnd.y, m_ptSelEnd.y);
    }
  else
    {
      InvalidateLines (ptStart.y, ptEnd.y);
      InvalidateLines (m_ptSelStart.y, m_ptSelEnd.y);
    }
  m_ptSelStart = ptStart;
  m_ptSelEnd = ptEnd;
}

void CCrystalTextView::
AdjustTextPoint (CPoint & point)
{
  point.x += GetCharWidth () / 2;   //todo

}

void CCrystalTextView::
OnSetFocus (CWnd * pOldWnd)
{
  __super::OnSetFocus (pOldWnd);

  m_bFocused = true;
  if (m_ptSelStart != m_ptSelEnd)
    InvalidateLines (m_ptSelStart.y, m_ptSelEnd.y);
  UpdateCaret ();
}

DWORD CCrystalTextView::
ParseLine (DWORD dwCookie, const TCHAR *pszChars, int nLength, TEXTBLOCK * pBuf, int &nActualItems)
{
  return m_CurSourceDef->ParseLineX (dwCookie, pszChars, nLength, pBuf, nActualItems);
}

int CCrystalTextView::
CalculateActualOffset (int nLineIndex, int nCharIndex, bool bAccumulate)
{
  const int nLength = GetLineLength (nLineIndex);
  ASSERT (nCharIndex >= 0 && nCharIndex <= nLength);
  LPCTSTR pszChars = GetLineChars (nLineIndex);
  int nOffset = 0;
  const int nTabSize = GetTabSize ();
  auto pIterChar = ICUBreakIterator::getCharacterBreakIterator(pszChars, nCharIndex);
  int I=0;
  switch (GetTextLayoutMode ())
    {
      case TEXTLAYOUT_TABLE_NOWORDWRAP:
        {
          int nColumnCount = m_pTextBuffer->GetColumnCount (nLineIndex);
          int nColumnTotalWidth = 0;
          int nColumn = 0;
          bool bInQuote = false;
          const int sep = m_pTextBuffer->GetFieldDelimiter ();
          const int quote = m_pTextBuffer->GetFieldEnclosure ();
          for (I = 0; I < nCharIndex; I = pIterChar->next())
            {
              if (!bInQuote && pszChars[I] == sep)
                {
                  nColumnTotalWidth += m_pTextBuffer->GetColumnWidth (nColumn++);
                  nOffset = nColumnTotalWidth;
                }
              else
                {
                  if (pszChars[I] == quote)
                    bInQuote = !bInQuote;
                  else if (pszChars[I] == '\t')
                    nOffset ++;
                  else
                    nOffset += GetCharCellCountFromChar (pszChars + I);
                  if (nColumn < nColumnCount && nOffset > nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn))
                    nOffset = nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn);
                }
            }
          return nOffset;
        }
        break;
      case TEXTLAYOUT_TABLE_WORDWRAP:
        {
          int nColumnIndex = 0;
          CPoint charPoint;
          int nSubLineStart = CharPosToPoint( nLineIndex, nCharIndex, charPoint, &nColumnIndex );
          for (int i = 0; i < nColumnIndex; ++i)
              nOffset += m_pTextBuffer->GetColumnWidth (i);
          for (int nIndex = 0; nIndex < nCharIndex; nIndex = pIterChar->next())
            {
              if( nIndex >= nSubLineStart )
                {
                  if (pszChars[nIndex] == '\t')
                    nOffset ++;
                  else
                    nOffset += GetCharCellCountFromChar (pszChars + nIndex);
                }
            }
          return nOffset;
        }
        break;
      default:
        {
          //BEGIN SW
          vector<int>   anBreaks(nLength + 1);
          int			nBreaks = 0;
 
          /*if( nLength > GetScreenChars() )*/
          WrapLineCached( nLineIndex, GetScreenChars(), &anBreaks, nBreaks );
 
          int	nPreOffset = 0;
          int	nPreBreak = 0;
 
          if( nBreaks > 0 )
            {
              int J=0;
              for( J = nBreaks - 1; J >= 0 && nCharIndex < anBreaks[J]; J-- );
              nPreBreak = (J >= 0) ? anBreaks[J] : 0;
            }
          //END SW
          for (I = 0; I < nCharIndex; I = pIterChar->next())
            {
              //BEGIN SW
              if( nPreBreak == I && nBreaks )
              nPreOffset = nOffset;
              //END SW
            if (pszChars[I] == _T ('\t'))
                nOffset += (nTabSize - nOffset % nTabSize);
              else
                nOffset += GetCharCellCountFromChar(pszChars + I);
            }
          if (bAccumulate)
            return nOffset;
          //BEGIN SW
          if( nPreBreak == I && nBreaks > 0)
            return 0;
          else
            return nOffset - nPreOffset;
          /*ORIGINAL
          return nOffset;
          *///END SW
        }
    }
}

int CCrystalTextView::
ApproxActualOffset (int nLineIndex, int nOffset)
{
  if (nOffset == 0)
    return 0;

  int nLength = GetLineLength (nLineIndex);
  LPCTSTR pszChars = GetLineChars (nLineIndex);
  int nCurrentOffset = 0;
  int nTabSize = GetTabSize ();
  auto pIterChar = ICUBreakIterator::getCharacterBreakIterator(pszChars, nLength);
  switch (GetTextLayoutMode ())
    {
      case TEXTLAYOUT_TABLE_NOWORDWRAP:
      case TEXTLAYOUT_TABLE_WORDWRAP:
        {
          int nColumnCount = m_pTextBuffer->GetColumnCount (nLineIndex);
          int nColumnTotalWidth = 0;
          bool bInQuote = false;
          const int sep = m_pTextBuffer->GetFieldDelimiter ();
          const int quote = m_pTextBuffer->GetFieldEnclosure ();
          for (int I = 0, nColumn = 0; I < nLength; I = pIterChar->next())
            {
              if (!bInQuote && pszChars[I] ==sep)
                {
                  nColumnTotalWidth += m_pTextBuffer->GetColumnWidth (nColumn++);
                  nCurrentOffset = nColumnTotalWidth;
                }
              else
                {
                  if (pszChars[I] == quote)
                    bInQuote = !bInQuote;
                  if (pszChars[I] == '\t')
                    nCurrentOffset ++;
                  else
                    nCurrentOffset += GetCharCellCountFromChar (pszChars + I);
                  if (nColumn < nColumnCount && nCurrentOffset > nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn))
                    nCurrentOffset = nColumnTotalWidth + m_pTextBuffer->GetColumnWidth (nColumn);
                }
              if (nCurrentOffset >= nOffset)
                {
                  if (nOffset <= nCurrentOffset - nTabSize / 2)
                    return I;
                  return pIterChar->next ();
                }
            }
        }
        break;
      default:
        {
          for (int I = 0; I < nLength; I = pIterChar->next())
            {
              if (pszChars[I] == _T ('\t'))
                nCurrentOffset += (nTabSize - nCurrentOffset % nTabSize);
              else
                {
                  nCurrentOffset += GetCharCellCountFromChar(pszChars + I);
                }
              if (nCurrentOffset >= nOffset)
                {
                  if (nOffset <= nCurrentOffset - nTabSize / 2)
                    return I;
                  return pIterChar->next();
                }
            }
        }
    }
  return nLength;
}

void CCrystalTextView::
EnsureVisible (CPoint pt)
{
  EnsureVisible(pt, pt);
}

void CCrystalTextView::
OnKillFocus (CWnd * pNewWnd)
{
  __super::OnKillFocus (pNewWnd);

  m_bFocused = false;
  UpdateCaret ();
  if (m_ptSelStart != m_ptSelEnd)
    InvalidateLines (m_ptSelStart.y, m_ptSelEnd.y);
  if (m_bDragSelection)
    {
      ReleaseCapture ();
      KillTimer (m_nDragSelTimer);
      m_bDragSelection = false;
    }
}

void CCrystalTextView::
OnSysColorChange ()
{
  __super::OnSysColorChange ();
  Invalidate ();
}

void CCrystalTextView::
GetText (const CPoint & ptStart, const CPoint & ptEnd, CString & text, bool bExcludeInvisibleLines /*= true*/)
{
  if (m_pTextBuffer != nullptr)
    m_pTextBuffer->GetText (ptStart.y, ptStart.x, ptEnd.y, ptEnd.x, text);
  else
    text = _T ("");
}

void CCrystalTextView::
GetTextInColumnSelection (CString & text, bool bExcludeInvisibleLines /*= true*/)
{
  if (m_pTextBuffer == nullptr)
    {
      text = _T ("");
      return;
    }

  PrepareSelBounds ();

  CString sEol = m_pTextBuffer->GetStringEol (CRLF_STYLE_DOS);

  int nBufSize = 1;
  for (int L = m_ptDrawSelStart.y; L <= m_ptDrawSelEnd.y; L++)
    nBufSize += GetLineLength (L) + sEol.GetLength ();
  LPTSTR pszBuf = text.GetBuffer (nBufSize);

  for (int I = m_ptDrawSelStart.y; I <= m_ptDrawSelEnd.y; I++)
    {
      if (bExcludeInvisibleLines && (GetLineFlags (I) & LF_INVISIBLE))
        continue;
      int nSelLeft, nSelRight;
      GetColumnSelection (I, nSelLeft, nSelRight);
      memcpy (pszBuf, GetLineChars (I) + nSelLeft, sizeof (TCHAR) * (nSelRight - nSelLeft));
      pszBuf += (nSelRight - nSelLeft);
      memcpy (pszBuf, sEol, sizeof (TCHAR) * sEol.GetLength ());
      pszBuf += sEol.GetLength ();
    }
  pszBuf[0] = 0;
  text.ReleaseBuffer ();
  text.FreeExtra ();
}

void CCrystalTextView::
UpdateView (CCrystalTextView * pSource, CUpdateContext * pContext,
            DWORD dwFlags, int nLineIndex /*= -1*/ )
{
  // SetTextType (GetExt (GetDocument ()->GetPathName ()));
  if (dwFlags & UPDATE_RESET)
    {
      ResetView ();
      RecalcVertScrollBar ();
      RecalcHorzScrollBar ();
      return;
    }

  int nLineCount = GetLineCount ();
  ASSERT (nLineCount > 0);
  ASSERT (nLineIndex >= -1 && nLineIndex < nLineCount);
  if ((dwFlags & UPDATE_SINGLELINE) != 0)
    {
      ASSERT (nLineIndex != -1);
      //  All text below this line should be reparsed
      const int cookiesSize = (int) m_ParseCookies->size();
      if (cookiesSize > 0)
        {
          ASSERT (cookiesSize == nLineCount);
          // must be reinitialized to invalid value (DWORD) - 1
          for (int i = nLineIndex; i < cookiesSize; ++i)
            (*m_ParseCookies)[i] = static_cast<DWORD>(-1);
        }
      //  This line'th actual length must be recalculated
      if (m_pnActualLineLength->size())
        {
          ASSERT (m_pnActualLineLength->size() == static_cast<size_t>(nLineCount));
          // must be initialized to invalid code -1
          (*m_pnActualLineLength)[nLineIndex] = -1;
          //BEGIN SW
          InvalidateLineCache( nLineIndex, nLineIndex );
          //END SW
        }
      //  Repaint the lines
      InvalidateLines (nLineIndex, -1, true);
    }
  else
    {
      if (m_bViewLineNumbers)
        // if enabling linenumber, we must invalidate all line-cache in visible area because selection margin width changes dynamically.
        nLineIndex = m_nTopLine < nLineIndex ? m_nTopLine : nLineIndex;

      if (nLineIndex == -1)
        nLineIndex = 0;         //  Refresh all text

      //  All text below this line should be reparsed
      if (m_ParseCookies->size())
        {
          size_t arrSize = m_ParseCookies->size();
          if (arrSize != static_cast<size_t>(nLineCount))
            {
              size_t oldsize = arrSize; 
              m_ParseCookies->resize(nLineCount);
              arrSize = nLineCount;
              // must be initialized to invalid value (DWORD) - 1
              for (size_t i = oldsize; i < arrSize; ++i)
                (*m_ParseCookies)[i] = static_cast<DWORD>(-1);
            }
          for (size_t i = nLineIndex; i < arrSize; ++i)
            (*m_ParseCookies)[i] = static_cast<DWORD>(-1);
        }

      //  Recalculate actual length for all lines below this
      if (m_pnActualLineLength->size())
        {
          size_t arrsize = m_pnActualLineLength->size();
          if (arrsize != static_cast<size_t>(nLineCount))
            {
              //  Reallocate actual length array
              size_t oldsize = arrsize; 
              m_pnActualLineLength->resize(nLineCount);
              arrsize = nLineCount;
              // must be initialized to invalid code -1
              for (size_t i = oldsize; i < arrsize; ++i)
                (*m_pnActualLineLength)[i] = -1;
            }
          for (size_t i = nLineIndex; i < arrsize; ++i)
            (*m_pnActualLineLength)[i] = -1;
        }
      //BEGIN SW
      InvalidateLineCache( nLineIndex, -1 );
      //END SW
      //  Repaint the lines
      InvalidateLines (nLineIndex, -1, true);
    }

  //  All those points must be recalculated and validated
  if (pContext != nullptr)
    {
      pContext->RecalcPoint (m_ptCursorPos);
      pContext->RecalcPoint (m_ptSelStart);
      pContext->RecalcPoint (m_ptSelEnd);
      pContext->RecalcPoint (m_ptAnchor);
      ASSERT_VALIDTEXTPOS (m_ptCursorPos);
      ASSERT_VALIDTEXTPOS (m_ptSelStart);
      ASSERT_VALIDTEXTPOS (m_ptSelEnd);
      ASSERT_VALIDTEXTPOS (m_ptAnchor);
      if (m_bDraggingText)
        {
          pContext->RecalcPoint (m_ptDraggedTextBegin);
          pContext->RecalcPoint (m_ptDraggedTextEnd);
          ASSERT_VALIDTEXTPOS (m_ptDraggedTextBegin);
          ASSERT_VALIDTEXTPOS (m_ptDraggedTextEnd);
        }
      CPoint ptTopLine (0, m_nTopLine);
      pContext->RecalcPoint (ptTopLine);
      ASSERT_VALIDTEXTPOS (ptTopLine);
      m_nTopLine = ptTopLine.y;
      UpdateCaret ();
    }

  //  Recalculate vertical scrollbar, if needed
  if ((dwFlags & UPDATE_VERTRANGE) != 0)
    {
      if (!m_bVertScrollBarLocked)
        RecalcVertScrollBar ();
    }

  //  Recalculate horizontal scrollbar, if needed
  if ((dwFlags & UPDATE_HORZRANGE) != 0)
    {
      if (!m_bHorzScrollBarLocked)
        RecalcHorzScrollBar ();
    }
}

HINSTANCE CCrystalTextView::
GetResourceHandle ()
{
#ifdef CRYSEDIT_RES_HANDLE
  return CRYSEDIT_RES_HANDLE;
#else
  if (s_hResourceInst != nullptr)
    return s_hResourceInst;
  return AfxGetResourceHandle ();
#endif
}

int CCrystalTextView::
OnCreate (LPCREATESTRUCT lpCreateStruct)
{
  m_lfBaseFont = {};
  _tcscpy_s (m_lfBaseFont.lfFaceName, _T ("FixedSys"));
  m_lfBaseFont.lfHeight = 0;
  m_lfBaseFont.lfWeight = FW_NORMAL;
  m_lfBaseFont.lfItalic = false;
  m_lfBaseFont.lfCharSet = DEFAULT_CHARSET;
  m_lfBaseFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
  m_lfBaseFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  m_lfBaseFont.lfQuality = DEFAULT_QUALITY;
  m_lfBaseFont.lfPitchAndFamily = DEFAULT_PITCH;

  if (__super::OnCreate (lpCreateStruct) == -1)
    return -1;

  ASSERT (m_hAccel == nullptr);
             // vvv GetResourceHandle () ???
  HINSTANCE hInst = AfxFindResourceHandle (MAKEINTRESOURCE(IDR_DEFAULT_ACCEL), RT_ACCELERATOR);
  ASSERT (hInst != nullptr);
  m_hAccel =::LoadAccelerators (hInst, MAKEINTRESOURCE (IDR_DEFAULT_ACCEL));
  ASSERT (m_hAccel != nullptr);
  return 0;
}

void CCrystalTextView::
SetAnchor (const CPoint & ptNewAnchor)
{
  ASSERT_VALIDTEXTPOS (ptNewAnchor);
  m_ptAnchor = ptNewAnchor;
}

void CCrystalTextView::
OnEditOperation (int nAction, LPCTSTR pszText, size_t cchText)
{
}

BOOL CCrystalTextView::
PreTranslateMessage (MSG * pMsg)
{
  if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
    {
      if (m_hAccel != nullptr)
        {
          if (::TranslateAccelerator (m_hWnd, m_hAccel, pMsg))
            return true;
        }
    }
  else if (pMsg->message == WM_LBUTTONDBLCLK)
    m_dwLastDblClickTime = GetTickCount();
  else if (pMsg->message == WM_LBUTTONDOWN && (GetTickCount() - GetDoubleClickTime()) < m_dwLastDblClickTime)
    {
      m_dwLastDblClickTime = 0;
      OnLButtonTrippleClk(static_cast<UINT>(pMsg->wParam), { GET_X_LPARAM(pMsg->lParam), GET_Y_LPARAM(pMsg->lParam) });
      return true;
    }
  return __super::PreTranslateMessage (pMsg);
}

void CCrystalTextView::
SetCursorPos (const CPoint & ptCursorPos)
{
  ASSERT_VALIDTEXTPOS (ptCursorPos);
  m_ptCursorPos = ptCursorPos;
  m_nIdealCharPos = CalculateActualOffset (m_ptCursorPos.y, m_ptCursorPos.x);
  UpdateCaret ();
}

void CCrystalTextView::
UpdateCompositionWindowPos() /* IME */
{
  HIMC hIMC = ImmGetContext(m_hWnd);
  COMPOSITIONFORM compform;

  compform.dwStyle = CFS_FORCE_POSITION;
  compform.ptCurrentPos = GetCaretPos();
  ImmSetCompositionWindow(hIMC, &compform);

  ImmReleaseContext(m_hWnd, hIMC);
}

void CCrystalTextView::
UpdateCompositionWindowFont() /* IME */
{
  HIMC hIMC = ImmGetContext(m_hWnd);

  ImmSetCompositionFont(hIMC, &m_lfBaseFont);

  ImmReleaseContext(m_hWnd, hIMC);
}

void CCrystalTextView::
SetTopMargin (bool bTopMargin)
{
  if (m_bTopMargin != bTopMargin)
    {
      m_bTopMargin = bTopMargin;
      if (::IsWindow (m_hWnd))
        {
          Invalidate ();
          m_nScreenLines = -1;
          RecalcVertScrollBar ();
          UpdateCaret ();
        }
    }
}

void CCrystalTextView::
SetSelectionMargin (bool bSelMargin)
{
  if (m_bSelMargin != bSelMargin)
    {
      m_bSelMargin = bSelMargin;
      if (::IsWindow (m_hWnd))
        {
          InvalidateScreenRect ();
          m_nTopSubLine = GetSubLineIndex(m_nTopLine);
          RecalcHorzScrollBar ();
          UpdateCaret ();
        }
    }
}

void CCrystalTextView::
SetViewLineNumbers (bool bViewLineNumbers)
{
  if (m_bViewLineNumbers != bViewLineNumbers)
    {
      m_bViewLineNumbers = bViewLineNumbers;
      if (::IsWindow (m_hWnd))
        {
          InvalidateScreenRect ();
          m_nTopSubLine = GetSubLineIndex(m_nTopLine);
          RecalcHorzScrollBar ();
          UpdateCaret ();
        }
    }
}

void CCrystalTextView::
SetFont (const LOGFONT & lf)
{
  m_lfBaseFont = lf;
  m_nCharWidth = -1;
  m_nLineHeight = -1;
  m_pCrystalRenderer->SetFont(lf);
  if (::IsWindow (m_hWnd))
    {
      InvalidateScreenRect();
      m_nTopSubLine = GetSubLineIndex(m_nTopLine);
      RecalcVertScrollBar ();
      RecalcHorzScrollBar ();
      UpdateCaret ();
    }
#ifdef _UNICODE
  ResetCharWidths();
#endif
}

void CCrystalTextView::
OnUpdateIndicatorPosition (CCmdUI * pCmdUI)
{
  ASSERT_VALIDTEXTPOS (m_ptCursorPos);
  CString stat;
                                                   // VVV m_ptCursorPos.x + 1 ???
  stat.Format (_T ("Ln %d, Col %d"), m_ptCursorPos.y + 1, m_nIdealCharPos + 1);
  pCmdUI->SetText (stat);
  //BEGIN SW
  if( pCmdUI->m_pOther != nullptr && !!pCmdUI->m_pOther->IsKindOf( RUNTIME_CLASS(CStatusBar) ) )
    OnUpdateStatusMessage( (CStatusBar*)pCmdUI->m_pOther );
  //END SW
}

void CCrystalTextView::
OnUpdateIndicatorCRLF (CCmdUI * pCmdUI)
{
  if (m_pTextBuffer != nullptr)
    {
      std::basic_string<TCHAR> eol;
      CRLFSTYLE crlfMode = m_pTextBuffer->GetCRLFMode ();
      switch (crlfMode)
        {
        case CRLF_STYLE_DOS:
          eol = LoadResString (IDS_EOL_DOS);
          pCmdUI->SetText (eol.c_str());
          pCmdUI->Enable (true);
          break;
        case CRLF_STYLE_UNIX:
          eol = LoadResString (IDS_EOL_UNIX);
          pCmdUI->SetText (eol.c_str());
          pCmdUI->Enable (true);
          break;
        case CRLF_STYLE_MAC:
          eol = LoadResString (IDS_EOL_MAC);
          pCmdUI->SetText (eol.c_str());
          pCmdUI->Enable (true);
          break;
        case CRLF_STYLE_MIXED:
          eol = LoadResString (IDS_EOL_MIXED);
          pCmdUI->SetText (eol.c_str());
          pCmdUI->Enable (true);
          break;
        default:
          pCmdUI->SetText (nullptr);
          pCmdUI->Enable (false);
        }
    }
  else
    {
      pCmdUI->SetText (nullptr);
      pCmdUI->Enable (false);
    }
}

void CCrystalTextView::
OnToggleBookmark (UINT nCmdID)
{
  int nBookmarkID = nCmdID - ID_EDIT_TOGGLE_BOOKMARK0;
  ASSERT (nBookmarkID >= 0 && nBookmarkID <= 9);
  if (m_pTextBuffer != nullptr)
    {
      DWORD dwFlags = GetLineFlags (m_ptCursorPos.y);
      DWORD dwMask = LF_BOOKMARK (nBookmarkID);
      m_pTextBuffer->SetLineFlag (m_ptCursorPos.y, dwMask, (dwFlags & dwMask) == 0);
    }
}

void CCrystalTextView::
OnGoBookmark (UINT nCmdID)
{
  int nBookmarkID = nCmdID - ID_EDIT_GO_BOOKMARK0;
  ASSERT (nBookmarkID >= 0 && nBookmarkID <= 9);
  if (m_pTextBuffer != nullptr)
    {
      int nLine = m_pTextBuffer->GetLineWithFlag (LF_BOOKMARK (nBookmarkID));
      if (nLine >= 0)
        {
          CPoint pt (0, nLine);
          ASSERT_VALIDTEXTPOS (pt);
          SetCursorPos (pt);
          SetSelection (pt, pt);
          SetAnchor (pt);
          EnsureVisible (pt);
        }
    }
}

void CCrystalTextView::
OnClearBookmarks ()
{
  if (m_pTextBuffer != nullptr)
    {
      for (int nBookmarkID = 0; nBookmarkID <= 9; nBookmarkID++)
        {
          int nLine = m_pTextBuffer->GetLineWithFlag (LF_BOOKMARK (nBookmarkID));
          if (nLine >= 0)
            {
              m_pTextBuffer->SetLineFlag (nLine, LF_BOOKMARK (nBookmarkID), false);
            }
        }

    }
}

void CCrystalTextView::
ShowCursor ()
{
  m_bCursorHidden = false;
  UpdateCaret ();
}

void CCrystalTextView::
HideCursor ()
{
  m_bCursorHidden = true;
  UpdateCaret ();
}

void CCrystalTextView::
OnDropSource (DROPEFFECT de)
{
  ASSERT (de == DROPEFFECT_COPY);
}

HGLOBAL CCrystalTextView::
PrepareDragData ()
{
  PrepareSelBounds ();
  if (m_ptDrawSelStart == m_ptDrawSelEnd)
    return nullptr;

  CString text;
  GetText (m_ptDrawSelStart, m_ptDrawSelEnd, text);
  int cchText = text.GetLength();
  SIZE_T cbData = (cchText + 1) * sizeof(TCHAR);
  HGLOBAL hData =::GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE, cbData);
  if (hData == nullptr)
    return nullptr;

  LPTSTR pszData = (LPTSTR)::GlobalLock (hData);
  if (pszData == nullptr)
    {
      ::GlobalFree(hData);
      return nullptr;
    }
  memcpy (pszData, text, cbData);
  ::GlobalUnlock (hData);

  m_ptDraggedTextBegin = m_ptDrawSelStart;
  m_ptDraggedTextEnd = m_ptDrawSelEnd;
  return hData;
}

static const TCHAR *memstr(const TCHAR *str1, size_t str1len, const TCHAR *str2, size_t str2len)
{
  ASSERT(str1 && str2 && str2len > 0);
  for (const TCHAR *p = str1; p < str1 + str1len; ++p)
    {
      if (*p == *str2)
        {
          if (memcmp(p, str2, str2len * sizeof(TCHAR)) == 0)
            return p;
        }
    }
  return nullptr;
}

static const TCHAR *memistr(const TCHAR *str1, size_t str1len, const TCHAR *str2, size_t str2len)
{
  ASSERT(str1 && str2 && str2len > 0);
  for (const TCHAR *p = str1; p < str1 + str1len; ++p)
    {
      if (toupper(*p) == toupper(*str2))
        {
          size_t i;
          for (i = 0; i < str2len; ++i)
            {
              if (toupper(p[i]) != toupper(str2[i]))
                break;
            }
          if (i == str2len)
            return p;
        }
    }
  return nullptr;
}

static ptrdiff_t
FindStringHelper (LPCTSTR pszLineBegin, size_t nLineLength, LPCTSTR pszFindWhere, LPCTSTR pszFindWhat, DWORD dwFlags, int &nLen, RxNode *&rxnode, RxMatchRes *rxmatch)
{
  if (dwFlags & FIND_REGEXP)
    {
      ptrdiff_t pos = -1;

      if (rxnode)
        RxFree (rxnode);
      rxnode = nullptr;
      if (pszFindWhat[0] == '^' && pszLineBegin != pszFindWhere)
        return pos;
      rxnode = RxCompile (pszFindWhat, (dwFlags & FIND_MATCH_CASE) != 0 ? RX_CASE : 0);
      if (rxnode && RxExec (rxnode, pszFindWhere, nLineLength - (pszFindWhere - pszLineBegin), pszFindWhere, rxmatch))
        {
          pos = rxmatch->Open[0];
          ASSERT((rxmatch->Close[0] - rxmatch->Open[0]) < INT_MAX);
          nLen = static_cast<int>(rxmatch->Close[0] - rxmatch->Open[0]);
        }
      return pos;
    }
  else
    {
      ASSERT (pszFindWhere != nullptr);
      ASSERT (pszFindWhat != nullptr);
      int nCur = 0;
      int nLength = (int) _tcslen (pszFindWhat);
      LPCTSTR pszFindWhereOrig = pszFindWhere;
      nLen = nLength;
      for (;;)
        {
          LPCTSTR pszPos;
          if (dwFlags & FIND_MATCH_CASE)
            pszPos = memstr(pszFindWhere,  nLineLength - (pszFindWhere - pszLineBegin), pszFindWhat, nLength);
          else
            pszPos = memistr(pszFindWhere, nLineLength - (pszFindWhere - pszLineBegin), pszFindWhat, nLength);
          if (pszPos == nullptr)
            return -1;
          if ((dwFlags & FIND_WHOLE_WORD) == 0)
            return nCur + (int) (pszPos - pszFindWhere);
          if (pszPos > pszFindWhereOrig && xisalnum (pszPos[-1]))
            {
              nCur += (int) (pszPos - pszFindWhere + 1);
              pszFindWhere = pszPos + 1;
              continue;
            }
          if (xisalnum (pszPos[nLength]))
            {
              nCur += (int) (pszPos - pszFindWhere + 1);
              pszFindWhere = pszPos + 1;
              continue;
            }
          return nCur + (int) (pszPos - pszFindWhere);
        }
    }
//~  ASSERT (false);               // Unreachable
}

/** 
 * @brief Select text in editor.
 * @param [in] ptStartPos Star position for highlight.
 * @param [in] nLength Count of characters to highlight.
 * @param [in] bCursorToLeft If true cursor is positioned to Left-end of text
 *  selection, if false cursor is positioned to right-end.
 */
bool CCrystalTextView::
HighlightText (const CPoint & ptStartPos, int nLength,
    bool bCursorToLeft /*= false*/)
{
  ASSERT_VALIDTEXTPOS (ptStartPos);
  CPoint ptEndPos = ptStartPos;
  int nCount = GetLineLength (ptEndPos.y) - ptEndPos.x;
  if (nLength <= nCount)
    {
      ptEndPos.x += nLength;
    }
  else
    {
      while (nLength > nCount)
        {
          nLength -= nCount + 1;
          nCount = GetLineLength (++ptEndPos.y);
        }
      ptEndPos.x = nLength;
    }
  ASSERT_VALIDTEXTPOS (m_ptCursorPos);  //  Probably 'nLength' is bigger than expected...

  m_ptCursorPos = bCursorToLeft ? ptStartPos : ptEndPos;
  m_ptAnchor = bCursorToLeft ? ptEndPos : ptStartPos;
  SetSelection (ptStartPos, ptEndPos);
  UpdateCaret ();
  
  // Scrolls found text to middle of screen if out-of-screen
  int nScreenLines = GetScreenLines();
  if (ptStartPos.y < m_nTopLine || ptEndPos.y > m_nTopLine + nScreenLines)
    {
      if (ptStartPos.y > nScreenLines / 2)
        ScrollToLine(ptStartPos.y - nScreenLines / 2);
      else
        ScrollToLine(ptStartPos.y);
      UpdateSiblingScrollPos (false);
    }
  EnsureVisible (ptStartPos, ptEndPos);
  return true;
}

bool CCrystalTextView::
FindText (LPCTSTR pszText, const CPoint & ptStartPos, DWORD dwFlags,
          bool bWrapSearch, CPoint * pptFoundPos)
{
  if (m_pMarkers != nullptr)
    {
      m_pMarkers->SetMarker(_T("EDITOR_MARKER"), pszText, dwFlags, COLORINDEX_MARKERBKGND0, false);
      if (m_pMarkers->GetEnabled())
        m_pMarkers->UpdateViews();
    }
  int nLineCount = GetLineCount ();
  return FindTextInBlock (pszText, ptStartPos, CPoint (0, 0),
                          CPoint (GetLineLength (nLineCount - 1), nLineCount - 1),
                          dwFlags, bWrapSearch, pptFoundPos);
}

int HowManyStr (LPCTSTR s, LPCTSTR m)
{
  LPCTSTR p = s;
  int n = 0;
  const int l = (int) _tcslen (m);
  while ((p = _tcsstr (p, m)) != nullptr)
    {
      n++;
      p += l;
    }
  return n;
}

int HowManyStr (LPCTSTR s, TCHAR c)
{
  LPCTSTR p = s;
  int n = 0;
  while ((p = _tcschr (p, c)) != nullptr)
    {
      n++;
      p++;
    }
  return n;
}

bool CCrystalTextView::
FindTextInBlock (LPCTSTR pszText, const CPoint & ptStartPosition,
                 const CPoint & ptBlockBegin, const CPoint & ptBlockEnd,
                 DWORD dwFlags, bool bWrapSearch, CPoint * pptFoundPos)
{
  CPoint ptCurrentPos = ptStartPosition;

  ASSERT (pszText != nullptr && _tcslen (pszText) > 0);
  ASSERT_VALIDTEXTPOS (ptCurrentPos);
  ASSERT_VALIDTEXTPOS (ptBlockBegin);
  ASSERT_VALIDTEXTPOS (ptBlockEnd);
  ASSERT (ptBlockBegin.y < ptBlockEnd.y || ptBlockBegin.y == ptBlockEnd.y &&
          ptBlockBegin.x <= ptBlockEnd.x);
  if (ptBlockBegin == ptBlockEnd)
    return false;
  CWaitCursor waitCursor;
  if (ptCurrentPos.y < ptBlockBegin.y || ptCurrentPos.y == ptBlockBegin.y &&
        ptCurrentPos.x < ptBlockBegin.x)
    ptCurrentPos = ptBlockBegin;

  CString what = pszText;
  int nEolns;
  if (dwFlags & FIND_REGEXP)
    {
      nEolns = HowManyStr (what, _T("\\n"));
    }
  else
    {
      nEolns = 0;
    }
  if (dwFlags & FIND_DIRECTION_UP)
    {
      //  Let's check if we deal with whole text.
      //  At this point, we cannot search *up* in selection
      ASSERT (ptBlockBegin.x == 0 && ptBlockBegin.y == 0);
      ASSERT (ptBlockEnd.x == GetLineLength (GetLineCount () - 1) &&
              ptBlockEnd.y == GetLineCount () - 1);

      //  Proceed as if we have whole text search.
      for (;;)
        {
          while (ptCurrentPos.y >= 0)
            {
              int nLineLength;
              CString line;
              if (dwFlags & FIND_REGEXP)
                {
                  for (int i = 0; i <= nEolns && ptCurrentPos.y >= i; i++)
                    {
                      CString item;
                      LPCTSTR pszChars = GetLineChars (ptCurrentPos.y - i);
                      if (i)
                        {
                          nLineLength = GetLineLength (ptCurrentPos.y - i);
                          ptCurrentPos.x = 0;
                          line = _T ('\n') + line;
                        }
                      else
                        {
                          nLineLength = ptCurrentPos.x != -1 ? ptCurrentPos.x : GetLineLength (ptCurrentPos.y - i);
                        }
                      if (nLineLength > 0)
                        {
                          item.SetString(pszChars, nLineLength);
                          line = item + line;
                        }
                    }
                  nLineLength = line.GetLength ();
                  if (ptCurrentPos.x == -1)
                    ptCurrentPos.x = 0;
                }
              else
                {
                  nLineLength = GetLineLength(ptCurrentPos.y);
                  if (ptCurrentPos.x == -1)
                    {
                      ptCurrentPos.x = nLineLength;
                    }
                  else if( ptCurrentPos.x > nLineLength )
                    ptCurrentPos.x = nLineLength;
                  if (ptCurrentPos.x == -1)
                    ptCurrentPos.x = 0;

                  line.SetString (GetLineChars (ptCurrentPos.y), ptCurrentPos.x);
                }

              ptrdiff_t nFoundPos = -1;
              int nMatchLen = what.GetLength();
              int nLineLen = line.GetLength();
              size_t nPos = 0;
              for (;;)
                {
                  size_t nPosRel = ::FindStringHelper(line, nLineLen, static_cast<LPCTSTR>(line) + nPos, what, dwFlags, m_nLastFindWhatLen, m_rxnode, &m_rxmatch);
                  if (nPosRel == -1)
                    break;
                  nFoundPos = nPos + nPosRel;
                  nMatchLen = m_nLastFindWhatLen;
                  nPos += nMatchLen == 0 ? 1 : nMatchLen;
                }

              if( nFoundPos != -1 )	// Found text!
                {
                  ptCurrentPos.x = static_cast<int>(nFoundPos);
                  *pptFoundPos = ptCurrentPos;
                  return true;
                }

              ptCurrentPos.y--;
              if( ptCurrentPos.y >= 0 )
                ptCurrentPos.x = GetLineLength( ptCurrentPos.y );
            }

          //  Beginning of text reached
          if (!bWrapSearch)
            return false;

          //  Start again from the end of text
          bWrapSearch = false;
          ptCurrentPos = CPoint (GetLineLength (GetLineCount () - 1), GetLineCount () - 1);
        }
    }
  else
    {
      for (;;)
        {
          while (ptCurrentPos.y <= ptBlockEnd.y)
            {
              int nLineLength;
              CString line;
              if (dwFlags & FIND_REGEXP)
                {
                  int nLines = m_pTextBuffer->GetLineCount ();
                  for (int i = 0; i <= nEolns && ptCurrentPos.y + i < nLines; i++)
                    {
                      LPCTSTR pszChars = GetLineChars (ptCurrentPos.y + i);
                      nLineLength = GetLineLength (ptCurrentPos.y + i);
                      if (i)
                        {
                          line += _T ('\n');
                        }
                      if (nLineLength > 0)
                        {
                          int nLineLengthOld = line.GetLength();
                          memcpy(line.GetBufferSetLength(nLineLengthOld + nLineLength) + nLineLengthOld, pszChars, nLineLength * sizeof(TCHAR));
                        }
                    }
                  nLineLength = line.GetLength ();
                }
              else
                {
                  nLineLength = GetLineLength (ptCurrentPos.y) - ptCurrentPos.x;
                  if (nLineLength <= 0)
                    {
                      ptCurrentPos.x = 0;
                      ptCurrentPos.y++;
                      continue;
                    }

                  line.SetString(GetLineChars(ptCurrentPos.y), GetLineLength(ptCurrentPos.y));
                }

              //  Perform search in the line
              size_t nPos = ::FindStringHelper (line, line.GetLength (), static_cast<LPCTSTR>(line) + ptCurrentPos.x, what, dwFlags, m_nLastFindWhatLen, m_rxnode, &m_rxmatch);
              if (nPos != -1)
                {
                  if (m_pszMatched != nullptr)
                    free(m_pszMatched);
                  m_pszMatched = _tcsdup (line);
                  if (nEolns)
                    {
                      CString item = line.Left (static_cast<LONG>(nPos));
                      LPCTSTR current = _tcsrchr (item, _T('\n'));
                      if (current)
                        current++;
                      else
                        current = item;
                      nEolns = HowManyStr (item, _T('\n'));
                      if (nEolns)
                        {
                          ptCurrentPos.y += nEolns;
                          ptCurrentPos.x = static_cast<LONG>(nPos - (current - (LPCTSTR) item));
                        }
                      else
                        {
                          ptCurrentPos.x += static_cast<LONG>(nPos - (current - (LPCTSTR) item));
                        }
                      if (ptCurrentPos.x < 0)
                        ptCurrentPos.x = 0;
                    }
                  else
                    {
                      ptCurrentPos.x += static_cast<LONG>(nPos);
                    }
                  //  Check of the text found is outside the block.
                  if (ptCurrentPos.y == ptBlockEnd.y && ptCurrentPos.x >= ptBlockEnd.x)
                    break;

                  *pptFoundPos = ptCurrentPos;
                  return true;
                }
              else
                {
                  if (m_pszMatched != nullptr)
                    free(m_pszMatched);
                  m_pszMatched = nullptr;
                }

              //  Go further, text was not found
              ptCurrentPos.x = 0;
              ptCurrentPos.y++;
            }

          //  End of text reached
          if (!bWrapSearch)
            return false;

          //  Start from the beginning
          bWrapSearch = false;
          ptCurrentPos = ptBlockBegin;
        }
    }

  //~ ASSERT (false);               // Unreachable
}

static DWORD ConvertSearchInfosToSearchFlags(const LastSearchInfos *lastSearch)
{
  DWORD dwSearchFlags = 0;
  if (lastSearch->m_bMatchCase)
    dwSearchFlags |= FIND_MATCH_CASE;
  if (lastSearch->m_bWholeWord)
    dwSearchFlags |= FIND_WHOLE_WORD;
  if (lastSearch->m_bRegExp)
    dwSearchFlags |= FIND_REGEXP;
  if (lastSearch->m_nDirection == 0)
    dwSearchFlags |= FIND_DIRECTION_UP;
  if (lastSearch->m_bNoWrap)
    dwSearchFlags |= FIND_NO_WRAP;
  if (lastSearch->m_bNoClose)
    dwSearchFlags |= FIND_NO_CLOSE;
  return dwSearchFlags;
}

static void ConvertSearchFlagsToLastSearchInfos(LastSearchInfos *lastSearch, DWORD dwFlags)
{
  lastSearch->m_bMatchCase = (dwFlags & FIND_MATCH_CASE) != 0;
  lastSearch->m_bWholeWord = (dwFlags & FIND_WHOLE_WORD) != 0;
  lastSearch->m_bRegExp = (dwFlags & FIND_REGEXP) != 0;
  lastSearch->m_nDirection = (dwFlags & FIND_DIRECTION_UP) == 0;
  lastSearch->m_bNoWrap = (dwFlags & FIND_NO_WRAP) != 0;
  lastSearch->m_bNoClose = (dwFlags & FIND_NO_CLOSE) != 0;
}

CPoint CCrystalTextView::
GetSearchPos(DWORD dwSearchFlags)
{
  CPoint ptSearchPos;
  if (IsSelection())
    {
      CPoint ptStart, ptEnd;
      GetSelection(ptStart, ptEnd);
      if( dwSearchFlags & FIND_DIRECTION_UP)
        ptSearchPos = ptStart;
      else
        ptSearchPos = ptEnd;
    }
  else
    ptSearchPos = m_ptCursorPos;
  return ptSearchPos;
}

bool CCrystalTextView::
FindText (const LastSearchInfos * lastSearch)
{
  CPoint ptTextPos;
  DWORD dwSearchFlags = ConvertSearchInfosToSearchFlags(lastSearch);
  if (!FindText (lastSearch->m_sText, GetSearchPos(dwSearchFlags), dwSearchFlags, !lastSearch->m_bNoWrap,
      &ptTextPos))
    {
      return false;
    }

  bool bCursorToLeft = (lastSearch->m_nDirection == 0);
  HighlightText (ptTextPos, m_nLastFindWhatLen, bCursorToLeft);

  //  Save search parameters for 'F3' command
  m_bLastSearch = true;
  if (m_pszLastFindWhat != nullptr)
    free (m_pszLastFindWhat);
  m_pszLastFindWhat = _tcsdup (lastSearch->m_sText);
  m_dwLastSearchFlags = dwSearchFlags;

  //  Save search parameters to registry
  VERIFY (RegSaveNumber (HKEY_CURRENT_USER, REG_EDITPAD, _T ("FindFlags"), m_dwLastSearchFlags));

  return true;
}

void CCrystalTextView::
OnEditFind ()
{
  CWinApp *pApp = AfxGetApp ();
  ASSERT (pApp != nullptr);

  if (m_pFindTextDlg == nullptr)
    m_pFindTextDlg = new CFindTextDlg (this);

  LastSearchInfos * lastSearch = m_pFindTextDlg->GetLastSearchInfos();

  if (m_bLastSearch)
    {
      //  Get the latest search parameters
      ConvertSearchFlagsToLastSearchInfos(lastSearch, m_dwLastSearchFlags);
      if (m_pszLastFindWhat != nullptr)
        lastSearch->m_sText = m_pszLastFindWhat;
    }
  else
    {
      DWORD dwFlags;
      if (!RegLoadNumber (HKEY_CURRENT_USER, REG_EDITPAD, _T ("FindFlags"), &dwFlags))
        dwFlags = 0;
      ConvertSearchFlagsToLastSearchInfos(lastSearch, dwFlags);
    }
  m_pFindTextDlg->UseLastSearch ();

  //  Take the current selection, if any
  if (IsSelection ())
    {
      CPoint ptSelStart, ptSelEnd;
      GetSelection (ptSelStart, ptSelEnd);
      if (ptSelStart.y == ptSelEnd.y)
        GetText (ptSelStart, ptSelEnd, m_pFindTextDlg->m_sText);
    }
  else
    {
      CPoint ptCursorPos = GetCursorPos ();
      CPoint ptStart = WordToLeft (ptCursorPos);
      CPoint ptEnd = WordToRight (ptCursorPos);
      if (IsValidTextPos (ptStart) && IsValidTextPos (ptEnd) && ptStart != ptEnd)
        GetText (ptStart, ptEnd, m_pFindTextDlg->m_sText);
    }

  //  Execute Find dialog

  // m_bShowInactiveSelection = true; // FP: removed because I like it
  m_pFindTextDlg->UpdateData(FALSE);
  m_pFindTextDlg->ShowWindow(SW_SHOW);
  // m_bShowInactiveSelection = false; // FP: removed because I like it

}

void CCrystalTextView::
OnEditRepeat ()
{
  bool bEnable = m_bLastSearch;
  // Show dialog if no last find text
  if (m_pszLastFindWhat == nullptr || _tcslen(m_pszLastFindWhat) == 0)
    bEnable = false;
  CString sText;
  if (bEnable)
    sText = m_pszLastFindWhat;
  else
    {
      // If last find-text exists, cut it to first line
      bEnable = !sText.IsEmpty ();
      if (bEnable)
        {
          int pos = sText.FindOneOf (_T("\r\n"));
          if (pos >= 0)
            sText = sText.Left (pos);
        }
    }

  // CTRL-F3 will find selected text..
  bool bControlKey = (::GetAsyncKeyState(VK_CONTROL)& 0x8000) != 0;
  // CTRL-SHIFT-F3 will find selected text, but opposite direction
  bool bShiftKey = (::GetAsyncKeyState(VK_SHIFT)& 0x8000) != 0;
  if (bControlKey)
    {
      if (IsSelection())
        {
          CPoint ptSelStart, ptSelEnd;
          GetSelection (ptSelStart, ptSelEnd);
          GetText (ptSelStart, ptSelEnd, sText);
        }
      else
        {
          CPoint ptCursorPos = GetCursorPos ();
          CPoint ptStart = WordToLeft (ptCursorPos);
          CPoint ptEnd = WordToRight (ptCursorPos);
          if (IsValidTextPos (ptStart) && IsValidTextPos (ptEnd) && ptStart != ptEnd)
            GetText (ptStart, ptEnd, sText);
        }
      if (!sText.IsEmpty())
        {
          bEnable = true;
          free(m_pszLastFindWhat);
          m_pszLastFindWhat = _tcsdup (sText);
          m_bLastSearch = true;
        }
    }
  if (bShiftKey)
    m_dwLastSearchFlags |= FIND_DIRECTION_UP;
  else
    m_dwLastSearchFlags &= ~FIND_DIRECTION_UP;
  if (bEnable)
    {
      CPoint ptFoundPos;
      //BEGIN SW
      // for correct backward search we need some changes:
      if (! FindText(sText, GetSearchPos(m_dwLastSearchFlags), m_dwLastSearchFlags,
            (m_dwLastSearchFlags & FIND_NO_WRAP) == 0, &ptFoundPos))
        {
          CString prompt;
          prompt.Format (LoadResString(IDS_EDIT_TEXT_NOT_FOUND).c_str(), (LPCTSTR)sText);
          AfxMessageBox (prompt, MB_ICONINFORMATION);
          return;
        }
      HighlightText (ptFoundPos, m_nLastFindWhatLen, (m_dwLastSearchFlags & FIND_DIRECTION_UP) != 0);
      m_bMultipleSearch = true; // More search
    }
  else
    OnEditFind(); // No previous find, open Find-dialog
}

void CCrystalTextView::
OnUpdateEditRepeat (CCmdUI * pCmdUI)
{
  pCmdUI->Enable (true);
}

void CCrystalTextView::
OnEditMark ()
{
  CString sText;
  DWORD dwFlags;
  if (!RegLoadNumber (HKEY_CURRENT_USER, REG_EDITPAD, _T ("MarkerFlags"), &dwFlags))
    dwFlags = 0;

  //  Take the current selection, if any
  if (IsSelection ())
    {
      CPoint ptSelStart, ptSelEnd;
      GetSelection (ptSelStart, ptSelEnd);
      if (ptSelStart.y == ptSelEnd.y)
        GetText (ptSelStart, ptSelEnd, sText);
    }
  else
    {
      CPoint ptCursorPos = GetCursorPos ();
      CPoint ptStart = WordToLeft (ptCursorPos);
      CPoint ptEnd = WordToRight (ptCursorPos);
      if (IsValidTextPos (ptStart) && IsValidTextPos (ptEnd) && ptStart != ptEnd)
        GetText (ptStart, ptEnd, sText);
    }

  CTextMarkerDlg markerDlg(*m_pMarkers, sText, dwFlags);

  if (markerDlg.DoModal() == IDOK)
    {
      //  Save search parameters to registry
      VERIFY (RegSaveNumber (HKEY_CURRENT_USER, REG_EDITPAD, _T ("MarkerFlags"), markerDlg.GetLastSearchFlags()));
      m_pMarkers->SaveToRegistry();
    }
}

void CCrystalTextView::
OnFilePageSetup ()
{
  CWinApp *pApp = AfxGetApp ();
  ASSERT (pApp != nullptr);

  CPageSetupDialog dlg;
  PRINTDLG pd;
  if (!pApp->GetPrinterDeviceDefaults (&pd))
    return;

  dlg.m_psd.hDevMode = pd.hDevMode;
  dlg.m_psd.hDevNames = pd.hDevNames;
  dlg.m_psd.Flags |= PSD_INHUNDREDTHSOFMILLIMETERS|PSD_MARGINS;
  dlg.m_psd.rtMargin.left = DEFAULT_PRINT_MARGIN;
  dlg.m_psd.rtMargin.right = DEFAULT_PRINT_MARGIN;
  dlg.m_psd.rtMargin.top = DEFAULT_PRINT_MARGIN;
  dlg.m_psd.rtMargin.bottom = DEFAULT_PRINT_MARGIN;
  CReg reg;
  if (reg.Open (HKEY_CURRENT_USER, REG_EDITPAD, KEY_READ))
    {
      DWORD dwTemp;
      if (reg.LoadNumber (_T ("PageWidth"), &dwTemp))
        dlg.m_psd.ptPaperSize.x = dwTemp;
      if (reg.LoadNumber (_T ("PageHeight"), &dwTemp))
        dlg.m_psd.ptPaperSize.y = dwTemp;
      if (reg.LoadNumber (_T ("PageLeft"), &dwTemp))
        dlg.m_psd.rtMargin.left = dwTemp;
      if (reg.LoadNumber (_T ("PageRight"), &dwTemp))
        dlg.m_psd.rtMargin.right = dwTemp;
      if (reg.LoadNumber (_T ("PageTop"), &dwTemp))
        dlg.m_psd.rtMargin.top = dwTemp;
      if (reg.LoadNumber (_T ("PageBottom"), &dwTemp))
        dlg.m_psd.rtMargin.bottom = dwTemp;
    }
  if (dlg.DoModal () == IDOK)
    {
      CReg reg1;
      if (reg1.Create (HKEY_CURRENT_USER, REG_EDITPAD, KEY_WRITE))
        {
          VERIFY (reg1.SaveNumber (_T ("PageWidth"), dlg.m_psd.ptPaperSize.x));
          VERIFY (reg1.SaveNumber (_T ("PageHeight"), dlg.m_psd.ptPaperSize.y));
          VERIFY (reg1.SaveNumber (_T ("PageLeft"), dlg.m_psd.rtMargin.left));
          VERIFY (reg1.SaveNumber (_T ("PageRight"), dlg.m_psd.rtMargin.right));
          VERIFY (reg1.SaveNumber (_T ("PageTop"), dlg.m_psd.rtMargin.top));
          VERIFY (reg1.SaveNumber (_T ("PageBottom"), dlg.m_psd.rtMargin.bottom));
        }
      pApp->SelectPrinter (dlg.m_psd.hDevNames, dlg.m_psd.hDevMode, false);
    }
}

/** 
 * @brief Adds/removes bookmark on given line.
 * This functions adds bookmark or removes bookmark on given line.
 * @param [in] Index (0-based) of line to add/remove bookmark.
 */
void CCrystalTextView::ToggleBookmark(int nLine)
{
  ASSERT(nLine >= 0 && nLine < GetLineCount());
  if (m_pTextBuffer != nullptr)
    {
      DWORD dwFlags = GetLineFlags (nLine);
      DWORD dwMask = LF_BOOKMARKS;
      m_pTextBuffer->SetLineFlag (nLine, dwMask, (dwFlags & dwMask) == 0, false);
      const int nBookmarkLine = m_pTextBuffer->GetLineWithFlag (LF_BOOKMARKS);
      if (nBookmarkLine >= 0)
        m_bBookmarkExist = true;
      else
        m_bBookmarkExist = false;
    }  
}
/** 
 * @brief Called when Toggle Bookmark is selected from the GUI.
 */
void CCrystalTextView::
OnToggleBookmark ()
{
  ToggleBookmark(m_ptCursorPos.y);
}

void CCrystalTextView::
OnNextBookmark ()
{
  if (m_pTextBuffer != nullptr)
    {
      int nLine = m_pTextBuffer->FindNextBookmarkLine (m_ptCursorPos.y);
      if (nLine >= 0)
        {
          CPoint pt (0, nLine);
          ASSERT_VALIDTEXTPOS (pt);
          SetCursorPos (pt);
          SetSelection (pt, pt);
          SetAnchor (pt);
          EnsureVisible (pt);
        }
    }
}

void CCrystalTextView::
OnPrevBookmark ()
{
  if (m_pTextBuffer != nullptr)
    {
      int nLine = m_pTextBuffer->FindPrevBookmarkLine (m_ptCursorPos.y);
      if (nLine >= 0)
        {
          CPoint pt (0, nLine);
          ASSERT_VALIDTEXTPOS (pt);
          SetCursorPos (pt);
          SetSelection (pt, pt);
          SetAnchor (pt);
          EnsureVisible (pt);
        }
    }
}

void CCrystalTextView::
OnClearAllBookmarks ()
{
  if (m_pTextBuffer != nullptr)
    {
      int nLineCount = GetLineCount ();
      for (int I = 0; I < nLineCount; I++)
        {
          if (m_pTextBuffer->GetLineFlags (I) & LF_BOOKMARKS)
            m_pTextBuffer->SetLineFlag (I, LF_BOOKMARKS, false);
        }
      m_bBookmarkExist = false;
    }
}

void CCrystalTextView::
OnUpdateNextBookmark (CCmdUI * pCmdUI)
{
  pCmdUI->Enable (m_bBookmarkExist);
}

void CCrystalTextView::
OnUpdatePrevBookmark (CCmdUI * pCmdUI)
{
  pCmdUI->Enable (m_bBookmarkExist);
}

void CCrystalTextView::
OnUpdateClearAllBookmarks (CCmdUI * pCmdUI)
{
  pCmdUI->Enable (m_bBookmarkExist);
}

void CCrystalTextView::
SetViewTabs (bool bViewTabs)
{
  if (bViewTabs != m_bViewTabs)
    {
      m_bViewTabs = bViewTabs;
      if (::IsWindow (m_hWnd))
        Invalidate ();
    }
}

void CCrystalTextView::
SetViewEols (bool bViewEols, bool bDistinguishEols)
{
  if (bViewEols != m_bViewEols || bDistinguishEols != m_bDistinguishEols)
    {
      m_bViewEols = bViewEols;
      m_bDistinguishEols = bDistinguishEols;
      if (::IsWindow (m_hWnd))
        Invalidate ();
    }
}

void CCrystalTextView::
SetFlags (DWORD dwFlags)
{
  if (m_dwFlags != dwFlags)
    {
      m_dwFlags = dwFlags;
      if (::IsWindow (m_hWnd))
        Invalidate ();
    }
}

int CCrystalTextView::
GetTopMarginHeight()
{
  if (!m_bTopMargin)
    return 0;
  return GetLineHeight();
}

/**
 * @brief Calculate margin area width.
 * This function calculates needed margin width. Needed width is (approx.)
 * one char-width for bookmark etc markers and rest to linenumbers (if
 * visible). If we have linenumbers visible we need to adjust width so that
 * biggest number fits.
 * @return Margin area width in pixels.
 */
int CCrystalTextView::
GetMarginWidth (CDC *pdc /*= nullptr*/)
{
  int nMarginWidth = 0;

  if (m_bViewLineNumbers)
    {
      const int nLines = GetLineCount();
      int nNumbers = 0;
      int n = 1;
      for (n = 1; n <= nLines; n *= 10)
        ++nNumbers;
      nMarginWidth += GetCharWidth () * nNumbers;
      if (!m_bSelMargin)
        nMarginWidth += 2; // Small gap when symbol part disabled
    }

  if (m_bSelMargin)
    {
      if (pdc == nullptr || !pdc->IsPrinting ())
        nMarginWidth += GetMarginIconSize () + 7;  // Width for icon markers and some margin
    }
  else
    {
      if (pdc == nullptr || !pdc->IsPrinting ())
        nMarginWidth += MARGIN_REV_WIDTH; // Space for revision marks
    }

  return nMarginWidth;
}

void CCrystalTextView::CopyProperties (CCrystalTextView *pSource)
{
  m_nTopLine = pSource->m_nTopLine;
  m_nTopSubLine = pSource->m_nTopSubLine;
  m_bViewTabs = pSource->m_bViewTabs;
  m_bViewEols = pSource->m_bViewEols;
  m_bDistinguishEols = pSource->m_bDistinguishEols;
  m_bTopMargin = pSource->m_bTopMargin;
  m_bSelMargin = pSource->m_bSelMargin;
  m_bViewLineNumbers = pSource->m_bViewLineNumbers;
  m_bSmoothScroll = pSource->m_bSmoothScroll;
  m_bWordWrap = pSource->m_bWordWrap;
  m_pColors = pSource->m_pColors;
  m_pMarkers = pSource->m_pMarkers;
  m_bDisableDragAndDrop = pSource->m_bDisableDragAndDrop;
  SetTextType(pSource->m_CurSourceDef);
  SetFont (pSource->m_lfBaseFont);
}

//
// Mouse wheel event.  zDelta is in multiples of 120.
// Divide by 40 so each click is 3 lines.  I know some
// drivers let you set the ammount of scroll, but I
// don't know how to retrieve this or if they just
// adjust the zDelta you get here.
BOOL CCrystalTextView::
OnMouseWheel (UINT nFlags, short zDelta, CPoint pt)
{
  SCROLLINFO si = {0};
  si.cbSize = sizeof (si);
  si.fMask = SIF_PAGE | SIF_RANGE;
  VERIFY (GetScrollInfo (SB_VERT, &si));

  int nNewTopSubLine= m_nTopSubLine - zDelta / 40;

  if (nNewTopSubLine < 0)
    nNewTopSubLine = 0;
  if (nNewTopSubLine > (si.nMax - (signed int)si.nPage + 1))
    nNewTopSubLine = si.nMax - si.nPage + 1;

  ScrollToSubLine(nNewTopSubLine, true);
  UpdateSiblingScrollPos(false);

  return __super::OnMouseWheel (nFlags, zDelta, pt);
}

void CCrystalTextView::
OnMouseHWheel (UINT nFlags, short zDelta, CPoint pt)
{
  SCROLLINFO si = { sizeof(si) };
  si.fMask = SIF_POS | SIF_RANGE;
  VERIFY (GetScrollInfo (SB_HORZ, &si));

  int nCurPos = si.nPos + zDelta / 40;
  if (nCurPos < si.nMin)
    nCurPos = si.nMin;
  else if (nCurPos > si.nMax)
    nCurPos = si.nMax;

  ScrollToChar (nCurPos, true);
  UpdateCaret ();
  UpdateSiblingScrollPos (true);

  __super::OnMouseHWheel (nFlags, zDelta, pt);
}

void CCrystalTextView::
OnSourceType (UINT nId)
{
  SetTextType ((CrystalLineParser::TextType) (nId - ID_SOURCE_PLAIN));
  Invalidate ();
}

void CCrystalTextView::
OnUpdateSourceType (CCmdUI * pCmdUI)
{
  pCmdUI->SetRadio (CrystalLineParser::m_SourceDefs + (pCmdUI->m_nID - ID_SOURCE_PLAIN) == m_CurSourceDef);
}

int
bracetype (TCHAR c)
{
  static LPCTSTR braces = _T("{}()[]<>");
  LPCTSTR pos = _tcschr (braces, c);
  return pos != nullptr ? (int) (pos - braces) + 1 : 0;
}

int
bracetype (LPCTSTR s)
{
  if (s[1])
    return 0;
  return bracetype (*s);
}

void CCrystalTextView::
OnMatchBrace ()
{
  CPoint ptCursorPos = GetCursorPos ();
  int nLength = m_pTextBuffer->GetLineLength (ptCursorPos.y);
  LPCTSTR pszText = m_pTextBuffer->GetLineChars (ptCursorPos.y), pszEnd = pszText + ptCursorPos.x;
  bool bAfter = false;
  int nType = 0;
  if (ptCursorPos.x < nLength)
    {
      nType = bracetype (*pszEnd);
      if (nType)
        {
          bAfter = false;
        }
      else if (ptCursorPos.x > 0)
        {
          nType = bracetype (pszEnd[-1]);
          bAfter = true;
        }
    }
  else if (ptCursorPos.x > 0)
    {
      nType = bracetype (pszEnd[-1]);
      bAfter = true;
    }
  if (nType)
    {
      int nOther, nCount = 0, nComment = 0;
      if (bAfter)
        {
          nOther = ((nType - 1) ^ 1) + 1;
          if (nOther & 1)
            pszEnd--;
        }
      else
        {
          nOther = ((nType - 1) ^ 1) + 1;
          if (!(nOther & 1))
            pszEnd++;
        }
      LPCTSTR pszOpenComment = m_CurSourceDef->opencomment,
        pszCloseComment = m_CurSourceDef->closecomment,
        pszCommentLine = m_CurSourceDef->commentline, pszTest;
      int nOpenComment = (int) _tcslen (pszOpenComment),
        nCloseComment = (int) _tcslen (pszCloseComment),
        nCommentLine = (int) _tcslen (pszCommentLine);
      if (nOther & 1)
        {
          for (;;)
            {
              while (--pszEnd >= pszText)
                {
                  pszTest = pszEnd - nOpenComment + 1;
                  if (pszTest >= pszText && !_tcsnicmp (pszTest, pszOpenComment, nOpenComment))
                    {
                      nComment--;
                      pszEnd = pszTest;
                      if (--pszEnd < pszText)
                        {
                          break;
                        }
                    }
                  pszTest = pszEnd - nCloseComment + 1;
                  if (pszTest >= pszText && !_tcsnicmp (pszTest, pszCloseComment, nCloseComment))
                    {
                      nComment++;
                      pszEnd = pszTest;
                      if (--pszEnd < pszText)
                        {
                          break;
                        }
                    }
                  if (!nComment)
                    {
                      pszTest = pszEnd - nCommentLine + 1;
                      if (pszTest >= pszText && !_tcsnicmp (pszTest, pszCommentLine, nCommentLine))
                        {
                          break;
                        }
                      if (bracetype (*pszEnd) == nType)
                        {
                          nCount++;
                        }
                      else if (bracetype (*pszEnd) == nOther)
                        {
                          if (!nCount--)
                            {
                              ptCursorPos.x = (LONG) (pszEnd - pszText);
                              if (bAfter)
                                ptCursorPos.x++;
                              SetCursorPos (ptCursorPos);
                              SetSelection (ptCursorPos, ptCursorPos);
                              SetAnchor (ptCursorPos);
                              EnsureVisible (ptCursorPos);
                              return;
                            }
                        }
                    }
                }
              if (ptCursorPos.y)
                {
                  ptCursorPos.x = m_pTextBuffer->GetLineLength (--ptCursorPos.y);
                  pszText = m_pTextBuffer->GetLineChars (ptCursorPos.y);
                  pszEnd = pszText + ptCursorPos.x;
                }
              else
                break;
            }
        }
      else
        {
          LPCTSTR pszBegin = pszText;
          pszText = pszEnd;
          pszEnd = pszBegin + nLength;
          int nLines = m_pTextBuffer->GetLineCount ();
          for (;;)
            {
              while (pszText < pszEnd)
                {
                  pszTest = pszText + nCloseComment;
                  if (pszTest <= pszEnd && !_tcsnicmp (pszText, pszCloseComment, nCloseComment))
                    {
                      nComment--;
                      pszText = pszTest;
                      if (pszText > pszEnd)
                        {
                          break;
                        }
                    }
                  pszTest = pszText + nOpenComment;
                  if (pszTest <= pszEnd && !_tcsnicmp (pszText, pszOpenComment, nOpenComment))
                    {
                      nComment++;
                      pszText = pszTest;
                      if (pszText > pszEnd)
                        {
                          break;
                        }
                    }
                  if (!nComment)
                    {
                      pszTest = pszText + nCommentLine;
                      if (pszTest <= pszEnd && !_tcsnicmp (pszText, pszCommentLine, nCommentLine))
                        {
                          break;
                        }
                      if (bracetype (*pszText) == nType)
                        {
                          nCount++;
                        }
                      else if (bracetype (*pszText) == nOther)
                        {
                          if (!nCount--)
                            {
                              ptCursorPos.x = (LONG) (pszText - pszBegin);
                              if (bAfter)
                                ptCursorPos.x++;
                              SetCursorPos (ptCursorPos);
                              SetSelection (ptCursorPos, ptCursorPos);
                              SetAnchor (ptCursorPos);
                              EnsureVisible (ptCursorPos);
                              return;
                            }
                        }
                    }
                  pszText++;
                }
              if (ptCursorPos.y < nLines)
                {
                  ptCursorPos.x = 0;
                  nLength = m_pTextBuffer->GetLineLength (++ptCursorPos.y);
                  pszBegin = pszText = m_pTextBuffer->GetLineChars (ptCursorPos.y);
                  pszEnd = pszBegin + nLength;
                }
              else
                break;
            }
        }
    }
}

void CCrystalTextView::
OnUpdateMatchBrace (CCmdUI * pCmdUI)
{
  CPoint ptCursorPos = GetCursorPos ();
  int nLength = m_pTextBuffer->GetLineLength (ptCursorPos.y);
  LPCTSTR pszText = m_pTextBuffer->GetLineChars (ptCursorPos.y) + ptCursorPos.x;
  pCmdUI->Enable (ptCursorPos.x < nLength && (bracetype (*pszText) || ptCursorPos.x > 0 && bracetype (pszText[-1])) || ptCursorPos.x > 0 && bracetype (pszText[-1]));
}

void CCrystalTextView::
OnEditGoTo ()
{
  CGotoDlg dlg (this);
  dlg.DoModal ();
}

void CCrystalTextView::
OnUpdateToggleSourceHeader (CCmdUI * pCmdUI)
{
  pCmdUI->Enable (m_CurSourceDef->type == CrystalLineParser::SRC_C);
}

void CCrystalTextView::
OnToggleSourceHeader ()
{
  if (m_CurSourceDef->type == CrystalLineParser::SRC_C)
    {
      CDocument *pDoc = GetDocument ();
      ASSERT (pDoc != nullptr);
      CString sFilePath = pDoc->GetPathName (), sOriginalPath = sFilePath;
      if (!_tcsicmp (sFilePath.Right (2), _T (".c")))
        {
          sFilePath = sFilePath.Left (sFilePath.GetLength () - 1) + _T ('h');
        }
      else if (!_tcsicmp (sFilePath.Right (4), _T (".cpp")))
        {
          sFilePath = sFilePath.Left (sFilePath.GetLength () - 3) + _T ('h');
        }
      else if (!_tcsicmp (sFilePath.Right (4), _T (".inl")))
        {
          sFilePath = sFilePath.Left (sFilePath.GetLength () - 3) + _T ('c');
          if (!FileExist(sFilePath))
            {
              sFilePath = sFilePath + _T ("pp");
            }
        }
      else if (!_tcsicmp (sFilePath.Right (4), _T (".hpp")))
        {
          sFilePath = sFilePath.Left (sFilePath.GetLength () - 3) + _T ("inl");
          if (!FileExist(sFilePath))
            {
              sFilePath = sFilePath.Left (sFilePath.GetLength () - 3) + _T ('c');
              if (!FileExist(sFilePath))
                {
                  sFilePath = sFilePath + _T ("pp");
                }
            }
        }
      else if (!_tcsicmp (sFilePath.Right (2), _T (".h")))
        {
          sFilePath = sFilePath.Left (sFilePath.GetLength () - 1) + _T ("hpp");
          if (!FileExist(sFilePath))
            {
              sFilePath = sFilePath.Left (sFilePath.GetLength () - 3) + _T ("inl");
              if (!FileExist(sFilePath))
                {
                  sFilePath = sFilePath.Left (sFilePath.GetLength () - 3) + _T ('c');
                  if (!FileExist(sFilePath))
                    {
                      sFilePath = sFilePath + _T ("pp");
                    }
                }
            }
        }
      if (FileExist(sFilePath))
        {
          if (!m_bSingle || !pDoc->IsModified () || pDoc->DoSave (sOriginalPath))
            {
              AfxGetApp ()->OpenDocumentFile (sFilePath);
              if (m_bSingle)
                {
                  m_ptCursorLast.x = m_ptCursorLast.y = 0;
                  ASSERT_VALIDTEXTPOS (m_ptCursorLast);
                  CPoint ptCursorPos = m_ptCursorLast;
                  SetCursorPos (ptCursorPos);
                  SetSelection (ptCursorPos, ptCursorPos);
                  SetAnchor (ptCursorPos);
                  EnsureVisible (ptCursorPos);
                  Invalidate ();
                }
            }
        }
    }
}

void CCrystalTextView::
OnUpdateTopMargin (CCmdUI * pCmdUI)
{
  pCmdUI->SetCheck (m_bTopMargin);
}

void CCrystalTextView::
OnTopMargin ()
{
  ASSERT (m_CurSourceDef != nullptr);
  if (m_bTopMargin)
    m_CurSourceDef->flags &= ~SRCOPT_TOPMARGIN;
  else
    m_CurSourceDef->flags |= SRCOPT_TOPMARGIN;
  SetTopMargin (!m_bTopMargin);
}

void CCrystalTextView::
OnUpdateSelMargin (CCmdUI * pCmdUI)
{
  pCmdUI->SetCheck (m_bSelMargin);
}

void CCrystalTextView::
OnSelMargin ()
{
  ASSERT (m_CurSourceDef != nullptr);
  if (m_bSelMargin)
    m_CurSourceDef->flags &= ~SRCOPT_SELMARGIN;
  else
    m_CurSourceDef->flags |= SRCOPT_SELMARGIN;
  SetSelectionMargin (!m_bSelMargin);
}

void CCrystalTextView::
OnUpdateWordWrap (CCmdUI * pCmdUI)
{
  pCmdUI->SetCheck (m_bWordWrap);
}

void CCrystalTextView::
OnWordWrap ()
{
  ASSERT (m_CurSourceDef != nullptr);
  if (m_bWordWrap)
    {
      m_CurSourceDef->flags &= ~SRCOPT_WORDWRAP;
      SetWordWrapping (false);
    }
  else
    {
      m_CurSourceDef->flags |= SRCOPT_WORDWRAP;
      SetWordWrapping (true);
    }
}

void CCrystalTextView::
OnForceRedraw ()
{
  //Invalidate ();
  RedrawWindow (nullptr, nullptr, RDW_INVALIDATE|RDW_UPDATENOW|RDW_ERASE|RDW_ERASENOW);
}

void CCrystalTextView::
OnToggleColumnSelection ()
{
  m_bRectangularSelection = !m_bRectangularSelection;
  Invalidate ();
}

LRESULT CCrystalTextView::
OnDpiChangedBeforeParent (WPARAM wParam, LPARAM lParam)
{
  const int oldDpi = m_dpi;
  __super::OnDpiChangedBeforeParent(wParam, lParam);
  if (m_dpi != oldDpi)
    {
      m_lfBaseFont.lfHeight = MulDiv(m_lfBaseFont.lfHeight, m_dpi, oldDpi);
      SetFont (m_lfBaseFont);
    }
  return 0;
}

void CCrystalTextView::SetRenderingMode(RENDERING_MODE nRenderingMode)
{
#ifdef _WIN64
  if (nRenderingMode == RENDERING_MODE_GDI)
    m_pCrystalRenderer.reset(new CCrystalRendererGDI());
  else
    m_pCrystalRenderer.reset(new CCrystalRendererDirectWrite(nRenderingMode));
  m_pCrystalRenderer->SetFont(m_lfBaseFont);
#endif
  m_nRenderingMode = nRenderingMode;
}

//BEGIN SW
bool CCrystalTextView::GetWordWrapping() const
{
  return m_bWordWrap;
}

void CCrystalTextView::SetWordWrapping( bool bWordWrap )
{
  m_bWordWrap = bWordWrap;

  if( IsWindow( m_hWnd ) )
    {
      m_nOffsetChar = 0;
      InvalidateScreenRect();
    }
}

CCrystalParser *CCrystalTextView::SetParser( CCrystalParser *pParser )
{
  CCrystalParser	*pOldParser = m_pParser;

  m_pParser = pParser;

  if( pParser != nullptr )
    pParser->m_pTextView = this;

  return pOldParser;
}
//END SW

/**
 * @brief Return whether a line is visible.
 */
bool CCrystalTextView::GetLineVisible (int nLineIndex) const
{
  return !m_bHideLines || !(GetLineFlags (nLineIndex) & LF_INVISIBLE);
}

//BEGIN SW
// incremental search imlementation
BOOL CCrystalTextView::OnCmdMsg( UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO *pHandlerInfo )
{
  // just look for commands
  if( nCode != CN_COMMAND || pExtra != nullptr )
    return __super::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo );

  // handle code:
  // each command that is not related to incremental search
  // ends the incremental search
  if( nID == ID_EDIT_FIND_INCREMENTAL_FORWARD || 
    nID == ID_EDIT_FIND_INCREMENTAL_BACKWARD || 
    nID == ID_EDIT_DELETE_BACK )
    return __super::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo );

  if( nID >= ID_EDIT_FIRST && nID <= ID_EDIT_LAST )
    m_bIncrementalSearchForward = m_bIncrementalSearchBackward = false;

  return __super::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo );
}

void CCrystalTextView::OnChar( wchar_t nChar, UINT nRepCnt, UINT nFlags )
{
  __super::OnChar( nChar, nRepCnt, nFlags );

  // we only have to handle character-input, if we are in incremental search mode
  if( !m_bIncrementalSearchForward && !m_bIncrementalSearchBackward )
    return;

  // exit incremental search, when Escape is pressed
  if( nChar == VK_ESCAPE )
    {
      // if not end incremental search
      m_bIncrementalSearchForward = m_bIncrementalSearchBackward = false;
      SetSelection( m_selStartBeforeIncrementalSearch, m_selEndBeforeIncrementalSearch );
      SetCursorPos( m_cursorPosBeforeIncrementalSearch );
      EnsureVisible( m_cursorPosBeforeIncrementalSearch );
      return;
    }

  // exit incremental search without destroying selection
  if( nChar == VK_RETURN )
    {
      m_bIncrementalSearchForward = m_bIncrementalSearchBackward = false;
      return;
    }

  // is the character valid for incremental search?
  if( !_istgraph( nChar ) && !(nChar == _T(' ')) && !(nChar == _T('\t')) )
    {
      // if not end incremental search
      m_bIncrementalSearchForward = m_bIncrementalSearchBackward = false;
      return;
    }

  // if last search was not successfull do not add a new character
  if( !m_bIncrementalFound )
    {
      MessageBeep( MB_OK );
      return;
    }

  // add character to incremental search string and search
  *m_pstrIncrementalSearchString += (TCHAR) nChar;
  OnEditFindIncremental();
}

LRESULT CCrystalTextView::OnImeStartComposition(WPARAM wParam, LPARAM lParam) /* IME */
{
  UpdateCompositionWindowFont();
  UpdateCompositionWindowPos();

  return DefWindowProc(WM_IME_STARTCOMPOSITION, wParam, lParam);
}

void CCrystalTextView::OnEditDeleteBack() 
{
  if( !m_bIncrementalSearchForward && !m_bIncrementalSearchBackward )
    return;

  // remove last character from search string
  if( m_pstrIncrementalSearchString->IsEmpty() )
    return;

  *m_pstrIncrementalSearchString = m_pstrIncrementalSearchString->Left( m_pstrIncrementalSearchString->GetLength() - 1 );
  OnEditFindIncremental();
}


void CCrystalTextView::OnEditFindIncremental( bool bFindNextOccurence /*= false*/ )
{
  // when string is empty, then goto position where the search starts
  if( m_pstrIncrementalSearchString->IsEmpty() )
    {
      SetSelection( m_incrementalSearchStartPos, m_incrementalSearchStartPos );
      SetCursorPos( m_incrementalSearchStartPos );
      EnsureVisible( m_incrementalSearchStartPos );
      return;
    }

  // otherwise search next occurence of search string, 
  // starting at current cursor position
  CPoint	matchStart, matchEnd;

  // calculate start point for search
  if( bFindNextOccurence )
    {
      CPoint	selStart, selEnd;
      GetSelection( selStart, selEnd );
      m_incrementalSearchStartPos = (m_bIncrementalSearchBackward)? selStart : selEnd;
    }

  m_bIncrementalFound = FindText( 
    *m_pstrIncrementalSearchString,
    m_incrementalSearchStartPos,
    m_bIncrementalSearchBackward? FIND_DIRECTION_UP : 0,
    true,
    &matchStart );

  if( !m_bIncrementalFound )
    {
      MessageBeep( MB_OK );
      return;
    }

  // select found text and set cursor to end of match
  matchEnd = matchStart;
  matchEnd.x+= m_pstrIncrementalSearchString->GetLength();
  SetSelection( matchStart, matchEnd );
  SetCursorPos( matchEnd );
  EnsureVisible( matchEnd );
}



void CCrystalTextView::OnEditFindIncrementalForward()
{
  if( !m_bIncrementalSearchForward && !m_bIncrementalSearchBackward )
    {
      // initialize
      if( !m_pstrIncrementalSearchString->IsEmpty() )
        *m_pstrIncrementalSearchStringOld = *m_pstrIncrementalSearchString;
      m_pstrIncrementalSearchString->Empty();
      m_incrementalSearchStartPos = m_cursorPosBeforeIncrementalSearch = m_ptCursorPos;
      GetSelection( m_selStartBeforeIncrementalSearch, m_selEndBeforeIncrementalSearch );
    }
  else if( m_bIncrementalSearchForward )
    {
      if( m_pstrIncrementalSearchString->IsEmpty() )
        {
          *m_pstrIncrementalSearchString = *m_pstrIncrementalSearchStringOld;
          m_pstrIncrementalSearchStringOld->Empty();
          OnEditFindIncremental();
        }
      else
        OnEditFindIncremental( true );

      return;
    }

  m_bIncrementalSearchForward = true;
  m_bIncrementalSearchBackward = false;
  m_bIncrementalFound = true;
  OnEditFindIncremental();
}

void CCrystalTextView::OnEditFindIncrementalBackward()
{
  if( !m_bIncrementalSearchForward && !m_bIncrementalSearchBackward )
    {
      // initialize
      if( !m_pstrIncrementalSearchString->IsEmpty() )
        *m_pstrIncrementalSearchStringOld = *m_pstrIncrementalSearchString;
      m_pstrIncrementalSearchString->Empty();
      GetSelection( m_selStartBeforeIncrementalSearch, m_selEndBeforeIncrementalSearch );
      m_incrementalSearchStartPos = m_cursorPosBeforeIncrementalSearch = m_ptCursorPos;
    }
  else if( m_bIncrementalSearchBackward )
    {
      if( m_pstrIncrementalSearchString->IsEmpty() )
        {
          *m_pstrIncrementalSearchString = *m_pstrIncrementalSearchStringOld;
          m_pstrIncrementalSearchStringOld->Empty();
          OnEditFindIncremental();
        }
      else
        OnEditFindIncremental( true );

      return;
    }

  m_bIncrementalSearchForward = false;
  m_bIncrementalSearchBackward = true;
  m_bIncrementalFound = true;
  OnEditFindIncremental();
}

void CCrystalTextView::OnUpdateEditFindIncrementalForward(CCmdUI* pCmdUI)
{
  if (m_pTextBuffer != nullptr)
    {
      int nLines = m_pTextBuffer->GetLineCount ();
      int nChars = m_pTextBuffer->GetLineLength (m_ptCursorPos.y);
      pCmdUI->Enable(m_ptCursorPos.y < nLines - 1 || m_ptCursorPos.x < nChars);
      return;
    }
  pCmdUI->Enable(false);
}

void CCrystalTextView::OnUpdateEditFindIncrementalBackward(CCmdUI* pCmdUI)
{
  if (m_pTextBuffer != nullptr)
    {
      pCmdUI->Enable(m_ptCursorPos.y > 0 || m_ptCursorPos.x > 0);
      return;
    }
  pCmdUI->Enable(false);
}

void CCrystalTextView::OnUpdateStatusMessage( CStatusBar *pStatusBar )
{
  static bool	bUpdatedAtLastCall = false;

  ASSERT( pStatusBar != nullptr && IsWindow( pStatusBar->m_hWnd ) );
  if( pStatusBar == nullptr || !IsWindow( pStatusBar->m_hWnd ) )
    return;

  if( !m_bIncrementalSearchForward && !m_bIncrementalSearchBackward )
    {
      if( bUpdatedAtLastCall )
        pStatusBar->SetPaneText( 0, LoadResString(AFX_IDS_IDLEMESSAGE).c_str() );

      bUpdatedAtLastCall = false;

      return;
    }

  CString	strFormat;
  UINT formatid = 0;
  if( !m_bIncrementalFound )
    formatid = IDS_FIND_INCREMENTAL_FAILED;
  else if( m_bIncrementalSearchForward )
    formatid = IDS_FIND_INCREMENTAL_FORWARD;
  else if( m_bIncrementalSearchBackward )
    formatid = IDS_FIND_INCREMENTAL_BACKWARD;
  else
    return;
  strFormat.Format( LoadResString(formatid).c_str(), (LPCTSTR)*m_pstrIncrementalSearchString );

  pStatusBar->SetPaneText( 0, strFormat );
  bUpdatedAtLastCall = false;
}
//END SW

bool CCrystalTextView::IsTextBufferInitialized () const
{
  return m_pTextBuffer && m_pTextBuffer->IsTextBufferInitialized(); 
}

CString CCrystalTextView::GetTextBufferEol(int nLine) const
{
  return m_pTextBuffer->GetLineEol(nLine); 
}

void CCrystalTextView::SetMarkersContext(CCrystalTextMarkers * pMarkers)
{
  pMarkers->AddView(this);
  m_pMarkers = pMarkers;
}

#ifdef _UNICODE
int CCrystalTextView::GetCharCellCountUnicodeChar(const wchar_t *pch)
{  
  wchar_t ch = *pch;
  if (!m_bChWidthsCalculated[ch/256])
    {
      if (U16_IS_SURROGATE(ch) && U16_IS_SURROGATE_LEAD(ch))
        {
          return wcwidth(U16_GET_SUPPLEMENTARY(ch, pch[1]));
        }
      else
        {
          int nWidthArray[256];
          wchar_t nStart = ch/256*256;
          wchar_t nEnd = nStart + 255;
          m_pCrystalRenderer->GetCharWidth(nStart, nEnd, nWidthArray);
          int nCharWidth = GetCharWidth();
          for (int i = 0; i < 256; i++) 
            {
              if (nCharWidth * 15 < nWidthArray[i] * 10)
                m_iChDoubleWidthFlags[(nStart+i)/32] |= 1 << (i % 32);
              else
                {
                  wchar_t ch2 = static_cast<wchar_t>(nStart + i);
                  if (wcwidth(ch2) > 1)
                    m_iChDoubleWidthFlags[(nStart + i) / 32] |= 1 << (i % 32);
                }
            }
          m_bChWidthsCalculated[ch / 256] = true;
        }
    }
  if (m_iChDoubleWidthFlags[ch / 32] & (1 << (ch % 32)))
    return 2;
  else
    return 1;
}
#endif

/** @brief Reset computed unicode character widths. */
void CCrystalTextView::ResetCharWidths ()
{
#ifdef _UNICODE
  ZeroMemory(m_bChWidthsCalculated, sizeof(m_bChWidthsCalculated));
  ZeroMemory(m_iChDoubleWidthFlags, sizeof(m_iChDoubleWidthFlags));
#endif
}

// This function assumes selection is in one line
void CCrystalTextView::EnsureVisible (CPoint ptStart, CPoint ptEnd)
{
  //  Scroll vertically
  //BEGIN SW
  int nSubLineCount = GetSubLineCount();
  int nNewTopSubLine = m_nTopSubLine;
  CPoint subLinePos;

  CharPosToPoint( ptStart.y, ptStart.x, subLinePos );
  subLinePos.y += GetSubLineIndex( ptStart.y );

  if( subLinePos.y >= nNewTopSubLine + GetScreenLines() )
    nNewTopSubLine = subLinePos.y - GetScreenLines() + 1;
  if( subLinePos.y < nNewTopSubLine )
    nNewTopSubLine = subLinePos.y;

  if( nNewTopSubLine < 0 )
    nNewTopSubLine = 0;
  if( nNewTopSubLine >= nSubLineCount )
    nNewTopSubLine = nSubLineCount - 1;

  if ( !m_bWordWrap && !m_bHideLines )
    {
      // WINMERGE: This line fixes (cursor) slowdown after merges!
      // I don't know exactly why, but propably we are setting
      // m_nTopLine to zero in ResetView() and are not setting to
      // valid value again. Maybe this is a good place to set it?
      m_nTopLine = nNewTopSubLine;
    }
  else
    {
      int dummy;
      GetLineBySubLine(nNewTopSubLine, m_nTopLine, dummy);
    }

  if( nNewTopSubLine != m_nTopSubLine )
    {
      ScrollToSubLine( nNewTopSubLine );
      UpdateCaret();
      UpdateSiblingScrollPos( false );
    }

  //  Scroll horizontally
  //BEGIN SW
  // we do not need horizontally scrolling, if we wrap the words
  if( GetTextLayoutMode () == TEXTLAYOUT_WORDWRAP )
    return;
  //END SW
  int nActualPos = CalculateActualOffset (ptStart.y, ptStart.x);
  int nNewOffset = m_nOffsetChar;
  const int nScreenChars = GetScreenChars ();

  if (ptStart == ptEnd)
    {
      // Keep 5 chars visible right to cursor
      if (nActualPos > nNewOffset + nScreenChars - 5)
        {
          // Add 10 chars width space after line
          nNewOffset = nActualPos - nScreenChars + 10;
        }
      // Keep 5 chars visible left to cursor
      if (nActualPos < nNewOffset + 5)
        {
          // Jump by 10 char steps, so user sees previous letters too
          nNewOffset = nActualPos - 10;
        }
    }
  else
    {
      int nActualEndPos = CalculateActualOffset (ptEnd.y, ptEnd.x);
      const int nBeginOffset = nActualPos - m_nOffsetChar;
      const int nEndOffset = nActualEndPos - m_nOffsetChar;
      const int nSelLen = nActualEndPos - nActualPos;

      // Selection fits to screen, scroll whole selection visible
      if (nSelLen < nScreenChars)
        {
          // Begin of selection not visible 
          if (nBeginOffset > nScreenChars)
            {
              // Scroll so that there is max 5 chars margin at end
              if (nScreenChars - nSelLen > 5)
                nNewOffset = nActualPos + 5 - nScreenChars + nSelLen;
              else
                nNewOffset = nActualPos - 5;
            }
          else if (nBeginOffset < 0)
            {
              // Scroll so that there is max 5 chars margin at begin
              if (nScreenChars - nSelLen >= 5)
                nNewOffset = nActualPos - 5;
              else
                nNewOffset = nActualPos - 5 - nScreenChars + nSelLen;
            }
          // End of selection not visible
          else if (nEndOffset > nScreenChars ||
              nEndOffset < 0)
            {
              nNewOffset = nActualPos - 5;
            }
        }
      else // Selection does not fit screen so scroll to begin of selection
        {
          nNewOffset = nActualPos - 5;
        }
    }

  // Horiz scroll limit to longest line + one screenwidth
  const int nMaxLineLen = GetMaxLineLength (m_nTopLine, GetScreenLines());
  if (nNewOffset >= nMaxLineLen + nScreenChars)
    nNewOffset = nMaxLineLen + nScreenChars - 1;
  if (nNewOffset < 0)
    nNewOffset = 0;

  if (m_nOffsetChar != nNewOffset)
    {
      ScrollToChar (nNewOffset);
      UpdateCaret ();
      UpdateSiblingScrollPos (true);
    }
}

// Analyze the first line of file to detect its type
// Mainly it works for xml files
bool CCrystalTextView::
SetTextTypeByContent (LPCTSTR pszContent)
{
  RxNode *rxnode = nullptr;
  RxMatchRes rxmatch;
  int nLen;
  if (::FindStringHelper(pszContent, _tcslen(pszContent), pszContent, _T("^\\s*\\<\\?xml\\s+.+?\\?\\>\\s*$"),
      FIND_REGEXP, nLen, rxnode, &rxmatch) == 0)
    {
      if (rxnode)
        RxFree (rxnode);
      return SetTextType(CrystalLineParser::SRC_XML);
    }
  if (rxnode)
    RxFree (rxnode);
  return false;
}

void CCrystalTextView::
AutoFitColumn (int nColumn)
{
  int nLastColumn = 0;
  int nLastColumnWidth = 0;
  const int nTabSize = GetTabSize ();
  std::vector<int> aColumnWidths;
  const int nScreenChars = GetScreenChars ();
  const int nMaxColumnWidth = nScreenChars < 1 ? 1 : nScreenChars - 1;
  for (auto& pbuf : m_pTextBuffer->GetTextBufferList ())
  {
      const TCHAR sep = pbuf->GetFieldDelimiter ();
      const int quote = pbuf->GetFieldEnclosure ();
      const int nLineCount = pbuf->GetLineCount ();
      for (int i = 0; i < nLineCount; ++i)
        {
          bool bInQuote = false;
          int nColumn2 = 0;
          int nColumnWidth = 0;
          const TCHAR* pszChars = pbuf->GetLineChars (i);
          const size_t nLineLength = pbuf->GetFullLineLength (i);
          for (size_t j = 0; j < nLineLength; j += U16_IS_SURROGATE (pszChars[j]) ? 2 : 1)
            {
              bool bDelimiterOrNewLine = false;
              TCHAR c = pszChars[j];
              if (c == quote)
                bInQuote = !bInQuote;
              if (!bInQuote && c == sep)
                {
                  bDelimiterOrNewLine = true;
                  ++nColumnWidth;
                }
              else if (c == '\r' || c == '\n')
                {
                  if (m_bWordWrap)
                    {
                      if (c == '\r')
                        {
                          if (j == nLineLength - 1 || pszChars[j + 1] != '\n')
                            {
                              nColumnWidth += 2;
                              bDelimiterOrNewLine = true;
                            }
                        }
                      else
                        {
                          if (j > 0 && pszChars[j - 1] == '\r')
                            nColumnWidth += 4;
                          else
                            nColumnWidth += 2;
                          bDelimiterOrNewLine = true;
                        }
                    }
                  else
                    nColumnWidth += GetCharCellCountFromChar (pszChars + j);
                }
              else if (c == '\t')
                nColumnWidth ++;
              else
                nColumnWidth += GetCharCellCountFromChar (pszChars + j);

              if (bDelimiterOrNewLine)
                {
                  if (nColumnWidth > nMaxColumnWidth)
                    nColumnWidth = nMaxColumnWidth;
                  if (static_cast<int>(aColumnWidths.size ()) < nColumn2 + 1)
                    aColumnWidths.resize (nColumn2 + 1, nTabSize);
                  if (aColumnWidths[nColumn2] < nColumnWidth)
                    aColumnWidths[nColumn2] = nColumnWidth;
                  nColumnWidth = 0;
                  if (c == sep)
                    ++nColumn2;
                }
            }
          if (nLastColumn < nColumn2)
            {
              nLastColumn = nColumn2;
              nLastColumnWidth = 0;
            }
          if (nLastColumnWidth < nColumnWidth)
              nLastColumnWidth = nColumnWidth;
        }
    }

  aColumnWidths.resize (nLastColumn + 1, nTabSize);
  if (aColumnWidths[nLastColumn] < nLastColumnWidth)
    aColumnWidths[nLastColumn] = nLastColumnWidth;

  for (size_t nColumn2 = 0; nColumn2 < aColumnWidths.size (); ++nColumn2)
    {
      if (nColumn == -1 || nColumn == static_cast<int>(nColumn2))
        m_pTextBuffer->SetColumnWidth (static_cast<int>(nColumn2), aColumnWidths[nColumn2]);
    }
  m_pTextBuffer->InvalidateColumns ();
}

CCrystalTextView::TextLayoutMode CCrystalTextView::GetTextLayoutMode () const
{
  if (m_pTextBuffer && m_pTextBuffer->GetTableEditing ())
    return m_bWordWrap ? TEXTLAYOUT_TABLE_WORDWRAP : TEXTLAYOUT_TABLE_NOWORDWRAP;
  return m_bWordWrap ? TEXTLAYOUT_WORDWRAP : TEXTLAYOUT_NOWORDWRAP;
}

////////////////////////////////////////////////////////////////////////////
