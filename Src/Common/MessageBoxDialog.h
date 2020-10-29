/*
 *	Extended MFC message boxes -- Version 1.1a
 *	Copyright (c) 2004 Michael P. Mehl. All rights reserved.
 *
 *	The contents of this file are subject to the Mozilla Public License
 *	Version 1.1a (the "License"); you may not use this file except in
 *	compliance with the License. You may obtain a copy of the License at 
 *	http://www.mozilla.org/MPL/.
 *
 *	Software distributed under the License is distributed on an "AS IS" basis,
 *	WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 *	for the specific language governing rights and limitations under the
 *	License. 
 *
 *	The Original Code is Copyright (c) 2004 Michael P. Mehl. All rights
 *	reserved. The Initial Developer of the Original Code is Michael P. Mehl
 *	<michael.mehl@web.de>.
 *
 *	Alternatively, the contents of this file may be used under the terms of
 *	the GNU Lesser General Public License Version 2.1 (the "LGPL License"),
 *	in which case the provisions of LGPL License are applicable instead of
 *	those above. If you wish to allow use of your version of this file only
 *	under the terms of the LGPL License and not to allow others to use your
 *	version of this file under the MPL, indicate your decision by deleting
 *	the provisions above and replace them with the notice and other provisions
 *	required by the LGPL License. If you do not delete the provisions above,
 *	a recipient may use your version of this file under either the MPL or
 *	the LGPL License.
 */

#pragma once

#include "resource.h"
#include "UnicodeString.h"
#include "utils/DpiAware.h"
#include <vector>

//////////////////////////////////////////////////////////////////////////////
// Message box style definitions (mostly taken from WinUser.h).

#ifndef MB_CANCELTRYCONTINUE
#define MB_CANCELTRYCONTINUE		0x00000006L	// Standard for Win 5.x.
#endif

#define MB_CONTINUEABORT			0x00000007L	// Additional style.
#define MB_SKIPSKIPALLCANCEL		0x00000008L	// Additional style.
#define MB_IGNOREIGNOREALLCANCEL	0x00000009L	// Additional style.

#define MB_DONT_DISPLAY_AGAIN		0x01000000L	// Additional style.
#define MB_DONT_ASK_AGAIN			0x02000000L	// Additional style.
#define MB_YES_TO_ALL				0x04000000L	// Additional style.
#define MB_NO_TO_ALL				0x08000000L	// Additional style.

#define MB_DEFAULT_CHECKED			0x10000000L // Additional style.
#define MB_RIGHT_ALIGN				0x20000000L	// Additional style.
#define MB_NO_SOUND					0x40000000L	// Additional style.

#define MB_MODELESS					0x80000000L	// Additional style.

#define MB_DEFBUTTON5				0x00000400L	// Additional style.
#define MB_DEFBUTTON6				0x00000500L	// Additional style.

//////////////////////////////////////////////////////////////////////////////
// Dialog element IDs.

#ifndef IDTRYAGAIN
#define IDTRYAGAIN					10			// Standard for Win 5.x.
#endif

#ifndef IDCONTINUE
#define IDCONTINUE					11			// Standard for Win 5.x.
#endif

#define IDYESTOALL					14			// Additional element.
#define IDNOTOALL					15			// Additional element.
#define IDSKIP						16			// Additional element.
#define IDSKIPALL					17			// Additional element.
#define IDIGNOREALL					18			// Additional element.
#define IDCHECKBOX					19			// Additional element.

//////////////////////////////////////////////////////////////////////////////
// Name of the registry section for storing the message box results.

#define REGISTRY_SECTION_MESSAGEBOX	_T("MessageBoxes")

//////////////////////////////////////////////////////////////////////////////
// Class definition.

class CMessageBoxDialog : public DpiAware::CDpiAwareWnd<CDialog>
{

	DECLARE_DYNAMIC(CMessageBoxDialog)

public:

	//////////////////////////////////////////////////////////////////////////
	// Constructors and destructors of the class.

	// Constructor of the class for direct providing of the message strings.
	CMessageBoxDialog ( CWnd* pParent, CString strMessage, 
		CString strTitle = _T(""), UINT nStyle = MB_OK, UINT nHelp = 0 );

	// Constructor of the class for loading the strings from the resources.
	CMessageBoxDialog ( CWnd* pParent, UINT nMessageID, UINT nTitleID = 0,
		UINT nStyle = MB_OK, UINT nHelp = 0 );

	// Default destructor of the class.
	virtual ~CMessageBoxDialog ( );

	enum { IDD = IDD_MESSAGE_BOX };

public:

	//////////////////////////////////////////////////////////////////////////
	// Methods for setting and retrieving dialog options.

	// Method for setting the style of the message box.
	void SetStyle ( UINT nStyle );

	// Method for retrieving the style of the message box.
	UINT GetStyle ( );

	// Methods for setting the message to be displayed in the message box.
	void SetMessage ( LPCTSTR strMessage );
	void SetMessage ( UINT nMessageID );

	// Method for retrieving the message to be displayed in the message box.
	const String &GetMessage ( );

	// Methods for setting the title to be displayed in the message box.
	void SetTitle ( LPCTSTR strTitle );
	void SetTitle ( UINT nTitleID );

