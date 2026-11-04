/*
 * Blit -- sprite, row, and region blitting.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_VIDEO_BLIT_C_H
#define NW_GROUP_VIDEO_BLIT_C_H

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
/*   AddRectToExtentList                                        video/AddRectToExtentList.c */
/*   AddTranslatedRectToExtentList                              video/AddTranslatedRectToExtentList.c */
/*   BlitPlayerRegionOrTileGrid                                 video/BlitPlayerRegionOrTileGrid.c */
/*   BlitRectOrTileGridToPlayerSurface                          video/BlitRectOrTileGridToPlayerSurface.c */
/*   CopyRowsBulkWithTailToDisplayBuffer                        video/CopyRowsBulkWithTailToDisplayBuffer.c */
/*   CopySpriteRowsToDisplayBuffer                              video/CopySpriteRowsToDisplayBuffer.c */
/*   FillActiveSurfaceWithReplicatedIndexDword                  video/FillActiveSurfaceWithReplicatedIndexDword.c */
/*   FlushGdiDrawingBatch                                       video/FlushGdiDrawingBatch.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void AddRectToExtentList (int nMaxY, int nMaxX, int nMinY, int nMinX);
extern void __stdcall AddTranslatedRectToExtentList(int nMaxY, int nMaxX, int nMinY, int nMinX);
extern BOOL __stdcall BlitPlayerRegionOrTileGrid(unsigned char bTileGridMode, int nSourceHeight, unsigned int nReserved, int nRegionHeight, int nRegionWidth, int nRegionY, int nRegionX, void *pSrcBits);
extern void __stdcall BlitRectOrTileGridToPlayerSurface(char cTileGridMode, int nRectBottom, int nRectRight, int nRectTop, int nRectLeft);
extern void __stdcall CopyRowsBulkWithTailToDisplayBuffer(int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol, int nSrcRowLimit, int nSrcStride, int nSrcFirstRow, int nSrcFirstCol, int nSrcBase);
extern int CopySpriteRowsToDisplayBuffer (void);
/* Queue and consume one full-frame extent.  Used when an asset-source switch
 * must be visible before the game naturally repaints the static screen. */
extern int NwBlitRedrawFullFrame (void);
extern void __stdcall FillActiveSurfaceWithReplicatedIndexDword(BYTE bIndex);
extern void FlushGdiDrawingBatch (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_VIDEO_BLIT_C_H */
