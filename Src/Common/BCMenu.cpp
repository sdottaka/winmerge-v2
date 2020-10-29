//*************************************************************************
// BCMenu.cpp : implementation file
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

#include "stdafx.h"        // Standard windows header file
#include "BCMenu.h"        // BCMenu class declaration
#include "utils/DpiAware.h"
#include <afxpriv.h>       //SK: makes A2W and other spiffy AFX macros work

#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define BCMENU_GAP 1

bool BCMenu::hicolor_bitmaps=false;

CImageList BCMenu::m_AllImages;
bool BCMenu::m_bHasNotLoadedImages = false;
std::vector<BCMenu::ImageData> BCMenu::m_AllImagesID;
int BCMenu::m_iconX = 16;
int BCMenu::m_iconY = 15;
MARGINS BCMenu::m_marginCheck = { 0 };
MARGINS BCMenu::m_marginSeparator = { 0 };
SIZE BCMenu::m_sizeCheck = { 0 };
SIZE BCMenu::m_sizeSeparator = { 0 };
int BCMenu::m_textBorder = 0;
int BCMenu::m_checkBgWidth = 0;
int BCMenu::m_gutterWidth = 0;
int BCMenu::m_arrowWidth = 0;
HTHEME BCMenu::m_hTheme = nullptr;

static class GdiplusToken
{
public:
	GdiplusToken()
	{
	}

	~GdiplusToken()
	{
		if (m_token != 0)
			Gdiplus::GdiplusShutdown(m_token);
	}

	void InitGdiplus()
	{
		if (m_token == 0)
		{
			Gdiplus::GdiplusStartupInput gdiplusStartupInput;
			Gdiplus::GdiplusStartup(&m_token, &gdiplusStartupInput, nullptr);
		}
	}

private:
	ULONG_PTR m_token = 0;
} m_gdiplusToken;

// The Representation of a 32 bit color table entry
#pragma pack(push)
#pragma pack(1)
typedef struct ssBGR {
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char pad;
} sBGR;

typedef sBGR *pBGR;
#pragma pack(pop)


// Returns the DI (Device Independent) bits of the Bitmap
// Here I use 32 bit since it's easy to adress in memory and no
// padding of the horizontal lines is required.
static pBGR MyGetDibBits(HDC hdcSrc, HBITMAP hBmpSrc, int nx, int ny)
{
	BITMAPINFO bi;
	int nRes;
	pBGR buf;

	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = nx;
	bi.bmiHeader.biHeight = ny;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = nx * 4 * ny;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;
	
	buf = (pBGR) malloc(nx * 4 * ny);
	nRes = GetDIBits(hdcSrc, hBmpSrc, 0, ny, buf, &bi, DIB_RGB_COLORS);
	if (nRes == 0) {
		free(buf);
		buf = nullptr;
	}
	return buf;
}

static void MySetDibBits(HDC hdcDst, HBITMAP hBmpDst, pBGR pdstBGR, int nx, int ny)
{
	BITMAPINFO bi;

	// Set the new Bitmap
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = nx;
	bi.bmiHeader.biHeight = ny;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = nx * 4 * ny;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;
	SetDIBits(hdcDst, hBmpDst, 0, ny, pdstBGR, &bi, DIB_RGB_COLORS);
}

CString BCMenuData::GetString(void)//returns the menu text
//depending on the MFC-Version we are using
{
	CString strText;
	if (m_szMenuText)
    {
		strText = m_szMenuText;
    }
	return strText;
}

CTypedPtrArray<CPtrArray, HMENU> BCMenu::m_AllSubMenus;  // Stores list of all sub-menus

IMPLEMENT_DYNAMIC( BCMenu, CMenu )

/*
===============================================================================
BCMenu::BCMenu()
BCMenu::~BCMenu()
-----------------

Constructor and Destructor.

===============================================================================
*/

BCMenu::BCMenu()
{
	// set the color used for the transparent background in all bitmaps
	m_bitmapBackground=RGB(192,192,192); //gray
	m_bitmapBackgroundFlag=false;
	m_loadmenu=false;
	if (m_hTheme==nullptr && IsThemeActive())
		ReopenTheme(DpiAware::GetDpiForWindow(AfxGetMainWnd()->m_hWnd));
}


BCMenu::~BCMenu()
{
	BCMenu::DestroyMenu();
}

BCMenuData::~BCMenuData()
{
	delete[] m_szMenuText; //Need not check for nullptr because ANSI X3J16 allows "delete nullptr"
}


void BCMenuData::SetWideString(const wchar_t *szWideString)
{
	delete[] m_szMenuText;//Need not check for nullptr because ANSI X3J16 allows "delete nullptr"
	
	if (szWideString)
    {
		const size_t MenuSiz = wcslen(szWideString) + 1;
		m_szMenuText = new wchar_t[MenuSiz];
		if (m_szMenuText)
			wcscpy_s(m_szMenuText, MenuSiz, szWideString);
    }
	else
		m_szMenuText=nullptr;//set to nullptr so we need not bother about dangling non-nullptr Ptrs
}

bool BCMenu::ReopenTheme(int dpi)
{
	if (!IsThemeActive())
		return false;
	if (m_hTheme != nullptr)
		CloseThemeData(m_hTheme);
	m_hTheme = DpiAware::OpenThemeDataForDpi(nullptr, _T("MENU"), dpi);
	if (m_hTheme == nullptr)
		return false;
	MARGINS marginCheckBg, marginArrow;	
	GetThemePartSize(m_hTheme, nullptr, MENU_POPUPCHECK, 0, nullptr, TS_TRUE, &m_sizeCheck);
	GetThemeMargins(m_hTheme, nullptr, MENU_POPUPCHECK, 0, TMT_CONTENTMARGINS, nullptr, &m_marginCheck);
	GetThemePartSize(m_hTheme, nullptr, MENU_POPUPSEPARATOR, 0, nullptr, TS_TRUE, &m_sizeSeparator); 
	GetThemeMargins(m_hTheme, nullptr, MENU_POPUPSEPARATOR, 0, TMT_SIZINGMARGINS, nullptr, &m_marginSeparator);
	GetThemeMargins(m_hTheme, nullptr, MENU_POPUPCHECKBACKGROUND, 0, TMT_CONTENTMARGINS, nullptr, &marginCheckBg);
	GetThemeMargins(m_hTheme, nullptr, MENU_POPUPSUBMENU, 0, TMT_CONTENTMARGINS, nullptr, &marginArrow);
	GetThemeInt(m_hTheme, MENU_POPUPBACKGROUND, 0, TMT_BORDERSIZE, &m_textBorder);
	m_checkBgWidth = m_marginCheck.cxLeftWidth + m_sizeCheck.cx + m_marginCheck.cxRightWidth;
	m_gutterWidth = marginCheckBg.cxLeftWidth + m_checkBgWidth + marginCheckBg.cxRightWidth;
	m_arrowWidth = marginArrow.cxRightWidth;
	return true;
}

bool BCMenu::IsMenu(HMENU submenu)
{
	INT_PTR m;
	INT_PTR numSubMenus = m_AllSubMenus.GetUpperBound();
	for(m=0;m<=numSubMenus;++m){
		if(submenu==m_AllSubMenus[m] || 
		  static_cast<UINT>(reinterpret_cast<uintptr_t>(submenu)) == static_cast<UINT>(reinterpret_cast<uintptr_t>(m_AllSubMenus[m])))
			return true;
	}
	return false;
}

BOOL BCMenu::DestroyMenu()
{
	// Destroy Sub menus:
	INT_PTR m,n;
	INT_PTR numAllSubMenus = m_AllSubMenus.GetUpperBound();
	for(n = numAllSubMenus; n>= 0; n--){
		if(m_AllSubMenus[n]==this->m_hMenu)m_AllSubMenus.RemoveAt(n);
	}
	INT_PTR numSubMenus = m_SubMenus.GetUpperBound();
	for(m = numSubMenus; m >= 0; m--){
		numAllSubMenus = m_AllSubMenus.GetUpperBound();
		for(n = numAllSubMenus; n>= 0; n--){
			if(m_AllSubMenus[n]==m_SubMenus[m])m_AllSubMenus.RemoveAt(n);
		}
		CMenu *ptr=FromHandle(m_SubMenus[m]);
		if(ptr != nullptr){
			bool flag = !!ptr->IsKindOf(RUNTIME_CLASS( BCMenu ));
			if(flag)delete(static_cast<BCMenu *>(ptr));
		}
	}
	m_SubMenus.RemoveAll();
	// Destroy menu data
	INT_PTR numItems = m_MenuList.GetUpperBound();
	for(m = 0; m <= numItems; m++)delete(m_MenuList[m]);
	m_MenuList.RemoveAll();
	// Call base-class implementation last:
	return CMenu::DestroyMenu();
};

/*
==========================================================================
void BCMenu::DrawItem(LPDRAWITEMSTRUCT)
---------------------------------------

  Called by the framework when a particular item needs to be drawn.  We
  overide this to draw the menu item in a custom-fashion, including icons
  and the 3D rectangle bar.
  ==========================================================================
*/

void BCMenu::DrawItem (LPDRAWITEMSTRUCT lpDIS)
{
	ASSERT(lpDIS != nullptr);
	LoadImages();
	CDC* pDC = CDC::FromHandle(lpDIS->hDC);
	if((pDC->GetDeviceCaps(RASTERCAPS) & RC_PALETTE) != 0)DrawItem_Win9xNT2000(lpDIS);
	else{
		if (m_hTheme == nullptr || !IsThemeActive())
			DrawItem_Win9xNT2000(lpDIS);
		else
			DrawItem_Theme(lpDIS);
	}
}

