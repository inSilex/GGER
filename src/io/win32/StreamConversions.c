#include "nwarch.h"
/*
 * StreamConversions -- Win32 stream line-ending and save-header conversions.
 *
 * Module: io/win32
 * Interface: include/StreamConversions.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   LoadAndVerifySaveFileHeaderChunk                           io/win32/LoadAndVerifySaveFileHeaderChunk.c
 *   ReadStreamChunkCrLfNormalized                              io/win32/ReadStreamChunkCrLfNormalized.c
 *   WriteStreamChunkLfToCrLfExpanded                           io/win32/WriteStreamChunkLfToCrLfExpanded.c
 */

/* ==========================================================================
 * Unit: LoadAndVerifySaveFileHeaderChunk
 * Source: io/win32/LoadAndVerifySaveFileHeaderChunk.c
 * ========================================================================== */
#ifdef NW_UNIT_LoadAndVerifySaveFileHeaderChunk
/*
 * Load And Verify Save File Header Chunk.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-loadandverifysavefileheaderchunk-c
 */

#include <stdint.h>
#include <windows.h>
#include "nwglobals.h"
#include "nwdebug.h"

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_log_mark(const char *stage);
#define NW_SAVE_BOOT(stage) platform_log_mark(stage)
#else
#define NW_SAVE_BOOT(stage) ((void)0)
#endif

/* Stage 1 runs deep on the main thread; shallow nwtrace_hex probes are still
 * available when NW_DEBUG_TRACE is set.  The former ESP/stack-descent diagnostics
 * were removed with the EH3 rewrite; they were temporary measurement code and
 * read below the live stack. */
extern void nwtrace_c(const char *tag);

typedef unsigned char  u8;
typedef unsigned int   u32;

#define SAVE_STREAM_LOGICAL_ORIGIN  2

extern void __stdcall ResetStreamCursorState(int nStreamIndex);      
extern int __stdcall SeekStreamLogicalOffset(int nOrigin, int nOffset);       
extern void CloseSaveSlotAndLatchInvalidDataError(void);             
extern u8   * __stdcall FillMemoryWithZeroBytes(u32 dwCount, u8 *pDst);        
extern int __stdcall OpenTrackedSlotBinaryAndStoreResult(const char *lpszName);
extern int __stdcall ReadExactChunkFromStream(u32 dwCount, u8 *pDst);        
extern DWORD __cdecl GetTrackedSlotFilePosition(void);                       

/* Optional trace: state of the stream slot/handle/position around the header seek. */
#define S1_FD       (*(DWORD *)(nw_data_window + 0x5F54))
#define S1_HANDLE   NW_WINDOW_PTR_REF(void, 0xBB8 + 4 * S1_FD)

/* ---- globals written by this function ---- */

#define SAVE_HDR_BYTES          0x0c
#define SAVE_HDR_VERSION_BYTE   0x07
#define SAVE_HDR_VERSION_VALUE  0x04
#define SAVE_HDR_MAGIC_OFFSET   0x08
#define SAVE_HDR_MAGIC_VALUE    0x3d53563du   /* "=VS=" little-endian */
#define SAVE_PAYLOAD_OFFSET     0x0c
#define SAVE_PAYLOAD_CAPACITY   0xb8

/* The field at payload+4 is the data-base file offset, despite the historical
 * name `g_dwSavedRecordCount` (the record count is payload+0, stored as
 * `g_dwSavedExtent`).  The original image put its overlay at 0x12800, so a
 * 1 MiB bound was enough.  The static SDL3 image is larger, so the overlay
 * starts past 1 MiB; bound the offset generously instead. */
#define SAVE_MAX_RECORD_COUNT   0x10000000u
#define SAVE_MAX_EXTENT         0xfeu

static u32 ReadU32(const u8 *pby)
{
    return (u32)pby[0]
         | ((u32)pby[1] << 8)
         | ((u32)pby[2] << 16)
         | ((u32)pby[3] << 24);
}

