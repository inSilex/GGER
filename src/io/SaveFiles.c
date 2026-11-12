#include "nwarch.h"
/*
 * SaveFiles -- save-file headers, regions, and load/save orchestration.
 *
 * Module: io
 * Interface: include/SaveFiles.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   ClearBufferRegionsFromDescriptor                           io/ClearBufferRegionsFromDescriptor.c
 *   CloseSaveFileSlot                                          io/CloseSaveFileSlot.c
 *   CloseSaveSlotAndLatchInvalidDataError                      io/CloseSaveSlotAndLatchInvalidDataError.c
 *   ForwardCloseSaveFileSlot                                   io/ForwardCloseSaveFileSlot.c
 *   InitializeDispatchRecords                                  io/InitializeDispatchRecords.c
 *   LoadDecryptRegionsFromDescriptor                           io/LoadDecryptRegionsFromDescriptor.c
 *   LoadOrRepairSaveFileRegions                                io/LoadOrRepairSaveFileRegions.c
 *   LoadRecordBlocksIntoTileCache                              io/LoadRecordBlocksIntoTileCache.c
 *   WriteBufferRegionsScrambledToStream                        io/WriteBufferRegionsScrambledToStream.c
 *   WriteSaveFileBuffersAndLoadTileCache                       io/WriteSaveFileBuffersAndLoadTileCache.c
 *   WriteSaveFileHeaderAndBufferRegions                        io/WriteSaveFileHeaderAndBufferRegions.c
 *   WriteSaveFileHeaderAndScrambledRegions                     io/WriteSaveFileHeaderAndScrambledRegions.c
 */

/* ==========================================================================
 * Unit: ClearBufferRegionsFromDescriptor
 * Source: io/ClearBufferRegionsFromDescriptor.c
 * ========================================================================== */
#ifdef NW_UNIT_ClearBufferRegionsFromDescriptor
/*
 * Clear Buffer Regions From Descriptor.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-clearbufferregionsfromdescriptor-c
 */

#include <stdint.h>

extern unsigned char *__stdcall FillMemoryWithZeroBytes(unsigned int dwCount,
                                                        unsigned char *pDst);

extern uintptr_t g_base_ptr_a; 

typedef struct {
    uint32_t dwOffsetFromBase; /* +0x00, guest offset from g_base_ptr_a */
    uint32_t dwByteCount;      /* +0x04, bytes to zero */
} BufferRegionRecord;

typedef struct {
    uint32_t dwRegionTally;      /* +0x00, number of records that follow */
    /* BufferRegionRecord records[dwRegionTally] follow at +0x04 */
} BufferRegionDescriptor;

_Static_assert(sizeof(BufferRegionRecord) == 8,
               "save region records are two guest dwords");

void __stdcall ClearBufferRegionsFromDescriptor(BufferRegionDescriptor *pDescriptor)
{
    BufferRegionRecord *pRecord;
    int iRegion;

    if (pDescriptor == 0) {
        return;
    }

    if (pDescriptor->dwRegionTally == 0u) {
        return;
    }

    for (iRegion = (int)pDescriptor->dwRegionTally - 1; iRegion >= 0; --iRegion) {
        pRecord = (BufferRegionRecord *)((unsigned char *)pDescriptor
                                        + 4 + 8 * iRegion);

        /* dwByteCount is pushed LAST and so becomes the callee's FIRST
         * argument; pDst is pushed first and becomes its SECOND. */
        FillMemoryWithZeroBytes(pRecord->dwByteCount,
                                (unsigned char *)(g_base_ptr_a
                                                 + pRecord->dwOffsetFromBase));
    }
}
#endif /* NW_UNIT_ClearBufferRegionsFromDescriptor */

/* ==========================================================================
 * Unit: CloseSaveFileSlot
 * Source: io/CloseSaveFileSlot.c
 * ========================================================================== */
#ifdef NW_UNIT_CloseSaveFileSlot
/*
 * Close Save File Slot.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-closesavefileslot-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

extern int __fastcall CloseTrackedHandleSlot(DWORD dwUnusedArg1,
                                              DWORD dwUnusedArg2,
                                              int   nSlotIndex);

#define SAVE_FILE_SLOT_INDEX ((int *)(nw_data_window + 0x5f54))

#define SLOT_INDEX_NONE      (-1)   

void __fastcall CloseSaveFileSlot(DWORD dwUnusedArg1, DWORD dwUnusedArg2)
{
    int *pnSaveFileSlotIndex = SAVE_FILE_SLOT_INDEX;
    int nSlotIndex;

    nSlotIndex = *pnSaveFileSlotIndex;

    if (nSlotIndex > 0) {
        CloseTrackedHandleSlot(dwUnusedArg1, dwUnusedArg2, nSlotIndex);
        *pnSaveFileSlotIndex = SLOT_INDEX_NONE;
    }
}
#endif /* NW_UNIT_CloseSaveFileSlot */

/* ==========================================================================
 * Unit: CloseSaveSlotAndLatchInvalidDataError
 * Source: io/CloseSaveSlotAndLatchInvalidDataError.c
 * ========================================================================== */
#ifdef NW_UNIT_CloseSaveSlotAndLatchInvalidDataError
/*
 * Closes the save slot and latches the invalid-data error.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-closesaveslotandlatchinvaliddataerror-c
 */

#include <windows.h>

extern void __fastcall CloseSaveFileSlot(DWORD dwUnusedArg1, DWORD dwUnusedArg2);

extern DWORD __stdcall SetStateFlagOnce(DWORD dwValueOrMessage);

static const char g_szInvalidGameDataMessage[] = "Invalid game data file.";

DWORD __fastcall CloseSaveSlotAndLatchInvalidDataError(DWORD dwUnusedArg1,
                                                       DWORD dwUnusedArg2)
{
    /* Release the tracked handle before latching, so the file is already closed by the
     * time the message loop can act on the flag. */
    CloseSaveFileSlot(dwUnusedArg1, dwUnusedArg2);

    /* First-writer-wins: publishes the fatal diagnostic if no earlier error claimed it.
     * The callee zeroes EAX, which is this function's return value (0 = failure). */
    (void)SetStateFlagOnce((DWORD)(uintptr_t)g_szInvalidGameDataMessage);

    return 0;
}
#endif /* NW_UNIT_CloseSaveSlotAndLatchInvalidDataError */

/* ==========================================================================
 * Unit: ForwardCloseSaveFileSlot
 * Source: io/ForwardCloseSaveFileSlot.c
 * ========================================================================== */
