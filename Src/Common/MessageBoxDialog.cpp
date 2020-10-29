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

/*
 *	The flag MB_DONT_DISPLAY_AGAIN or MB_DONT_ASK_AGAIN is stored in the registry
 *  See GenerateRegistryKey for the creation of the key
 *  The "normal" rule is to use the help Id as identifier
 *  And it is really simple to just repeat the text ID as help ID
 *  (for message formed with AfxFormatString, repeat the ID used to format the string)
 *
 *  Search for MB_DONT_DISPLAY_AGAIN and MB_DONT_ASK_AGAIN for all the
 *  concerned AfxMessageBox
 */

#include "StdAfx.h"

#include "MessageBoxDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using std::vector;

IMPLEMENT_DYNAMIC(CMessageBoxDialog, CDialog)

//////////////////////////////////////////////////////////////////////////////
// Layout values (in dialog units).

#define CX_BORDER					8		// Width of the border.
#define CY_BORDER					8		// Height of the border.

#define CX_CHECKBOX_ADDON			14		// Additional width of the checkbox.

#define CX_BUTTON					51		// Standard width of a button.
#define CY_BUTTON					15		// Standard height of a button.
#define CX_BUTTON_BORDER			4		// Standard border for a button.
#define CY_BUTTON_BORDER			1		// Standard border for a button.
#define CX_BUTTON_SPACE				4		// Standard space for a button.

#define CX_DLGUNIT_BASE				1000	// Values used for converting
#define CY_DLGUNIT_BASE				1000	// dialog units to pixels.

//////////////////////////////////////////////////////////////////////////////
// Timer values.

#define MESSAGE_BOX_TIMER			2201	// Event identifier for the timer.

//////////////////////////////////////////////////////////////////////////////
// Constructors and destructors of the class.

/*
 *	Constructor of the class.
 *
 *	This constructor is used to provide the strings directly without providing
 *	resource IDs from which these strings should be retrieved. If no title is
 *	given, the application name will be used as the title of the dialog.
 */
 CMessageBoxDialog::CMessageBoxDialog ( CWnd* pParent, CString strMessage, 
	CString strTitle, UINT nStyle, UINT nHelp ) 
	: DpiAware::CDpiAwareWnd<CDialog> ( CMessageBoxDialog::IDD, pParent )
	, m_strMessage(strMessage)
	, m_strTitle(strTitle.IsEmpty() ? AfxGetAppName() : strTitle)
	, m_nStyle(nStyle)
	, m_nHelp(nHelp)
	, m_hIcon(nullptr)
	, m_nTimeoutSeconds(0)
	, m_bTimeoutDisabled(false)
	, m_nTimeoutTimer(0)
	, m_strRegistryKey(_T(""))
	, m_nDefaultButton(IDC_STATIC)
	, m_nEscapeButton(IDC_STATIC)
	, m_sDialogUnit(CSize(0, 0))
	, m_sIcon(CSize(0, 0))
	, m_sMessage(CSize(0, 0))
	, m_sCheckbox(CSize(0, 0))
	, m_sButton(CSize(0, 0))
{
	// Enable the active accessibility.
	ASSERT(!strMessage.IsEmpty());

	m_aButtons.clear();

	const int dpi = DpiAware::GetDpiForWindow((pParent ? pParent : AfxGetMainWnd())->m_hWnd);
	UpdateResources(dpi);
}

/*
 *	Constructor of the class.
 *
 *	This constructor is used to load the strings for the title and the message
 *	text from the resources of this project. If no title is given, the
 *	application name will be used as the title of the dialog.
 */
CMessageBoxDialog::CMessageBoxDialog ( CWnd* pParent, UINT nMessageID,
	UINT nTitleID, UINT nStyle, UINT nHelp ) 
	: CMessageBoxDialog(pParent, 
		LoadResString(nMessageID).c_str(), 
		nTitleID == 0 ? AfxGetAppName() : LoadResString(nTitleID).c_str(), 
		nStyle, nHelp )
{
}

/*
 *	Destructor of the class.
 */
CMessageBoxDialog::~CMessageBoxDialog ( )
{
}

//////////////////////////////////////////////////////////////////////////////
// Methods for setting and retrieving dialog options.

/*
 *	Method for setting the style of the message box.
 */
inline void CMessageBoxDialog::SetStyle ( UINT nStyle )
{
	// Set the style of the message box.
	m_nStyle = nStyle;
}

/*
 *	Method for retrieving the style of the message box.
 */
inline UINT CMessageBoxDialog::GetStyle ( )
{
	// Return the current style of the message box.
	return m_nStyle;
}

/*
 *	Method for setting the message to be displayed in the message box.
 */
inline void CMessageBoxDialog::SetMessage ( LPCTSTR strMessage )
{
	ASSERT(*strMessage != '\0');

	// Save the message text.
	m_strMessage = strMessage;
}

/*
 *	Methods for setting the message to be displayed in the message box.
 */
inline void CMessageBoxDialog::SetMessage ( UINT nMessageID )
{
	// Load the message from the resources.
	m_strMessage = LoadResString(nMessageID);
	ASSERT(!m_strMessage.empty());
}

/*
 *	Method for retrieving the message to be displayed in the message box.
 */
inline const String &CMessageBoxDialog::GetMessage ( )
{
	// Return the message text.
	return m_strMessage;
}

/*
 *	Method for setting the title to be displayed in the message box.
 */
inline void CMessageBoxDialog::SetTitle ( LPCTSTR strTitle )
{
	// Check whether a title was given.
	if ( *strTitle == '\0' )
	{
		// Use the application name as the title.
		strTitle = AfxGetAppName();
	}

	// Save the title.
	m_strTitle = strTitle;
}

/*
 *	Method for setting the title to be displayed in the message box.
 */
inline void CMessageBoxDialog::SetTitle ( UINT nTitleID )
{
	// Check whether an ID was given.
	if ( nTitleID == 0 )
	{
		// Use the application name as the title.
		m_strTitle = AfxGetAppName();
	}
	else
	{
		// Try to load the string from the resources.
		m_strTitle = LoadResString(nTitleID);
		ASSERT(!m_strTitle.empty());
	}
}

/*
 *	Method for retrieving the title to be displayed in the message box.
 */
inline const String &CMessageBoxDialog::GetTitle ( )
{
	// Return the title of the message box.
	return m_strTitle;
}

/*
 *	Method for setting the icon to be displayed in the message box.
 */
inline void CMessageBoxDialog::SetMessageIcon ( HICON hIcon )
{
	ASSERT(hIcon != nullptr);

	// Save the icon.
	m_hIcon = hIcon;
}

