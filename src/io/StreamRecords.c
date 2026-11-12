#include "nwarch.h"
/*
 * StreamRecords -- tagged record streams, compression, and seek/read/write helpers.
 *
 * Module: io
 * Interface: include/StreamRecords.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   CheckIndexedStreamEntryAndReadRecordTag                    io/CheckIndexedStreamEntryAndReadRecordTag.c
 *   CopyStreamRecordToBuffer                                   io/CopyStreamRecordToBuffer.c
 *   DecodeTaggedRecordLengthToOutParam                         io/DecodeTaggedRecordLengthToOutParam.c
 *   DecompressLzssBlockToBuffer                                io/DecompressLzssBlockToBuffer.c
 *   GetStreamLogicalBytePosition                               io/GetStreamLogicalBytePosition.c
 *   ReadBlockAndLatchPendingSlotHook                           io/ReadBlockAndLatchPendingSlotHook.c
 *   ReadExactChunkFromStream                                   io/ReadExactChunkFromStream.c
 *   ReadLzssOrRawTaggedRecordBlock                             io/ReadLzssOrRawTaggedRecordBlock.c
 *   ReadStreamBlockRawOrLzss                                   io/ReadStreamBlockRawOrLzss.c
 *   ReadStreamBytesToDestBuffer                                io/ReadStreamBytesToDestBuffer.c
 *   ReadStreamChunkCountFromGlobalStream                       io/ReadStreamChunkCountFromGlobalStream.c
 *   ReadStreamChunkFromDescriptorVerifyCount                   io/ReadStreamChunkFromDescriptorVerifyCount.c
 *   ReadStreamHeaderRecordIntoGlobal                           io/ReadStreamHeaderRecordIntoGlobal.c
 *   ReadTaggedRecordDeltaCountAndLatchStreamPosition           io/ReadTaggedRecordDeltaCountAndLatchStreamPosition.c
 *   SeekStreamFileSlotToOffset                                 io/SeekStreamFileSlotToOffset.c
 *   SeekStreamLogicalOffset                                    io/SeekStreamLogicalOffset.c
 *   WriteStreamChunkCheckComplete                              io/WriteStreamChunkCheckComplete.c
 *   WriteStreamChunkFromStackArgumentDword                     io/WriteStreamChunkFromStackArgumentDword.c
 */

/* ==========================================================================
 * Unit: CheckIndexedStreamEntryAndReadRecordTag
 * Source: io/CheckIndexedStreamEntryAndReadRecordTag.c
 * ========================================================================== */
#ifdef NW_UNIT_CheckIndexedStreamEntryAndReadRecordTag
/*
 * Check Indexed Stream Entry And Read Record Tag.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-checkindexedstreamentryandreadrecordtag-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

#define STREAM_ENTRY_COUNT        (*(DWORD    *)(nw_data_window + 0x16c4))
#define STREAM_INDEX_TABLE_BASE   ((DWORD)(uintptr_t)NW_WINDOW_PTR_REF(uint8_t, 0x16c0))
#define STREAM_DATA_BASE          (*(DWORD    *)(nw_data_window + 0x16bc))

/* @(nw_data_window + 0x16d0) -- the global slot-descriptor byte. This function NEVER reads or writes it
 * directly; the tag byte it needs is fetched for it by FetchNextStreamByteIntoSlotDescriptor,
 * which owns that global (see src/vm/FetchNextStreamByteIntoSlotDescriptor.c). Declared here only
 * to document that this function's last action leaves that byte live for its callers. */
#define SLOT_DESCRIPTOR_BYTE      (*(BYTE *)(nw_data_window + 0x16d0))

extern BOOL __stdcall SeekStreamLogicalOffset(DWORD dwMoveMethod, int nLogicalOffset);

extern int __stdcall ReadStreamBytesToDestBuffer(DWORD dwRequestedBytes, char *lpDest);

extern int __cdecl FetchNextStreamByteIntoSlotDescriptor(void);

extern DWORD __fastcall CloseSaveSlotAndLatchInvalidDataError(DWORD dwUnusedArg1,
                                                               DWORD dwUnusedArg2);

#define SEEK_BEGIN 0
#define SEEK_END   2

/* The 5-byte index entry exactly as the body lays it out on the stack: four little-endian
 * offset bytes at +0 and the tag byte at +4. Kept as a byte array (no struct padding, so the
 * +4 tag really is the FIFTH byte) and decoded with an explicit little-endian read. */
typedef struct {
    unsigned char abEntryRecord[5];
} STREAM_INDEX_ENTRY;

/* Optional trace. */
extern void nwsafe_trace(const char *);
extern void nwtrace_hex(const char *, unsigned long);