#ifdef NW_UNIT_ForwardCloseSaveFileSlot
/*
 * Forward Close Save File Slot.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-forwardclosesavefileslot-c
 */

#include <windows.h>

extern void __fastcall CloseSaveFileSlot(DWORD dwUnusedArg1, DWORD dwUnusedArg2);

void ForwardCloseSaveFileSlot(void)
{
    CloseSaveFileSlot(0, 0);
}
#endif /* NW_UNIT_ForwardCloseSaveFileSlot */

/* ==========================================================================
 * Unit: InitializeDispatchRecords
 * Source: io/InitializeDispatchRecords.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeDispatchRecords
/*
 * Load the record-dispatch image described by the save header: open NETWALK<variant>.SAV, validate the two-dword image signature, read the
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-initializedispatchrecords-c
 */

#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

/* nwglobals.h uses the project-wide u8 spelling; define it before the
 * macros are expanded. */
typedef uint8_t u8;

typedef struct tagFrameCtl {
    uint8_t  bRunning;        /* +0x00 */
    uint8_t  bFrameDivisor;   /* +0x01 */
    uint8_t  bCharPending;    /* +0x02 */
    uint8_t  bCharLatched;    /* +0x03 */
    uint32_t dwFrameCount;    /* +0x04 */
} FRAMECTL, *PFRAMECTL;

typedef struct NwSaveRecord {
    uint32_t pReadDest;       /* +0x00, only the writer gates on this */
    uint32_t dwStreamOffset;  /* +0x04, not used by this loader */
    uint32_t pArena;          /* +0x08 */
    uint32_t dwSlotCount;     /* +0x0c */
} NwSaveRecord;

typedef struct NwSaveRegionDescriptor NwSaveRegionDescriptor;

_Static_assert(sizeof(FRAMECTL) == 8, "frame-control record is 8 bytes");
_Static_assert(offsetof(FRAMECTL, bFrameDivisor) == 1, "divisor is +1");
_Static_assert(offsetof(FRAMECTL, dwFrameCount) == 4, "frame count is +4");
_Static_assert(sizeof(NwSaveRecord) == 16, "save descriptor is 16 bytes");
_Static_assert(offsetof(NwSaveRecord, pArena) == 8, "arena is +8");
_Static_assert(offsetof(NwSaveRecord, dwSlotCount) == 0x0c, "count is +0xc");

/* Record-state slots in nw_data_window, named as in the save writer. */
#define g_dwAuxRecordIndex    (*(uint32_t *)(void *)(nw_data_window + 0x62f8))
#define g_pStateTable         NW_WINDOW_PTR_REF(uint8_t, 0x6248)
#define g_dwSlotRegionBase    (*(uint32_t *)(void *)(nw_data_window + 0x6304))
#define g_dwRecordArrayBase   NW_WINDOW_PTR_VALUE(0x630c)
#define g_dwSlotPointerDelta  (*(uint32_t *)(void *)(nw_data_window + 0x6318))

#define g_bRecordLoadActive   (*(uint8_t *)(void *)(nw_data_window + 0x67f9))

#define g_pLoadRegionDesc0 NW_WINDOW_PTR_REF(NwSaveRegionDescriptor, 0x637c)
#define g_pLoadRegionDesc1 NW_WINDOW_PTR_REF(NwSaveRegionDescriptor, 0x6380)
#define g_pLoadRegionDesc2 NW_WINDOW_PTR_REF(NwSaveRegionDescriptor, 0x6384)

extern unsigned char g_abSaveFileHeaderRecord[76]; 
extern char g_szSaveVariantSuffix[4];              
extern char g_szRuntimeSourcePath[];               

extern int __stdcall OpenTempPathOrExeVariantSourceFile(int nVariantNumber,
                                                        const char *pszSuffix,
                                                        const char *pszSourcePath);
extern int __stdcall ReadStreamChunkFromDescriptorVerifyCount(uint32_t cbBytes,
                                                              char *lpBuffer);
extern void __cdecl ReadStreamHeaderRecordIntoGlobal(void);
extern void __stdcall ForwardStackArgToSlotHook(uint32_t dwArg);
extern uint32_t __stdcall DispatchPendingCommandLoop(uint32_t arg0);
extern int __stdcall LoadDecryptRegionsFromDescriptor(
    NwSaveRegionDescriptor *pDescriptor);
extern uint32_t *__stdcall AllocHeapBlock(uint32_t dwSizeBytes);
extern void __stdcall ApplyDeltaToUnmarkedEntries(int nDelta, int nEntryCount,
                                                  uint8_t *pEntries);
extern uint32_t __stdcall LoadRecordBlocksIntoTileCache(uint8_t flagA,
                                                        uint32_t recordIndex);
extern void __fastcall CloseGlobalSlotAndResetToUnused(uint32_t dwUnusedArg1,
                                                        uint32_t dwUnusedArg2);
extern int __stdcall UpdateMciAudioStateAndPlay(unsigned char bMciAudioMode);
extern int __stdcall DispatchPendingSlotHookForIndex(uint32_t dwSlotIndex);
extern BOOL __stdcall InstallActiveGridCellRef(uint8_t bRefreshExtents,
                                               int nRecordIndex);
extern void __cdecl DispatchExceptionOnGlobalScopeRecord(void);
extern int __cdecl TruncateGlobalSlotFileToZeroLength(void);