/*
 *	Method for setting the icon to be displayed in the message box.
 */
inline void CMessageBoxDialog::SetMessageIcon ( UINT nIconID )
{
	// Try to load the given icon.
	m_hIcon = AfxGetApp()->LoadIcon(nIconID);

	ASSERT(m_hIcon != nullptr);
}

/*
 *	Method for retrieving the icon to be displayed in the message box.
 */
inline HICON CMessageBoxDialog::GetMessageIcon ( )
{
	// Return the icon for the message box.
	return m_hIcon;
}

/*
 *	Method for setting a timeout.
 *
 *	A timeout is a countdown, which starts, when the message box is displayed.
 *	There are two modes for a timeout: The "un-disabled" or "enabled" timeout
 *	means, that the user can choose any button, but if he doesn't choose one,
 *	the default button will be assumed as being chossen, when the countdown is
 *	finished. The other mode, a "disabled" countdown is something like a nag
 *	screen. All buttons will be disabled, until the countdown is finished.
 *	After that, the user can click any button.
 */
void CMessageBoxDialog::SetTimeout ( UINT nSeconds, bool bDisabled /*= false*/)
{
	// Save the settings for the timeout.
	m_nTimeoutSeconds	= nSeconds;
	m_bTimeoutDisabled	= bDisabled;
}

/*
 *	Method for retrieving the seconds for a timeout.
 */
inline UINT CMessageBoxDialog::GetTimeoutSeconds ( )
{
	// Return the seconds for the timeout.
	return m_nTimeoutSeconds;
}

/*
 *	Method for retrieving whether a timeout is disabled.
 */
inline bool CMessageBoxDialog::GetTimeoutDisabled ( )
{
	// Return the flag whether the timeout is disabled.
	return m_bTimeoutDisabled;
}

//////////////////////////////////////////////////////////////////////////////
// Methods for handling the stored states.

/*
 *	Method for resetting the message boxes stored in the registry.
 *
 *	This method removes all results of formerly displayed message boxes from
 *	the registry and therefore resets the state of the message boxes. Even
 *	those, where the user checked "Don't display/ask again" will again be
 *	displayed.
 */
void CMessageBoxDialog::ResetMessageBoxes ( )
{
	// Try to retrieve a handle to the application object.
	CWinApp* pApplication = AfxGetApp();

	ASSERT(pApplication != nullptr);

	// Check whether a handle was retrieved.
	if ( pApplication != nullptr )
	{
		// Delete the message box results stored in the registry.
		pApplication->WriteProfileString(REGISTRY_SECTION_MESSAGEBOX, nullptr, nullptr);
	}
}

//////////////////////////////////////////////////////////////////////////////
// Methods for handling common window functions.

/*
 *	Parameters for passing to ModelessMessageBoxThread.
 */
struct ModelessMesssageBoxParam
{
	CString strMessage;
	UINT nType;
};

/*
 *	Thread for displaying the message boxes asyncronously.
 */
static UINT ModelessMesssageBoxThread(LPVOID lpParam)
{
	struct ModelessMesssageBoxParam *p = (struct ModelessMesssageBoxParam *)lpParam;
	
	// Create the message box dialog.
	CMessageBoxDialog dlgMessage(nullptr, p->strMessage, _T(""), p->nType);
	
	delete p;

	// Display the message box dialog
	dlgMessage.Create(CMessageBoxDialog::IDD);
	dlgMessage.ShowWindow(SW_SHOW);
	dlgMessage.RunModalLoop();
	dlgMessage.DestroyWindow();

	return 0;
}

/*
 * Method for retrieving the former result of the message box from the registry.
 */
int CMessageBoxDialog::GetFormerResult()
{
	// Check whether the registry key was already generated.
	if (m_strRegistryKey.IsEmpty())
	{
		// Create the registry key for this dialog.
		m_strRegistryKey = GenerateRegistryKey();
	}

	// Try to read the former result of the message box from the registry.
	return AfxGetApp()->GetProfileInt(
		REGISTRY_SECTION_MESSAGEBOX, m_strRegistryKey, (-1));
}

/*
 * Method for storing the former result of the message box to the registry.
 */
int CMessageBoxDialog::SetFormerResult(int nResult)
{
	int nOldResult = GetFormerResult();
	// Try to write the former result of the message box to the registry.
	AfxGetApp()->WriteProfileInt(
		REGISTRY_SECTION_MESSAGEBOX, m_strRegistryKey, nResult);
	return nOldResult;
}

/*
 *	Method for displaying the dialog.
 *
 *	If the MB_DONT_DISPLAY_AGAIN or MB_DONT_ASK_AGAIN flag is set, this
 *	method will check, whether a former result for this dialog was stored
 *	in the registry. If yes, the former result will be returned without
 *	displaying the dialog. Otherwise the message box will be displayed in
 *	the normal way.
 */
INT_PTR CMessageBoxDialog::DoModal ( )
{
	// Check whether the result may be retrieved from the registry.
	if ( ( m_nStyle & MB_DONT_DISPLAY_AGAIN ) ||
		( m_nStyle & MB_DONT_ASK_AGAIN ) )
	{
		// Try to read the former result of the message box from the registry.
		int nFormerResult = GetFormerResult();

		// Check whether a result was retrieved.
		if ( nFormerResult != (-1) )
		{
			// Return the former result without displaying the dialog.
			return nFormerResult;
		}
	}

	if (m_nStyle & MB_MODELESS) {
		// Show the messsage box dialog asyncronously.
		ModelessMesssageBoxParam *pParam = new ModelessMesssageBoxParam();
		pParam->strMessage = m_strMessage.c_str();
		pParam->nType      = m_nStyle & ~MB_MODELESS;
		AfxBeginThread(ModelessMesssageBoxThread, pParam);
		return IDOK;
	}

	// Call the parent method.
	return __super::DoModal();
}

/*
 *	Method for closing the dialog.
 *
 *	If the MB_DONT_DISPLAY_AGAIN or MB_DONT_ASK_AGAIN flag is set, this
 *	method will check, one of the checkbox was marked to save the result in
 *	the registry. If yes, the result of this dialog will be stored in the
 *	registry.
 */
