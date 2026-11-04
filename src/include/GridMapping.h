/*
 * GridMapping -- command-to-grid tile/cell mapping helpers.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_GRID_GRIDMAPPING_C_H
#define NW_GROUP_GRID_GRIDMAPPING_C_H

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
/*   DispatchOneBasedCommandToGridRenderMapper                  grid/DispatchOneBasedCommandToGridRenderMapper.c */
/*   DispatchOneBasedCommandToSentinelGroupMapper               grid/DispatchOneBasedCommandToSentinelGroupMapper.c */
/*   DispatchThreeToElevenCommandToSentinelGroupMapper          grid/DispatchThreeToElevenCommandToSentinelGroupMapper.c */
/*   ExpandCellTileIndexBitmapBitsToGridBytes                   grid/ExpandCellTileIndexBitmapBitsToGridBytes.c */
/*   IsRemappedIndexArgEqual                                    grid/IsRemappedIndexArgEqual.c */
/*   MapGridCellsAgainstCallerSentinelToDisplayByte             grid/MapGridCellsAgainstCallerSentinelToDisplayByte.c */
/*   MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero      grid/MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero.c */
/*   MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree     grid/MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree.c */
/*   MapGridTilesViaBitmapToDisplayGrid                         grid/MapGridTilesViaBitmapToDisplayGrid.c */
/*   MapGridTilesViaSentinelFeToDisplayGrid                     grid/MapGridTilesViaSentinelFeToDisplayGrid.c */
/*   MapTileBitmapBitToGridZeroOrThree                          grid/MapTileBitmapBitToGridZeroOrThree.c */
/*   SelectAndDispatchGridRenderMappingByCommand                grid/SelectAndDispatchGridRenderMappingByCommand.c */
/*   SelectAndDispatchGridRenderMappingBySentinelGroup          grid/SelectAndDispatchGridRenderMappingBySentinelGroup.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern int __stdcall DispatchOneBasedCommandToGridRenderMapper(uint32_t nCommand, uint32_t dwRecordIndex);
extern int __stdcall DispatchOneBasedCommandToSentinelGroupMapper(uint32_t nCommand, uint32_t dwSentinel);
extern int __stdcall DispatchThreeToElevenCommandToSentinelGroupMapper(uint32_t nCommand);
extern int __stdcall ExpandCellTileIndexBitmapBitsToGridBytes(const BYTE *pBitmap);
extern uint32_t __stdcall IsRemappedIndexArgEqual(uint32_t dwIndexArg, uint32_t dwValueArg);
extern int __stdcall MapGridCellsAgainstCallerSentinelToDisplayByte(BYTE bSentinel);
extern int __stdcall MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero(BYTE bSentinel);
extern int __stdcall MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree(BYTE bSentinel);
extern void __stdcall MapGridTilesViaBitmapToDisplayGrid(BYTE *pTileBitmap);
extern void MapGridTilesViaSentinelFeToDisplayGrid (void);
extern int __stdcall MapTileBitmapBitToGridZeroOrThree(const BYTE *pTileBitmap);
extern int __stdcall SelectAndDispatchGridRenderMappingByCommand(BYTE bMapperSelect, BYTE bRecordIndex, BYTE bCommand);
extern int __stdcall SelectAndDispatchGridRenderMappingBySentinelGroup(BYTE bMapperSelector, BYTE bSentinelByte, BYTE bCommand);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_GRID_GRIDMAPPING_C_H */
