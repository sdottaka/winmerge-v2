////////////////////////////////////////////////////////////////////////////
//  File:       ccrystaltextview.h
//  Version:    1.0.0.0
//  Created:    29-Dec-1998
//
//  Author:     Stcherbatchenko Andrei
//  E-mail:     windfall@gmx.de
//
//  Interface of the CCrystalTextView class, a part of Crystal Edit -
//  syntax coloring text editor.
//
//  You are free to use or modify this code to the following restrictions:
//  - Acknowledge me somewhere in your about box, simple "Parts of code by.."
//  will be enough. If you can't (or don't want to), contact me personally.
//  - LEAVE THIS HEADER INTACT
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//  19-Jul-99
//      Ferdinand Prantl:
//  +   FEATURE: see cpps ...
//
//  ... it's being edited very rapidly so sorry for non-commented
//        and maybe "ugly" code ...
////////////////////////////////////////////////////////////////////////////
/** 
 * @file  ccrystaltextview.h
 *
 * @brief Declaration file for CCrystalTextView
 */

#pragma once

#include <vector>
#include "crystalparser.h"
#include "parsers/crystallineparser.h"
#include "renderers/ccrystalrenderer.h"
#include "utils/cregexp.h"
#include "utils/icu.hpp"
#include "utils/DpiAware.h"

////////////////////////////////////////////////////////////////////////////
// Forward class declarations

class CCrystalTextBuffer;
class CUpdateContext;
struct ViewableWhitespaceChars;
class SyntaxColors;
class CFindTextDlg;
struct LastSearchInfos;
class CCrystalTextMarkers;

////////////////////////////////////////////////////////////////////////////
// CCrystalTextView class declaration

//  CCrystalTextView::FindText() flags
enum : unsigned
{
  FIND_MATCH_CASE = 0x0001U,
  FIND_WHOLE_WORD = 0x0002U,
  FIND_REGEXP = 0x0004U,
  FIND_DIRECTION_UP = 0x0010U,
  REPLACE_SELECTION = 0x0100U, 
  FIND_NO_WRAP = 0x200U,
  FIND_NO_CLOSE = 0x400U
};

//  CCrystalTextView::UpdateView() flags
enum : unsigned
{
  UPDATE_HORZRANGE = 0x0001U,  //  update horz scrollbar
  UPDATE_VERTRANGE = 0x0002U, //  update vert scrollbar
  UPDATE_SINGLELINE = 0x0100U,    //  single line has changed
  UPDATE_FLAGSONLY = 0x0200U, //  only line-flags were changed

  UPDATE_RESET = 0x1000U       //  document was reloaded, update all!
};

/**
 * @brief Class for text view.
 * This class implements class for text viewing. Class implements all
 * the routines we need for showing, selecting text etc. BUT it does
 * not implement text editing. There are classes inherited from this
 * class which implement text editing.
 */