int __stdcall LoadAndVerifySaveFileHeaderChunk(const char *lpszSaveFileName)
{
    /* 0x00c4-byte stack frame; header at +0x00, payload at +0x0c (0xb8 bytes,
       which is exactly the remainder of the frame). */
    u8 abyFrame[SAVE_HDR_BYTES + SAVE_PAYLOAD_CAPACITY];
    u8 *pbyHeader  = abyFrame;
    u8 *pbyPayload = abyFrame + SAVE_PAYLOAD_OFFSET;
    u32 dwPayloadBytes;
    u32 dwHdrCursor;

    g_bSaveLoadActive = 0;

    ResetStreamCursorState(0);

    int r_open = OpenTrackedSlotBinaryAndStoreResult(lpszSaveFileName);
    nwtrace_hex("s1_open_rc", (unsigned)r_open);
    if (r_open == 0) {
        NW_SAVE_BOOT("save_open_fail");
        goto lblFail;
    }

    nwtrace_hex("s1_fd", (unsigned long)S1_FD);
    nwtrace_hex("s1_handle", (unsigned long)(uintptr_t)S1_HANDLE);
    nwtrace_hex("s1_pos_before", (unsigned long)GetTrackedSlotFilePosition());
    nwtrace_hex("s1_rawpos_before", (unsigned long)SetFilePointer(S1_HANDLE, 0, NULL, FILE_CURRENT));
    int r_seek1 = SeekStreamLogicalOffset(SAVE_STREAM_LOGICAL_ORIGIN, -(int)SAVE_HDR_BYTES);
    nwtrace_hex("s1_seek_rc", (unsigned)r_seek1);
    nwtrace_hex("s1_pos_after", (unsigned long)GetTrackedSlotFilePosition());
    nwtrace_hex("s1_rawpos_after", (unsigned long)SetFilePointer(S1_HANDLE, 0, NULL, FILE_CURRENT));
    if (r_seek1 == 0) {
        NW_SAVE_BOOT("save_seek_end_fail");
        goto lblFail;
    }

#ifdef NWDEBUG_ENABLED
    extern void nwsafe_trace(const char *);
    extern void nwtrace_hex(const char *, unsigned long);
    extern unsigned char nw_data_window[];
#endif

    FillMemoryWithZeroBytes(SAVE_HDR_BYTES, pbyHeader);
#ifdef NWDEBUG_ENABLED
    nwsafe_trace("s1_bread");
    nwtrace_hex("s1_fd", (unsigned long)(*(DWORD *)(nw_data_window + 0x5F54)));
#endif

    int r_read = ReadExactChunkFromStream(SAVE_HDR_BYTES, pbyHeader);
    nwtrace_hex("s1_read_rc", (unsigned)r_read);
    nwtrace_hex("s1_h0", (unsigned)ReadU32(pbyHeader));
    nwtrace_hex("s1_h1", (unsigned)ReadU32(pbyHeader + 4));
    nwtrace_hex("s1_rawpos_post", (unsigned long)SetFilePointer(S1_HANDLE, 0, NULL, FILE_CURRENT));
    nwtrace_hex("s1_hdr_magic", (unsigned)(pbyHeader[8] | (pbyHeader[9]<<8) | (pbyHeader[10]<<16) | (pbyHeader[11]<<24)));
    if (r_read == 0) {
        NW_SAVE_BOOT("save_read_header_fail");
        goto lblFail;
    }

    if (pbyHeader[SAVE_HDR_VERSION_BYTE] != SAVE_HDR_VERSION_VALUE) {
        NWDEBUG("savehdr FAIL version byte=%X", pbyHeader[SAVE_HDR_VERSION_BYTE]);
        NW_SAVE_BOOT("save_version_fail");
        goto lblFail;
    }

    if (ReadU32(pbyHeader + SAVE_HDR_MAGIC_OFFSET) != SAVE_HDR_MAGIC_VALUE) {
        NWDEBUG("savehdr FAIL magic=%08X", ReadU32(pbyHeader + SAVE_HDR_MAGIC_OFFSET));
        NW_SAVE_BOOT("save_magic_fail");
        goto lblFail;
    }

    dwHdrCursor    = ReadU32(pbyHeader);

    g_pSavedStateCursor = (u8 *)(uintptr_t)(dwHdrCursor + SAVE_HDR_BYTES);

    /* Rewind past the header plus the payload just described. */
    if (SeekStreamLogicalOffset(SAVE_STREAM_LOGICAL_ORIGIN,
                                -(int)(dwHdrCursor + SAVE_HDR_BYTES)) == 0) {
        NW_SAVE_BOOT("save_seek_payload_fail");
        goto lblFail;
    }

    FillMemoryWithZeroBytes(SAVE_PAYLOAD_CAPACITY, pbyPayload);

    /* NOTE: dwPayloadBytes comes straight from the file and is NOT range
       checked against SAVE_PAYLOAD_CAPACITY before this read. */
    dwPayloadBytes = dwHdrCursor;
    if (ReadExactChunkFromStream(dwPayloadBytes, pbyPayload) == 0) {
        NW_SAVE_BOOT("save_read_payload_fail");
        goto lblFail;
    }

    g_dwSavedRecordCount = ReadU32(pbyPayload + 0x04);
    if (g_dwSavedRecordCount > SAVE_MAX_RECORD_COUNT) {
        NW_SAVE_BOOT("save_record_count_fail");
        goto lblFail;
    }

    g_dwSavedExtent = ReadU32(pbyPayload);
    if (g_dwSavedExtent > SAVE_MAX_EXTENT) {
        NW_SAVE_BOOT("save_extent_fail");
        goto lblFail;
    }

    /* Success path: it never calls the closer - the tracked slot stays open. */
    return 1;

lblFail:

    CloseSaveSlotAndLatchInvalidDataError();
    return 0;
}
#endif /* NW_UNIT_LoadAndVerifySaveFileHeaderChunk */

