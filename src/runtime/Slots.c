#include "nwarch.h"
/*
 * Slots -- slot tables, hook state, and thread-context helpers.
 *
 * Module: runtime
 * Interface: include/Slots.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   AcquireNextFreeSlot                                        runtime/AcquireNextFreeSlot.c
 *   ClearTopSlotStatusAndRebuildRegionBounds                   runtime/ClearTopSlotStatusAndRebuildRegionBounds.c
 *   CopyRecordsToSlotChain                                     runtime/CopyRecordsToSlotChain.c
 *   DispatchPendingSlotHookForIndex                            runtime/DispatchPendingSlotHookForIndex.c
 *   ReleaseSlotSemaphore                                       runtime/ReleaseSlotSemaphore.c
 *   ResetHookTableToDefaults                                   runtime/ResetHookTableToDefaults.c
 *   ResetPendingSlotHookState                                  runtime/ResetPendingSlotHookState.c
 *   ResetSlotAndHookTablesToDefaults                           runtime/ResetSlotAndHookTablesToDefaults.c
 *   ResetSlotTablesFormatApplyAndDispatchSettings              runtime/ResetSlotTablesFormatApplyAndDispatchSettings.c
 *   ReturnConstantOne                                          runtime/ReturnConstantOne.c
 *   ReturnTrueSlotHook                                         runtime/ReturnTrueSlotHook.c
 *   SelectNextActiveSlot                                       runtime/SelectNextActiveSlot.c
 *   SetPendingFlagFromSlotHookResult                           runtime/SetPendingFlagFromSlotHookResult.c
 *   SetThreadContextValueAndClearFlag                          runtime/SetThreadContextValueAndClearFlag.c
 *   SlotHookReturnOne                                          runtime/SlotHookReturnOne.c
 *   SlotHookReturnOneAlt                                       runtime/SlotHookReturnOneAlt.c
 *   SlotHookReturnVoid                                         runtime/SlotHookReturnVoid.c
 */

/* ==========================================================================
 * Unit: AcquireNextFreeSlot
 * Source: runtime/AcquireNextFreeSlot.c
 * ========================================================================== */
#ifdef NW_UNIT_AcquireNextFreeSlot
/*
 * Walks the fixed-stride 16-byte slot chain downward from its top, skipping in-use slots (status byte bit-0 set).
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-acquirenextfreeslot-c
 */

#include <stdint.h>

extern uint8_t *g_pSlotsTop;

extern int32_t g_dwHeapRegionEndSentinel;

/* Resolved callee: row 54. Publishes a new region-end sentinel and recomputes size-in-words. */
extern void __stdcall SetHeapRegionBounds(int32_t pHeapRegionEnd) __asm__("_SetHeapRegionBounds");



uint8_t *AcquireNextFreeSlot(void)
{
    uint8_t bStatusByte;   /* status byte of the current slot (bit-0 = in-use)     */
    uint8_t *pbCurSlot;    /* current slot record, walking 16-byte strides downward*/

    bStatusByte = *g_pSlotsTop;       /* read the topmost slot's status first      */
    pbCurSlot   = g_pSlotsTop;        /* start the scan at the top                 */

    /* Skip occupied slots (bit-0 set) and stop once we hit a free slot or the
       region-end sentinel. */
    while (((bStatusByte & 1u) != 0u) && (pbCurSlot != (uint8_t *)(uintptr_t)g_dwHeapRegionEndSentinel)) {
        pbCurSlot   = pbCurSlot - 0x10;  /* step down one 16-byte slot             */
        bStatusByte = *pbCurSlot;       /* read that slot's status                */
    }

    *pbCurSlot = 1u;                    /* claim the found slot (mark in-use)      */

    if (pbCurSlot == (uint8_t *)(uintptr_t)g_dwHeapRegionEndSentinel) {
        /* Ran to the sentinel with no free slot: grow the arena by one slot.     */
        SetHeapRegionBounds((int32_t)(uintptr_t)(pbCurSlot - 0x10));
    }

    return pbCurSlot;
}
#endif /* NW_UNIT_AcquireNextFreeSlot */