NW_DIAG int __stdcall InitializeDispatchRecords(uint32_t nVariantNumber)
{
    PFRAMECTL pFrameCtl;
    uint32_t aImageSignature[2];
    uint32_t iRecord;
    uint32_t dwRecordIndex;
    uint32_t dwSlotCount;
    uint32_t cbArena;
    uint32_t iSlot;
    uint8_t *pArena;
    uint8_t *pSlots;
    uint8_t bSavedModeEnabled;
    uint8_t bSavedPendingDispatch;
    uint8_t bSavedOpZeroOrOne;
    uint8_t bSavedModeSelector;
    uint8_t bSavedPlaybackArmed;
    int32_t nMciMode;
    int32_t nPendingHookIndex;
    int32_t nActiveRecordIndex;

    if ((uint8_t)OpenTempPathOrExeVariantSourceFile(
            (int)nVariantNumber, g_szSaveVariantSuffix,
            g_szRuntimeSourcePath) == 0u) {
        return 0;
    }

    if ((uint8_t)ReadStreamChunkFromDescriptorVerifyCount(
            8u, (char *)aImageSignature) == 0u ||
        aImageSignature[0] != 0x3d53563du ||
        aImageSignature[1] != 0x80000018u) {
        (void)TruncateGlobalSlotFileToZeroLength();
        CloseGlobalSlotAndResetToUnused(0u, 0u);
        return 0;
    }

    /* The header reader overwrites the whole 76-byte record.  These five
     * bytes are runtime state, not file state, and are restored afterwards. */
    bSavedModeEnabled      = g_abSaveFileHeaderRecord[0x14]; 
    bSavedPendingDispatch  = g_abSaveFileHeaderRecord[0x15]; 
    bSavedOpZeroOrOne      = g_abSaveFileHeaderRecord[0x16]; 
    bSavedModeSelector     = g_abSaveFileHeaderRecord[0x17]; 
    bSavedPlaybackArmed    = g_abSaveFileHeaderRecord[0x18]; 

    ReadStreamHeaderRecordIntoGlobal();

    g_abSaveFileHeaderRecord[0x14] = bSavedModeEnabled;
    g_abSaveFileHeaderRecord[0x15] = bSavedPendingDispatch;
    g_abSaveFileHeaderRecord[0x16] = bSavedOpZeroOrOne;
    g_abSaveFileHeaderRecord[0x17] = bSavedModeSelector;
    g_abSaveFileHeaderRecord[0x18] = bSavedPlaybackArmed;

    g_abSaveFileHeaderRecord[0x31] = g_bEaseTarget0; 
    g_abSaveFileHeaderRecord[0x32] = 0u;             

    ForwardStackArgToSlotHook(0u);

    /* Snapshot the file-backed fields before the command loop runs. */
    nMciMode = (int32_t)(int8_t)g_abSaveFileHeaderRecord[0x19]; 
    nPendingHookIndex = *(int32_t *)(void *)(
        g_abSaveFileHeaderRecord + 0x38);                       
    nActiveRecordIndex = *(int32_t *)(void *)(
        g_abSaveFileHeaderRecord + 0x3c);                       

    g_bRecordLoadActive = 1u;
    g_bSaveLoadActive = 1u;
    g_bActiveCellReady = 0u;

    DispatchPendingCommandLoop(
        *(uint32_t *)(void *)(g_abSaveFileHeaderRecord + 0x08));

    (void)LoadDecryptRegionsFromDescriptor(g_pLoadRegionDesc0);
    (void)LoadDecryptRegionsFromDescriptor(g_pLoadRegionDesc1);
    (void)LoadDecryptRegionsFromDescriptor(g_pLoadRegionDesc2);

    for (iRecord = 0; iRecord < g_dwAuxRecordIndex; ++iRecord) {
        NwSaveRecord *pRecord;
        uint32_t dwDescriptorAddress;

        (void)ReadStreamChunkFromDescriptorVerifyCount(
            4u, (char *)&dwRecordIndex);

        dwDescriptorAddress = g_dwRecordArrayBase +
                              (dwRecordIndex << 4);
        pRecord = (NwSaveRecord *)(uintptr_t)nw_guest_to_host(dwDescriptorAddress);

        (void)ReadStreamChunkFromDescriptorVerifyCount(
            4u, (char *)&dwSlotCount);
        pRecord->dwSlotCount = dwSlotCount;

        cbArena = (dwSlotCount << 4) + 2u * g_dwSlotRegionBase;
        pArena = (uint8_t *)AllocHeapBlock(cbArena);
        pRecord->pArena = (uint32_t)(uintptr_t)pArena;

        (void)ReadStreamChunkFromDescriptorVerifyCount(
            cbArena, (char *)pArena);

        pSlots = pArena + 2u * g_dwSlotRegionBase;
        ApplyDeltaToUnmarkedEntries((int)g_dwSlotPointerDelta,
                                    (int)dwSlotCount, pSlots);

        for (iSlot = 0; iSlot < dwSlotCount; ++iSlot) {
            uint8_t bFlags = pSlots[0];

            if ((bFlags & 0x02u) != 0u && (bFlags & 0x08u) != 0u) {
                uint32_t cbPayload =
                    g_pStateTable[((uint32_t)pSlots[1] << 4) + 3u];
                uint8_t *pPayload =
                    (uint8_t *)AllocHeapBlock(cbPayload);

                *(uint32_t *)(void *)(pSlots + 0x0c) =
                    (uint32_t)(uintptr_t)pPayload;
                (void)ReadStreamChunkFromDescriptorVerifyCount(
                    cbPayload, (char *)pPayload);
            }

            pSlots += 16;
        }
    }

    g_bRecordLoadActive = 0u;
    g_bSaveLoadActive = 0u;

    (void)LoadRecordBlocksIntoTileCache(
        0u, *(uint32_t *)(void *)(g_abSaveFileHeaderRecord + 0x0c));
    CloseGlobalSlotAndResetToUnused(0u, 0u);

    pFrameCtl = g_pFrameCtl;
    pFrameCtl->bFrameDivisor = g_abSaveFileHeaderRecord[0x33];
    pFrameCtl->dwFrameCount =
        *(uint32_t *)(void *)(g_abSaveFileHeaderRecord + 0x34);

    if (nMciMode >= 0) {
        (void)UpdateMciAudioStateAndPlay((unsigned char)nMciMode);
    }
    if (nPendingHookIndex >= 0) {
        (void)DispatchPendingSlotHookForIndex((uint32_t)nPendingHookIndex);
    }

    g_abSaveFileHeaderRecord[0x32] = 0u;
    if (nActiveRecordIndex >= 0) {
        (void)InstallActiveGridCellRef(0u, (int)nActiveRecordIndex);
    }

    g_bActiveCellReady = 1u;
    DispatchExceptionOnGlobalScopeRecord();
    return 0;
}
#endif /* NW_UNIT_InitializeDispatchRecords */

/* ==========================================================================
 * Unit: LoadDecryptRegionsFromDescriptor
 * Source: io/LoadDecryptRegionsFromDescriptor.c
 * ========================================================================== */
#ifdef NW_UNIT_LoadDecryptRegionsFromDescriptor
/*
 * Walks the caller-supplied buffer-region descriptor backward, loading each listed region from the program's global stream into its slot in the...
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-loaddecryptregionsfromdescriptor-c
 */

#include <stdint.h>

extern int __stdcall ReadStreamChunkFromDescriptorVerifyCount(unsigned int cbRequestedBytes,
                                                            char          *lpBuffer);

