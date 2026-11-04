/*
 * InputRouting -- input record queues and grid-span dispatch.
 *
 * Module: input
 * Interface: include/InputRouting.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   StoreArgsAndProcessQueuedRecords                           input/StoreArgsAndProcessQueuedRecords.c
 *   WalkGridSpanChainAndDispatchInputByte                      input/WalkGridSpanChainAndDispatchInputByte.c
 */

/* ==========================================================================
 * Unit: StoreArgsAndProcessQueuedRecords
 * Source: input/StoreArgsAndProcessQueuedRecords.c
 * ========================================================================== */
#ifdef NW_UNIT_StoreArgsAndProcessQueuedRecords
/*
 * Per-frame input handoff: the main loop pushes the raw input frame (pointer inside the client, key/modifier/click bytes, anchor and cursor points, and
 *
 * Module: input
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-storeargsandprocessqueuedrecords-c
 */

#include <windows.h>
#include <stdint.h>
#include <setjmp.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

/* The input opcode latch is defined by the linker at window+0x6342. */
extern unsigned char g_bOpZeroOrOne;

/* Existing diagnostic entry hook; it is called before the body proper and
 * preserves all game state. */

#ifdef NWTEST_GLOBAL_SCOPE_JUMP

extern void DispatchExceptionOnGlobalScopeRecord(void);
extern void nwtrace_hex(const char *tag, unsigned long value);
#endif

jmp_buf g_nwGlobalScopeJump;
int     g_nwGlobalScopeJumpActive;

#define NW_STORE_RETURN(value) \
    do { g_nwGlobalScopeJumpActive = 0; return (value); } while (0)

extern void __stdcall AppendByteToShiftedField32(uint8_t bNewByte);

extern void __stdcall SelectGridSpanRecordByPointAndPublishSelectionState(
    uint8_t bAttrCode, uint8_t bCommand,
    int32_t nPointY1, int32_t nPointX1,
    int32_t nPointY2, int32_t nPointX2);

extern void ProcessTwoBytesEaseAndFireHookOnChange(void);
extern int __stdcall ProcessInputAdvanceFrameInterval(void);
extern int __stdcall WalkGridSpanChainAndDispatchInputByte(uint8_t bArg);
extern uint32_t ProcessQueuedTaskRecords(void);
extern int __stdcall AdvanceActiveGridCellSpanToOffset(int nOffsetY, int nOffsetX);
extern int32_t AnimateScrollTowardSelectedGridCell(void);

#define NW_INPUT_CLEAR 0x0eu

NW_DIAG uint32_t __stdcall
StoreArgsAndProcessQueuedRecords(uint32_t bPointerInsideClient,
                                 uint32_t bKeyCode,
                                 uint32_t bModifierSlot,
                                 uint32_t bClickAction,
                                 uint32_t dwAnchorY,
                                 uint32_t dwAnchorX,
                                 uint32_t dwCursorY,
                                 uint32_t dwCursorX,
                                 uint32_t dwSizeHighWord,
                                 uint32_t dwSizeLowWord)
    __asm__("_StoreArgsAndProcessQueuedRecords");

NW_DIAG uint32_t __stdcall
StoreArgsAndProcessQueuedRecords(uint32_t bPointerInsideClient,
                                 uint32_t bKeyCode,
                                 uint32_t bModifierSlot,
                                 uint32_t bClickAction,
                                 uint32_t dwAnchorY,
                                 uint32_t dwAnchorX,
                                 uint32_t dwCursorY,
                                 uint32_t dwCursorX,
                                 uint32_t dwSizeHighWord,
                                 uint32_t dwSizeLowWord)
{
    uint8_t bCommand = (uint8_t)bClickAction;
    int32_t bResumedFrame;

    if (setjmp(g_nwGlobalScopeJump) == 0) {
        g_nwGlobalScopeJumpActive = 1;
        bResumedFrame = 0;
    } else {

        g_nwGlobalScopeJumpActive = 1;
        bResumedFrame = 1;
    }

#ifdef NWTEST_GLOBAL_SCOPE_JUMP
    if (bResumedFrame == 0) {
        static int nTestFired;
        if (!nTestFired) {
            nTestFired = 1;
            nwtrace_hex("gscope_fire", 0);
            DispatchExceptionOnGlobalScopeRecord();
        }
    } else {
        nwtrace_hex("gscope_resume", 1);
    }
#endif

    if (bResumedFrame != 0 || g_bOpZeroOrOne == 0u) {
        bCommand = NW_INPUT_CLEAR;
        bModifierSlot = (bModifierSlot & 0xffffff00u) | NW_INPUT_CLEAR;
        bKeyCode = (bKeyCode & 0xffffff00u) | NW_INPUT_CLEAR;
    }

    g_default_recsel = (uint8_t)bPointerInsideClient;
    g_view_clamp_a = (int32_t)dwSizeLowWord;      
    g_view_clamp_b = (int32_t)dwSizeHighWord;     
    g_bInputChar = (uint8_t)bKeyCode;             

    if ((uint8_t)bKeyCode != NW_INPUT_CLEAR)
        AppendByteToShiftedField32((uint8_t)bKeyCode);

    SelectGridSpanRecordByPointAndPublishSelectionState(
        (uint8_t)bModifierSlot, bCommand,
        (int32_t)dwAnchorY, (int32_t)dwAnchorX,
        (int32_t)dwCursorY, (int32_t)dwCursorX);

    ProcessTwoBytesEaseAndFireHookOnChange();
    if (!ProcessInputAdvanceFrameInterval())
        NW_STORE_RETURN(0u);

    if (g_bGridSpanRunActive != 0u) {
        if (!WalkGridSpanChainAndDispatchInputByte(bCommand))
            NW_STORE_RETURN(0u);
    } else {
        if ((uint8_t)ProcessQueuedTaskRecords() == 0u)
            NW_STORE_RETURN(0u);
    }

    while (g_state_417388 != 0) {
        g_bInputChar = NW_INPUT_CLEAR;
        if (!ProcessInputAdvanceFrameInterval())
            NW_STORE_RETURN(0u);

        if (g_bGridSpanRunActive != 0u) {
            if (!WalkGridSpanChainAndDispatchInputByte(bCommand))
                NW_STORE_RETURN(0u);
        } else {
            if ((uint8_t)ProcessQueuedTaskRecords() == 0u)   /* AL only */
                NW_STORE_RETURN(0u);
        }
    }

    if (!AdvanceActiveGridCellSpanToOffset((int)dwCursorY, (int)dwCursorX))
        NW_STORE_RETURN(0u);
    if (!AnimateScrollTowardSelectedGridCell())
        NW_STORE_RETURN(0u);
    NW_STORE_RETURN(1u);
}
#endif /* NW_UNIT_StoreArgsAndProcessQueuedRecords */