/* ==========================================================================
 * Unit: ClearTopSlotStatusAndRebuildRegionBounds
 * Source: runtime/ClearTopSlotStatusAndRebuildRegionBounds.c
 * ========================================================================== */
#ifdef NW_UNIT_ClearTopSlotStatusAndRebuildRegionBounds
/*
 * Clear Top Slot Status And Rebuild Region Bounds.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-cleartopslotstatusandrebuildregionbounds-c
 */

#include <stdint.h>

extern uintptr_t g_pSlotsTop;            

extern void __stdcall SetHeapRegionBounds(uintptr_t pRegionStart) __asm__("_SetHeapRegionBounds");

void ResetArenaChainTopStatusAndBounds(void) {
    uintptr_t top = g_pSlotsTop;         
    *(uint8_t *)top = 0;                 
    SetHeapRegionBounds(g_pSlotsTop);    
}

__asm__(".globl _ClearTopSlotStatusAndRebuildRegionBounds\n"
        ".set _ClearTopSlotStatusAndRebuildRegionBounds, _ResetArenaChainTopStatusAndBounds\n");
#define _ALIAS2 1
#endif /* NW_UNIT_ClearTopSlotStatusAndRebuildRegionBounds */

/* ==========================================================================
 * Unit: CopyRecordsToSlotChain
 * Source: runtime/CopyRecordsToSlotChain.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyRecordsToSlotChain
/*
 * Copy Records To Slot Chain.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-copyrecordstoslotchain-c
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;

#define SLOT_STRIDE    0x10u /* chain slots are 16-byte records, walked top -> bottom */
#define RECORD_STRIDE  0x10u /* argument indexes the record array: base + arg*0x10   */

extern void *g_SlotChainTopPtr;       
extern void *g_SlotChainBottomPtr;    
/* 0x630c is a pointer slot in the recovered image. */
#define g_RecordArrayBase NW_WINDOW_PTR_REF(uint8_t, 0x630c)
extern u32   g_SlotRegionBase;        
extern u32   g_RecordStrideBytes;     
extern u32   g_RecordCountLimit;      
extern void *g_BulkCopyBasePtr;       
extern u8    g_BulkCopyShiftAmount;   

/* ---- Callees (signatures resolved by parent) ---- */
extern void *MoveMemoryRegion(void *dest, const void *src, size_t n);     
extern void ClearTopSlotStatusAndRebuildRegionBounds(void);               

extern uint32_t *__stdcall AllocHeapBlock(uint32_t dwSizeBytes);          

/* Four MOVSDs emit 32 bytes, but the next copy starts only 16 in and
 * overwrites the trailing half; net effect = a 16-byte block at stride 0x10. */
static void copy_slot_16(void **dst, const void *slot)
{
    u8 *d = (u8 *)*dst;
    memcpy(d, slot, SLOT_STRIDE);
    *dst = d + SLOT_STRIDE;
}

