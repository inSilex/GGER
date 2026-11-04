/*
 * GridCore -- grid-delta application and frame composition.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_GRID_GRIDCORE_C_H
#define NW_GROUP_GRID_GRIDCORE_C_H

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
/*   ApplyGridDeltaPlain                                        grid/ApplyGridDeltaPlain.c */
/*   ApplyGridDeltaSignedExt                                    grid/ApplyGridDeltaSignedExt.c */
/*   ApplyGridDeltaSignedLoop                                   grid/ApplyGridDeltaSignedLoop.c */
/*   ApplyGridDeltaZeroSkip                                     grid/ApplyGridDeltaZeroSkip.c */
/*   ComposeFrameCellsFromStreamRecords                         grid/ComposeFrameCellsFromStreamRecords.c */
/*   ComputeCellAddressAndDispatchGridDelta                     grid/ComputeCellAddressAndDispatchGridDelta.c */
/*   WrapGridAxesAndDispatch                                    grid/WrapGridAxesAndDispatch.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint8_t *ApplyGridDeltaPlain (uint8_t *pStream, uint32_t rowCount, uint32_t colCount, uint8_t *pDst, uint32_t rewind);
extern uint8_t *ApplyGridDeltaSignedExt (uint8_t *pStream, uint32_t rowCount, uint32_t colCount, uint8_t *pDst, uint32_t rewind);
extern uint8_t *ApplyGridDeltaSignedLoop (uint8_t *pStream, uint32_t rowCount, uint32_t colCount, uint8_t *pDst, uint32_t rewind);
extern uint8_t *ApplyGridDeltaZeroSkip (uint8_t *pStream, uint32_t rowCount, uint32_t colCount, uint8_t *pDst, uint32_t rewind);
extern uint32_t ComposeFrameCellsFromStreamRecords (void);
extern uint32_t __stdcall ComputeCellAddressAndDispatchGridDelta(uint32_t rowCount, uint32_t colCount, uint32_t row, uint32_t column, uint32_t decoderIndex, uint8_t *pStream);
extern int __stdcall WrapGridAxesAndDispatch(uint32_t dwCellOperandBlock);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_GRID_GRIDCORE_C_H */
