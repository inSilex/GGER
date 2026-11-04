/*
 * Pool -- pooled block allocation and string duplication.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_RUNTIME_POOL_C_H
#define NW_GROUP_RUNTIME_POOL_C_H

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
/*   AllocateAndZeroPooledBlockArray                            runtime/AllocateAndZeroPooledBlockArray.c */
/*   AllocatePooledBlock                                        runtime/AllocatePooledBlock.c */
/*   DuplicateCStringIntoPooledBlock                            runtime/DuplicateCStringIntoPooledBlock.c */
/*   EnsurePooledBlockCapacity                                  runtime/EnsurePooledBlockCapacity.c */
/*   GetPooledBlockUsableSize                                   runtime/GetPooledBlockUsableSize.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void *AllocateAndZeroPooledBlockArray (uint32_t element_count, uint32_t element_size);
extern void *AllocatePooledBlock (uint32_t size);
extern char *DuplicateCStringIntoPooledBlock (const char *pszSrc);
extern void *EnsurePooledBlockCapacity (void *block, unsigned int min_bytes);
extern uint32_t GetPooledBlockUsableSize (void *block);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_RUNTIME_POOL_C_H */