void CMessageBoxDialog::EndDialog ( int nResult )
{
	// Create a variable for storing the state of the checkbox.
	bool bDontDisplayAgain = false;

	// Try to access the checkbox control.
	CWnd* pCheckboxWnd = GetDlgItem(IDCHECKBOX);

	// Check whether the control can be accessed.
	if ( pCheckboxWnd != nullptr )
	{
		// Check whether the checkbox is checked.
		bDontDisplayAgain = 
			( ((CButton*)pCheckboxWnd)->GetCheck() == BST_CHECKED );
	}

	// Check whether the result may be stored in the registry.
	if ( ( ( m_nStyle & MB_DONT_DISPLAY_AGAIN ) && bDontDisplayAgain ) ||
		( ( m_nStyle & MB_DONT_ASK_AGAIN ) && bDontDisplayAgain ) )
	{
		SetFormerResult(nResult);
	}
	
	// Call the parent method.
	__super::EndDialog(nResult);
}

/*
 *	Method for initializing the dialog.
 *
 *	This method is used for initializing the dialog. It will create the
 *	content of the dialog, which means it will create all controls and will
 *	size the dialog to fit it's content.
 */
BOOL CMessageBoxDialog::OnInitDialog ( )
{
	// Call the parent method.
	if ( !__super::OnInitDialog() )
	{
		// Return with an error.
		return FALSE;
	}

	// Set the title of the dialog.
	SetWindowText(m_strTitle.c_str());

	// Set the help ID of the dialog.
	SetHelpID(m_nHelp);

	// Parse the style of the message box.
	ParseStyle();

	// Create the elements of the dialog.
	m_tooltips.Create(this);
	CreateIconControl();
	CreateMessageControl();
	CreateCheckboxControl();
	CreateButtonControls();

	// Define the layout of the dialog.
	DefineLayout();

	// Check whether no sound should be generated.
	if ( !( m_nStyle & MB_NO_SOUND ) )
	{
		// Do a beep.
		MessageBeep(m_nStyle & MB_ICONMASK);
	}

	// Check whether the window should be system modal.
	if ( m_nStyle & MB_SYSTEMMODAL )
	{
		// Modify the style of the window.
		ModifyStyle(0, DS_SYSMODAL);
	}

	// Check whether to bring the window to the foreground.
	if ( m_nStyle & MB_SETFOREGROUND )
	{
		// Bring the window to the foreground.
		SetForegroundWindow();
	}

	// Check whether the window should be the topmost window.
	if ( m_nStyle & MB_TOPMOST )
	{
		// Modify the style of the window.
		ModifyStyleEx(0, WS_EX_TOPMOST);
	}

	// Check whether an escape button was defined.
	if ( m_nEscapeButton == IDC_STATIC )
	{
		// Disable the close item from the system menu.
		GetSystemMenu(FALSE)->EnableMenuItem(SC_CLOSE, MF_GRAYED);
	}

	// Check whether a timeout is set.
	if ( m_nTimeoutSeconds > 0 )
	{
		// Check whether it's a disabled timeout.
		if ( m_bTimeoutDisabled )
		{
			// Run through all created buttons.
            for (vector<MSGBOXBTN>::iterator iter = m_aButtons.begin(); iter != m_aButtons.end(); ++iter)
			{
				// Try to retrieve a handle for the button.
                CWnd* pButtonWnd = GetDlgItem(iter->nID);

				ASSERT(pButtonWnd != nullptr);

				// Check whether the handle was retrieved.
				if ( pButtonWnd != nullptr )
				{
					// Disable the button.
					pButtonWnd->EnableWindow(FALSE);
				}
			}

			// Try to retrieve the handle of the checkbox.
			CWnd* pCheckboxWnd = GetDlgItem(IDCHECKBOX);

			// Check whether the checkbox handle was retrieved.
			if ( pCheckboxWnd != nullptr )
			{
				// Disable the checkbox.
				pCheckboxWnd->EnableWindow(FALSE);
			}
		}

		// Install a timer.
		m_nTimeoutTimer = SetTimer(MESSAGE_BOX_TIMER, 1000, nullptr);
	}

	// Check whether a default button was defined.
	if ( m_nDefaultButton != IDC_STATIC )
	{
		// Set the focus to the default button.
		GetDlgItem(m_nDefaultButton)->SetFocus();

		// Set the default ID of the dialog.
		SetDefID(m_nDefaultButton);

		// Return FALSE to set the focus correctly.
		return FALSE;
	}

	// Everything seems to be done successfully.
	return TRUE;
}

/*
 *	Method for handling command messages.
 *
 *	This method will handle command messages, which are those messages, which
 *	are generated, when a user clicks a button of the dialog.
 */
