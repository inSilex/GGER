/*
 * AppShutdown -- quoted command construction, resource release, and process exit.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_APP_WIN32_APPSHUTDOWN_C_H
#define NW_GROUP_APP_WIN32_APPSHUTDOWN_C_H

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
/*   BuildQuotedCommandImage                                    app/win32/BuildQuotedCommandImage.c */
/*   ShutdownAndReleaseResources                                app/win32/ShutdownAndReleaseResources.c */
/*   TerminateWithExitStatusCode                                app/win32/TerminateWithExitStatusCode.c */
/*   WriteStdoutStringAndExit                                   app/win32/WriteStdoutStringAndExit.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern char *BuildQuotedCommandImage (const char *pszCommandLine);
extern void ShutdownAndReleaseResources (void);
extern void TerminateWithExitStatusCode (UINT uExitCode);
extern void WriteStdoutStringAndExit (char *szMessage);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_APP_WIN32_APPSHUTDOWN_C_H */