DWORD __stdcall CheckIndexedStreamEntryAndReadRecordTag(DWORD dwEntryIndex)
{
#ifdef NWDEBUG_ENABLED
    nwsafe_trace("citrt_in");
    nwtrace_hex("citrt_count", STREAM_ENTRY_COUNT);
    nwtrace_hex("citrt_tbase", STREAM_INDEX_TABLE_BASE);
    nwtrace_hex("citrt_dbase", STREAM_DATA_BASE);
#endif
    STREAM_INDEX_ENTRY sEntry;
    unsigned long      ulEntriesAhead;
    unsigned long      ulSeekDistance;
    unsigned long      ulPayloadOffset;

    /* Unsigned bounds check: reject only an index strictly past the last entry. */
    if (dwEntryIndex > STREAM_ENTRY_COUNT) {
        goto Error;
    }

    /* Locate this entry relative to the END of the file: 5-byte stride, counted backward
     * from the entry count, plus the fixed table base and one stride past the entry. */
    ulEntriesAhead = (unsigned long)STREAM_ENTRY_COUNT - (unsigned long)dwEntryIndex;
    ulSeekDistance = ulEntriesAhead * 5UL
                   + (unsigned long)STREAM_INDEX_TABLE_BASE
                   + 5UL;
#ifdef NWDEBUG_ENABLED
    nwsafe_trace("citrt_bseek");
    nwtrace_hex("citrt_seekdist", ulSeekDistance);
#endif
    if (!SeekStreamLogicalOffset((DWORD)SEEK_END, (int)(0UL - ulSeekDistance))) {
        goto Error;
    }

    /* Read the whole 5-byte entry into the stack field. */
    if (!ReadStreamBytesToDestBuffer(5U, (char *)&sEntry)) {
        goto Error;
    }

    /* Only the FIFTH byte is validated, and only against the LOW BYTE of the index. */
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("citrt_tagbyte", ((unsigned long)sEntry.abEntryRecord[4] & 0xFFu));
#endif
    if (sEntry.abEntryRecord[4] != (unsigned char)dwEntryIndex) {
        ulPayloadOffset = (unsigned long)sEntry.abEntryRecord[4];
        goto Error;
    }

    /* The first four bytes are the payload offset, little-endian; add the data base to
     * turn it into an absolute file position. */
    ulPayloadOffset =  (unsigned long)sEntry.abEntryRecord[0]
                     | ((unsigned long)sEntry.abEntryRecord[1] << 8)
                     | ((unsigned long)sEntry.abEntryRecord[2] << 16)
                     | ((unsigned long)sEntry.abEntryRecord[3] << 24);
    ulPayloadOffset += (unsigned long)STREAM_DATA_BASE;

#ifdef NWDEBUG_ENABLED
    nwsafe_trace("citrt_post_seek2");
#endif
    if (!SeekStreamLogicalOffset((DWORD)SEEK_BEGIN, (int)ulPayloadOffset)) {
        nwsafe_trace("citrt_seek2_fail");
        goto Error;
    }

    /* Hand the stream position to the shared tag fetcher, which leaves the byte it read
     * live in the global slot descriptor (nw_data_window + 0x16d0) for the callers downstream to decode. */
    if (!FetchNextStreamByteIntoSlotDescriptor()) {
        goto Error;
    }

    /* Success. */
    (void)SLOT_DESCRIPTOR_BYTE;   /* published by the callee above, not read here */
    return 1;

Error:
    /* Releases the tracked handle and latches the fatal "Invalid game data file." message;
     * returns 0, which is also what this function returns. */
    (void)CloseSaveSlotAndLatchInvalidDataError(0, 0);
    return 0;
}
#endif /* NW_UNIT_CheckIndexedStreamEntryAndReadRecordTag */

/* ==========================================================================
 * Unit: CopyStreamRecordToBuffer
 * Source: io/CopyStreamRecordToBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyStreamRecordToBuffer
/*
 * Copy Stream Record To Buffer.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-copystreamrecordtobuffer-c
 */

#include <stdint.h>

extern uint32_t g_dwStreamRecordByteCount; 
extern void    *g_pStreamRecordCursor;     

void *MoveMemoryRegion(void *pDest, const void *pSrc, uint32_t dwByteCount);

void *__stdcall CopyStreamRecordToBuffer(void *pDest)
{
    return MoveMemoryRegion(pDest, g_pStreamRecordCursor, g_dwStreamRecordByteCount);
}
#endif /* NW_UNIT_CopyStreamRecordToBuffer */

/* ==========================================================================
 * Unit: DecodeTaggedRecordLengthToOutParam
 * Source: io/DecodeTaggedRecordLengthToOutParam.c
 * ========================================================================== */
#ifdef NW_UNIT_DecodeTaggedRecordLengthToOutParam
/*
 * Fetches the next inline record tag byte and decodes the record's length/count word out of the stream, publishing it through a caller out-pointer.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-decodetaggedrecordlengthtooutparam-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

/* ---------------------------------------------------------------------------
 * GLOBALS
 * ------------------------------------------------------------------------- */

#define G_RECORD_TAG_BYTE       (*(const BYTE *)(nw_data_window + 0x16d0))

#define T_LENGTH_BIAS_SUB      ((const DWORD *)(uintptr_t)(nw_data_window + 0x1f8))
#define T_LENGTH_BIAS_ADD      ((const DWORD *)(uintptr_t)(nw_data_window + 0x20c))

extern int __cdecl FetchNextStreamByteIntoSlotDescriptor(void);

extern int __stdcall ReadStreamBytesToDestBuffer(DWORD dwRequestedBytes, char *lpDest);

extern DWORD __fastcall CloseSaveSlotAndLatchInvalidDataError(DWORD dwUnusedArg1,
                                                              DWORD dwUnusedArg2);

