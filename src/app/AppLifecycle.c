#include "nwarch.h"
/*
 * AppLifecycle -- process startup, global tables, subsystem reset, and orderly teardown.
 *
 * Module: app
 * Interface: include/AppLifecycle.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   ClearWritableDataSpanOnce                                  app/ClearWritableDataSpanOnce.c
 *   CrtInitApiThunks                                           app/CrtInitApiThunks.c
 *   DispatchExitHooksAndClearShutdownFlags                     app/DispatchExitHooksAndClearShutdownFlags.c
 *   DispatchPendingCommandLoop                                 app/DispatchPendingCommandLoop.c
 *   DispatchSubsystemInitByOpcode                              app/DispatchSubsystemInitByOpcode.c
 *   DrainHandlerTableAndCallExitHooks                          app/DrainHandlerTableAndCallExitHooks.c
 *   InitializeAppSubsystemsFromSavedPath                       app/InitializeAppSubsystemsFromSavedPath.c
 *   InitializeCrtRuntimeAndCtors                               app/InitializeCrtRuntimeAndCtors.c
 *   InitializeHookAndFlagGlobals                               app/InitializeHookAndFlagGlobals.c
 *   ParseCommandLineIntoArgcArgv                               app/ParseCommandLineIntoArgcArgv.c
 *   ProcessQueuedTaskRecords                                   app/ProcessQueuedTaskRecords.c
 *   ResetSubsystemsAndFreeGlobalSlots                          app/ResetSubsystemsAndFreeGlobalSlots.c
 *   ResetSubsystemsAndLatchStateFlag                           app/ResetSubsystemsAndLatchStateFlag.c
 *   RunInitTablesAndStoreBlockSizeInWords                      app/RunInitTablesAndStoreBlockSizeInWords.c
 */

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern void platform_boot_mark(const char *stage);
#define NW_NATIVE_BOOT(stage) platform_boot_mark(stage)
#else
#define NW_NATIVE_BOOT(stage) ((void)0)
#endif

/* ==========================================================================
 * Unit: ClearWritableDataSpanOnce
 * Source: app/ClearWritableDataSpanOnce.c
 * ========================================================================== */
#ifdef NW_UNIT_ClearWritableDataSpanOnce
/*
 * Clear Writable Data Span Once.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-clearwritabledataspanonce-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];
#include "nwdebug.h"

#define g_dwSpanInitGuard  (*(volatile uint32_t *)(nw_data_window + 0xdc0UL))

#define SPAN_BEGIN ((uintptr_t)(nw_data_window + 0x11b0))
#define SPAN_END   ((uintptr_t)(nw_data_window + 0x6820UL))

void ClearWritableDataSpanOnce(void)
{
    NWDEBUG("enter ClearWritableDataSpanOnce");
    uint8_t  *pCur;
    uintptr_t cbSpan;

    if (g_dwSpanInitGuard != 0u)           
        return;                            /* already initialised */

    pCur   = (uint8_t *)SPAN_BEGIN;        /* EDI = (nw_data_window + 0x11b0) */
    cbSpan = SPAN_END - SPAN_BEGIN;        

    if ((cbSpan & 0xFFFFu) == 0u)
        return;

    {   /* XOR EAX,EAX; CLD; REP STOSB ES:EDI -- byte-granular zero fill */
        uintptr_t i;

        for (i = 0u; i < cbSpan; i++)
            pCur[i] = (uint8_t)0x00;
    }

    g_dwSpanInitGuard = 1u;                
}
#endif /* NW_UNIT_ClearWritableDataSpanOnce */

/* ==========================================================================
 * Unit: CrtInitApiThunks
 * Source: app/CrtInitApiThunks.c
 * ========================================================================== */
#ifdef NW_UNIT_CrtInitApiThunks
/*
 * Crt Init Api Thunks.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-crtinitapithunks-c
 */

#include <stdint.h>

/* kernel32 import thunks in the mingw archive carry @0 decoration; name them
 * explicitly so ld binds to the real DLL entry instead of a local duplicate.
 * ELF has no stdcall decoration, so the Linux compatibility layer supplies
 * these as ordinary C functions. */
#if defined(_WIN64)
extern uint32_t WinGetCurrentThreadId(void) __asm__("GetCurrentThreadId");
extern char   *WinGetEnvironmentStringsA(void) __asm__("GetEnvironmentStringsA");
extern uint32_t WinGetVersion(void) __asm__("GetVersion");
#elif defined(__ELF__)
extern uint32_t WinGetCurrentThreadId(void);
extern char   *WinGetEnvironmentStringsA(void);
extern uint32_t WinGetVersion(void);
#else
extern uint32_t WinGetCurrentThreadId(void) __asm__("_GetCurrentThreadId@0");
extern char   *WinGetEnvironmentStringsA(void) __asm__("_GetEnvironmentStringsA@0");
extern uint32_t WinGetVersion(void) __asm__("_GetVersion@0");
#endif

extern unsigned char nw_data_window[] __asm__("nw_data_window");
#ifdef NWDEBUG_ENABLED
void nwtrace_hex(const char *, unsigned long);
#endif

uint32_t ThunkGetCurrentThreadId(void)
{
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("thunk_tid_enter", 0);
#endif
    uint32_t t = WinGetCurrentThreadId();
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("thunk_tid_ret", t);
#endif
    return t;
}

char *ThunkGetEnvironmentStrings(void)
{
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("thunk_envstrings_enter", 0);
#endif
    char *p = WinGetEnvironmentStringsA();
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("thunk_envstrings_ret", (unsigned long)(uintptr_t)p);
#endif
    return p;
}

uint32_t ThunkGetVersion(void)
{
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("thunk_getversion_enter", 0);
#endif
    uint32_t r = WinGetVersion();
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("thunk_getversion_ret", r);
#endif
    return r;
}
#endif /* NW_UNIT_CrtInitApiThunks */

/* ==========================================================================
 * Unit: DispatchExitHooksAndClearShutdownFlags
 * Source: app/DispatchExitHooksAndClearShutdownFlags.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchExitHooksAndClearShutdownFlags
/*
 * Dispatch Exit Hooks And Clear Shutdown Flags.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-dispatchexithooksandclearshutdownflags-c
 */

#include <stdint.h>

extern unsigned char nw_data_window[] __asm__("nw_data_window");

#define NW_OFF_SHUTDOWN_IN_PROGRESS 0xdbc
#define NW_OFF_SHUTDOWN_DIRECT_FLAG 0xdb8
#define NW_OFF_SHUTDOWN_FINAL_FLAG  0xdb4
#define NW_OFF_EXIT_HOOK_TABLE      0xccc
#define NW_OFF_EXIT_OVERRIDE_HOOK   0x6410
#define NW_OFF_EXIT_HOOK            0x6414

#define NW_DWORD(off) \
    (*(volatile uint32_t *)(void *)(nw_data_window + (off)))

/* The shipped body reloads the hook-table slot and then dereferences it, so
 * keep the volatile pointer load and the dereference as separate accesses. */
#define NW_HOOK_SLOT(off) \
    ((volatile uint32_t *)(uintptr_t)NW_DWORD(off))

typedef void (*NwExitHookFn)(void);
typedef void (*NwExitOverrideHookFn)(uint32_t exit_code);

extern void __stdcall DrainHandlerTableAndCallExitHooks(void);
extern void TerminateWithExitStatusCode(uint32_t exit_code);

uint32_t DispatchExitHooksAndClearShutdownFlags(uint32_t exit_code)
    __asm__("DispatchExitHooksAndClearShutdownFlags");

#define NW_DIAG __attribute__((section(".nwdiag")))

NW_DIAG uint32_t DispatchExitHooksAndClearShutdownFlags(uint32_t exit_code)
{
    NW_DWORD(NW_OFF_SHUTDOWN_IN_PROGRESS) = 1u;

    if (NW_DWORD(NW_OFF_SHUTDOWN_DIRECT_FLAG) == 0u) {
        if (NW_DWORD(NW_OFF_EXIT_HOOK_TABLE) != 0u &&
            *NW_HOOK_SLOT(NW_OFF_EXIT_HOOK_TABLE) != 0u) {
            for (;;) {
                uint32_t entry = *NW_HOOK_SLOT(NW_OFF_EXIT_HOOK_TABLE);

                ((NwExitHookFn)(uintptr_t)entry)();

                NW_DWORD(NW_OFF_EXIT_HOOK_TABLE) =
                    NW_DWORD(NW_OFF_EXIT_HOOK_TABLE) - 4u;

                if (*NW_HOOK_SLOT(NW_OFF_EXIT_HOOK_TABLE) == 0u) {
                    break;
                }
            }
        }
    }

    if (NW_DWORD(NW_OFF_EXIT_OVERRIDE_HOOK) != 0u) {
        ((NwExitOverrideHookFn)(uintptr_t)
             NW_DWORD(NW_OFF_EXIT_OVERRIDE_HOOK))(exit_code);
    } else {
        DrainHandlerTableAndCallExitHooks();

        if (NW_DWORD(NW_OFF_SHUTDOWN_DIRECT_FLAG) == 0u) {
            if (NW_DWORD(NW_OFF_EXIT_HOOK) != 0u) {
                ((NwExitHookFn)(uintptr_t)
                     NW_DWORD(NW_OFF_EXIT_HOOK))();
            }

            if (NW_DWORD(NW_OFF_SHUTDOWN_FINAL_FLAG) == 0u) {
                TerminateWithExitStatusCode(exit_code);
            }
        }
    }

    NW_DWORD(NW_OFF_SHUTDOWN_DIRECT_FLAG) = 0u;
    NW_DWORD(NW_OFF_SHUTDOWN_FINAL_FLAG) = 0u;
    return 0u;
}
#endif /* NW_UNIT_DispatchExitHooksAndClearShutdownFlags */