void __stdcall CopyRecordsToSlotChain(int index)
{
    u8 *rec = (u8 *)g_RecordArrayBase + (size_t)index * RECORD_STRIDE; 

    /* ---- Phase 1: count eligible slots, top -> bottom ------------------- */
    u32 slot_count = 0;                              /* rec[+12]; cleared then incremented */
    void *cur = g_SlotChainTopPtr;                  /* [+0x10]                          */
    while (cur > g_SlotChainBottomPtr) {            
        const u8 b = *(const u8 *)cur;
        if (((b & 3u) == 1u && !(b & 4u)) || ((b & 3u) == 3u))
            slot_count++;                           /* nibble 1 w/o bit 0x4, or nibble 3 */
        cur = (void *)((u8 *)cur - SLOT_STRIDE);    /* [+0x10] -= 0x10                 */
    }

    *(u32 *)(rec + 12) = slot_count;

    /* ---- Phase 1b: resolve arena anchor for the counted region ---------- */
    u32 off = (slot_count << 4) + g_SlotRegionBase * 2u;   /* LEA ECX,[EDX+ECX*0x2] */
    void *anchor = AllocHeapBlock(off);                        
    *(u32 *)(rec + 8) = (u32)(uintptr_t)anchor;           /* rec[+8] <- result   */

    /* ---- Phase 2: bulk copy of records ---------------------------------- */
    u32 i = 0;   /* [+0x28]: +1 per iteration; the EBX register mirror runs at
                  * twice that rate and drives the destination anchor below.   */
    while (i < g_RecordCountLimit) {            /* CMP,JL (signed compare in bytes) */
        MoveMemoryRegion(
            (void *)((uintptr_t)(*(u32 *)(rec + 8)) + i * 2u * g_RecordStrideBytes),
                                                                        /* dest: last pushed */
            (const void *)((const u8 *)g_BulkCopyBasePtr + ((i << g_BulkCopyShiftAmount) * 2u)),
                                                                        /* src : middle push */
            (size_t)(g_RecordStrideBytes << 1));                       /* n   : first push  */
        i += 1;                                  /* INC [+0x34] -> slot [+0x28] */
    }

    /* ---- Phase 3: per-slot copy + header rewire, top -> bottom --------- */
    void *dst = (void *)((uintptr_t)(*(u32 *)(rec + 8)) + g_SlotRegionBase * 2u); /* [+0x14] */
    u32 out_idx = 0;                             /* [+0x28]                       */
    u16 last_assigned = 0;                       /* [+0x18]; sticky, only the nibble-3 case rewrites it */

    cur = g_SlotChainTopPtr;                     /* [+0x10]                       */
    while (cur > g_SlotChainBottomPtr) {         
        const u8 saved_byte = *(const u8 *)cur;  /* status byte, kept in [+0x20] */

        if ((saved_byte & 3u) == 3u) {           /* ---- case: nibble 3 -------- */
            u8 *c = (u8 *)cur;
            last_assigned = (u16)out_idx;        /* [+0x18] saved before any INC */
            *(u16 *)(c + 0xA) = 0xFFFFu;         /* preamble word at [+0xA]      */

            if (*(const u16 *)(c + 8) != 0xFFFFu) {   /* rewire branch          */
                void *ref = (void *)((u8 *)g_SlotChainTopPtr - (size_t)(*(const u16 *)(c + 8) & 0xFFFFu) * SLOT_STRIDE);
                const u16 nxt = (u16)(out_idx + 1);   /* AX=idx then INC AX     */
                *(u16 *)(c + 8) = nxt;                /* cur[+8]                */
                *(u16 *)(c + 0xA) = nxt;              /* cur[+0xA], overwrites 0xffff */
                copy_slot_16(&dst, cur);              /* four MOVSDs from cur   */
                *c = 0;                               /* [ECX]=DL: always 0 (EAX clobbered to 0x10) */
                out_idx += 1;                         /* INC [+0x28]            */
                *(u16 *)((u8 *)ref + 6) = last_assigned; /* ref[+6]: header low bytes <- saved idx */
                ((u8 *)ref)[0] |= 0x80;               /* status bit 0x80        */
                copy_slot_16(&dst, ref);              /* four MOVSDs from EBP   */
                *(u8 *)ref = 0;                       /* [EBP]=DL: always 0    */
                out_idx += 1;                         /* INC [+0x28]            */
            } else {                                 /* old == 0xffff: plain path */
                copy_slot_16(&dst, cur);              /* four MOVSDs            */
                *c = 0;                               /* [cur]=0 (literal)     */
                out_idx += 1;                         /* INC [+0x28]            */
            }

            const u32 dist = (u32)((size_t)((const u8 *)g_SlotChainTopPtr - (const u8 *)cur) / SLOT_STRIDE); /* IDIV 0x10 */
            if (g_SlotChainTopPtr > g_SlotChainBottomPtr) {               
                void *scan = g_SlotChainTopPtr;                  /* EBP, top down   */
                do {
                    const u8 sbyte = *(const u8 *)scan;
                    if ((sbyte & 7u) == 1u &&
                        (u32)*(const u16 *)((const u8 *)scan + 6) == dist) { /* [+6] matches */
                        *(u16 *)((u8 *)scan + 6) = last_assigned;/* <- sticky idx ([+0x18]) */
                        copy_slot_16(&dst, scan);                /* four MOVSDs          */
                        *(u8 *)scan = 0;                         /* clear status byte    */
                        out_idx += 1;                            /* INC [+0x28]         */
                    }
                    scan = (void *)((u8 *)scan - SLOT_STRIDE);  /* ADD EBP,-0x10      */
                } while (scan > g_SlotChainBottomPtr);           /* CMP,JBE exit, JMP back */
            }
        } else if ((saved_byte & 3u) == 1u) {        /* ---- case: nibble 1 ---- */
            u8 *c = (u8 *)cur;
            if (*(const u16 *)(c + 6) == 0xFFFFu && !(saved_byte & 4u)) {
                copy_slot_16(&dst, cur);              /* four MOVSDs            */
                *c = 0;                               /* [cur]=0 (literal)     */
                out_idx += 1;                         /* INC [+0x28]            */
            }
        }

        cur = (void *)((u8 *)cur - SLOT_STRIDE);     /* [+0x10] -= 0x10          */
    }

    ClearTopSlotStatusAndRebuildRegionBounds();      
}
#endif /* NW_UNIT_CopyRecordsToSlotChain */

