/*
 * Shared data-window globals and fixed-offset storage aliases.
 *
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#include-nwglobals-h
 */

#ifndef NWGLOBALS_H
#define NWGLOBALS_H

#include <stdint.h>
#include "nwarch.h"
#include "nwgridctxrec.h"

extern unsigned char nw_data_window[] __asm__("nw_data_window");

#define g_pGridRefTabEntry NW_WINDOW_PTR_REF(unsigned char, 0x1770)

/* ProcessGridSpanCommandStream state: span stack, token/field buffer, and the
 * grid cursor offsets used while walking a command stream. The storage lives in
 * the fixed data window. */

#define g_abCtypeTable         ((uint8_t *)(intptr_t)(nw_data_window + 0xa4d))

#define g_pStreamCursor NW_WINDOW_PTR_REF(uint8_t, 0x1760)

#define g_nGridTabIndex        (*(int32_t *)(intptr_t)(nw_data_window + 0x1764))

#define g_nGridCellOffsetX     (*(int32_t *)(intptr_t)(nw_data_window + 0x1768))

#define g_nCellOffsetY         (*(int32_t *)(intptr_t)(nw_data_window + 0x176c))

#define g_abFieldBuf           ((uint8_t *)(intptr_t)(nw_data_window + 0x1774))

#define g_apGridSpanStack      ((GridCellSpanRec **)NW_WINDOW_PTR_SLOTS(0x1874))

#define g_nGridSpanStackDepth  (*(int32_t *)(intptr_t)(nw_data_window + 0x1c70))

#define g_dwFieldWidth         (*(int32_t *)(intptr_t)(nw_data_window + 0x1c74))

#define g_pConvDest NW_WINDOW_PTR_REF(uint8_t, 0x1c78)

#define g_bConvType            (*(uint8_t *)(intptr_t)(nw_data_window + 0x1c7c))

#define g_bFieldMode           (*(uint8_t *)(intptr_t)(nw_data_window + 0x1c7d))

#define g_bDigitsOnly          (*(uint8_t *)(intptr_t)(nw_data_window + 0x1c7e))

#define g_dwSavedCurGridCtx    (*(uint32_t *)(intptr_t)(nw_data_window + 0x1c80))

#define g_dwSavedGridCtxAux    (*(uint32_t *)(intptr_t)(nw_data_window + 0x1c84))

#define g_dwSavedGridUserData  (*(uint32_t *)(intptr_t)(nw_data_window + 0x1c88))

#define g_dwGridCtxAux         (*(uint32_t *)(intptr_t)(nw_data_window + 0x6214))

#define g_pCurGridCtx NW_WINDOW_PTR_REF(GridCtxRec, 0x6218)

#define g_dwGridUserData       (*(uint32_t *)(intptr_t)(nw_data_window + 0x621c))

#define g_pVmBaseA NW_WINDOW_PTR_REF(uint8_t, 0x63e4)

#define g_pVmBaseB NW_WINDOW_PTR_REF(uint8_t, 0x63e8)

#define g_bGridSpanRunActive   (*(uint8_t *)(intptr_t)(nw_data_window + 0x6802))

#define g_ActiveGridCell         (*(ActiveGridCellRec *)(intptr_t)(nw_data_window + 0x74))

#define g_ScratchBudget          (*(int *)(intptr_t)(nw_data_window + 0x1cd4))

#define g_ScreenBase             NW_WINDOW_PTR_VALUE(0x1cc8)

#define g_aWaveHeaders           (*(WAVEHDR (*)[8])(void *)(nw_data_window + 0x1ce0))

#define g_abEaseBytes            ((uint8_t *)(intptr_t)(nw_data_window + 0x6358))

#define g_adwSlotTable           ((unsigned char *)(void *)(nw_data_window + 0x1e28))

#define g_apfnSlotHooks          (*(SlotHookFn (*)[12])(void *)NW_WINDOW_PTR_SLOTS(0x620))

#define g_bActiveCellReady       (*(uint8_t *)(intptr_t)(nw_data_window + 0x6808))

#define g_bClickAction           (*(uint8_t *)(intptr_t)(nw_data_window + 0x5f50))

