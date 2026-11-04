/*
 * StreamRecords -- tagged record streams, compression, and seek/read/write helpers.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_IO_STREAMRECORDS_C_H
#define NW_GROUP_IO_STREAMRECORDS_C_H

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
/*   CheckIndexedStreamEntryAndReadRecordTag                    io/CheckIndexedStreamEntryAndReadRecordTag.c */
/*   CopyStreamRecordToBuffer                                   io/CopyStreamRecordToBuffer.c */
/*   DecodeTaggedRecordLengthToOutParam                         io/DecodeTaggedRecordLengthToOutParam.c */
/*   DecompressLzssBlockToBuffer                                io/DecompressLzssBlockToBuffer.c */
/*   GetStreamLogicalBytePosition                               io/GetStreamLogicalBytePosition.c */
/*   ReadBlockAndLatchPendingSlotHook                           io/ReadBlockAndLatchPendingSlotHook.c */
/*   ReadExactChunkFromStream                                   io/ReadExactChunkFromStream.c */
/*   ReadLzssOrRawTaggedRecordBlock                             io/ReadLzssOrRawTaggedRecordBlock.c */
/*   ReadStreamBlockRawOrLzss                                   io/ReadStreamBlockRawOrLzss.c */
/*   ReadStreamBytesToDestBuffer                                io/ReadStreamBytesToDestBuffer.c */
/*   ReadStreamChunkCountFromGlobalStream                       io/ReadStreamChunkCountFromGlobalStream.c */
/*   ReadStreamChunkFromDescriptorVerifyCount                   io/ReadStreamChunkFromDescriptorVerifyCount.c */
/*   ReadStreamHeaderRecordIntoGlobal                           io/ReadStreamHeaderRecordIntoGlobal.c */
/*   ReadTaggedRecordDeltaCountAndLatchStreamPosition           io/ReadTaggedRecordDeltaCountAndLatchStreamPosition.c */
/*   SeekStreamFileSlotToOffset                                 io/SeekStreamFileSlotToOffset.c */
/*   SeekStreamLogicalOffset                                    io/SeekStreamLogicalOffset.c */
/*   WriteStreamChunkCheckComplete                              io/WriteStreamChunkCheckComplete.c */
/*   WriteStreamChunkFromStackArgumentDword                     io/WriteStreamChunkFromStackArgumentDword.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern DWORD __stdcall CheckIndexedStreamEntryAndReadRecordTag(DWORD dwEntryIndex);
extern void *__stdcall CopyStreamRecordToBuffer(void *pDest);
extern void __fastcall DecodeTaggedRecordLengthToOutParam(DWORD dwUnusedArg1, DWORD dwUnusedArg2, DWORD *pdwLengthOut);
extern void DecompressLzssBlockToBuffer (int cbSrc, BYTE *pbDst, BYTE *pbSrc);
extern int GetStreamLogicalBytePosition (void);
extern BOOL ReadBlockAndLatchPendingSlotHook (void);
extern int __stdcall ReadExactChunkFromStream(DWORD cbRequestedBytes, char *lpBuffer);
extern DWORD __stdcall ReadLzssOrRawTaggedRecordBlock(BYTE *pbDestBuf);
extern int __stdcall ReadStreamBlockRawOrLzss(DWORD cbUnusedBlockSize, BYTE *pbDestBuf);
extern int __stdcall ReadStreamBytesToDestBuffer(DWORD dwRequestedBytes, char *lpDest);
extern int __stdcall ReadStreamChunkCountFromGlobalStream(DWORD cbRequestedBytes, char *lpBuffer);
extern int __stdcall ReadStreamChunkFromDescriptorVerifyCount(DWORD cbRequestedBytes, char *lpBuffer);
extern void ReadStreamHeaderRecordIntoGlobal (void);
extern void __stdcall ReadTaggedRecordDeltaCountAndLatchStreamPosition(int *pRecordEndPosition);
extern BOOL __stdcall SeekStreamFileSlotToOffset(DWORD dwMoveMethod, LONG lOffset);
extern BOOL __stdcall SeekStreamLogicalOffset(DWORD dwMoveMethod, int nLogicalOffset);
extern BOOL __stdcall WriteStreamChunkCheckComplete(DWORD cbCount, char *lpBuffer);
extern void __stdcall WriteStreamChunkFromStackArgumentDword(DWORD dwStackArgumentValue);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_IO_STREAMRECORDS_C_H */