BOOL CMessageBoxDialog::OnCmdMsg ( UINT nID, int nCode, void* pExtra,
	AFX_CMDHANDLERINFO* pHandlerInfo )
{
	// Check whether it's the help button.
	if ( ( nID == IDHELP ) && ( nCode == CN_COMMAND ) )
	{
		// Display the help for this message box.
		OnHelp();

		// The message has been processed successfully.
		return TRUE;
	}

	// Check whether the ID of the control element is interesting for us.
	if ( ( nID != IDC_STATIC ) && ( nID != IDCHECKBOX ) && 
		( nCode == CN_COMMAND ) )
	{
		// End the dialog with the given ID.
		EndDialog(nID);

		// The message has been processed successfully.
		return TRUE;
	}

	// Call the parent method.
	return __super::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

/*
 *	Method for handling messages before dispatching them.
 *
 *	This message will handle message before they get dispatched the normal way
 *	and will therefore implement the additional behavior of this dialog.
 */
BOOL CMessageBoxDialog::PreTranslateMessage ( MSG* pMsg )
{
	if (pMsg->message == WM_LBUTTONDOWN ||
		pMsg->message == WM_LBUTTONUP ||
		pMsg->message == WM_MOUSEMOVE)
	{
		m_tooltips.RelayEvent(pMsg);
	}

	// Check whether it's a key message and whether it's not a disable timeout.
	if ( pMsg->message == WM_KEYDOWN )
	{
		// Check whether a disabled timeout is running.
		if ( m_bTimeoutDisabled && ( m_nTimeoutSeconds > 0 ) )
		{
			// Stop here and do nothing until the timeout is finished.
			return TRUE;
		}

		// Check whether it's the return key.
		if ( pMsg->wParam == VK_RETURN )
		{
			// Try to retrieve the current focus.
			CWnd* pFocusWnd = GetFocus();

			// Check whether a handle was retrieved.
			if ( pFocusWnd != nullptr )
			{
				// Try to determine the ID of the element.
				int nID = pFocusWnd->GetDlgCtrlID();

				// Run through the list of defined buttons.
				for (vector<MSGBOXBTN>::iterator iter = m_aButtons.begin(); iter != m_aButtons.end(); ++iter)
				{
					// Check whether the ID is a button.
					if ( iter->nID == static_cast<UINT>(nID) )
					{
						// Save this ID as the default ID.
						m_nDefaultButton = nID;

						// Break the loop to save time.
						break;
					}
				}

				// End the dialog with the default command.
				EndDialog(m_nDefaultButton);

				// The message has been processed successfully.
				return TRUE;
			}
		}

		// Check whether it's the escape key.
		if ( ( pMsg->wParam == VK_ESCAPE ) || ( pMsg->wParam == VK_CANCEL ) )
		{
			// Check whether an escape button was defined.
			if ( m_nEscapeButton != IDC_STATIC )
			{
				// End the dialog with this ID.
				EndDialog(m_nEscapeButton);
			}

			// The message has been processed successfully.
			return TRUE;
		}
	}

	// Call the parent method.
	return __super::PreTranslateMessage(pMsg);
}

/*
 *	Method for handling a timer event.
 *
 *	When a timeout for the message box is set, this method will perform the
 *	update of the dialog controls every second.
 */
void CMessageBoxDialog::OnTimer ( UINT_PTR nIDEvent )
{
	// Check whether the event is interesting for us.
	if ( nIDEvent == MESSAGE_BOX_TIMER )
	{
		// Decrease the remaining seconds.
		m_nTimeoutSeconds--;

		// Check whether the timeout is finished.
		if ( m_nTimeoutSeconds == 0 )
		{
			// Kill the timer for this event and reset the handle.
			KillTimer(m_nTimeoutTimer);

			// Check whether it has been a disabled timeout.
			if ( m_bTimeoutDisabled )
			{
				// Run through all defined buttons.
				for (vector<MSGBOXBTN>::iterator iter = m_aButtons.begin(); iter != m_aButtons.end(); ++iter)
				{
					// Try to retrieve a handle to access the button.
					CWnd* pButtonWnd = GetDlgItem(iter->nID);

					ASSERT(pButtonWnd != nullptr);

					// Check whether a handle was retrieved.
					if ( pButtonWnd != nullptr )
					{
						// Enable the button again.
						pButtonWnd->EnableWindow(TRUE);
					}
				}

				// Try to retrieve a handle for the checkbox.
				CWnd* pCheckboxWnd = GetDlgItem(IDCHECKBOX);

				// Check whether the checkbox was found.
				if ( pCheckboxWnd != nullptr )
				{
					// Enable the checkbox.
					pCheckboxWnd->EnableWindow(TRUE);
				}
			}
			else
			{
				// End the dialog with the default button.
				EndDialog(m_nDefaultButton);
			}
		}

		// Run through the list of defined buttons.
		for (vector<MSGBOXBTN>::iterator iter = m_aButtons.begin(); iter != m_aButtons.end(); ++iter)
		{
			// Check whether this button is the default button.
			if ( iter->nID == static_cast<UINT>(m_nDefaultButton) )
			{
				// Try to load the text for the button.
				String strButtonText = LoadResString(iter->nTitle);
				// Check whether the timeout is finished.
				if ( m_nTimeoutSeconds > 0 )
				{
					// Add the remaining seconds to the text of the button.
					TCHAR szTimeoutSeconds[40];
					wsprintf(szTimeoutSeconds, _T(" = %u"), m_nTimeoutSeconds);
					strButtonText += szTimeoutSeconds;
				}
				// Set the text of the button.
				SetDlgItemText(iter->nID, strButtonText.c_str());
			}
		}
	}

	// Call the parent method.
	__super::OnTimer(nIDEvent);
}

BOOL CMessageBoxDialog::OnEraseBkgnd(CDC* pDC)
{
	CRect rect;
	GetClientRect(&rect);
	pDC->FillSolidRect(&rect, ::GetSysColor(COLOR_BTNFACE));
	rect.bottom = rect.bottom - YDialogUnitToPixel(CY_BUTTON + CY_BORDER * 2);
	pDC->FillSolidRect(&rect, ::GetSysColor(COLOR_WINDOW));
	return TRUE;
}

HBRUSH CMessageBoxDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (nCtlColor == CTLCOLOR_STATIC)
	{
		pDC->SetBkMode(OPAQUE);
		pDC->SetBkColor(::GetSysColor(COLOR_WINDOW));
		pDC->SetTextColor(m_clrMainInstructionFont);
		return static_cast<HBRUSH>(GetSysColorBrush(COLOR_WINDOW));
	}
	return __super::OnCtlColor(pDC, pWnd, nCtlColor);
}

LRESULT CMessageBoxDialog::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
	__super::OnDpiChanged(wParam, lParam);
	UpdateResources(m_dpi);
	Default();
	if (!GetDlgItem(IDCHECKBOX))
		return 0;
	for (vector<MSGBOXBTN>::iterator iter = m_aButtons.begin(); iter != m_aButtons.end(); ++iter)
		GetDlgItem(iter->nID)->SetFont(&m_font);
	GetDlgItem(IDCHECKBOX)->SetFont(&m_font);
	m_stcMessage.SetFont(&m_fontMainInstruction);
	Invalidate();
	m_sDialogUnit = {};
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Other dialog handling methods.

/*
 *	Method for handling window messages.
 */
BOOL CMessageBoxDialog::OnWndMsg ( UINT message, WPARAM wParam, LPARAM lParam,
	LRESULT* pResult )
{
	// Check whether to close the dialog.
	if ( message == WM_CLOSE )
	{
		// Check whether a disabled timeout is running.
		if ( m_bTimeoutDisabled && ( m_nTimeoutSeconds > 0 ) )
		{
			// Stop here and do nothing until the timeout is finished.
			return TRUE;
		}

		// Check whether an escape button is defined.
		if ( m_nEscapeButton != IDC_STATIC )
		{
			// End the dialog with this command.
			EndDialog(m_nEscapeButton);
		}

		// The message was handled successfully.
		return TRUE;
	}

	// Call the parent method.
	return __super::OnWndMsg(message, wParam, lParam, pResult);
}

BEGIN_MESSAGE_MAP(CMessageBoxDialog, DpiAware::CDpiAwareWnd<CDialog>)
	ON_WM_TIMER()
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////////
// Helper methods.

/*
 *	Method for generating a registry key.
 *
 *	This method tries to create a registry key, which will be used for storing
 *	the result of the message box, if the MB_DONT_DISPLAY_AGAIN or the
 *	MB_DONT_ASK_AGAIN flag is set.
 */