#define g_bColFar                (*(uint8_t *)(intptr_t)(nw_data_window + 0x6359))

#define g_bColOrigin             (*(uint8_t *)(intptr_t)(nw_data_window + 0x6358))

#define g_bCommandGuardByte      (*(BYTE *)(intptr_t)(nw_data_window + 0x67fe))

#define g_bCommandOut            (*(uint8_t *)(intptr_t)(nw_data_window + 0x6805))

#define g_bDispatchAllowedFlag   (*(uint8_t *)(intptr_t)(nw_data_window + 0x67f8))

#define g_bEaseTarget0           (*(uint8_t *)(intptr_t)(nw_data_window + 0x67fa))

#define g_bEaseTarget1           (*(uint8_t *)(intptr_t)(nw_data_window + 0x67fb))

#define g_bForwardAllToHandler   (*(uint8_t *)(intptr_t)(nw_data_window + 0x60c3))

#define g_bGateCellIndex         (*(uint8_t *)(intptr_t)(nw_data_window + 0x67fd))

#define g_bGateCommand           (*(uint8_t *)(intptr_t)(nw_data_window + 0x67fe))

#define g_bInputChar             (*(BYTE *)(intptr_t)(nw_data_window + 0x6800))

#define g_bKeyCode               (*(uint8_t *)(intptr_t)(nw_data_window + 0x5f52))

#define g_bLastDivisor           (*(BYTE *)(intptr_t)(nw_data_window + 0x627b))

#define g_bLastGridPenIndex      (*(uint8_t *)(intptr_t)(nw_data_window + 0x1201))

#define g_bLiveInputByte         (*(uint8_t *)(intptr_t)(nw_data_window + 0x6800))

#define g_bModifierSlot          (*(uint8_t *)(intptr_t)(nw_data_window + 0x5f51))

#define g_bPendingDispatchEnabled (*(uint8_t *)(intptr_t)(nw_data_window + 0x6341))

#define g_bPendingHookFlag       (*(uint8_t *)(intptr_t)(nw_data_window + 0x175c))

#define g_bPickedCellLow         (*(uint8_t *)(intptr_t)(nw_data_window + 0x6803))

#define g_bProbeAExpect          (*(BYTE *)(intptr_t)(nw_data_window + 0x67fe))

#define g_bProbeAIndex           (*(BYTE *)(intptr_t)(nw_data_window + 0x67fd))

#define g_bProbeBIndex           (*(BYTE *)(intptr_t)(nw_data_window + 0x6803))

#define g_bReachedCell           (*(BYTE *)(intptr_t)(nw_data_window + 0x6804))

#define g_bRowFar                (*(uint8_t *)(intptr_t)(nw_data_window + 0x635a))

#define g_bRowOrigin             (*(uint8_t *)(intptr_t)(nw_data_window + 0x635b))

#define g_bSaveLoadActive        (*(u8 *)(intptr_t)(nw_data_window + 0x67f7))

#define g_bSentinelGroupA        (*(BYTE *)(intptr_t)(nw_data_window + 0x67fd))

#define g_bSentinelGroupB        (*(BYTE *)(intptr_t)(nw_data_window + 0x6803))

#define g_bTrackCellExtents      (*(BYTE *)(intptr_t)(nw_data_window + 0x1202))

#define g_bUtSlot4Result         (*(uint8_t *)(intptr_t)(nw_data_window + 0x6806))

#define g_bUtSlot5Result         (*(uint8_t *)(intptr_t)(nw_data_window + 0x6807))

#define g_bVmAudioStateByte      (*(uint8_t *)(intptr_t)(nw_data_window + 0x6345))

#define g_blit_mode_byte         (*(uint8_t *)(intptr_t)(nw_data_window + 0x1201))

#define g_childHeightDefault     (*(int *)(intptr_t)(nw_data_window + 0x6074))

#define g_childWidthDefault      (*(int *)(intptr_t)(nw_data_window + 0x6070))

#define g_childX                 (*(int *)(intptr_t)(nw_data_window + 0x6068))

#define g_childY                 (*(int *)(intptr_t)(nw_data_window + 0x606c))

