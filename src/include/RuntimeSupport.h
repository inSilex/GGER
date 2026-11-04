/*
 * RuntimeSupport -- token arrays, block relinking, and format scanning.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_RUNTIME_RUNTIMESUPPORT_C_H
#define NW_GROUP_RUNTIME_RUNTIMESUPPORT_C_H

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
/*   BuildTokenPointerArrayFromGlobalString                     runtime/BuildTokenPointerArrayFromGlobalString.c */
/*   RelinkBlockHeaderNode                                      runtime/RelinkBlockHeaderNode.c */
/*   RelinkListNodeUnderSlotLock                                runtime/RelinkListNodeUnderSlotLock.c */
/*   RelinkMaskedBlockHeaderNodes                               runtime/RelinkMaskedBlockHeaderNodes.c */
/*   ScanFourDecimalFieldsToByteOrWordGlobals                   runtime/ScanFourDecimalFieldsToByteOrWordGlobals.c */
/*   ScanStringFormatToVarArgs                                  runtime/ScanStringFormatToVarArgs.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern int BuildTokenPointerArrayFromGlobalString (void);
extern void RelinkBlockHeaderNode (uintptr_t block);
extern void RelinkListNodeUnderSlotLock (void *pUnlinkSlot);
extern void RelinkMaskedBlockHeaderNodes (void);
extern int __stdcall ScanFourDecimalFieldsToByteOrWordGlobals(DWORD dwPublishMode, char *pszInput);
extern int ScanStringFormatToVarArgs (char *pszInput, char *pszFormat, ...);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_RUNTIME_RUNTIMESUPPORT_C_H */
