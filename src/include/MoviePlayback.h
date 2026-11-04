/*
 * MoviePlayback -- AVI/BMP movie discovery and startup playback.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_VIDEO_WIN32_MOVIEPLAYBACK_C_H
#define NW_GROUP_VIDEO_WIN32_MOVIEPLAYBACK_C_H

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
/*   BuildAviBmpPathsFromSourcePath                             video/win32/BuildAviBmpPathsFromSourcePath.c */
/*   FindMovieFileAcrossDrivesAndEmitBmpAviPaths                video/win32/FindMovieFileAcrossDrivesAndEmitBmpAviPaths.c */
/*   PlayStartupMovieWithMessagePumpAndTeardown                 video/win32/PlayStartupMovieWithMessagePumpAndTeardown.c */
/*   PlayVideoTileMovieAndLoadBmp                               video/win32/PlayVideoTileMovieAndLoadBmp.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern int BuildAviBmpPathsFromSourcePath (char *pszSrcPath, char *pszBmpPathOut, char *pszAviPathOut);
extern int __stdcall FindMovieFileAcrossDrivesAndEmitBmpAviPaths(const char *pszName, char *pszBmpPathOut, char *pszAviPathOut);
extern int __stdcall PlayStartupMovieWithMessagePumpAndTeardown(DWORD dwMovieRectWord3, DWORD dwMovieRectWord2, DWORD dwMovieWindowY, DWORD dwMovieWindowX, const char *pszMovieStem);
extern void PlayVideoTileMovieAndLoadBmp (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_VIDEO_WIN32_MOVIEPLAYBACK_C_H */
