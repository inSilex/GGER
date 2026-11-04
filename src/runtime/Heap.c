#include "nwarch.h"
/*
 * Heap -- heap arenas, blocks, bounds, and memory fill/copy.
 *
 * Module: runtime
 * Interface: include/Heap.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   AllocHeapBlock                                             runtime/AllocHeapBlock.c
 *   AllocateHeapCursorBlock                                    runtime/AllocateHeapCursorBlock.c
 *   FillMemoryWithByte                                         runtime/FillMemoryWithByte.c
 *   FillMemoryWithDword                                        runtime/FillMemoryWithDword.c
 *   FillMemoryWithDwordFromStackArgs                           runtime/FillMemoryWithDwordFromStackArgs.c
 *   FillMemoryWithZeroBytes                                    runtime/FillMemoryWithZeroBytes.c
 *   FreeArenaSlot                                              runtime/FreeArenaSlot.c
 *   FreeGlobalHeapBlock                                        runtime/FreeGlobalHeapBlock.c
 *   FreeHeapBlock                                              runtime/FreeHeapBlock.c
 *   FreeHeapBlockIfNotNull                                     runtime/FreeHeapBlockIfNotNull.c
 *   GetHeapArenaBlockEnd                                       runtime/GetHeapArenaBlockEnd.c
 *   InitializeGlobalHeapBuffer                                 runtime/InitializeGlobalHeapBuffer.c
 *   InitializeGlobalThreadBufferOrDie                          runtime/InitializeGlobalThreadBufferOrDie.c
 *   InstallInitialHeapRegionBounds                             runtime/InstallInitialHeapRegionBounds.c
 *   MoveMemoryRegion                                           runtime/MoveMemoryRegion.c
 *   PublishHeapStateTripleFromTwoSources                       runtime/PublishHeapStateTripleFromTwoSources.c
 *   SetHeapRegionBounds                                        runtime/SetHeapRegionBounds.c
 */

/* ==========================================================================
 * Unit: AllocHeapBlock
 * Source: runtime/AllocHeapBlock.c
 * ========================================================================== */
#ifdef NW_UNIT_AllocHeapBlock
/*
 * Alloc Heap Block.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-allocheapblock-c
 */

#include <stdint.h>
#include "nwglobals.h"

extern uint32_t *g_pHeapFirstNode; 
extern uint32_t *g_pCursorBlock;   

extern uint32_t SetOutOfMemoryFlag(void); 

uint32_t *__stdcall AllocHeapBlock(uint32_t dwSizeBytes)
{
    uint32_t *pBlock;         /* node being examined; later the node being split  */
    uint32_t  dwWordsNeeded;  /* payload in words, rounded up, +1 for the header  */
    uint32_t  dwBlockHeader;  /* header word of the node under examination         */
    uint32_t  dwRemainder;    /* size in words left over after the split          */

    /* Round the byte request up to whole words, plus one header word. */
    dwWordsNeeded = dwSizeBytes >> 2;
    if ((dwSizeBytes & 3u) != 0u) {
        dwWordsNeeded += 1u;
    }
    dwWordsNeeded += 1u;

    /* Optional trace: request + heap head/cursor at entry. */
    {
        extern void nwtrace_hex(const char *tag, unsigned long v);
        nwtrace_hex("ahb_sz", (unsigned long)dwSizeBytes);
        nwtrace_hex("ahb_words", (unsigned long)dwWordsNeeded);
        nwtrace_hex("ahb_head", (unsigned long)(uintptr_t)g_pHeapFirstNode);
        nwtrace_hex("ahb_cur", (unsigned long)(uintptr_t)g_pCursorBlock);
    }

    pBlock        = g_pHeapFirstNode;
    dwBlockHeader = pBlock[0];

    /* Seek the first free node that can hold dwWordsNeeded. */
    while (((dwBlockHeader & 0x80000000u) != 0u) || (dwBlockHeader <= dwWordsNeeded)) {
        if (g_pCursorBlock <= pBlock) {   /* walked past the free region */

            {
                extern void nwtrace_hex(const char *tag, unsigned long v);
                nwtrace_hex("ahb_FAIL_sz", (unsigned long)dwSizeBytes);
                nwtrace_hex("ahb_FAIL_at", (unsigned long)(uintptr_t)pBlock);
                nwtrace_hex("ahb_FAIL_hdr", (unsigned long)dwBlockHeader);
            }
            return (uint32_t *)(uintptr_t)SetOutOfMemoryFlag(); /* NULL */
        }
        pBlock       += dwBlockHeader & 0x7fffffffu;
        dwBlockHeader = pBlock[0];
    }

    /* Split: the leftover tail becomes a new free node after the allocation. */
    dwRemainder = dwBlockHeader - dwWordsNeeded;
    if (dwRemainder != 0u) {
        pBlock[dwWordsNeeded] = dwRemainder;
        if (pBlock == g_pCursorBlock) {   /* cursor sat in the split region */
            g_pCursorBlock   = pBlock + dwWordsNeeded;
            g_dwCursorWords  = dwRemainder;
        }
    }

    pBlock[0] = dwWordsNeeded | 0x80000000u; /* claim it: size + in-use bit */

    return pBlock + 1;   /* caller gets the data word, one past the header */
}
#endif /* NW_UNIT_AllocHeapBlock */

