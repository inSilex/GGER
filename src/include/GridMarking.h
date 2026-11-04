/*
 * GridMarking -- grid marking, path routing, and cursor wrapping.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_GRID_GRIDMARKING_C_H
#define NW_GROUP_GRID_GRIDMARKING_C_H

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
/*   AnimateScrollTowardSelectedGridCell                        grid/AnimateScrollTowardSelectedGridCell.c */
/*   ApplyDeltaToUnmarkedEntries                                grid/ApplyDeltaToUnmarkedEntries.c */
/*   ClassifyAxisSpanOrientation                                grid/ClassifyAxisSpanOrientation.c */
/*   ComputeScaledGridOffset                                    grid/ComputeScaledGridOffset.c */
/*   MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo       grid/MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo.c */
/*   MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo        grid/MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo.c */
/*   MarkZeroCellsAdjacentToSixCellsAndRecordTwo                grid/MarkZeroCellsAdjacentToSixCellsAndRecordTwo.c */
/*   RouteGridPathToTargetCell                                  grid/RouteGridPathToTargetCell.c */
/*   SequenceGridMarkingSweepsUntilMatchAndDispatch             grid/SequenceGridMarkingSweepsUntilMatchAndDispatch.c */
/*   WrapGridAxisCursorsStampCellKeyAndRetireSpans              grid/WrapGridAxisCursorsStampCellKeyAndRetireSpans.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern int32_t AnimateScrollTowardSelectedGridCell (void);
extern void __stdcall ApplyDeltaToUnmarkedEntries(int nDelta, int nEntryCount, BYTE *pEntries);
extern int ClassifyAxisSpanOrientation (void);
extern unsigned char MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo (void);
extern unsigned char MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo (void);
extern BYTE MarkZeroCellsAdjacentToSixCellsAndRecordTwo (void);
extern int __stdcall RouteGridPathToTargetCell(BYTE bCellMatch);
extern int __stdcall SequenceGridMarkingSweepsUntilMatchAndDispatch(BYTE bMode);
extern void __stdcall WrapGridAxisCursorsStampCellKeyAndRetireSpans(uint32_t dwAxisDeltaAndMode);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_GRID_GRIDMARKING_C_H */