class EDITPADC_CLASS CCrystalTextView : public DpiAware::CDpiAwareWnd<CView>
  {
    DECLARE_DYNCREATE (CCrystalTextView)

    friend CCrystalParser;
    friend CCrystalTextBuffer;

protected:
    //  Search parameters
    bool m_bLastSearch;
    DWORD m_dwLastSearchFlags;
    LPTSTR m_pszLastFindWhat;
    bool m_bMultipleSearch;       // More search
    CFindTextDlg *m_pFindTextDlg;

private :
    bool m_bCursorHidden;

    //  Painting caching bitmap
    CBitmap *m_pCacheBitmap;

    //  Line/character dimensions
    int m_nLineHeight, m_nCharWidth;
    void CalcLineCharDim ();

    //  Text attributes
    bool m_bViewTabs;
    bool m_bViewEols;
    bool m_bDistinguishEols;
    bool m_bTopMargin;
    bool m_bSelMargin;
    bool m_bViewLineNumbers;
    DWORD m_dwFlags;

    //  Amount of lines/characters that completely fits the client area
    int m_nScreenLines, m_nScreenChars;

    SyntaxColors * m_pColors;
    CCrystalTextMarkers * m_pMarkers;

    //BEGIN SW
    /**
    Contains for each line the number of sublines. If the line is not
    wrapped, the value for this line is 1. The value of a line is invalid,
    if it is -1.

    Must create pointer, because contructor uses AFX_ZERO_INIT_OBJECT to
    initialize the member objects. This would destroy a CArray object.
    */
    CArray<int, int> *m_panSubLines;
    CArray<int, int> *m_panSubLineIndexCache;
    int m_nLastLineIndexCalculatedSubLineIndex;
    //END SW

    int m_nIdealCharPos;

    bool m_bFocused;
protected:
    CPoint m_ptAnchor;
private:
    LOGFONT m_lfBaseFont;
    LOGFONT m_lfSavedBaseFont;

    //  Parsing stuff

    /**  
    This array must be initialized to (DWORD) - 1, code for invalid values (not yet computed).
    We prefer to limit the recomputing delay to the moment when we need to read
    a parseCookie value for drawing.
    GetParseCookie must always be used to read the m_ParseCookies value of a line.
    If the actual value is invalid code, GetParseCookie computes the value, 
    stores it in m_ParseCookies, and returns the new valid value.
    When we edit the text, the parse cookies value may change for the modified line
    and all the lines below (As m_ParseCookies[line i] depends on m_ParseCookies[line (i-1)])
    It would be a loss of time to recompute all these values after each action.
    So we just set all these values to invalid code (DWORD) - 1.
    */
    std::vector<DWORD> *m_ParseCookies;
    DWORD GetParseCookie (int nLineIndex);

    /**
    Pre-calculated line lengths (in characters)
    This array works as the parse cookie Array
    and must be initialized to - 1, code for invalid values (not yet computed).
    for the same reason.
    */
    std::vector<int> *m_pnActualLineLength;

protected:
    bool m_bPreparingToDrag;
    bool m_bDraggingText;
    bool m_bDragSelection, m_bWordSelection, m_bLineSelection, m_bRectangularSelection, m_bColumnSelection;
    int m_nColumnResizing;
    UINT_PTR m_nDragSelTimer;
    DWORD m_dwLastDblClickTime;

    CPoint m_ptDrawSelStart, m_ptDrawSelEnd;

    CPoint m_ptCursorPos, m_ptCursorLast;
    CPoint m_ptSelStart, m_ptSelEnd;
    void PrepareSelBounds ();

    //  Helper functions
    int ExpandChars (int nLineIndex, int nOffset, int nCount, CString & line, int nActualOffset);
    int ExpandCharsTableEditingNoWrap (int nLineIndex, int nOffset, int nCount, CString & line, int nActualOffset);
    void AutoFitColumn(int nColumn = -1);
    enum TextLayoutMode { TEXTLAYOUT_NOWORDWRAP, TEXTLAYOUT_WORDWRAP, TEXTLAYOUT_TABLE_NOWORDWRAP, TEXTLAYOUT_TABLE_WORDWRAP };
    TextLayoutMode GetTextLayoutMode() const;

    int ApproxActualOffset (int nLineIndex, int nOffset);
    void AdjustTextPoint (CPoint & point);
    void DrawLineHelperImpl (CPoint & ptOrigin, const CRect & rcClip,
 int nColorIndex,
                             int nBgColorIndex, COLORREF crText, COLORREF crBkgnd, int nLineIndex, int nOffset, int nCount, int &nActualOffset);
    bool IsInsideSelBlock (CPoint ptTextPos);

    bool m_bBookmarkExist;        // More bookmarks
    void ToggleBookmark(int nLine);

public :
    enum RENDERING_MODE
    {
      RENDERING_MODE_GDI = -1,
      RENDERING_MODE_DWRITE_DFEAULT = 0,
      RENDERING_MODE_DWRITE_ALIASED = 1,
      RENDERING_MODE_DWRITE_GDI_CLASSIC = 2,
      RENDERING_MODE_DWRITE_GDI_NATURAL = 3,
      RENDERING_MODE_DWRITE_NATURAL = 4,
      RENDERING_MODE_DWRITE_NATURAL_SYMMETRIC = 5,
    };

    virtual void ResetView ();
    virtual int GetLineCount ();
    virtual void OnUpdateCaret ();
    bool IsTextBufferInitialized () const;
    CString GetTextBufferEol (int nLine) const;

    SyntaxColors * GetSyntaxColors() { return m_pColors; }
    void SetColorContext(SyntaxColors * pColors) { m_pColors = pColors; }
    CCrystalTextMarkers * GetMarkers() const { return m_pMarkers; }
    void SetMarkersContext(CCrystalTextMarkers * pMarkers);
    static CLIPFORMAT GetClipTcharTextFormat() { return sizeof(TCHAR) == 1 ? CF_TEXT : CF_UNICODETEXT; }

protected :
    CPoint WordToRight (CPoint pt);
    CPoint WordToLeft (CPoint pt);
    bool           m_bOverrideCaret;

    bool m_bSingle;
    CCrystalTextBuffer *m_pTextBuffer;
    HACCEL m_hAccel;
    bool m_bVertScrollBarLocked, m_bHorzScrollBarLocked;
    CPoint m_ptDraggedTextBegin, m_ptDraggedTextEnd;
    void UpdateCaret ();
    void SetAnchor (const CPoint & ptNewAnchor);
    int GetTopMarginHeight ();
    int GetMarginWidth (CDC *pdc = nullptr);
    bool IsValidTextPos (const CPoint &point);
    bool IsValidTextPosX (const CPoint &point);
    bool IsValidTextPosY (const CPoint &point);

    bool m_bShowInactiveSelection;
    //  [JRT]
    bool m_bDisableDragAndDrop;

    //BEGIN SW
    bool m_bWordWrap;
    bool m_bHideLines;
    CCrystalParser *m_pParser;
    //END SW

    int ClientToIdealTextPos (int x);
    CPoint ClientToText (const CPoint & point);
    int ClientToColumn (int x);
    int ClientToColumnResizing (int x);
    CPoint TextToClient (const CPoint & point);
    int ColumnToClient (int nColumn);
    void InvalidateLines (int nLine1, int nLine2, bool bInvalidateMargin = false);
    int CalculateActualOffset (int nLineIndex, int nCharIndex, bool bAccumulate = false);

    //  Printing
    int m_nPrintPages;
    CFont *m_pPrintFont;
    int m_nPrintLineHeight;
    bool m_bPrintHeader, m_bPrintFooter;
    CRect m_ptPageArea, m_rcPrintArea;
    bool m_bPrinting;
    void GetPrintMargins (long & nLeft, long & nTop, long & nRight, long & nBottom);
    virtual void RecalcPageLayouts (CDC * pdc, CPrintInfo * pInfo);
    virtual void PrintHeader (CDC * pdc, int nPageNum);
    virtual void PrintFooter (CDC * pdc, int nPageNum);
    virtual void GetPrintHeaderText (int nPageNum, CString & text);
    virtual void GetPrintFooterText (int nPageNum, CString & text);

    //  Keyboard handlers
    void MoveLeft (bool bSelect);
    void MoveRight (bool bSelect);
    void MoveWordLeft (bool bSelect);
    void MoveWordRight (bool bSelect);
    void MoveUp (bool bSelect);
    void MoveDown (bool bSelect);
    void MoveHome (bool bSelect);
    void MoveEnd (bool bSelect);
    void MovePgUp (bool bSelect);
    void MovePgDn (bool bSelect);
    void MoveCtrlHome (bool bSelect);
    void MoveCtrlEnd (bool bSelect);

    void SelectAll ();
    void Copy ();

    bool IsSelection () const;
    bool IsInsideSelection (const CPoint & ptTextPos);
    bool GetColumnSelection (int nLine, int & nLeftTextPos, int & nRightTextPos);
    void GetSelection (CPoint & ptStart, CPoint & ptEnd);
    void GetFullySelectedLines(int & firstLine, int & lastLine);
    virtual void SetSelection (const CPoint & ptStart, const CPoint & ptEnd, bool bUpdateView = true);

    int m_nTopLine, m_nOffsetChar;
    //BEGIN SW
    /**
    The index of the subline that is the first visible line on the screen.
    */
    int m_nTopSubLine;
    //END SW
    bool m_bSmoothScroll;

    int GetLineHeight ();
    //BEGIN SW
    /**
    Returns the number of sublines the given line contains of.
    Allway "1", if word wrapping is disabled.

    @param nLineIndex Index of the line to get the subline count of.

    @return Number of sublines the given line contains of
    */
    int GetSubLines( int nLineIndex );

    virtual int GetEmptySubLines( int nLineIndex ) { return 0; }
    bool IsEmptySubLineIndex( int nSubLineIndex );

    /**
    Converts the given character position for the given line into a point.

    After the call the x-member of the returned point contains the
    character position relative to the beginning of the subline. The y-member
    contains the zero based index of the subline relative to the line, the
    character position was given for.

    @param nLineIndex Zero based index of the line, nCharPos refers to.
    @param nCharPos The character position, the point shoult be calculated for.
    @param charPoint Reference to a point, which should receive the result.

    @return The character position of the beginning of the subline charPoint.y.
    */
    int CharPosToPoint( int nLineIndex, int nCharPos, CPoint &charPoint, int* pnColumn = nullptr );

    /**
    Converts the given cursor point for the given line to the character position
    for the given line.

    The y-member of the cursor position specifies the subline inside the given
    line, the cursor is placed on and the x-member specifies the cursor position
    (in character widths) relative to the beginning of that subline.

    @param nLineIndex Zero based index of the line the cursor position refers to.
    @param curPoint Position of the cursor relative to the line in sub lines and
        char widths.

    @return The character position the best matches the cursor position.
    */
    int CursorPointToCharPos( int nLineIndex, const CPoint &curPoint );

    /**
    Converts the given cursor position to a text position.

    The x-member of the subLinePos parameter describes the cursor position in
    char widths relative to the beginning of the subline described by the
    y-member. The subline is the zero based number of the subline relative to
    the beginning of the text buffer.

    <p>
    The returned point contains a valid text position, where the y-member is
    the zero based index of the textline and the x-member is the character
    position inside this line.

    @param subLinePos The sublinebased cursor position
        (see text above for detailed description).
    @param textPos The calculated line and character position that best matches
        the cursor position (see text above for detailed descritpion).
    */
    void SubLineCursorPosToTextPos( const CPoint &subLinePos, CPoint &textPos );

    /**
    Returns the character position relative to the given line, that matches
    the end of the given sub line.

    @param nLineIndex Zero based index of the line to work on.
    @param nSubLineOffset Zero based index of the subline inside the given line.

    @return Character position that matches the end of the given subline, relative
        to the given line.
    */
    int SubLineEndToCharPos( int nLineIndex, int nSubLineOffset );

    /**
    Returns the character position relative to the given line, that matches
    the start of the given sub line.

    @param nLineIndex Zero based index of the line to work on.
    @param nSubLineOffset Zero based index of the subline inside the given line.

    @return Character position that matches the start of the given subline, relative
        to the given line.
    */
    int SubLineHomeToCharPos( int nLineIndex, int nSubLineOffset );
    //END SW
    int GetCharWidth ();
    int GetMaxLineLength (int nTopLine, int nLines);
    int GetScreenLines ();
    int GetScreenChars ();

    void RecalcVertScrollBar (bool bPositionOnly = false, bool bRedraw = true);
    virtual void RecalcHorzScrollBar (bool bPositionOnly = false, bool bRedraw = true);

    //  Scrolling helpers
    void ScrollToChar (int nNewOffsetChar, bool bNoSmoothScroll = false, bool bTrackScrollBar = true);
    void ScrollToLine (int nNewTopLine, bool bNoSmoothScroll = false, bool bTrackScrollBar = true);

    //BEGIN SW
    /**
    Scrolls to the given sub line, so that the sub line is the first visible
    line on the screen.

    @param nNewTopSubLine Index of the sub line to scroll to.
    @param bNoSmoothScroll true to disable smooth scrolling, else false.
    @param bTrackScrollBar true to recalculate the scroll bar after scrolling,
        else false.
    */
    virtual void ScrollToSubLine( int nNewTopSubLine, bool bNoSmoothScroll = false, bool bTrackScrollBar = true );
    //END SW

    //  Splitter support
    virtual void UpdateSiblingScrollPos (bool bHorz);
    virtual void OnUpdateSibling (CCrystalTextView * pUpdateSource, bool bHorz);
    CCrystalTextView *GetSiblingView (int nRow, int nCol);

    //BEGIN SW
    /**
    Returns the number of sublines in the whole text buffer.

    The number of sublines is the sum of all sublines of all lines.

    @return Number of sublines in the whole text buffer.
    */
    virtual int GetSubLineCount();

    /**
    Returns the zero-based subline index of the given line.

    @param nLineIndex Index of the line to calculate the subline index of

    @return The zero-based subline index of the given line.
    */
    virtual int GetSubLineIndex( int nLineIndex );

    /**
     * @brief Splits the given subline index into line and sub line of this line.
     * @param [in] nSubLineIndex The zero based index of the subline to get info about
     * @param [out] nLine Gets the line number the give subline is included in
     * @param [out] nSubLine Get the subline of the given subline relative to nLine
     */
    virtual void GetLineBySubLine(int nSubLineIndex, int &nLine, int &nSubLine);

public:
    virtual int GetLineLength (int nLineIndex) const;
    virtual int GetFullLineLength (int nLineIndex) const;
    virtual int GetViewableLineLength(int nLineIndex) const;
    virtual int GetLineActualLength (int nLineIndex);
    virtual LPCTSTR GetLineChars (int nLineIndex) const;
protected:
    virtual DWORD GetLineFlags (int nLineIndex) const;
    virtual void GetText (const CPoint & ptStart, const CPoint & ptEnd, CString & text, bool bExcludeInvisibleLines = true);
    virtual void GetTextInColumnSelection (CString & text, bool bExcludeInvisibleLines = true);

    //  Clipboard overridable
    virtual bool TextInClipboard ();
    virtual bool PutToClipboard (LPCTSTR pszText, int cchText, bool bColumnSelection = false);
    virtual bool GetFromClipboard (CString & text, bool & bColumnSelection);

    //  Drag-n-drop overrideable
    virtual HGLOBAL PrepareDragData ();
    virtual DROPEFFECT GetDropEffect () { return DROPEFFECT_COPY; }
    virtual void OnDropSource (DROPEFFECT de);
    bool IsDraggingText () const;

    virtual COLORREF GetColor (int nColorIndex);
    virtual void GetLineColors (int nLineIndex, COLORREF & crBkgnd,
                                COLORREF & crText, bool & bDrawWhitespace);
    virtual bool GetItalic (int nColorIndex);
    virtual bool GetBold (int nColorIndex);

    void DrawLineHelper (CPoint & ptOrigin, const CRect & rcClip, int nColorIndex, int nBgColorIndex,
                         COLORREF crText, COLORREF crBkgnd, int nLineIndex, int nOffset, int nCount, int &nActualOffset, CPoint ptTextPos);
    virtual void DrawSingleLine (const CRect & rect, int nLineIndex);
    virtual void DrawTopMargin (const CRect & rect);
    virtual void DrawMargin (const CRect & rect, int nLineIndex, int nLineNumber);

    inline int GetCharCellCountFromChar(const TCHAR *pch)
    {
        TCHAR ch = *pch;
        if (ch >= _T('\x00') && ch <= _T('\x7F'))
          {
            if (ch <= _T('\x1F') && ch != '\t')
              {
                if (ch == '\r' && pch[1] == '\n')
                  return 6;
                else
                  return 3;
              }
            else
                return 1;
        } 
        // This assumes a fixed width font
        // But the UNICODE case handles double-wide glyphs (primarily Chinese characters)
#ifdef _UNICODE
        return GetCharCellCountUnicodeChar(pch);
#else
        return 1;
#endif
    }

    int GetMarginIconSize() const
    {
        return MulDiv(CCrystalRenderer::MARGIN_ICON_SIZE, m_dpi, USER_DEFAULT_SCREEN_DPI);
    }

#ifdef _UNICODE
    bool m_bChWidthsCalculated[65536/256];
    int m_iChDoubleWidthFlags[65536/32];
    int GetCharCellCountUnicodeChar(const wchar_t *pch);
#endif
    void ResetCharWidths();

    //BEGIN SW
    // word wrapping

    /**
    Called to wrap the line with the given index into sublines.

    The standard implementation wraps the line at the first non-whitespace after
    an whitespace that exceeds the visible line width (nMaxLineWidth). Override
    this function to provide your own word wrapping.

    <b>Attention:</b> Never call this function directly,
    call WrapLineCached() instead, which calls this method.

    @param nLineIndex The index of the line to wrap

    @param nMaxLineWidth The number of characters a subline of this line should
    not exceed (except whitespaces)

    @param anBreaks An array of integers. Put the positions where to wrap the line
    in that array (its allready allocated). If this pointer is `nullptr`, the function
    has only to compute the number of breaks (the parameter nBreaks).

    @param nBreaks The number of breaks this line has (number of sublines - 1). When
    the function is called, this variable is 0. If the line is not wrapped, this value
    should be 0 after the call.

    @see WrapLineCached()
    */
    virtual void WrapLine( int nLineIndex, int nMaxLineWidth, std::vector<int> *anBreaks, int &nBreaks );

    /**
    Called to wrap the line with the given index into sublines.

    Call this method instead of WrapLine() (which is called internal by this
    method). This function uses an internal cache which contains the number
    of sublines for each line, so it has only to call WrapLine(), if the
    cache for the given line is invalid or if the caller wants to get the
    wrap postions (anBreaks != nullptr).

    This functions also tests m_bWordWrap -- you can call it even if
    word wrapping is disabled and you will retrieve a valid value.

    @param nLineIndex The index of the line to wrap

    @param nMaxLineWidth The number of characters a subline of this line should
    not exceed (except whitespaces)

    @param anBreaks An array of integers. Put the positions where to wrap the line
    in that array (its allready allocated). If this pointer is `nullptr`, the function
    has only to compute the number of breaks (the parameter nBreaks).

    @param nBreaks The number of breaks this line has (number of sublines - 1). When
    the function is called, this variable is 0. If the line is not wrapped, this value
    should be 0 after the call.

    @see WrapLine()
    @see m_anSubLines
    */
    void WrapLineCached( int nLineIndex, int nMaxLineWidth, std::vector<int> *anBreaks, int &nBreaks );

    /**
    Invalidates the cached data for the given lines.

    <b>Remarks:</b> Override this method, if your derived class caches other
    view specific line info, which becomes invalid, when this line changes.
    Call this standard implementation in your overriding.

    @param nLineIndex1 The index of the first line to invalidate.

    @param nLineIndex2 The index of the last line to invalidate. If this value is
    -1 (default) all lines from nLineIndex1 to the end are invalidated.
    */
    virtual void InvalidateLineCache( int nLineIndex1, int nLineIndex2 );
    virtual void InvalidateSubLineIndexCache( int nLineIndex1 );
    void InvalidateScreenRect(bool bInvalidateView = true);
    //END SW

    virtual HINSTANCE GetResourceHandle ();

    //BEGIN SW
    // function to draw a single screen line
    // (a wrapped line can consist of many screen lines
    virtual void DrawScreenLine( CPoint &ptOrigin, const CRect &rcClip,
         const std::vector<CrystalLineParser::TEXTBLOCK>& blocks,
        int &nActualItem, COLORREF crText,
        COLORREF crBkgnd, bool bDrawWhitespace,
        int nLineIndex, int nOffset,
        int nCount, int &nActualOffset, CPoint ptTextPos );
    //END SW

    std::vector<CrystalLineParser::TEXTBLOCK> MergeTextBlocks(const std::vector<CrystalLineParser::TEXTBLOCK>& blocks1, const std::vector<CrystalLineParser::TEXTBLOCK>& blocks2) const;
    virtual std::vector<CrystalLineParser::TEXTBLOCK> GetMarkerTextBlocks(int nLineIndex) const;
    virtual std::vector<CrystalLineParser::TEXTBLOCK> GetAdditionalTextBlocks (int nLineIndex);

public:
    virtual CString GetHTMLLine (int nLineIndex, LPCTSTR pszTag);
    virtual CString GetHTMLStyles ();
    std::vector<CrystalLineParser::TEXTBLOCK> GetTextBlocks(int nLineIndex);
protected:
    virtual CString GetHTMLAttribute (int nColorIndex, int nBgColorIndex, COLORREF crText, COLORREF crBkgnd);

    //BEGIN SW
    // helpers for incremental search

    /**
    Called each time the position-information in the status bar
    is updated. Use this to change the text of the message field
    in the status bar.

    @param pStatusBar
        Pointer to the status bar
    */
    void OnUpdateStatusMessage( CStatusBar *pStatusBar );

    /**
    Called by OnFindIncrementalForward() and OnFindIncrementalBackward().

    @param bFindNextOccurence
        true, if the method should look for the next occurence of the
        search string in search direction.

    @see #OnFindIncrementalForward
    @see #OnFindIncrementalBackward
    */
    void OnEditFindIncremental( bool bFindNextOccurence = false );

    /** true if incremental forward search is active, false otherwise */
    bool m_bIncrementalSearchForward;

    /** true if incremental backward search is active, false otherwise */
    bool m_bIncrementalSearchBackward;

private:
    /** true if we found the string to search for */
    bool m_bIncrementalFound;

    /** String we are looking for.*/
    CString *m_pstrIncrementalSearchString;

    /** String we looked for last time.*/
    CString *m_pstrIncrementalSearchStringOld;

    /** Start of selection at the time the incremental search started */
    CPoint m_selStartBeforeIncrementalSearch;

    /** Start of selection at the time the incremental search started */
    CPoint m_selEndBeforeIncrementalSearch;

    /** Cursor position at the time the incremental search started */
    CPoint m_cursorPosBeforeIncrementalSearch;

    /** position to start the incremental search at */
    CPoint m_incrementalSearchStartPos;

    //END SW

public :
    void GoToLine (int nLine, bool bRelative);
    DWORD ParseLine (DWORD dwCookie, const TCHAR *pszChars, int nLength, CrystalLineParser::TEXTBLOCK * pBuf, int &nActualItems);

    // Attributes
public :
    int GetCRLFMode ();
    void SetCRLFMode (enum CRLFSTYLE nCRLFMode);
    bool GetViewTabs () const { return m_bViewTabs; }
    void SetViewTabs (bool bViewTabs);
    bool GetViewEols () const { return m_bViewEols; }
    void SetViewEols (bool bViewEols, bool bDistinguishEols);
    int GetTabSize ();
    void SetTabSize (int nTabSize);
    bool GetTopMargin () const { return m_bTopMargin; }
    void SetTopMargin (bool bTopMargin);
    bool GetSelectionMargin () const { return m_bSelMargin; }
    void SetSelectionMargin (bool bSelMargin);
    bool GetViewLineNumbers() const { return m_bViewLineNumbers; }
    void SetViewLineNumbers(bool bViewLineNumbers);
    void GetFont (LOGFONT & lf) const { lf = m_lfBaseFont; }
    void SetFont (const LOGFONT & lf);
    DWORD GetFlags () const { return m_dwFlags; }
    void SetFlags (DWORD dwFlags);
    bool GetSmoothScroll () const { return m_bSmoothScroll; }
    void SetSmoothScroll (bool bSmoothScroll) { m_bSmoothScroll = bSmoothScroll; }
    //  [JRT]:
    bool GetDisableDragAndDrop () const { return m_bDisableDragAndDrop; }
    void SetDisableDragAndDrop (bool bDDAD) { m_bDisableDragAndDrop = bDDAD; }

    static RENDERING_MODE GetRenderingModeDefault() { return s_nRenderingModeDefault;  }
    static void SetRenderingModeDefault(RENDERING_MODE nRenderingMode) { s_nRenderingModeDefault = nRenderingMode;  }
    RENDERING_MODE GetRenderingMode() const { return m_nRenderingMode;  }
    void SetRenderingMode(RENDERING_MODE nRenderingMode);

    //BEGIN SW
    bool GetWordWrapping() const;
    virtual void SetWordWrapping( bool bWordWrap );

    virtual void CopyProperties(CCrystalTextView *pSource);

    /**
    Sets the Parser to use to parse the file.

    @param pParser Pointer to parser to use. Set to `nullptr` to use no parser.

    @return Pointer to parser used before or `nullptr`, if no parser has been used before.
    */
    CCrystalParser *SetParser( CCrystalParser *pParser );
    //END SW

    bool GetEnableHideLines () const { return m_bHideLines; }
    void SetEnableHideLines (bool bHideLines) { m_bHideLines = bHideLines; }
    bool GetLineVisible (int nLineIndex) const;

    //  Default handle to resources
    static HINSTANCE s_hResourceInst;

    int m_nLastFindWhatLen;
    RxNode *m_rxnode;
    RxMatchRes m_rxmatch;
    LPTSTR m_pszMatched;
    static LOGFONT m_LogFont;
    static RENDERING_MODE s_nRenderingModeDefault;
    RENDERING_MODE m_nRenderingMode;

    std::unique_ptr<CCrystalRenderer> m_pCrystalRenderer;
    CCrystalRenderer *m_pCrystalRendererSaved;

    //  Source type
    CrystalLineParser::TextDefinition *m_CurSourceDef;
    bool m_bRememberLastPos;
    virtual bool DoSetTextType (CrystalLineParser::TextDefinition *def);
    virtual bool SetTextType (LPCTSTR pszExt);
    virtual bool SetTextType (CrystalLineParser::TextType enuType);
    virtual bool SetTextType (CrystalLineParser::TextDefinition *def);
    virtual bool SetTextTypeByContent (LPCTSTR pszContent);
    static void LoadSettings ();
    static void SaveSettings ();

    // Operations
public :
    virtual void ReAttachToBuffer (CCrystalTextBuffer * pBuf = nullptr);
    virtual void AttachToBuffer (CCrystalTextBuffer * pBuf = nullptr);
    virtual void DetachFromBuffer ();

    //  Buffer-view interaction, multiple views
    virtual CCrystalTextBuffer *LocateTextBuffer ();
    virtual void UpdateView (CCrystalTextView * pSource, CUpdateContext * pContext, DWORD dwFlags, int nLineIndex = -1);

    //  Attributes
    CPoint GetCursorPos () const { return m_ptCursorPos; }
    virtual void SetCursorPos (const CPoint & ptCursorPos);
    void ShowCursor ();
    void HideCursor ();
    CPoint GetAnchor() const { return m_ptAnchor; }
    void SetNewAnchor (const CPoint & ptNewAnchor) { SetAnchor(ptNewAnchor); }
    void SetNewSelection (const CPoint & ptStart, const CPoint & ptEnd, bool bUpdateView = true) { SetSelection(ptStart, ptEnd, bUpdateView); }

    //  Operations
    virtual void EnsureVisible (CPoint pt);
    virtual void EnsureVisible (CPoint ptStart, CPoint ptEnd);

    //  Text search helpers
    CPoint GetSearchPos (DWORD dwSearchFlags);
    bool FindText (LPCTSTR pszText, const CPoint & ptStartPos, DWORD dwFlags, bool bWrapSearch, CPoint * pptFoundPos);
    bool FindTextInBlock (LPCTSTR pszText, const CPoint & ptStartPos, const CPoint & ptBlockBegin, const CPoint & ptBlockEnd,
                          DWORD dwFlags, bool bWrapSearch, CPoint * pptFoundPos);
    bool FindText (const LastSearchInfos * lastSearch);
    bool HighlightText (const CPoint & ptStartPos, int nLength,
      bool bCursorToLeft = false);

    // IME (input method editor)
    void UpdateCompositionWindowPos();
    void UpdateCompositionWindowFont();

    //  Overridable: an opportunity for Auto-Indent, Smart-Indent etc.
    virtual void OnEditOperation (int nAction, LPCTSTR pszText, size_t cchText);

    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CCrystalTextView)