/* ==========================================================================
 * Unit: AllocateHeapCursorBlock
 * Source: runtime/AllocateHeapCursorBlock.c
 * ========================================================================== */
#ifdef NW_UNIT_AllocateHeapCursorBlock
/*
 * Bumps the heap cursor by a 4-byte-rounded request and republishes the block-list head.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-allocateheapcursorblock-c
 */

#include <stdint.h>
#include "nwglobals.h"

extern uint32_t *g_pHeapFirstNode; 
extern uint32_t *g_pCursorBlock;   

/* Error sentinel returned when the heap is exhausted. */
extern uint32_t   g_dwAllocFailResult; 

extern uint32_t SetOutOfMemoryFlag(void); 

extern uint32_t __stdcall AllocateHeapCursorBlock(uint32_t dwSizeBytes) __asm__("_AllocateHeapCursorBlock");

uint32_t __stdcall AllocateHeapCursorBlock(uint32_t dwSizeBytes)
{
    uint32_t dwBlockWords; /* dwSizeBytes rounded to 4 and expressed in words */

    if ((dwSizeBytes & 3u) != 0u) {
        dwSizeBytes += 4u;
    }
    dwBlockWords = dwSizeBytes >> 2;

    /* The recovered command stream can publish a stale 32-bit cursor into the
     * aliased scalar slot.  The list head is republished by every successful
     * bumper, so recover the cursor from it instead of writing through a
     * pointer outside the guest heap. */
    if (g_pCursorBlock != g_pHeapFirstNode) {
        g_pCursorBlock = g_pHeapFirstNode;
    }
    if (dwBlockWords >= g_dwCursorWords) {
        (void)SetOutOfMemoryFlag();
        return g_dwAllocFailResult;
    }

    g_dwCursorWords -= dwBlockWords;              /* charge the request         */
    g_pCursorBlock += dwBlockWords;               /* bump cursor by the words  */
    g_pHeapFirstNode = g_pCursorBlock;            /* republish list head       */
    return (uint32_t)(uintptr_t)g_pCursorBlock;
}
#endif /* NW_UNIT_AllocateHeapCursorBlock */

/* ==========================================================================
 * Unit: FillMemoryWithByte
 * Source: runtime/FillMemoryWithByte.c
 * ========================================================================== */
#ifdef NW_UNIT_FillMemoryWithByte
/*
 * Fills a buffer with one byte and returns the destination buffer.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-fillmemorywithbyte-c
 */

#include <stdint.h>

extern void FillMemoryWithDword(unsigned int dwCount, unsigned int dwFill,
                                void *pDst);