void BCMenu::DrawItem_Win9xNT2000 (LPDRAWITEMSTRUCT lpDIS)
{
	ASSERT(lpDIS != nullptr);
	CDC* pDC = CDC::FromHandle(lpDIS->hDC);
	CRect rect;
	UINT state0 = reinterpret_cast<BCMenuData*>(lpDIS->itemData)->nFlags;
	COLORREF clrBack=GetSysColor(COLOR_MENU);
	CBrush brBackground(clrBack);

	if((state0 & MF_SEPARATOR) != 0){
		rect.CopyRect(&lpDIS->rcItem);
		pDC->FillRect (rect,&brBackground);
		rect.top += (rect.Height()>>1);
		pDC->DrawEdge(&rect,EDGE_ETCHED,BF_TOP);
	}
	else{
		CRect rect2;
		bool standardflag = false;
		bool selectedflag = false;
		bool disableflag = false;
		bool checkflag=false;
		COLORREF crText = GetSysColor(COLOR_MENUTEXT);
		int dy;
		INT_PTR xoffset=-1;
		
		// set some colors
		CPen penBack(PS_SOLID,0,clrBack);
		CBrush brSelect(GetSysColor(COLOR_HIGHLIGHT));
		
		// draw the colored rectangle portion
		
		rect.CopyRect(&lpDIS->rcItem);
		rect2=rect;
		
		// draw the up/down/focused/disabled state
		
		UINT state = lpDIS->itemState;
		CString strText;
		
		if(lpDIS->itemData != NULL){
			BCMenuData *mdata = reinterpret_cast<BCMenuData *>(lpDIS->itemData);
			strText = mdata->GetString();

			xoffset=mdata->global_offset;
			
			if((state&ODS_CHECKED)!=0 && xoffset<0){
			}
			else if(xoffset != -1){
				standardflag=true;
				if((state&ODS_SELECTED)!=0 && (state&ODS_GRAYED)==0)
					selectedflag=true;
				else 
				if((state&ODS_GRAYED)!=0) 
					disableflag=true;
			}
		}
		else{
			strText.Empty();
		}
		
		if((state&ODS_SELECTED)!=0){ // draw the down edges
			
			CPen *pOldPen = pDC->SelectObject (&penBack);
			
			// You need only Text highlight and thats what you get
			
			if(checkflag||standardflag||selectedflag||disableflag||(state&ODS_CHECKED)!=0)
				rect2.SetRect(rect.left+m_iconX+4+BCMENU_GAP,rect.top,rect.right,rect.bottom);
			pDC->FillRect (rect2,&brSelect);
			
			pDC->SelectObject (pOldPen);
			crText = GetSysColor(COLOR_HIGHLIGHTTEXT);
		}
		else {
			CPen *pOldPen = pDC->SelectObject (&penBack);
			pDC->FillRect (rect,&brBackground);
			pDC->SelectObject (pOldPen);
			
			// draw the up edges	
			pDC->Draw3dRect (rect,clrBack,clrBack);
		}
		
		// draw the text if there is any
		//We have to paint the text only if the image is nonexistant
		
		dy = (rect.Height()-4-m_iconY)/2;
		dy = dy<0 ? 0 : dy;
		
		if(checkflag||standardflag||selectedflag||disableflag){
			rect2.SetRect(rect.left+1,rect.top+1+dy,rect.left+m_iconX+3,
				rect.top+m_iconY+3+dy);
			pDC->Draw3dRect (rect2,clrBack,clrBack);
			if(disableflag){
				if(!selectedflag){
					CBitmap bitmapstandard;
					GetBitmapFromImageList(pDC,(int)xoffset,bitmapstandard);
					rect2.SetRect(rect.left,rect.top+dy,rect.left+m_iconX+4,
                        rect.top+m_iconY+4+dy);
					pDC->Draw3dRect (rect2,clrBack,clrBack);
					if(hicolor_bitmaps)
						DitherBlt3(pDC,rect.left+2,rect.top+2+dy,m_iconX,m_iconY,
						bitmapstandard,clrBack);
					else
						DitherBlt2(pDC,rect.left+2,rect.top+2+dy,m_iconX,m_iconY,
						bitmapstandard,0,0,clrBack);
				}
			}
			else if(selectedflag){
				pDC->FillRect (rect2,&brBackground);
				rect2.SetRect(rect.left,rect.top+dy,rect.left+m_iconX+4,
					rect.top+m_iconY+4+dy);
				if(state&ODS_CHECKED)
					pDC->Draw3dRect(rect2,GetSysColor(COLOR_3DSHADOW),
					GetSysColor(COLOR_3DHILIGHT));
				else
					pDC->Draw3dRect(rect2,GetSysColor(COLOR_3DHILIGHT),
					GetSysColor(COLOR_3DSHADOW));
				CPoint ptImage(rect.left+2,rect.top+2+dy);
				if(xoffset >= 0) m_AllImages.Draw(pDC,(int)xoffset,ptImage,ILD_TRANSPARENT);
			}
			else{
				if(state&ODS_CHECKED){
					CBrush cbTemp = LightenColor(clrBack, 0.6);
					pDC->FillRect(rect2,&cbTemp);
					rect2.SetRect(rect.left,rect.top+dy,rect.left+m_iconX+4,
                        rect.top+m_iconY+4+dy);
					pDC->Draw3dRect(rect2,GetSysColor(COLOR_3DSHADOW),
					GetSysColor(COLOR_3DHILIGHT));
				}
				else{
					pDC->FillRect (rect2,&brBackground);
					rect2.SetRect(rect.left,rect.top+dy,rect.left+m_iconX+4,
                        rect.top+m_iconY+4+dy);
					pDC->Draw3dRect (rect2,clrBack,clrBack);
				}
				CPoint ptImage(rect.left+2,rect.top+2+dy);
				if(xoffset >= 0) m_AllImages.Draw(pDC,(int)xoffset,ptImage,ILD_TRANSPARENT);
			}
		}
		if(xoffset<0 && (state&ODS_CHECKED)!=0 && !checkflag){
			rect2.SetRect(rect.left+1,rect.top+2+dy,rect.left+m_iconX+1,
				rect.top+m_iconY+2+dy);
			CMenuItemInfo info;
			info.fMask = MIIM_CHECKMARKS;
			::GetMenuItemInfo((HMENU)lpDIS->hwndItem,lpDIS->itemID,
				MF_BYCOMMAND, &info);
			if((state&ODS_CHECKED)!=0 || info.hbmpUnchecked!=nullptr) {
				Draw3DCheckmark(pDC, rect2, (state&ODS_SELECTED)!=0,
					(state&ODS_CHECKED)!=0 ? info.hbmpChecked : info.hbmpUnchecked);
			}
		}
		
		//This is needed always so that we can have the space for check marks
		
		rect.left = rect.left + m_iconX + 8 + BCMENU_GAP; 
		
		if(!strText.IsEmpty()){
			
			CRect rectt(rect.left,rect.top-1,rect.right,rect.bottom-1);
			
			//   Find tabs
			
			CString leftStr,rightStr;
			leftStr.Empty();rightStr.Empty();
			int tablocr=strText.ReverseFind(_T('\t'));
			if(tablocr!=-1){
				rightStr=strText.Mid(tablocr+1);
				leftStr=strText.Left(strText.Find(_T('\t')));
				rectt.right-=m_iconX;
			}
			else leftStr=strText;
			
			int iOldMode = pDC->GetBkMode();
			pDC->SetBkMode( TRANSPARENT);
			
			// Draw the text in the correct colour:
			
			UINT nFormat  = DT_LEFT|DT_SINGLELINE|DT_VCENTER;
			UINT nFormatr = DT_RIGHT|DT_SINGLELINE|DT_VCENTER;
			if((lpDIS->itemState & ODS_GRAYED)==0){
				pDC->SetTextColor(crText);
				pDC->DrawText (leftStr,rectt,nFormat);
				if(tablocr!=-1) pDC->DrawText (rightStr,rectt,nFormatr);
			}
			else{
				
				// Draw the disabled text
				if((state & ODS_SELECTED)==0){
					RECT offset = *rectt;
					offset.left+=1;
					offset.right+=1;
					offset.top+=1;
					offset.bottom+=1;
					pDC->SetTextColor(GetSysColor(COLOR_BTNHILIGHT));
					pDC->DrawText(leftStr,&offset, nFormat);
					if(tablocr!=-1) pDC->DrawText (rightStr,&offset,nFormatr);
					pDC->SetTextColor(GetSysColor(COLOR_GRAYTEXT));
					pDC->DrawText(leftStr,rectt, nFormat);
					if(tablocr!=-1) pDC->DrawText (rightStr,rectt,nFormatr);
				}
				else{
					// And the standard Grey text:
					pDC->SetTextColor(clrBack);
					pDC->DrawText(leftStr,rectt, nFormat);
					if(tablocr!=-1) pDC->DrawText (rightStr,rectt,nFormatr);
				}
			}
			pDC->SetBkMode( iOldMode );
		}
	}
}

inline COLORREF BCMenu::LightenColor(COLORREF col,double factor)
{
	if(factor>0.0&&factor<=1.0){
		BYTE red,green,blue,lightred,lightgreen,lightblue;
		red = GetRValue(col);
		green = GetGValue(col);
		blue = GetBValue(col);
		lightred = (BYTE)((factor*(255-red)) + red);
		lightgreen = (BYTE)((factor*(255-green)) + green);
		lightblue = (BYTE)((factor*(255-blue)) + blue);
		col = RGB(lightred,lightgreen,lightblue);
	}
	return col;
}