/* ==========================================================================
 * Unit: ReadStreamChunkCrLfNormalized
 * Source: io/win32/ReadStreamChunkCrLfNormalized.c
 * ========================================================================== */
#ifdef NW_UNIT_ReadStreamChunkCrLfNormalized
/*
 * Reads up to cbBufferSize bytes from stream file descriptor dwFileIndex into lpBuffer.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-readstreamchunkcrlfnormalized-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];
#include <string.h>

#define STREAM_FD_MAX      0x3c
#define STREAM_FLAGS_BASE  (nw_data_window + 0xb7c)
#define STREAM_HANDLE_BASE NW_WINDOW_PTR_SLOTS(0xbb8)

#define STREAM_FLAGS(fd)   (*(BYTE   *)((uintptr_t)STREAM_FLAGS_BASE + (uintptr_t)(fd)))
#define STREAM_HANDLE(fd)  (((HANDLE *)STREAM_HANDLE_BASE)[(fd)])

/* Descriptor flag bits this routine actually tests. */
#define STREAM_F_EOF       0x01   /* set on a short read, cleared on entry to the
                                   * text path, and forced at CTRL-Z */
#define STREAM_F_APPEND    0x08   /* append: never rewind the file pointer */
#define STREAM_F_TEXT      0x10   /* text mode: CRLF -> LF, CTRL-Z ends the data */

#define STREAM_EBADF       9      /* errno value stored on every failure path */
#define STREAM_CTRL_Z      0x1a

extern void __fastcall SetThreadContextValueAndClearFlag(DWORD dwUnusedParam1,
                                              DWORD dwUnusedParam2,
                                              DWORD dwErrno);

