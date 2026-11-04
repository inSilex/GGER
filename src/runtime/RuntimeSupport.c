#include "nwarch.h"
/*
 * RuntimeSupport -- token arrays, block relinking, and format scanning.
 *
 * Module: runtime
 * Interface: include/RuntimeSupport.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   BuildTokenPointerArrayFromGlobalString                     runtime/BuildTokenPointerArrayFromGlobalString.c
 *   RelinkBlockHeaderNode                                      runtime/RelinkBlockHeaderNode.c
 *   RelinkListNodeUnderSlotLock                                runtime/RelinkListNodeUnderSlotLock.c
 *   RelinkMaskedBlockHeaderNodes                               runtime/RelinkMaskedBlockHeaderNodes.c
 *   ScanFourDecimalFieldsToByteOrWordGlobals                   runtime/ScanFourDecimalFieldsToByteOrWordGlobals.c
 *   ScanStringFormatToVarArgs                                  runtime/ScanStringFormatToVarArgs.c
 */

/* ==========================================================================
 * Unit: BuildTokenPointerArrayFromGlobalString
 * Source: runtime/BuildTokenPointerArrayFromGlobalString.c
 * ========================================================================== */
#ifdef NW_UNIT_BuildTokenPointerArrayFromGlobalString
/*
 * Build Token Pointer Array From Global String.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-buildtokenpointerarrayfromglobalstring-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

extern unsigned char nw_data_window[] __asm__("nw_data_window");

#define g_pszTokenSource \
    NW_WINDOW_PTR_REF(char, 0xd0d)
#define g_apTokenPointers \
    NW_WINDOW_PTR_REF(void *, 0x1198)

int __cdecl GetStringLength(const char *pszStr);
void *AllocatePooledBlock(uint32_t size);
void *EnsurePooledBlockCapacity(void *block, unsigned int min_bytes);
char *DuplicateCStringIntoPooledBlock(const char *pszSrc);

NW_DIAG int BuildTokenPointerArrayFromGlobalString(void)
{
    const char *pToken = g_pszTokenSource;
    void **pArray;
    uint32_t dwBytes;
    uint32_t i;

    {
        uint32_t dwSlots = 1u;
        const char *pWalk = pToken;

        while (*pWalk != '\0') {
            ++dwSlots;
            pWalk += GetStringLength(pWalk) + 1;
        }
        dwBytes = dwSlots << 2;
    }

    if (g_apTokenPointers == NULL) {
        pArray = (void **)AllocatePooledBlock(dwBytes);
    } else {
        pArray = (void **)EnsurePooledBlockCapacity(g_apTokenPointers, dwBytes);
    }

    if (pArray == NULL) {
        return -1;
    }

    g_apTokenPointers = pArray;

    i = 0u;
    pToken = g_pszTokenSource;
    while (*pToken != '\0') {
        char *pCopy = DuplicateCStringIntoPooledBlock(pToken);
        if (pCopy == NULL) {
            return -1;
        }
        pArray[i++] = (void *)pCopy;
        pToken += GetStringLength(pToken) + 1;
    }
    pArray[i] = NULL;
    return 0;
}
#endif /* NW_UNIT_BuildTokenPointerArrayFromGlobalString */

/* ==========================================================================
 * Unit: RelinkBlockHeaderNode
 * Source: runtime/RelinkBlockHeaderNode.c
 * ========================================================================== */
#ifdef NW_UNIT_RelinkBlockHeaderNode
/*
 * Relink Block Header Node.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-relinkblockheadernode-c
 */

#include <stdint.h>

/* Two-word link header sitting immediately below the caller's block. */
typedef struct BlockHeaderNode {
    uint32_t link0; /* +0 : read on the node, read+written on p */
    uint32_t link1; /* +4 : written on the node */
} BlockHeaderNode;

void RelinkBlockHeaderNode(uintptr_t block)
{
    uintptr_t node = block - 4u; /* 32-bit wrap matches the SUB/JC guard */

    if (node < 4u) {             /* unsigned borrow: header below address 4 */
        return;                  /* nothing to relink */
    }

    uintptr_t p = (uintptr_t)((BlockHeaderNode *)node)->link0;
    uint32_t q = ((BlockHeaderNode *)p)->link0;

    ((BlockHeaderNode *)p)->link0 = (uint32_t)node; /* p now points at node */
    ((BlockHeaderNode *)node)->link1 = q;           /* node takes over q   */
}
#endif /* NW_UNIT_RelinkBlockHeaderNode */

/* ==========================================================================
 * Unit: RelinkListNodeUnderSlotLock
 * Source: runtime/RelinkListNodeUnderSlotLock.c
 * ========================================================================== */