void __fastcall DecodeTaggedRecordLengthToOutParam(DWORD dwUnusedArg1,
                                                   DWORD dwUnusedArg2,
                                                   DWORD *pdwLengthOut)
{
    DWORD dwShiftBits;
    DWORD dwTagLowBits;
    DWORD dwFieldWidth;

    (void)dwUnusedArg1;
    (void)dwUnusedArg2;

    if (FetchNextStreamByteIntoSlotDescriptor() == 0) {
        goto Fail;
    }
#ifdef NWDEBUG_ENABLED
    {   /* Optional trace: tag byte + stream position for the record-length decode */
        extern void nwtrace_hex(const char *, unsigned long);
        extern int GetStreamLogicalBytePosition(void);
        nwtrace_hex("dlen_tag", (unsigned long)G_RECORD_TAG_BYTE);
        nwtrace_hex("dlen_pos", (unsigned long)GetStreamLogicalBytePosition());
    }
#endif

    if ((G_RECORD_TAG_BYTE & 0x80u) == 0) {
        *pdwLengthOut = (DWORD)G_RECORD_TAG_BYTE;
        goto Tail;
    }

    /* ---- ARM B: bit 7 set, bit 5 (0x20) clear -- the tag's low 5 bits are the byte COUNT ---- */
    if ((G_RECORD_TAG_BYTE & 0x20u) == 0) {
        dwFieldWidth = (DWORD)(G_RECORD_TAG_BYTE & 0x1Fu);   /* 00404d13 / 00404d18 */

    /* ---- ARM A: bit 7 set, bit 5 (0x20) set -- ((tag>>2)&3)+1 is the byte WIDTH of a field
     *      that is then bias-corrected; it is not itself the published quantity. ---- */
    } else {
        dwFieldWidth = ((DWORD)G_RECORD_TAG_BYTE >> 2) & 0x3u;   /* 00404d04/4d07/4d0a */
        dwFieldWidth += 1u;                                       /* 00404d10 INC ESI  */
    }

    *pdwLengthOut = 0;
    if (ReadStreamBytesToDestBuffer(dwFieldWidth, (char *)pdwLengthOut) == 0) {
        goto Fail;
    }

    if ((G_RECORD_TAG_BYTE & 0x20u) != 0) {

        /* 00404d40..00404d58: add the tag's low two bits at bit position width*8.
         * x86 masks an SHL count by 0x1f, so width 4 (4*8 = 32) wraps to a shift of 0 and
         * the low bits are added into the LOW byte instead. That wrap is the binary's. */
        dwShiftBits = (dwFieldWidth * 8u) & 0x1Fu;
        dwTagLowBits = (DWORD)(G_RECORD_TAG_BYTE & 0x3u);
        *pdwLengthOut += dwTagLowBits << dwShiftBits;

        /* 00404d5e..00404d7d: tag bit 4 picks the bias table and the direction.
         * The SUBTRACT arm can leave a negative dword in the out-pointer. */
        if ((G_RECORD_TAG_BYTE & 0x10u) != 0) {

            *pdwLengthOut = T_LENGTH_BIAS_SUB[dwFieldWidth * 2] - *pdwLengthOut;
        } else {
            *pdwLengthOut += T_LENGTH_BIAS_ADD[dwFieldWidth * 2];
        }
    }

Tail:
#ifdef NWDEBUG_ENABLED
    {   /* Optional trace: the published record-length word */
        extern void nwtrace_hex(const char *, unsigned long);
        nwtrace_hex("dlen_val", (unsigned long)*pdwLengthOut);
    }
#endif

    (void)FetchNextStreamByteIntoSlotDescriptor();
    return;

Fail:

    (void)CloseSaveSlotAndLatchInvalidDataError(dwUnusedArg1, dwUnusedArg2);
    return;
}
#endif /* NW_UNIT_DecodeTaggedRecordLengthToOutParam */

/* ==========================================================================
 * Unit: DecompressLzssBlockToBuffer
 * Source: io/DecompressLzssBlockToBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_DecompressLzssBlockToBuffer
/*
 * Expands one LZSS-compressed block into pbDst: a control byte carries 8 flags consumed LSB-first, a set flag copies the next source byte literally.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-decompresslzssblocktobuffer-c
 */

#include <windows.h>

void DecompressLzssBlockToBuffer(int cbSrc, BYTE *pbDst, BYTE *pbSrc)
{
    for (;;) {
        BYTE bFlags;      /* DL - control byte, bits consumed LSB first */
        int  cBitsLeft;   /* DH - control bits still unexamined (8 down to 1) */

        bFlags    = *pbSrc++;
        cBitsLeft = 8;
        cbSrc--;
        if (cbSrc < 0) {
            return;
        }

        for (;;) {
            if (bFlags & 1u) {

                cbSrc--;                    
                *pbDst++ = *pbSrc++;
                if (--cBitsLeft == 0) {
                    break;                  
                }
            } else {

                WORD wBackRef;              /* unaligned 16-bit read, x86 tolerates it */
                int  nDistance;
                int  nRepeat;

                cbSrc -= 2;
                if (cbSrc < 0) {
                    return;
                }
                wBackRef = *(const WORD *)pbSrc;
                pbSrc += 2;

                nDistance = (int)((((DWORD)wBackRef >> 12) << 8) | ((DWORD)wBackRef & 0xFFu));
                nRepeat   = (int)((wBackRef >> 8) & 0x0Fu) + 2;

                /* Byte-at-a-time so a distance smaller than the length replicates the
                   run instead of reading stale bytes - i.e. RLE overlap works. */
                do {
                    *pbDst = pbDst[-nDistance];
                    pbDst++;
                } while (--nRepeat >= 0);

                if (--cBitsLeft == 0) {
                    break;                  
                }
            }

            bFlags = (BYTE)(bFlags >> 1);
        }
    }
}
#endif /* NW_UNIT_DecompressLzssBlockToBuffer */

/* ==========================================================================
 * Unit: GetStreamLogicalBytePosition
 * Source: io/GetStreamLogicalBytePosition.c
 * ========================================================================== */
#ifdef NW_UNIT_GetStreamLogicalBytePosition
/*
 * Returns the stream's logical read position in BYTES: the raw OS file position of the tracked input slot minus the bytes still sitting unconsumed in...
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-getstreamlogicalbyteposition-c
 */

#include <windows.h>
#include "nwglobals.h"

extern DWORD __cdecl GetTrackedSlotFilePosition(void);

extern uint32_t g_nBufferedBytesRemaining;  /* dword at nw_data_window+0x16b4 */

int __cdecl GetStreamLogicalBytePosition(void)
{
    /* Raw OS position (bytes from file start) minus unread read-ahead bytes. */
    return (int)GetTrackedSlotFilePosition() - (int)g_nBufferedBytesRemaining;
}
#endif /* NW_UNIT_GetStreamLogicalBytePosition */

/* ==========================================================================
 * Unit: ReadBlockAndLatchPendingSlotHook
 * Source: io/ReadBlockAndLatchPendingSlotHook.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadBlockAndLatchPendingSlotHook
/*
 * Record-type 4 handler of the stream container loop: first discards any pending slot-hook state that the preceding records have made obsolete.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-readblockandlatchpendingslothook-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

#define g_cbBlockSize      (*(DWORD  *)(nw_data_window + 0x16fc))  /* stored byte count           */
#define g_cbHookBlockBytes (*(DWORD  *)(nw_data_window + 0x1730))  /* allocation size, dead arg 1 */
#define g_pbHookBlockBuf   NW_WINDOW_PTR_REF(BYTE, 0x1734)  /* read destination, hook arg  */

#define g_bPendingHookRecordSet (*(BYTE *)(nw_data_window + 0x1739))
#define g_bFrameAbandonSeen     (*(BYTE *)(nw_data_window + 0x173a))

