/*
 * StreamConversions -- Win32 stream line-ending and save-header conversions.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_IO_WIN32_STREAMCONVERSIONS_C_H
#define NW_GROUP_IO_WIN32_STREAMCONVERSIONS_C_H

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
/*   LoadAndVerifySaveFileHeaderChunk                           io/win32/LoadAndVerifySaveFileHeaderChunk.c */
/*   ReadStreamChunkCrLfNormalized                              io/win32/ReadStreamChunkCrLfNormalized.c */
/*   WriteStreamChunkLfToCrLfExpanded                           io/win32/WriteStreamChunkLfToCrLfExpanded.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern int __stdcall LoadAndVerifySaveFileHeaderChunk(const char *lpszSaveFileName);
extern int ReadStreamChunkCrLfNormalized (DWORD dwFileIndex, char *lpBuffer, DWORD cbBufferSize);
extern int WriteStreamChunkLfToCrLfExpanded (DWORD dwFileIndex, char *lpBuffer, DWORD cbBufferSize);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_IO_WIN32_STREAMCONVERSIONS_C_H */