CString CMessageBoxDialog::GenerateRegistryKey ( )
{
	// Create a string to store the registry key.
	CString strRegistryKey = _T("");

	// Check whether a help ID is given.
	if ( m_nHelp != 0 )
	{
		// Simply use the help ID, because we assume, it's unique.
		strRegistryKey.Format(_T("%u"), m_nHelp);
	}
	else
	{
		// POSSIBLE BUG: The following algorithm for creating a checksum is
		// very simple and may not ensure, the registry key is really unique.
		// But for now it may be enough.

		// Create a variable to store the checksum.
		int nChecksum = 0;

		// Run through the message string.
        for ( String::size_type i = 0; i < m_strMessage.length(); i++ )
		{
			// Get the char at the given position and add it to the checksum.
			nChecksum += static_cast<int>(static_cast<int>(m_strMessage[i]) * i);
		}

		// Convert the checksum to a string.
		strRegistryKey.Format(_T("%d"), nChecksum);
	}

	// Return the registry key.
	return strRegistryKey;
}

/*
 *	Method for adding a button to the list of buttons.
 *
 *	This method adds a button to the list of buttons, which will be created in
 *	the dialog, but it will not create the button control itself.
 */
void CMessageBoxDialog::AddButton ( UINT nID, UINT nTitle, bool bIsDefault /*= false*/,
	bool bIsEscape /*= false*/ )
{
	// Create a new structure to store the button information.
	MSGBOXBTN bButton = { nID, nTitle };

	// Add the button to the list of buttons.
    m_aButtons.push_back(bButton);

	// Check whether this button is the default button.
	if ( bIsDefault )
	{
		// Save the ID of the button as the ID of the default button.
		m_nDefaultButton = nID;
	}

	// Check whether this button is the escape button.
	if ( bIsEscape )
	{
		// Save the ID of the button as the ID of the escape button.
		m_nEscapeButton = nID;
	}
}

/*
 *	Method for converting a dialog unit x value to a pixel value.
 */
inline int CMessageBoxDialog::XDialogUnitToPixel ( int x )
{
	// Check whether the dimension of a dialog unit has already been determined.
	if ( m_sDialogUnit.cx == 0 )
	{
		// Create a rect for mapping it to the dialog rect.
		CRect rcDialog(0, 0, CX_DLGUNIT_BASE, CY_DLGUNIT_BASE);

		// Map the rect to the dialog.
		MapDialogRect(rcDialog);

		// Save the rect.
		m_sDialogUnit = rcDialog.Size();
	}

	// Return the converted value.
	return ( x * m_sDialogUnit.cx / CX_DLGUNIT_BASE );
}

/*
 *	Method for converting a dialog unit y value to a pixel value.
 */
inline int CMessageBoxDialog::YDialogUnitToPixel ( int y )
{
	// Check whether the dimension of a dialog unit has already been determined.
	if ( m_sDialogUnit.cy == 0 )
	{
		// Create a rect for mapping it to the dialog rect.
		CRect rcDialog(0, 0, CX_DLGUNIT_BASE, CY_DLGUNIT_BASE);

		// Map the rect to the dialog.
		MapDialogRect(rcDialog);

		// Save the rect.
		m_sDialogUnit = rcDialog.Size();
	}

	// Return the converted value.
	return ( y * m_sDialogUnit.cy / CY_DLGUNIT_BASE );
}

/*
 *	Method for parsing the given style.
 *
 *	This method will parse the given style for the message box and will create
 *	the elements of the dialog box according to it. If you want to add more
 *	user defined styles, simply modify this method.
 */