#ifdef NW_UNIT_RelinkListNodeUnderSlotLock
/*
 * Relink List Node Under Slot Lock.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-relinklistnodeunderslotlock-c
 */

#include <stdint.h>
#include <windows.h>
#include "nwglobals.h"

#define g_wSlot1LockDepth (*(uint16_t *)(nw_data_window + 0x641A))

void __cdecl CreateSlotSemaphore(uint32_t uiSlotIndex);

void __cdecl ReleaseSlotSemaphore(uint32_t uiSlotIndex);

#define NW_DIAG __attribute__((section(".nwdiag")))

NW_DIAG void __cdecl RelinkListNodeUnderSlotLock(void *pUnlinkSlot)
{

    if (__atomic_add_fetch(&g_wSlot1LockDepth, 1u, __ATOMIC_SEQ_CST) != 0u) {
        CreateSlotSemaphore(1u);
    }

    /* Guarded inline unlink (SUB EAX,4 / JBE skips the whole splice when the
     * parameter is <= 4).  EAX = parameter - 4 is the value stored back. */
    if ((uintptr_t)pUnlinkSlot > 4u) {
        void *pBack = (void *)((char *)pUnlinkSlot - 4);     /* EAX */
        void *pPrevField = *(void **)pBack;                  /* EDX = [EAX] */
        void *pNextField = *(void **)pPrevField;             /* ECX = [EDX] */
        *(void **)pPrevField = pBack;                        /* [EDX] = EAX */
        *(void **)pUnlinkSlot = pNextField;                  /* [EAX+4] = ECX */
    }

    if ((int16_t)__atomic_sub_fetch(&g_wSlot1LockDepth, 1u,
                                    __ATOMIC_SEQ_CST) >= 0) {
        ReleaseSlotSemaphore(1u);
    }
}
#endif /* NW_UNIT_RelinkListNodeUnderSlotLock */

/* ==========================================================================
 * Unit: RelinkMaskedBlockHeaderNodes
 * Source: runtime/RelinkMaskedBlockHeaderNodes.c
 * ========================================================================== */
#ifdef NW_UNIT_RelinkMaskedBlockHeaderNodes
/*
 * Relink Masked Block Header Nodes.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-relinkmaskedblockheadernodes-c
 */

#include <stdint.h>
extern unsigned char nw_data_window[];

/* Absolute globals, as read by the body. */
#define BLOCK_ARRAY_BASE (*(volatile uint32_t *)(nw_data_window + 0xf9c))
#define BLOCK_PARAM      (*(volatile uint32_t *)(nw_data_window + 0xfa0))
#define BLOCK_ENTRY_TABLE ((uintptr_t)(nw_data_window + 0xfa4))

/* Two-word link header; relinked in place by RelinkBlockHeaderNode. */
typedef struct BlockHeaderNode {
    uint32_t link0; /* +0 : preset by this function to &entry[k] */
    uint32_t link1; /* +4 : written by the callee with the displaced link */
} BlockHeaderNode;

/* One 8-byte entry of the table at (nw_data_window + 0xfa4). */
typedef struct BlockEntry {
    uint32_t node;   /* +0 : head node of the chain this entry replaces */
    uint32_t stride; /* +4 : bytes the cursor advances past this entry */
} BlockEntry;

void RelinkBlockHeaderNode(uintptr_t block);

void RelinkMaskedBlockHeaderNodes(void)
{
    uintptr_t cursor = (uintptr_t)BLOCK_ARRAY_BASE + BLOCK_PARAM;
    uintptr_t entry = BLOCK_ENTRY_TABLE;
    uint32_t mask = 0x10000u - BLOCK_PARAM; /* SAR, but the mask stays positive */

    while (mask != 0u) {
        if ((mask & 0x8u) != 0u) {       /* bit 3 of the shifted mask */
            uint32_t *header = (uint32_t *)cursor;
            header[0] = (uint32_t)entry;  /* MOV [EDI],ESI */
            RelinkBlockHeaderNode((uintptr_t)(cursor + 4u));
            cursor += ((const BlockEntry *)entry)->stride;
        }
        entry += 8u;                     /* ADD ESI,0x8 */
        mask >>= 1;
    }
}
#endif /* NW_UNIT_RelinkMaskedBlockHeaderNodes */

/* ==========================================================================
 * Unit: ScanFourDecimalFieldsToByteOrWordGlobals
 * Source: runtime/ScanFourDecimalFieldsToByteOrWordGlobals.c
 * ========================================================================== */