extern void __stdcall ApplyRollingKeyXorToBuffer(unsigned int dwKeyIndex,
                                                 int           nCount,
                                                 unsigned char *pBuf);

extern uintptr_t g_base_ptr_a; 

typedef struct {
    uint32_t dwOffsetFromBase; /* +0x00, guest offset from g_base_ptr_a;
                                *       also the rolling-XOR key index   */
    uint32_t dwByteCount;      /* +0x04, bytes to read, then to decrypt */
} BufferRegionRecord;

typedef struct {
    uint32_t dwRegionTally;      /* +0x00, number of records that follow */
    /* BufferRegionRecord records[dwRegionTally] follow at +0x04 */
} BufferRegionDescriptor;

_Static_assert(sizeof(BufferRegionRecord) == 8,
               "save region records are two guest dwords");

/* Returns 1 when every listed region was read in full and decrypted, 0 when
 * the first short read aborted the walk, and also 1 for a NULL descriptor. */
int __stdcall LoadDecryptRegionsFromDescriptor(BufferRegionDescriptor *pRegionDescriptor)
{
    BufferRegionRecord *pRecord;
    unsigned char      *pBuf;
    unsigned int        dwKeyIndex;
    unsigned int        dwByteCount;
    int                 iRegion;

    if (pRegionDescriptor == 0) {
        return 1;                       /* JZ to the AL=1 epilogue: success */
    }

    if (pRegionDescriptor->dwRegionTally == 0u) {
        return 1;                       /* same epilogue as the completed loop */
    }

    for (iRegion = (int)pRegionDescriptor->dwRegionTally - 1; iRegion >= 0; --iRegion) {
        pRecord     = (BufferRegionRecord *)((unsigned char *)pRegionDescriptor
                                             + 4 + 8 * iRegion);
        dwKeyIndex  = pRecord->dwOffsetFromBase;  /* +0x00 */
        dwByteCount = pRecord->dwByteCount;        /* +0x04 */

        pBuf = (unsigned char *)(g_base_ptr_a + pRecord->dwOffsetFromBase);

        /* dwByteCount is pushed LAST and so becomes the callee's FIRST
         * argument; pBuf is pushed first and becomes its SECOND. */
        if (!ReadStreamChunkFromDescriptorVerifyCount(dwByteCount, (char *)pBuf)) {
            return 0;                   
        }

        ApplyRollingKeyXorToBuffer(dwKeyIndex, (int)dwByteCount, pBuf);
    }

    return 1;
}
#endif /* NW_UNIT_LoadDecryptRegionsFromDescriptor */

/* ==========================================================================
 * Unit: LoadOrRepairSaveFileRegions
 * Source: io/LoadOrRepairSaveFileRegions.c
 * ========================================================================== */
#ifdef NW_UNIT_LoadOrRepairSaveFileRegions
/*
 * Load Or Repair Save File Regions.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-loadorrepairsavefileregions-c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
extern unsigned char nw_data_window[];

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_log_mark(const char *stage);
static void nw_save_markf(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    platform_log_mark(line);
}
#else
#define nw_save_markf(...) ((void)0)
#endif

/* --- callees: names resolved live through /get_function_by_address ---------- */
extern int   __stdcall OpenTempPathOrExeVariantSourceFile(int nVariantNumber,
                                                          const char *pszSuffix,
                                                          const char *pszSourcePath);
extern int   __stdcall WriteSaveFileHeaderAndScrambledRegions(int32_t nVariantNumber);
extern int   __stdcall ReadStreamChunkFromDescriptorVerifyCount(uint32_t dwBytes,
                                                                void *pBuf);
extern void  __stdcall ReadStreamHeaderRecordIntoGlobal(void);
extern void  __stdcall TruncateGlobalSlotFileToZeroLength(void);
extern void CloseGlobalSlotAndResetToUnused(void);
extern int   __stdcall RewindGlobalSlotFileToStart(void);
extern int   __stdcall SeekTrackedHandleAbsoluteOffset(int32_t lDistanceToMove);
extern int   __stdcall LoadDecryptRegionsFromDescriptor(intptr_t pRegionDescriptor);
extern int   __stdcall ClearBufferRegionsFromDescriptor(intptr_t pDescriptor);

