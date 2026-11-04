/*
 * InputFields -- field-byte update and frame-interval input helpers.
 *
 * Module: input
 * Interface: include/InputFields.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   AppendByteToShiftedField32                                 input/AppendByteToShiftedField32.c
 *   InitializeBlankSpaceFilledField32                          input/InitializeBlankSpaceFilledField32.c
 *   ProcessInputAdvanceFrameInterval                           input/ProcessInputAdvanceFrameInterval.c
 *   ProcessTwoBytesEaseAndFireHookOnChange                     input/ProcessTwoBytesEaseAndFireHookOnChange.c
 *   SetFrameIntervalFromDivisor                                input/SetFrameIntervalFromDivisor.c
 *   StepByteTowardTarget                                       input/StepByteTowardTarget.c
 */

/* ==========================================================================
 * Unit: AppendByteToShiftedField32
 * Source: input/AppendByteToShiftedField32.c
 * ========================================================================== */
#ifdef NW_UNIT_AppendByteToShiftedField32
/*
 * Append Byte To Shifted Field32.
 *
 * Module: input
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-appendbytetoshiftedfield32-c
 */

#include <stdint.h>

extern void *__cdecl MoveMemoryRegion(void *pDest, const void *pSrc, uint32_t dwByteCount);

extern uint8_t szField32[32];   

void __stdcall AppendByteToShiftedField32(uint8_t bNewByte)
{
    /* MoveMemoryRegion(&field[0], &field[1], 31): shift the field one byte left,
     * dropping field[0], so field[0..30] become the old field[1..31]. */
    (void)MoveMemoryRegion((void *)szField32, (const void *)(szField32 + 1), 0x1fu);

    szField32[31] = bNewByte;
}
#endif /* NW_UNIT_AppendByteToShiftedField32 */

/* ==========================================================================
 * Unit: InitializeBlankSpaceFilledField32
 * Source: input/InitializeBlankSpaceFilledField32.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeBlankSpaceFilledField32
/*
 * Initialize Blank Space Filled Field32.
 *
 * Module: input
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-initializeblankspacefilledfield32-c
 */

#include <windows.h>
#include <stdint.h>

extern void *FillMemoryWithByte(void *pDst, unsigned char bFill, unsigned int dwCount);

extern unsigned char szField32[32];      
extern unsigned char cFieldTerminator;   

void InitializeBlankSpaceFilledField32(void)
{
#ifdef NWDEBUG_ENABLED
extern void nwsafe_trace(const char *);
    nwsafe_trace("ibf32_in");
#endif

    /* Fill the whole 32-byte field with spaces, then terminate it. */
    FillMemoryWithByte((void *)szField32, (unsigned char)0x20, 32u);

    cFieldTerminator = 0u;
}
#endif /* NW_UNIT_InitializeBlankSpaceFilledField32 */

/* ==========================================================================
 * Unit: ProcessInputAdvanceFrameInterval
 * Source: input/ProcessInputAdvanceFrameInterval.c
 * ========================================================================== */
#ifdef NW_UNIT_ProcessInputAdvanceFrameInterval
/*
 * Process Input Advance Frame Interval.
 *
 * Module: input
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-processinputadvanceframeinterval-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

typedef struct tagFrameCtl {
    BYTE  bRunning;      /* +0 non-zero => frame advance is halted          */
    BYTE  bFrameDivisor; /* +1 frame-rate divisor, clamped to 1..50        */
    BYTE  bCharPending;  /* +2 non-zero while an input byte is latched     */
    BYTE  bCharLatched;  /* +3 the latched input byte                      */
    DWORD dwFrameCount;  /* +4 frames advanced, incremented once per call  */
} FRAMECTL, *PFRAMECTL;

/*
 * The REST bridge exposes no symbol names for these (get_function_by_address
 * returns address-level data only), so they stay extern instead of invented.
 */

extern BOOL __stdcall SetFrameIntervalFromDivisor(BYTE bDivisor);

#define FRAME_DIVISOR_MAX 0x32 /* 50 */
#define INPUT_CHAR_CLEAR  0x0e /* control code 14 */

BOOL __stdcall ProcessInputAdvanceFrameInterval(void)
{
    PFRAMECTL pCtl = g_pFrameCtl;
    BYTE      bDivisor;

    if (g_bInputChar != INPUT_CHAR_CLEAR) {
        pCtl->bCharLatched = g_bInputChar;
        pCtl->bCharPending = 1;
    } else {
        pCtl->bCharPending = 0;
        pCtl->bCharLatched = 0;
    }

    if (pCtl->bRunning != 0) {
        return FALSE; /* halted: no counter bump, no interval refresh */
    }

    pCtl->dwFrameCount++;

    if (pCtl->bFrameDivisor < 1) {
        pCtl->bFrameDivisor = 1;
    }
    if (pCtl->bFrameDivisor > FRAME_DIVISOR_MAX) {
        pCtl->bFrameDivisor = FRAME_DIVISOR_MAX;
    }

    bDivisor = pCtl->bFrameDivisor;

    /* Re-derive the frame interval only when the divisor actually moved. */
    if (bDivisor != g_bLastDivisor) {
        g_bLastDivisor = bDivisor;
        (void)SetFrameIntervalFromDivisor(bDivisor);
    }

    return TRUE;
}
#endif /* NW_UNIT_ProcessInputAdvanceFrameInterval */