void CMessageBoxDialog::ParseStyle ( )
{
	// Switch the style of the buttons.
	switch ( m_nStyle & MB_TYPEMASK )
	{

		case MB_OKCANCEL:

			// Add two buttons: "Ok" and "Cancel".
			AddButton(IDOK, IDS_MESSAGEBOX_OK, true);
			AddButton(IDCANCEL, IDS_MESSAGEBOX_CANCEL, false, true);

			break;

		case MB_ABORTRETRYIGNORE:

			// Add three buttons: "Abort", "Retry" and "Ignore".
			AddButton(IDABORT, IDS_MESSAGEBOX_ABORT, true);
			AddButton(IDRETRY, IDS_MESSAGEBOX_RETRY);
			AddButton(IDIGNORE, IDS_MESSAGEBOX_IGNORE);

			break;

		case MB_YESNOCANCEL:

			// Add three buttons: "Yes", "No" and "Cancel".
			AddButton(IDYES, IDS_MESSAGEBOX_YES, true);

			// Check whether to add a "Yes to all" button.
			if ( m_nStyle & MB_YES_TO_ALL )
			{
				// Add the additional button.
				AddButton(IDYESTOALL, IDS_MESSAGEBOX_YES_TO_ALL);
			}

			AddButton(IDNO, IDS_MESSAGEBOX_NO);

			// Check whether to add a "No to all" button.
			if ( m_nStyle & MB_NO_TO_ALL )
			{
				// Add the additional button.
				AddButton(IDNOTOALL, IDS_MESSAGEBOX_NO_TO_ALL);
			}

			AddButton(IDCANCEL, IDS_MESSAGEBOX_CANCEL, false, true);

			break;

		case MB_YESNO:
			
			// Add two buttons: "Yes" and "No".
			AddButton(IDYES, IDS_MESSAGEBOX_YES, true);

			// Check whether to add a "Yes to all" button.
			if ( m_nStyle & MB_YES_TO_ALL )
			{
				// Add the additional button.
				AddButton(IDYESTOALL, IDS_MESSAGEBOX_YES_TO_ALL);
			}

			AddButton(IDNO, IDS_MESSAGEBOX_NO);

			// Check whether to add a "No to all" button.
			if ( m_nStyle & MB_NO_TO_ALL )
			{
				// Add the additional button.
				AddButton(IDNOTOALL, IDS_MESSAGEBOX_NO_TO_ALL);
			}

			break;

		case MB_RETRYCANCEL:

			// Add two buttons: "Retry" and "Cancel".
			AddButton(IDRETRY, IDS_MESSAGEBOX_RETRY, true);
			AddButton(IDCANCEL, IDS_MESSAGEBOX_CANCEL, false, true);

			break;

		case MB_CANCELTRYCONTINUE:

			// Add three buttons: "Cancel", "Try again" and "Continue".
			AddButton(IDCANCEL, IDS_MESSAGEBOX_CANCEL, true, true);
			AddButton(IDTRYAGAIN, IDS_MESSAGEBOX_RETRY);
			AddButton(IDCONTINUE, IDS_MESSAGEBOX_CONTINUE);

			break;

		case MB_CONTINUEABORT:

			// Add two buttons: "Continue" and "Abort".
			AddButton(IDCONTINUE, IDS_MESSAGEBOX_CONTINUE, true);
			AddButton(IDABORT, IDS_MESSAGEBOX_ABORT);

			break;

		case MB_SKIPSKIPALLCANCEL:

			// Add three buttons: "Skip", "Skip all" and "Cancel".
			AddButton(IDSKIP, IDS_MESSAGEBOX_SKIP, true);
			AddButton(IDSKIPALL, IDS_MESSAGEBOX_SKIPALL);
			AddButton(IDCANCEL, IDS_MESSAGEBOX_CANCEL, false, true);

			break;

		case MB_IGNOREIGNOREALLCANCEL:

			// Add three buttons: "Ignore", "Ignore all" and "Cancel".
			AddButton(IDIGNORE, IDS_MESSAGEBOX_IGNORE, true);
			AddButton(IDIGNOREALL, IDS_MESSAGEBOX_IGNOREALL);
			AddButton(IDCANCEL, IDS_MESSAGEBOX_CANCEL, false, true);

			break;

		default:
		case MB_OK:

			// Add just one button: "Ok".
			AddButton(IDOK, IDS_MESSAGEBOX_OK, true, true);

			break;

	}

	// Check whether to add a help button.
	if ( m_nStyle & MB_HELP )
	{
		// Add the help button.
		AddButton(IDHELP, IDS_MESSAGEBOX_HELP);
	}

	// Check whether a default button was defined.
	if ( m_nStyle & MB_DEFMASK )
	{
		// Create a variable to store the index of the default button.
        vector<MSGBOXBTN>::size_type nDefaultIndex = 0;

		// Switch the default button.
		switch ( m_nStyle & MB_DEFMASK )
		{

			case MB_DEFBUTTON1:

				// Set the index of the default button.
				nDefaultIndex = 0;

				break;

			case MB_DEFBUTTON2:

				// Set the index of the default button.
				nDefaultIndex = 1;

				break;

			case MB_DEFBUTTON3:

				// Set the index of the default button.
				nDefaultIndex = 2;

				break;

			case MB_DEFBUTTON4:

				// Set the index of the default button.
				nDefaultIndex = 3;

				break;

			case MB_DEFBUTTON5:

				// Set the index of the default button.
				nDefaultIndex = 4;

				break;

			case MB_DEFBUTTON6:

				// Set the index of the default button.
				nDefaultIndex = 5;

				break;

		}

		// Check whether enough buttons are available.
		if ( m_aButtons.size() >= ( nDefaultIndex + 1 ) )
		{
			// Set the new default button.
			m_nDefaultButton = m_aButtons[nDefaultIndex].nID;
		}
	}

	// Check whether an icon was specified.
	if ( ( m_nStyle & MB_ICONMASK ) && ( m_hIcon == nullptr ) )
	{
		// Switch the icon.
		LPTSTR icon = nullptr;
		switch ( m_nStyle & MB_ICONMASK )
		{

			case MB_ICONEXCLAMATION:

				// Load the icon with the exclamation mark.
				icon = IDI_EXCLAMATION;

				break;

			case MB_ICONHAND:

				// Load the icon with the error symbol.
				icon = IDI_HAND;

				break;

			case MB_ICONQUESTION:

				// Load the icon with the question mark.
				icon = IDI_QUESTION;

				break;

			case MB_ICONASTERISK:

				// Load the icon with the information symbol.
				icon = IDI_ASTERISK;

				break;

		}
		if (icon)
		{
			const int cx = GetSystemMetrics(SM_CXICON);
			const int cy = GetSystemMetrics(SM_CYICON);
			DpiAware::LoadIconWithScaleDown(nullptr, icon, cx, cy, &m_hIcon);
		}
	}
}

/*
 *	Method for creating the icon control.
 *
 *	This method will check whether the handle for an icon was defined and if
 *	yes it will create an control in the dialog to display that icon.
 */
void CMessageBoxDialog::CreateIconControl ( )
{
	// Check whether an icon was defined.
	if ( m_hIcon != nullptr )
	{
		// Create a structure to read information about the icon.
		ICONINFO iiIconInfo;

		// Retrieve information about the icon.
		GetIconInfo(m_hIcon, &iiIconInfo);

		ASSERT(iiIconInfo.fIcon);

		// Create a handle to access the bitmap information of the icon.
		BITMAP bmIcon;

		// Retrieve the bitmap information of the icon.
		GetObject((HGDIOBJ)iiIconInfo.hbmColor, sizeof(bmIcon), &bmIcon);

		// Save the size of the icon.
		m_sIcon.cx = bmIcon.bmWidth;
		m_sIcon.cy = bmIcon.bmHeight;

		// Create a dummy rect for the icon control.
		CRect rcDummy;

		// Create the control for the icon.
		m_stcIcon.Create(nullptr, WS_CHILD | WS_VISIBLE | WS_DISABLED | SS_ICON,
			rcDummy, this, (UINT)IDC_STATIC);

		// Set the icon of the control.
		m_stcIcon.SetIcon(m_hIcon);
	}
}

/*
 *	Method for creating the text control.
 *
 *	This method create the control displaying the text of the message for the
 *	message box. It will also try to determine the size required for the
 *	message.
 */