/* ==========================================================================
 * Unit: DispatchPendingSlotHookForIndex
 * Source: runtime/DispatchPendingSlotHookForIndex.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchPendingSlotHookForIndex
/*
 * Dispatch Pending Slot Hook For Index.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-dispatchpendingslothookforindex-c
 */

#include <windows.h>
#include "nwglobals.h"   /* __stdcall */
#include <stdint.h>    /* uint8_t / uint32_t — NOT pulled in by windows.h here */





extern void __stdcall ResetPendingSlotHookState(void);

extern int __stdcall SetPendingFlagFromSlotHookResult(uint32_t dwSlotArg);

int __stdcall DispatchPendingSlotHookForIndex(uint32_t dwSlotIndex)
{
    if (g_bDispatchAllowedFlag != 0) {
        return 1;                                    
    }

    ResetPendingSlotHookState();                     /* clears flag, result = -1 */

    if (g_bPendingDispatchEnabled == 0) {
        return 1;                                    
    }

    g_dwPendingHookResult = dwSlotIndex;             
    dwSlotIndex = g_pdwSlotArgTable[dwSlotIndex];    /* PUSH [ECX + EAX*4]      */

    /* MOVSX EAX, AL — only the low byte of the hook result is kept. */
    return (int)(signed char)SetPendingFlagFromSlotHookResult(dwSlotIndex);
}
#endif /* NW_UNIT_DispatchPendingSlotHookForIndex */

/* ==========================================================================
 * Unit: ReleaseSlotSemaphore
 * Source: runtime/ReleaseSlotSemaphore.c
 * ========================================================================== */
#ifdef NW_UNIT_ReleaseSlotSemaphore
/*
 * Release Slot Semaphore.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-releaseslotsemaphore-c
 */

#include <stdint.h>
#include "nwglobals.h"

#ifdef NWDEBUG_ENABLED
void nwtrace_hex(const char *, unsigned long);
#endif
#include <windows.h>

#define g_pawSlotSemaphoreHandles ((uint32_t *)(nw_data_window + 0x66d4))   