/* The record's sub-type byte, read only by the flush condition. */
#define g_bBlockSubType     (*(BYTE *)(nw_data_window + 0x16f9))

/* ---------------------------------------------------------------------------
 * CALLEES
 * ------------------------------------------------------------------------- */

extern void ResetPendingSlotHookState(void);

extern int __stdcall ReadStreamBlockRawOrLzss(DWORD cbUnusedBlockSize, BYTE *pbDestBuf);

extern DWORD __stdcall SetPendingFlagFromSlotHookResult(DWORD dwHookArg);

extern BOOL __stdcall SeekStreamFileSlotToOffset(DWORD dwMoveMethod, LONG lOffset);

#define SEEK_CUR 1

BOOL __cdecl ReadBlockAndLatchPendingSlotHook(void)
{

    if (g_bPendingHookRecordSet != 0 &&
        (g_bFrameAbandonSeen == 0 || g_bBlockSubType != 0)) {
        ResetPendingSlotHookState();
        g_bPendingHookRecordSet = 0;
    }

    /* A zero-length payload is a legitimate no-op: success, and the latch is
     * left exactly as it was. */
    if (g_cbBlockSize == 0) {
        return TRUE;
    }

    /* ---- already registered: step over the block instead of reading it ----
     * SEEK_CUR by exactly the stored byte count, so an LZSS-compressed payload
     * is skipped by its compressed length, which is the length that is on disk.
     * The seek's own result is returned straight through -- note that this path
     * does NOT rewrite the latch, so the registration survives the skip. */
    if (g_bPendingHookRecordSet != 0) {
        return SeekStreamFileSlotToOffset(SEEK_CUR, (LONG)g_cbBlockSize);
    }

    /* ---- read the payload, then register it ---- */
    if (!ReadStreamBlockRawOrLzss(g_cbHookBlockBytes, g_pbHookBlockBuf)) {
        /* Short read: the buffer holds partial data, so it is NOT handed to the
         * hook and the latch is explicitly cleared. */
        g_bPendingHookRecordSet = 0;
        return FALSE;
    }

    g_bPendingHookRecordSet =
        (BYTE)(SetPendingFlagFromSlotHookResult((DWORD)(ULONG_PTR)g_pbHookBlockBuf)
              & 0xFFu);

    return (BOOL)(signed char)g_bPendingHookRecordSet;

}
#endif /* NW_UNIT_ReadBlockAndLatchPendingSlotHook */

/* ==========================================================================
 * Unit: ReadExactChunkFromStream
 * Source: io/ReadExactChunkFromStream.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadExactChunkFromStream
/*
 * Read Exact Chunk From Stream.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-readexactchunkfromstream-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

#define GLOBAL_STREAM_FD  (*(DWORD *)(nw_data_window + 0x5f54))

extern int __cdecl ReadStreamChunkCrLfNormalized(DWORD dwFileIndex,
                                                 char *lpBuffer,
                                                 DWORD cbBufferSize);

int __stdcall ReadExactChunkFromStream(DWORD  cbRequestedBytes,
                                     char  *lpBuffer)
{
#ifdef NWDEBUG_ENABLED
    extern void nwsafe_trace(const char *);
    extern void nwtrace_hex(const char *, unsigned long);
    extern unsigned char nw_data_window[];
    nwsafe_trace("recs_in");
    nwtrace_hex("recs_fd", (unsigned long)(*(DWORD *)(nw_data_window + 0x5F54)));
#endif

    int cbRead;

    /* Short reads and the callee's -1 both fail the equality test. */
    cbRead = ReadStreamChunkCrLfNormalized(GLOBAL_STREAM_FD, lpBuffer,
                                           cbRequestedBytes);
    return cbRead == (int)cbRequestedBytes;
}
#endif /* NW_UNIT_ReadExactChunkFromStream */

/* ==========================================================================
 * Unit: ReadLzssOrRawTaggedRecordBlock
 * Source: io/ReadLzssOrRawTaggedRecordBlock.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadLzssOrRawTaggedRecordBlock
/*
 * Read Lzss Or Raw Tagged Record Block.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-readlzssorrawtaggedrecordblock-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

/* ---------------------------------------------------------------------------
 * GLOBALS
 * ------------------------------------------------------------------------- */

/* @(nw_data_window + 0x16cc) -- cleared to 0 on entry (00404dff) and written ONLY on the LZSS arm
 * (00404ebf) with the COMPRESSED byte count that was simultaneously handed to
 * DecompressLzssBlockToBuffer as its cbSrc budget. Stays 0 on every other path, so 0
 * unambiguously means "the record just consumed was not LZSS-coded". This is the same
 * role as g_cbBlockSize in src/io/ReadStreamBlockRawOrLzss.c: the compressed-source budget. */
#define G_COMPRESSED_SRC_BYTES   (*(DWORD *)(nw_data_window + 0x16cc))

/* @(nw_data_window + 0x16c8) -- cleared to 0 on entry (00404e04), then published with one of THREE
 * different quantities depending on the arm taken:
 *   bit-6 arm (00404e31)  : the tag's low 5 bits -- a RAW copy byte count, 0..31;
 *   raw arm  (00404edf)   : the 1..4-byte length word streamed in at 00404e7c;
 *   LZSS arm (00404ecf)   : the 1..4-byte UNCOMPRESSED-length word streamed in at 00404ea2.
 * It is therefore a union-ish "length of the record just consumed, in whichever space that
 * record's class uses" slot, NOT one quantity. See the CONTRADICTION section below. */
#define G_LAST_RECORD_LENGTH    (*(DWORD *)(nw_data_window + 0x16c8))

#define G_RECORD_TAG_BYTE       (*(BYTE  *)(nw_data_window + 0x16d0))

#define G_COMPRESSED_LEN_SEED   (*(DWORD *)(nw_data_window + 0x22c))
#define G_UNCOMPRESSED_LEN_SEED (*(DWORD *)(nw_data_window + 0x230))

extern int __cdecl FetchNextStreamByteIntoSlotDescriptor(void);

