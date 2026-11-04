/*
 * WinMain -- Win32 entry point, instance/global setup, and the main message loop.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_APP_WIN32_WINMAIN_C_H
#define NW_GROUP_APP_WIN32_WINMAIN_C_H

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
/*   InitializeAndRunWinMain                                    app/win32/InitializeAndRunWinMain.c */
/*   InitializeGlobalsAndInstanceTag                            app/win32/InitializeGlobalsAndInstanceTag.c */
/*   InitializeStandardHandlesInTrackedTable                    app/win32/InitializeStandardHandlesInTrackedTable.c */
/*   LoadWin32sIniVersionPackMajorMinor                         app/win32/LoadWin32sIniVersionPackMajorMinor.c */
/*   RunCommandLineAndWait                                      app/win32/RunCommandLineAndWait.c */
/*   RunMainMessageLoop                                         app/win32/RunMainMessageLoop.c */
/*   VerifyWin32sVersionOrReportStartupError                    app/win32/VerifyWin32sVersionOrReportStartupError.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern int InitializeAndRunWinMain (void);
extern void InitializeGlobalsAndInstanceTag (void);
extern void InitializeStandardHandlesInTrackedTable (void);
extern uint32_t LoadWin32sIniVersionPackMajorMinor (void);
extern int RunCommandLineAndWait (int commandLine);
extern int RunMainMessageLoop (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
extern int VerifyWin32sVersionOrReportStartupError (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_APP_WIN32_WINMAIN_C_H */
