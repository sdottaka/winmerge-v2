//*************************************************************************
// BCMenu.h : header file
// Version : 3.036
// Date : June 2005
// Author : Brent Corkum
// Email :  corkum@rocscience.com
// Latest Version : http://www.rocscience.com/~corkum/BCMenu.html
// 
// Bug Fixes and portions of code supplied by:
//
// Ben Ashley,Girish Bharadwaj,Jean-Edouard Lachand-Robert,
// Robert Edward Caldecott,Kenny Goers,Leonardo Zide,
// Stefan Kuhr,Reiner Jung,Martin Vladic,Kim Yoo Chul,
// Oz Solomonovich,Tongzhe Cui,Stephane Clog,Warren Stevens,
// Damir Valiulin,David Kinder,Marc Loiry
//
// You are free to use/modify this code but leave this header intact.
// This class is public domain so you are free to use it any of
// your applications (Freeware,Shareware,Commercial). All I ask is
// that you let me know so that if you have a real winner I can
// brag to my buddies that some of my code is in your app. I also
// wouldn't mind if you sent me a copy of your application since I
// like to play with new stuff.
//*************************************************************************

#pragma once

#include <afxtempl.h>

// BCMenuData class. Fill this class structure to define a single menu item:
class BCMenuData
{
	wchar_t *m_szMenuText;
public:
	BCMenuData () {pContext=nullptr;
	nFlags=0;nID=0;syncflag=0;m_szMenuText=nullptr;global_offset=-1;};
	void SetWideString(const wchar_t *szWideString);
	const wchar_t *GetWideString(void) {return m_szMenuText;};
	~BCMenuData ();
	CString GetString(void);//returns the menu text
	INT_PTR global_offset;
	UINT nFlags,syncflag;
	UINT_PTR nID;
	void *pContext; // used to attach user data
};

struct CMenuItemInfo : public MENUITEMINFO 
{
	CMenuItemInfo() : MENUITEMINFO{ sizeof(MENUITEMINFO) }
	{
	}
};

class BCMenu : public CMenu
{
	DECLARE_DYNAMIC( BCMenu )
public:
	BCMenu(); 
	virtual ~BCMenu();

	// Functions for loading and applying bitmaps to menus (see example application)
	virtual BOOL LoadMenu(LPCTSTR lpszResourceName);
	virtual BOOL LoadMenu(int nResource)
	{
		return BCMenu::LoadMenu(MAKEINTRESOURCE(nResource));
	}

	bool LoadToolbar(UINT nToolBar);
	bool AddBitmapToImageList(CImageList *list,UINT nResourceID);
	bool ReplaceBitmapInImageList(CImageList *list,int xoffset,UINT nResourceID);
	static HBITMAP LoadSysColorBitmap(int nResourceId);
	
	bool AppendMenu(UINT nFlags, UINT_PTR nIDNewItem = 0, const wchar_t* lpszNewItem = nullptr, int nIconNormal = -1)
	{
		return AppendODMenu(lpszNewItem, nFlags, nIDNewItem, nIconNormal);
	}
	bool AppendODMenu(const wchar_t *lpstrText,UINT nFlags = MF_OWNERDRAW,UINT_PTR nID = 0,int nIconNormal = -1);  
	
	// functions for inserting a menu option, use the InsertMenu call (see above define)
	bool InsertMenu(UINT nPosition, UINT nFlags, UINT_PTR nIDNewItem = 0, wchar_t* lpszNewItem = nullptr, int nIconNormal= -1)
	{
		return InsertODMenu(nPosition, lpszNewItem, nFlags, nIDNewItem, nIconNormal);
	}
	bool InsertODMenu(UINT nPosition,wchar_t *lpstrText,UINT nFlags = MF_OWNERDRAW,UINT_PTR nID = 0,int nIconNormal = -1);  

	// functions for modifying a menu option, use the ModifyODMenu call (see above define)
	bool ModifyODMenu(wchar_t *lpstrText,UINT_PTR nID=0,int nIconNormal=-1);

	// for deleting and removing menu options
	bool	RemoveMenu(UINT uiId,UINT nFlags);
	bool	DeleteMenu(UINT uiId,UINT nFlags);
	
	// Destoying
	virtual BOOL DestroyMenu();

	// function for retrieving and setting a menu options text (use this function
	// because it is ownerdrawn)
	bool GetMenuText(UINT id,CString &string,UINT nFlags = MF_BYPOSITION);
	bool SetMenuText(UINT id,CString string, UINT nFlags = MF_BYPOSITION);

	// Drawing: 
	virtual void DrawItem( LPDRAWITEMSTRUCT);  // Draw an item
	virtual void MeasureItem( LPMEASUREITEMSTRUCT );  // Measure an item