void *FillMemoryWithByte(void *pDst, unsigned char bFill, unsigned int dwCount)
{
    unsigned char *pCursor = (unsigned char *)pDst;
    unsigned char *const pStart = pCursor;
    unsigned int dwRemaining;

    if (dwCount >= 8) {
        unsigned int dwPattern;

        /* Broadcast bFill into all four bytes for the dword bulk fill. */
        dwPattern = (unsigned int)bFill | ((unsigned int)bFill << 8) |
                    ((unsigned int)bFill << 16) | ((unsigned int)bFill << 24);

        /* Head: single-byte stores until the cursor is dword aligned. */
        while (((uintptr_t)pCursor & (uintptr_t)3) != 0ul) {
            *pCursor++ = bFill;
            dwCount--;
        }

        dwRemaining = dwCount;
        FillMemoryWithDword(dwCount >> 2, dwPattern, pCursor);
        pCursor += (dwRemaining >> 2) * 4u;
        dwCount = dwRemaining & 3;
    }

    /* Tail: whatever bytes remain (also the only loop when dwCount < 8). */
    while (dwCount != 0) {
        *pCursor++ = bFill;
        dwCount--;
    }

    return pStart;
}
#endif /* NW_UNIT_FillMemoryWithByte */

/* ==========================================================================
 * Unit: FillMemoryWithDword
 * Source: runtime/FillMemoryWithDword.c
 * ========================================================================== */
#ifdef NW_UNIT_FillMemoryWithDword
/*
 * Fills a caller-supplied destination buffer with dwCount copies of a 32-bit pattern: writes 8 dwords (32 bytes) per loop iteration.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-fillmemorywithdword-c
 */

void FillMemoryWithDword(unsigned int dwCount, unsigned int dwFill, void *pDst)
{
    unsigned int *pdwDst = (unsigned int *)pDst;
    unsigned int i;

    /* Fast path: eight dwords (32 bytes) per iteration. */
    for (i = dwCount >> 3; i != 0; i--) {
        pdwDst[0] = dwFill;
        pdwDst[1] = dwFill;
        pdwDst[2] = dwFill;
        pdwDst[3] = dwFill;
        pdwDst[4] = dwFill;
        pdwDst[5] = dwFill;
        pdwDst[6] = dwFill;
        pdwDst[7] = dwFill;
        pdwDst += 8;
    }

    /* Tail: the remaining dwCount & 7 dwords. */
    for (i = dwCount & 7; i != 0; i--) {
        *pdwDst++ = dwFill;
    }
}
#endif /* NW_UNIT_FillMemoryWithDword */

/* ==========================================================================
 * Unit: FillMemoryWithDwordFromStackArgs
 * Source: runtime/FillMemoryWithDwordFromStackArgs.c
 * ========================================================================== */
#ifdef NW_UNIT_FillMemoryWithDwordFromStackArgs
/*
 * Fills a buffer with a dword pattern supplied on the stack.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-fillmemorywithdwordfromstackargs-c
 */

extern void FillMemoryWithDword(unsigned int dwCount, unsigned int dwFill,
                                void *pDst);

void *FillMemoryWithDwordFromStackArgs(void *pDst, unsigned int dwFill,
                                       unsigned int dwCount)
{
    FillMemoryWithDword(dwCount, dwFill, pDst);

    return pDst;
}
#endif /* NW_UNIT_FillMemoryWithDwordFromStackArgs */

/* ==========================================================================
 * Unit: FillMemoryWithZeroBytes
 * Source: runtime/FillMemoryWithZeroBytes.c
 * ========================================================================== */
#ifdef NW_UNIT_FillMemoryWithZeroBytes
/*
 * Fill Memory With Zero Bytes.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-fillmemorywithzerobytes-c
 */

extern unsigned char *FillMemoryWithByte(void *pDst, unsigned char bFill,
                                         unsigned int dwCount);

unsigned char *__stdcall FillMemoryWithZeroBytes(unsigned int dwCount,
                                                 unsigned char *pDst)
{
    return FillMemoryWithByte(pDst, 0u, dwCount);
}
#endif /* NW_UNIT_FillMemoryWithZeroBytes */

/* ==========================================================================
 * Unit: FreeArenaSlot
 * Source: runtime/FreeArenaSlot.c
 * ========================================================================== */
#ifdef NW_UNIT_FreeArenaSlot
/*
 * Frees a slot in the custom-heap arena by clearing its status byte; if it was the last occupied slot at the tail of the region.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-freearenaslot-c
 */

