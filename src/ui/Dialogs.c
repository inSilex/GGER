/*
 * Dialogs -- deferred error strings and message-box helpers.
 *
 * Module: ui
 * Interface: include/Dialogs.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   FlushDeferredErrorString                                   ui/FlushDeferredErrorString.c
 *   ShowMainWindowErrorBoxWithFixedCaption                     ui/ShowMainWindowErrorBoxWithFixedCaption.c
 */

/* ==========================================================================
 * Unit: FlushDeferredErrorString
 * Source: ui/FlushDeferredErrorString.c
 * ========================================================================== */
#ifdef NW_UNIT_FlushDeferredErrorString
/*
 * Flush Deferred Error String.
 *
 * Module: ui
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#ui-flushdeferrederrorstring-c
 */

#include <stddef.h> // NULL only; no other headers needed for this function

extern void __stdcall ShowMainWindowErrorBoxWithFixedCaption(void *error_string); 
extern char *g_pending_error_string;                                    

void FlushDeferredErrorString(void)
{
#ifdef NWDEBUG_ENABLED
    { extern void nwtrace_hex(const char *, unsigned long);
      nwtrace_hex("err_flush", (unsigned long)(size_t)g_pending_error_string); }
#endif
    if (g_pending_error_string != NULL) {   
        ShowMainWindowErrorBoxWithFixedCaption(g_pending_error_string); 
        g_pending_error_string = NULL;      
    }
}
#endif /* NW_UNIT_FlushDeferredErrorString */

/* ==========================================================================
 * Unit: ShowMainWindowErrorBoxWithFixedCaption
 * Source: ui/ShowMainWindowErrorBoxWithFixedCaption.c
 * ========================================================================== */
#ifdef NW_UNIT_ShowMainWindowErrorBoxWithFixedCaption
/*
 * Shows an error box with the application caption.
 *
 * Module: ui
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#ui-showmainwindowerrorboxwithfixedcaption-c
 */

#include <windows.h>   /* LPCSTR */
#include <stdint.h>

extern void ShowMainWindowErrorBoxAfterBeep(LPCSTR lpszText, LPCSTR lpszCaption);

void __stdcall ShowMainWindowErrorBoxWithFixedCaption(LPCSTR lpszText)
{
    ShowMainWindowErrorBoxAfterBeep(lpszText, "Game Maker Player");
}
#endif /* NW_UNIT_ShowMainWindowErrorBoxWithFixedCaption */
