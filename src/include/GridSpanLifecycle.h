/*
 * GridSpanLifecycle -- grid span allocation, publication, and release.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_GRID_GRIDSPANLIFECYCLE_C_H
#define NW_GROUP_GRID_GRIDSPANLIFECYCLE_C_H

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
/*   AcquireGridCellSpanRecordAndRetirePrevious                 grid/AcquireGridCellSpanRecordAndRetirePrevious.c */
/*   AddGridCellSpanToExtentList                                grid/AddGridCellSpanToExtentList.c */
/*   AdvanceActiveGridCellSpanToOffset                          grid/AdvanceActiveGridCellSpanToOffset.c */
/*   AdvanceGridCellSubSpan                                     grid/AdvanceGridCellSubSpan.c */
/*   AdvancePendingGridCellSpanEntryAndDispatch                 grid/AdvancePendingGridCellSpanEntryAndDispatch.c */
/*   AllocGridCellSpanRecord                                    grid/AllocGridCellSpanRecord.c */
/*   AllocateGridCellSpanRecordAndDispatch                      grid/AllocateGridCellSpanRecordAndDispatch.c */
/*   ClearGridSpanChainBoundsAndHeapState                       grid/ClearGridSpanChainBoundsAndHeapState.c */
/*   InstallActiveGridCellRef                                   grid/InstallActiveGridCellRef.c */
/*   NotifyCellOperandHookAndAllocateGridSpanRecord             grid/NotifyCellOperandHookAndAllocateGridSpanRecord.c */
/*   PublishCursorSlotSpanAndFlagContextOnOperandMismatch       grid/PublishCursorSlotSpanAndFlagContextOnOperandMismatch.c */
/*   PublishGridCellSpanByDepthIndexToExtentList                grid/PublishGridCellSpanByDepthIndexToExtentList.c */
/*   ReleaseCurrentContextSpanGroup                             grid/ReleaseCurrentContextSpanGroup.c */
/*   ReleaseGridCellSpanRecord                                  grid/ReleaseGridCellSpanRecord.c */
/*   ReleaseGridCellSpanRecordsAndDispatchEntry                 grid/ReleaseGridCellSpanRecordsAndDispatchEntry.c */
/*   ReleaseSameDepthGridCellSpans                              grid/ReleaseSameDepthGridCellSpans.c */
/*   ReleaseSpanRecordByDepthIndex                              grid/ReleaseSpanRecordByDepthIndex.c */
/*   ReleaseTopGridCellSpans                                    grid/ReleaseTopGridCellSpans.c */
/*   ResolveGridReferenceAndAddCellSpan                         grid/ResolveGridReferenceAndAddCellSpan.c */
/*   RestartCurrentGridSpanGroup                                grid/RestartCurrentGridSpanGroup.c */
/*   SelectGridSpanRecordByPointAndPublishSelectionState        grid/SelectGridSpanRecordByPointAndPublishSelectionState.c */
/*   SetDefaultGridCellOffsetAndExtent                          grid/SetDefaultGridCellOffsetAndExtent.c */
/*   TestPointInsideGridSpanRecord                              grid/TestPointInsideGridSpanRecord.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint32_t __stdcall AcquireGridCellSpanRecordAndRetirePrevious(uint8_t bRefRelative, uint16_t wCellOffsetY, uint16_t wCellOffsetX, uint32_t nRefTabIndex);
extern BOOL __stdcall AdvanceActiveGridCellSpanToOffset(int nOffsetY, int nOffsetX);
extern void __stdcall AdvancePendingGridCellSpanEntryAndDispatch(unsigned char bRedrawFromPool, uint32_t dwStateIndex);
extern void __stdcall AllocateGridCellSpanRecordAndDispatch(int nCellMajor, int nCellMinor, uint32_t dwOpcode);
extern void ClearGridSpanChainBoundsAndHeapState (void);
extern BOOL __stdcall InstallActiveGridCellRef(BYTE bRefreshExtents, int nRecordIndex);
extern int __stdcall NotifyCellOperandHookAndAllocateGridSpanRecord(uint32_t dwCellOperandBlock);
extern void __stdcall PublishGridCellSpanByDepthIndexToExtentList(int nIndex);
extern unsigned int ReleaseCurrentContextSpanGroup (void);
extern void ReleaseGridCellSpanRecordsAndDispatchEntry (unsigned char bFullSweepMode, int nCellMajor, int nCellMinor);
extern void ReleaseSpanRecordByDepthIndex (int nDepthIndex);
extern void ReleaseTopGridCellSpans (int32_t nSpanCount);
extern unsigned char *__stdcall RestartCurrentGridSpanGroup(unsigned char *pInstrStream);
extern void __stdcall SelectGridSpanRecordByPointAndPublishSelectionState(uint8_t bAttrCode, uint8_t bCommand, int32_t nPointY1, int32_t nPointX1, int32_t nPointY2, int32_t nPointX2);
extern void __stdcall SetDefaultGridCellOffsetAndExtent(uint16_t wOffsetY, uint16_t wOffsetX);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_GRID_GRIDSPANLIFECYCLE_C_H */