extern int __stdcall ReadStreamBytesToDestBuffer(DWORD dwRequestedBytes, char *lpDest);

extern DWORD __fastcall CloseSaveSlotAndLatchInvalidDataError(DWORD dwUnusedArg1,
                                                              DWORD dwUnusedArg2);

extern void *__stdcall GetHeapArenaBlockEnd(DWORD dwRequestSize);

extern void __cdecl DecompressLzssBlockToBuffer(int cbSrc, BYTE *pbDst, BYTE *pbSrc);

extern void nwtrace_hex(const char *tag, unsigned long v);

DWORD __stdcall ReadLzssOrRawTaggedRecordBlock(BYTE *pbDestBuf)
{
    /* Optional trace: destination + the two buffer-selection flags. */
    nwtrace_hex("rlz_dst", (unsigned long)(uintptr_t)pbDestBuf);
    nwtrace_hex("rlz_f9", (unsigned long)*(volatile unsigned char *)(intptr_t)(nw_data_window + 0x67f9));
    nwtrace_hex("rlz_f7", (unsigned long)*(volatile unsigned char *)(intptr_t)(nw_data_window + 0x67f7));

    DWORD dwCompressedByteCount;   
    DWORD dwUncompressedByteCount; 
    DWORD dwLengthWidthBytes;      /* (tag & 3) + 1 -- width of BOTH streamed length words. */
    BYTE *pbScratchBlockEnd;       /* LZSS arm only: arena block holding the compressed bytes. */

    /* Clear the two published lengths up front, so a failed or skipped record leaves them
     * at 0 rather than at the previous record's values. */
    G_COMPRESSED_SRC_BYTES = 0;
    G_LAST_RECORD_LENGTH    = 0;

    if (FetchNextStreamByteIntoSlotDescriptor() == 0) {
        CloseSaveSlotAndLatchInvalidDataError(0, 0);
        return 0;
    }

    if ((G_RECORD_TAG_BYTE & 0x80u) == 0) {
        CloseSaveSlotAndLatchInvalidDataError(0, 0);
        return 0;
    }

    /* ============ ARM A: tag bit 6 SET (00404e2c) -- short RAW copy ============ */
    if ((G_RECORD_TAG_BYTE & 0x40u) != 0) {

        /* The low 5 bits ARE the raw byte count, published before the read. A count of 0
         * is a legal empty record and skips the read entirely -- no decompressor here. */
        dwCompressedByteCount = (DWORD)(G_RECORD_TAG_BYTE & 0x1Fu);
        G_LAST_RECORD_LENGTH  = dwCompressedByteCount;

        if (dwCompressedByteCount != 0) {
            if (ReadStreamBytesToDestBuffer(dwCompressedByteCount,
                                            (char *)pbDestBuf) == 0) {
                CloseSaveSlotAndLatchInvalidDataError(0, 0);
                return 0;
            }
        }

    /* ============ ARM B: tag bit 6 CLEAR (00404e54) -- length-prefixed ============ */
    } else {

        /* Two in-image-zero seeds; only the low byte of each can survive a 1-byte read. */
        dwCompressedByteCount   = G_COMPRESSED_LEN_SEED;
        dwUncompressedByteCount = G_UNCOMPRESSED_LEN_SEED;

        /* Read the COMPRESSED byte count, 1..4 bytes wide, into the local itself
         * (LEA at 00404e67 makes &dwCompressedByteCount the destination). */
        dwLengthWidthBytes = (DWORD)(G_RECORD_TAG_BYTE & 3u) + 1u;
        if (ReadStreamBytesToDestBuffer(dwLengthWidthBytes,
                                        (char *)&dwCompressedByteCount) == 0) {
            CloseSaveSlotAndLatchInvalidDataError(0, 0);
            return 0;
        }

        /* ---- B1: tag bits 3/2 CLEAR -- stored raw, no decompression ---- */
        if ((G_RECORD_TAG_BYTE & 0x0Cu) == 0) {

            /* The length is the FIRST word read above; copied through unchanged. */
            G_LAST_RECORD_LENGTH = dwCompressedByteCount;
            if (ReadStreamBytesToDestBuffer(dwCompressedByteCount,
                                            (char *)pbDestBuf) == 0) {
                CloseSaveSlotAndLatchInvalidDataError(0, 0);
                return 0;
            }

        /* ---- B2: tag bits 3/2 SET -- LZSS coded ---- */
        } else {

            pbScratchBlockEnd = (BYTE *)GetHeapArenaBlockEnd(dwCompressedByteCount);

            /* Read the UNCOMPRESSED byte count, same 1..4-byte width. */
            if (ReadStreamBytesToDestBuffer(dwLengthWidthBytes,
                                            (char *)&dwUncompressedByteCount) == 0) {
                CloseSaveSlotAndLatchInvalidDataError(0, 0);
                return 0;
            }

            /* Read the COMPRESSED payload into the arena scratch block. */
            if (ReadStreamBytesToDestBuffer(dwCompressedByteCount,
                                            (char *)pbScratchBlockEnd) == 0) {
                CloseSaveSlotAndLatchInvalidDataError(0, 0);
                return 0;
            }

            /* cbSrc is the COMPRESSED count -- 1:1 with what 00404ebf publishes, and the
             * same value sized the scratch block. It is NOT the tag's low 5 bits. */
            G_COMPRESSED_SRC_BYTES = dwCompressedByteCount;
            DecompressLzssBlockToBuffer((int)G_COMPRESSED_SRC_BYTES,
                                        pbDestBuf,
                                        pbScratchBlockEnd);

            /* The published record length is the UNCOMPRESSED size on this arm. */
            G_LAST_RECORD_LENGTH = dwUncompressedByteCount;
        }
    }

    return FetchNextStreamByteIntoSlotDescriptor();
}
#endif /* NW_UNIT_ReadLzssOrRawTaggedRecordBlock */