int __stdcall LoadOrRepairSaveFileRegions(int32_t nVariantNumber)
{
    uint32_t signature[2];   /* the 8-byte local at [ESP+0x4] */
    int      openRet;
    int      iteration;

    nw_save_markf("save_repair_begin_%d", (int)nVariantNumber);
    openRet = OpenTempPathOrExeVariantSourceFile(
        nVariantNumber,
        (const char *)(intptr_t)(nw_data_window + 0x632c),
        (const char *)(intptr_t)(nw_data_window + 0x6390));
    nw_save_markf("save_repair_open_%d_fd_%u", openRet,
                  *(unsigned int *)(nw_data_window + 0x5f58));
    if ((openRet & 0xFF) == 0)
        return 0;                                  

    if (*(const volatile unsigned char *)(intptr_t)(nw_data_window + 0x67f9) != 0) {

        for (iteration = 0;; iteration++) {
            int matches;
            int readOk;
            int writeRet;

            /* PUSH &signature / PUSH 0x8 - count first, buffer second. */
            readOk = ReadStreamChunkFromDescriptorVerifyCount(0x8u, signature);
            if (readOk)
                matches = (signature[0] == 0x3d53563du &&
                           signature[1] == 0x80000018u);
            else
                matches = 0;                        /* short read = mismatch */

            nw_save_markf("save_repair_iter_%d_ok_%d_sig_%08x_%08x",
                          iteration, readOk, signature[0], signature[1]);
            if (matches) {
                nw_save_markf("save_repair_match_%d", iteration);
                break;
            }

            /* Corrupt/absent signature: throw the file away and rebuild it. */
            nw_save_markf("save_repair_trunc_%d", iteration);
            (void)TruncateGlobalSlotFileToZeroLength();
            CloseGlobalSlotAndResetToUnused();
            nw_save_markf("save_repair_rewrite_%d", iteration);
            /* Keep the rebuild outside nw_save_markf: on desktop the macro
             * discards its arguments, which would skip the write entirely. */
            writeRet = (int)WriteSaveFileHeaderAndScrambledRegions(nVariantNumber);
            nw_save_markf("save_repair_rewrite_ret_%d", writeRet);
            (void)writeRet;

            openRet = OpenTempPathOrExeVariantSourceFile(
                nVariantNumber,
                (const char *)(intptr_t)(nw_data_window + 0x632c),
                (const char *)(intptr_t)(nw_data_window + 0x6390));
            nw_save_markf("save_repair_reopen_%d_ret_%d_fd_%u", iteration,
                          openRet, *(unsigned int *)(nw_data_window + 0x5f58));
            if ((openRet & 0xFF) == 0)
                return 0;                                 
            if (iteration >= 7) {
                nw_save_markf("save_repair_cap_%d", iteration);
                return 0;
            }
        }
        nw_save_markf("save_repair_loaded");

        ReadStreamHeaderRecordIntoGlobal();
        *(volatile unsigned char *)(intptr_t)(nw_data_window + 0x635d) =
            *(const volatile unsigned char *)(intptr_t)(nw_data_window + 0x67fa);
        *(volatile unsigned char *)(intptr_t)(nw_data_window + 0x635e) =
            *(const volatile unsigned char *)(intptr_t)(nw_data_window + 0x67fb);
        *(volatile int32_t *)(intptr_t)(nw_data_window + 0x16f4) =
            (int32_t)RewindGlobalSlotFileToStart();

        (void)LoadDecryptRegionsFromDescriptor(
            (intptr_t)NW_WINDOW_PTR_VALUE(0x637c));
        (void)LoadDecryptRegionsFromDescriptor(
            (intptr_t)NW_WINDOW_PTR_VALUE(0x6380));

        /* Only this path discards the second descriptor and clears the mode
         * byte, leaving the next save on the writer's header-first branch. */
        (void)ClearBufferRegionsFromDescriptor(
            (intptr_t)NW_WINDOW_PTR_VALUE(0x6380));
        *(volatile unsigned char *)(intptr_t)(nw_data_window + 0x67f9) = 0;

    } else {

        (void)SeekTrackedHandleAbsoluteOffset(
            *(volatile int32_t *)(intptr_t)(nw_data_window + 0x16f4));
        (void)LoadDecryptRegionsFromDescriptor(
            (intptr_t)NW_WINDOW_PTR_VALUE(0x637c));
        (void)LoadDecryptRegionsFromDescriptor(
            (intptr_t)NW_WINDOW_PTR_VALUE(0x6380));
    }

    CloseGlobalSlotAndResetToUnused();
    return 1;                                       
}
#endif /* NW_UNIT_LoadOrRepairSaveFileRegions */

/* ==========================================================================
 * Unit: LoadRecordBlocksIntoTileCache
 * Source: io/LoadRecordBlocksIntoTileCache.c
 * ========================================================================== */
#ifdef NW_UNIT_LoadRecordBlocksIntoTileCache
/*
 * Load Record Blocks Into Tile Cache.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-loadrecordblocksintotilecache-c
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "nwglobals.h"

typedef struct NwRecordDescriptor {
    uint32_t pReadDest;       /* +0x00 */
    uint32_t dwStreamOffset;  /* +0x04 */
    uint32_t pArena;          /* +0x08 */
    uint32_t dwSlotCount;     /* +0x0c */
} NwRecordDescriptor;

/* Record-state globals, all in nw_data_window. */
#define g_dwLastRecordIndex   (*(uint32_t *)(void *)(nw_data_window + 0x62f4))
#define g_pRecordArrayBase    NW_WINDOW_PTR_VALUE(0x630c)
#define g_dwRecordStride      (*(uint32_t *)(void *)(nw_data_window + 0x61e8))
#define g_dwRecordRowLimit    (*(uint32_t *)(void *)(nw_data_window + 0x61ec))
#define g_pBulkCopyBase       NW_WINDOW_PTR_VALUE(0x6250)
#define g_dwBulkCopyShift     (*(uint32_t *)(void *)(nw_data_window + 0x6254))
#define g_dwSlotRegionBase    (*(uint32_t *)(void *)(nw_data_window + 0x6304))

extern void __stdcall CopyRecordsToSlotChain(int index);
extern BOOL __stdcall SeekStreamLogicalOffset(DWORD dwMoveMethod, LONG lOffset);
extern DWORD __stdcall ReadLzssOrRawTaggedRecordBlock(BYTE *pbDestBuf);
extern void *__cdecl MoveMemoryRegion(void *pDest, const void *pSrc,
                                      uint32_t dwByteCount);
extern uint8_t *__cdecl AcquireNextFreeSlot(void);
extern void __stdcall FreeHeapBlock(uint8_t *pData);
extern void __stdcall FlushPendingSurfaceRectsAndReseedRandom(char bDrawPending);
extern BOOL RebuildGridPaletteAndResetExtent(void);

#define NW_DIAG __attribute__((section(".nwdiag")))

NW_DIAG uint32_t __stdcall LoadRecordBlocksIntoTileCache(uint8_t flagA,
                                                         uint32_t recordIndex)
{
    NwRecordDescriptor *pDesc;
    uint32_t dwDescAddr;
    uint32_t dwOldIndex;
    uint32_t dwRow;
    uint32_t dwSrcOffset;
    uint32_t dwSlot;
    uint32_t dwSlotCount;

    dwOldIndex = g_dwLastRecordIndex;
    if (dwOldIndex != 0xffffffffu && flagA == 0u)
        CopyRecordsToSlotChain((int)dwOldIndex);

    g_dwLastRecordIndex = recordIndex;
    dwDescAddr = g_pRecordArrayBase + (recordIndex << 4);
    pDesc = (NwRecordDescriptor *)(uintptr_t)nw_guest_to_host(dwDescAddr);

    if ((uint8_t)SeekStreamLogicalOffset(
            0u, (LONG)pDesc->dwStreamOffset) == 0u)
        return 0u;

    if ((uint8_t)ReadLzssOrRawTaggedRecordBlock(
            (BYTE *)(uintptr_t)nw_guest_to_host(pDesc->pReadDest)) == 0u)
        return 0u;

    dwSrcOffset = 0u;
    for (dwRow = 0u; (int32_t)dwRow < (int32_t)g_dwRecordRowLimit; ++dwRow) {
        uintptr_t pDst = (uintptr_t)g_pBulkCopyBase +
            (((uint32_t)dwRow << (g_dwBulkCopyShift & 31u)) * 2u);
        uintptr_t pSrc = (uintptr_t)nw_guest_to_host(pDesc->pArena +
            (dwSrcOffset * g_dwRecordStride));

        MoveMemoryRegion((void *)pDst, (const void *)pSrc,
                         g_dwRecordStride * 2u);
        dwSrcOffset += 2u;
    }

    dwSrcOffset = pDesc->pArena + (g_dwSlotRegionBase * 2u);
    dwSlotCount = pDesc->dwSlotCount;
    for (dwSlot = 0u; dwSlot < dwSlotCount; ++dwSlot) {
        uint8_t *pSlot = AcquireNextFreeSlot();

        memcpy(pSlot, (const void *)(uintptr_t)nw_guest_to_host(dwSrcOffset), 16u);
        dwSrcOffset += 16u;
    }

    pDesc->dwSlotCount = 0u;
    FreeHeapBlock((uint8_t *)(uintptr_t)nw_guest_to_host(pDesc->pArena));
    FlushPendingSurfaceRectsAndReseedRandom(0);

    if (flagA == 0u) {
        if ((uint8_t)RebuildGridPaletteAndResetExtent() == 0u)
            return 0u;
    }
    return 1u;
}
#endif /* NW_UNIT_LoadRecordBlocksIntoTileCache */