void CMessageBoxDialog::CreateMessageControl ( )
{
	ASSERT(!m_strMessage.empty());

	// Create a DC for accessing the display driver.
	CDC dcDisplay;
	dcDisplay.CreateDC(_T("DISPLAY"), nullptr, nullptr, nullptr);

	// Select the new font and store the old one.
	CFont* pOldFont = dcDisplay.SelectObject(&m_fontMainInstruction);

	HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info{ sizeof MONITORINFO };
	GetMonitorInfo(monitor, &info);

	// Define the maximum width of the message.
	int nMaxWidth = ( info.rcMonitor.right / 2 ) + 100;

	// Check whether an icon is displayed.
	if ( m_hIcon != nullptr )
	{
		// Decrease the maximum width.
		nMaxWidth -= m_sIcon.cx + 2 * XDialogUnitToPixel(CX_BORDER);
	}

	// Create a rect with the maximum width.
	CRect rcMessage(0, 0, nMaxWidth, nMaxWidth);

	// Draw the text and retrieve the size of the text.
	dcDisplay.DrawText(m_strMessage.c_str(), rcMessage, DT_LEFT | DT_NOPREFIX | 
		DT_WORDBREAK | DT_EXPANDTABS | DT_CALCRECT);

	// Save the size required for the message.
	m_sMessage = rcMessage.Size();

	// Select the old font again.
	dcDisplay.SelectObject(pOldFont);

	// Create a dummy rect for the control.
	CRect rcDummy;

	// Create a variable with the style of the control.
	DWORD dwStyle = WS_CHILD | WS_VISIBLE;

	// Check whether the text should be right aligned.
	if ( m_nStyle & MB_RIGHT )
	{
		// Align the text to the right.
		dwStyle |= SS_RIGHT;
	}
	else
	{
		// Align the text to the left.
		dwStyle |= SS_LEFT;
	}

	// Create the static control for the message.
	String strMessage = m_strMessage;
	strutils::replace(strMessage, _T("&"), _T("&&"));
	m_stcMessage.Create(strMessage.c_str(), dwStyle, rcDummy, this,
		(UINT)IDC_STATIC);

	// Check whether the text will be read from right to left.
	if ( m_nStyle & MB_RTLREADING )
	{
		// Change the extended style of the control.
		m_stcMessage.ModifyStyleEx(0, WS_EX_RTLREADING);
	}

	// Set the font of the dialog.
	m_stcMessage.SetFont(&m_fontMainInstruction);
}

/*
 *	Method for creating the checkbox control.
 *
 *	If the user either specified the MB_DONT_DISPLAY_AGAIN or
 *	MB_DONT_ASK_AGAIN flag, this method will create a checkbox in the dialog
 *	for enabling the user to disable this dialog in the future.
 */
void CMessageBoxDialog::CreateCheckboxControl ( )
{
	// Check whether a checkbox is required.
	if ( ( m_nStyle & MB_DONT_DISPLAY_AGAIN ) ||
		( m_nStyle & MB_DONT_ASK_AGAIN ) )
	{
		// Create a variable for storing the title of the checkbox.
		String strCheckboxTitle;

		// Check which style is used.
		if ( m_nStyle & MB_DONT_DISPLAY_AGAIN )
		{
			// Load the string for the checkbox.
			strCheckboxTitle = LoadResString(IDS_MESSAGEBOX_DONT_DISPLAY_AGAIN);
		}
		else
		{
			// Check which style is used.
			if ( m_nStyle & MB_DONT_ASK_AGAIN )
			{
				// Load the string for the checkbox.
				strCheckboxTitle = LoadResString(IDS_MESSAGEBOX_DONT_ASK_AGAIN);
			}
		}

		ASSERT(!strCheckboxTitle.empty());

		// Create a handle to access the DC of the dialog.
		CClientDC dc(this);

		// Retrieve the font for this dialog and select it.
		CFont* pWndFont = &m_font;
		CFont* pOldFont = dc.SelectObject(pWndFont);

		// Retrieve the size of the text.
		m_sCheckbox = dc.GetTextExtent(strCheckboxTitle.c_str(), static_cast<int>(strCheckboxTitle.length()));

		// Add the additional value to the width of the checkbox.
		m_sCheckbox.cx += XDialogUnitToPixel(CX_CHECKBOX_ADDON);

		// Select the old font again.
		dc.SelectObject(pOldFont);

		// Create a dummy rect for the checkbox.
		CRect rcDummy;

		// Create a handle for the checkbox.
		CButton btnCheckbox;

		// Create the checkbox.
		btnCheckbox.Create(strCheckboxTitle.c_str(), WS_CHILD | WS_VISIBLE | 
			WS_TABSTOP | BS_AUTOCHECKBOX, rcDummy, this, IDCHECKBOX);

		// Check whether the checkbox should be marked checked at startup.
		if ( m_nStyle & MB_DEFAULT_CHECKED )
		{
			// Mark the checkbox.
			btnCheckbox.SetCheck(BST_CHECKED);
		}

		// Set the font of the control.
		btnCheckbox.SetFont(pWndFont);

		// Add a tooltip to the checkbox
		m_tooltips.AddTool(&btnCheckbox, LoadResString(IDS_MESSAGEBOX_CHECKBOX_TOOLTIP).c_str());

		// Remove the subclassing again.
		btnCheckbox.UnsubclassWindow();
	}
}

/*
 *	Method for creating the button controls.
 *
 *	According to the list of buttons, which should be displayed in this
 *	message box, this method will create them and add them to the dialog.
 */
void CMessageBoxDialog::CreateButtonControls ( )
{
	// Initialize the control with the size of the button.
	m_sButton = CSize(XDialogUnitToPixel(CX_BUTTON),
		YDialogUnitToPixel(CY_BUTTON));

	// Create a handle to access the DC of the dialog.
	CClientDC dc(this);

	// Retrieve the font for this dialog and select it.
	CFont* pWndFont = &m_font;
	CFont* pOldFont = dc.SelectObject(pWndFont);

	// Create a dummy rect.
	CRect rcDummy;

	// Run through all buttons defined in the list of the buttons.
	for (vector<MSGBOXBTN>::iterator iter = m_aButtons.begin(); iter != m_aButtons.end(); ++iter)
	{
		// Create a string and load the title of the button.
		String strButtonText = LoadResString(iter->nTitle);
		// Check whether there's a timeout set.
		if ( m_nTimeoutSeconds > 0 )
		{
			// Add the remaining seconds to the text of the button.
			TCHAR szTimeoutSeconds[40];
			wsprintf(szTimeoutSeconds, _T(" = %u"), m_nTimeoutSeconds);
			strButtonText += szTimeoutSeconds;
		}

		// Retrieve the size of the text.
		CSize sButtonText = dc.GetTextExtent(strButtonText.c_str(), static_cast<int>(strButtonText.length()));

		// Resize the button.
		m_sButton.cx = max(m_sButton.cx, sButtonText.cx);
		m_sButton.cy = max(m_sButton.cy, sButtonText.cy);

		// Create a new handle for creating a button control.
		CButton btnControl;

		// Create the button.
		btnControl.Create(strButtonText.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			rcDummy, this, iter->nID);

		// Set the font of the control.
		btnControl.SetFont(pWndFont);

		// Remove the subclassing again.
		btnControl.UnsubclassWindow();
	}

	// Add margins to the button size.
	m_sButton.cx += 2 * XDialogUnitToPixel(CX_BUTTON_BORDER);
	m_sButton.cy += 2 * XDialogUnitToPixel(CY_BUTTON_BORDER);

	// Select the old font again.
	dc.SelectObject(pOldFont);
}

