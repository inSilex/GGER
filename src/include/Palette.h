/*
 * Palette -- logical palette setup and grid palette rebuild.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_VIDEO_PALETTE_C_H
#define NW_GROUP_VIDEO_PALETTE_C_H

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
/*   ReadPaletteBlockAndSetLogicalPalette                       video/ReadPaletteBlockAndSetLogicalPalette.c */
/*   RebuildGridPaletteAndResetExtent                           video/RebuildGridPaletteAndResetExtent.c */
/*   RngPaletteHandlers                                         video/RngPaletteHandlers.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern BOOL ReadPaletteBlockAndSetLogicalPalette (void);
extern BOOL RebuildGridPaletteAndResetExtent (void);
extern uint32_t NwRngTablePickA (uint8_t *record);
extern uint32_t NwRngTablePickB (uint8_t *record);
extern uint32_t NwRngTablePickC (uint8_t *record);
extern uint32_t NwRngTablePickD (uint8_t *record);
extern uint32_t NwRngTablePickE (uint8_t *record);
extern uint32_t NwRngTablePickF (uint8_t *record);
extern uint32_t NwRngTablePickG (uint8_t *record);
extern uint32_t NwRngTablePickH (uint8_t *record);
extern uint32_t NwRngTablePickI (uint8_t *record);
extern uint32_t NwRngTablePickJ (uint8_t *record);
extern uint32_t NwRngTablePickK (uint8_t *record);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_VIDEO_PALETTE_C_H */
