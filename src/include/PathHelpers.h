/*
 * PathHelpers -- Win32 path construction, deletion, and drive selection.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_IO_WIN32_PATHHELPERS_C_H
#define NW_GROUP_IO_WIN32_PATHHELPERS_C_H

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
/*   BuildHlpSiblingPath                                        io/win32/BuildHlpSiblingPath.c */
/*   BuildTempPathForSourceFile                                 io/win32/BuildTempPathForSourceFile.c */
/*   DeleteExeVariantFileWithTempFallback                       io/win32/DeleteExeVariantFileWithTempFallback.c */
/*   DeleteFileAndCacheErrno                                    io/win32/DeleteFileAndCacheErrno.c */
/*   FindBestAvailableDriveAndEmitLabel                         io/win32/FindBestAvailableDriveAndEmitLabel.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void BuildHlpSiblingPath (const char *pszSrcPath);
extern void __stdcall BuildTempPathForSourceFile(char *pszSrcPath, LPSTR pszDestBuf);
extern int __stdcall DeleteExeVariantFileWithTempFallback(int nVariantNumber, LPCSTR pszSuffix, LPCSTR pszSourcePath);
extern int DeleteFileAndCacheErrno (LPCSTR pszFilename);
extern int FindBestAvailableDriveAndEmitLabel (unsigned char *pbDriveTypeTable, char *szLabelOut);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_IO_WIN32_PATHHELPERS_C_H */