#ifdef NW_UNIT_ScanFourDecimalFieldsToByteOrWordGlobals
/*
 * Scan Four Decimal Fields To Byte Or Word Globals.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-scanfourdecimalfieldstobyteorwordglobals-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */

extern int __cdecl ScanStringFormatToVarArgs(char *pszInput, char *pszFormat, ...);

/* ------------------------------------------------------------------ */
/* Globals. Addressed by their absolute PE addresses rather than by     */
/* invented extern symbol names, so the literals stay visible and no    */
/* 32-bit-global pointer arithmetic is needed.                          */
/* ------------------------------------------------------------------ */

#define PFMT_FOUR_DECIMALS  "%d %d %d %d"

/* Byte path targets (mode 0), four consecutive bytes. */
#define PUB_BYTE_0  ((BYTE  *)(uintptr_t)(nw_data_window + 0x6358))   /* frame slot [ESP+0] */
#define PUB_BYTE_1  ((BYTE  *)(uintptr_t)(nw_data_window + 0x6359))   /* frame slot [ESP+4] */
#define PUB_BYTE_2  ((BYTE  *)(uintptr_t)(nw_data_window + 0x635a))   /* frame slot [ESP+8] */
#define PUB_BYTE_3  ((BYTE  *)(uintptr_t)(nw_data_window + 0x635b))   /* frame slot [ESP+c] */
/* Word path targets (mode 1), three consecutive 16-bit slots. Pre-initialised
 * elsewhere to 0x0010, 0x0050 and 0xffff. */
#define PUB_WORD_0  ((WORD *)(uintptr_t)(nw_data_window + 0x6352))    /* frame slot [ESP+0] */
#define PUB_WORD_1  ((WORD *)(uintptr_t)(nw_data_window + 0x6354))    /* frame slot [ESP+4] */
#define PUB_WORD_2  ((WORD *)(uintptr_t)(nw_data_window + 0x6356))    /* frame slot [ESP+8] */

/* Two publish shapes selected by the mode word. */
#define PUBLISH_MODE_BYTES  0u
#define PUBLISH_MODE_WORDS  1u

/* ------------------------------------------------------------------ */
/* Function                                                            */
/* ------------------------------------------------------------------ */

/*
 * Scan up to four space-separated decimal integers out of pszInput and publish
 * the low byte of each (mode 0) or the low word of the first three (mode 1).
 * pszInput is consumed by the caller's own cursor: ScanStringFormatToVarArgs
 * hands &pszInput to the engine as its stream handle, so the thunks advance the
 * CALLER's pointer. That side effect is invisible here because pszInput is a
 * by-value copy of the caller's pointer.
 */
int __stdcall ScanFourDecimalFieldsToByteOrWordGlobals(DWORD dwPublishMode,
                                                      char  *pszInput)
{
    int aiFields[4];        /* frame [ESP+0..0xc]: four scanned integers */
    int iAssigned;          /* scanf result - read by nobody */

    aiFields[1] = 0;
    aiFields[2] = 0;
    aiFields[3] = 0;

    iAssigned = ScanStringFormatToVarArgs(pszInput, PFMT_FOUR_DECIMALS,
                                          &aiFields[0], &aiFields[1],
                                          &aiFields[2], &aiFields[3]);

    (void)iAssigned;

    if (dwPublishMode == PUBLISH_MODE_BYTES) {
        *PUB_BYTE_0 = (BYTE)aiFields[0];
        *PUB_BYTE_1 = (BYTE)aiFields[1];
        *PUB_BYTE_2 = (BYTE)aiFields[2];
        *PUB_BYTE_3 = (BYTE)aiFields[3];
    } else if (dwPublishMode == PUBLISH_MODE_WORDS) {
        /* The fourth scanned integer has no 16-bit target and is dropped. */
        *PUB_WORD_0 = (WORD)aiFields[0];
        *PUB_WORD_1 = (WORD)aiFields[1];
        *PUB_WORD_2 = (WORD)aiFields[2];
    }
    /* Any other mode falls straight through to the epilogue, storing nothing,
     * so whichever set was written by the previous run stays live. */

    return 1;               
}
#endif /* NW_UNIT_ScanFourDecimalFieldsToByteOrWordGlobals */

/* ==========================================================================
 * Unit: ScanStringFormatToVarArgs
 * Source: runtime/ScanStringFormatToVarArgs.c
 * ========================================================================== */
#ifdef NW_UNIT_ScanStringFormatToVarArgs
/*
 * Scan String Format To Var Args.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-scanstringformattovarargs-c
 */

#include <stdarg.h>

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */

typedef int (*PFN_GETC)(void *pStream);
typedef int (*PFN_UNGETC)(int ch, void *pStream);