void BCMenu::DrawItem_Theme(LPDRAWITEMSTRUCT lpDIS)
{
	ASSERT(lpDIS != nullptr);
	CDC* pDC = CDC::FromHandle(lpDIS->hDC);
	HDC hDC = lpDIS->hDC;
	CRect rect(&lpDIS->rcItem);
	UINT state = lpDIS->itemState;
	CRect rectGutter(rect.left, rect.top, rect.left + m_gutterWidth, rect.bottom);

	const int stateId =
		(state & ODS_GRAYED)!=0 ?
			((state & ODS_SELECTED)!=0 ? MPI_DISABLEDHOT : MPI_DISABLED)
		:
			((state & ODS_SELECTED)!=0 ? MPI_HOT : MPI_NORMAL);

	DrawThemeBackground(m_hTheme, hDC, MENU_POPUPBACKGROUND, 0, &rect, nullptr);
	DrawThemeBackground(m_hTheme, hDC, MENU_POPUPGUTTER, 0, &rectGutter, nullptr);
	DrawThemeBackground(m_hTheme, hDC, MENU_POPUPITEM, stateId, &rect, nullptr);
	
	BCMenuData *mdata = reinterpret_cast<BCMenuData*>(lpDIS->itemData);
	if(mdata == nullptr)
		return;

	if ((mdata->nFlags & MF_SEPARATOR)!=0){
		CRect rectSeparator(rectGutter.right + m_marginSeparator.cxLeftWidth,
			rect.top + m_marginSeparator.cyTopHeight,
		    rect.right - m_marginSeparator.cxRightWidth,
			rect.top + m_marginSeparator.cyTopHeight + m_sizeSeparator.cy);
		DrawThemeBackground(m_hTheme, hDC, MENU_POPUPSEPARATOR, 0, &rectSeparator, nullptr);
		return;
	}

	INT_PTR xoffset = mdata->global_offset;
	CString	strText = mdata->GetString();

	const int dpi = DpiAware::GetDpiForWindow(AfxGetMainWnd()->m_hWnd);
	const int cxSMIcon = DpiAware::GetSystemMetricsForDpi(SM_CXSMICON, dpi);
	const int cySMIcon = DpiAware::GetSystemMetricsForDpi(SM_CYSMICON, dpi);

	if(xoffset >= 0){
		CImage bitmapstandard;
		GetBitmapFromImageList(pDC,(int)xoffset,bitmapstandard);
		if((state & ODS_GRAYED)!=0)
			GetDisabledBitmap(bitmapstandard);
		m_gdiplusToken.InitGdiplus();
		Gdiplus::Bitmap bm(bitmapstandard.GetWidth(), bitmapstandard.GetHeight(), 
			bitmapstandard.GetPitch(), PixelFormat32bppARGB, (BYTE *)bitmapstandard.GetBits());
		Gdiplus::Graphics dcDst(pDC->m_hDC);
		dcDst.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		Gdiplus::Rect rcDst(
			static_cast<int>(rect.left + (rectGutter.right - cxSMIcon) / 2.0 + 0.5),
			static_cast<int>(rect.top  + (rect.Height() - cySMIcon) / 2.0 + 0.5),
			cxSMIcon, cySMIcon);
		dcDst.DrawImage(&bm, rcDst, 0, 0, m_iconX, m_iconY, Gdiplus::UnitPixel);
	}
	if(xoffset < 0 && (state&ODS_CHECKED)!=0){
		CMenuItemInfo info;
		info.fMask = MIIM_CHECKMARKS;
		::GetMenuItemInfo((HMENU)lpDIS->hwndItem,lpDIS->itemID,
			MF_BYCOMMAND, &info);
		if((state&ODS_CHECKED)!=0 || (info.hbmpUnchecked != nullptr)) {
			int stateIdCheck = 0;
			if (info.hbmpChecked == nullptr)
				stateIdCheck = (state & ODS_GRAYED)!=0 ? MC_CHECKMARKDISABLED : MC_CHECKMARKNORMAL;
			else
				stateIdCheck = (state & ODS_GRAYED)!=0 ? MC_BULLETDISABLED : MC_BULLETNORMAL;
			int stateIdCheckBk = (state & ODS_GRAYED)!=0 ? MCB_DISABLED : MCB_NORMAL;
			CRect rectCheck(
				rect.left + m_marginCheck.cxLeftWidth,
				rect.top + m_marginCheck.cyTopHeight,
				rect.left + m_marginCheck.cxLeftWidth + m_sizeCheck.cx,
				rect.top + m_marginCheck.cyTopHeight + m_sizeCheck.cy);
			CRect rectCheckBg(rect.left,rect.top,rect.left+m_checkBgWidth,rect.bottom);
			DrawThemeBackground(m_hTheme, hDC, MENU_POPUPCHECKBACKGROUND, stateIdCheckBk, &rectCheckBg, nullptr);
			DrawThemeBackground(m_hTheme, hDC, MENU_POPUPCHECK, stateIdCheck, &rectCheck, nullptr);
		}
	}
	
	if(!strText.IsEmpty()){
		
		CRect rectt(rectGutter.right + m_textBorder, rect.top, rect.right, rect.bottom);
		
		//   Find tabs
		
		CString leftStr,rightStr;
		leftStr.Empty();rightStr.Empty();
		int tablocr=strText.ReverseFind(_T('\t'));
		if(tablocr!=-1){
			rightStr=strText.Mid(tablocr+1);
			leftStr=strText.Left(strText.Find(_T('\t')));
			rectt.right-=cxSMIcon;
		}
		else leftStr=strText;
		
		int iOldMode = pDC->GetBkMode();
		pDC->SetBkMode( TRANSPARENT);
		
		// Draw the text in the correct colour:
		
		UINT nFormat  = DT_LEFT|DT_SINGLELINE|DT_VCENTER;
		UINT nFormatr = DT_RIGHT|DT_SINGLELINE|DT_VCENTER;
		pDC->SetTextColor(
			((lpDIS->itemState & ODS_GRAYED)==0) ? GetSysColor(COLOR_MENUTEXT) : GetSysColor(COLOR_GRAYTEXT));
		pDC->DrawText (leftStr,rectt,nFormat);
		if(tablocr!=-1) pDC->DrawText (rightStr,rectt,nFormatr);
		pDC->SetBkMode( iOldMode );
	}
}

bool BCMenu::GetBitmapFromImageList(CDC* pDC,int nIndex,CImage &bmp)
{
	CDC dc;
	dc.CreateCompatibleDC(pDC);
	bmp.Create(m_iconX, -m_iconY, 32, CImage::createAlphaChannel);
	memset(bmp.GetBits(), 0xff, abs(bmp.GetPitch()) * m_iconY);
	HGDIOBJ pOldBmp = dc.SelectObject(bmp.operator HBITMAP());
	POINT pt = {0};
	SIZE  sz = {m_iconX, m_iconY};

	IMAGELISTDRAWPARAMS drawing;

	drawing.cbSize = IMAGELISTDRAWPARAMS_V3_SIZE;
	drawing.himl = m_AllImages.m_hImageList;
	drawing.i = nIndex;
	drawing.hdcDst = dc.m_hDC;
	drawing.x = pt.x;
	drawing.y = pt.y;
	drawing.cx = sz.cx;
	drawing.cy = sz.cy;
	drawing.xBitmap = pt.x;
	drawing.yBitmap = pt.y;
	drawing.rgbBk = CLR_NONE;
	drawing.rgbFg = CLR_DEFAULT;
	drawing.fStyle = ILD_NORMAL;
	drawing.dwRop = SRCCOPY;

	ImageList_DrawIndirect(&drawing);

	int pitch = bmp.GetPitch();
	BYTE *p = (BYTE *)bmp.GetBits();
	for (int y = 0; y < m_iconY; ++y)
	{
		for (int x = 0; x < m_iconX; ++x)
		{
			if (p[x * 4 + y * pitch + 3] == 0xff)
				p[x * 4 + y * pitch + 3] = 0;
			else
				p[x * 4 + y * pitch + 3] = 0xff;
		}
	}

	dc.SelectObject( pOldBmp );
	return true;
}

bool BCMenu::GetBitmapFromImageList(CDC* pDC,int nIndex,CBitmap &bmp)
{
	CDC dc;
	dc.CreateCompatibleDC(pDC);
	bmp.CreateCompatibleBitmap(pDC,m_iconX,m_iconY);
	CBitmap* pOldBmp = dc.SelectObject(&bmp);
	POINT pt = {0};
	SIZE  sz = {m_iconX, m_iconY};

	IMAGELISTDRAWPARAMS drawing;

	drawing.cbSize = IMAGELISTDRAWPARAMS_V3_SIZE;
	drawing.himl = m_AllImages.m_hImageList;
	drawing.i = nIndex;
	drawing.hdcDst = dc.m_hDC;
	drawing.x = pt.x;
	drawing.y = pt.y;
	drawing.cx = sz.cx;
	drawing.cy = sz.cy;
	drawing.xBitmap = pt.x;
	drawing.yBitmap = pt.y;
	drawing.rgbBk = GetSysColor(COLOR_3DFACE);
	drawing.rgbFg = CLR_DEFAULT;
	drawing.fStyle = ILD_NORMAL;
	drawing.dwRop = SRCCOPY;

	ImageList_DrawIndirect(&drawing);

	dc.SelectObject( pOldBmp );
	return true;
}