#define g_pszSlotSemNamePrefix ((char *)(nw_data_window + 0x6809))   

char * __cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc);

char * __cdecl AppendCStringToDest(char *pDestBuf, const char *pszSrc);

char * ConvertIntegerToString(int iValue, char *pDest, unsigned uBase);

void ReleaseSlotSemaphore(uint32_t uiSlotIndex)
{
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("apb_rs_enter", 0);
#endif

    char szSlotSemName[28];  
    char szIndexHexBuf[20];  

    CopyNulTerminatedString(szSlotSemName, g_pszSlotSemNamePrefix);
    AppendCStringToDest(
        szSlotSemName,
        ConvertIntegerToString((int)uiSlotIndex, szIndexHexBuf, 16u));

    /* Adopt the cached handle; lazy-create only if this slot has none yet. */
    if (g_pawSlotSemaphoreHandles[uiSlotIndex] == 0u) {
        HANDLE hCreated;
        do {
            hCreated = CreateSemaphoreA(NULL, 0, 1, szSlotSemName);
            if (hCreated != NULL) {
                g_pawSlotSemaphoreHandles[uiSlotIndex] = (uint32_t)(uintptr_t)hCreated;
            }
        } while (g_pawSlotSemaphoreHandles[uiSlotIndex] == 0u);
    }

    /* Release exactly one count from the slot's semaphore. */
    HANDLE hSlotSem = (HANDLE)(uintptr_t)g_pawSlotSemaphoreHandles[uiSlotIndex];
    (void)ReleaseSemaphore(hSlotSem, 1, NULL);
}
#endif /* NW_UNIT_ReleaseSlotSemaphore */

/* ==========================================================================
 * Unit: ResetHookTableToDefaults
 * Source: runtime/ResetHookTableToDefaults.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetHookTableToDefaults
/*
 * Reset Hook Table To Defaults.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-resethooktabletodefaults-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef uint32_t (*SlotHookFn)(void);

extern void CloseMciCdAudioDeviceAndClearEnabledFlag(void);
extern uint8_t __cdecl ReturnConstantOne(void);
extern uint8_t __stdcall SlotHookReturnOne(int dwUnusedOperand)
    __asm__("_ReturnOneRetFour");
extern uint8_t __stdcall SlotHookReturnOneAlt(int dwUnusedOperand)
    __asm__("_ReturnOneRetFourCaseD45");
extern void SlotHookReturnVoid(void) __asm__("_ReturnBareRetStub");

uint32_t ResetHookTableToDefaults(void)
{
    volatile SlotHookFn *slots =
        (volatile SlotHookFn *)NW_WINDOW_PTR_SLOTS(0x620);
    SlotHookFn currentSlot11;

    CloseMciCdAudioDeviceAndClearEnabledFlag();

    currentSlot11 = slots[11];
    (void)currentSlot11();

    slots[1]  = (SlotHookFn)SlotHookReturnOne;
    slots[8]  = (SlotHookFn)ReturnConstantOne;
    slots[7]  = (SlotHookFn)ReturnConstantOne;
    slots[6]  = (SlotHookFn)ReturnConstantOne;
    slots[5]  = (SlotHookFn)ReturnConstantOne;
    slots[4]  = (SlotHookFn)ReturnConstantOne;
    slots[3]  = (SlotHookFn)ReturnConstantOne;
    slots[2]  = (SlotHookFn)ReturnConstantOne;
    slots[0]  = (SlotHookFn)ReturnConstantOne;
    slots[10] = (SlotHookFn)SlotHookReturnOneAlt;
    slots[9]  = (SlotHookFn)SlotHookReturnOneAlt;
    slots[11] = (SlotHookFn)SlotHookReturnVoid;

    return (uint32_t)(uintptr_t)SlotHookReturnVoid;
}
#endif /* NW_UNIT_ResetHookTableToDefaults */