/* ==========================================================================
 * Unit: WalkGridSpanChainAndDispatchInputByte
 * Source: input/WalkGridSpanChainAndDispatchInputByte.c
 * ========================================================================== */
#ifdef NW_UNIT_WalkGridSpanChainAndDispatchInputByte
/*
 * Walk Grid Span Chain And Dispatch Input Byte.
 *
 * Module: input
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#input-walkgridspanchainanddispatchinputbyte-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

/* ---- The 16-byte grid-span chain record, from the offsets touched here ---- */
#define GRIDSPAN_REC_STRIDE   16u
#define GRIDSPAN_TAG_MASK     0x87u   /* AND AL,0x87 */
#define GRIDSPAN_TAG_VALUE    0x81u   
#define GRIDSPAN_NO_LINK      0xFFFFu 

#pragma pack(push, 1)
typedef struct {
    uint8_t  bTag;        /* +0x00 tested (tag & 0x87) == 0x81                */
    uint8_t  bSubIndex;   /* +0x01 zero-based sub-span index/count           */
    uint8_t  bSubCount;   /* +0x02 one-based equivalent; == bSubIndex + 1     */
    uint8_t  _u03;        /* +0x03 (pending-link bits per siblings) untouched */
    uint8_t  bCopyLo;     /* +0x04 copy target <- link[+6]                    */
    uint8_t  bCopyHi;     /* +0x05 copy target <- link[+7]                    */
    uint16_t wLinkIndex;  /* +0x06 index of the linked record from the TOP    */
    uint8_t  _u08[8];     /* +0x08..+0x0f untouched here                      */
} GridSpanChainRec;
#pragma pack(pop)

/* ---- Chain sentinel globals (runtime-populated BSS) ---- */

/* src/grid/AdvanceGridCellSubSpan.c: __stdcall, three args; the LAST is the record. */
extern int __stdcall AdvanceGridCellSubSpan(uint8_t bReleaseSlot, uint8_t bFinalPass, void *pRec) __asm__("_AdvanceGridCellSubSpan");

extern unsigned int __stdcall ProcessGridSpanCommandStream(unsigned char bFirstCommand); 

int __stdcall WalkGridSpanChainAndDispatchInputByte(unsigned char bArg)
{
    const uint8_t *pTop    = (const uint8_t *)(uintptr_t)g_pGridSpanChainTop;
    const uint8_t *pBottom = (const uint8_t *)(uintptr_t)g_pGridSpanChainBottom;

    if ((uintptr_t)pTop > (uintptr_t)pBottom) {
        uint8_t *pRec = (uint8_t *)(uintptr_t)g_pGridSpanChainTop;  

        do {
            if ((pRec[0] & GRIDSPAN_TAG_MASK) == GRIDSPAN_TAG_VALUE) {   
                if (((uint32_t)pRec[1] + 1u) == (uint32_t)pRec[2]) {      
                    const uint16_t wLink = *(const uint16_t *)(const void *)(pRec + 6); 

                    if (wLink != GRIDSPAN_NO_LINK) {                      
                        const uint8_t *pLink =
                            (const uint8_t *)((uintptr_t)pTop -
                                              (uintptr_t)wLink * GRIDSPAN_REC_STRIDE);
                        pRec[4] = pLink[6];                               
                        pRec[5] = pLink[7];                               
                    }
                }

                AdvanceGridCellSubSpan(0u, 0u, pRec);                     
            }

            pRec -= GRIDSPAN_REC_STRIDE;                                  /* ADD EBX,-0x10 */
        } while ((uintptr_t)pRec > (uintptr_t)pBottom);                   
    }

    if (g_bLiveInputByte == 0x20u || g_bLiveInputByte == 0x0du) {         
        return (int)ProcessGridSpanCommandStream(g_bLiveInputByte);       
    }
    if (g_bLiveInputByte == 0x0eu) {                                      
        return 1;                                                         
    }
    if (bArg == 0x0eu) {                                                  
        return (int)ProcessGridSpanCommandStream(g_bLiveInputByte);       /* JNZ not-taken -> call */
    }
    return 1;                                                             /* XOR EAX,EAX before; AL=1 */
}
#endif /* NW_UNIT_WalkGridSpanChainAndDispatchInputByte */