/*
==========================================================================
void BCMenu::MeasureItem(LPMEASUREITEMSTRUCT)
---------------------------------------------

  Called by the framework when it wants to know what the width and height
  of our item will be.  To accomplish this we provide the width of the
  icon plus the width of the menu text, and then the height of the icon.
  
	==========================================================================
*/

void BCMenu::MeasureItem( LPMEASUREITEMSTRUCT lpMIS )
{
	UINT state = reinterpret_cast<BCMenuData*>(lpMIS->itemData)->nFlags;
	if((state & MF_SEPARATOR)!=0){
		lpMIS->itemWidth = 0;
		if (m_hTheme != nullptr)
			lpMIS->itemHeight = m_marginSeparator.cyTopHeight + m_sizeSeparator.cy + m_marginSeparator.cyBottomHeight;
		else
			lpMIS->itemHeight = 3;
	}
	else{
		CFont fontMenu;
		LOGFONT lfMenuFont;
		const int dpi = DpiAware::GetDpiForWindow(AfxGetMainWnd()->m_hWnd);
		DpiAware::GetNonClientLogFont(lfMenuFont, offsetof(NONCLIENTMETRICS, lfMenuFont), dpi);
		fontMenu.CreateFontIndirect (&lfMenuFont);
		
		// Obtain the width of the text:
		CClientDC dc(AfxGetMainWnd() ? AfxGetMainWnd() : CWnd::GetDesktopWindow());     // Get device context
		CFont* pFont=nullptr;    // Select menu font in...
		
		pFont = dc.SelectObject (&fontMenu);// Select menu font in...
        
		//Get pointer to text SK
		const wchar_t *lpstrText = reinterpret_cast<BCMenuData*>(lpMIS->itemData)->GetWideString();//SK: we use const to prevent misuse
		    
		SIZE size;
		size.cx=size.cy=0;
		
		VERIFY(::GetTextExtentPoint32W(dc.m_hDC,lpstrText,
			lstrlenW(lpstrText),&size)); //SK should also work on 95
		CSize t = CSize(size);
		dc.SelectObject (pFont);  // Select old font in
		
		// Set width and height:
		
		const int BCMENU_PAD=4;
		if (m_hTheme == nullptr)
			lpMIS->itemWidth = MulDiv(m_iconX+BCMENU_PAD+8, dpi, USER_DEFAULT_SCREEN_DPI) +t.cx;
		else
			lpMIS->itemWidth = m_gutterWidth+m_textBorder+t.cx+m_arrowWidth;
		int temp = DpiAware::GetSystemMetricsForDpi(SM_CYMENU, dpi);
		int temp2 = MulDiv(m_iconY + BCMENU_PAD, dpi, USER_DEFAULT_SCREEN_DPI);
		lpMIS->itemHeight = temp>temp2 ? temp : temp2;
	}
}

bool BCMenu::AppendODMenu(const wchar_t *lpstrText,UINT nFlags,UINT_PTR nID,
                           int nIconNormal)
{
	// Add the MF_OWNERDRAW flag if not specified:
	if(nID == 0){
		if((nFlags&MF_BYPOSITION)!=0)
			nFlags=MF_SEPARATOR|MF_OWNERDRAW|MF_BYPOSITION;
		else 
			nFlags=MF_SEPARATOR|MF_OWNERDRAW;
	}
	else 
	if((nFlags & MF_OWNERDRAW)==0)
		nFlags |= MF_OWNERDRAW;
	
	if((nFlags & MF_POPUP)!=0){
		m_AllSubMenus.Add((HMENU)nID);
		m_SubMenus.Add((HMENU)nID);
	}
	
	BCMenuData *mdata = new BCMenuData;
	m_MenuList.Add(mdata);
	mdata->SetWideString(lpstrText);    //SK: modified for dynamic allocation
	
	if(nIconNormal>=0){
		mdata->global_offset = AddToGlobalImageList(nIconNormal,static_cast<int>(nID));
	}
	else mdata->global_offset = GlobalImageListOffset(static_cast<int>(nID));

	mdata->nFlags = nFlags;
	mdata->nID = nID;
	bool returnflag=!!CMenu::AppendMenu(nFlags, nID, (LPCTSTR)mdata);
	if(m_loadmenu)RemoveTopLevelOwnerDraw();
	return returnflag;
}

bool BCMenu::InsertODMenu(UINT nPosition,wchar_t *lpstrText,UINT nFlags,UINT_PTR nID,
                           int nIconNormal)
{
	if((nFlags & MF_BYPOSITION) == 0){
		UINT iPosition =0;
		BCMenu* pMenu = FindMenuOption(nPosition,iPosition);
		if(pMenu != nullptr){
			return pMenu->InsertODMenu(iPosition,lpstrText,nFlags|MF_BYPOSITION,nID,nIconNormal);
		}
		else return false;
	}
	
	if(nID==0)
		nFlags=MF_SEPARATOR|MF_OWNERDRAW|MF_BYPOSITION;
	else 
	if((nFlags & MF_OWNERDRAW)==0)
		nFlags |= MF_OWNERDRAW;

	int menustart=0;

	if((nFlags & MF_POPUP)!=0){
		if(m_loadmenu){
			menustart=GetMenuStart();
			if(nPosition<(UINT)menustart)menustart=0;
		}
		m_AllSubMenus.Add((HMENU)nID);
		m_SubMenus.Add((HMENU)nID);
	}

	//Stephane Clog suggested adding this, believe it or not it's in the help 
	if(nPosition==(UINT)-1)nPosition=GetMenuItemCount();
	
	BCMenuData *mdata = new BCMenuData;
	m_MenuList.InsertAt(nPosition-menustart,mdata);
	mdata->SetWideString(lpstrText);    //SK: modified for dynamic allocation
	
	if(nIconNormal>=0){
		mdata->global_offset = AddToGlobalImageList(nIconNormal, static_cast<int>(nID));
	}
	else mdata->global_offset = GlobalImageListOffset(static_cast<int>(nID));
	mdata->nFlags = nFlags;
	mdata->nID = nID;
	bool returnflag=!!CMenu::InsertMenu(nPosition,nFlags,nID,(LPCTSTR)mdata);
	if(m_loadmenu)RemoveTopLevelOwnerDraw();
	return returnflag;
}

bool BCMenu::ModifyODMenu(wchar_t *lpstrText,UINT_PTR nID,int nIconNormal)
{
	UINT nLoc;
	BCMenuData *mdata;
	CArray<BCMenu*,BCMenu*>bcsubs;
	CArray<UINT,UINT&>bclocs;
	
	// Find the old BCMenuData structure:
	BCMenu *psubmenu = FindMenuOption(static_cast<int>(nID),nLoc);
	do{
		if(psubmenu!=nullptr && nLoc!=-1)mdata = psubmenu->m_MenuList[nLoc];
		else{
			// Create a new BCMenuData structure:
			mdata = new BCMenuData;
			m_MenuList.Add(mdata);
		}
		
		ASSERT(mdata != nullptr);
		if(lpstrText != nullptr)
			mdata->SetWideString(lpstrText);  //SK: modified for dynamic allocation
		if(nIconNormal>=0){
			mdata->global_offset = AddToGlobalImageList(nIconNormal, static_cast<int>(nID));
		}
		else mdata->global_offset = GlobalImageListOffset(static_cast<int>(nID));
		mdata->nFlags &= ~(MF_BYPOSITION);
		mdata->nFlags |= MF_OWNERDRAW;
		mdata->nID = nID;
		bcsubs.Add(psubmenu);
		bclocs.Add(nLoc);
		if(psubmenu!=nullptr && nLoc!=-1)
			psubmenu = FindAnotherMenuOption(static_cast<int>(nID),nLoc,bcsubs,bclocs);
		else 
			psubmenu=nullptr;
	}while(psubmenu != nullptr);
	return !!CMenu::ModifyMenu(static_cast<UINT>(nID),mdata->nFlags, static_cast<UINT>(nID),(LPCTSTR)mdata);
}

BCMenuData *BCMenu::NewODMenu(UINT pos,UINT nFlags,UINT_PTR nID,CString string)
{
	BCMenuData *mdata;
	
	mdata = new BCMenuData;
	mdata->SetWideString((LPCTSTR)string);//SK: modified for dynamic allocation
	mdata->nFlags = nFlags;
	mdata->nID = nID;
	
//	if((nFlags & MF_POPUP)!=0)m_AllSubMenus.Add((HMENU)nID);
		
	if ((nFlags&MF_OWNERDRAW)!=0){
		ASSERT((nFlags&MF_STRING) == 0);
		ModifyMenu(pos,nFlags,nID,(LPCTSTR)mdata);
	}
	else 
	if ((nFlags&MF_STRING)!=0){
		ASSERT((nFlags&MF_OWNERDRAW) == 0);
		ModifyMenu(pos,nFlags,nID,mdata->GetString());
	}
	else{
		ASSERT((nFlags&MF_SEPARATOR) != 0);
		ModifyMenu(pos,nFlags,nID);
	}
	
	return mdata;
};