extern int ScanFormatConversions(PFN_UNGETC pfnUngetcChar,
                                 PFN_GETC pfnGetcChar,
                                 void *pScanStream,
                                 const char *pszFormat,
                                 void **ppVaArgs);

/* ------------------------------------------------------------------ */

/* are reproduced here rather than declared extern.                     */
/* ------------------------------------------------------------------ */

#define SCAN_EOF (-1)

static int ScanStringReadNextChar(void *pStream)
{
    char **ppszCursor = (char **)pStream;
    char chRead;

    if (**ppszCursor == '\0')
        return SCAN_EOF;

    chRead = **ppszCursor;
    (*ppszCursor)++;                        /* ff 00 - advance, then report */
    return (int)(signed char)chRead;         /* 0f be 01 */
}

static int ScanStringUnreadChar(int ch, void *pStream)
{
    char **ppszCursor = (char **)pStream;   /* 8b 4c 24 08 */
    char  *pszPos;

    if (ch == SCAN_EOF)                     /* 83 fa ff / 89 d0 - no-op */
        return SCAN_EOF;

    (*ppszCursor)--;                        /* ff 09 - step the cursor back */
    pszPos = *ppszCursor;                   /* 8b 09 - re-read it */
    *pszPos = (char)ch;                     /* 88 01 - overwrite that byte */
    return (int)(signed char)ch;            /* 0f be c0 */
}

/* ------------------------------------------------------------------ */
/* Interface                                                            */
/* ------------------------------------------------------------------ */

/* sscanf(pszInput, pszFormat, ...). The varargs are NOT named here; the
 * engine reaches them through the va_list this function forwards, which is
 * exactly the address of the first variadic slot on the stack. */
#if defined(__aarch64__)
/* The recovered implementation below is a 32-bit-style pointer walk over the
 * variadic slot array.  That is a valid description of the x86-64 va_list
 * spill area, but AArch64's va_list is an opaque descriptor, not a contiguous
 * array of arguments.  Collect the pointer arguments through the ABI instead
 * and hand ScanFormatConversions the same pointer-slot array it expects. */
static int ScanCountVarArgs(const char *pszFormat)
{
    int iCount = 0;

    while (*pszFormat != '\0') {
        if (*pszFormat++ != '%')
            continue;
        if (*pszFormat == '%') {
            pszFormat++;
            continue;
        }
        if (*pszFormat == '*') {
            pszFormat++;
            continue;
        }
        iCount++;
        while (*pszFormat != '\0' &&
               *pszFormat != '%' &&
               *pszFormat != 'd' && *pszFormat != 'i' &&
               *pszFormat != 'o' && *pszFormat != 'u' &&
               *pszFormat != 'x' && *pszFormat != 'X' &&
               *pszFormat != 'b' && *pszFormat != 'p' &&
               *pszFormat != 's' && *pszFormat != 'c' &&
               *pszFormat != 'n')
            pszFormat++;
        if (*pszFormat != '\0')
            pszFormat++;
    }
    return iCount;
}
#endif

int __cdecl ScanStringFormatToVarArgs(char *pszInput, char *pszFormat, ...)
{
    va_list vaArgs;
    /* The engine's own declaration speaks in void** because it walks the slot
     * pointer four bytes at a time; va_list is char* on this target, so route
     * the cast through void* to keep both descriptions of the same pointer. */
    void **ppVaArgs;
    int iAssigned;

    va_start(vaArgs, pszFormat);            /* 0040d280: LEA EAX,[ESP+0xc] */
#if defined(__aarch64__)
    {
        void *aArgs[16];
        int iCount = ScanCountVarArgs(pszFormat);
        int i;

        if (iCount > (int)(sizeof(aArgs) / sizeof(aArgs[0])))
            iCount = (int)(sizeof(aArgs) / sizeof(aArgs[0]));
        for (i = 0; i < iCount; i++)
            aArgs[i] = va_arg(vaArgs, void *);
        for (; i < (int)(sizeof(aArgs) / sizeof(aArgs[0])); i++)
            aArgs[i] = NULL;
        ppVaArgs = aArgs;
    }
#else
    ppVaArgs = (void **)(void *)vaArgs;
#endif

    iAssigned = ScanFormatConversions(ScanStringUnreadChar,
                                      ScanStringReadNextChar,
                                      &pszInput,
                                      pszFormat,
                                      ppVaArgs);

    va_end(vaArgs);

    return iAssigned;                       /* EAX is untouched by ADD ESP/RET */
}
#endif /* NW_UNIT_ScanStringFormatToVarArgs */