#include <stdint.h>

extern int32_t g_dwHeapRegionEndSentinel;

extern uint8_t *g_pSlotsTop;

/* Resolved callee: row 54. Publishes a new region-end sentinel and recomputes size-in-words. */
extern void __stdcall SetHeapRegionBounds(int32_t pHeapRegionEnd) __asm__("_SetHeapRegionBounds");

void FreeArenaSlot(uint8_t *pSlotToFree)
{
    uint8_t *pbCandidate;

    *pSlotToFree = 0u;   /* mark slot free (clear in-use bit) */

    if ((int32_t)(uintptr_t)(pSlotToFree - 0x10) == g_dwHeapRegionEndSentinel) {
        do {
            pbCandidate = pSlotToFree;
            if (pbCandidate == (uint8_t *)(uintptr_t)g_pSlotsTop) break;
            pSlotToFree = pbCandidate + 0x10;
        } while ((*pSlotToFree & 1u) == 0u);

        SetHeapRegionBounds((int32_t)(uintptr_t)pbCandidate);   /* shrink region to first free slot */
    }
}
#endif /* NW_UNIT_FreeArenaSlot */

/* ==========================================================================
 * Unit: FreeGlobalHeapBlock
 * Source: runtime/FreeGlobalHeapBlock.c
 * ========================================================================== */
#ifdef NW_UNIT_FreeGlobalHeapBlock
/*
 * Free Global Heap Block.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-freeglobalheapblock-c
 */

#include <stdint.h>

extern uint32_t *g_pGlobalBuffer;              /* window +0x1614 */
extern void __stdcall FreeHeapBlock(uint8_t *pData);

void FreeGlobalHeapBlock(void)
{
    FreeHeapBlock((uint8_t *)g_pGlobalBuffer);
}
#endif /* NW_UNIT_FreeGlobalHeapBlock */

/* ==========================================================================
 * Unit: FreeHeapBlock
 * Source: runtime/FreeHeapBlock.c
 * ========================================================================== */
#ifdef NW_UNIT_FreeHeapBlock
/*
 * Free Heap Block.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-freeheapblock-c
 */

#include <stdint.h>
#include "nwglobals.h"

extern uint32_t *g_pHeapFirstNode; 
extern uint32_t *g_pCursorBlock;   

void __stdcall FreeHeapBlock(uint8_t *pData)
{
    uint32_t *pPrevBlock;     /* node preceding the freed one in the list        */
    uint32_t *pBlock;         /* current node (reassigned to prev if merged left)*/
    uint32_t *pNextFreeCand;  /* next-node ptr, only when that node is free      */
    uint32_t dwNextHdrWord;   /* header word of the following node               */
    uint32_t dwMergedWords;   /* size in words accumulated from both merges      */

    pPrevBlock = NULL;

    /* Walk the list (stride = low bits of each header) until reaching the node
       whose data starts at pData. */
    for (pBlock = g_pHeapFirstNode;
         (uint8_t *)pData - 4 != (uint8_t *)pBlock;) {
        pPrevBlock = pBlock;
        pBlock += pBlock[0] & 0x7fffffffu;
    }

    dwMergedWords = pBlock[0] & 0x7fffffffu;   /* size of this block in words */

    /* Merge right: if the following node is free, absorb it. */
    pNextFreeCand = NULL;
    dwNextHdrWord = pBlock[pBlock[0] & 0x7fffffffu];
    if ((dwNextHdrWord & 0x80000000u) == 0u) {
        dwMergedWords += dwNextHdrWord;
        pNextFreeCand = pBlock + (pBlock[0] & 0x7fffffffu);
    }

    /* Merge left: if the preceding node is free, absorb it and inherit its position. */
    if ((pPrevBlock != NULL) && ((pPrevBlock[0] & 0x80000000u) == 0u)) {
        dwMergedWords += pPrevBlock[0];
        pBlock = pPrevBlock;
    }

    if (pNextFreeCand == g_pCursorBlock) {
        g_pCursorBlock = pBlock;
        g_dwCursorWords = dwMergedWords;
    }

    *pBlock = dwMergedWords;   /* publish merged size, clears the in-use bit  */
}
#endif /* NW_UNIT_FreeHeapBlock */