bool BCMenu::LoadToolbar(UINT nToolBar)
{
	bool returnflag=false;
	CToolBar bar;
	
	CWnd* pWnd = AfxGetMainWnd();
	if (pWnd == nullptr)pWnd = CWnd::GetDesktopWindow();
	bar.Create(pWnd);
	if(bar.LoadToolBar(nToolBar)){
		returnflag=true;
		for(int i=0;i<bar.GetCount();++i){
			UINT nID = bar.GetItemID(i); 
			if(nID!=0 && GetMenuState(nID, MF_BYCOMMAND)!=0xFFFFFFFF){
				int xoffset=bar.CommandToIndex(nID);
				if(xoffset>=0){
					UINT nStyle;
					int xset;
					bar.GetButtonInfo(xoffset,nID,nStyle,xset);
					if(xset>0)xoffset=xset;
				}
				ModifyODMenu(nullptr, nID, MAKELONG(nToolBar, 0x4000 + xoffset));
			}
		}
	}
	return returnflag;
}

// O.S.
BCMenuData *BCMenu::FindMenuItem(UINT_PTR nID)
{
	BCMenuData *pData = nullptr;
	int i;
	
	for(i = 0; i <= m_MenuList.GetUpperBound(); i++){
		if (m_MenuList[i]->nID == nID){
			pData = m_MenuList[i];
			break;
		}
	}
	if (pData == nullptr){
		UINT loc;
		BCMenu *pMenu = FindMenuOption(static_cast<int>(nID), loc);
		ASSERT (pMenu != this);
		if (loc != -1){
			return pMenu->FindMenuItem(nID);
		}
	}
	return pData;
}


BCMenu *BCMenu::FindAnotherMenuOption(int nId,UINT& nLoc,CArray<BCMenu*,BCMenu*>&bcsubs,
									  CArray<UINT,UINT&>&bclocs)
{
	BCMenu *psubmenu,*pgoodmenu;
	bool foundflag;
	int nummenu = GetMenuItemCount();
	
	for(int i=0;i<nummenu;++i){
#ifdef _CPPRTTI 
		psubmenu=dynamic_cast<BCMenu *>(GetSubMenu(i));
#else
		psubmenu=static_cast<BCMenu *>(GetSubMenu((int)i));
#endif
		if(psubmenu != nullptr){
			pgoodmenu=psubmenu->FindAnotherMenuOption(nId,nLoc,bcsubs,bclocs);
			if(pgoodmenu != nullptr)
				return pgoodmenu;
		}
		else if(nId==(int)GetMenuItemID(i)){
			INT_PTR numsubs=bcsubs.GetSize();
			foundflag=true;
			for(INT_PTR j=0;j<numsubs;++j){
				if(bcsubs[j]==this && bclocs[j]==static_cast<UINT>(i)){
					foundflag=false;
					break;
				}
			}
			if(foundflag){
				nLoc=static_cast<UINT>(i);
				return this;
			}
		}
	}
	nLoc = static_cast<UINT>(-1);
	return nullptr;
}

BCMenu *BCMenu::FindMenuOption(int nId,UINT& nLoc)
{
	BCMenu *psubmenu,*pgoodmenu;
	int nummenu = GetMenuItemCount();
	
	for(int i=0;i<nummenu;++i){
#ifdef _CPPRTTI 
		psubmenu=dynamic_cast<BCMenu *>(GetSubMenu(i));
#else
		psubmenu=static_cast<BCMenu *>(GetSubMenu(i));
#endif
		if(psubmenu != nullptr){
			pgoodmenu=psubmenu->FindMenuOption(nId,nLoc);
			if(pgoodmenu != nullptr)
				return pgoodmenu;
		}
		else if(nId==(int)GetMenuItemID(i)){
			nLoc=i;
			return this;
		}
	}
	nLoc = static_cast<UINT>(-1);
	return nullptr;
}

BCMenuData *BCMenu::FindMenuOption(wchar_t *lpstrText)
{
	BCMenu *psubmenu;
	BCMenuData *pmenulist;
	int nummenu = GetMenuItemCount();
	
	for(int i=0;i<nummenu;++i){
#ifdef _CPPRTTI 
		psubmenu=dynamic_cast<BCMenu *>(GetSubMenu(i));
#else
		psubmenu=static_cast<BCMenu *>(GetSubMenu(i));
#endif
		if(psubmenu != nullptr){
			pmenulist=psubmenu->FindMenuOption(lpstrText);
			if(pmenulist != nullptr)
				return pmenulist;
		}
		else{
			for(int j=0;j<=m_MenuList.GetUpperBound();++j){     
				const wchar_t *szWide;//SK: we use const to prevent misuse of this Ptr
				szWide = m_MenuList[j]->GetWideString ();
				if(szWide != nullptr && wcscmp(lpstrText,szWide)==0)//SK: modified for dynamic allocation
					return m_MenuList[j];
			}
		}
	}
	return nullptr;
}

BOOL BCMenu::LoadMenu(LPCTSTR lpszResourceName)
{
	ASSERT_VALID(this);
	ASSERT(lpszResourceName != nullptr);
	
	// Find the Menu Resource:
	HINSTANCE hInst = AfxFindResourceHandle(lpszResourceName,RT_MENU);
	HRSRC hRsrc = ::FindResource(hInst,lpszResourceName,RT_MENU);
	if (hRsrc == nullptr){
		hInst = nullptr;
		hRsrc = ::FindResource(hInst,lpszResourceName,RT_MENU);
	}
	if(hRsrc == nullptr)
		return FALSE;
	
	// Load the Menu Resource:
	
	HGLOBAL hGlobal = LoadResource(hInst, hRsrc);
	if(hGlobal == nullptr)
		return FALSE;

	// first destroy the menu if we're trying to loadmenu again
	DestroyMenu();

	// Attempt to create us as a menu...
	if(!CMenu::CreateMenu())
		return FALSE;
	
	// Get Item template Header, and calculate offset of MENUITEMTEMPLATES
	
	MENUITEMTEMPLATEHEADER *pTpHdr=
		(MENUITEMTEMPLATEHEADER*)LockResource(hGlobal);
	BYTE* pTp=(BYTE*)pTpHdr + 
		(sizeof(MENUITEMTEMPLATEHEADER) + pTpHdr->offset);
	
	
	// Variables needed during processing of Menu Item Templates:
	
	WORD    dwFlags = 0;              // Flags of the Menu Item
	WORD    dwID  = 0;              // ID of the Menu Item
	CTypedPtrArray<CPtrArray, BCMenu*>  stack;    // Popup menu stack
	CArray<bool,bool>  stackEnd;    // Popup menu stack
	stack.Add(this);                  // Add it to this...
	stackEnd.Add(false);
	
	do{
		// Obtain Flags and (if necessary), the ID...
		memcpy(&dwFlags, pTp, sizeof(WORD));pTp+=sizeof(WORD);// Obtain Flags
		if((dwFlags & MF_POPUP)==0){
			memcpy(&dwID, pTp, sizeof(WORD)); // Obtain ID
			pTp+=sizeof(WORD);
		}
		else dwID = 0;
		
		UINT uFlags = (UINT)dwFlags; // Remove MF_END from the flags that will
		if((uFlags & MF_END) != 0) // be passed to the Append(OD)Menu functions.
			uFlags -= MF_END;
		
		// Obtain Caption (and length)
		
		const size_t CaptionSiz = wcslen((wchar_t *)pTp)+1;
		wchar_t *szCaption=new wchar_t[CaptionSiz];
		wcscpy_s(szCaption, CaptionSiz, reinterpret_cast<wchar_t *>(pTp));
		pTp=&pTp[(wcslen((wchar_t *)pTp)+1)*sizeof(wchar_t)];//modified SK
		
		// Handle popup menus first....
		
		//WideCharToMultiByte
		if((dwFlags & MF_POPUP)!=0){
			if((dwFlags & MF_END)!=0)
				stackEnd.SetAt(stack.GetUpperBound(),true);
			BCMenu* pSubMenu = new BCMenu;
			pSubMenu->CreatePopupMenu();
			
			// Append it to the top of the stack:
			
			stack[stack.GetUpperBound()]->AppendODMenu(szCaption,uFlags,
				(UINT_PTR)pSubMenu->m_hMenu, -1);
			stack.Add(pSubMenu);
			stackEnd.Add(false);
		}
		else {
			stack[stack.GetUpperBound()]->AppendODMenu(szCaption, uFlags,
				dwID, -1);
			if((dwFlags & MF_END)!=0)
				stackEnd.SetAt(stack.GetUpperBound(),true);
			INT_PTR j = stack.GetUpperBound();
			while(j>=0 && stackEnd.GetAt(j)){
				stack.RemoveAt(j);
				stackEnd.RemoveAt(j);
				--j;
			}
		}
		
		delete[] szCaption;
	}while(stack.GetUpperBound() != -1);
	
	int nummenu = GetMenuItemCount();
	for(int i=0;i<nummenu;++i){
		CString str=m_MenuList[i]->GetString();
		if(GetSubMenu(i)){
			m_MenuList[i]->nFlags=MF_POPUP|MF_BYPOSITION;
			ModifyMenu(i,MF_POPUP|MF_BYPOSITION,
				(UINT_PTR)GetSubMenu(i)->m_hMenu,str);
		}
		else{
			m_MenuList[i]->nFlags=MF_STRING|MF_BYPOSITION;
			ModifyMenu(i,MF_STRING|MF_BYPOSITION,m_MenuList[i]->nID,str);
		}
	}

	m_loadmenu=true;
	
	return TRUE;
}

int BCMenu::GetMenuStart(void)
{
	if(!m_loadmenu)return 0;

	CString name,str;
	int menuloc=-1,listloc=-1,menustart=0,i=0;
	INT_PTR nummenulist=m_MenuList.GetSize();
	int nummenu=GetMenuItemCount();

	while(i<nummenu&&menuloc==-1){
		GetMenuString (i, name, MF_BYPOSITION);
		if(name.GetLength()>0){
			for(int j=0;j<nummenulist;++j){
				str=m_MenuList[j]->GetString();
				if(name==str){
					menuloc=i;
					listloc=j;
					break;
				}
			}
		}
		++i;
	}
	if(menuloc>=0&&listloc>=0&&menuloc>=listloc)menustart=menuloc-listloc;
	return menustart;
}