/* ==========================================================================
 * Unit: DispatchPendingCommandLoop
 * Source: app/DispatchPendingCommandLoop.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchPendingCommandLoop
/*
 * Dispatch Pending Command Loop.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-dispatchpendingcommandloop-c
 */

#include <stdint.h>
#include <stddef.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

extern unsigned char nw_data_window[];

#define NW8(off)  (*(volatile uint8_t  *)(void *)(nw_data_window + (off)))
#define NW32(off) (*(volatile uint32_t *)(void *)(nw_data_window + (off)))

#define G_VM_RECORD_PTR NW_WINDOW_PTR_REF(uint8_t, 0x16d4)
#define G_VM_SAVED_PTR  NW_WINDOW_PTR_REF(uint8_t, 0x16d8)
#define G_VM_RECORD_ADDR ((uint32_t)(uintptr_t)G_VM_RECORD_PTR)
#define G_VM_SAVED_ADDR  ((uint32_t)(uintptr_t)G_VM_SAVED_PTR)

extern void __stdcall ResetStreamCursorState(unsigned char *pStreamSource);
extern void InitializeBlankSpaceFilledField32(void);
extern uint8_t __stdcall WriteSaveFileHeaderAndScrambledRegions(int32_t nVariantNumber);
extern uint8_t __stdcall CheckIndexedStreamEntryAndReadRecordTag(uint32_t dwEntryIndex);
extern void FreeGlobalHeapBlock(void);
extern uint8_t __stdcall InstallActiveGridCellRef(uint8_t bRefreshExtents,
                                                   int32_t nRecordIndex);
extern uint8_t __stdcall LoadOrRepairSaveFileRegions(int32_t nVariantNumber);
extern uint8_t __stdcall LoadRecordBlocksIntoTileCache(uint8_t bFlag,
                                                       uint32_t dwRecordIndex);
extern void __fastcall DecodeTaggedRecordLengthToOutParam(uint32_t dwUnusedArg1,
                                                          uint32_t dwUnusedArg2,
                                                          uint32_t *pdwLengthOut);
extern void __stdcall ReadTaggedRecordDeltaCountAndLatchStreamPosition(
    int32_t *pRecordEndPosition);
extern uint8_t InterpretOpcodeStreamUntilEnd(void);
extern uint8_t __stdcall ExecuteOpcodeAndStoreOperand(uint32_t arg1_operand,
                                                      uint32_t arg2_addend,
                                                      uint32_t arg3_index,
                                                      uint32_t arg4_index,
                                                      uint32_t arg5_slot,
                                                      uint32_t arg6_opcode)
    __asm__("_ExecuteOpcodeAndStoreOperand");
extern uint8_t FetchNextStreamByteIntoSlotDescriptor(void);
extern uint32_t __stdcall ReadLzssOrRawTaggedRecordBlock(uint8_t *pbDestBuf);
extern void ResetGridSpanGeometryIfModeFlagClear(void);
extern uint8_t RebuildGridHeapAndCursorStateFromStream(void);
extern void InitializeGridArenaFromMapHeader(void);
extern uint32_t __stdcall AllocateHeapCursorBlock(uint32_t dwSizeBytes)
    __asm__("_AllocateHeapCursorBlock");
extern void SeedRandomFromClockAndClearWorkQueue(void);
extern void InstallInitialHeapRegionBounds(void);
extern void InitializeGlobalHeapBuffer(void);
extern int32_t GetStreamLogicalBytePosition(void);
extern void ResetPendingSlotHookState(void);
extern void DispatchMciAudioReset(void);
extern uint32_t ClearAllSlotEntries(void);

/*
 * Zero-byte anchors for the window command table at nw_data_window + 0x26c.
 * The table is no longer consulted by this function; these labels only keep
 * the generated window snapshot's entries resolvable and add no code.
 */
__asm__(".text\n"
        ".globl WAnch_00405cd3\n" "WAnch_00405cd3:\n"
        ".globl WAnch_00405d00\n" "WAnch_00405d00:\n"
        ".globl WAnch_00405d12\n" "WAnch_00405d12:\n"
        ".globl WAnch_00405d24\n" "WAnch_00405d24:\n"
        ".globl WAnch_00405d78\n" "WAnch_00405d78:\n");

NW_DIAG uint32_t __stdcall DispatchPendingCommandLoop(uint32_t arg)
{
    /* Incoming ESI: proven behaviourally equivalent to zero (see header). */
    uint32_t esi_state = 0u;

    /* EDI is the stream-entry index; it starts at 1. */
    uint32_t edi = 1u;

    uint32_t local10 = 0u;
    uint32_t local14 = 0u;
    uint32_t local18 = 0u;
    uint32_t local1c = 0u;
    int32_t  local20 = 0;

    uint32_t scratch[19];

    uint32_t entry_index;

    ResetStreamCursorState((unsigned char *)0);

    InitializeBlankSpaceFilledField32();

    if (NW8(0x67f9) == 0u) {
        if (!WriteSaveFileHeaderAndScrambledRegions((int32_t)NW32(0x638c)))
            return 0u;
    }

    NW_NATIVE_BOOT("dpcl_begin");
    if (!CheckIndexedStreamEntryAndReadRecordTag(1u)) {
        NW_NATIVE_BOOT("dpcl_idx1_fail");
        return 0u;
    }
    NW_NATIVE_BOOT("dpcl_idx1_ok");

    NW8(0x16e4) = 1u;
    NW32(0x62f4) = 0xffffffffu;
    NW32(0x16e0) = 0xffffffffu;
    NW32(0x62f8) = 0u;
    NW32(0x62f0) = arg;
    NW_WINDOW_PTR_VALUE(0x6384) = 0u;
    NW_WINDOW_PTR_VALUE(0x6380) = 0u;
    NW_WINDOW_PTR_VALUE(0x637c) = 0u;
    NW32(0x16ec) = 0u;
    NW32(0x16e8) = 0u;
    NW_WINDOW_PTR_REF(uint8_t, 0x16f0) = NULL;

    ResetPendingSlotHookState();
    DispatchMciAudioReset();
    (void)ClearAllSlotEntries();

    entry_index = arg + 2u;

    for (;;) {
        uint32_t tag = NW8(0x16d0);   

        switch (tag) {
        case 0x00u:

            if (edi == 1u) {
                edi = entry_index;                       
                if (!CheckIndexedStreamEntryAndReadRecordTag(edi)) {
                    NW_NATIVE_BOOT("dpcl_idxn_fail");
                    return 0u;
                }
                NW_NATIVE_BOOT("dpcl_idxn_ok");
                continue;
            }

            FreeGlobalHeapBlock();                       
            NW_NATIVE_BOOT("dpcl_free");
            if (NW8(0x67f7) != 0u) {
                NW_NATIVE_BOOT("dpcl_done_noregs");
                return 1u;                               
            }

            if (!InstallActiveGridCellRef(0u, 0)) {
                NW_NATIVE_BOOT("dpcl_install_fail");
                return 0u;
            }
            NW_NATIVE_BOOT("dpcl_install_ok");
            if (!LoadOrRepairSaveFileRegions((int32_t)NW32(0x638c))) {
                NW_NATIVE_BOOT("dpcl_regions_fail");
                return 0u;
            }
            NW_NATIVE_BOOT("dpcl_regions_ok");

            {
                uint32_t record_index = NW32(0x16e0);    
                if (record_index == 0xffffffffu)
                    record_index = 0u;                   
                if (!LoadRecordBlocksIntoTileCache(0u, record_index)) {
                    NW_NATIVE_BOOT("dpcl_cache_fail");
                    return 0u;
                }
            }
            NW_NATIVE_BOOT("dpcl_done");
            return 1u;                                   

        case 0x01u:
            DecodeTaggedRecordLengthToOutParam(0u, 0u, &local14);
            continue;                                    

        case 0x02u:
            DecodeTaggedRecordLengthToOutParam(0u, 0u, &local18);
            continue;                                    

        case 0x03u:
            DecodeTaggedRecordLengthToOutParam(0u, 0u, &local1c);
            continue;                                    

        case 0x05u:

            ReadTaggedRecordDeltaCountAndLatchStreamPosition(&local20);
            if (esi_state == 0x14u) {
                uint32_t *slot_table = (uint32_t *)(uintptr_t)NW32(0x63d0);
                slot_table[local10] = (uint32_t)local20;
            }
            continue;

        case 0x06u:

            {
                static unsigned nwOpcodeStreams;
                nwOpcodeStreams++;
                if (nwOpcodeStreams == 1u)
                    NW_NATIVE_BOOT("dpcl_opcode_begin1");
                else if (nwOpcodeStreams == 2u)
                    NW_NATIVE_BOOT("dpcl_opcode_begin2");
                else if (nwOpcodeStreams == 3u)
                    NW_NATIVE_BOOT("dpcl_opcode_begin3");
            }
            if (!InterpretOpcodeStreamUntilEnd()) {
                NW_NATIVE_BOOT("dpcl_opcode_fail");
                return 0u;
            }
            NW_NATIVE_BOOT("dpcl_opcode_ok");
            continue;

        case 0xffu:

            {
                static int nwFfBegin;
                if (!nwFfBegin) {
                    nwFfBegin = 1;
                    NW_NATIVE_BOOT("dpcl_ff_begin");
                }
            }
            if (!ExecuteOpcodeAndStoreOperand(G_VM_SAVED_ADDR, local1c, local18,
                                              local14, local10, esi_state)) {
                NW_NATIVE_BOOT("dpcl_ff_exec_fail");
                return 0u;
            }
            if (!FetchNextStreamByteIntoSlotDescriptor()) {
                NW_NATIVE_BOOT("dpcl_ff_fetch_fail");
                return 0u;
            }
            continue;

        case 0x04u:

            switch (esi_state) {
            case 0x0fu:

                {
                    uint8_t *dest;
                    if (NW8(0x67f9) != 0u && NW8(0x67f7) == 0u)
                        dest = nw_data_window + 0x632c;
                    else
                        dest = (uint8_t *)scratch;

                    (void)ReadLzssOrRawTaggedRecordBlock(dest);
                    ResetGridSpanGeometryIfModeFlagClear();
                }
                continue;

            case 0x10u:

                NW_NATIVE_BOOT("dpcl_rebuild_begin");
                if (!RebuildGridHeapAndCursorStateFromStream()) {
                    NW_NATIVE_BOOT("dpcl_rebuild_fail");
                    return 0u;
                }
                NW_NATIVE_BOOT("dpcl_rebuild_ok");
                continue;

            case 0x11u:

                if (local10 == arg) {
                    InitializeGridArenaFromMapHeader();
                    continue;
                }
                goto run_opcode;

            case 0x18u:

                if (NW8(0x16e4) != 0u) {
                    NW8(0x16e4) = 0u;
                    if (NW32(0x16ec) != 0u) {
                        NW_WINDOW_PTR_REF(uint8_t, 0x16f0) =
                            (uint8_t *)(uintptr_t)G_VM_RECORD_ADDR;
                        (void)AllocateHeapCursorBlock(NW32(0x16ec));
                    }
                    SeedRandomFromClockAndClearWorkQueue();
                    InstallInitialHeapRegionBounds();
                    InitializeGlobalHeapBuffer();

                    G_VM_RECORD_PTR = NW_WINDOW_PTR_REF(uint8_t, 0x1614);
                }
                NW32(0x16dc) = (uint32_t)GetStreamLogicalBytePosition();
                NW32(0x62f8) = NW32(0x62f8) + 1u;
                goto run_opcode;

            default:

                goto run_opcode;
            }

        default:

            local1c = 0u;
            local18 = 0u;
            local14 = 0u;
            local10 = 0u;
            {
                uint32_t tag_byte = NW8(0x16d0);
                esi_state = tag_byte & 0x7fu;
                if ((tag_byte & 0x80u) != 0u) {
                    (void)FetchNextStreamByteIntoSlotDescriptor();
                    continue;                            
                }
            }
            DecodeTaggedRecordLengthToOutParam(0u, 0u, &local10);
            continue;                                    

        }

run_opcode:

        {
            uint32_t block = G_VM_RECORD_ADDR;
            uint32_t current_block;

            G_VM_SAVED_PTR = (uint8_t *)(uintptr_t)block;
            (void)ReadLzssOrRawTaggedRecordBlock((uint8_t *)(uintptr_t)block);

            current_block = G_VM_RECORD_ADDR;
            if (!ExecuteOpcodeAndStoreOperand(current_block, local1c, local18,
                                              local14, local10, esi_state))
                return 0u;

            if (esi_state == 0x43u) {

                if (NW8(0x16e5) != 0u) {
                    uint32_t ebx = NW32(0x16c8) - 4u;
                    uint32_t ecx = ebx;

                    if (NW32(0x16cc) != 0u) {
                        ebx = NW32(0x16cc);
                        ecx = NW32(0x16c8);
                    }
                    if (ecx > NW32(0x16ec))
                        NW32(0x16ec) = ecx;

                    {
                        uint32_t *record = (uint32_t *)(uintptr_t)G_VM_RECORD_ADDR;
                        record[1] = 0x4469736bu;           /* "Disk" */
                    }
                    {
                        uint32_t length = (uint32_t)GetStreamLogicalBytePosition();
                        length -= ebx;
                        length--;
                        ((uint32_t *)(uintptr_t)G_VM_RECORD_ADDR)[2] = length;
                    }
                    {
                        uint32_t trailing = NW32(0x16cc);
                        ((uint32_t *)(uintptr_t)G_VM_RECORD_ADDR)[3] = trailing;
                    }
                    NW32(0x16c8) = 0x10u;                 
                }
            } else if (esi_state == 0x11u || esi_state == 0x20u ||
                       esi_state == 0x40u || esi_state == 0x50u ||
                       esi_state == 0x18u || esi_state == 0x19u) {

                continue;
            }

            G_VM_RECORD_PTR = (uint8_t *)(uintptr_t)
                AllocateHeapCursorBlock(NW32(0x16c8));
            continue;
        }
    }
}
#endif /* NW_UNIT_DispatchPendingCommandLoop */