public :
    virtual void OnDraw (CDC * pDC);  // overridden to draw this view

    virtual BOOL PreCreateWindow (CREATESTRUCT & cs);
    virtual BOOL PreTranslateMessage (MSG * pMsg);
    virtual void OnPrepareDC (CDC * pDC, CPrintInfo * pInfo = nullptr);
    virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO *pHandlerInfo);
protected :
    virtual void OnInitialUpdate ();  // called first time after construct

    virtual BOOL OnPreparePrinting (CPrintInfo * pInfo);
    virtual void OnBeginPrinting (CDC * pDC, CPrintInfo * pInfo);
    virtual void OnEndPrinting (CDC * pDC, CPrintInfo * pInfo);
    virtual void OnPrint (CDC * pDC, CPrintInfo * pInfo);
    //}}AFX_VIRTUAL

    // Implementation
public :
    CCrystalTextView ();
    ~CCrystalTextView ();

protected :

    // Generated message map functions
protected :
#ifdef _DEBUG
    void AssertValidTextPos (const CPoint & pt);
#endif

    //{{AFX_MSG(CCrystalTextView)
    afx_msg void OnDestroy ();
    afx_msg BOOL OnEraseBkgnd (CDC * pDC);
    afx_msg void OnSize (UINT nType, int cx, int cy);
    afx_msg void OnVScroll (UINT nSBCode, UINT nPos, CScrollBar * pScrollBar);
    afx_msg BOOL OnSetCursor (CWnd * pWnd, UINT nHitTest, UINT message);
    afx_msg void OnLButtonDown (UINT nFlags, CPoint point);
    afx_msg void OnSetFocus (CWnd * pOldWnd);
    afx_msg void OnHScroll (UINT nSBCode, UINT nPos, CScrollBar * pScrollBar);
    afx_msg void OnLButtonUp (UINT nFlags, CPoint point);
    afx_msg void OnMouseMove (UINT nFlags, CPoint point);
    afx_msg void OnTimer (UINT_PTR nIDEvent);
    afx_msg void OnKillFocus (CWnd * pNewWnd);
    afx_msg void OnLButtonDblClk (UINT nFlags, CPoint point);
    afx_msg void OnLButtonTrippleClk (UINT nFlags, CPoint point);
    afx_msg void OnEditCopy ();
    afx_msg void OnUpdateEditCopy (CCmdUI * pCmdUI);
    afx_msg void OnEditSelectAll ();
    afx_msg void OnRButtonDown (UINT nFlags, CPoint point);
    afx_msg void OnSysColorChange ();
    afx_msg int OnCreate (LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnEditFind ();
    afx_msg void OnEditRepeat ();
    afx_msg void OnUpdateEditRepeat (CCmdUI * pCmdUI);
    afx_msg void OnEditMark ();
    afx_msg void OnEditDeleteBack();
    afx_msg void OnChar( wchar_t nChar, UINT nRepCnt, UINT nFlags );

    afx_msg BOOL OnMouseWheel (UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnMouseHWheel (UINT nFlags, short zDelta, CPoint pt);
    LRESULT OnImeStartComposition(WPARAM wParam, LPARAM lParam);
    //}}AFX_MSG
    afx_msg void OnFilePageSetup ();

    afx_msg void OnCharLeft ();
    afx_msg void OnExtCharLeft ();
    afx_msg void OnCharRight ();
    afx_msg void OnExtCharRight ();
    afx_msg void OnWordLeft ();
    afx_msg void OnExtWordLeft ();
    afx_msg void OnWordRight ();
    afx_msg void OnExtWordRight ();
    afx_msg void OnLineUp ();
    afx_msg void OnExtLineUp ();
    afx_msg void OnLineDown ();
    afx_msg void OnExtLineDown ();
    afx_msg void OnPageUp ();
    afx_msg void OnExtPageUp ();
    afx_msg void OnPageDown ();
    afx_msg void OnExtPageDown ();
    afx_msg void OnLineEnd ();
    afx_msg void OnExtLineEnd ();
    afx_msg void OnHome ();
    afx_msg void OnExtHome ();
    afx_msg void OnTextBegin ();
    afx_msg void OnExtTextBegin ();
    afx_msg void OnTextEnd ();
    afx_msg void OnExtTextEnd ();
    afx_msg void OnUpdateIndicatorCRLF (CCmdUI * pCmdUI);
    afx_msg void OnUpdateIndicatorPosition (CCmdUI * pCmdUI);
    afx_msg void OnToggleBookmark (UINT nCmdID);
    afx_msg void OnGoBookmark (UINT nCmdID);
    afx_msg void OnClearBookmarks ();

    afx_msg void OnToggleBookmark ();     // More bookmarks

    afx_msg void OnClearAllBookmarks ();
    afx_msg void OnNextBookmark ();
    afx_msg void OnPrevBookmark ();
    afx_msg void OnUpdateClearAllBookmarks (CCmdUI * pCmdUI);
    afx_msg void OnUpdateNextBookmark (CCmdUI * pCmdUI);
    afx_msg void OnUpdatePrevBookmark (CCmdUI * pCmdUI);

    afx_msg void ScrollUp ();
    afx_msg void ScrollDown ();
    afx_msg void ScrollLeft ();
    afx_msg void ScrollRight ();

    afx_msg void OnSourceType (UINT nId);
    afx_msg void OnUpdateSourceType (CCmdUI * pCmdUI);
    afx_msg void OnMatchBrace ();
    afx_msg void OnUpdateMatchBrace (CCmdUI * pCmdUI);
    afx_msg void OnEditGoTo ();
    afx_msg void OnUpdateToggleSourceHeader (CCmdUI * pCmdUI);
    afx_msg void OnToggleSourceHeader ();
    afx_msg void OnUpdateTopMargin (CCmdUI * pCmdUI);
    afx_msg void OnTopMargin ();
    afx_msg void OnUpdateSelMargin (CCmdUI * pCmdUI);
    afx_msg void OnSelMargin ();
    afx_msg void OnUpdateWordWrap (CCmdUI * pCmdUI);
    afx_msg void OnWordWrap ();
    afx_msg void OnForceRedraw ();

    //BEGIN SW
    // incremental search
    afx_msg void OnEditFindIncrementalForward();
    afx_msg void OnEditFindIncrementalBackward();
    afx_msg void OnUpdateEditFindIncrementalForward(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditFindIncrementalBackward(CCmdUI* pCmdUI);
    //END SW

    afx_msg void OnToggleColumnSelection ();
    afx_msg LRESULT OnDpiChangedBeforeParent(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP ()
  };

#ifdef _DEBUG
#define ASSERT_VALIDTEXTPOS(pt)     AssertValidTextPos(pt);
#else
#define ASSERT_VALIDTEXTPOS(pt)
#endif


inline bool CCrystalTextView::IsDraggingText () const
{
  return m_bDraggingText;
}