int __cdecl ReadStreamChunkCrLfNormalized(DWORD dwFileIndex,
                                          char *lpBuffer,
                                          DWORD cbBufferSize)
{
    BYTE   byFlags;          /* descriptor flag byte sampled on entry          */
    char  *pEnd;             /* EDX   -- one past the valid data                */
    char  *pScan;            /* EDI   -- SCASB cursor, one past a matched byte  */
    char  *pSrc;             /* ESI   -- first byte not yet compacted           */
    char  *pWrite;           /* [EBP-0x4] -- compaction destination cursor       */
    DWORD  cbRead;           /* lpNumberOfBytesRead handed to ReadFile          */
    DWORD  cbMove;           /* bytes relocated by one compaction step          */
    DWORD  cbFree;           /* ECX -- bytes still free in the buffer           */
    long   lCrRemoved;       
    char  *pCr;              /* the trailing CR inspected by the partial path   */

#ifdef NWDEBUG_ENABLED
extern void nwsafe_trace(const char *);
extern void nwtrace_hex(const char *, unsigned long);
#endif

    if (dwFileIndex >= STREAM_FD_MAX) {               
#ifdef NWDEBUG_ENABLED
        nwsafe_trace("rcn_fd_toobig");
        nwtrace_hex("rcn_fdval", (unsigned long)dwFileIndex);
#endif
        goto error;                                   
    }

    if (cbBufferSize == 0) {                          
#ifdef NWDEBUG_ENABLED
        nwsafe_trace("rcn_zerolen");
#endif
        return 0;                                     
    }

#ifdef NWDEBUG_ENABLED
    nwsafe_trace("rcn_in");
#endif

    byFlags = STREAM_FLAGS(dwFileIndex);
#ifdef NWDEBUG_ENABLED
    { BYTE _bf = (BYTE)byFlags;
      nwtrace_hex("rcn_flags", ((unsigned long)_bf << 16u) + (0x8000u | (_bf & STREAM_F_TEXT ? 1 : 2))); }
#endif
    if ((byFlags & STREAM_F_TEXT) == 0) {             
        cbRead = 0;
#ifdef NWDEBUG_ENABLED
        nwsafe_trace("rcn_pre_rf");
#endif
        if (!ReadFile(STREAM_HANDLE(dwFileIndex), lpBuffer, cbBufferSize,
                      &cbRead, NULL)) {               
#ifdef NWDEBUG_ENABLED
            nwsafe_trace("rcn_rf_fail");
#endif
            goto error;
        }
#ifdef NWDEBUG_ENABLED
        nwsafe_trace("rcn_rf_ok");
        nwtrace_hex("rcn_cbread", cbRead);
#endif
        return (int)cbRead;                           
    }

    /* ---- text mode ---- */
    STREAM_FLAGS(dwFileIndex) = (BYTE)(byFlags & (BYTE)~STREAM_F_EOF);
    pWrite = lpBuffer;
    cbFree = cbBufferSize;

refill:                                                
    cbRead = 0;
    if (!ReadFile(STREAM_HANDLE(dwFileIndex), pWrite, cbFree, &cbRead, NULL))
        goto error;                                    
    if (cbRead < cbFree)
        STREAM_FLAGS(dwFileIndex) |= STREAM_F_EOF;     
    if (cbRead == 0)
        return (int)(pWrite - lpBuffer);               
    pEnd = pWrite + cbRead;

    for (pScan = pWrite; pScan < pEnd && *pScan != STREAM_CTRL_Z; pScan++)
        ;
    if (pScan < pEnd) {                                
        pScan--;                                       
        if ((STREAM_FLAGS(dwFileIndex) & STREAM_F_APPEND) == 0 &&
            SetFilePointer(STREAM_HANDLE(dwFileIndex),
                           (LONG)(pEnd - pScan), NULL, FILE_CURRENT)
                == INVALID_SET_FILE_POINTER)           
            goto error;
        STREAM_FLAGS(dwFileIndex) |= STREAM_F_EOF;     
        pEnd = pScan;                                  
    }

    /* CRLF -> LF, compacted in place inside the caller's buffer.  Every pair found
     * shifts the run since the previous LF one byte left, so the CR is dropped and
     * the LF survives.  pScan trails its match by one byte, exactly as SCASB does. */
    pSrc = NULL;                                        
    for (;;) {
        while (pScan < pEnd && *pScan != '\r')          
            pScan++;
        if (pScan == pEnd)
            return (int)(pEnd - lpBuffer);              
        pScan++;                                        /* past the CR */
        if (pScan >= pEnd)
            goto partial_line;                          
        if (*pScan != '\n')
            continue;                                   

        if (pSrc == NULL) {                             
            pSrc = pScan - 1;                           /* ESI = the LF       */
            pWrite = pScan - 2;                         /* [EBP-0x4] = LF - 1  */
            break;

            for (;;) {                                   
                while (pScan < pEnd && *pScan != '\r')
                    pScan++;
                if (pScan == pEnd)
                    goto finish;                         
                pScan++;
                if (pScan >= pEnd)
                    goto partial_line;                   
                if (*pScan != '\n')
                    continue;                            
                cbMove = (DWORD)((pScan - 1) - pSrc);    
                memmove(pWrite, pSrc, cbMove);            
                pWrite += cbMove;                        
                pSrc = pScan;                            
            }
        }
        cbMove = (DWORD)((pScan - 1) - pSrc);
        memmove(pWrite, pSrc, cbMove);
        pWrite += cbMove;
        pSrc = pScan;
    }

partial_line:                                            
    if ((STREAM_FLAGS(dwFileIndex) & STREAM_F_EOF) == 0) {
        pCr = pScan - 1;                                 /* [EDI-0x1] */

        cbRead = 0;
        if (!ReadFile(STREAM_HANDLE(dwFileIndex), pEnd - 1, 1, &cbRead, NULL))
            goto error;                                  
        if (cbRead == 0)
            STREAM_FLAGS(dwFileIndex) |= STREAM_F_EOF;   
        else if (*pCr != '\n') {                         
            /* A CRLF was split across the buffer: leave the CR here and back the
             * file pointer up so the LF arrives with the next read. */
            *pCr = '\r';                                 
            if (SetFilePointer(STREAM_HANDLE(dwFileIndex), -1L, NULL,
                               FILE_CURRENT) == INVALID_SET_FILE_POINTER)
                goto error;                              
        }
    }
    goto finish;                                         

finish:                                                  
    if (pSrc == NULL)
        return (int)(pEnd - lpBuffer);                   
    cbMove = (DWORD)(pScan - pSrc);                      
    memmove(pWrite, pSrc, cbMove);                       
    pWrite += cbMove;
    if ((STREAM_FLAGS(dwFileIndex) & STREAM_F_APPEND) != 0)
        return (int)(pWrite - lpBuffer);                 
    lCrRemoved = (long)(pSrc - pWrite);                  
    if (lCrRemoved == 0)
        return (int)(pWrite - lpBuffer);                 
    if ((STREAM_FLAGS(dwFileIndex) & STREAM_F_EOF) == 0) {

        cbFree = (DWORD)lCrRemoved;
        goto refill;                                     
    }
    return (int)(pWrite - lpBuffer);                     

error:                                                    
    SetThreadContextValueAndClearFlag(0, 0, STREAM_EBADF);
    return -1;                                            
}
#endif /* NW_UNIT_ReadStreamChunkCrLfNormalized */