/* ==========================================================================
 * Unit: DispatchSubsystemInitByOpcode
 * Source: app/DispatchSubsystemInitByOpcode.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchSubsystemInitByOpcode
/*
 * Dispatch Subsystem Init By Opcode.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-dispatchsubsysteminitbyopcode-c
 */

#include <stdint.h>

extern unsigned char nw_data_window[];

#define NW_DIAG __attribute__((section(".nwdiag")))

/* stdcall: size -> pointer to payload (NULL on exhaustion). */
extern uint32_t *__stdcall AllocHeapBlock(uint32_t dwSizeBytes);

/* Common failure tail: reset the subsystem state and return zero. */
extern void ResetSubsystemsAndLatchStateFlag(void);

enum {
    NW_SUBSYS_MODE          = 0x16f9,
    NW_SUBSYS_ENABLE_A      = 0x16fa,
    NW_SUBSYS_ENABLE_B      = 0x16fb,
    NW_SUBSYS_SOURCE_A      = 0x16fc,
    NW_SUBSYS_SOURCE_B      = 0x1700,
    NW_SUBSYS_VALUE_MODE1   = 0x1704,
    NW_SUBSYS_VALUE_MODE1_B = 0x1708,
    NW_SUBSYS_VALUE_MODE1_C = 0x170c,
    NW_SUBSYS_VALUE_MODE2   = 0x1710,
    NW_SUBSYS_BLOCK_MODE2   = 0x1714,
    NW_SUBSYS_VALUE_MODE3A  = 0x1718,
    NW_SUBSYS_BLOCK_MODE3A  = 0x171c,
    NW_SUBSYS_VALUE_MODE3B  = 0x1720,
    NW_SUBSYS_BLOCK_MODE3B  = 0x1724,
    NW_SUBSYS_VALUE_MODE4A  = 0x1728,
    NW_SUBSYS_BLOCK_MODE4A  = 0x172c,
    NW_SUBSYS_VALUE_MODE4B  = 0x1730,
    NW_SUBSYS_BLOCK_MODE4B  = 0x1734,
    NW_SUBSYS_VALUE_MODE5   = 0x173c,
    NW_SUBSYS_VALUE_MODE6A  = 0x1744,
    NW_SUBSYS_VALUE_MODE6B  = 0x1748
};

static inline uint8_t nw_byte(uint32_t off)
{
    return nw_data_window[off];
}

static inline uint32_t nw_dword(uint32_t off)
{
    uint32_t value;
    __builtin_memcpy(&value, nw_data_window + off, sizeof value);
    return value;
}

static inline void nw_store_dword(uint32_t off, uint32_t value)
{
    __builtin_memcpy(nw_data_window + off, &value, sizeof value);
}

static NW_DIAG uint32_t nw_subsystem_failed(void)
{
    ResetSubsystemsAndLatchStateFlag();
    return 0u;
}

/* Allocate and publish a block, treating NULL as the common failure. */
static NW_DIAG uint32_t nw_subsystem_block(uint32_t value_off, uint32_t block_off,
                                   uint32_t size)
{
    uint32_t *block;

    nw_store_dword(value_off, size);
    block = AllocHeapBlock(size);
    NW_WINDOW_PTR_REF(uint8_t, block_off) = (uint8_t *)block;
    return block != (uint32_t *)0;
}

NW_DIAG uint32_t DispatchSubsystemInitByOpcode(void)
{
    uint8_t mode = nw_byte(NW_SUBSYS_MODE);

    switch (mode) {
    case 0:
        if (nw_dword(NW_SUBSYS_SOURCE_A) != 0x563d2d5bu ||
            nw_dword(NW_SUBSYS_SOURCE_B) != 0x5d2d3d53u)
            return nw_subsystem_failed();
        return 3u;

    case 1:
        nw_store_dword(NW_SUBSYS_VALUE_MODE1, 1u);
        nw_store_dword(NW_SUBSYS_VALUE_MODE1_B, 0u);
        if (nw_byte(NW_SUBSYS_ENABLE_A))
            nw_store_dword(NW_SUBSYS_VALUE_MODE1, nw_dword(NW_SUBSYS_SOURCE_A));
        if (nw_byte(NW_SUBSYS_ENABLE_B))
            nw_store_dword(NW_SUBSYS_VALUE_MODE1_C, nw_dword(NW_SUBSYS_SOURCE_B));
        return 1u;

    case 2:
        if (!nw_byte(NW_SUBSYS_ENABLE_A))
            return 1u;
        if (!nw_subsystem_block(NW_SUBSYS_VALUE_MODE2, NW_SUBSYS_BLOCK_MODE2,
                                nw_dword(NW_SUBSYS_SOURCE_A)))
            return nw_subsystem_failed();
        return 1u;

    case 3:
        if (nw_byte(NW_SUBSYS_ENABLE_A)) {
            if (!nw_subsystem_block(NW_SUBSYS_VALUE_MODE3A,
                                    NW_SUBSYS_BLOCK_MODE3A,
                                    nw_dword(NW_SUBSYS_SOURCE_A)))
                return nw_subsystem_failed();
        }
        if (!nw_byte(NW_SUBSYS_ENABLE_B))
            return 1u;
        if (!nw_subsystem_block(NW_SUBSYS_VALUE_MODE3B,
                                NW_SUBSYS_BLOCK_MODE3B,
                                nw_dword(NW_SUBSYS_SOURCE_B)))
            return nw_subsystem_failed();
        return 1u;

    case 4:
        if (nw_byte(NW_SUBSYS_ENABLE_A)) {
            if (!nw_subsystem_block(NW_SUBSYS_VALUE_MODE4A,
                                    NW_SUBSYS_BLOCK_MODE4A,
                                    nw_dword(NW_SUBSYS_SOURCE_A)))
                return nw_subsystem_failed();
        }
        if (!nw_byte(NW_SUBSYS_ENABLE_B))
            return 1u;
        if (!nw_subsystem_block(NW_SUBSYS_VALUE_MODE4B,
                                NW_SUBSYS_BLOCK_MODE4B,
                                nw_dword(NW_SUBSYS_SOURCE_B)))
            return nw_subsystem_failed();
        return 1u;

    case 5:
        if (nw_byte(NW_SUBSYS_ENABLE_A))
            nw_store_dword(NW_SUBSYS_VALUE_MODE5, nw_dword(NW_SUBSYS_SOURCE_A));
        if (nw_byte(NW_SUBSYS_ENABLE_B))
            nw_store_dword(NW_SUBSYS_VALUE_MODE5, nw_dword(NW_SUBSYS_SOURCE_B));
        return 1u;

    case 6:
        if (nw_byte(NW_SUBSYS_ENABLE_A))
            nw_store_dword(NW_SUBSYS_VALUE_MODE6A, nw_dword(NW_SUBSYS_SOURCE_A));
        if (nw_byte(NW_SUBSYS_ENABLE_B))
            nw_store_dword(NW_SUBSYS_VALUE_MODE6B, nw_dword(NW_SUBSYS_SOURCE_B));
        return 1u;

    default:
        return 1u;
    }
}
#endif /* NW_UNIT_DispatchSubsystemInitByOpcode */