void BCMenu::RemoveTopLevelOwnerDraw(void)
{
	CString str;
	INT_PTR nummenulist=m_MenuList.GetSize();
	int nummenu = GetMenuItemCount();

	int menustart=GetMenuStart();
	for(int i=menustart,j=0;i<nummenu;++i,++j){
		if(j<nummenulist){
			str=m_MenuList[j]->GetString();
			if(GetSubMenu(i)){
				m_MenuList[j]->nFlags=MF_POPUP|MF_BYPOSITION;
				ModifyMenu(i,MF_POPUP|MF_BYPOSITION,
					(UINT_PTR)GetSubMenu(i)->m_hMenu,str);
			}
		}
	}

}

//--------------------------------------------------------------------------
//[18.06.99 rj]
bool BCMenu::GetMenuText(UINT id, CString& string, UINT nFlags/*= MF_BYPOSITION*/)
{
	bool returnflag=false;
	
	if((MF_BYPOSITION&nFlags) != 0){
		INT_PTR numMenuItems = m_MenuList.GetUpperBound();
		if(static_cast<INT_PTR>(id)<=numMenuItems){
			string=m_MenuList[id]->GetString();
			returnflag=true;
		}
	}
	else{
		UINT uiLoc;
		BCMenu* pMenu = FindMenuOption(id,uiLoc);
		if(pMenu != nullptr) 
			returnflag = pMenu->GetMenuText(uiLoc,string);
	}
	return returnflag;
}


void BCMenu::DrawRadioDot(CDC *pDC,int x,int y,COLORREF color)
{
	CRect rcDot(x,y,x+6,y+6);
	CBrush brush(color);
	CPen pen(PS_SOLID,0,color);
	CBrush *pOldBrush=pDC->SelectObject(&brush);
	CPen *pOldPen=pDC->SelectObject(&pen);
	pDC->Ellipse(&rcDot);
	pDC->SelectObject(pOldBrush);
	pDC->SelectObject(pOldPen);
}

void BCMenu::DrawCheckMark(CDC* pDC,int x,int y,COLORREF color,bool narrowflag /*= false*/)
{
	int dp=0;
	CPen penBack(PS_SOLID,0,color);
	CPen *pOldPen = pDC->SelectObject (&penBack);
	if(narrowflag)dp=1;

	pDC->MoveTo(x,y+2);
	pDC->LineTo(x,y+5-dp);
	
	pDC->MoveTo(x+1,y+3);
	pDC->LineTo(x+1,y+6-dp);
	
	pDC->MoveTo(x+2,y+4);
	pDC->LineTo(x+2,y+7-dp);
	
	pDC->MoveTo(x+3,y+3);
	pDC->LineTo(x+3,y+6-dp);
	
	pDC->MoveTo(x+4,y+2);
	pDC->LineTo(x+4,y+5-dp);
	
	pDC->MoveTo(x+5,y+1);
	pDC->LineTo(x+5,y+4-dp);
	
	pDC->MoveTo(x+6,y);
	pDC->LineTo(x+6,y+3-dp);
	
	pDC->SelectObject (pOldPen);
}

BCMenuData *BCMenu::FindMenuList(UINT_PTR nID)
{
	for(int i=0;i<=m_MenuList.GetUpperBound();++i){
		if(m_MenuList[i]->nID==nID && !m_MenuList[i]->syncflag){
			m_MenuList[i]->syncflag=1;
			return m_MenuList[i];
		}
	}
	return nullptr;
}

void BCMenu::InitializeMenuList(int value)
{
	for(int i=0;i<=m_MenuList.GetUpperBound();++i)
		m_MenuList[i]->syncflag=value;
}

void BCMenu::DeleteMenuList(void)
{
	for(int i=0;i<=m_MenuList.GetUpperBound();++i){
		if(m_MenuList[i]->syncflag==0){
			delete m_MenuList[i];
		}
	}
}

void BCMenu::SynchronizeMenu(void)
{
	CTypedPtrArray<CPtrArray, BCMenuData*> temp;
	CString string;
	UINT_PTR submenu,nID=0;
	
	InitializeMenuList(0);
	for(int j=0;j<GetMenuItemCount();++j){
		BCMenuData *mdata=nullptr;
		UINT state=GetMenuState(j,MF_BYPOSITION);
		if((state&MF_POPUP)!=0){
			submenu=(UINT_PTR)GetSubMenu(j)->m_hMenu;
			mdata=FindMenuList(submenu);
			GetMenuString(j,string,MF_BYPOSITION);
			if(mdata == nullptr)mdata=NewODMenu(j,
				(state&0xFF)|MF_BYPOSITION|MF_POPUP|MF_OWNERDRAW,submenu,string);
			else if(string.GetLength()>0)
				mdata->SetWideString(string);  //SK: modified for dynamic allocation
		}
		else 
		if((state&MF_SEPARATOR)!=0){
			mdata=FindMenuList(0);
			if(mdata == nullptr)mdata=NewODMenu(j,
				state|MF_BYPOSITION|MF_SEPARATOR|MF_OWNERDRAW,0,_T(""));//SK: modified for Unicode correctness
			else ModifyMenu(j,mdata->nFlags,nID,(LPCTSTR)mdata);
		}
		else{
			nID=GetMenuItemID(j);
			mdata=FindMenuList(nID);
			GetMenuString(j,string,MF_BYPOSITION);
			if(mdata == nullptr)mdata=NewODMenu(j,state|MF_BYPOSITION|MF_OWNERDRAW,
				nID,string);
			else{
				mdata->nFlags=state|MF_BYPOSITION|MF_OWNERDRAW;
				if(string.GetLength()>0)
					mdata->SetWideString(string);//SK: modified for dynamic allocation
				
				ModifyMenu(j,mdata->nFlags,nID,(LPCTSTR)mdata);
			}
		}
		if(mdata != nullptr)temp.Add(mdata);
	}
	DeleteMenuList();
	m_MenuList.RemoveAll();
	m_MenuList.Append(temp);
	temp.RemoveAll(); 
}

void BCMenu::UpdateMenu(CMenu *pmenu)
{
#ifdef _CPPRTTI 
	BCMenu *psubmenu = dynamic_cast<BCMenu *>(pmenu);
#else
	BCMenu *psubmenu = static_cast<BCMenu *>(pmenu);
#endif
	if(psubmenu != nullptr)psubmenu->SynchronizeMenu();
}

LRESULT BCMenu::FindKeyboardShortcut(UINT nChar, UINT nFlags,
                                     CMenu *pMenu)
{
#ifdef _CPPRTTI 
	BCMenu *pBCMenu = dynamic_cast<BCMenu *>(pMenu);
#else
	BCMenu *pBCMenu = static_cast<BCMenu *>(pMenu);
#endif
	if(pBCMenu!=nullptr && (nFlags&MF_POPUP)!=0){
		CString key(_T('&'),2);//SK: modified for Unicode correctness
		key.SetAt(1,(TCHAR)nChar);
		key.MakeLower();
		CString menutext;
		int menusize = pBCMenu->GetMenuItemCount();
		if(menusize!=(pBCMenu->m_MenuList.GetUpperBound()+1))
			pBCMenu->SynchronizeMenu();
		for(int i=0;i<menusize;++i){
			if(pBCMenu->GetMenuText(i,menutext)){
				menutext.MakeLower();
				if(menutext.Find(key)>=0)return MAKELRESULT(i,2);
			}
		}
	}
	return 0;
}

void BCMenu::GetTransparentBitmap(CBitmap &bmp)
{
	CDC ddc;
	COLORREF col,newcol;
	BITMAP BitMap;

	bmp.GetBitmap(&BitMap);
	ddc.CreateCompatibleDC(nullptr);
	CBitmap * pddcOldBmp = ddc.SelectObject(&bmp);

	// use this to get the background color, takes into account color shifting
	CDC ddc2;
	CBitmap bmp2;
	ddc2.CreateCompatibleDC(nullptr);
	bmp2.CreateCompatibleBitmap(&ddc,BitMap.bmWidth,BitMap.bmHeight);
	col=RGB(255,0,255); // Original was RGB(192,192,192)
	CBitmap * pddcOldBmp2 = ddc2.SelectObject(&bmp2);
	CRect rect(0,0,BitMap.bmWidth,BitMap.bmHeight);
	CBrush cbTemp = col;
	ddc2.FillRect(rect, &cbTemp);
	ddc2.SelectObject(pddcOldBmp2);
	newcol=GetSysColor(COLOR_3DFACE);

	pBGR pdstBGR = MyGetDibBits(ddc2.m_hDC,(HBITMAP)bmp.m_hObject,BitMap.bmWidth,BitMap.bmHeight);
	sBGR bgcolBGR = *pdstBGR;
	sBGR newcolBGR = {GetBValue(newcol),GetGValue(newcol), GetRValue(newcol),0};
	pBGR pcurBGR = pdstBGR;

	for(int i=0;i<BitMap.bmWidth;++i){
		for(int j=0;j<BitMap.bmHeight;++j){
			if(*(DWORD *)pcurBGR == *(DWORD *)&bgcolBGR)
				*pcurBGR = newcolBGR;
			pcurBGR++;
		}
	}

	MySetDibBits(ddc2.m_hDC, (HBITMAP)bmp.m_hObject,pdstBGR,BitMap.bmWidth,BitMap.bmHeight);
	free(pdstBGR);

	ddc.SelectObject(pddcOldBmp);
}

