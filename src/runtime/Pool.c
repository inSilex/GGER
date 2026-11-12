/*
 * Pool -- pooled block allocation and string duplication.
 *
 * Module: runtime
 * Interface: include/Pool.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   AllocateAndZeroPooledBlockArray                            runtime/AllocateAndZeroPooledBlockArray.c
 *   AllocatePooledBlock                                        runtime/AllocatePooledBlock.c
 *   DuplicateCStringIntoPooledBlock                            runtime/DuplicateCStringIntoPooledBlock.c
 *   EnsurePooledBlockCapacity                                  runtime/EnsurePooledBlockCapacity.c
 *   GetPooledBlockUsableSize                                   runtime/GetPooledBlockUsableSize.c
 */

/* ==========================================================================
 * Unit: AllocateAndZeroPooledBlockArray
 * Source: runtime/AllocateAndZeroPooledBlockArray.c
 * ========================================================================== */
#ifdef NW_UNIT_AllocateAndZeroPooledBlockArray
/*
 * Allocate And Zero Pooled Block Array.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-allocateandzeropooledblockarray-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

void *AllocatePooledBlock(uint32_t byte_count);
void *FillMemoryWithByte(void *dest, int value, uint32_t count);

NW_DIAG void *AllocateAndZeroPooledBlockArray(uint32_t element_count,
                                               uint32_t element_size)
{
    uint32_t total = element_count * element_size;
    void *block = AllocatePooledBlock(total);

    if (block == NULL) {
        return NULL;
    }
    FillMemoryWithByte(block, 0, total);
    return block;
}
#endif /* NW_UNIT_AllocateAndZeroPooledBlockArray */

/* ==========================================================================
 * Unit: AllocatePooledBlock
 * Source: runtime/AllocatePooledBlock.c
 * ========================================================================== */
#ifdef NW_UNIT_AllocatePooledBlock
/*
 * Allocate Pooled Block.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-allocatepooledblock-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define g_wPoolLockDepth \
    (*(uint16_t *)(void *)(nw_data_window + 0x641a))
#define g_dwPoolBlockBase \
    (*(volatile uint32_t *)(void *)(nw_data_window + 0xf9c))
#define g_dwPoolCursor \
    (*(volatile uint32_t *)(void *)(nw_data_window + 0xfa0))
#define g_abyPoolClassTable \
    ((const volatile int8_t *)(const void *)(nw_data_window + 0x108c))

typedef struct NwPoolSlabEntry {
    uint32_t dwFreeHead;   /* +0: first free payload header, or NULL */
    uint32_t dwBlockSize;  /* +4: bytes consumed by one allocation */
} NwPoolSlabEntry;

void __cdecl CreateSlotSemaphore(uint32_t uiSlotIndex);
void __cdecl ReleaseSlotSemaphore(uint32_t uiSlotIndex);
void __cdecl RelinkMaskedBlockHeaderNodes(void);
void *AllocateTrackedBlock(uint32_t requested_size);

static void PoolEnter(void)
{
    if (__atomic_add_fetch(&g_wPoolLockDepth, 1u, __ATOMIC_SEQ_CST) != 0u) {
        CreateSlotSemaphore(1u);
    }
}

static void *PoolReturnAndUnlock(void *pResult)
{
    if ((int16_t)__atomic_sub_fetch(&g_wPoolLockDepth, 1u,
                                    __ATOMIC_SEQ_CST) >= 0) {
        ReleaseSlotSemaphore(1u);
    }
    return pResult;
}

NW_DIAG void *AllocatePooledBlock(uint32_t size)
{
    NwPoolSlabEntry *pEntry;
    uint32_t dwAdjusted;
    uint32_t dwClass;
    uint8_t *pBlock;
    uint32_t *pHeader;

    PoolEnter();

    dwAdjusted = size + 4u;
    if (dwAdjusted <= 8u) {
        dwAdjusted = 8u;
    }

    {
        uint32_t dwIndex = dwAdjusted - 1u;
        int32_t nClass;

        if ((dwIndex & 0xffffff00u) == 0u) {
            nClass = (int32_t)g_abyPoolClassTable[dwIndex & 0xffu];
        } else if ((dwIndex & 0xffff0000u) == 0u) {
            nClass = (int32_t)g_abyPoolClassTable[(dwIndex >> 8) & 0xffu] + 8;
        } else if ((dwIndex & 0xff000000u) == 0u) {
            nClass = (int32_t)g_abyPoolClassTable[(dwIndex >> 16) & 0xffu] + 16;
        } else {
            nClass = (int32_t)g_abyPoolClassTable[(dwIndex >> 24) & 0xffu] + 24;
        }
        dwClass = (uint32_t)nClass;
    }

    pEntry = (NwPoolSlabEntry *)(void *)(nw_data_window + 0xfa4u + (dwClass << 3));

    if (pEntry->dwFreeHead != 0u) {
        pHeader = (uint32_t *)(uintptr_t)nw_guest_to_host(pEntry->dwFreeHead);
        pEntry->dwFreeHead = pHeader[1];
        return PoolReturnAndUnlock((void *)(pHeader + 1));
    }

    if (dwAdjusted > 0x10000u) {
        pBlock = (uint8_t *)AllocateTrackedBlock(pEntry->dwBlockSize);
        if (pBlock == (uint8_t *)(uintptr_t)0xffffffffu) {
            return PoolReturnAndUnlock(NULL);
        }
        *(uint32_t *)(void *)pBlock = (uint32_t)(uintptr_t)pEntry;
        return PoolReturnAndUnlock((void *)(pBlock + 4));
    }

    if (pEntry->dwBlockSize + g_dwPoolCursor < 0x10000u) {
        pBlock = (uint8_t *)(uintptr_t)nw_guest_to_host(g_dwPoolBlockBase + g_dwPoolCursor);
        g_dwPoolCursor += pEntry->dwBlockSize;
        *(uint32_t *)(void *)pBlock = (uint32_t)(uintptr_t)pEntry;
        return PoolReturnAndUnlock((void *)(pBlock + 4));
    }

    RelinkMaskedBlockHeaderNodes();
    pBlock = (uint8_t *)AllocateTrackedBlock(0x10000u);
    g_dwPoolBlockBase = (uint32_t)(uintptr_t)pBlock;
    if (pBlock == (uint8_t *)(uintptr_t)0xffffffffu) {
        g_dwPoolBlockBase = 0u;
        g_dwPoolCursor = 0x10000u;
        return PoolReturnAndUnlock(NULL);
    }

    g_dwPoolCursor = 0u;
    pBlock = (uint8_t *)(uintptr_t)nw_guest_to_host(
        g_dwPoolBlockBase + g_dwPoolCursor);
    g_dwPoolCursor += pEntry->dwBlockSize;
    *(uint32_t *)(void *)pBlock = (uint32_t)(uintptr_t)pEntry;
    return PoolReturnAndUnlock((void *)(pBlock + 4));
}
#endif /* NW_UNIT_AllocatePooledBlock */