/* ==========================================================================
 * Unit: DrainHandlerTableAndCallExitHooks
 * Source: app/DrainHandlerTableAndCallExitHooks.c
 * ========================================================================== */
#ifdef NW_UNIT_DrainHandlerTableAndCallExitHooks
/*
 * DrainHandlerTableAndCallExitHooks
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-drainhandlertableandcallexithooks-c
 */

#include <stdint.h>
#include <windows.h>
extern unsigned char nw_data_window[];

#define HANDLER_TABLE_BASE  ((uintptr_t)(nw_data_window + 0x11a8))
#define HANDLER_TABLE_END   ((uintptr_t)(nw_data_window + 0x11b0))

/* Program-owned, runtime-patched status value pushed for the tail call. */
#define EXIT_STATUS_SLOT    ((uintptr_t)(nw_data_window + 0xd0d))

extern void DisableHookCallback(void);

typedef void (*handler_fn)(void);

void __stdcall DrainHandlerTableAndCallExitHooks(void)
{
    uint8_t *slot = (uint8_t *)HANDLER_TABLE_BASE;

    while ((uintptr_t)slot < HANDLER_TABLE_END) {
        uintptr_t off = (uintptr_t)(slot - nw_data_window);
        /* The window bytes keep the original four-byte guest addresses.
         * NwWindowApplyFixups() replaces them with the low half of the host
         * function pointer, which is not callable on a 64-bit host; the
         * native-pointer side table holds the full address. */
        handler_fn h = (handler_fn)(uintptr_t)NW_WINDOW_PTR_VALUE(off);

        slot += 4;
        if (h == 0)
            continue;

        *(uint32_t *)(void *)(slot - 4) = 0u;
        NW_WINDOW_PTR_VALUE(off) = 0u;

        h();
    }

    DisableHookCallback();

    FreeEnvironmentStringsA(
        (LPSTR)(uintptr_t)*(volatile uint32_t *)EXIT_STATUS_SLOT);

                               
}                              
#endif /* NW_UNIT_DrainHandlerTableAndCallExitHooks */

/* ==========================================================================
 * Unit: InitializeAppSubsystemsFromSavedPath
 * Source: app/InitializeAppSubsystemsFromSavedPath.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeAppSubsystemsFromSavedPath
/*
 * Initialize App Subsystems From Saved Path.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-initializeappsubsystemsfromsavedpath-c
 */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NW_DIAG __attribute__((section(".nwdiag")))
#define NW32(off) (*(volatile uint32_t *)(nw_data_window + (off)))
#define NW8(off)  (*(volatile uint8_t  *)(nw_data_window + (off)))

extern unsigned char nw_data_window[] __asm__("nw_data_window");
extern char *__cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc);
extern int __stdcall LoadAndVerifySaveFileHeaderChunk(const char *pszPath);
#ifdef NW_PLATFORM_SDL
extern void NwAssetSetOverlayPath(const char *pszPath);
extern void NwAssetPreloadHighRes(void);
extern void NwAssetPreloadAllHighRes(void);
extern void NwAssetWarmStreamCache(void);
extern void NwMidiSink_SetSoundfontPath(const char *path);
#endif
extern uint32_t InitGlobalRngAndResetStateFlags(void);
extern BOOL TryPlayStartupMovieWithWindowFallback(void);
extern BOOL __stdcall SetFrameIntervalFromDivisor(BYTE bDivisor);
extern int NativeProbeReturnsTrue(void);
extern BOOL WINAPI InitializeAudioSubsystemByOsVersionAndOpenMciDevice(
    BYTE *pbOutSlot5, BYTE *pbOutSlot4, DWORD dwUnusedArg3, BYTE bSlotCount);
extern int DispatchLockedProbeEntryIfArmed(void);
extern void NoOpStackCleanupStub(void) __asm__("_FunNoOpRet4_00402010");

#if defined(NW_PLATFORM_SDL)
#include <stddef.h>
#include <string.h>

/* The playable SDL builds may keep the VM stream outside the executable.  This
 * keeps the byte-frozen Win32 oracle untouched (its source path still points at
 * the appended stream) while letting the shipped binary use an edited overlay:
 *
 *   overlay.nw      0x12800-byte dbase padding + overlay + original trailer
 *   overlay.bin     raw overlay bytes (preferred fallback)
 *   overlay.trailer 38-byte original trailer for overlay.bin
 *
 * overlay.nw is used in place.  For a raw overlay.bin we materialize a temporary
 * stream because the recovered loader requires the trailer at the file end.
 * The dbase padding keeps dbase=0x12800, matching the offsets embedded in
 * inline "Disk" image records.
 */
/* Original NETWALK.EXE trailer.  Used only when overlay.bin is present without
 * a sidecar trailer, so a raw extracted overlay still loads in an external-only
 * build.  Edited overlays that change record offsets should ship overlay.nw or
 * an updated overlay.trailer instead. */
static const unsigned char kNwDefaultOverlayTrailer[38] = {
    0xb1, 0x10, 0x0e, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x4b, 0x8c, 0x3f, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x28, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x3d, 0x56, 0x53, 0x3d
};

static int NwExternalTrailerValid(const unsigned char *pTrailer)
{
    return pTrailer[33] == 0x04u &&
           pTrailer[34] == 0x3du && pTrailer[35] == 0x56u &&
           pTrailer[36] == 0x53u && pTrailer[37] == 0x3du;
}

static void NwExternalJoinModuleFile(const char *pszModule, const char *pszName,
                                     char *pszOut, size_t cbOut)
{
    size_t cbDir = 0;
    size_t cbName;
    const char *pEnd = pszModule;

    if (pszOut == NULL || cbOut == 0 || pszModule == NULL || pszName == NULL) {
        return;
    }
    pszOut[0] = '\0';
    while (*pEnd != '\0') {
        pEnd++;
    }
    while (pEnd > pszModule && pEnd[-1] != '\\' && pEnd[-1] != '/') {
        pEnd--;
    }
    cbDir = (size_t)(pEnd - pszModule);
    cbName = strlen(pszName);
    if (cbDir + cbName + 2u > cbOut) {
        return;
    }
    memcpy(pszOut, pszModule, cbDir);
    if (cbDir != 0 && pszOut[cbDir - 1] != '\\' && pszOut[cbDir - 1] != '/') {
        pszOut[cbDir++] = '\\';
    }
    memcpy(pszOut + cbDir, pszName, cbName + 1u);
}

