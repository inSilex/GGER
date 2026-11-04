/*
 * TextRender -- text row rendering into the display buffer.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_VIDEO_TEXTRENDER_C_H
#define NW_GROUP_VIDEO_TEXTRENDER_C_H

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
/*   CopyTextRowsForwardSkipNulToDisplayBuffer                  video/CopyTextRowsForwardSkipNulToDisplayBuffer.c */
/*   CopyTextRowsReverseSkipNulToDisplayBuffer                  video/CopyTextRowsReverseSkipNulToDisplayBuffer.c */
/*   CopyTextRowsToDisplayBuffer                                video/CopyTextRowsToDisplayBuffer.c */
/*   CopyTextRowsUnrolledSkipNulToDisplayBuffer                 video/CopyTextRowsUnrolledSkipNulToDisplayBuffer.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void __stdcall CopyTextRowsForwardSkipNulToDisplayBuffer(int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol, int nSrcRowLimit, int nSrcStride, int nNegSrcFirstRow, int nSrcFirstCol, int nSrcBase);
extern void __stdcall CopyTextRowsReverseSkipNulToDisplayBuffer(int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol, int nSrcRowLimit, int nSrcStride, int nSrcFirstRow, int nSrcFirstCol, int nSrcBase);
extern void __stdcall CopyTextRowsToDisplayBuffer(int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol, int nSrcRowLimit, int nSrcStride, int nSrcFirstRow, int nSrcFirstCol, int nSrcBase);
extern void __stdcall CopyTextRowsUnrolledSkipNulToDisplayBuffer(int nDstRowLimit, int nDstColLimit, int nDstFirstRow, int nDstFirstCol, int nSrcRowLimit, int nSrcStride, int nSrcFirstRow, int nSrcFirstCol, int nSrcBase);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_VIDEO_TEXTRENDER_C_H */
