/*
 * USER initialization code
 */

#include "windef.h"
#include "wingdi.h"
#include "winuser.h"
#include "winreg.h"
#include "wine/winbase16.h"
#include "wine/winuser16.h"

#include "dce.h"
#include "dialog.h"
#include "global.h"
#include "input.h"
#include "keyboard.h"
#include "menu.h"
#include "message.h"
#include "queue.h"
#include "spy.h"
#include "sysmetrics.h"
#include "user.h"
#include "win.h"
#include "debugtools.h"

DEFAULT_DEBUG_CHANNEL(graphics);

USER_DRIVER USER_Driver;

static HMODULE graphics_driver;

#define GET_USER_FUNC(name) \
   if (!(USER_Driver.p##name = (void*)GetProcAddress( graphics_driver, #name ))) \
      FIXME("%s not found in graphics driver\n", #name)

/* load the graphics driver */
static BOOL load_driver(void)
{
    char buffer[MAX_PATH];
    HKEY hkey;
    DWORD type, count;

    if (RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\Wine", 0, NULL,
                         REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL ))
    {
        MESSAGE("load_driver: Cannot create config registry key\n" );
        return FALSE;
    }
    count = sizeof(buffer);
    if (RegQueryValueExA( hkey, "GraphicsDriver", 0, &type, buffer, &count ))
        strcpy( buffer, "x11drv" );  /* default value */
    RegCloseKey( hkey );

    if (!(graphics_driver = LoadLibraryA( buffer )))
    {
        MESSAGE( "Could not load graphics driver '%s'\n", buffer );
        return FALSE;
    }

    GET_USER_FUNC(Synchronize);
    GET_USER_FUNC(CheckFocus);
    GET_USER_FUNC(UserRepaintDisable);
    GET_USER_FUNC(InitKeyboard);
    GET_USER_FUNC(VkKeyScan);
    GET_USER_FUNC(MapVirtualKey);
    GET_USER_FUNC(GetKeyNameText);
    GET_USER_FUNC(ToUnicode);
    GET_USER_FUNC(GetBeepActive);
    GET_USER_FUNC(SetBeepActive);
    GET_USER_FUNC(Beep);
    GET_USER_FUNC(GetDIState);
    GET_USER_FUNC(GetDIData);
    GET_USER_FUNC(GetKeyboardConfig);
    GET_USER_FUNC(SetKeyboardConfig);
    GET_USER_FUNC(InitMouse);
    GET_USER_FUNC(SetCursor);
    GET_USER_FUNC(MoveCursor);
    GET_USER_FUNC(GetScreenSaveActive);
    GET_USER_FUNC(SetScreenSaveActive);
    GET_USER_FUNC(GetScreenSaveTimeout);
    GET_USER_FUNC(SetScreenSaveTimeout);
    GET_USER_FUNC(LoadOEMResource);
    GET_USER_FUNC(IsSingleWindow);
    GET_USER_FUNC(AcquireClipboard);
    GET_USER_FUNC(ReleaseClipboard);
    GET_USER_FUNC(SetClipboardData);
    GET_USER_FUNC(GetClipboardData);
    GET_USER_FUNC(IsClipboardFormatAvailable);
    GET_USER_FUNC(RegisterClipboardFormat);
    GET_USER_FUNC(IsSelectionOwner);
    GET_USER_FUNC(ResetSelectionOwner);

    return TRUE;
}


/***********************************************************************
 *           palette_init
 *
 * Patch the function pointers in GDI for SelectPalette and RealizePalette
 */
static void palette_init(void)
{
    void **ptr;
    HMODULE module = GetModuleHandleA( "gdi32" );
    if (!module)
    {
        ERR( "cannot get GDI32 handle\n" );
        return;
    }
    if ((ptr = (void**)GetProcAddress( module, "pfnSelectPalette" ))) *ptr = SelectPalette16;
    else ERR( "cannot find pfnSelectPalette in GDI32\n" );
    if ((ptr = (void**)GetProcAddress( module, "pfnRealizePalette" ))) *ptr = UserRealizePalette;
    else ERR( "cannot find pfnRealizePalette in GDI32\n" );
}


/***********************************************************************
 *           USER initialisation routine
 */
BOOL WINAPI USER_Init(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    HINSTANCE16 instance;
    int queueSize;

    if ( USER_HeapSel ) return TRUE;

    /* Create USER heap */
    if ((instance = LoadLibrary16( "USER.EXE" )) < 32) return FALSE;
    USER_HeapSel = GlobalHandleToSel16( instance );

     /* Global atom table initialisation */
    if (!ATOM_Init( USER_HeapSel )) return FALSE;

    /* Load the graphics driver */
    if (!load_driver()) return FALSE;

    /* Initialize window handling (critical section) */
    WIN_Init();

    /* Initialize system colors and metrics*/
    SYSMETRICS_Init();
    SYSCOLOR_Init();

    /* Setup palette function pointers */
    palette_init();

    /* Create the DCEs */
    DCE_Init();

    /* Initialize window procedures */
    if (!WINPROC_Init()) return FALSE;

    /* Initialize built-in window classes */
    if (!WIDGETS_Init()) return FALSE;

    /* Initialize dialog manager */
    if (!DIALOG_Init()) return FALSE;

    /* Initialize menus */
    if (!MENU_Init()) return FALSE;

    /* Initialize message spying */
    if (!SPY_Init()) return FALSE;

    /* Create system message queue */
    queueSize = GetProfileIntA( "windows", "TypeAhead", 120 );
    if (!QUEUE_CreateSysMsgQueue( queueSize )) return FALSE;

    /* Set double click time */
    SetDoubleClickTime( GetProfileIntA("windows","DoubleClickSpeed",452) );

    /* Create message queue of initial thread */
    InitThreadInput16( 0, 0 );

    /* Create desktop window */
    if (!WIN_CreateDesktopWindow()) return FALSE;

    /* Initialize keyboard driver */
    KEYBOARD_Enable( keybd_event, InputKeyStateTable );

    /* Initialize mouse driver */
    MOUSE_Enable( mouse_event );

    /* Start processing X events */
    USER_Driver.pUserRepaintDisable( FALSE );

    return TRUE;
}