/* ==========================================================================
 * Unit: ResetPendingSlotHookState
 * Source: runtime/ResetPendingSlotHookState.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetPendingSlotHookState
/*
 * Flushes a pending slot-hook dispatch: if the pending-flag global is set it fires the hook thunk.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-resetpendingslothookstate-c
 */

#include <stdint.h>
#include "nwglobals.h"



extern uint32_t InvokeSlotHookThunk(void);

void ResetPendingSlotHookState(void)
{
#ifdef NWDEBUG_ENABLED
extern void nwsafe_trace(const char *);
    nwsafe_trace("rps_in");
#endif

    if (g_bPendingHookFlag != 0) {
        (void)InvokeSlotHookThunk();   /* CMP/JZ/CALL — result deliberately unused */
    }
    g_dwPendingHookResult = 0xFFFFFFFFu;   
    g_bPendingHookFlag    = 0;            
}
#endif /* NW_UNIT_ResetPendingSlotHookState */

/* ==========================================================================
 * Unit: ResetSlotAndHookTablesToDefaults
 * Source: runtime/ResetSlotAndHookTablesToDefaults.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetSlotAndHookTablesToDefaults
/*
 * Reset Slot And Hook Tables To Defaults.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-resetslotandhooktablestodefaults-c
 */

#include <stdint.h>

extern uint32_t ClearAllSlotEntries(void);
extern uint32_t __stdcall DispatchMciPlaybackCommand(uint32_t dwOpCode);
extern uint32_t ResetHookTableToDefaults(void);

void ResetSlotAndHookTablesToDefaults(void)
{
    (void)ClearAllSlotEntries();
    (void)DispatchMciPlaybackCommand(0u);
    ResetHookTableToDefaults();
}
#endif /* NW_UNIT_ResetSlotAndHookTablesToDefaults */

/* ==========================================================================
 * Unit: ResetSlotTablesFormatApplyAndDispatchSettings
 * Source: runtime/ResetSlotTablesFormatApplyAndDispatchSettings.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetSlotTablesFormatApplyAndDispatchSettings
/*
 * Reset Slot Tables Format Apply And Dispatch Settings.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-resetslottablesformatapplyanddispatchsettings-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

extern void ResetSlotAndHookTablesToDefaults(void);
extern int __cdecl ScanStringFormatToVarArgs(char *pszInput, char *pszFormat, ...);
extern int __stdcall PlayStartupMovieWithMessagePumpAndTeardown(
    uint32_t dwMovieRectWord3, uint32_t dwMovieRectWord2,
    uint32_t dwMovieWindowY, uint32_t dwMovieWindowX,
    const char *pszMovieStem);
extern void InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook(void);

NW_DIAG int __stdcall
ResetSlotTablesFormatApplyAndDispatchSettings(uint32_t param_1)
    __asm__("ResetSlotTablesFormatApplyAndDispatchSettings");

NW_DIAG int __stdcall
ResetSlotTablesFormatApplyAndDispatchSettings(uint32_t param_1)
{
    char stem[0x3c];
    int32_t value0 = 0;
    int32_t value1 = 0;
    int32_t value2 = 0;
    int32_t value3 = 0;
    int32_t movieResult;

    ResetSlotAndHookTablesToDefaults();

    (void)ScanStringFormatToVarArgs(
        (char *)(uintptr_t)param_1,
        (char *)(intptr_t)(nw_data_window + 0x504), /* "%s %d %d %d %d" */
        stem, &value3, &value2, &value1, &value0);

    movieResult = PlayStartupMovieWithMessagePumpAndTeardown(
        (uint32_t)value0, (uint32_t)value1,
        (uint32_t)value2, (uint32_t)value3, stem);

    InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook();

    return (int)(signed char)(uint8_t)movieResult;
}

__asm__(".globl _ResetSlotTablesFormatApplyAndDispatchSettings\n"
        ".set _ResetSlotTablesFormatApplyAndDispatchSettings, "
        "ResetSlotTablesFormatApplyAndDispatchSettings\n");