	// Method for retrieving the title to be displayed in the message box.
	const String &GetTitle ( );

	// Methods for setting the icon to be displayed in the message box.
	void SetMessageIcon ( HICON hIcon );
	void SetMessageIcon ( UINT nIconID );

	// Method for retrieving the icon to be displayed in the message box.
	HICON GetMessageIcon ( );

	// Method for setting a timeout.
	void SetTimeout ( UINT nSeconds, bool bDisabled = false );

	// Method for retrieving the seconds for the timeout.
	UINT GetTimeoutSeconds ( );

	// Method for retrieving whether a timeout is disabled.
	bool GetTimeoutDisabled ( );

	// Method for retrieving the former result of the message box from the registry.
	int GetFormerResult();
	// Method for storing the former result of the message box to the registry.
	int SetFormerResult(int nResult);

public:

	//////////////////////////////////////////////////////////////////////////
	// Methods for handling the stored states.

	// Method for resetting the message boxes stored in the registry.
	static void ResetMessageBoxes ( );

public:

	//////////////////////////////////////////////////////////////////////////
	// Methods for handling common window functions.

	// Method for displaying the dialog.
	virtual INT_PTR DoModal ( );

	// Method for closing the dialog.
	void EndDialog ( int nResult );

	// Method for initializing the dialog.
	virtual BOOL OnInitDialog ( );

	// Method for handling command messages.
	virtual BOOL OnCmdMsg ( UINT nID, int nCode, void* pExtra,
		AFX_CMDHANDLERINFO* pHandlerInfo );

	// Method for handling messages before dispatching them.
	virtual BOOL PreTranslateMessage ( MSG* pMsg );

	// Method for handling a timer event.
	afx_msg void OnTimer ( UINT_PTR nIDEvent );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg HBRUSH OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor );
	afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);

protected:

	//////////////////////////////////////////////////////////////////////////
	// Other methods for handling common window functions.

	// Method for handling window messages.
	virtual BOOL OnWndMsg ( UINT message, WPARAM wParam, LPARAM lParam,
		LRESULT* pResult );

	DECLARE_MESSAGE_MAP()

private:

	//////////////////////////////////////////////////////////////////////////
	// Private member variables of this dialog.

	String		m_strMessage;		// Message to be displayed.
	String		m_strTitle;			// Title to be used.
	UINT		m_nStyle;			// Style of the message box.
	UINT		m_nHelp;			// Help context of the message box.

	HICON		m_hIcon;			// Icon to be displayed in the dialog.

	UINT		m_nTimeoutSeconds;	// Seconds for a timeout.
	bool		m_bTimeoutDisabled;	// Flag whether the timeout is disabled.
	UINT_PTR	m_nTimeoutTimer;	// Timer for the timeout.

	CString		m_strRegistryKey;	// Entry for storing the result in the
									// registry, if the MB_DONT_DISPLAY_AGAIN
									// or MB_DONT_ASK_AGAIN flag is given.
	CFont       m_font;
	CFont       m_fontMainInstruction;
	COLORREF    m_clrMainInstructionFont;

private:

	//////////////////////////////////////////////////////////////////////////
	// Control handling types and variables.

	typedef struct tagMSGBOXBTN
	{
		UINT	nID;				// ID of a dialog button.
		UINT	nTitle;				// ID of the title string resource.
	} MSGBOXBTN;

    std::vector<MSGBOXBTN> m_aButtons;
									// List of all buttons in the dialog.

	int			m_nDefaultButton;	// ID of the default button.
	int			m_nEscapeButton;	// ID of the escape button.

	CStatic		m_stcIcon;			// Static control for the icon.
	CStatic		m_stcMessage;		// Static control for the message.
	CToolTipCtrl	m_tooltips;

private:

	//////////////////////////////////////////////////////////////////////////
	// Size handling variables.

	CSize		m_sDialogUnit;		// Variable for the size of a dialog unit.
	CSize		m_sIcon;			// Variable for the size of the icon.
	CSize		m_sMessage;			// Variable for the size of the message.
	CSize		m_sCheckbox;		// Variable for the size of the checkbox.
	CSize		m_sButton;			// Variable for the size of a button.

private:

	//////////////////////////////////////////////////////////////////////////
	// Helper methods.
	
	// Method for generating the registry key.
	CString	GenerateRegistryKey ( );

	// Method for adding a button to the list of buttons.
	void AddButton ( UINT nID, UINT nTitle, bool bIsDefault = false,
		bool bIsEscape = false );

	// Methods for converting a dialog units to a pixel values.
	int XDialogUnitToPixel ( int x );
	int YDialogUnitToPixel ( int y );

	// Method for parsing the given style.
	void ParseStyle ( );

	// Method for creating the icon control.
	void CreateIconControl ( );

	// Method for creating the message control.
	void CreateMessageControl ( );

	// Method for creating the checkbox control.
	void CreateCheckboxControl ( );

	// Method for creating the button controls.
	void CreateButtonControls ( );

	// Method for defining the layout of the dialog.
	void DefineLayout ( );

	void UpdateResources (int dpi);

};
