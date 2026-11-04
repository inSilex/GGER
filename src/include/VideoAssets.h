/*
 * VideoAssets -- sprite/tile loading and screen geometry.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_VIDEO_VIDEOASSETS_C_H
#define NW_GROUP_VIDEO_VIDEOASSETS_C_H

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
/*   FlushPendingSurfaceRectsAndReseedRandom                    video/FlushPendingSurfaceRectsAndReseedRandom.c */
/*   LoadDiskSpriteImageFromStream                              video/LoadDiskSpriteImageFromStream.c */
/*   LoadNetwalkTileImageFile                                   video/LoadNetwalkTileImageFile.c */
/*   PublishScreenGeometryAndClaimFrameBuffer                   video/PublishScreenGeometryAndClaimFrameBuffer.c */
/*   TryPlayStartupMovieWithWindowFallback                      video/TryPlayStartupMovieWithWindowFallback.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void FlushPendingSurfaceRectsAndReseedRandom (char bDrawPending);
extern BYTE *__stdcall LoadDiskSpriteImageFromStream(WORD *pwSpriteRecord);
extern void __stdcall LoadNetwalkTileImageFile(const char *pszFilename);
extern BOOL TryPlayStartupMovieWithWindowFallback (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_VIDEO_VIDEOASSETS_C_H */