/* ==========================================================================
 * Unit: ProcessTwoBytesEaseAndFireHookOnChange
 * Source: input/ProcessTwoBytesEaseAndFireHookOnChange.c
 * ========================================================================== */
#ifdef NW_UNIT_ProcessTwoBytesEaseAndFireHookOnChange
/*
 * Process Two Bytes Ease And Fire Hook On Change.
 *
 * Module: input
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-processtwobyteseaseandfirehookonchange-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <string.h>


int StepByteTowardTarget(uint8_t bTarget, uint8_t *pValue);

uint32_t __stdcall ProcessSlotHookThunkArg(uint32_t dwArg);

uint32_t __stdcall ForwardStackArgToSlotHook(uint32_t dwArg);

#define EASE_BYTE_1 5 
#define EASE_BYTE_2 6 

void EaseTwoBytesAndFireHookOnChange(void)
{
    uint32_t dwHookArg;

    if (StepByteTowardTarget(g_bEaseTarget0, &g_abEaseBytes[EASE_BYTE_1]) != 0) {

        memcpy(&dwHookArg, &g_abEaseBytes[EASE_BYTE_1], sizeof dwHookArg);
        ProcessSlotHookThunkArg(dwHookArg);
    }
    /* falls through -- no separate exit */

    if (StepByteTowardTarget(g_bEaseTarget1, &g_abEaseBytes[EASE_BYTE_2]) != 0) {
        memcpy(&dwHookArg, &g_abEaseBytes[EASE_BYTE_2], sizeof dwHookArg);
        ForwardStackArgToSlotHook(dwHookArg);
    }

}

__asm__(".globl _ProcessTwoBytesEaseAndFireHookOnChange\n"
        ".set _ProcessTwoBytesEaseAndFireHookOnChange, _EaseTwoBytesAndFireHookOnChange\n");
#define _FILENAME_ALIAS 1
#endif /* NW_UNIT_ProcessTwoBytesEaseAndFireHookOnChange */

/* ==========================================================================
 * Unit: SetFrameIntervalFromDivisor
 * Source: input/SetFrameIntervalFromDivisor.c
 * ========================================================================== */
#ifdef NW_UNIT_SetFrameIntervalFromDivisor
/*
 * Sets the global frame interval to 1000 / divisor milliseconds and always returns TRUE; a zero divisor leaves the interval at 0
 *
 * Module: input
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-setframeintervalfromdivisor-c
 */

#include <windows.h>
#include <stdint.h>

extern DWORD g_dwFrameIntervalMs;

/* Verb-first, 3 specifier tokens (Frame / Interval / From Divisor) per conventions.md. */
BOOL __stdcall SetFrameIntervalFromDivisor(BYTE bDivisor)
{
    g_dwFrameIntervalMs = 0;

    if (bDivisor != 0) {
        /* IDIV is signed; bDivisor is zero-extended into ECX, so 1..255 => 1000..3 ms. */
        g_dwFrameIntervalMs = (DWORD)(1000 / (int32_t)bDivisor);
    }

    return TRUE;
}
#endif /* NW_UNIT_SetFrameIntervalFromDivisor */

/* ==========================================================================
 * Unit: StepByteTowardTarget
 * Source: input/StepByteTowardTarget.c
 * ========================================================================== */
#ifdef NW_UNIT_StepByteTowardTarget
/*
 * Steps one byte toward a target without overshooting and returns the applied delta.
 *
 * Module: input
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-stepbytetowardtarget-c
 */

#include <stdint.h>

extern uint8_t g_frameStepDivisor;

int StepByteTowardTarget(uint8_t bTarget, uint8_t *pValue)
{
    int iDelta = (int)bTarget - (int)*pValue;   /* SUB EBX,EAX (both zero-extended) */
    int iStep;

    if (iDelta == 0) {                          
        return 0;                               /* equal -> no store, return 0 */
    }

    iStep = (int)(0xffu / (unsigned)g_frameStepDivisor);

    if (iDelta < 0) {                           
        iStep = -iStep;                         /* NEG dword [ESP+0x4] */
        if (iStep < iDelta) {                   /* JGE would keep -step */
            iStep = iDelta;                     /* clamp to the difference */
        }
    } else {
        if (iStep > iDelta) {                   /* JLE would keep step */
            iStep = iDelta;                     /* clamp to the difference */
        }
    }

    *pValue = (uint8_t)((int)*pValue + iStep);

    return iDelta;                              
}
#endif /* NW_UNIT_StepByteTowardTarget */