/* ==========================================================================
 * Unit: FreeHeapBlockIfNotNull
 * Source: runtime/FreeHeapBlockIfNotNull.c
 * ========================================================================== */
#ifdef NW_UNIT_FreeHeapBlockIfNotNull
/*
 * Free Heap Block If Not Null.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-freeheapblockifnotnull-c
 */

typedef unsigned char byte;

void __stdcall FreeHeapBlock(byte *pData);        

void FreeHeapBlockIfNotNull(byte *pData)
{
    if (pData != 0) {
        FreeHeapBlock(pData);
    }
}
#endif /* NW_UNIT_FreeHeapBlockIfNotNull */

/* ==========================================================================
 * Unit: GetHeapArenaBlockEnd
 * Source: runtime/GetHeapArenaBlockEnd.c
 * ========================================================================== */
#ifdef NW_UNIT_GetHeapArenaBlockEnd
/*
 * Get Heap Arena Block End.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-getheaparenablockend-c
 */

#include <stdint.h>
extern unsigned char nw_data_window[];

extern uint8_t g_abHeapScratchBuf[152];

extern int32_t g_dwHeapRegionEndSentinel;

extern uint8_t *g_pHeapSlotBase;

extern int32_t g_dwHeapRegionSizeInWords;

/* Blocks up to 0x98 bytes are served from the scratch buffer above, so that a small
 * request never disturbs the arena tail bookkeeping. */
#define HEAP_SCRATCH_BUF_ADDR (nw_data_window + 0x1618)
#define HEAP_SCRATCH_MAX_SIZE 0x98u

void * __stdcall GetHeapArenaBlockEnd(uint32_t dwRequestSize)
{
    uint32_t guest_end;

    if (dwRequestSize <= HEAP_SCRATCH_MAX_SIZE) {
        return (void *)HEAP_SCRATCH_BUF_ADDR;
    }

    if (g_dwHeapRegionEndSentinel != 0) {
        guest_end = (uint32_t)g_dwHeapRegionEndSentinel - dwRequestSize;
    } else if (g_pHeapSlotBase != NULL) {
        /*
         * The original performs this addition in 32 bits and wraps.  The
         * heap-capacity words intentionally share the arena-size slot, so a
         * 64-bit intermediate would leave the low 4 GiB and produce a wild
         * pointer.  Keep the guest arithmetic width explicit.
         */
        guest_end = (uint32_t)(uintptr_t)g_pHeapSlotBase
                  + (uint32_t)g_dwHeapRegionSizeInWords * 4u
                  - dwRequestSize;
    } else {
        return NULL;        /* arena uninitialized: no valid end address */
    }

    return (void *)(uintptr_t)guest_end;
}
#endif /* NW_UNIT_GetHeapArenaBlockEnd */

/* ==========================================================================
 * Unit: InitializeGlobalHeapBuffer
 * Source: runtime/InitializeGlobalHeapBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeGlobalHeapBuffer
/*
 * Initialize Global Heap Buffer.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-initializeglobalheapbuffer-c
 */

#include <stdint.h>

/* Module-owned globals (see AllocHeapBlock.c / FreeHeapBlock.c for the heap contract): */
extern uint32_t *g_pHeapFirstNode; 
extern uint32_t  g_dwBufferCount;  
extern uint32_t  g_dwMinBufferLen; 
extern uint32_t *g_pGlobalBuffer;  

extern uint32_t *__stdcall AllocHeapBlock(uint32_t dwSizeBytes); 

void InitializeGlobalHeapBuffer(void)
{
    uint32_t dwSizeBytes; /* byte size handed to the heap allocator */
    uint32_t dwHeaderLen; /* fixed 0x14-byte (20-byte) structure overhead */

    /* The buffer holds a 20-byte header followed by one 4-byte slot per item. */
    dwHeaderLen = 20u;
    dwSizeBytes = g_dwBufferCount * 4u + dwHeaderLen;

    /* Clamp upward: never allocate less than the configured minimum. */
    if (dwSizeBytes < g_dwMinBufferLen) {
        dwSizeBytes = g_dwMinBufferLen;
    }

    /* Stores NULL unchanged if the heap is exhausted. */
    g_pGlobalBuffer = AllocHeapBlock(dwSizeBytes);
}
#endif /* NW_UNIT_InitializeGlobalHeapBuffer */

