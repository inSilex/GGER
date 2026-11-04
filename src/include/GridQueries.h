/*
 * GridQueries -- grid context queries and descriptor lookup.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_GRID_GRIDQUERIES_C_H
#define NW_GROUP_GRID_GRIDQUERIES_C_H

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
/*   FindPredecessorDescriptorPointerByBackwardScan             grid/FindPredecessorDescriptorPointerByBackwardScan.c */
/*   GridContextQueries                                         grid/GridContextQueries.c */
/*   GridStateHandlers                                          grid/GridStateHandlers.c */
/*   StampGridContextLayerAndPublishDepthSlot                   grid/StampGridContextLayerAndPublishDepthSlot.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint8_t *__stdcall FindPredecessorDescriptorPointerByBackwardScan(const uint8_t *pKeyRecord);
extern uint32_t NwGridCtxYIsNone (void);
extern uint32_t NwGridCtxLayerHasMask90_10 (void);
extern uint32_t NwGridCtxLayerHasMaskA0_20 (void);
extern uint32_t __stdcall NwGridCtxLayerMaskB0Equals(uint32_t value);
extern uint32_t __stdcall NwGridCtxLayerMask4FEquals(uint32_t value);
extern uint32_t __stdcall NwGridCtxLayerEquals(uint32_t value);
extern uint32_t NwGridCtxPendingIsNone (void);
extern uint32_t __stdcall NwGridCtxPendingEquals(uint32_t value);
extern uint32_t NwGridCtxLayerFromRowCount (void);
extern uint32_t __stdcall NwGridCtxReadLayer(uint8_t *out);
extern uint32_t __stdcall NwReachedStepCodeEquals(uint32_t value);
extern uint32_t __stdcall NwReachedBearingCodeEquals(uint32_t value);
extern uint32_t __stdcall NwReachedStepCodeMatches(uint32_t value);
extern uint32_t __stdcall NwReachedBearingCodeMatches(uint32_t value);
extern uint32_t __stdcall NwReachedCellCommand(uint32_t mode);
extern uint32_t __stdcall NwReachedCellAllocFromArg(uint32_t arg);
extern uint32_t __stdcall NwReachedCellAllocFromArgOrOne(uint32_t arg);
extern uint32_t __stdcall NwGridCtxFlagsMatch(uint32_t mask);
extern char *__stdcall NwCopyStringSwapArgs(uint32_t arg1, uint32_t arg2);
extern unsigned int __stdcall StampGridContextLayerAndPublishDepthSlot(uint32_t dwStampByte);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_GRID_GRIDQUERIES_C_H */