/* ==========================================================================
 * Unit: WriteStreamChunkLfToCrLfExpanded
 * Source: io/win32/WriteStreamChunkLfToCrLfExpanded.c
 * ========================================================================== */
#ifdef NW_UNIT_WriteStreamChunkLfToCrLfExpanded
/*
 * Write Stream Chunk Lf To Cr Lf Expanded.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-writestreamchunklftocrlfexpanded-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];
#include <string.h>

#define STREAM_FD_MAX      0x3c
#define STREAM_FLAGS_BASE  (nw_data_window + 0xb7c)
#define STREAM_HANDLE_BASE NW_WINDOW_PTR_SLOTS(0xbb8)

#define STREAM_FLAGS(fd)   (*(BYTE   *)((uintptr_t)STREAM_FLAGS_BASE + (uintptr_t)(fd)))
#define STREAM_HANDLE(fd)  (((HANDLE *)STREAM_HANDLE_BASE)[(fd)])

/* Descriptor flag bits this routine actually tests. */
#define STREAM_F_SEEK_END  0x04   /* reposition the handle to end of file first   */
#define STREAM_F_TEXT      0x10   /* text mode: LF -> CRLF on the way out         */

#define STREAM_EBADF       9      /* errno on a bad fd / handle / overflow         */
#define STREAM_ENOSPC      0x1c   /* errno when WriteFile transfers nothing       */