#define g_coord_base_x           (*(int32_t *)(intptr_t)(nw_data_window + 0x6348))

#define g_coord_base_y           (*(int32_t *)(intptr_t)(nw_data_window + 0x634c))

#define g_daylight               (*(int32_t *)(intptr_t)(nw_data_window + 0xf58))

#define g_default_grid_record    (*(GridCellRec *)(intptr_t)(nw_data_window + 0x74))

#define g_default_recsel         (*(uint8_t *)(intptr_t)(nw_data_window + 0x1202))

#define g_display_object NW_WINDOW_PTR_REF(void, 0x11b0)

#define g_dwAnchorX              (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f48))

#define g_dwAnchorY              (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f4c))

#define g_dwAudioSlotCount       (*(uint32_t *)(intptr_t)(nw_data_window + 0x5ea8))

#define g_dwAudioSlotCountSource (*(uint32_t *)(intptr_t)(nw_data_window + 0x635c))

#define g_dwAxisShift            (*(uint32_t *)(intptr_t)(nw_data_window + 0x6254))

#define g_dwCellHeight           (*(int32_t *)(intptr_t)(nw_data_window + 0x6270))

#define g_dwCellRowShift         (*(uint32_t *)(intptr_t)(nw_data_window + 0x6254))

#define g_dwCellScaleX           (*(int32_t *)(intptr_t)(nw_data_window + 0x626c))

#define g_dwCellScaleY           (*(int32_t *)(intptr_t)(nw_data_window + 0x6270))

#define g_dwCellWidth            (*(int32_t *)(intptr_t)(nw_data_window + 0x626c))

#define g_dwClientLeft           (*(int32_t *)(intptr_t)(nw_data_window + 0x6264))

#define g_dwClientTop            (*(int32_t *)(intptr_t)(nw_data_window + 0x6268))

#define g_dwCmdArg0              (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f00))

#define g_dwCmdArg1              (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f04))

#define g_dwCurrent1             (*(DWORD *)(intptr_t)(nw_data_window + 0x5f34))

#define g_dwCursorWords          (*(uint32_t *)(intptr_t)(nw_data_window + 0x1610))

#define g_dwCursorX              (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f40))

#define g_dwCursorY              (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f44))

/* Pool/record table base: a pointer slot, relocated through the native side
 * table on 64-bit builds.  The raw 32-bit window word is cleared once the
 * snapshot is converted, so reading it directly yields NULL. */
#define g_dwGridCtxRecordBase    ((uint32_t)NW_WINDOW_PTR_VALUE(0x624c))

#define g_dwHeapCursor           (*(uint32_t *)(intptr_t)(nw_data_window + 0x160c))

#define g_dwMciAudioEnabled      (*(DWORD *)(intptr_t)(nw_data_window + 0x1e24))

#define g_dwMciStatusReply       (*(uint32_t *)(intptr_t)(nw_data_window + 0x60bc))

#define g_dwMciTaskId            (*(MCIDEVICEID *)(intptr_t)(nw_data_window + 0x1e20))

#define g_dwOpenExtraSource      (*(uint32_t *)(intptr_t)(nw_data_window + 0x6404))

#define g_dwPaneBaseX            (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f28))

#define g_dwPaneBaseY            (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f2c))

#define g_dwPaneDeltaX           (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f38))

#define g_dwPaneDeltaY           (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f3c))

#define g_dwPendingHookResult    (*(uint32_t *)(intptr_t)(nw_data_window + 0x6364))

#define g_dwPointOffsetX         (*(int32_t *)(intptr_t)(nw_data_window + 0x6348))

#define g_dwPointOffsetY         (*(int32_t *)(intptr_t)(nw_data_window + 0x634c))

#define g_dwRefDescriptorCount   (*(uint32_t *)(intptr_t)(nw_data_window + 0x6204))

#define g_dwSavedExtent          (*(u32 *)(intptr_t)(nw_data_window + 0x16c4))

#define g_dwSavedRecordCount     (*(u32 *)(intptr_t)(nw_data_window + 0x16bc))

#define g_dwScrollX              (*(int32_t *)(intptr_t)(nw_data_window + 0x6348))

