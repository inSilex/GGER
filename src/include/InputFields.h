/*
 * InputFields -- field-byte update and frame-interval input helpers.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_INPUT_INPUTFIELDS_C_H
#define NW_GROUP_INPUT_INPUTFIELDS_C_H

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
/*   AppendByteToShiftedField32                                 input/AppendByteToShiftedField32.c */
/*   InitializeBlankSpaceFilledField32                          input/InitializeBlankSpaceFilledField32.c */
/*   ProcessInputAdvanceFrameInterval                           input/ProcessInputAdvanceFrameInterval.c */
/*   ProcessTwoBytesEaseAndFireHookOnChange                     input/ProcessTwoBytesEaseAndFireHookOnChange.c */
/*   SetFrameIntervalFromDivisor                                input/SetFrameIntervalFromDivisor.c */
/*   StepByteTowardTarget                                       input/StepByteTowardTarget.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void __stdcall AppendByteToShiftedField32(uint8_t bNewByte);
extern void InitializeBlankSpaceFilledField32 (void);
extern BOOL __stdcall ProcessInputAdvanceFrameInterval(void);
extern void EaseTwoBytesAndFireHookOnChange (void);
extern BOOL __stdcall SetFrameIntervalFromDivisor(BYTE bDivisor);
extern int StepByteTowardTarget (uint8_t bTarget, uint8_t *pValue);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_INPUT_INPUTFIELDS_C_H */