void BCMenu::GetDisabledBitmap(CBitmap &bmp,COLORREF background)
{
	CDC ddc;
	COLORREF discol;
	BITMAP BitMap;

	bmp.GetBitmap(&BitMap);
	ddc.CreateCompatibleDC(nullptr);
	CBitmap * pddcOldBmp = ddc.SelectObject(&bmp);

	// use this to get the background color, takes into account color shifting
	CDC ddc2;
	CBitmap bmp2;
	ddc2.CreateCompatibleDC(nullptr);
	bmp2.CreateCompatibleBitmap(&ddc,BitMap.bmWidth,BitMap.bmHeight);
	CBitmap * pddcOldBmp2 = ddc2.SelectObject(&bmp2);
	CRect rect(0,0,BitMap.bmWidth,BitMap.bmHeight);
	CBrush cbTemp = GetSysColor(COLOR_3DFACE);
	ddc2.FillRect(rect, &cbTemp);
	ddc2.SelectObject(pddcOldBmp2);
	discol=GetSysColor(COLOR_BTNSHADOW);

	pBGR pdstBGR = MyGetDibBits(ddc2.m_hDC,(HBITMAP)bmp.m_hObject,BitMap.bmWidth,BitMap.bmHeight);
	sBGR bgcolBGR = *pdstBGR;
	sBGR backgroundBGR = {GetBValue(background),GetGValue(background),GetRValue(background),0};
	pBGR pcurBGR = pdstBGR;

	for(int i=0;i<BitMap.bmWidth;++i){
		for(int j=0;j<BitMap.bmHeight;++j){
			if(*(DWORD *)pcurBGR != *(DWORD *)&bgcolBGR){
				int avgcol = ((DWORD)pcurBGR->r+(DWORD)pcurBGR->g+(DWORD)pcurBGR->b)/3;
				double factor = avgcol/255.0;
				COLORREF newcol = LightenColor(discol,factor);
				sBGR newcolBGR = {GetBValue(newcol),GetGValue(newcol),GetRValue(newcol),0};
				*pcurBGR = newcolBGR;
			}
			else{
				if(background)
					*pcurBGR = backgroundBGR;
			}
			pcurBGR++;
		}
	}

	MySetDibBits(ddc2.m_hDC,(HBITMAP)bmp.m_hObject,pdstBGR,BitMap.bmWidth,BitMap.bmHeight);
	free(pdstBGR);

	ddc.SelectObject(pddcOldBmp);
}

void BCMenu::GetDisabledBitmap(CImage &bmp)
{
	COLORREF discol=GetSysColor(COLOR_BTNSHADOW);
	pBGR pcurBGR = static_cast<pBGR>(bmp.GetBits());

	for(int i=0;i<bmp.GetWidth();++i){
		for(int j=0;j<bmp.GetHeight();++j){
			int avgcol = ((DWORD)pcurBGR->r+(DWORD)pcurBGR->g+(DWORD)pcurBGR->b)/3;
			double factor = avgcol/255.0;
			COLORREF newcol = LightenColor(discol,factor);
			sBGR newcolBGR = {GetBValue(newcol),GetGValue(newcol),GetRValue(newcol),pcurBGR->pad};
			*pcurBGR = newcolBGR;
			pcurBGR++;
		}
	}
}

bool BCMenu::AddBitmapToImageList(CImageList *bmplist,UINT nResourceID)
{
	bool bReturn=false;

	HBITMAP hbmp=LoadSysColorBitmap(nResourceID);
	if(hbmp!=nullptr){
		CBitmap bmp;
		bmp.Attach(hbmp);
		if(bmplist->Add(&bmp,GetBitmapBackground())>=0)bReturn=true;
	}
	else{ // a hicolor bitmap
		CBitmap mybmp;
		if(mybmp.LoadBitmap(nResourceID)){
			hicolor_bitmaps=true;
			GetTransparentBitmap(mybmp);
			if(bmplist->Add(&mybmp,GetBitmapBackground())>=0)bReturn=true;
		}
	}
	return bReturn;
}

bool BCMenu::ReplaceBitmapInImageList(CImageList* bmplist, int xoffset, UINT nResourceID)
{
	bool result = AddBitmapToImageList(bmplist, nResourceID);
	const int cnt = bmplist->GetImageCount();
	if (xoffset < cnt - 1)
	{
		bmplist->Copy(xoffset, cnt - 1);
		bmplist->Remove(cnt - 1);
	}
	return result;
}

bool BCMenu::Draw3DCheckmark(CDC *dc, const CRect& rc,
                             bool bSelected, HBITMAP hbmCheck)
{
	CRect rcDest = rc;
	COLORREF col=GetSysColor(COLOR_MENU);
	if(!bSelected)col = LightenColor(col,0.6);
	CBrush cbTemp = col;
	dc->FillRect(rcDest, &cbTemp);
	dc->DrawEdge(&rcDest, BDR_SUNKENOUTER, BF_RECT);
	if (hbmCheck == nullptr)
		DrawCheckMark(dc,rc.left+4,rc.top+4,GetSysColor(COLOR_MENUTEXT));
	else 
		DrawRadioDot(dc,rc.left+5,rc.top+4,GetSysColor(COLOR_MENUTEXT));
	return true;
}

void BCMenu::DitherBlt2(CDC *drawdc, int nXDest, int nYDest, int nWidth, 
                        int nHeight, CBitmap &bmp, int nXSrc, int nYSrc,
						COLORREF bgcolor)
{
	// create a monochrome memory DC
	CDC ddc;
	ddc.CreateCompatibleDC(nullptr);
	CBitmap bwbmp;
	bwbmp.CreateCompatibleBitmap(&ddc, nWidth, nHeight);
	CBitmap * pddcOldBmp = ddc.SelectObject(&bwbmp);
	
	CDC dc;
	dc.CreateCompatibleDC(nullptr);
	CBitmap * pdcOldBmp = dc.SelectObject(&bmp);
	
	// build a mask
	ddc.PatBlt(0, 0, nWidth, nHeight, WHITENESS);
	dc.SetBkColor(GetSysColor(COLOR_BTNFACE));
	ddc.BitBlt(0, 0, nWidth, nHeight, &dc, nXSrc,nYSrc, SRCCOPY);
	dc.SetBkColor(GetSysColor(COLOR_BTNHILIGHT));
	ddc.BitBlt(0, 0, nWidth, nHeight, &dc, nXSrc,nYSrc, SRCPAINT);
	
	// Copy the image from the toolbar into the memory DC
	// and draw it (grayed) back into the toolbar.
	dc.FillSolidRect(0,0, nWidth, nHeight, bgcolor);
	//SK: Looks better on the old shell
	dc.SetBkColor(RGB(0, 0, 0));
	dc.SetTextColor(RGB(255, 255, 255));
	CBrush brHilight(GetSysColor(COLOR_BTNHILIGHT));
	CBrush brShadow(GetSysColor(COLOR_BTNSHADOW));
	CBrush * pOldBrush = dc.SelectObject(&brHilight);
	dc.BitBlt(0,0, nWidth, nHeight, &ddc, 0, 0, 0x00E20746L);
	drawdc->BitBlt(nXDest+1,nYDest+1,nWidth, nHeight, &dc,0,0,SRCCOPY);
	dc.BitBlt(1,1, nWidth, nHeight, &ddc, 0, 0, 0x00E20746L);
	dc.SelectObject(&brShadow);
	dc.BitBlt(0,0, nWidth, nHeight, &ddc, 0, 0, 0x00E20746L);
	drawdc->BitBlt(nXDest,nYDest,nWidth, nHeight, &dc,0,0,SRCCOPY);
	// reset DCs
	ddc.SelectObject(pddcOldBmp);
	dc.SelectObject(pOldBrush);
	dc.SelectObject(pdcOldBmp);
}

void BCMenu::DitherBlt3(CDC *drawdc, int nXDest, int nYDest, int nWidth, 
                        int nHeight, CBitmap &bmp,COLORREF bgcolor)
{
	GetDisabledBitmap(bmp,bgcolor);
	CDC dc;
	dc.CreateCompatibleDC(nullptr);
	CBitmap * pdcOldBmp = dc.SelectObject(&bmp);
	drawdc->BitBlt(nXDest,nYDest,nWidth, nHeight, &dc,0,0,SRCCOPY);
	// reset DCs
	dc.SelectObject(pdcOldBmp);
}

WORD BCMenu::NumBitmapColors(LPBITMAPINFOHEADER lpBitmap)
{
	WORD returnval = 0;

	if ( lpBitmap->biClrUsed != 0){
		returnval=(WORD)lpBitmap->biClrUsed;
	}
	else{
		switch (lpBitmap->biBitCount){
			case 1:
				returnval=2;
				break;
			case 4:
				returnval=16;
				break;
			case 8:
				returnval=256;
				break;
			default:
				returnval=0;
				break;
		}
	}
	return returnval;
}