/* ==========================================================================
 * Unit: ReadStreamBlockRawOrLzss
 * Source: io/ReadStreamBlockRawOrLzss.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadStreamBlockRawOrLzss
/*
 * Reads one data block off the program's stream into the caller's buffer.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-readstreamblockraworlzss-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

#define GLOBAL_BLOCK_SIZE     (*(DWORD  *)(nw_data_window + 0x16fc))
#define GLOBAL_UNCOMP_SIZE    (*(DWORD  *)(nw_data_window + 0x1700))
#define GLOBAL_BLOCK_SCRATCH  NW_WINDOW_PTR_REF(BYTE, 0x1714)

extern int __stdcall ReadExactChunkFromStream(DWORD cbRequestedBytes, char *lpBuffer);

extern void __cdecl DecompressLzssBlockToBuffer(int cbSrc, BYTE *pbDst, BYTE *pbSrc);

int __stdcall ReadStreamBlockRawOrLzss(DWORD cbUnusedBlockSize, BYTE *pbDestBuf)
{
    (void)cbUnusedBlockSize;   /* dead parameter: never pushed to, never read */

    if (GLOBAL_BLOCK_SIZE == 0) {
        return 1;                                  
    }

    if (GLOBAL_BLOCK_SIZE != GLOBAL_UNCOMP_SIZE) {

        
        (void)ReadExactChunkFromStream(GLOBAL_BLOCK_SIZE,
                                       (char *)GLOBAL_BLOCK_SCRATCH);

        DecompressLzssBlockToBuffer((int)GLOBAL_BLOCK_SIZE,
                                    pbDestBuf,
                                    GLOBAL_BLOCK_SCRATCH);

        return 1;                                  
    }

    
    {
        int r = ReadExactChunkFromStream(GLOBAL_BLOCK_SIZE, (char *)pbDestBuf);
        return r;
    }
}
#endif /* NW_UNIT_ReadStreamBlockRawOrLzss */

/* ==========================================================================
 * Unit: ReadStreamBytesToDestBuffer
 * Source: io/ReadStreamBytesToDestBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadStreamBytesToDestBuffer
/*
 * Reads the requested number of bytes from the program's global stream into the caller's destination buffer.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-readstreambytestodestbuffer-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

#define STAGING_BUF_BASE      NW_WINDOW_PTR_REF(char, 0x16b8)  /* staging buffer base pointer        */
#define STAGING_CAPACITY      (*(DWORD    *)(nw_data_window + 0x6260))  /* staging byte capacity              */
#define STAGING_TOTAL_BYTES   (*(DWORD    *)(nw_data_window + 0x16b0))  /* bytes filled into staging this round*/
#define STAGING_REMAINING     (*(DWORD    *)(nw_data_window + 0x16b4))  /* remaining-to-drain in staging      */

extern int __stdcall ReadExactChunkFromStream(DWORD dwByteCount, char *lpBuffer);

extern int __stdcall ReadStreamChunkCountFromGlobalStream(DWORD dwByteCount, char *lpBuffer);

extern void *__cdecl MoveMemoryRegion(void *lpDest, const void *lpSrc, DWORD dwByteCount);

#ifdef NWDEBUG_ENABLED
extern void nwsafe_trace(const char *);
extern void nwtrace_hex(const char *, unsigned long);
#endif

int __stdcall ReadStreamBytesToDestBuffer(DWORD dwRequestedBytes, char *lpDest)
{
#ifdef NWDEBUG_ENABLED
    /* NOTE: STAGING_* are window macros, not variables -- no externs allowed here. */
    nwsafe_trace("rsbt_in");
    nwtrace_hex("rsbt_req", (unsigned long)dwRequestedBytes);
    nwtrace_hex("rsbt_dst", (unsigned long)(uintptr_t)lpDest);
#endif

    DWORD dwCopy;
    char *pSrc;

    for (;;) {
        /* Staging buffer empty?  Decide between the direct fast path and a buffered top-up. */
        if (STAGING_REMAINING == 0) {
            if (STAGING_TOTAL_BYTES <= dwRequestedBytes || STAGING_BUF_BASE == NULL) {
                /* Direct read straight from the global stream, no buffering; return its flag. */
                return ReadExactChunkFromStream(dwRequestedBytes, lpDest);
            }
            /* Top up the staging buffer; returned value is a signed byte count (see externs). */
            STAGING_TOTAL_BYTES =
                (DWORD)ReadStreamChunkCountFromGlobalStream(STAGING_CAPACITY,
                                                            (char *)STAGING_BUF_BASE);
            STAGING_REMAINING   = STAGING_TOTAL_BYTES;
        }

        /* Drain up to the requested number of bytes out of staging into lpDest. */
        dwCopy = STAGING_REMAINING;
        if (dwRequestedBytes < dwCopy) {
            dwCopy = dwRequestedBytes;
        }

        pSrc = (char *)STAGING_BUF_BASE + (STAGING_TOTAL_BYTES - STAGING_REMAINING);
        MoveMemoryRegion(lpDest, pSrc, dwCopy);

        lpDest            += dwCopy;
        STAGING_REMAINING -= dwCopy;
        dwRequestedBytes  -= dwCopy;

        if (dwRequestedBytes == 0) {
            return 1;   /* full delivery */
        }
    }
}
#endif /* NW_UNIT_ReadStreamBytesToDestBuffer */

/* ==========================================================================
 * Unit: ReadStreamChunkCountFromGlobalStream
 * Source: io/ReadStreamChunkCountFromGlobalStream.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadStreamChunkCountFromGlobalStream
/*
 * Read Stream Chunk Count From Global Stream.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-readstreamchunkcountfromglobalstream-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

#define GLOBAL_STREAM_FD  (*(DWORD *)(nw_data_window + 0x5f54))

extern int __cdecl ReadStreamChunkCrLfNormalized(DWORD dwFileIndex,
                                                 char *lpBuffer,
                                                 DWORD cbBufferSize);

int __stdcall ReadStreamChunkCountFromGlobalStream(DWORD cbRequestedBytes,
                                                char  *lpBuffer)
{
    return ReadStreamChunkCrLfNormalized(GLOBAL_STREAM_FD, lpBuffer,
                                         cbRequestedBytes);
}
#endif /* NW_UNIT_ReadStreamChunkCountFromGlobalStream */