	// Static functions used for handling menu's in the mainframe
	static bool ReopenTheme(int dpi);
	static void UpdateMenu(CMenu *pmenu);
	static bool IsMenu(CMenu *submenu)
	{
		return IsMenu(submenu->m_hMenu);
	}
	static bool IsMenu(HMENU submenu);
	static LRESULT FindKeyboardShortcut(UINT nChar,UINT nFlags,CMenu *pMenu);

	// Customizing:
	// Set icon size
	static void SetIconSize (int width, int height)
	{
		m_iconX = width;
		m_iconY = height;
	}

	// set the color in the bitmaps that is the background transparent color
	void SetBitmapBackground(COLORREF color)
	{
		m_bitmapBackground=color;
		m_bitmapBackgroundFlag=true;
	}
	void UnSetBitmapBackground(void)
	{
		m_bitmapBackgroundFlag=false;
	}

	COLORREF GetBitmapBackground() const { return m_bitmapBackgroundFlag ? m_bitmapBackground : GetSysColor(COLOR_3DFACE); }
	// obsolete functions for setting how menu images are dithered for disabled menu options
	static inline COLORREF LightenColor(COLORREF col,double factor);

public:
	// Miscellaneous Protected Member functions
protected:
	BCMenuData *FindMenuItem(UINT_PTR nID);
	BCMenu *FindMenuOption(int nId,UINT& nLoc);
	BCMenu *FindAnotherMenuOption(int nId,UINT& nLoc,CArray<BCMenu*,BCMenu*>&bcsubs,
								  CArray<UINT,UINT&>&bclocs);
	BCMenuData *FindMenuOption(wchar_t *lpstrText);
	void DrawCheckMark(CDC* pDC,int x,int y,COLORREF color,bool narrowflag=false);
	void DrawRadioDot(CDC *pDC,int x,int y,COLORREF color);
	BCMenuData *NewODMenu(UINT pos,UINT nFlags,UINT_PTR nID,CString string);
	void SynchronizeMenu(void);
	void InitializeMenuList(int value);
	void DeleteMenuList(void);
	BCMenuData *FindMenuList(UINT_PTR nID);
	void DrawItem_Win9xNT2000 (LPDRAWITEMSTRUCT lpDIS);
	bool Draw3DCheckmark(CDC *dc, const CRect& rc,bool bSelected,HBITMAP hbmCheck);
	void DrawItem_Theme (LPDRAWITEMSTRUCT lpDIS);
	void DitherBlt2(CDC *drawdc, int nXDest, int nYDest, int nWidth, 
		int nHeight, CBitmap &bmp, int nXSrc, int nYSrc,COLORREF bgcolor);
	void DitherBlt3(CDC *drawdc, int nXDest, int nYDest, int nWidth, 
		int nHeight, CBitmap &bmp,COLORREF bgcolor);
	bool GetBitmapFromImageList(CDC* pDC,int nIndex,CBitmap &bmp);
	bool GetBitmapFromImageList(CDC* pDC,int nIndex,CImage &bmp);
	static WORD NumBitmapColors(LPBITMAPINFOHEADER lpBitmap);
	void RemoveTopLevelOwnerDraw(void);
	int GetMenuStart(void);
	void GetTransparentBitmap(CBitmap &bmp);
	void GetDisabledBitmap(CBitmap &bmp,COLORREF background=0);
	void GetDisabledBitmap(CImage &bmp);
	INT_PTR AddToGlobalImageList(int nIconNormal,int nID);
	int GlobalImageListOffset(int nID);
	void LoadImages();
	
// Member Variables
protected:
	CTypedPtrArray<CPtrArray, BCMenuData*> m_MenuList;  // Stores list of menu items 
	// When loading an owner-drawn menu using a Resource, BCMenu must keep track of
	// the popup menu's that it creates. Warning, this list *MUST* be destroyed
	// last item first :)
	CTypedPtrArray<CPtrArray, HMENU>  m_SubMenus;  // Stores list of sub-menus 
	// Stores a list of all BCMenu's ever created 
	static CTypedPtrArray<CPtrArray, HMENU>  m_AllSubMenus;
	// Global ImageList
	static CImageList m_AllImages;
	struct ImageData { int id; int resourceId; int bitmapIndex; };
	static std::vector<ImageData> m_AllImagesID;
	static bool m_bHasNotLoadedImages;
	// icon size
	static int m_iconX;
	static int m_iconY;
	COLORREF m_bitmapBackground;
	bool m_bitmapBackgroundFlag;
	static bool hicolor_bitmaps;
	bool m_loadmenu;
	static MARGINS m_marginCheck;
	static MARGINS m_marginSeparator;
	static SIZE m_sizeCheck;
	static SIZE m_sizeSeparator;
	static int m_textBorder;
	static int m_checkBgWidth;
	static int m_gutterWidth;
	static int m_arrowWidth;
	static HTHEME m_hTheme;
}; 