static int NwExternalFileExists(const char *pszPath)
{
    DWORD dwAttr = GetFileAttributesA(pszPath);

    return dwAttr != INVALID_FILE_ATTRIBUTES &&
           (dwAttr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int NwExternalReadTrailer(const char *pszPath, unsigned char *pTrailer)
{
    HANDLE hFile;
    DWORD dwRead = 0;
    int fOk;

    hFile = CreateFileA(pszPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }
    fOk = ReadFile(hFile, pTrailer, 38u, &dwRead, NULL) && dwRead == 38u &&
          NwExternalTrailerValid(pTrailer);
    CloseHandle(hFile);
    return fOk;
}

static int NwExternalReadModuleTrailer(const char *pszModule,
                                       unsigned char *pTrailer)
{
    HANDLE hFile;
    DWORD dwSize;
    DWORD dwRead = 0;
    int fOk = 0;

    hFile = CreateFileA(pszModule, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }
    dwSize = GetFileSize(hFile, NULL);
    if (dwSize != INVALID_FILE_SIZE && dwSize >= 38u &&
        SetFilePointer(hFile, (LONG)(dwSize - 38u), NULL, FILE_BEGIN) !=
            INVALID_SET_FILE_POINTER) {
        fOk = ReadFile(hFile, pTrailer, 38u, &dwRead, NULL) && dwRead == 38u &&
              NwExternalTrailerValid(pTrailer);
    }
    CloseHandle(hFile);
    return fOk;
}

static int NwExternalMaterialize(const char *pszRaw, const unsigned char *pTrailer,
                                 char *pszOut, size_t cbOut)
{
    char szTempDir[MAX_PATH];
    char szTempName[MAX_PATH];
    char szBuf[65536];
    HANDLE hIn;
    HANDLE hOut;
    DWORD dwGot = 0;
    DWORD dwWrote = 0;
    int fOk = 1;

    if (GetTempPathA((DWORD)sizeof szTempDir, szTempDir) == 0 ||
        GetTempFileNameA(szTempDir, "nw", 0u, szTempName) == 0) {
        return 0;
    }
    hIn = CreateFileA(pszRaw, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hIn == INVALID_HANDLE_VALUE) {
        DeleteFileA(szTempName);
        return 0;
    }
    hOut = CreateFileA(szTempName, GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE) {
        CloseHandle(hIn);
        DeleteFileA(szTempName);
        return 0;
    }
    /* Keep the original dbase so the "Disk" image-offset rebasing is unchanged. */
    memset(szBuf, 0, sizeof szBuf);
    {
        DWORD dwLeft = 0x12800u;
        while (dwLeft != 0) {
            DWORD dwChunk = dwLeft > (DWORD)sizeof szBuf
                                ? (DWORD)sizeof szBuf : dwLeft;
            if (!WriteFile(hOut, szBuf, dwChunk, &dwWrote, NULL) ||
                dwWrote != dwChunk) {
                fOk = 0;
                break;
            }
            dwLeft -= dwChunk;
        }
    }
    while (fOk) {
        if (!ReadFile(hIn, szBuf, (DWORD)sizeof szBuf, &dwGot, NULL)) {
            fOk = 0;
            break;
        }
        if (dwGot == 0) {
            break;
        }
        if (!WriteFile(hOut, szBuf, dwGot, &dwWrote, NULL) ||
            dwWrote != dwGot) {
            fOk = 0;
            break;
        }
    }
    if (fOk && (!WriteFile(hOut, pTrailer, 38u, &dwWrote, NULL) ||
                dwWrote != 38u)) {
        fOk = 0;
    }
    CloseHandle(hOut);
    CloseHandle(hIn);
    if (!fOk) {
        DeleteFileA(szTempName);
        return 0;
    }
    if (strlen(szTempName) + 1u > cbOut) {
        DeleteFileA(szTempName);
        return 0;
    }
    strcpy(pszOut, szTempName);
    return 1;
}

static const char *NwExternalSelectOverlay(const char *pszModule,
                                           char *pszOut, size_t cbOut)
{
    char szOverlayNw[MAX_PATH];
    char szOverlayBin[MAX_PATH];
    char szTrailer[MAX_PATH];
    unsigned char abyTrailer[38];

    NwExternalJoinModuleFile(pszModule, "overlay.nw", szOverlayNw,
                             sizeof szOverlayNw);
    if (szOverlayNw[0] != '\0' && NwExternalFileExists(szOverlayNw)) {
        if (strlen(szOverlayNw) + 1u <= cbOut) {
            strcpy(pszOut, szOverlayNw);
            return pszOut;
        }
        return pszModule;
    }

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
    /* Payload loaders do not always expose extra files beside the ELF.  The
     * payload can always read /data, and the native title can read its
     * writable /download0, so accept an overlay placed there as well. */
    {
        static const char *const candidates[] = {
            "/data/overlay.nw",
            "/data/netwalk/overlay.nw",
            "/tmp/overlay.nw",
            "/download0/overlay.nw",
            "overlay.nw"
        };
        size_t i;

        for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
            if (NwExternalFileExists(candidates[i]) &&
                strlen(candidates[i]) + 1u <= cbOut) {
                strcpy(pszOut, candidates[i]);
                return pszOut;
            }
        }
    }
#endif

    NwExternalJoinModuleFile(pszModule, "overlay.bin", szOverlayBin,
                             sizeof szOverlayBin);
    if (szOverlayBin[0] == '\0' || !NwExternalFileExists(szOverlayBin)) {
        return pszModule;
    }

    NwExternalJoinModuleFile(pszModule, "overlay.trailer", szTrailer,
                             sizeof szTrailer);
    if (szTrailer[0] == '\0' ||
        !NwExternalReadTrailer(szTrailer, abyTrailer)) {
        if (!NwExternalReadModuleTrailer(pszModule, abyTrailer)) {
            memcpy(abyTrailer, kNwDefaultOverlayTrailer, sizeof abyTrailer);
        }
    }
    /* The temporary stream starts with the original 0x12800 dbase padding. */
    abyTrailer[19] = 0x00u;
    abyTrailer[20] = 0x28u;
    abyTrailer[21] = 0x01u;
    abyTrailer[22] = 0x00u;

    if (NwExternalMaterialize(szOverlayBin, abyTrailer, pszOut, cbOut)) {
        return pszOut;
    }
    return pszModule;
}
#endif /* NW_PLATFORM_SDL */

NW_DIAG uint32_t __stdcall
InitializeAppSubsystemsFromSavedPath(const char *pszPath)
    __asm__("_InitializeAppSubsystemsFromSavedPath");

NW_DIAG uint32_t __stdcall
InitializeAppSubsystemsFromSavedPath(const char *pszPath)
{
    uint32_t dwIntervalSource;
#if defined(NW_PLATFORM_SDL)
    const char *pszDataPath;
    char szExternalOverlay[MAX_PATH];
    char szSoundfontPath[MAX_PATH] = { 0 };
#else
    char *pszSavedPath;
#endif

    NW_NATIVE_BOOT("app_begin");
    /* Added pre-bind prologue; see the file comment. */
    NW_WINDOW_PTR_REF(void, 0x6230u) = (void *)&NoOpStackCleanupStub;

#if defined(NW_PLATFORM_SDL)
    /* Keep the module path in 0x6390: later save-file variant builders expect
     * an .EXE path.  Only the initial data-stream open uses the external file.
     * The recovered slot is 0x40 bytes; truncate rather than overwrite the
     * adjacent grid-state fields when the portable install path is longer. */
    (void)strncpy((char *)(nw_data_window + 0x6390u), pszPath, 0x3fu);
    ((char *)(nw_data_window + 0x6390u))[0x3fu] = '\0';
    pszDataPath = NwExternalSelectOverlay(pszPath, szExternalOverlay,
                                          sizeof szExternalOverlay);
    NwAssetSetOverlayPath(pszDataPath);
    /* gm.sf2 is deployed beside overlay.nw.  Resolve it from that directory
     * instead of relying on the native title's cwd or SDL_GetBasePath. */
    NwExternalJoinModuleFile(pszDataPath, "gm.sf2", szSoundfontPath,
                             sizeof szSoundfontPath);
    NwMidiSink_SetSoundfontPath(szSoundfontPath);
    /* Read the VM stream and the 4x sprites before the first level draws
     * them.  This turns several cold-storage reads on the first gameplay
     * frame into one sequential startup read. */
    NwAssetWarmStreamCache();
    /* The full preload includes the 4x background.  Keep the single-image
     * preload as a fallback for the case where the pool allocation fails. */
    NwAssetPreloadAllHighRes();
    NwAssetPreloadHighRes();
    NW_NATIVE_BOOT("app_overlay");
#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
    {
        char szBootPath[320];
        snprintf(szBootPath, sizeof szBootPath, "app_overlay_path=%s",
                 pszDataPath != NULL ? pszDataPath : "(null)");
        platform_boot_mark(szBootPath);
    }
#endif
    if ((uint8_t)LoadAndVerifySaveFileHeaderChunk(pszDataPath) == 0u) {
        NW_NATIVE_BOOT("app_save_fail");
        return 0u;
    }
#else
    /* The legacy save-path slot is 0x40 bytes.  Keep the bounded copy for
     * callers that read it, but load from the full module path. */
    (void)strncpy((char *)(nw_data_window + 0x6390u), pszPath, 0x3fu);
    ((char *)(nw_data_window + 0x6390u))[0x3fu] = '\0';
    pszSavedPath = (char *)pszPath;
    if ((uint8_t)LoadAndVerifySaveFileHeaderChunk(pszSavedPath) == 0u) {
        NW_NATIVE_BOOT("app_save_fail");
        return 0u;
    }
#endif
    NW_NATIVE_BOOT("app_save_ok");

    if ((uint8_t)InitGlobalRngAndResetStateFlags() == 0u) {
        NW_NATIVE_BOOT("app_rng_fail");
        return 0u;
    }
    NW_NATIVE_BOOT("app_rng_ok");

    if ((uint8_t)TryPlayStartupMovieWithWindowFallback() == 0u) {
        NW_NATIVE_BOOT("app_movie_fail");
        return 0u;
    }
    NW_NATIVE_BOOT("app_movie_ok");

    dwIntervalSource = (uint32_t)NW_WINDOW_PTR_VALUE(0x6378u);
    if ((uint8_t)SetFrameIntervalFromDivisor(
            *(const uint8_t *)((uintptr_t)dwIntervalSource + 1u)) == 0u) {
        NW_NATIVE_BOOT("app_interval_fail");
        return 0u;
    }
    NW_NATIVE_BOOT("app_interval_ok");

    if ((uint8_t)NativeProbeReturnsTrue() == 0u) {
        NW_NATIVE_BOOT("app_probe_fail");
        return 0u;
    }
    NW_NATIVE_BOOT("app_probe_ok");

    if ((uint8_t)InitializeAudioSubsystemByOsVersionAndOpenMciDevice(
            (BYTE *)(nw_data_window + 0x6807u),
            (BYTE *)(nw_data_window + 0x6806u),
            0u,
            NW8(0x635cu)) == 0u) {
        NW_NATIVE_BOOT("app_audio_fail");
        return 0u;
    }
    NW_NATIVE_BOOT("app_audio_ok");

    NW8(0x67fau) = NW8(0x6806u);
    NW8(0x67fbu) = NW8(0x6807u);

    if ((uint8_t)DispatchLockedProbeEntryIfArmed() == 0u) {
        NW_NATIVE_BOOT("app_dispatch_fail");
        return 0u;
    }

    NW_NATIVE_BOOT("app_ready");
    return 1u;
}
#endif /* NW_UNIT_InitializeAppSubsystemsFromSavedPath */

/* ==========================================================================
 * Unit: InitializeCrtRuntimeAndCtors
 * Source: app/InitializeCrtRuntimeAndCtors.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeCrtRuntimeAndCtors
/*
 * Initialize Crt Runtime And Ctors.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-initializecrtruntimeandctors-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))
#define NW16(off) (*(volatile uint16_t *)(nw_data_window + (off)))
#define NW32(off) (*(volatile uint32_t *)(nw_data_window + (off)))

extern unsigned char nw_data_window[] __asm__("nw_data_window");
extern uint32_t ThunkGetCurrentThreadId(void);
extern char *ThunkGetEnvironmentStrings(void);
extern uint32_t ThunkGetVersion(void);
extern uint32_t CallHookIfInstalled(void);
extern void CallRegisteredGlobalHookWithTrue(void);
extern uint32_t RunInitTablesAndStoreBlockSizeInWords(void);
extern void WriteStdoutStringAndExit(char *szMessage);

NW_DIAG uint32_t InitializeCrtRuntimeAndCtors(void)
{
    uint32_t dwVersion;
    uint32_t dwLow;
    uint16_t wHookResult;

    NW32(0x0d3bu) = ThunkGetCurrentThreadId();
    *((char **)(void *)NW_WINDOW_PTR_SLOTS(0x0d0du)) = ThunkGetEnvironmentStrings();

    dwVersion = ThunkGetVersion();
    /* XCHG AL,AH swaps the two bytes of the low word. */
    dwLow = ((dwVersion & 0x000000ffu) << 8) |
            ((dwVersion & 0x0000ff00u) >> 8) |
            (dwVersion & 0xffff0000u);
    NW16(0x0cdeu) = (uint16_t)dwLow;
    NW16(0x0cdau) = (uint16_t)(dwVersion >> 16);

    /* INC AX / JNE: fatal only when the 16-bit hook result is 0xffff. */
    wHookResult = (uint16_t)CallHookIfInstalled();
    ++wHookResult;
    if (wHookResult == 0u) {
        WriteStdoutStringAndExit((char *)(nw_data_window + 0x0cf7u));
    }

    CallRegisteredGlobalHookWithTrue();
    return RunInitTablesAndStoreBlockSizeInWords();
}
#endif /* NW_UNIT_InitializeCrtRuntimeAndCtors */