/* ==========================================================================
 * Unit: InitializeGlobalThreadBufferOrDie
 * Source: runtime/InitializeGlobalThreadBufferOrDie.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeGlobalThreadBufferOrDie
/*
 * Initialize Global Thread Buffer Or Die.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-initializeglobalthreadbufferordie-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define g_pGlobalThreadBuffer \
    NW_WINDOW_PTR_REF(uint8_t, 0xe84)

extern void *AllocateAndZeroPooledBlockArray(uint32_t element_count,
                                             uint32_t element_size);
extern void WriteStdoutStringAndExit(char *szMessage)
    __attribute__((noreturn));

NW_DIAG void *InitializeGlobalThreadBufferOrDie(void)
{
    uint8_t *pBuffer = AllocateAndZeroPooledBlockArray(0x1004u, 1u);

    if (pBuffer == 0) {
        WriteStdoutStringAndExit((char *)(void *)(nw_data_window + 0xe88));
    }

    g_pGlobalThreadBuffer = pBuffer;
    *(uint32_t *)(void *)(pBuffer + 4) =
        (uint32_t)(uintptr_t)(nw_data_window + 0xd23);
    return pBuffer;
}
#endif /* NW_UNIT_InitializeGlobalThreadBufferOrDie */

/* ==========================================================================
 * Unit: InstallInitialHeapRegionBounds
 * Source: runtime/InstallInitialHeapRegionBounds.c
 * ========================================================================== */
#ifdef NW_UNIT_InstallInitialHeapRegionBounds
/*
 * Installs the initial heap-region chain top.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-installinitialheapregionbounds-c
 */

#include <stdint.h>
#include "nwglobals.h"

extern void __stdcall SetHeapRegionBounds(int32_t pHeapRegionEnd);
extern uint8_t *g_pHeapSlotBase;

void InstallInitialHeapRegionBounds(void)
{
    int32_t end = (int32_t)(uintptr_t)g_pHeapSlotBase
                + (int32_t)(g_dwCursorWords * 4u) - 0x10;

    /* The recovered slot at +0x6240 is the slot-chain top pointer.  Keeping it
     * in the native pointer table matches the 64-bit window model. */
    NW_WINDOW_PTR_REF(uint8_t, 0x6240) = (uint8_t *)(uintptr_t)end;
    SetHeapRegionBounds(end);
}
#endif /* NW_UNIT_InstallInitialHeapRegionBounds */

/* ==========================================================================
 * Unit: MoveMemoryRegion
 * Source: runtime/MoveMemoryRegion.c
 * ========================================================================== */
#ifdef NW_UNIT_MoveMemoryRegion
/*
 * Copies an overlapping memory region and returns the destination.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-movememoryregion-c
 */

#include <stdint.h>

typedef int __cdecl_int; /* calling-convention marker only */