/* ==========================================================================
 * Unit: ReadStreamChunkFromDescriptorVerifyCount
 * Source: io/ReadStreamChunkFromDescriptorVerifyCount.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadStreamChunkFromDescriptorVerifyCount
/*
 * Read Stream Chunk From Descriptor Verify Count.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-readstreamchunkfromdescriptorverifycount-c
 */

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
extern unsigned char nw_data_window[];

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_log_mark(const char *stage);
static void nw_read_markf(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    platform_log_mark(line);
}
#else
#define nw_read_markf(...) ((void)0)
#endif

#define GLOBAL_STREAM_FD  (*(DWORD *)(size_t)(nw_data_window + 0x5f58))

extern int __cdecl ReadStreamChunkCrLfNormalized(DWORD dwFileIndex,
                                                  char  *lpBuffer,
                                                  DWORD  cbBufferSize);

int __stdcall ReadStreamChunkFromDescriptorVerifyCount(DWORD cbRequestedBytes,
                                                    char  *lpBuffer)
{
    DWORD cbRead;   /* bytes returned by the normalising read; -1 on error */

    cbRead = (DWORD)ReadStreamChunkCrLfNormalized(GLOBAL_STREAM_FD,
                                                  lpBuffer,
                                                  cbRequestedBytes);
    if (cbRead != cbRequestedBytes) {
        nw_read_markf("save_rd_short_fd_%u_req_%u_got_%d_err_%lu",
                      (unsigned)GLOBAL_STREAM_FD, (unsigned)cbRequestedBytes,
                      (int)cbRead, (unsigned long)GetLastError());
    }
    return cbRead == cbRequestedBytes;
}
#endif /* NW_UNIT_ReadStreamChunkFromDescriptorVerifyCount */

/* ==========================================================================
 * Unit: ReadStreamHeaderRecordIntoGlobal
 * Source: io/ReadStreamHeaderRecordIntoGlobal.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadStreamHeaderRecordIntoGlobal
/*
 * Read Stream Header Record Into Global.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-readstreamheaderrecordintoglobal-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

#define GLOBAL_HEADER_RECORD (*(char *)(size_t)(nw_data_window + 0x632c))

/* Byte count of the fixed read: 0x4c = 76 decimal.  Pushed last, so it is the
 * callee's FIRST argument (the count).  A compile-time constant in the code, not
 * a value carried over from the caller. */
#define HEADER_RECORD_SIZE 0x4c

extern int __stdcall ReadStreamChunkFromDescriptorVerifyCount(DWORD cbRequestedBytes,
                                                           char  *lpBuffer);

void __cdecl ReadStreamHeaderRecordIntoGlobal(void)
{
    /* Result deliberately discarded -- the callee's bool (all requested bytes
     * delivered) is left in EAX and dropped by the following RET. */
    (void)ReadStreamChunkFromDescriptorVerifyCount(HEADER_RECORD_SIZE,
                                                   &GLOBAL_HEADER_RECORD);
}
#endif /* NW_UNIT_ReadStreamHeaderRecordIntoGlobal */

/* ==========================================================================
 * Unit: ReadTaggedRecordDeltaCountAndLatchStreamPosition
 * Source: io/ReadTaggedRecordDeltaCountAndLatchStreamPosition.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadTaggedRecordDeltaCountAndLatchStreamPosition
/*
 * Read Tagged Record Delta Count And Latch Stream Position.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-readtaggedrecorddeltacountandlatchstreamposition-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

#define SLOT_DESCRIPTOR_BYTE   (*(BYTE *)(nw_data_window + 0x16d0))

extern int __cdecl FetchNextStreamByteIntoSlotDescriptor(void);

extern int __stdcall ReadStreamBytesToDestBuffer(DWORD dwRequestedBytes, char *lpDest);

extern int __cdecl GetStreamLogicalBytePosition(void);

extern BOOL __stdcall SeekStreamLogicalOffset(DWORD dwMoveMethod, int nLogicalOffset);

extern DWORD __fastcall CloseSaveSlotAndLatchInvalidDataError(DWORD dwUnusedArg1,
                                                               DWORD dwUnusedArg2);

#define SEEK_CUR 1

/* Record-class mask on the tag byte. Bits 7,6,5,3,2 must ALL be set for this record flavor;
 * bits 1:0 are free and carry the delta length. 0xEC = 1110 1100b. */
#define TAG_CLASS_MASK   0xECu
#define TAG_LENGTH_MASK  0x03u

void __stdcall ReadTaggedRecordDeltaCountAndLatchStreamPosition(int *pRecordEndPosition)
{
    DWORD dwDeltaBytes;
    DWORD dwDeltaValue;

    /* Tag fetch FIRST: the whole decode below is driven by the byte it leaves in (nw_data_window + 0x16d0). */
    if (!FetchNextStreamByteIntoSlotDescriptor()) {
        goto Error;
    }

    /* Record-class gate: bits 7,6,5,3,2 must all be set; bits 1:0 carry the delta length. */
    if (((unsigned long)SLOT_DESCRIPTOR_BYTE & TAG_CLASS_MASK) != TAG_CLASS_MASK) {
        goto Error;
    }

    /* Scratch slot semantics preserved exactly: the dword store zero-extends the count into all
     * four bytes, then the read overwrites only the low `count` bytes -- that is what turns the
     * 1..4 file bytes into a little-endian widened unsigned value. */
    dwDeltaBytes = (DWORD)((unsigned long)SLOT_DESCRIPTOR_BYTE & TAG_LENGTH_MASK) + 1UL;
    dwDeltaValue = dwDeltaBytes;

    /* Read the 1..4-byte little-endian delta over the count dword; high bytes stay zero. */
    if (!ReadStreamBytesToDestBuffer(dwDeltaBytes, (char *)&dwDeltaValue)) {
        goto Error;
    }

    /* Latch the logical byte position reached AFTER the delta, BEFORE the skip below. */
    *pRecordEndPosition = GetStreamLogicalBytePosition();

    /* Step over the skipped payload, then pull the next record's tag into (nw_data_window + 0x16d0).
     * The fetcher's success flag is deliberately not tested -- this is a pure side effect. */
    (void)SeekStreamLogicalOffset((DWORD)SEEK_CUR, (int)dwDeltaValue);
    (void)FetchNextStreamByteIntoSlotDescriptor();
    return;

Error:
    /* Releases the tracked handle and latches the fatal "Invalid game data file." message. */
    (void)CloseSaveSlotAndLatchInvalidDataError(0, 0);
}
#endif /* NW_UNIT_ReadTaggedRecordDeltaCountAndLatchStreamPosition */