HBITMAP BCMenu::LoadSysColorBitmap(int nResourceId)
{
	HINSTANCE hInst = 
		AfxFindResourceHandle(MAKEINTRESOURCE(nResourceId),RT_BITMAP);
	HRSRC hRsrc = 
		::FindResource(hInst,MAKEINTRESOURCE(nResourceId),RT_BITMAP);
	if (hRsrc == nullptr){
		hInst = nullptr;
		hRsrc = ::FindResource(hInst,MAKEINTRESOURCE(nResourceId),RT_BITMAP);
	}
	if (hRsrc == nullptr)
		return nullptr;

	// determine how many colors in the bitmap
	HGLOBAL hglb;
	if ((hglb = LoadResource(hInst, hRsrc)) == nullptr)
		return nullptr;
	LPBITMAPINFOHEADER lpBitmap = (LPBITMAPINFOHEADER)LockResource(hglb);
	if (lpBitmap == nullptr)
		return nullptr;
	WORD numcol = NumBitmapColors(lpBitmap);
	::FreeResource(hglb);

	if(numcol!=16)
		return nullptr;

	return ::AfxLoadSysColorBitmap(hInst, hRsrc, FALSE);
}

bool BCMenu::RemoveMenu(UINT uiId,UINT nFlags)
{
	if(MF_BYPOSITION&nFlags){
		UINT uint = GetMenuState(uiId,MF_BYPOSITION);
		if((uint&MF_SEPARATOR)!=0 && (uint&MF_POPUP)==0){
			delete m_MenuList.GetAt(uiId);
			m_MenuList.RemoveAt(uiId);
		}
		else{
			BCMenu* pSubMenu = static_cast<BCMenu*>(GetSubMenu(uiId));
			if(pSubMenu == nullptr){
				UINT uiCommandId = GetMenuItemID(uiId);
				for(int i=0;i<m_MenuList.GetSize(); i++){
					if(m_MenuList[i]->nID==uiCommandId){
						delete m_MenuList.GetAt(i);
						m_MenuList.RemoveAt(i);
						break;
					}
				}
			}
			else{
				INT_PTR numSubMenus = m_SubMenus.GetUpperBound();
				for(INT_PTR m = numSubMenus; m >= 0; m--){
					if(m_SubMenus[m]==pSubMenu->m_hMenu){
						INT_PTR numAllSubMenus = m_AllSubMenus.GetUpperBound();
						for(INT_PTR n = numAllSubMenus; n>= 0; n--){
							if(m_AllSubMenus[n]==m_SubMenus[m])m_AllSubMenus.RemoveAt(n);
						}
						m_SubMenus.RemoveAt(m);
					}
				}
				int num = pSubMenu->GetMenuItemCount();
				int i=0;
				for(i=num-1;i>=0;--i)pSubMenu->RemoveMenu(i,MF_BYPOSITION);
				for(i=(int)m_MenuList.GetUpperBound();i>=0;i--){
					if(m_MenuList[i]->nID==(UINT_PTR)pSubMenu->m_hMenu){
						delete m_MenuList.GetAt(i);
						m_MenuList.RemoveAt(i);
						break;
					}
				}
				delete pSubMenu; 
			}
		}
	}
	else{
		UINT iPosition =0;
		BCMenu* pMenu = FindMenuOption(uiId,iPosition);
		// bug fix RIA 14th September 2000 
		// failed to return correct value on call to remove menu as the item was 
		// removed twice. The second time its not found 
		// so a value of 0 was being returned 
		if(pMenu != nullptr) 
			return pMenu->RemoveMenu(iPosition,MF_BYPOSITION); // added return 
	}
	return !!CMenu::RemoveMenu(uiId,nFlags);
}

bool BCMenu::DeleteMenu(UINT uiId,UINT nFlags)
{
	if((MF_BYPOSITION&nFlags)!=0){
		UINT uint = GetMenuState(uiId,MF_BYPOSITION);
		if((uint&MF_SEPARATOR)!=0 && (uint&MF_POPUP)==0){
			// make sure it's a separator
			INT_PTR menulistsize=m_MenuList.GetSize();	
			if(uiId<(UINT)menulistsize){
				CString str=m_MenuList[uiId]->GetString();
				if(str==""){
					delete m_MenuList.GetAt(uiId);
					m_MenuList.RemoveAt(uiId);
				}
			}
		}
		else{
			BCMenu* pSubMenu = static_cast<BCMenu*>(GetSubMenu(uiId));
			if(pSubMenu == nullptr){
				UINT uiCommandId = GetMenuItemID(uiId);
				for(int i=0;i<m_MenuList.GetSize(); i++){
					if(m_MenuList[i]->nID==uiCommandId){
						delete m_MenuList.GetAt(i);
						m_MenuList.RemoveAt(i);
						break;
					}
				}
			}
			else{
				INT_PTR numSubMenus = m_SubMenus.GetUpperBound();
				for(INT_PTR m = numSubMenus; m >= 0; m--){
					if(m_SubMenus[m]==pSubMenu->m_hMenu){
						INT_PTR numAllSubMenus = m_AllSubMenus.GetUpperBound();
						for(INT_PTR n = numAllSubMenus; n>= 0; n--){
							if(m_AllSubMenus[n]==m_SubMenus[m])m_AllSubMenus.RemoveAt(n);
						}
						m_SubMenus.RemoveAt(m);
					}
				}
				int num = pSubMenu->GetMenuItemCount();
				for(int i=num-1;i>=0;--i)pSubMenu->DeleteMenu(i,MF_BYPOSITION);
				for(INT_PTR i=m_MenuList.GetUpperBound();i>=0;i--){
					if(m_MenuList[i]->nID==(UINT_PTR)pSubMenu->m_hMenu){
						delete m_MenuList.GetAt(i);
						m_MenuList.RemoveAt(i);
						break;
					}
				}
				delete pSubMenu;
			}
		}
	}
	else{
		UINT iPosition =0;
		BCMenu* pMenu = FindMenuOption(uiId,iPosition);
		if(pMenu != nullptr)
			return pMenu->DeleteMenu(iPosition,MF_BYPOSITION);
	}

	return !!CMenu::DeleteMenu(uiId,nFlags);
}

// Larry Antram
bool BCMenu::SetMenuText(UINT id, CString string, UINT nFlags/*= MF_BYPOSITION*/ )
{
	bool returnflag=false;
	
	if((MF_BYPOSITION&nFlags) != 0)
	{
		INT_PTR numMenuItems = m_MenuList.GetUpperBound();
		if(static_cast<INT_PTR>(id)<=numMenuItems){
			m_MenuList[id]->SetWideString((LPCTSTR)string);
			returnflag=true;
		}
	}
	else{
		UINT uiLoc;
		BCMenu* pMenu = FindMenuOption(id,uiLoc);
		if(pMenu != nullptr) 
			returnflag = pMenu->SetMenuText(uiLoc,string);
	}
	return returnflag;
}

int BCMenu::GlobalImageListOffset(int nID)
{
	INT_PTR numcurrent=m_AllImagesID.size();
	int existsloc = -1;
	for(INT_PTR i=0;i<numcurrent;++i){
		if(m_AllImagesID[i].id==nID){
			existsloc=static_cast<int>(i);
			break;
		}
	}
	return existsloc;
}

void BCMenu::LoadImages()
{
	if (!m_bHasNotLoadedImages)
		return;
	HIMAGELIST hImageList = m_AllImages.m_hImageList;
	if(hImageList == nullptr)
		m_AllImages.Create(m_iconX,m_iconY,ILC_COLORDDB|ILC_MASK,1,1);
	std::map<int, std::unique_ptr<CImageList>> mapImageList;
	for (size_t i = 0; i < m_AllImagesID.size(); ++i)
	{
		const int resourceId = m_AllImagesID[i].resourceId;
		if (resourceId != -1)
		{
			if (m_AllImagesID[i].bitmapIndex >= 0)
			{
				if (mapImageList.find(resourceId) == mapImageList.end())
				{
					mapImageList.emplace(resourceId, new CImageList);
					mapImageList[resourceId]->Create(m_iconX, m_iconY, ILC_COLORDDB | ILC_MASK, 1, 1);
					AddBitmapToImageList(mapImageList[resourceId].get(), resourceId);
				}
				HICON hIcon = mapImageList[resourceId]->ExtractIcon(m_AllImagesID[i].bitmapIndex);
				if (static_cast<int>(i) < m_AllImages.GetImageCount())
					m_AllImages.Replace(static_cast<int>(i), hIcon);
				else
					m_AllImages.Add(hIcon);
				DestroyIcon(hIcon);
			}
			else
			{
				ReplaceBitmapInImageList(&m_AllImages, static_cast<int>(i), resourceId);
			}
			m_AllImagesID[i].resourceId = -1;
			m_AllImagesID[i].bitmapIndex = -1;
		}
	}
	m_bHasNotLoadedImages = false;
}

INT_PTR BCMenu::AddToGlobalImageList(int nIconNormal,int nID)
{
	INT_PTR loc = -1;
	INT_PTR numcurrent=m_AllImagesID.size();
	int existsloc = GlobalImageListOffset(nID);
	if(existsloc>=0){
		m_AllImagesID[existsloc].resourceId = (nIconNormal & 0x40000000) ? (nIconNormal & 0xffff) : nIconNormal;
		m_AllImagesID[existsloc].bitmapIndex = (nIconNormal & 0x40000000) ? ((nIconNormal & 0x3fff0000) >> 16) : -1;
		loc = existsloc;
	}
	else{
		m_AllImagesID.push_back({ nID, 
			(nIconNormal & 0x40000000) ? (nIconNormal & 0xffff) : nIconNormal, 
			(nIconNormal & 0x40000000) ? ((nIconNormal & 0x3fff0000) >> 16) : -1 });
		loc=numcurrent;
	}
	m_bHasNotLoadedImages = true;
	return loc;
}