/* ==========================================================================
 * Unit: DuplicateCStringIntoPooledBlock
 * Source: runtime/DuplicateCStringIntoPooledBlock.c
 * ========================================================================== */
#ifdef NW_UNIT_DuplicateCStringIntoPooledBlock
/*
 * Allocates strlen(src)+1 bytes from the pooled allocator and copies the whole string, including its terminator, with the overlap-safe memory mover
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-duplicatecstringintopooledblock-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

int __cdecl GetStringLength(const char *pszStr);
void *AllocatePooledBlock(uint32_t size);
void *__cdecl MoveMemoryRegion(void *pDest, const void *pSrc,
                               uint32_t dwByteCount);

NW_DIAG char *DuplicateCStringIntoPooledBlock(const char *pszSrc)
{
    uint32_t dwBytes = (uint32_t)GetStringLength(pszSrc) + 1u;
    char *pCopy = (char *)AllocatePooledBlock(dwBytes);

    if (pCopy != NULL) {
        (void)MoveMemoryRegion(pCopy, pszSrc, dwBytes);
    }
    return pCopy;
}
#endif /* NW_UNIT_DuplicateCStringIntoPooledBlock */

/* ==========================================================================
 * Unit: EnsurePooledBlockCapacity
 * Source: runtime/EnsurePooledBlockCapacity.c
 * ========================================================================== */
#ifdef NW_UNIT_EnsurePooledBlockCapacity
/*
 * Returns a pooled block with at least min_bytes usable bytes: - NULL block: allocate a fresh block of min_bytes
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-ensurepooledblockcapacity-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

void *AllocatePooledBlock(uint32_t byte_count);
uint32_t GetPooledBlockUsableSize(void *block);
void *__cdecl MoveMemoryRegion(void *pDest, const void *pSrc,
                               uint32_t dwByteCount);
void __cdecl RelinkListNodeUnderSlotLock(void *pUnlinkSlot);

NW_DIAG void *EnsurePooledBlockCapacity(void *block, unsigned int min_bytes)
{
    uint32_t usable;
    void *pNew;

    if (block == NULL) {
        return AllocatePooledBlock((uint32_t)min_bytes);
    }
    if (min_bytes == 0u) {
        RelinkListNodeUnderSlotLock(block);
        return NULL;
    }

    usable = GetPooledBlockUsableSize(block);
    if (usable >= (uint32_t)min_bytes) {
        return block;
    }

    pNew = AllocatePooledBlock((uint32_t)min_bytes);
    if (pNew == NULL) {
        return NULL;
    }
    (void)MoveMemoryRegion(pNew, block, usable);
    RelinkListNodeUnderSlotLock(block);
    return pNew;
}
#endif /* NW_UNIT_EnsurePooledBlockCapacity */

/* ==========================================================================
 * Unit: GetPooledBlockUsableSize
 * Source: runtime/GetPooledBlockUsableSize.c
 * ========================================================================== */
#ifdef NW_UNIT_GetPooledBlockUsableSize
/*
 * Get Pooled Block Usable Size.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-getpooledblockusablesize-c
 */

#include <stdint.h>

uint32_t GetPooledBlockUsableSize(void *block)
{
    /* The slab header stores the descriptor pointer in one 32-bit guest word. */
    const uint32_t *descriptor = (const uint32_t *)(uintptr_t)
        nw_guest_to_host(*(const uint32_t *)((const uint8_t *)block - 4));
    return descriptor[1] - 4u;
}
#endif /* NW_UNIT_GetPooledBlockUsableSize */