#define g_dwScrollY              (*(int32_t *)(intptr_t)(nw_data_window + 0x634c))

#define g_dwSelPairAH            (*(uint32_t *)(intptr_t)(nw_data_window + 0x63fc))

#define g_dwSelPairAV            (*(uint32_t *)(intptr_t)(nw_data_window + 0x63f8))

#define g_dwSelPairBH            (*(uint32_t *)(intptr_t)(nw_data_window + 0x63f4))

#define g_dwSelPairBV            (*(uint32_t *)(intptr_t)(nw_data_window + 0x63f0))

#define g_dwSizeHighWord         (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f24))

#define g_dwSizeLowWord          (*(uint32_t *)(intptr_t)(nw_data_window + 0x5f20))

#define g_dwSlotAuxZeroedA       (*(uint32_t *)(intptr_t)(nw_data_window + 0x5eac))

#define g_dwSlotAuxZeroedB       (*(uint32_t *)(intptr_t)(nw_data_window + 0x5eb0))

#define g_dwSlotCount            (*(uint32_t *)(intptr_t)(nw_data_window + 0x5ea8))

#define g_dwSourceFileOpenSlot   (*(DWORD *)(intptr_t)(nw_data_window + 0x5f58))

#define g_dwSourceRowShift       (*(uint32_t *)(intptr_t)(nw_data_window + 0x6254))

#define g_dwViewHeight           (*(int32_t *)(intptr_t)(nw_data_window + 0x61f4))

#define g_dwViewWidth            (*(int32_t *)(intptr_t)(nw_data_window + 0x61f0))

#define g_flag_417801            (*(uint8_t *)(intptr_t)(nw_data_window + 0x6801))

#define g_hAudioInitContext NW_WINDOW_PTR_REF(HANDLE, 0x1ea8)

#define g_hdcDest NW_WINDOW_PTR_REF(HDC, 0x5f00)

#define g_hwndChild NW_WINDOW_PTR_REF(HWND, 0x60bc)

#define g_hwndHelpWindow NW_WINDOW_PTR_REF(HWND, 0x6404)

#define g_hwndMainWindow NW_WINDOW_PTR_REF(HWND, 0x6404)

#define g_hwndPlayerOrMainUnknown NW_WINDOW_PTR_REF(HANDLE, 0x6404)

#define g_nBearingCode           (*(int *)(intptr_t)(nw_data_window + 0x1c9c))

#define g_nCellUp                (*(int *)(intptr_t)(nw_data_window + 0x63dc))

#define g_nColumnsPerRow         (*(int *)(intptr_t)(nw_data_window + 0x61e8))

#define g_nCoordH                (*(int *)(intptr_t)(nw_data_window + 0x1c94))

#define g_nCoordV                (*(int *)(intptr_t)(nw_data_window + 0x1c98))

#define g_nExtentOffsetX         (*(int *)(intptr_t)(nw_data_window + 0x6348))

#define g_nExtentOffsetY         (*(int *)(intptr_t)(nw_data_window + 0x634c))

#define g_nFileSlot              (*(int *)(intptr_t)(nw_data_window + 0x5f54))

#define g_nGridColumns           (*(int *)(intptr_t)(nw_data_window + 0x61e8))

#define g_nGridColumnsM1         (*(int *)(intptr_t)(nw_data_window + 0x63d8))

#define g_nGridMaxX              (*(int *)(intptr_t)(nw_data_window + 0x61f0))

#define g_nGridRowCount          (*(int *)(intptr_t)(nw_data_window + 0x61ec))

#define g_nGridRows              (*(int *)(intptr_t)(nw_data_window + 0x61ec))

#define g_nGridRowsM1            (*(int *)(intptr_t)(nw_data_window + 0x63e0))

#define g_nLocatedCellX          (*(int *)(intptr_t)(nw_data_window + 0x1c94))

#define g_nLocatedCellY          (*(int *)(intptr_t)(nw_data_window + 0x1c98))

#define g_nRowCount              (*(int *)(intptr_t)(nw_data_window + 0x61ec))

#define g_nRowStride             (*(int *)(intptr_t)(nw_data_window + 0x61e8))

#define g_nSourceRowShift        (*(int *)(intptr_t)(nw_data_window + 0x6254))

