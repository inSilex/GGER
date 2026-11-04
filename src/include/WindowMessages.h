/*
 * WindowMessages -- Win32 message pumping and window-procedure dispatch.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_INPUT_WIN32_WINDOWMESSAGES_C_H
#define NW_GROUP_INPUT_WIN32_WINDOWMESSAGES_C_H

#include <stdint.h>
#include <stddef.h>
#include <windows.h>

#ifndef NW_GROUP_COMPAT_TYPES
#define NW_GROUP_COMPAT_TYPES
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef unsigned int uint;
typedef unsigned char byte;
typedef uint32_t dword;
typedef void *HANDLE_T;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Units in this group: */
/*   DispatchQueuedMessagesAndStampStartTime                    input/win32/DispatchQueuedMessagesAndStampStartTime.c */
/*   DispatchQueuedNonInputMessages                             input/win32/DispatchQueuedNonInputMessages.c */
/*   GetActiveArrowCursor                                       input/win32/GetActiveArrowCursor.c */
/*   HandleMainWindowMessage                                    input/win32/HandleMainWindowMessage.c */
/*   ProcessMainWindowMessages                                  input/win32/ProcessMainWindowMessages.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern BYTE __stdcall DispatchQueuedMessagesAndStampStartTime(DWORD *pdwStartTime);
extern void DispatchQueuedNonInputMessages (void);
extern HCURSOR GetActiveArrowCursor (void);
extern LRESULT __stdcall HandleMainWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern LRESULT __stdcall ProcessMainWindowMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern LRESULT __stdcall GmwPlayerWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_INPUT_WIN32_WINDOWMESSAGES_C_H */