/* ==========================================================================
 * Unit: InitializeHookAndFlagGlobals
 * Source: app/InitializeHookAndFlagGlobals.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeHookAndFlagGlobals
/*
 * Startup hook/flag initializer
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-initializehookandflagglobals-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define g_bHookFlag343 \
    (*(uint8_t *)(void *)(nw_data_window + 0x6343))
#define g_bHookFlag341 \
    (*(uint8_t *)(void *)(nw_data_window + 0x6341))
#define g_bHookFlag340 \
    (*(uint8_t *)(void *)(nw_data_window + 0x6340))
#define g_bHookFlag342 \
    (*(uint8_t *)(void *)(nw_data_window + 0x6342))
#define g_bRecordLoadActive \
    (*(uint8_t *)(void *)(nw_data_window + 0x67f9))
#define g_bConfigSlotArg \
    (*(uint8_t *)(void *)(nw_data_window + 0x6806))
#define g_bConfigSlotArg2 \
    (*(uint8_t *)(void *)(nw_data_window + 0x6807))
#define g_bSlotStateArg \
    (*(uint8_t *)(void *)(nw_data_window + 0x635d))
#define g_bSlotStateArg2 \
    (*(uint8_t *)(void *)(nw_data_window + 0x635e))
#define g_dwSaveVariantArg \
    (*(uint32_t *)(void *)(nw_data_window + 0x638c))

extern uint32_t ClearAllSlotEntries(void);
extern uint32_t InvokeSlotHookThunk(void);
extern uint32_t __stdcall ProcessSlotHookThunkArg(uint32_t dwArg);
extern uint32_t __stdcall ForwardStackArgToSlotHook(uint32_t dwArg);
extern int __stdcall WriteSaveFileHeaderAndScrambledRegions(int32_t nVariantNumber);
extern uint32_t ResetHookTableToDefaults(void);
extern void ClearGridActiveFlag(void);
extern void ForwardCloseSaveFileSlot(void);

NW_DIAG void InitializeHookAndFlagGlobals(void)
{
    (void)ClearAllSlotEntries();
    (void)InvokeSlotHookThunk();

    g_bHookFlag343 = 1;
    g_bHookFlag341 = 1;
    g_bHookFlag340 = 1;
    g_bHookFlag342 = 1;
    g_bRecordLoadActive = 1;

    g_bSlotStateArg = g_bConfigSlotArg;
    (void)ProcessSlotHookThunkArg((uint32_t)g_bConfigSlotArg);

    g_bSlotStateArg2 = g_bConfigSlotArg2;
    (void)ForwardStackArgToSlotHook((uint32_t)g_bConfigSlotArg2);

    (void)WriteSaveFileHeaderAndScrambledRegions(
        (int32_t)g_dwSaveVariantArg);
    (void)ResetHookTableToDefaults();
    ClearGridActiveFlag();
    ForwardCloseSaveFileSlot();
}
#endif /* NW_UNIT_InitializeHookAndFlagGlobals */

/* ==========================================================================
 * Unit: ParseCommandLineIntoArgcArgv
 * Source: app/ParseCommandLineIntoArgcArgv.c
 * ========================================================================== */
#ifdef NW_UNIT_ParseCommandLineIntoArgcArgv
/*
 * Parse Command Line Into Argc Argv.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-parsecommandlineintoargcargv-c
 */

#include <stdint.h>
#include "nwdebug.h"

extern unsigned char nw_data_window[] __asm__("nw_data_window");

extern char *SkipQuotedTokenThenControlCharsToToken(char *pszCursor);
extern char *BuildQuotedCommandImage(char *pszTail);
extern void  WriteStdoutStringAndExit(char *szMessage);
extern int   GetStringLength(const char *pszStr);

void *AllocatePooledBlock(uint32_t byte_count);

void ParseCommandLineIntoArgcArgv(void)
{
    char *pCursor   = NW_WINDOW_PTR_REF(char, 0xE7C);
    char *pImage;
    char *pRead;
    char *pWrite;
    char **pArgv;
    int   nArgc     = 0;
    int   bInQuotes;
    int   nBackslashes;
    int   i;

    NWDEBUG("ParseCommandLineIntoArgcArgv");

    /* Stage 1 -- strip the program name from the published command line. */
    pCursor = SkipQuotedTokenThenControlCharsToToken(pCursor);
    NW_WINDOW_PTR_REF(char, 0xE7C) = pCursor;

    /* Stage 2 -- quoted command image. */
    pImage = BuildQuotedCommandImage(pCursor);
    if (pImage == NULL)
        WriteStdoutStringAndExit((char *)(nw_data_window + 0xE9C));

    /* Stage 3 -- destructive tokenizer, in place. */
    pRead  = pImage;
    pWrite = pImage;

    for (;;) {
        char c = *pRead;

        if (c == '\0')
            break;
        if ((c == ' ') || (c == '\t')) {
            pRead++;
            continue;
        }

        nArgc++;
        bInQuotes    = 0;
        nBackslashes = 0;

        for (;;) {
            int bOdd;

            c = *pRead++;

            if (c == '"') {

                bOdd    = nBackslashes & 1;
                pWrite -= nBackslashes / 2;
                if (bOdd)
                    pWrite[-1] = '"';
                else
                    bInQuotes ^= 1;
                nBackslashes = 0;
            } else if ((c == ' ') || (c == '\t')) {
                if (bInQuotes) {

                    nBackslashes = 0;
                    *pWrite++ = c;
                } else {

                    *pWrite++ = '\0';
                    break;
                }
            } else if (c == '\0') {

                *pWrite++ = '\0';
                goto publish;
            } else {

                nBackslashes = (c == '\\') ? nBackslashes + 1 : 0;
                *pWrite++ = c;
            }
        }
    }

publish:
    /* Stage 4 -- argc, argv and argv[0]. */
    *(uint32_t *)(nw_data_window + 0xD11) = (uint32_t)nArgc;

    pArgv = (char **)AllocatePooledBlock((uint32_t)(nArgc * 4 + 4));
    *(char ***)(void *)NW_WINDOW_PTR_SLOTS(0xD15) = pArgv;
    if (pArgv == NULL)
        WriteStdoutStringAndExit((char *)(nw_data_window + 0xE9C));

    pCursor = pImage;
    for (i = 0; i < nArgc; i++) {
        pArgv[i] = pCursor;
        pCursor += GetStringLength(pCursor) + 1;
    }
    pArgv[i] = NULL;

    NW_WINDOW_PTR_REF(char, 0xCD6) = pArgv[0];
}
#endif /* NW_UNIT_ParseCommandLineIntoArgcArgv */

/* ==========================================================================
 * Unit: ProcessQueuedTaskRecords
 * Source: app/ProcessQueuedTaskRecords.c
 * ========================================================================== */