#endif /* NW_UNIT_ResetSlotTablesFormatApplyAndDispatchSettings */

/* ==========================================================================
 * Unit: ReturnConstantOne
 * Source: runtime/ReturnConstantOne.c
 * ========================================================================== */
#ifdef NW_UNIT_ReturnConstantOne
/*
 * Returns one and ignores its arguments.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-returnconstantone-c
 */

#include <stdint.h>

uint8_t __cdecl ReturnConstantOne(void)
{
    return 1;
}
#endif /* NW_UNIT_ReturnConstantOne */

/* ==========================================================================
 * Unit: ReturnTrueSlotHook
 * Source: runtime/ReturnTrueSlotHook.c
 * ========================================================================== */
#ifdef NW_UNIT_ReturnTrueSlotHook
/*
 * Slot hook that always reports success.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-returntrueslothook-c
 */

#include <stdint.h>

int __cdecl ReturnTrueSlotHook(void) { return 1; }
#endif /* NW_UNIT_ReturnTrueSlotHook */

/* ==========================================================================
 * Unit: SelectNextActiveSlot
 * Source: runtime/SelectNextActiveSlot.c
 * ========================================================================== */
#ifdef NW_UNIT_SelectNextActiveSlot
/*
 * Walks the 16-byte slot chain downward to replace the currently selected slot with the topmost occupied one below it (wrapping to above).
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-selectnextactiveslot-c
 */

#include <stdint.h>

/* Module-owned globals (data section): */
extern uint8_t *g_pActiveSlot;  
extern uint8_t *g_pSlotsTop;    
extern uint8_t *g_pSlotsBottom; 
extern uint8_t *g_pStateTable;  

/*
 * Slot records are laid out at a fixed stride of 0x10 bytes, walked in descending address order:
 *   byte 0 : status flags; low two bits == 3 marks the slot as occupied/active
 *   byte 1 : index into g_pStateTable (entries 0x10 apart); its first byte must equal 3
 */

void __stdcall SelectNextActiveSlot(uint8_t *pOldSlot)
{
    uint8_t *pNextSlot = NULL; /* fallback: topmost occupied slot at/above pOldSlot   */
    int      nBelowOld = 0;    /* visited slots strictly below pOldSlot               */

    /* Only acts when asked to advance from the current selection. */
    if (pOldSlot != g_pActiveSlot) {
        return;
    }

    for (uint8_t *pSlot = g_pSlotsTop; pSlot > g_pSlotsBottom; pSlot -= 0x10u) {
        if (pSlot < pOldSlot) {
            nBelowOld++;
        }
        /* Occupied slot whose state-table entry is also flagged active: */
        if (((*pSlot & 0x03u) == 0x03u) &&
            g_pStateTable[pSlot[1] * 0x10u] == 0x03u) {
            if (nBelowOld != 0) { /* first occupied slot below the old one: done */
                g_pActiveSlot = pSlot;
                return;
            }
            if (pNextSlot == NULL) { /* remember topmost candidate above it     */
                pNextSlot = pSlot;
            }
        }
    }

    /* No occupied slot below: wrap to the remembered topmost one, or clear. */
    g_pActiveSlot = pNextSlot;
}
#endif /* NW_UNIT_SelectNextActiveSlot */

/* ==========================================================================
 * Unit: SetPendingFlagFromSlotHookResult
 * Source: runtime/SetPendingFlagFromSlotHookResult.c
 * ========================================================================== */
