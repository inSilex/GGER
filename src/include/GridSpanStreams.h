/*
 * GridSpanStreams -- grid span command streams and arena setup.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_GRID_GRIDSPANSTREAMS_C_H
#define NW_GROUP_GRID_GRIDSPANSTREAMS_C_H

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
/*   DecodeGridSpanIndexStream                                  grid/DecodeGridSpanIndexStream.c */
/*   GridSpanEntryActions                                       grid/GridSpanEntryActions.c */
/*   GridSpanStreamWrappers                                     grid/GridSpanStreamWrappers.c */
/*   InitializeAxisSpanFromSavedPoint                           grid/InitializeAxisSpanFromSavedPoint.c */
/*   InitializeGridArenaFromMapHeader                           grid/InitializeGridArenaFromMapHeader.c */
/*   InitializeGridSpanCommandStreamRun                         grid/InitializeGridSpanCommandStreamRun.c */
/*   ProcessGridSpanCommandStream                               grid/ProcessGridSpanCommandStream.c */
/*   ProcessPendingGridCellSpanEntry                            grid/ProcessPendingGridCellSpanEntry.c */
/*   RebuildGridHeapAndCursorStateFromStream                    grid/RebuildGridHeapAndCursorStateFromStream.c */
/*   ResetGridSpanGeometryIfModeFlagClear                       grid/ResetGridSpanGeometryIfModeFlagClear.c */
/*   ResetTileRecordStateAndSeekStream                          grid/ResetTileRecordStateAndSeekStream.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern unsigned int __stdcall DecodeGridSpanIndexStream(short unsigned int wOffsetY, int nCellOffsetX, int nGridTabIndex, unsigned char *pInstrStream);
extern uint32_t NwProcessPendingSpanEntry (void);
extern uint32_t __stdcall NwAdvancePendingSpanEntryKeepIndex(uint32_t dwStateIndex);
extern uint32_t __stdcall NwAdvancePendingSpanEntryRedrawIndex(uint32_t dwStateIndex);
extern uint32_t NwReleaseContextSpanByYIndex (void);
extern uint32_t __stdcall NwInstallActiveCellRefWithRefresh(uint32_t nRecordIndex);
extern uint32_t NwInstallActiveCellRefZero (void);
extern uint32_t __stdcall NwCellPackedBitForArg(uint32_t index);
extern uint32_t __stdcall NwAcquireScaledSpanAndArm(uint32_t index);
extern uint32_t __stdcall NwAcquireSpansAndRunStream(uint32_t index);
extern uint32_t __stdcall NwInitSpanRunScaled(const uint8_t *pInstrStream, uint32_t nGridTabIndex);
extern uint32_t __stdcall NwInitSpanRunFromTable(const uint8_t *pInstrStream, uint32_t index);
extern uint32_t __stdcall NwDecodeSpanRunScaled(const uint8_t *pInstrStream, uint32_t nGridTabIndex);
extern uint32_t __stdcall NwDecodeSpanRunFromTable(const uint8_t *pInstrStream, uint32_t index);
extern void __stdcall InitializeAxisSpanFromSavedPoint(char cSelectCode);
extern void InitializeGridArenaFromMapHeader (void);
extern unsigned int __stdcall InitializeGridSpanCommandStreamRun(int nCellOffsetY, int nGridCellOffsetX, int nGridTabIndex, unsigned char *pInstrStream);
extern unsigned int __stdcall ProcessGridSpanCommandStream(unsigned char bFirstCommand);
extern void ProcessPendingGridCellSpanEntry (void);
extern dword RebuildGridHeapAndCursorStateFromStream (void);
extern void ResetGridSpanGeometryIfModeFlagClear (void);
extern BOOL __stdcall ResetTileRecordStateAndSeekStream(LONG lRecordStreamOffset);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_GRID_GRIDSPANSTREAMS_C_H */