/*
 *	Method for defining the layout of the dialog.
 *
 *	This method will define the actual layout of the dialog. This layout is
 *	based on the created controls for the dialog.
 */
void CMessageBoxDialog::DefineLayout ( )
{
	// Create a variable for storing the size of the dialog.
	CSize sClient = CSize(4 * XDialogUnitToPixel(CX_BORDER),
		3 * YDialogUnitToPixel(CY_BORDER));

	// Create a variable to store the left position for a control element.
	int nXPosition = XDialogUnitToPixel(CX_BORDER) * 2;
	int nYPosition = YDialogUnitToPixel(CY_BORDER) * 2;

	// Check whether an icon is defined.
	if ( m_hIcon != nullptr )
	{
		// Move the icon control.
		m_stcIcon.MoveWindow(XDialogUnitToPixel(CX_BORDER) * 2, 
			YDialogUnitToPixel(CY_BORDER) * 2, m_sIcon.cx, m_sIcon.cy);

		// Add the size of the icon to the size of the dialog.
		sClient.cx += m_sIcon.cx + XDialogUnitToPixel(CX_BORDER);
		sClient.cy += m_sIcon.cy + YDialogUnitToPixel(CY_BORDER);

		// Increase the x position for following control elements.
		nXPosition += m_sIcon.cx + XDialogUnitToPixel(CX_BORDER);
	}

	// Change the size of the dialog according to the size of the message.
	sClient.cx += m_sMessage.cx + XDialogUnitToPixel(CX_BORDER);
	sClient.cy = max(sClient.cy, m_sMessage.cy + 2 * 
		YDialogUnitToPixel(CY_BORDER) + YDialogUnitToPixel(CY_BORDER / 2));

	// Set the position of the message text.
	if (m_sMessage.cy < m_sIcon.cy)
		nYPosition += (m_sIcon.cy - m_sMessage.cy) / 2;
	m_stcMessage.MoveWindow(nXPosition, nYPosition, m_sMessage.cx,
		m_sMessage.cy);

	// Define the new y position.
	nYPosition += m_sMessage.cy + YDialogUnitToPixel(CY_BORDER) +
		YDialogUnitToPixel(CY_BORDER / 2);

	// Check whether an checkbox is defined.
	if ( ( m_nStyle & MB_DONT_DISPLAY_AGAIN ) ||
		( m_nStyle & MB_DONT_ASK_AGAIN ) )
	{
		// Try to determine the control element for the checkbox.
		CWnd* pCheckboxWnd = GetDlgItem(IDCHECKBOX);

		ASSERT(pCheckboxWnd != nullptr);

		// Check whether the control was retrieved.
		if ( pCheckboxWnd != nullptr )
		{
			// Move the checkbox window.
			pCheckboxWnd->MoveWindow(nXPosition, nYPosition, m_sCheckbox.cx,
				m_sCheckbox.cy);

			// Resize the dialog if necessary.
			sClient.cx = max(sClient.cx, nXPosition + m_sCheckbox.cx +
				XDialogUnitToPixel(CX_BORDER));
			sClient.cy = max(sClient.cy, nYPosition + m_sCheckbox.cy +
				YDialogUnitToPixel(CY_BORDER));
		}
	}

	// Calculate the width of the buttons.
	int cxButtons = static_cast<int>(
		( m_aButtons.size() - 1 ) * XDialogUnitToPixel(CX_BUTTON_SPACE) +
		m_aButtons.size() * m_sButton.cx);
	int cyButtons = m_sButton.cy;

	// Add the size of the buttons to the dialog.
	sClient.cx = max(sClient.cx, 2 * XDialogUnitToPixel(CX_BORDER) + cxButtons);
	sClient.cy += cyButtons + YDialogUnitToPixel(CY_BORDER) * 2;

	// Calculate the start y position for the buttons.
	int nXButtonPosition = ( sClient.cx - cxButtons ) / 2;
	int nYButtonPosition = sClient.cy - YDialogUnitToPixel(CY_BORDER) - 
		m_sButton.cy;

	// Check whether the buttons should be right aligned.
	if ( m_nStyle & MB_RIGHT_ALIGN )
	{
		// Right align the buttons.
		nXButtonPosition = sClient.cx - cxButtons - 
			XDialogUnitToPixel(CX_BORDER);
	}

	// Run through all buttons.
	for (vector<MSGBOXBTN>::iterator iter = m_aButtons.begin(); iter != m_aButtons.end(); ++iter)
	{
		// Try to retrieve the handle to access the button.
		CWnd* pButton = GetDlgItem(iter->nID);

		ASSERT(pButton != nullptr);

		// Check whether the handle was retrieved successfully.
		if ( pButton != nullptr )
		{
			// Move the button.
			pButton->MoveWindow(nXButtonPosition, nYButtonPosition, 
				m_sButton.cx, m_sButton.cy);

			// Set the new x position of the next button.
			nXButtonPosition += m_sButton.cx + 
				XDialogUnitToPixel(CX_BUTTON_SPACE);
		}
	}

	// Set the dimensions of the dialog.
	CRect rcClient(0, 0, sClient.cx, sClient.cy);

	// Calculate the window rect.
	CalcWindowRect(rcClient);

	// Move the window.
	MoveWindow(rcClient);

	// Center the window.
	CenterWindow();
}

void CMessageBoxDialog::UpdateResources(int dpi)
{
	LOGFONT lfMessageFont;
	DpiAware::GetNonClientLogFont(lfMessageFont, offsetof(NONCLIENTMETRICS, lfMessageFont), dpi);
	m_font.DeleteObject();
	m_font.CreateFontIndirect(&lfMessageFont);

	m_fontMainInstruction.DeleteObject();
	LOGFONT lf = { 0 };
	HTHEME hTheme = DpiAware::OpenThemeDataForDpi(nullptr, _T("TEXTSTYLE"), dpi);
	if (hTheme != nullptr && SUCCEEDED(GetThemeFont(hTheme, nullptr, TEXT_MAININSTRUCTION, 0, TMT_FONT, &lf)))
	{
		m_fontMainInstruction.CreateFontIndirect(&lf);
		GetThemeColor(hTheme, TEXT_MAININSTRUCTION, 0, TMT_TEXTCOLOR, &m_clrMainInstructionFont);
		CloseThemeData(hTheme);
	}
	else
	{
		m_fontMainInstruction.CreateFontIndirect(&lfMessageFont);
		m_clrMainInstructionFont = GetSysColor(COLOR_WINDOWTEXT);
	}
}