#ifdef NW_UNIT_SetPendingFlagFromSlotHookResult
/*
 * Set Pending Flag From Slot Hook Result.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-setpendingflagfromslothookresult-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef uint32_t (__stdcall *SlotHookFn)(uint32_t dwArg);

extern uint32_t __stdcall HandleSlotHookWithArg(uint32_t dwArg1,
                                                uint32_t dwArg2,
                                                uint32_t dwArg3);


uint32_t __stdcall SetPendingFlagFromSlotHookResult(uint32_t dwCallerArg)
{
    uint32_t dwHookResult = HandleSlotHookWithArg(0u, 0xFFFFFFFFu, dwCallerArg);

    g_bPendingHookFlag = (uint8_t)(dwHookResult & 0xFFu);   
    return dwHookResult;   
}
#endif /* NW_UNIT_SetPendingFlagFromSlotHookResult */

/* ==========================================================================
 * Unit: SetThreadContextValueAndClearFlag
 * Source: runtime/SetThreadContextValueAndClearFlag.c
 * ========================================================================== */
#ifdef NW_UNIT_SetThreadContextValueAndClearFlag
/*
 * Set Thread Context Value And Clear Flag.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-setthreadcontextvalueandclearflag-c
 */

#include <windows.h>

extern void *FindCurrentThreadContext(DWORD dwUnusedParam1, DWORD dwUnusedParam2);

/* Field offsets inside the per-thread context record (1024-entry global table). */
#define TCX_FIELD_VALUE 0x10   /* DWORD written with dwValue */
#define TCX_FIELD_FLAG  0x8c   /* DWORD zeroed on every successful call */

void __fastcall SetThreadContextValueAndClearFlag(DWORD dwUnusedParam1,
                                                  DWORD dwUnusedParam2,
                                                  DWORD dwValue)
{
    BYTE *pContext = (BYTE *)FindCurrentThreadContext(dwUnusedParam1, dwUnusedParam2);

    if (pContext == NULL)
        return;                      /* thread has no context record */

    *(DWORD *)(pContext + TCX_FIELD_VALUE) = dwValue;
    *(DWORD *)(pContext + TCX_FIELD_FLAG)  = 0;
}
#endif /* NW_UNIT_SetThreadContextValueAndClearFlag */

/* ==========================================================================
 * Unit: SlotHookReturnOne
 * Source: runtime/SlotHookReturnOne.c
 * ========================================================================== */
#ifdef NW_UNIT_SlotHookReturnOne
/*
 * Slot callback returning one and popping one stack argument.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-slothookreturnone-c
 */

#include <stdint.h>

uint8_t __stdcall SlotHookReturnOne(int dwUnusedOperand)
    __asm__("_ReturnOneRetFour");

uint8_t __stdcall SlotHookReturnOne(int dwUnusedOperand)
{
    (void)dwUnusedOperand;
    return 1;
}
#endif /* NW_UNIT_SlotHookReturnOne */

/* ==========================================================================
 * Unit: SlotHookReturnOneAlt
 * Source: runtime/SlotHookReturnOneAlt.c
 * ========================================================================== */
#ifdef NW_UNIT_SlotHookReturnOneAlt
/*
 * Slot callback returning one and popping one stack argument.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-slothookreturnonealt-c
 */

#include <stdint.h>

uint8_t __stdcall SlotHookReturnOneAlt(int dwUnusedOperand)
    __asm__("_ReturnOneRetFourCaseD45");

uint8_t __stdcall SlotHookReturnOneAlt(int dwUnusedOperand)
{
    (void)dwUnusedOperand;
    return 1;
}
#endif /* NW_UNIT_SlotHookReturnOneAlt */

/* ==========================================================================
 * Unit: SlotHookReturnVoid
 * Source: runtime/SlotHookReturnVoid.c
 * ========================================================================== */
#ifdef NW_UNIT_SlotHookReturnVoid
/*
 * Slot callback with no result and no stack cleanup.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-slothookreturnvoid-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

NW_DIAG void SlotHookReturnVoid(void) __asm__("_ReturnBareRetStub");

NW_DIAG void SlotHookReturnVoid(void)
{
}
#endif /* NW_UNIT_SlotHookReturnVoid */