#ifdef NW_UNIT_ProcessQueuedTaskRecords
/*
 * Process Queued Task Records.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-processqueuedtaskrecords-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

extern unsigned char nw_data_window[] __asm__("nw_data_window");

enum {
    NW_HANDLER_TABLE      = 0x00f4,
    NW_DEFERRED_RECORD    = 0x1204,
    NW_CURRENT_CONTEXT    = 0x6218,
    NW_GRID_USER_DATA     = 0x621c,
    NW_CURRENT_STATE      = 0x6214,
    NW_HANDLER            = 0x6230,
    NW_STACK_TOP          = 0x6240,
    NW_STACK_BOTTOM       = 0x6244,
    NW_STATE_TABLE        = 0x6248,
    NW_MASK_TABLE         = 0x6388,
    NW_VM_BASE_B          = 0x63e8,
    NW_DISPATCH_INDEX     = 0x63ec,
    NW_CELL_MODE          = 0x67f6,
    NW_OPCODE_FLAG        = 0x67ff,
    NW_REACHED_CELL       = 0x6804
};

/* Explicit-width window accessors.  __builtin_memcpy expands to the same
 * mov/movzx/movb forms at -O0; no unaligned pointer dereference is spelled. */
static NW_DIAG uint8_t nw_load8(const void *p)
{
    uint8_t value;
    __builtin_memcpy(&value, p, sizeof value);
    return value;
}

static NW_DIAG uint16_t nw_load16(const void *p)
{
    uint16_t value;
    __builtin_memcpy(&value, p, sizeof value);
    return value;
}

static NW_DIAG uint32_t nw_load32(const void *p)
{
    uint32_t value;
    __builtin_memcpy(&value, p, sizeof value);
    return value;
}

static NW_DIAG void nw_store8(void *p, uint8_t value)
{
    __builtin_memcpy(p, &value, sizeof value);
}

static NW_DIAG void nw_store16(void *p, uint16_t value)
{
    __builtin_memcpy(p, &value, sizeof value);
}

static NW_DIAG void nw_store32(void *p, uint32_t value)
{
    __builtin_memcpy(p, &value, sizeof value);
}

/* Pointer-sized data-window slots keep their native value in the x86-64
 * side table.  The recovered task walker still does 32-bit arithmetic on the
 * low guest address, but every load/store of a pointer field must go through
 * these helpers. */
static NW_DIAG void *nw_load_ptr(uint32_t off)
{
    return (void *)(uintptr_t)NW_WINDOW_PTR_VALUE(off);
}

static NW_DIAG void nw_store_ptr(uint32_t off, const void *value)
{
    NW_WINDOW_PTR_REF(void, off) = (void *)(uintptr_t)value;
}

static NW_DIAG uint32_t nw_load_ptr32(uint32_t off)
{
    return (uint32_t)(uintptr_t)NW_WINDOW_PTR_VALUE(off);
}

extern uint8_t __stdcall AdvanceGridCellSubSpan(uint8_t bReleaseSlot,
                                                              uint8_t bFinalPass,
                                                              void *pRecord)
    __asm__("_AdvanceGridCellSubSpan");

extern void __stdcall ComputeScaledGridOffset(void *pRecord)
    __asm__("_ComputeScaledGridOffset");

extern void __stdcall AddGridCellSpanToExtentList(void *pRecord);

extern void __stdcall SelectNextActiveSlot(void *pRecord);

extern int32_t __stdcall ExecuteOpcodeStream(uint32_t dwMode,
                                                           uint32_t dwStream)
    __asm__("_ExecuteOpcodeStream");

NW_DIAG uint8_t ProcessQueuedTaskRecords(void)
{
    uint8_t  *record;
    uint8_t  *child;
    uint32_t  chain_top;
    uint32_t  span_current;
    uint32_t  span_next;
    uint32_t  child_address;
    uint32_t  child_list;
    uint32_t  state_record_address;
    uint32_t  bit_index;
    uint32_t  bias;
    uint32_t  dispatch_index;
    uint32_t  mode;
    int32_t   status;
    int32_t   depth;

    record = (uint8_t *)nw_load_ptr(NW_DEFERRED_RECORD);
    if (record == (uint8_t *)0) {
        record = (uint8_t *)nw_load_ptr(NW_STACK_TOP);
    }
    if ((uint32_t)(uintptr_t)record <= nw_load_ptr32(NW_STACK_BOTTOM)) {
        goto epilogue;
    }

record_ready:
    /* Low two bits must mark the record as queued. */
    if ((nw_load8(record) & 0x03u) != 0x03u) {
        goto low_bits_path;
    }

    /* Optional mask table: bit rec[1]&7 in byte rec[1]>>3 must be set. */
    if (nw_load32(nw_data_window + NW_MASK_TABLE) != 0u) {
        const uint8_t *mask_table =
            (const uint8_t *)(uintptr_t)nw_load32(nw_data_window + NW_MASK_TABLE);
        uint32_t index = nw_load8(record + 1);
        uint32_t mask  = 1u << (index & 7u);

        if ((nw_load8(mask_table + (index >> 3)) & mask) == 0u) {
            goto next_record;
        }
    }

    /* Pending state bit: clear it and move on. */
    if ((nw_load8(record + 3) & 0x01u) != 0u) {
        nw_store8(record + 3, (uint8_t)(nw_load8(record + 3) & 0xfeu));
        goto next_record;
    }

    if ((nw_load8(record) & 0x04u) != 0u) {
        /* Arm A: advance using the record's previous span and final pass. */
        span_current = nw_load16(record + 0x0a);
        span_next    = nw_load16(record + 0x08);
        chain_top    = nw_load_ptr32(NW_STACK_TOP);
        child_address = chain_top - (span_current << 4);

        if (AdvanceGridCellSubSpan((uint8_t)(span_current != span_next), 1u,
                                   (void *)(uintptr_t)child_address) == 0u) {
            if ((uint32_t)(uintptr_t)record ==
                nw_load_ptr32(NW_DEFERRED_RECORD)) {
                goto epilogue;
            }
            goto next_record;
        }

        nw_store16(record + 0x0a, nw_load16(record + 0x08));
        span_next = nw_load16(record + 0x08);
        if (span_next != 0xffffu) {
            chain_top = nw_load_ptr32(NW_STACK_TOP);
            child = (uint8_t *)(uintptr_t)(chain_top - (span_next << 4));
            nw_store8(child, (uint8_t)(nw_load8(child) | 0x80u));
            nw_store8(child + 4, nw_load8(record + 6));
            nw_store8(child + 5, nw_load8(record + 7));
            ComputeScaledGridOffset((void *)child);
            AddGridCellSpanToExtentList((void *)child);
        }

        /* Arm A consumes the span flag even when there is no child. */
        nw_store8(record, (uint8_t)(nw_load8(record) & 0xfbu));
        goto after_span;
    }

    /* Arm B: same child setup, but advance with (release=1, final=0) and
     * leave the record's bit2 set. */
    span_next = nw_load16(record + 0x0a);
    if (span_next != 0xffffu) {
        chain_top = nw_load_ptr32(NW_STACK_TOP);
        child_address = chain_top - (span_next << 4);

        if (AdvanceGridCellSubSpan(1u, 0u,
                                   (void *)(uintptr_t)child_address) != 0u) {
            nw_store16(record + 0x0a, nw_load16(record + 0x08));
            span_next = nw_load16(record + 0x08);
            if (span_next != 0xffffu) {
                chain_top = nw_load_ptr32(NW_STACK_TOP);
                child = (uint8_t *)(uintptr_t)(chain_top - (span_next << 4));
                nw_store8(child, (uint8_t)(nw_load8(child) | 0x80u));
                nw_store8(child + 4, nw_load8(record + 6));
                nw_store8(child + 5, nw_load8(record + 7));
                ComputeScaledGridOffset((void *)child);
                AddGridCellSpanToExtentList((void *)child);
            }
        }
    }
    goto after_span;

low_bits_path:
    /* Not a queued record: only retire it if no mask table is active and it
     * is a released span with no anchor. */
    if (nw_load32(nw_data_window + NW_MASK_TABLE) != 0u) {
        goto next_record;
    }
    if ((nw_load8(record) & 0x83u) != 0x81u) {
        goto next_record;
    }
    if (nw_load16(record + 6) != 0xffffu) {
        goto next_record;
    }
    (void)AdvanceGridCellSubSpan(1u, 0u, (void *)record);
    goto next_record;

after_span:
    nw_store_ptr(NW_CURRENT_CONTEXT, record);

    chain_top = nw_load_ptr32(NW_STACK_TOP);
    depth = (int32_t)(chain_top - (uint32_t)(uintptr_t)record);
    nw_store32(nw_data_window + NW_GRID_USER_DATA, (uint32_t)(depth / 16));

    state_record_address = ((uint32_t)nw_load8(record + 1) << 4) +
                           nw_load_ptr32(NW_STATE_TABLE);
    nw_store_ptr(NW_CURRENT_STATE, (void *)(uintptr_t)state_record_address);

    if ((nw_load8(record) & 0x08u) != 0u) {
        nw_store_ptr(NW_VM_BASE_B, (void *)(uintptr_t)nw_load32(record + 0x0c));
    } else {
        nw_store_ptr(NW_VM_BASE_B, record + 0x0c);
    }

    child_list = nw_load32((const void *)((const unsigned char *)(uintptr_t)state_record_address + 8));
    nw_store8(nw_data_window + NW_REACHED_CELL, 0u);
    if (child_list == 0u) {
        goto next_record;
    }

child_outer:
    /* The mode is re-read from the current context on every child record. */
    mode = nw_load8((const void *)(uintptr_t)
                    nw_load_ptr32(NW_CURRENT_CONTEXT));
    mode &= 0xf0u;
    nw_store8(nw_data_window + NW_CELL_MODE, (uint8_t)mode);

    if (mode == 0x80u) {
        bias = 3u;
    } else if (mode == 0x40u) {
        bias = 2u;
    } else if (mode == 0x20u) {
        bias = 1u;
    } else {
        bias = 0u;
    }

    bit_index = 0u;

child_bit:
    /* State record byte 1 selects which child bits are live. */
    state_record_address = nw_load_ptr32(NW_CURRENT_STATE);
    if ((nw_load8((const void *)((const unsigned char *)(uintptr_t)state_record_address + 1)) &
         (1u << bit_index)) == 0u) {
        goto next_bit;
    }

    dispatch_index = ((bit_index & 3u) + bias) & 3u;
    if (bit_index > 3u) {
        dispatch_index += 4u;
    }
    nw_store32(nw_data_window + NW_DISPATCH_INDEX, dispatch_index);
    /* The handler table holds relocated host function pointers.  On a PIE
     * host their low 32 bits are not callable, so take the full pointer from
     * the native-pointer side table. */
    nw_store_ptr(NW_HANDLER,
               nw_load_ptr(NW_HANDLER_TABLE + (dispatch_index << 2)));
    nw_store8(nw_data_window + NW_OPCODE_FLAG, 0u);

    status = ExecuteOpcodeStream(0u,
                                nw_load32((const void *)(uintptr_t)child_list));
    if (status == 0) {
        goto next_bit;
    }
    if (status == 1) {
        if (nw_load8(nw_data_window + NW_OPCODE_FLAG) != 0u) {
            bit_index = 0u;
            goto child_list_check;
        }
        goto deferred_check;
    }

    /* Any other status aborts the record. */
    if (nw_load_ptr32(NW_DEFERRED_RECORD) != 0u) {
        goto clear_deferred;
    }
    SelectNextActiveSlot((void *)record);
    goto next_record;

next_bit:
    bit_index++;
    if (bit_index < 8u) {
        goto child_bit;
    }

child_list_check:
    child = (uint8_t *)(uintptr_t)nw_load32((const void *)(uintptr_t)child_list);
    if ((nw_load8(child) & 0x80u) == 0u) {
        child_list += 4u;
        goto child_outer;
    }
    if (bit_index != 0u) {
        goto deferred_check;
    }

    /* The first child stopped mid-cell: remember this record for next frame. */
    nw_store_ptr(NW_DEFERRED_RECORD, record);
    goto epilogue;

deferred_check:
    if (nw_load_ptr32(NW_DEFERRED_RECORD) == 0u) {
        goto next_record;
    }

clear_deferred:
    nw_store_ptr(NW_DEFERRED_RECORD, NULL);
    goto epilogue;

next_record:
    record -= 16;
    if ((uint32_t)(uintptr_t)record <= nw_load_ptr32(NW_STACK_BOTTOM)) {
        goto epilogue;
    }
    goto record_ready;

epilogue:
    nw_store_ptr(NW_CURRENT_CONTEXT, NULL);
    nw_store32(nw_data_window + NW_GRID_USER_DATA, 0xffffu);
    return 1u;
}
#endif /* NW_UNIT_ProcessQueuedTaskRecords */