/* ==========================================================================
 * Unit: WriteBufferRegionsScrambledToStream
 * Source: io/WriteBufferRegionsScrambledToStream.c
 * ========================================================================== */
#ifdef NW_UNIT_WriteBufferRegionsScrambledToStream
/*
 * Walks the caller-supplied buffer-region descriptor backward and writes every region it lists to the current stream descriptor.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-writebufferregionsscrambledtostream-c
 */

#include <stdint.h>

extern void __stdcall ApplyRollingKeyXorToBuffer(unsigned int dwKeyIndex,
                                                 int           nCount,
                                                 unsigned char *pBuf);

extern int __stdcall WriteStreamChunkCheckComplete(unsigned int cbCount,
                                                 char         *lpBuffer);

extern uintptr_t g_base_ptr_a; 

typedef struct {
    uint32_t dwOffsetFromBase; /* +0x00, guest offset from g_base_ptr_a */
    uint32_t dwByteCount;      /* +0x04, bytes to write */
} BufferRegionRecord;

typedef struct {
    uint32_t dwRegionTally;      /* +0x00, number of records that follow */
    /* BufferRegionRecord records[dwRegionTally] follow at +0x04 */
} BufferRegionDescriptor;

_Static_assert(sizeof(BufferRegionRecord) == 8,
               "save region records are two guest dwords");

int __stdcall WriteBufferRegionsScrambledToStream(BufferRegionDescriptor *pDescriptor)
{
    BufferRegionRecord *pRecord;
    unsigned char *pBuf;
    int iRegion;
    int bComplete;

    if (pDescriptor == 0) {
        return 1;
    }

    if (pDescriptor->dwRegionTally == 0u) {
        return 1;
    }

    for (iRegion = (int)pDescriptor->dwRegionTally - 1; iRegion >= 0; --iRegion) {
        pRecord = (BufferRegionRecord *)((unsigned char *)pDescriptor
                                        + 4 + 8 * iRegion);
        pBuf = (unsigned char *)(g_base_ptr_a + pRecord->dwOffsetFromBase);

        /* SCRAMBLE. dwKeyIndex is the region's own pool offset, so each region
         * starts its own rolling-key walk at key[(offset + 1) & 0x1f] and a
         * region is never scrambled twice into the same byte pattern. */
        ApplyRollingKeyXorToBuffer(pRecord->dwOffsetFromBase,
                                   (int)pRecord->dwByteCount,
                                   pBuf);

        /* WRITE. cbCount is pushed last and so lands in the callee's low slot,
         * which WriteStreamChunkCheckComplete reads as its FIRST argument. */
        bComplete = WriteStreamChunkCheckComplete(pRecord->dwByteCount,
                                                  (char *)pBuf);

        if (!bComplete) {

            return 0;
        }

        /* UNSCRAMBLE. The same XOR with the same key stream, which is the
         * inverse of the first call, so the pool ends up holding plaintext. */
        ApplyRollingKeyXorToBuffer(pRecord->dwOffsetFromBase,
                                   (int)pRecord->dwByteCount,
                                   pBuf);
    }

    return 1;
}
/* One caller references this with @0 (no stack args) and another with @4. */
#if !defined(__ELF__)
__asm__(".globl _WriteBufferRegionsScrambledToStream@0\n"
        ".set _WriteBufferRegionsScrambledToStream@0, _WriteBufferRegionsScrambledToStream@4\n");
#endif
#define _ALIAS3 1
#endif /* NW_UNIT_WriteBufferRegionsScrambledToStream */

/* ==========================================================================
 * Unit: WriteSaveFileBuffersAndLoadTileCache
 * Source: io/WriteSaveFileBuffersAndLoadTileCache.c
 * ========================================================================== */
#ifdef NW_UNIT_WriteSaveFileBuffersAndLoadTileCache
/*
 * Write the current save image and then reload the active record's tile cache
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-writesavefilebuffersandloadtilecache-c
 */

#include <stddef.h>
#include <stdint.h>
#include "nwglobals.h"

/* The frame-control record read through g_pFrameCtl.  Only +1 and +4 are
 * consumed here; the layout is the same one ProcessInputAdvanceFrameInterval
 * established from the indexed accesses. */
typedef struct tagFrameCtl {
    uint8_t  bRunning;        /* +0x00 */
    uint8_t  bFrameDivisor;   /* +0x01 */
    uint8_t  bCharPending;    /* +0x02 */
    uint8_t  bCharLatched;    /* +0x03 */
    uint32_t dwFrameCount;    /* +0x04 */
} FRAMECTL, *PFRAMECTL;

#define g_dwRecordField08     (*(uint32_t *)(void *)(nw_data_window + 0x62f0))
#define g_dwLastRecordIndex   (*(uint32_t *)(void *)(nw_data_window + 0x62f4))
#define g_dwAuxRecordIndex    (*(uint32_t *)(void *)(nw_data_window + 0x62f8))
#define g_pStateTable         NW_WINDOW_PTR_REF(uint8_t, 0x6248)
#define g_dwSlotRegionBase    (*(uint32_t *)(void *)(nw_data_window + 0x6304))
#define g_dwRecordCountLimit  (*(uint32_t *)(void *)(nw_data_window + 0x6308))
#define g_dwRecordArrayBase   NW_WINDOW_PTR_VALUE(0x630c)
#define g_dwSlotPointerDelta  (*(uint32_t *)(void *)(nw_data_window + 0x6318))
#define g_pSaveRegionDescThird NW_WINDOW_PTR_REF(NwSaveRegionDescriptor, 0x6384)