#define g_nStartCol              (*(int *)(intptr_t)(nw_data_window + 0x1c94))

#define g_nStartRow              (*(int *)(intptr_t)(nw_data_window + 0x1c98))

#define g_nStepCode              (*(int *)(intptr_t)(nw_data_window + 0x1ca0))

#define g_nStreamFileSlot        (*(int *)(intptr_t)(nw_data_window + 0x5f54))

#define g_nTargetCol             (*(int *)(intptr_t)(nw_data_window + 0x1c8c))

#define g_nTargetRow             (*(int *)(intptr_t)(nw_data_window + 0x1c90))

#define g_nTileTableRowShift     (*(int *)(intptr_t)(nw_data_window + 0x6254))

#define g_pCellGrid NW_WINDOW_PTR_REF(uint16_t, 0x6250)

#define g_pCellTableEnd NW_WINDOW_PTR_REF(GridCellTableEntry, 0x6240)

#define g_pColorTableBlock NW_WINDOW_PTR_REF(uint8_t, 0x6310)

#define g_pFrameCtl              NW_WINDOW_PTR_REF(PFRAMECTL, 0x6378)

#define g_pGridSpanChainBottom NW_WINDOW_PTR_REF(uint8_t, 0x6244)

#define g_pGridSpanChainTop NW_WINDOW_PTR_REF(uint8_t, 0x6240)

#define g_pInputState NW_WINDOW_PTR_REF(uint8_t, 0x5efc)

#define g_pNetwalkImageRecord NW_WINDOW_PTR_REF(NetwalkImageRecord, 0x5ef8)

#define g_pNetwalkTileSignature NW_WINDOW_PTR_REF(NetwalkTileSignature, 0x5ef4)

#define g_pSavedStateCursor NW_WINDOW_PTR_REF(u8, 0x16c0)

#define g_pUtRegistration NW_WINDOW_PTR_REF(void, 0x6400)

#define g_pdwSlotArgTable NW_WINDOW_PTR_REF(uint32_t, 0x6328)

#define g_pdwTileRecord          (*(DWORD *)(intptr_t)(nw_data_window + 0x5ef4))

#define g_ppszEntryTable NW_WINDOW_PTR_REF(char, 0x1198)

#define g_sprite_list_bot NW_WINDOW_PTR_REF(void, 0x6244)

#define g_sprite_list_top NW_WINDOW_PTR_REF(void, 0x6240)

#define g_state_417388           (*(int32_t *)(intptr_t)(nw_data_window + 0x6388))

#define g_timezone               (*(int32_t *)(intptr_t)(nw_data_window + 0xf50))

#define g_tzset_called           (*(int32_t *)(intptr_t)(nw_data_window + 0xcc8))

#define g_view_clamp_a           (*(int32_t *)(intptr_t)(nw_data_window + 0x11f8))

#define g_view_clamp_b           (*(int32_t *)(intptr_t)(nw_data_window + 0x11fc))

#define g_view_flag_7d           (*(uint8_t *)(intptr_t)(nw_data_window + 0x627d))

/* VM operand mirrors. A/B are the current cell coordinates; the *Key slots are
 * the low/high halves packed into the cursor record's cell key. */
#define g_vmOperandB             (*(uint32_t *)(intptr_t)(nw_data_window + 0x6220))

#define g_vmOperandA             (*(uint32_t *)(intptr_t)(nw_data_window + 0x6224))

#define g_vmOperandBKey          (*(uint32_t *)(intptr_t)(nw_data_window + 0x6228))

#define g_vmOperandAKey          (*(uint32_t *)(intptr_t)(nw_data_window + 0x622c))

#define g_wBounceDelta           (*(int16_t *)(intptr_t)(nw_data_window + 0x6356))

#define g_wBounceThreshold       (*(uint16_t *)(intptr_t)(nw_data_window + 0x6354))

#define g_wCellIndex             (*(uint16_t *)(intptr_t)(nw_data_window + 0x6350))

#define g_wMaxStepPixels         (*(uint16_t *)(intptr_t)(nw_data_window + 0x6352))

#endif /* NWGLOBALS_H */
