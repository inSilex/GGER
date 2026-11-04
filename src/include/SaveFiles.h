/*
 * SaveFiles -- save-file headers, regions, and load/save orchestration.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_IO_SAVEFILES_C_H
#define NW_GROUP_IO_SAVEFILES_C_H

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
/*   ClearBufferRegionsFromDescriptor                           io/ClearBufferRegionsFromDescriptor.c */
/*   CloseSaveFileSlot                                          io/CloseSaveFileSlot.c */
/*   CloseSaveSlotAndLatchInvalidDataError                      io/CloseSaveSlotAndLatchInvalidDataError.c */
/*   ForwardCloseSaveFileSlot                                   io/ForwardCloseSaveFileSlot.c */
/*   InitializeDispatchRecords                                  io/InitializeDispatchRecords.c */
/*   LoadDecryptRegionsFromDescriptor                           io/LoadDecryptRegionsFromDescriptor.c */
/*   LoadOrRepairSaveFileRegions                                io/LoadOrRepairSaveFileRegions.c */
/*   LoadRecordBlocksIntoTileCache                              io/LoadRecordBlocksIntoTileCache.c */
/*   WriteBufferRegionsScrambledToStream                        io/WriteBufferRegionsScrambledToStream.c */
/*   WriteSaveFileBuffersAndLoadTileCache                       io/WriteSaveFileBuffersAndLoadTileCache.c */
/*   WriteSaveFileHeaderAndBufferRegions                        io/WriteSaveFileHeaderAndBufferRegions.c */
/*   WriteSaveFileHeaderAndScrambledRegions                     io/WriteSaveFileHeaderAndScrambledRegions.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void __fastcall CloseSaveFileSlot(DWORD dwUnusedArg1, DWORD dwUnusedArg2);
extern DWORD __fastcall CloseSaveSlotAndLatchInvalidDataError(DWORD dwUnusedArg1, DWORD dwUnusedArg2);
extern void ForwardCloseSaveFileSlot (void);
extern int __stdcall InitializeDispatchRecords(uint32_t nVariantNumber);
extern int __stdcall LoadOrRepairSaveFileRegions(int32_t nVariantNumber);
extern uint32_t __stdcall LoadRecordBlocksIntoTileCache(uint8_t flagA, uint32_t recordIndex);
extern int __stdcall WriteSaveFileBuffersAndLoadTileCache(uint32_t nVariantNumber);
extern void WriteSaveFileHeaderAndBufferRegions (void);
extern int __stdcall WriteSaveFileHeaderAndScrambledRegions(int32_t nVariantNumber);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_IO_SAVEFILES_C_H */
