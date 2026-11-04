/*
 * Tables -- case-insensitive table lookup and wide multiply.
 *
 * Module: runtime
 * Interface: include/Tables.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   FindTableEntryValueByKeyNoCase                             runtime/FindTableEntryValueByKeyNoCase.c
 *   FindTableValueByKeyA                                       runtime/FindTableValueByKeyA.c
 *   MultiplyLow64Product                                       runtime/MultiplyLow64Product.c
 */

/* ==========================================================================
 * Unit: FindTableEntryValueByKeyNoCase
 * Source: runtime/FindTableEntryValueByKeyNoCase.c
 * ========================================================================== */
#ifdef NW_UNIT_FindTableEntryValueByKeyNoCase
/*
 * Find Table Entry Value By Key No Case.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-findtableentryvaluebykeynocase-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <stddef.h>


#define g_pcwSlotSemCounters ((volatile uint16_t *)(nw_data_window + 0x6418))

void  __cdecl CreateSlotSemaphore(uint32_t uiSlotIndex);

void  __cdecl ReleaseSlotSemaphore(uint32_t uiSlotIndex);

char * __cdecl FindFirstCharInString(char *pszString, char cSearch);

int   __cdecl GetStringLength(const char *pszString);

int   __cdecl CompareBoundedAsciiStringsNoCase(const char *pszLeft,
                                               const char *pszRight,
                                               int           nMaxCount);

/** Slot index this function guards; EDX is loaded with 2 twice. */
#define SLOT_INDEX 2

char * __cdecl FindTableEntryValueByKeyNoCase(char *pszKey)
{
    char **ppEntry;        /* EBP-0xc: cursor over g_ppszEntryTable */
    char  *pchEquals;      /* EBP-0x8: FindFirstCharInString(entry, '=') */
    char  *pszResult;      /* EBP-0x4: the value pointer, NULL until a match */
    unsigned short usAfter;

    pszResult = NULL;                       /* MOV [EBP-0x4],0 */

    usAfter = (unsigned short)__atomic_add_fetch(&g_pcwSlotSemCounters[SLOT_INDEX], 1u,
                                                 __ATOMIC_SEQ_CST);
    if (usAfter != 0u) {
        CreateSlotSemaphore(SLOT_INDEX);
    }

    if ((g_ppszEntryTable != NULL) && (pszKey != NULL)) {
        ppEntry = g_ppszEntryTable;
        for (; *ppEntry != NULL; ppEntry++) {       
            pchEquals = FindFirstCharInString(*ppEntry, '=');   /* 0x3d */
            if (pchEquals == NULL) {
                continue;
            }
            /* The key portion must be exactly strlen(pszKey) long. */
            if (GetStringLength(pszKey) != (int)(pchEquals - *ppEntry)) {
                continue;
            }

            if (CompareBoundedAsciiStringsNoCase(*ppEntry, pszKey,
                                                 GetStringLength(pszKey)) != 0) {
                continue;
            }
            pszResult = pchEquals + 1;               /* value side of the split */
            break;
        }
    }

    usAfter = (unsigned short)__atomic_sub_fetch(&g_pcwSlotSemCounters[SLOT_INDEX], 1u,
                                                 __ATOMIC_SEQ_CST);
    if ((int16_t)usAfter >= 0) {
        ReleaseSlotSemaphore(SLOT_INDEX);
    }

    return pszResult;
}
#endif /* NW_UNIT_FindTableEntryValueByKeyNoCase */

/* ==========================================================================
 * Unit: FindTableValueByKeyA
 * Source: runtime/FindTableValueByKeyA.c
 * ========================================================================== */
#ifdef NW_UNIT_FindTableValueByKeyA
/*
 * Find Table Value By Key A.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-findtablevaluebykeya-c
 */

#include <stddef.h>
#include "nwglobals.h"

#ifdef NWDEBUG_ENABLED
void nwtrace_hex(const char *, unsigned long);
#endif

extern char g_szTableKeyA[];

char * __cdecl FindTableEntryValueByKeyNoCase(char *pszKey);

char * __cdecl FindTableValueByKeyA(void)
{
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("ftvba_enter", 0);
#endif

    return FindTableEntryValueByKeyNoCase(g_szTableKeyA);
}
#endif /* NW_UNIT_FindTableValueByKeyA */

/* ==========================================================================
 * Unit: MultiplyLow64Product
 * Source: runtime/MultiplyLow64Product.c
 * ========================================================================== */
#ifdef NW_UNIT_MultiplyLow64Product
/*
 * Computes the low 64 bits of a 64x64 unsigned multiply via a four-way MUL sequence selected by which operand high words are nonzero; leaf, zero callees
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-multiplylow64product-c
 */

#include <stdint.h>

unsigned __int64 MultiplyLow64Product(unsigned __int64 dwMultiplicand,
                                      unsigned __int64 dwMultiplier)
{
    return dwMultiplicand * dwMultiplier;   /* implicit mod 2^64 == the low 64 bits */
}
#endif /* NW_UNIT_MultiplyLow64Product */