/* ==========================================================================
 * Unit: SeekStreamFileSlotToOffset
 * Source: io/SeekStreamFileSlotToOffset.c
 * ========================================================================== */
#ifdef NW_UNIT_SeekStreamFileSlotToOffset
/*
 * Seek Stream File Slot To Offset.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-seekstreamfileslottooffset-c
 */

#include <windows.h>
#include "nwglobals.h"


extern DWORD __cdecl SeekTrackedSlotHandleOffset(int   nSlotIndex,
                                                 LONG  lDistanceToMove,
                                                 DWORD dwMoveMethod);


#define SETFILEPOINTER_INVALID ((DWORD)0xffffffffL)

BOOL __stdcall SeekStreamFileSlotToOffset(DWORD dwMoveMethod,
                                          LONG  lOffset)
{

    unsigned long r = (unsigned long)SeekTrackedSlotHandleOffset(g_nStreamFileSlot, lOffset, dwMoveMethod);
#ifdef NWDEBUG_ENABLED
    extern void nwsafe_trace(const char *);
    nwsafe_trace("sso_out");
#endif
    return (BOOL)(r != SETFILEPOINTER_INVALID);
}
#endif /* NW_UNIT_SeekStreamFileSlotToOffset */

/* ==========================================================================
 * Unit: SeekStreamLogicalOffset
 * Source: io/SeekStreamLogicalOffset.c
 * ========================================================================== */
#ifdef NW_UNIT_SeekStreamLogicalOffset
/*
 * Seeks the program's global stream file in logical stream coordinates: when the caller moves relative to the current position the pending...
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-seekstreamlogicaloffset-c
 */

#include <windows.h>
#include "nwglobals.h"

extern BOOL __stdcall SeekStreamFileSlotToOffset(DWORD dwMoveMethod,
                                                 LONG  lOffset);


extern DWORD g_dwStreamRemainingBytes;

#define SEEK_CUR_STREAM 1   /* FILE_CUR: the one origin whose result depends on the backlog */

BOOL __stdcall SeekStreamLogicalOffset(DWORD dwMoveMethod,
                                       int    nLogicalOffset)
{
    if (dwMoveMethod == SEEK_CUR_STREAM) {
        nLogicalOffset -= (int)g_dwStreamRemainingBytes;   /* cancel the pending-buffer backlog */
    }
    g_dwStreamRemainingBytes = 0;                         /* discard the unread buffer */

    {
#ifdef NWDEBUG_ENABLED
        extern void nwsafe_trace(const char *);
#endif
        int r = (int)SeekStreamFileSlotToOffset(dwMoveMethod, (LONG)nLogicalOffset);
#ifdef NWDEBUG_ENABLED
        nwsafe_trace("slo_out");
#endif
        return (BOOL)r;
    }
}
#endif /* NW_UNIT_SeekStreamLogicalOffset */

/* ==========================================================================
 * Unit: WriteStreamChunkCheckComplete
 * Source: io/WriteStreamChunkCheckComplete.c
 * ========================================================================== */
#ifdef NW_UNIT_WriteStreamChunkCheckComplete
/*
 * Write Stream Chunk Check Complete.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-writestreamchunkcheckcomplete-c
 */

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
extern unsigned char nw_data_window[];

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_log_mark(const char *stage);
static void nw_stream_markf(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    platform_log_mark(line);
}
#else
#define nw_stream_markf(...) ((void)0)
#endif

extern int __cdecl WriteStreamChunkLfToCrLfExpanded(DWORD dwFileIndex,
                                                    char  *lpBuffer,
                                                    DWORD cbBufferSize);

#define STREAM_CURRENT_DESCRIPTOR ((DWORD *)(nw_data_window + 0x5f58))

BOOL __stdcall WriteStreamChunkCheckComplete(DWORD cbCount, char *lpBuffer)
{
    DWORD dwDescriptor;   /* descriptor index forwarded from (nw_data_window + 0x5f58) */
    int   nBytesWritten;  /* EAX from the callee: input bytes accepted, or -1 */

    dwDescriptor = *STREAM_CURRENT_DESCRIPTOR;

    nBytesWritten = WriteStreamChunkLfToCrLfExpanded(dwDescriptor,
                                                    lpBuffer,
                                                    cbCount);

    if (nBytesWritten != (int)cbCount) {
        nw_stream_markf("save_wr_short_%u_%d_err_%lu", (unsigned)cbCount,
                        nBytesWritten, (unsigned long)GetLastError());
    }

    return (nBytesWritten == (int)cbCount) ? TRUE : FALSE;
}
#endif /* NW_UNIT_WriteStreamChunkCheckComplete */

/* ==========================================================================
 * Unit: WriteStreamChunkFromStackArgumentDword
 * Source: io/WriteStreamChunkFromStackArgumentDword.c
 * ========================================================================== */
#ifdef NW_UNIT_WriteStreamChunkFromStackArgumentDword
/*
 * Writes one dword from a stack argument to the stream.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-writestreamchunkfromstackargumentdword-c
 */

#include <windows.h>

extern BOOL __stdcall WriteStreamChunkCheckComplete(DWORD cbCount, char *lpBuffer);

void __stdcall WriteStreamChunkFromStackArgumentDword(DWORD dwStackArgumentValue)
{
    /* LEA EAX,[ESP+0x4] hands over the ADDRESS of the argument slot, so the callee reads
       the 4 bytes of the value the caller pushed -- not the value itself. */
    WriteStreamChunkCheckComplete(4u, (char *)&dwStackArgumentValue);
}
#endif /* NW_UNIT_WriteStreamChunkFromStackArgumentDword */
