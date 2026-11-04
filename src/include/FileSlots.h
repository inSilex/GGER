/*
 * FileSlots -- tracked file slots and save-variant path selection.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_IO_FILESLOTS_C_H
#define NW_GROUP_IO_FILESLOTS_C_H

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
/*   BuildExeVariantNameFromBaseName                            io/BuildExeVariantNameFromBaseName.c */
/*   CallOpenTrackedHandleSlotWithFixedModeBits                 io/CallOpenTrackedHandleSlotWithFixedModeBits.c */
/*   CloseGlobalSlotAndResetToUnused                            io/CloseGlobalSlotAndResetToUnused.c */
/*   DeleteNetWalkSaveVariantByIndex                            io/DeleteNetWalkSaveVariantByIndex.c */
/*   FindFirstDriveTypeMatchAndEmitLetter                       io/FindFirstDriveTypeMatchAndEmitLetter.c */
/*   GetTrackedSlotFilePosition                                 io/GetTrackedSlotFilePosition.c */
/*   OpenTempPathOrExeVariantSourceFile                         io/OpenTempPathOrExeVariantSourceFile.c */
/*   OpenTrackedSlotBinaryAndStoreResult                        io/OpenTrackedSlotBinaryAndStoreResult.c */
/*   RebaseInlineImageRecord                                    io/RebaseInlineImageRecord.c */
/*   RewindGlobalSlotFileToStart                                io/RewindGlobalSlotFileToStart.c */
/*   SeekTrackedHandleAbsoluteOffset                            io/SeekTrackedHandleAbsoluteOffset.c */
/*   TruncateGlobalSlotFileToZeroLength                         io/TruncateGlobalSlotFileToZeroLength.c */
/*   UpdateCachedSlotStatus                                     io/UpdateCachedSlotStatus.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void __stdcall BuildExeVariantNameFromBaseName(int nVariantNumber, const char *pszSuffix, const char *pszSourcePath, char *szOutputName);
extern int CallOpenTrackedHandleSlotWithFixedModeBits (const char *pszFilename, DWORD dwAccessWord, DWORD dwAttrInput);
extern void CloseGlobalSlotAndResetToUnused (DWORD dwUnusedArg1, DWORD dwUnusedArg2);
extern void __stdcall DeleteNetWalkSaveVariantByIndex(int nVariantNumber);
extern int FindFirstDriveTypeMatchAndEmitLetter (char bDriveType, unsigned char *pbDriveTypeTable, char *szLabelOut);
extern DWORD GetTrackedSlotFilePosition (void);
extern int __stdcall OpenTempPathOrExeVariantSourceFile(int nVariantNumber, LPCSTR pszSuffix, LPCSTR pszSourcePath);
extern int __stdcall OpenTrackedSlotBinaryAndStoreResult(const char *pszFilename);
extern uint8_t *RebaseInlineImageRecordAndLoadCursor (void);
extern DWORD RewindGlobalSlotFileToStart (void);
extern BOOL __stdcall SeekTrackedHandleAbsoluteOffset(LONG lDistanceToMove);
extern int TruncateGlobalSlotFileToZeroLength (void);
extern int UpdateCachedSlotStatus (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_IO_FILESLOTS_C_H */