typedef struct NwSaveRecord {
    uint32_t pReadDest;       /* +0x00 */
    uint32_t dwStreamOffset;  /* +0x04, not used by this writer */
    uint32_t pArena;          /* +0x08 */
    uint32_t dwSlotCount;     /* +0x0c */
} NwSaveRecord;

_Static_assert(sizeof(FRAMECTL) == 8, "frame-control record is 8 bytes");
_Static_assert(offsetof(FRAMECTL, bFrameDivisor) == 1, "divisor is +1");
_Static_assert(offsetof(FRAMECTL, dwFrameCount) == 4, "frame count is +4");
_Static_assert(sizeof(NwSaveRecord) == 16, "save descriptor is 16 bytes");
_Static_assert(offsetof(NwSaveRecord, pReadDest) == 0, "gate is +0");
_Static_assert(offsetof(NwSaveRecord, pArena) == 8, "arena is +8");
_Static_assert(offsetof(NwSaveRecord, dwSlotCount) == 0x0c, "count is +0xc");

/* Descriptor layout owned by WriteBufferRegionsScrambledToStream.  Only the
 * pointer type is needed here; the writer handles the record contents. */
typedef struct NwSaveRegionDescriptor NwSaveRegionDescriptor;

extern int __stdcall OpenTempPathOrExeVariantSourceFile(int nVariantNumber,
                                                        const char *pszSuffix,
                                                        const char *pszSourcePath);
extern void __stdcall CopyRecordsToSlotChain(int index);
extern void WriteSaveFileHeaderAndBufferRegions(void);
extern int __stdcall WriteBufferRegionsScrambledToStream(
    NwSaveRegionDescriptor *pDescriptor);
extern void __stdcall WriteStreamChunkFromStackArgumentDword(
    uint32_t dwStackArgumentValue);
extern void __stdcall ApplyDeltaToUnmarkedEntries(int nDelta,
                                                  int nEntryCount,
                                                  uint8_t *pEntries);
extern int __stdcall WriteStreamChunkCheckComplete(uint32_t cbCount,
                                                   char *lpBuffer);
extern void __fastcall CloseGlobalSlotAndResetToUnused(uint32_t dwUnusedArg1,
                                                        uint32_t dwUnusedArg2);
extern uint32_t __stdcall LoadRecordBlocksIntoTileCache(uint8_t flagA,
                                                        uint32_t recordIndex);

/* Data-window objects shared with the save/load family. */
extern unsigned char g_abSaveFileHeaderRecord[76]; 
extern char g_szSaveVariantSuffix[4];              
extern char g_szRuntimeSourcePath[];               

#define NW_DIAG __attribute__((section(".nwdiag")))

NW_DIAG int __stdcall WriteSaveFileBuffersAndLoadTileCache(
    uint32_t nVariantNumber)
{
    uint32_t dwRecordOffset;
    uint32_t iRecord;
    FRAMECTL *pFrameCtl;

    if ((uint8_t)OpenTempPathOrExeVariantSourceFile(
            (int)nVariantNumber, g_szSaveVariantSuffix,
            g_szRuntimeSourcePath) == 0u) {
        return 0;
    }

    g_bActiveCellReady = 0u;

    CopyRecordsToSlotChain((int)g_dwLastRecordIndex);

    pFrameCtl = g_pFrameCtl;
    g_abSaveFileHeaderRecord[0x33] = pFrameCtl->bFrameDivisor;
    *(uint32_t *)(void *)(g_abSaveFileHeaderRecord + 0x34) =
        pFrameCtl->dwFrameCount;
    *(uint32_t *)(void *)(g_abSaveFileHeaderRecord + 0x10) =
        g_dwAuxRecordIndex;
    *(uint32_t *)(void *)(g_abSaveFileHeaderRecord + 0x08) =
        g_dwRecordField08;
    *(uint32_t *)(void *)(g_abSaveFileHeaderRecord + 0x0c) =
        g_dwLastRecordIndex;

    WriteSaveFileHeaderAndBufferRegions();
    (void)WriteBufferRegionsScrambledToStream(g_pSaveRegionDescThird);

    dwRecordOffset = 0u;
    for (iRecord = 0u; iRecord < g_dwRecordCountLimit; ++iRecord) {
        NwSaveRecord *pRecord = (NwSaveRecord *)(uintptr_t)
            nw_guest_to_host(g_dwRecordArrayBase + dwRecordOffset);
        uint32_t dwSlotCount;
        uint32_t dwSlotRegionBytes;
        uint8_t *pSlots;

        if (pRecord->pReadDest != 0u) {
            dwSlotCount = pRecord->dwSlotCount;
            dwSlotRegionBytes = g_dwSlotRegionBase * 2u;
            pSlots = (uint8_t *)(uintptr_t)
                nw_guest_to_host(pRecord->pArena + dwSlotRegionBytes);

            WriteStreamChunkFromStackArgumentDword(iRecord);
            WriteStreamChunkFromStackArgumentDword(dwSlotCount);

            ApplyDeltaToUnmarkedEntries((int)(0u - g_dwSlotPointerDelta),
                                        (int32_t)dwSlotCount, pSlots);

            (void)WriteStreamChunkCheckComplete(
                dwSlotRegionBytes + dwSlotCount * 16u,
                (char *)(uintptr_t)nw_guest_to_host(pRecord->pArena));

            ApplyDeltaToUnmarkedEntries((int32_t)g_dwSlotPointerDelta,
                                        (int32_t)dwSlotCount, pSlots);

            while (dwSlotCount != 0u) {
                uint8_t bFlags = pSlots[0];

                if ((bFlags & 2u) != 0u && (bFlags & 8u) != 0u) {
                    uint32_t cbPayload =
                        g_pStateTable[((uint32_t)pSlots[1] << 4) + 3u];
                    uint32_t pPayload =
                        *(uint32_t *)(void *)(pSlots + 0x0c);

                    (void)WriteStreamChunkCheckComplete(
                        cbPayload, (char *)(uintptr_t)nw_guest_to_host(pPayload));
                }

                pSlots += 16u;
                --dwSlotCount;
            }
        }

        dwRecordOffset += 16u;
    }

    CloseGlobalSlotAndResetToUnused(0u, 0u);

    (void)LoadRecordBlocksIntoTileCache(1u, g_dwLastRecordIndex);

    g_bActiveCellReady = 1u;
    return 1;
}
#endif /* NW_UNIT_WriteSaveFileBuffersAndLoadTileCache */

