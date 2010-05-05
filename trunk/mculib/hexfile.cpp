// Utility class for displaying, loading, and saving bainary data in a char
// array. For saving and loading, this class supports the following file
// types: raw binary, Intel HEX, Motorola S-REC, and Atmel Generic. This
// class also makes use of the ShineInHex.dll to create a MDI child
// hex edit window to allow displaying and modifying of the binary data.
//
// Copyright (C) 2010 Wojciech Stryjewski <thvortex@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation; either version 2.1 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//
// The ShineInHex.dll control used by this class was written by Ramunas Cvirka
// <ramguru_zo@yahoo.com>. The author's original website is no longer available
// (http://www.stud.ktu.lt/~ramcvir/ShineInHex/) however I was able to get in
// touch within him by email to verify that the control can be used for any
// purpose. The original "SIH_final_14F.zip" file containing the dll, source,
// and documentation can be download from the MASM Forum at:
// http://www.masm32.com/board/index.php?topic=3671.msg27404
//

#include <windows.h>
#include <commctrl.h>
#pragma hdrstop
#include "hexfile.h"

// Window class name used only internally
#define CLASS_NAME "VMLAB Hexfile Editor"

// These macros check the assertion that a Win32 API call returned succesfully.
// If the call failed, then this macro will display a message box with detailed
// error information.
#define W32_ASSERT(cond) if((cond) == 0) \
   { w32_error("W32_ASSERT( "#cond" );", __FILE__, __LINE__); }

// Initial size of MDI child window (not client area)
enum { INIT_WIDTH = 629, INIT_HEIGHT = 305 };

// Minimum allowed size of MDI child window (not client area)
enum { MIN_WIDTH = 250, MIN_HEIGHT = 64 };

// Window messages supported by hex editor control (no .h file available)
enum {
   HEXM_SETFONT          = WM_USER+100,
   HEXM_SETOFFSETGRADCOL = WM_USER+101,
   HEXM_SETHEADERGRADCOL = WM_USER+102,
   HEXM_SETVIEW1TEXTCOL  = WM_USER+103,
   HEXM_SETHEADERTEXTCOL = WM_USER+104,
   HEXM_SETVIEW2COL      = WM_USER+105,
   HEXM_SETVIEW3COL      = WM_USER+106,
   HEXM_SETVIEW2SELCOL   = WM_USER+107,
   HEXM_SETVIEW3SELCOL   = WM_USER+108,
   HEXM_SETACTIVECHARCOL = WM_USER+109,
   HEXM_SETMODBYTES1COL  = WM_USER+110,
   HEXM_SETMODBYTES2COL  = WM_USER+111,
   HEXM_SETMODBYTES3COL  = WM_USER+112,
   HEXM_SETPOINTER       = WM_USER+113,
   HEXM_UNSETPOINTER     = WM_USER+114,
   HEXM_SETOFFSET        = WM_USER+115,
   HEXM_SETSEL           = WM_USER+116,
   HEXM_UNDO             = WM_USER+117,
   HEXM_REDO             = WM_USER+118,
   HEXM_CANUNDO		    = WM_USER+119,
   HEXM_CANREDO          = WM_USER+120,
   HEXM_SETREADONLY      = WM_USER+121,
};

// Global variables shared by all Hexfile objects; initialized by first init()
HMODULE VMLAB_module = NULL; // Handle to VMLAB.EXE for icon loading
HMODULE Library = NULL;      // Loaded ShineInHex.dll library
HWND VMLAB_window = NULL;    // Top level window that owns all dialog boxes
HWND MDI_client = NULL;      // MDI client window inside VMLAB window
ATOM MDI_class = NULL;       // Window class of MDI_child window
int Ref_count = 0;           // Reference count for library/class unloading

void w32_error(char *pCode, char *pFile, int pLine)
//*************************
// This function is called if any Windows API call has failed. It uses the
// GetLastError() API to retrieve an extended error code value, calls
// FormatMessage() to produce a human readable error message, and then calls
// MessageBox() to display the error message returned by FormatMessage().
// The PRINT() interface function cannot be used here because this may be
// called from the DLL static constructor when VMLAB is not ready yet to handle
// any interface function calls from the components.
// TODO: Use second FormatMessage() to display pFile and pLine
{
   DWORD errorValue, bytesWritten;
   char *msgPtr = NULL;

   errorValue = GetLastError();   
   bytesWritten = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | // dwFlags (allocate string buffer)
      FORMAT_MESSAGE_FROM_SYSTEM |     // dwFlags (use system message table)
      FORMAT_MESSAGE_IGNORE_INSERTS,   // dwFlags (no va_list passed in)
      NULL,            // lpSource (ignored for system messages)
      errorValue,      // dwMessageId (return value from GetLastError)
      0,               // dwLanguageId (0 means system specified)
      (LPTSTR)&msgPtr, // lpBuffer (returns pointer to allocated buffer)
      0,               // nSize (no minimum size specified for allocated buffer)
      NULL             // Arguments (no va_list arguments needed)
   );
  
   // Display message box if the message was succesfully retrieved or print a
   // generic error if no message could be found. The VMLAB_window could be
   // NULL but that won't cause any major problems here.
   if(bytesWritten) {
      MessageBox(VMLAB_window, msgPtr, pCode, MB_OK | MB_ICONERROR);
      LocalFree(msgPtr);
   } else {
      MessageBox(VMLAB_window, "An unknown Win32 error has occurred.", pCode,
         MB_OK | MB_ICONERROR);
   }
}

void hide(HWND pWindow)
//******************************
// This function is called from Hexfile::hide(void) and from MDI_proc()
// (on WM_SYSCOMMAND/SC_CLOSE) to perform the real work of hiding the MDI
// child window.
{
   // Hide this MDI child window, active the next MDI child, and request that
   // the MDI client refreshes the "Windows" menu in the top-most VMALB window
   // to remove the hidden MDI child.
   ShowWindow(pWindow, SW_HIDE);
   SendMessage(MDI_client, WM_MDINEXT, 0, 0);
   SendMessage(MDI_client, WM_MDIREFRESHMENU, 0, 0);
   W32_ASSERT( DrawMenuBar(VMLAB_window) );   
}

LRESULT CALLBACK MDI_proc(HWND pWindow, UINT pMessage, WPARAM pW, LPARAM pL)
//******************************
{
   HWND child;

   switch(pMessage) {
      // System menu (close/minimize/maximise) command. Closing the window
      // (SC_CLOSE) will merely hide it instead of destryoing it. All other
      // system commands are passed through.
      case WM_SYSCOMMAND:
      {
         if(pW == SC_CLOSE) {
            hide(pWindow);
            return true;
         }
         break;
      }

      // Returns the minimum size that the user can drag the window to. Since
      // the hex editor control doesn't draw properly at small sizes, a minimum
      // width has to be enforced here. For consistency, a minimum height is
      // also enforced which guarantees at least one row of data is visible.
      case WM_GETMINMAXINFO:
      {
         MINMAXINFO *info = (MINMAXINFO *) pL;
         info->ptMinTrackSize.x = MIN_WIDTH;
         info->ptMinTrackSize.y = MIN_HEIGHT;
         break;
      }
      
      // If the MDI window changed size then update hex editor size to
      // always fill entire client area of MDI window. Note that the first
      // WM_SIZE passed to MDI_proc is before the HEX_child window exists
      // yet.
      case WM_SIZE:
      {
         HWND child = GetWindow(pWindow, GW_CHILD);
         if(child) {
            W32_ASSERT( MoveWindow(child, 0, 0, LOWORD(pL), HIWORD(pL), true) );
         }
         break;
      }
      
      // If MDI client has gained keyboard focus, then pass the focus to the
      // child hex editor. Using WM_MDIACTIVATE is not good enough here
      // because the latter message is not sent if only the parent MDI frame
      // is activated, but the active MDI child has not changed.
      case WM_SETFOCUS:
      {
         HWND child = GetWindow(pWindow, GW_CHILD);
         if(child) {
            SendMessage(child, WM_SETFOCUS, 0, 0);
         }
         break;
      }

      default:
         break;
   };

   // All other messages must be passed to default MDI child window procedure
   return DefMDIChildProc(pWindow, pMessage, pW, pL);
}

Hexfile::Hexfile()
//******************************
// The default constructor allows Hexfile objects to be placed directly in
// the DECLAVE_VAR block. The real initialization will hapen when the init()
// function is called.
{
   // Global reference count is used in destructor to release all class
   // resources acquired in the very first init() call.
   Ref_count++;
}

Hexfile::~Hexfile()
//******************************
// Destructor releases resource previously acquired in init(). Based on the
// static reference count, the last Hexfile instance to be destroyed also
// releases global resources acquired by the first init() call.
{
   // Destroying the MDI_child will also destroy the contained HEX_child.
   if(MDI_child) {
      SendMessage(MDI_client, WM_MDIDESTROY, (WPARAM) MDI_child, 0);
   }
   
   // The last instance of the Hexfile class must also unregister the Win32
   // window class and unload the DLL library from init(). All class variables
   // are set to NULL so the next init() can re-initialize them.
   Ref_count--;
   if(Ref_count == 0) {
      if(MDI_class) {
         W32_ASSERT( UnregisterClass( (LPCTSTR) MDI_class, Instance) );
         MDI_class = NULL;
      }
      if(Library) {
         W32_ASSERT( FreeLibrary(Library) );
         Library = NULL;
      }
      VMLAB_window = NULL;
      MDI_client = NULL;
   }
}

void Hexfile::init(HINSTANCE pInstance, HWND pHandle, char *pTitle, int pIcon)
//******************************
// Must be called at least once on each Hexfile object before calling any
// other methods. The "pInstance" must have been saved by the caller from
// the DllEntryPoint() or DllMain() function. The "pHandle" should be the
// same as passed to the On_window_init() callback. The "pTitle" specifies
// the window title of the child window. The "pIcon" is optional and specifies
// the resource ID for a small icon to display in the corner of the window;
// if the resource doesn't exist in the pInstance DLL then the main VMLAB
// executable is searched.
{
   // TODO: Verify args are not NULL; 
   // TODO: Verify not already called on same window
   
   // First init() finds top-level VMLAB window so it can later be passed as
   // hwndOwner to GetOpenFileName(), GetSaveFileName(), and MessageBox().
   // FindWindow() cannot be used here because there could be two VMLAB
   // windows open if running in multiprocess mode.
   if(VMLAB_window == NULL) {
      VMLAB_window = pHandle;
      while(HWND parent = GetParent(VMLAB_window)) {
         VMLAB_window = parent;
      }
   }
   
   // Lookup the module handle to VMLAB.EXE from the window class of the
   // main VMLAB window. This module handle is used in LoadIcon() later on
   // to allow using icons from the main executable if an icon resource
   // was not found in the user's component DLL.
   if(VMLAB_module == NULL) {      
      W32_ASSERT( VMLAB_module = (HINSTANCE) GetClassLong(VMLAB_window, GCL_HMODULE) );
   }
   
   // First init() finds the MDI client window which is a child of the
   // top-level VMLAB window. The MDI client window acts as a parent for the
   // MDI child window that will is created to display the hex editor.
   if(MDI_client == NULL) {
      W32_ASSERT( MDI_client = FindWindowEx(VMLAB_window, NULL,
         "MDIClient", NULL) );
   }
   
   // Load DLL to automatically register "SHINEINHEX" window class
   if(Library == NULL) {
      W32_ASSERT( Library = LoadLibrary("ShineInHex.dll") );
   }
   
   // The HINSTANCE must be saved so that it can be later passed to
   // UnregisterClass() in the destructor.
   if(Instance == NULL) {
      Instance = pInstance;
   }
   
   // Try to load specified icon resource first from user's DLL and failing
   // that, from the main VMLAB executable. Because LoadIcon() returns a
   // shared icon, it is not necessary to later unload the icon in the
   // destructor.
   if(Icon == NULL && pIcon) {
      Icon = LoadIcon(pInstance, MAKEINTRESOURCE(pIcon));
      if(Icon == NULL) {
         W32_ASSERT( Icon = LoadIcon(VMLAB_module, MAKEINTRESOURCE(pIcon)) );
      }
   }

   // Register custom window class to handle behavior of the MDI child window
   if(MDI_class == NULL) {
      WNDCLASSEX mdiClass = {
         sizeof(WNDCLASSEX),  // cbSize (size of entire structure)
         0,                   // style (for performance don't clip children) // TODO??
         MDI_proc,            // lpfnWndProc (pointer to window procedure)
         0,                   // cbClsExtra (no extra class memory needed)
         0,                   // cbWndExtra (no extra window memory needed)
         pInstance,           // hInstance (DLL instance from DllEntryPoint)
         NULL,                // hIcon (large icon not used for child windows)
         NULL,                // hCursor (use default cursor) // TODO: use constant
         NULL,                // hbrBackground (hex editor fills entire window)
         NULL,                // lpszMenuName (no default class menu needed)
         CLASS_NAME,          // lpszClassName (class name used only in Hexfile)
         Icon,                // hIconSm (small icon loaded from resource)
      };
      W32_ASSERT( MDI_class = RegisterClassEx(&mdiClass) );
   }

   // Create initially hidden MDI child window containing the hex editor
   // control window as a child. Calling show() will make the window visible
   // and clicking the window's close box (SC_CLOSE) will hide it again.   
   if(MDI_child == NULL) {
      MDI_child = CreateMDIWindow(
         CLASS_NAME,      // lpClassName (returned by RegisterClassEx)
         pTitle,          // lpWindowName (text displayed in title bar)
         WS_OVERLAPPEDWINDOW, // dwStyle (same style as other MDI children)
         0,               // X (upper left corner of VMLAB window)
         0,               // Y (upper left corner of VMLAB window)
         INIT_WIDTH,      // nWidth (default based on Control Panel size)
         INIT_HEIGHT,     // nHeight (default based on Control Panel size)
         MDI_client,      // hWndParent (parent MDI client window for child)
         pInstance,       // hInstance (window was created by this DLL)
         NULL             // lParam (no special value to pass to window proc)
      );
      W32_ASSERT( MDI_child );
      
      // TODO: Assert that HEX_child == NULL
      
      // Hex editor window alays fills entire client area of MDI child
      RECT rect;
      W32_ASSERT( GetClientRect(MDI_child, &rect) );      
      
      // Create a hex editor window embedded in MDI child window
      HEX_child = CreateWindowEx(
         WS_EX_CLIENTEDGE,// dwExStyle (extra inset border to match VMLAB)
         "SHINEINHEX",    // lpClassName (registered by loading ShineInHex.dll)
         "",              // lpWindowName (not dispplayed by hex editor)
         WS_VISIBLE |     // dwStyle (always visible in parent window)
         WS_CHILD |       // dwStyle (child window with no frame)
         WS_VSCROLL,      // dwStyle (show vertical scroll bar)
         0,               // x (fills entire client area of parent)
         0,               // y (fills entire client area of parent)
         rect.right,      // nWidth (fills entire client area of parent)
         rect.bottom,     // nHeight (fills entire client area of parent)
         MDI_child,       // hWndParent (control embeds in MDI child window)
         NULL,            // hMenu (used as id; not needed for one child)
         pInstance,       // hInstance (window was created by this DLL)
         NULL             // lParam (no special value to pass to window proc)
      );
      W32_ASSERT( HEX_child );
      
      // All text in hex edit is displayed in black and all backgrounds are
      // drawn in white to match the layout of other VMLAB windows. Current
      // cursor position is shown as white text on a black background, and
      // selections in the hex editor are white text on a blue background.
      COLORREF txtColFg = GetSysColor(COLOR_WINDOWTEXT);
      COLORREF txtColBg = GetSysColor(COLOR_WINDOW);
      COLORREF selColFg = GetSysColor(COLOR_HIGHLIGHTTEXT);
      COLORREF selColBg = GetSysColor(COLOR_HIGHLIGHT);
      COLORREF hdrColFg = GetSysColor(COLOR_BTNTEXT);
      COLORREF hdrColBg = GetSysColor(COLOR_BTNFACE);
      
      // Initialize display settings for ShineInHex control
      SendMessage(HEX_child, HEXM_SETFONT, 3, 0); // Courier New font
      SendMessage(HEX_child, HEXM_SETOFFSETGRADCOL, hdrColBg, hdrColBg);
      SendMessage(HEX_child, HEXM_SETHEADERGRADCOL, hdrColBg, hdrColBg);
      SendMessage(HEX_child, HEXM_SETVIEW1TEXTCOL, hdrColFg, hdrColFg);
      SendMessage(HEX_child, HEXM_SETHEADERTEXTCOL, hdrColFg, hdrColFg);
      SendMessage(HEX_child, HEXM_SETVIEW2COL, txtColBg, txtColFg);
      SendMessage(HEX_child, HEXM_SETVIEW3COL, txtColBg, txtColFg);
      SendMessage(HEX_child, HEXM_SETVIEW2SELCOL, selColBg, selColFg);
      SendMessage(HEX_child, HEXM_SETVIEW3SELCOL, selColBg, selColFg);
      SendMessage(HEX_child, HEXM_SETACTIVECHARCOL, txtColFg, txtColBg);
      SendMessage(HEX_child, HEXM_SETMODBYTES1COL, txtColBg, txtColFg);
      SendMessage(HEX_child, HEXM_SETMODBYTES2COL, txtColBg, txtColFg);
      SendMessage(HEX_child, HEXM_SETMODBYTES3COL, txtColBg, txtColFg);
      
      // The hex editor doesn't work very well without any data and
      // will in fact crash if you try typing in the edit window. If
      // show() is called before data(), this will ensure the editor
      // functions properly until data() is called.
      SendMessage(HEX_child, HEXM_SETPOINTER, (WPARAM) &Dummy, 1);      
   }
}

void Hexfile::data(void *pPointer, int pSize, int pOffset)
//******************************
{
   // TODO: assert that HEX_child should always exist

   // Update local variables used by load() and save()
   Pointer = pPointer;
   Size = pSize;
   Offset = pOffset;
   
   // Update internal state in the editor. Because the editor is not stable
   // without any data, the Dummy member variable is used as a placeholder
   // when unsetting the internal data pointers.   
   SendMessage(HEX_child, HEXM_SETOFFSET, (WPARAM) Offset, 0);
   if(Pointer && Size) {
      SendMessage(HEX_child, HEXM_SETPOINTER, (WPARAM) Pointer, Size);
   } else {
      SendMessage(HEX_child, HEXM_SETPOINTER, (WPARAM) &Dummy, 1);
   }
   
   // Force a redraw of the hex editor window
   InvalidateRect(HEX_child, NULL, true);
}
void Hexfile::hide()
//******************************
// Hide the window from view. Although this function is available publicly,
// calling should not be necessary since closing the window will automatically
// perform the same action.
{
   // TODO: Assert MDI_child window already exists; init() was called

   ::hide(MDI_child);
}

void Hexfile::refresh()
//******************************
// Update displayed hex data because simulation has changed memory contents
{
   // Hex editor seems to use double buffering to prevent flicker. Sending
   // a HEXM_SETPOINTER by calling data() will force the editor to re-read
   // the data memory..
   data(Pointer, Size, Offset);
}

void Hexfile::show()
//******************************
// Called to handle "View" button in the GUI. the MDI child window that was
// previously created inside init() is made visible.
{   
   // TODO: Assert MDI_child window already exists; init() was called
      
   // Make window visible and activate (i.e. is brought to the front and
   // obtains keyboard focus). Request that the MDI client refreshes the
   // "Windows" menu in the top-most VMALB window to include the now visible
   // MDI child.
   ShowWindow(MDI_child, SW_SHOW);
   SendMessage(MDI_client, WM_MDIACTIVATE, (WPARAM) MDI_child, 0);   
   SendMessage(MDI_client, WM_MDIREFRESHMENU, 0, 0);
   W32_ASSERT( DrawMenuBar(VMLAB_window) );   
}

void Hexfile::readonly(bool pReadOnly)
//******************************
// Set the hex editor control to either read-only or read/write
{
   // TODO: Assert HEX_child window already exists; init was called()
   
   SendMessage(HEX_child, HEXM_SETREADONLY, pReadOnly, 0);
}