void *__cdecl MoveMemoryRegion(void *pDest, const void *pSrc, uint32_t dwByteCount)
{
    uint8_t       *pbDst = (uint8_t *)pDest;
    const uint8_t *pbSrc = (const uint8_t *)pSrc;

    if ((dwByteCount == 0u) || (pbDst == pbSrc)) {
        return pDest;
    }

    /* [pbSrc, srcEnd) and [pbDst, ...) overlap: copy from the high end down. */
    if ((pbSrc < pbDst) && (pbDst < pbSrc + dwByteCount)) {
        uint8_t       *pbDstTail = pbDst + dwByteCount;  /* one past last byte */
        const uint8_t *pbSrcTail = pbSrc + dwByteCount;

        while (((uintptr_t)pbDstTail & 3u) != 0u &&         /* peel to DWORD alignment, */
               ((size_t)(pbDstTail - pbDst) >= 4u)) {       /*   but never below the region */
            --pbDstTail;
            --pbSrcTail;
            *pbDstTail = *pbSrcTail;
        }

        while ((size_t)(pbDstTail - pbDst) >= 4u) {         /* DWORD chunks, high to low */
            *(uint32_t *)(pbDstTail - 4u) = *(const uint32_t *)(pbSrcTail - 4u);
            pbDstTail -= 4;
            pbSrcTail -= 4;
        }

        while (pbDstTail > pbDst) {                         /* trailing bytes */
            --pbDstTail;
            --pbSrcTail;
            *pbDstTail = *pbSrcTail;
        }
    } else {
        /* No overlap: copy left-to-right. */

        while (((uintptr_t)pbSrc & 3u) != 0u && (dwByteCount >= 4u)) {
            *pbDst = *pbSrc;                                /* byte-wise to DWORD alignment */
            ++pbDst;
            ++pbSrc;
            --dwByteCount;
        }

        for (uint32_t dwWords = dwByteCount >> 2; dwWords != 0u; --dwWords) {
            *(uint32_t *)pbDst = *(const uint32_t *)pbSrc;  /* DWORD chunks */
            pbDst += 4;
            pbSrc += 4;
        }

        for (uint32_t dwRemain = dwByteCount & 3u; dwRemain != 0u; --dwRemain) {
            *pbDst = *pbSrc;                                /* trailing bytes */
            ++pbDst;
            ++pbSrc;
        }
    }

    return pDest;
}
#endif /* NW_UNIT_MoveMemoryRegion */

/* ==========================================================================
 * Unit: PublishHeapStateTripleFromTwoSources
 * Source: runtime/PublishHeapStateTripleFromTwoSources.c
 * ========================================================================== */
#ifdef NW_UNIT_PublishHeapStateTripleFromTwoSources
/*
 * Publish Heap State Triple From Two Sources.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-publishheapstatetriplefromtwosources-c
 */

#include <stdint.h>
#include "nwglobals.h"

extern uint32_t g_dwFramebufferArenaBytes;

extern uint32_t g_dwHeapCursorSource;

extern uint32_t g_dwHeapListHead;      
extern uint32_t g_dwHeapCapacityWords; 
extern uint32_t *g_pHeapFirstNode;
extern uint32_t *g_pCursorBlock;

uint32_t PublishHeapStateTripleFromTwoSources(void)
{
    g_dwHeapCapacityWords = g_dwFramebufferArenaBytes >> 2u;

    g_pHeapFirstNode = (uint32_t *)(uintptr_t)g_dwHeapCursorSource;
    g_pCursorBlock   = (uint32_t *)(uintptr_t)g_dwHeapCursorSource;
    return g_dwHeapCursorSource;
}
#endif /* NW_UNIT_PublishHeapStateTripleFromTwoSources */

/* ==========================================================================
 * Unit: SetHeapRegionBounds
 * Source: runtime/SetHeapRegionBounds.c
 * ========================================================================== */
#ifdef NW_UNIT_SetHeapRegionBounds
/*
 * Set Heap Region Bounds.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-setheapregionbounds-c
 */

#include <stdint.h>

extern int32_t g_dwHeapRegionEndSentinel;

extern uint8_t *g_pHeapSlotBase;

extern int32_t g_dwHeapRegionSizeInWords;

void __stdcall SetHeapRegionBounds(int32_t) __asm__("_SetHeapRegionBounds");
void __stdcall SetHeapRegionBounds(int32_t pHeapRegionEnd)
{
    int32_t dwSizeInWords;

    /* Publish the new region-end sentinel (walk loops compare against this). */
    g_dwHeapRegionEndSentinel = pHeapRegionEnd;

    dwSizeInWords = (int32_t)((uint32_t)pHeapRegionEnd -
                              (uint32_t)(uintptr_t)g_pHeapSlotBase) / 4 - 4;

    g_dwHeapRegionSizeInWords = dwSizeInWords;   

    *(int32_t *)(uintptr_t)g_pHeapSlotBase = dwSizeInWords; /* free-node header word */
}
#endif /* NW_UNIT_SetHeapRegionBounds */