/* ==========================================================================
 * Unit: ResetSubsystemsAndFreeGlobalSlots
 * Source: app/ResetSubsystemsAndFreeGlobalSlots.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetSubsystemsAndFreeGlobalSlots
/*
 * Reset Subsystems And Free Global Slots.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-resetsubsystemsandfreeglobalslots-c
 */

#include <stddef.h>
#include <stdint.h>

extern unsigned char nw_data_window[];

typedef unsigned char  u8;
typedef unsigned char *u8p;

/*
 * The five slot fields are addresses in the recovered 32-bit guest address
 * space, not native host pointers.  Keep them 32 bits wide on every host so
 * the record offsets stay exactly as the original window defines them.  The
 * pointers currently come from the low-address host heap; a future guest
 * arena will make the conversion explicit on all architectures.
 */
typedef struct GlobalSlotRecord {
    u8      pad00[0x14];   /* +0x00 .. +0x13 */
    uint32_t pSlot0;       /* +0x14 */
    u8      pad18[4];      /* +0x18  (gap -- NOT a slot) */
    uint32_t pSlot1;       /* +0x1C */
    u8      pad20[4];      /* +0x20  (gap -- NOT a slot) */
    uint32_t pSlot2;       /* +0x24 */
    u8      pad28[4];      /* +0x28  (gap -- NOT a slot) */
    uint32_t pSlot3;       /* +0x2C */
    u8      pad30[4];      /* +0x30  (gap -- NOT a slot) */
    uint32_t pSlot4;       /* +0x34 */
    u8      fClearAllSlots;/* +0x38 */
} GlobalSlotRecord;

/* Compile-time pin: fail the build rather than silently shift the record. */
typedef char nw_chk_slot0 [offsetof(GlobalSlotRecord, pSlot0)         == 0x14 ? 1 : -1];
typedef char nw_chk_slot1 [offsetof(GlobalSlotRecord, pSlot1)         == 0x1C ? 1 : -1];
typedef char nw_chk_slot2 [offsetof(GlobalSlotRecord, pSlot2)         == 0x24 ? 1 : -1];
typedef char nw_chk_slot3 [offsetof(GlobalSlotRecord, pSlot3)         == 0x2C ? 1 : -1];
typedef char nw_chk_slot4 [offsetof(GlobalSlotRecord, pSlot4)         == 0x34 ? 1 : -1];
typedef char nw_chk_flag  [offsetof(GlobalSlotRecord, fClearAllSlots) == 0x38 ? 1 : -1];

static GlobalSlotRecord *const g_pRecord = (GlobalSlotRecord *)(nw_data_window + 0x1700);

void ClearAllSlotEntries(void);             
void ResetPendingSlotHookState(void);       
void DispatchMciAudioReset(void);           
void FreeHeapBlockIfNotNull(u8p pData);     
void RebuildGridPaletteAndResetExtent(void);

int ResetSubsystemsAndFreeGlobalSlots(void)
{
    if (g_pRecord->fClearAllSlots != 0u) {
        ClearAllSlotEntries();
    }

    ResetPendingSlotHookState();
    DispatchMciAudioReset();

    FreeHeapBlockIfNotNull((u8p)(uintptr_t)g_pRecord->pSlot0);
    FreeHeapBlockIfNotNull((u8p)(uintptr_t)g_pRecord->pSlot1);
    FreeHeapBlockIfNotNull((u8p)(uintptr_t)g_pRecord->pSlot2);
    FreeHeapBlockIfNotNull((u8p)(uintptr_t)g_pRecord->pSlot3);
    FreeHeapBlockIfNotNull((u8p)(uintptr_t)g_pRecord->pSlot4);

    RebuildGridPaletteAndResetExtent();

    return 1;  /* machine writes AL only; upper EAX bytes are callee residue */
}
#endif /* NW_UNIT_ResetSubsystemsAndFreeGlobalSlots */

/* ==========================================================================
 * Unit: ResetSubsystemsAndLatchStateFlag
 * Source: app/ResetSubsystemsAndLatchStateFlag.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetSubsystemsAndLatchStateFlag
/*
 * Reset Subsystems And Latch State Flag.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-resetsubsystemsandlatchstateflag-c
 */

#include <stdint.h>
extern unsigned char nw_data_window[];

extern uint8_t ResetSubsystemsAndFreeGlobalSlots(void);

extern uint32_t __stdcall SetStateFlagOnce(uint32_t state_flag);

#define NW_STATE_FLAG_PTR ((uint32_t)(nw_data_window + 0x2c4))

void ResetSubsystemsAndLatchStateFlag(void)
{

    (void)ResetSubsystemsAndFreeGlobalSlots();
    SetStateFlagOnce(NW_STATE_FLAG_PTR);

}


/* Keeps the original .rdata extent. This block is data, never executed. */
const unsigned char ResetSubsystemsAndLatchStateFlag_bytes[16] = {
    0xe8, 0xa3, 0xff, 0xff, 0xff,
    0x68, 0xc4, 0x12, 0x41, 0x00,
    0xe8, 0x99, 0x5e, 0x00, 0x00,
    0xc3
};
#endif /* NW_UNIT_ResetSubsystemsAndLatchStateFlag */

/* ==========================================================================
 * Unit: RunInitTablesAndStoreBlockSizeInWords
 * Source: app/RunInitTablesAndStoreBlockSizeInWords.c
 * ========================================================================== */
#ifdef NW_UNIT_RunInitTablesAndStoreBlockSizeInWords
/*
 * Run Init Tables And Store Block Size In Words.
 *
 * Module: app
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-runinittablesandstoreblocksizeinwords-c
 */

#include <stdint.h>

__asm__(".globl RunInitTablesAndStoreBlockSizeInWords\n" "RunInitTablesAndStoreBlockSizeInWords:\n");

extern void FindTableValueByKeyA(void);                      
extern void InitializeStandardHandlesInTrackedTable(void);   
extern unsigned char nw_data_window[] __asm__("nw_data_window");

#ifdef NWDEBUG_ENABLED
void nwtrace_hex(const char *, unsigned long);
#endif

uint32_t RunInitTablesAndStoreBlockSizeInWords(void)
{

#ifdef NWDEBUG_ENABLED
    nwtrace_hex("inittables_site1_enter", 1);
#endif
    FindTableValueByKeyA();

    InitializeStandardHandlesInTrackedTable();
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("inittables_after_site1", 2);
#endif

    /* sites 2..4 hold no callable entries in this image; nothing to run. */

    *(uint16_t *)(nw_data_window + 0xCD0) = 65;

#ifdef NWDEBUG_ENABLED
    nwtrace_hex("inittables_done", *(uint16_t *)(nw_data_window + 0xCD0));
#endif
    return 65;   
}

/* C definition already exports the underscored symbol for i386; no asm alias needed. */
#endif /* NW_UNIT_RunInitTablesAndStoreBlockSizeInWords */