/* ==========================================================================
 * Unit: WriteSaveFileHeaderAndBufferRegions
 * Source: io/WriteSaveFileHeaderAndBufferRegions.c
 * ========================================================================== */
#ifdef NW_UNIT_WriteSaveFileHeaderAndBufferRegions
/*
 * Write Save File Header And Buffer Regions.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-writesavefileheaderandbufferregions-c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_log_mark(const char *stage);
static void nw_save_write_markf(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    platform_log_mark(line);
}
#else
#define nw_save_write_markf(...) ((void)0)
#endif

/* Descriptor layout reproduced from src/io/WriteBufferRegionsScrambledToStream.c and
 * src/io/LoadDecryptRegionsFromDescriptor.c, which agree with each other: an 8-byte
 * stride, the tally header at +0x00, and dwOffsetFromBase FOUR BYTES BELOW
 * dwByteCount inside a record. Declared here, before the callee prototypes that
 * name it, and reproduced so this file cannot drift from the two siblings
 * silently. This function does not index records itself. */
typedef struct {
    uint32_t dwOffsetFromBase; /* +0x00, guest offset from g_base_ptr_a; also
                                *       the rolling-XOR key index (masked 0x1f) */
    uint32_t dwByteCount;      /* +0x04, bytes in this region */
} BufferRegionRecord;

typedef struct {
    uint32_t dwRegionTally;     /* +0x00, number of records that follow */
    /* BufferRegionRecord records[dwRegionTally] follow at +0x04 */
} BufferRegionDescriptor;

_Static_assert(sizeof(BufferRegionRecord) == 8,
               "save region records are two guest dwords");

extern int __stdcall WriteStreamChunkCheckComplete(uint32_t cbCount,
                                                 char    *lpBuffer);

extern int __stdcall WriteBufferRegionsScrambledToStream(BufferRegionDescriptor *pDescriptor);

extern unsigned char g_abSaveFileHeaderMagic[8]; 

extern unsigned char g_abSaveFileHeaderRecord[76]; 

extern BufferRegionDescriptor *g_pSaveRegionDescPrimary;   
extern BufferRegionDescriptor *g_pSaveRegionDescSecondary; 

void WriteSaveFileHeaderAndBufferRegions(void)
{
    int nResult;

    /* Keep the I/O outside the diagnostic macro: on PC the macro discards its
     * arguments, so passing the calls as arguments would compile them away. */
    /* 1. The 8-byte magic, unchanged. The count is pushed LAST so it lands in the
     *    callee's low slot -- (cbCount, lpBuffer), not the reverse. */
    nResult = (int)WriteStreamChunkCheckComplete(8u, (char *)g_abSaveFileHeaderMagic);
    nw_save_write_markf("save_wr_magic_%d", nResult);

    /* 2. The whole 76-byte header record, including anything the caller patched
     *    immediately before calling. Nothing reads the return value, so a short
     *    write here does not stop the region dumps that follow. */
    nResult = (int)WriteStreamChunkCheckComplete(76u, (char *)g_abSaveFileHeaderRecord);
    nw_save_write_markf("save_wr_header_%d", nResult);

    nResult = (int)WriteBufferRegionsScrambledToStream(g_pSaveRegionDescPrimary);
    nw_save_write_markf("save_wr_primary_%d", nResult);
    nResult = (int)WriteBufferRegionsScrambledToStream(g_pSaveRegionDescSecondary);
    nw_save_write_markf("save_wr_secondary_%d", nResult);
}
#endif /* NW_UNIT_WriteSaveFileHeaderAndBufferRegions */

/* ==========================================================================
 * Unit: WriteSaveFileHeaderAndScrambledRegions
 * Source: io/WriteSaveFileHeaderAndScrambledRegions.c
 * ========================================================================== */
#ifdef NW_UNIT_WriteSaveFileHeaderAndScrambledRegions
/*
 * Write Save File Header And Scrambled Regions.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-writesavefileheaderandscrambledregions-c
 */

#include <stdint.h>
extern unsigned char nw_data_window[];

extern int  __stdcall OpenTempPathOrExeVariantSourceFile(int nVariantNumber,
                                                          const char *pszSuffix,
                                                          const char *pszSourcePath);
extern void WriteSaveFileHeaderAndBufferRegions(void);            
extern int  __stdcall SeekTrackedHandleAbsoluteOffset(int32_t lDistanceToMove); 
extern int  __stdcall WriteBufferRegionsScrambledToStream(void *pDescriptor); 
extern void __fastcall CloseGlobalSlotAndResetToUnused(uint32_t dwUnusedArg1,
                                                        uint32_t dwUnusedArg2);  

/* --- globals this function only READS or CLEARS ----------------------------- */
/* (nw_data_window + 0x67f9) : byte, mode flag - zero selects the header-first (fresh) write   */
/* (nw_data_window + 0x633c) : dword, absolute seek offset, cleared to 0 before the header write  */

int __stdcall WriteSaveFileHeaderAndScrambledRegions(int32_t nVariantNumber)
{
    int openRet;

    openRet = OpenTempPathOrExeVariantSourceFile(
        nVariantNumber,
        (const char *)(intptr_t)(nw_data_window + 0x632c),
        (const char *)(intptr_t)(nw_data_window + 0x6390));

    if (openRet == 0) {
        return 0;                                  
    }

    if (*(const volatile unsigned char *)(intptr_t)(nw_data_window + 0x67f9) != 0) {

        *(volatile int32_t *)(intptr_t)(nw_data_window + 0x633c) = 0;
        WriteSaveFileHeaderAndBufferRegions();
    } else {

        (void)SeekTrackedHandleAbsoluteOffset(
            *(const volatile int32_t *)(intptr_t)(nw_data_window + 0x16f4));
        (void)WriteBufferRegionsScrambledToStream(
            (void *)NW_WINDOW_PTR_VALUE(0x637c));
        (void)WriteBufferRegionsScrambledToStream(
            (void *)NW_WINDOW_PTR_VALUE(0x6380));
    }

    CloseGlobalSlotAndResetToUnused(0, 0);

    return 1;                                      
}
#endif /* NW_UNIT_WriteSaveFileHeaderAndScrambledRegions */
