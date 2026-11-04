/*
 * Heap -- heap arenas, blocks, bounds, and memory fill/copy.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_RUNTIME_HEAP_C_H
#define NW_GROUP_RUNTIME_HEAP_C_H

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
/*   AllocHeapBlock                                             runtime/AllocHeapBlock.c */
/*   AllocateHeapCursorBlock                                    runtime/AllocateHeapCursorBlock.c */
/*   FillMemoryWithByte                                         runtime/FillMemoryWithByte.c */
/*   FillMemoryWithDword                                        runtime/FillMemoryWithDword.c */
/*   FillMemoryWithDwordFromStackArgs                           runtime/FillMemoryWithDwordFromStackArgs.c */
/*   FillMemoryWithZeroBytes                                    runtime/FillMemoryWithZeroBytes.c */
/*   FreeArenaSlot                                              runtime/FreeArenaSlot.c */
/*   FreeGlobalHeapBlock                                        runtime/FreeGlobalHeapBlock.c */
/*   FreeHeapBlock                                              runtime/FreeHeapBlock.c */
/*   FreeHeapBlockIfNotNull                                     runtime/FreeHeapBlockIfNotNull.c */
/*   GetHeapArenaBlockEnd                                       runtime/GetHeapArenaBlockEnd.c */
/*   InitializeGlobalHeapBuffer                                 runtime/InitializeGlobalHeapBuffer.c */
/*   InitializeGlobalThreadBufferOrDie                          runtime/InitializeGlobalThreadBufferOrDie.c */
/*   InstallInitialHeapRegionBounds                             runtime/InstallInitialHeapRegionBounds.c */
/*   MoveMemoryRegion                                           runtime/MoveMemoryRegion.c */
/*   PublishHeapStateTripleFromTwoSources                       runtime/PublishHeapStateTripleFromTwoSources.c */
/*   SetHeapRegionBounds                                        runtime/SetHeapRegionBounds.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern uint32_t *__stdcall AllocHeapBlock(uint32_t dwSizeBytes);
extern uint32_t AllocateHeapCursorBlock (uint32_t dwSizeBytes);
extern void *FillMemoryWithByte (void *pDst, unsigned char bFill, unsigned int dwCount);
extern void FillMemoryWithDword (unsigned int dwCount, unsigned int dwFill, void *pDst);
extern void *FillMemoryWithDwordFromStackArgs (void *pDst, unsigned int dwFill, unsigned int dwCount);
extern unsigned char *__stdcall FillMemoryWithZeroBytes(unsigned int dwCount, unsigned char *pDst);
extern void FreeArenaSlot (uint8_t *pSlotToFree);
extern void FreeGlobalHeapBlock (void);
extern void __stdcall FreeHeapBlock(uint8_t *pData);
extern void FreeHeapBlockIfNotNull (byte *pData);
extern void *__stdcall GetHeapArenaBlockEnd(uint32_t dwRequestSize);
extern void InitializeGlobalHeapBuffer (void);
extern void *InitializeGlobalThreadBufferOrDie (void);
extern void InstallInitialHeapRegionBounds (void);
extern void *MoveMemoryRegion (void *pDest, const void *pSrc, uint32_t dwByteCount);
extern uint32_t PublishHeapStateTripleFromTwoSources (void);
extern void SetHeapRegionBounds (int32_t pHeapRegionEnd);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_RUNTIME_HEAP_C_H */