extern DWORD (__cdecl *g_pfStandardStreamRedirect)(DWORD dwFileIndex,
                                                   char *lpBuffer,
                                                   DWORD cbWrite);

extern void __fastcall SetThreadContextValueAndClearFlag(DWORD dwUnusedParam1,
                                              DWORD dwUnusedParam2,
                                              DWORD dwErrno);

int __cdecl WriteStreamChunkLfToCrLfExpanded(DWORD dwFileIndex,
                                             char *lpBuffer,
                                             DWORD cbBufferSize)
{
    HANDLE hSlot;              /* hSlotHandle[dwFileIndex]                     */
    BYTE   byFlags;            /* descriptor flag byte sampled on entry          */
    DWORD  cbWritten;          /* lpNumberOfBytesWritten handed to WriteFile    */
    DWORD  cbNewlines;         /* LF bytes counted in the caller's buffer       */
    DWORD  cbAllocSize;        /* expansion buffer size, DWORD-rounded           */
    char  *pExpandBuf;         /* GlobalAlloc'd staging buffer                  */
    char  *pExpandEnd;         /* last writable byte of that buffer             */
    char  *pSrc;               /* source cursor into the caller's buffer        */
    char  *pDst;               /* write cursor into the staging buffer          */
    DWORD  cbOutputBudget;     /* output bytes still owed to the staging buffer */
    BYTE   bCopyVerbatim;      /* sticky: current run holds no LF               */
    char  *pScan;              /* SCASB cursor                                  */
    char  *pRunStart;          /* first byte of the run being copied            */
    DWORD  cbScanLeft;         /* remaining scans of the REPNE loops            */
    DWORD  cbRun;              /* source bytes covered by one run              */
    DWORD  cbCopy;             /* bytes this run may copy right now             */
    DWORD  cbExpandBytes;      /* staging bytes produced so far                 */
    DWORD  cbResult;           /* value handed back to the caller               */
    BOOL   bRunEndsWithLf;
    BOOL   bExpanded;          /* this run grew an LF into CRLF                 */

    if (dwFileIndex >= STREAM_FD_MAX)                      
        goto error;                                        

    if (cbBufferSize == 0)                                 
        return 0;                                          

    hSlot  = STREAM_HANDLE(dwFileIndex);
    byFlags = STREAM_FLAGS(dwFileIndex);

    /* Neither seek-to-end nor text mode: a single raw write. */
    if ((byFlags & (STREAM_F_SEEK_END | STREAM_F_TEXT)) == 0)   
        goto plain_write;

    if ((byFlags & STREAM_F_SEEK_END) != 0) {               

        if ((WORD)SetFilePointer(hSlot, 0, NULL, FILE_END) == 0xffff)
            goto error;                                    
        if ((byFlags & STREAM_F_TEXT) == 0)                
            goto plain_write;
    }

    /* ---- text mode: LF -> CRLF ---- */

    /* Count the LF bytes already in the caller's buffer; the REPNE scan is bounded
     * by cbBufferSize, so a buffer with no LF at all skips the expansion entirely. */
    cbNewlines = 0;
    for (pScan = lpBuffer, cbScanLeft = cbBufferSize; cbScanLeft != 0;
         cbScanLeft--, pScan++)
        if (*pScan == '\n')
            cbNewlines++;                                  

    if (cbNewlines == 0)                                   
        goto plain_write;                                  

    /* One extra byte per LF, rounded down to a whole DWORD; both ADDs are
     * carry-checked, so a request that would wrap 32 bits is rejected. */
    cbAllocSize = cbBufferSize + cbNewlines;               
    if (cbAllocSize < cbBufferSize)
        goto error;
    cbAllocSize += 4;                                      
    if (cbAllocSize < 4)
        goto error;
    cbAllocSize &= ~3u;                                    

    pExpandBuf = (char *)GlobalAlloc(GMEM_FIXED, cbAllocSize);
    if (pExpandBuf == NULL)                                
        goto error;

    pExpandEnd     = pExpandBuf + cbAllocSize - 1;         
    pSrc           = lpBuffer;                             
    pDst           = pExpandBuf;                           
    cbOutputBudget = cbBufferSize;                         
    bCopyVerbatim  = 0;

    for (;;) {

        pRunStart  = pSrc;
        cbScanLeft = cbOutputBudget;
        while (*pSrc != '\n' && cbScanLeft != 0) {
            pSrc++;
            cbScanLeft--;
        }
        bRunEndsWithLf = (*pSrc == '\n');
        cbRun = (DWORD)((pSrc + (bRunEndsWithLf ? 1 : 0)) - pRunStart);
        pSrc  = pSrc + (bRunEndsWithLf ? 1 : 0);

        if (!bRunEndsWithLf)                               
            bCopyVerbatim = 1;

        cbOutputBudget = cbScanLeft;                       

        if ((size_t)(pDst + cbRun) > (size_t)pExpandEnd) { 

            cbCopy = (size_t)pDst >= (size_t)pExpandEnd
                     ? 0u : (DWORD)(pExpandEnd - pDst);     
            bExpanded = FALSE;
        } else if (bCopyVerbatim) {
            cbCopy    = cbRun;                             
            bExpanded = FALSE;
        } else {
            cbCopy    = cbRun;                             
            bExpanded = TRUE;
        }

        memmove(pDst, pRunStart, cbCopy);                  /* MOVSD/MOVSB pairs */
        pDst += cbCopy;

        if (bExpanded) {

            pDst[-1] = '\r';
            pDst[0]  = '\n';
            pDst++;
            if (cbOutputBudget != 0)                       
                continue;                                  /* room left: no flush yet */
        } else {

            cbOutputBudget += cbRun - cbCopy;              
        }

        cbExpandBytes = (DWORD)(pDst - pExpandBuf);

        if (dwFileIndex < 3 && g_pfStandardStreamRedirect != NULL)  
            cbWritten = g_pfStandardStreamRedirect(dwFileIndex, pExpandBuf,
                                                   cbExpandBytes);
        else {
            if (!WriteFile(hSlot, pExpandBuf, cbExpandBytes, &cbWritten, NULL))
                goto error;                                
        }

        if (cbWritten != cbExpandBytes)                    
            goto short_write;

        if (cbOutputBudget == 0)                           
            break;                                         

        pDst = pExpandBuf;                                 
    }

    cbResult = cbBufferSize;                               
    goto done;

short_write:                                               
    /* Convert the bytes WriteFile refused back into INPUT bytes: every LF in the
     * unwritten region is the extra CR that the expansion added, so subtract them. */
    {
        DWORD cbPending = cbExpandBytes - cbWritten;
        WORD  wPending  = (WORD)cbPending;

        pScan      = pExpandBuf + cbWritten + 1;
        for (cbScanLeft = cbPending - 1; cbScanLeft != 0; cbScanLeft--, pScan++)
            if (*pScan == '\n')
                wPending--;                                

        cbResult = cbBufferSize - cbOutputBudget - (DWORD)wPending;  
        if (cbResult == 0) {                               
            SetThreadContextValueAndClearFlag(0, 0, STREAM_ENOSPC);
            cbResult = (DWORD)-1;                          
        }
    }

done:                                                     
    GlobalFree(pExpandBuf);
    return (int)cbResult;                                  

plain_write:                                               
    cbWritten = 0;
    if (dwFileIndex < 3 && g_pfStandardStreamRedirect != NULL)      
        cbWritten = g_pfStandardStreamRedirect(dwFileIndex, lpBuffer, cbBufferSize);
    else {
        if (!WriteFile(hSlot, lpBuffer, cbBufferSize, &cbWritten, NULL))
            goto error;                                    
    }
    if (cbWritten == 0) {                                  
        SetThreadContextValueAndClearFlag(0, 0, STREAM_ENOSPC);
        return -1;                                         
    }
    return (int)cbWritten;                                 

error:                                                     
    SetThreadContextValueAndClearFlag(0, 0, STREAM_EBADF);
    return -1;                                             
}
#endif /* NW_UNIT_WriteStreamChunkLfToCrLfExpanded */
