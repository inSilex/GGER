#include "nwarch.h"
/*
 * Hooks -- installed hooks, probe dispatch, and exception handoff.
 *
 * Module: runtime
 * Interface: include/Hooks.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   CallGmwDispatch206                                         runtime/CallGmwDispatch206.c
 *   CallHookIfInstalled                                        runtime/CallHookIfInstalled.c
 *   CallRegisteredGlobalHookIfInstalled                        runtime/CallRegisteredGlobalHookIfInstalled.c
 *   CallRegisteredGlobalHookWithTrue                           runtime/CallRegisteredGlobalHookWithTrue.c
 *   DisableHookCallback                                        runtime/DisableHookCallback.c
 *   DispatchExceptionOnGlobalScopeRecord                       runtime/DispatchExceptionOnGlobalScopeRecord.c
 *   DispatchIndexedProbeEntry                                  runtime/DispatchIndexedProbeEntry.c
 *   DispatchLockedProbeEntryIfArmed                            runtime/DispatchLockedProbeEntryIfArmed.c
 *   ForwardStackArgToSlotHook                                  runtime/ForwardStackArgToSlotHook.c
 *   HandleSlotHookWithArg                                      runtime/HandleSlotHookWithArg.c
 *   InvokeSlotHookThunk                                        runtime/InvokeSlotHookThunk.c
 *   NativeProbeReturnsTrue                                     runtime/NativeProbeReturnsTrue.c
 *   NoOpStackCleanupStub                                       runtime/NoOpStackCleanupStub.c
 *   ProcessSlotHookThunkArg                                    runtime/ProcessSlotHookThunkArg.c
 *   RetryLockedProbeDispatch                                   runtime/RetryLockedProbeDispatch.c
 */

/* ==========================================================================
 * Unit: CallGmwDispatch206
 * Source: runtime/CallGmwDispatch206.c
 * ========================================================================== */
#ifdef NW_UNIT_CallGmwDispatch206
/*
 * Call Gmw Dispatch206.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-callgmwdispatch206-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef uint32_t (*GmwDispatch206Fn)(void);

uint32_t CallGmwDispatch206(void)
{
    GmwDispatch206Fn fn = (GmwDispatch206Fn)(uintptr_t)
        *(const uint32_t *)(const void *)(nw_data_window + 0x63c);
    return fn();
}
#endif /* NW_UNIT_CallGmwDispatch206 */

/* ==========================================================================
 * Unit: CallHookIfInstalled
 * Source: runtime/CallHookIfInstalled.c
 * ========================================================================== */
#ifdef NW_UNIT_CallHookIfInstalled
/*
 * Call Hook If Installed.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-callhookifinstalled-c
 */

#include <stdint.h>

extern unsigned char nw_data_window[] __asm__("nw_data_window");

#define NW_DIAG __attribute__((section(".nwdiag")))

typedef uint32_t (*NwGlobalInitHook)(void);

NW_DIAG uint32_t CallHookIfInstalled(void)
{
    NwGlobalInitHook hook =
        NW_WINDOW_PTR_REF(void, 0x119c);

    if (hook == 0)
        return 0;
    return hook();
}
#endif /* NW_UNIT_CallHookIfInstalled */

/* ==========================================================================
 * Unit: CallRegisteredGlobalHookIfInstalled
 * Source: runtime/CallRegisteredGlobalHookIfInstalled.c
 * ========================================================================== */
#ifdef NW_UNIT_CallRegisteredGlobalHookIfInstalled
/*
 * Call Registered Global Hook If Installed.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-callregisteredglobalhookifinstalled-c
 */

#include <stdint.h>
#include "nwdebug.h"
extern unsigned char nw_data_window[];

static void (**volatile g_ppRegisteredHook)(void) =
    (void (**)(void))(intptr_t)(nw_data_window + 0x67ec);

/* Invokes the hook registered at (nw_data_window + 0x67ec) with no arguments if one is installed;
 * otherwise does nothing. __cdecl, no parameters, no return value of its own. */
void CallRegisteredGlobalHookIfInstalled(void)
{
    uint32_t raw = *(const uint32_t *)(const void *)(nw_data_window + 0x67ec);
    void (*hook)(void) = (void (*)(void))(uintptr_t)raw;

    NWDEBUG("enter CallRegisteredGlobalHookIfInstalled");
#ifdef NWDEBUG_ENABLED
    extern void nwtrace_hex(const char *, unsigned long);
    nwtrace_hex("crh g_pp_addr", (unsigned long)(uintptr_t)g_ppRegisteredHook);
    nwtrace_hex("crh slot_ptr_val", raw);
#endif
    if (hook != 0) {
        NWDEBUG("about to call hook");
        hook();
    }
}
#endif /* NW_UNIT_CallRegisteredGlobalHookIfInstalled */

/* ==========================================================================
 * Unit: CallRegisteredGlobalHookWithTrue
 * Source: runtime/CallRegisteredGlobalHookWithTrue.c
 * ========================================================================== */
#ifdef NW_UNIT_CallRegisteredGlobalHookWithTrue
/*
 * Call Registered Global Hook With True.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-callregisteredglobalhookwithtrue-c
 */

typedef void (*PFN_GLOBALHOOK)(int);
extern PFN_GLOBALHOOK g_pfnGlobalHook;

#ifdef NWDEBUG_ENABLED
void nwtrace_hex(const char *, unsigned long);
#endif

void CallRegisteredGlobalHookWithTrue(void)
{
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("hook67f0_enter", (unsigned long)(void *)g_pfnGlobalHook);
#endif
    if (g_pfnGlobalHook != 0)
    {
        g_pfnGlobalHook(1);   /* return value in EAX is intentionally discarded */
#ifdef NWDEBUG_ENABLED
        nwtrace_hex("hook67f0_after_call", 2);
#endif
    }

}
#endif /* NW_UNIT_CallRegisteredGlobalHookWithTrue */

/* ==========================================================================
 * Unit: DisableHookCallback
 * Source: runtime/DisableHookCallback.c
 * ========================================================================== */
#ifdef NW_UNIT_DisableHookCallback
/*
 * Disable Hook Callback.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-disablehookcallback-c
 */

#include <stdint.h>
extern unsigned char nw_data_window[];

/*
 * Calls the optional hook at (nw_data_window + 0x67f0) with 0 ("off") if one is installed; otherwise does
 * nothing. No arguments, __cdecl, no return value of its own.
 */
void DisableHookCallback(void)
{
    nw_host_ptr_t raw = NW_WINDOW_PTR_VALUE(0x67f0);
    if (raw != 0u) {
        ((void (*)(int))(uintptr_t)raw)(0);
    }
}
#endif /* NW_UNIT_DisableHookCallback */

/* ==========================================================================
 * Unit: DispatchExceptionOnGlobalScopeRecord
 * Source: runtime/DispatchExceptionOnGlobalScopeRecord.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchExceptionOnGlobalScopeRecord
/*
 * Dispatch Exception On Global Scope Record.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-dispatchexceptiononglobalscoperecord-c
 */

#include <setjmp.h>

extern jmp_buf g_nwGlobalScopeJump;
extern int     g_nwGlobalScopeJumpActive;

#define NW_DIAG __attribute__((section(".nwdiag")))

NW_DIAG void DispatchExceptionOnGlobalScopeRecord(void)
{
    if (g_nwGlobalScopeJumpActive) {
        g_nwGlobalScopeJumpActive = 0;
        longjmp(g_nwGlobalScopeJump, 1);
    }
}
#endif /* NW_UNIT_DispatchExceptionOnGlobalScopeRecord */

/* ==========================================================================
 * Unit: DispatchIndexedProbeEntry
 * Source: runtime/DispatchIndexedProbeEntry.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchIndexedProbeEntry
/*
 * Dispatch Indexed Probe Entry.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-dispatchindexedprobeentry-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define g_bProbeDispatchEnabled \
    (*(uint8_t *)(void *)(nw_data_window + 0x6343))
#define g_bProbeDispatchPending \
    (*(uint8_t *)(void *)(nw_data_window + 0x6801))
/* 0x63d0 holds the low 32-bit address of a table of 32-bit stream offsets;
 * the producer stores each entry as a dword (see AppLifecycle.c case 0x05). */
#define g_apProbeDispatchTable \
    ((const uint32_t *)(uintptr_t)nw_guest_to_host( \
     *(const uint32_t *)(const void *)(nw_data_window + 0x63d0)))

extern uint8_t __stdcall RetryLockedProbeDispatch(uintptr_t entry)
    __asm__("_RetryLockedProbeDispatch");

int __stdcall DispatchIndexedProbeEntry(int nIndex)
    __asm__("_DispatchIndexedProbeEntry");

int __stdcall DispatchIndexedProbeEntry(int nIndex)
{
    uint8_t result;

    if (g_bProbeDispatchEnabled == 0)
        return 1;

    g_bProbeDispatchPending++;
    result = RetryLockedProbeDispatch(
        (uintptr_t)g_apProbeDispatchTable[(uint32_t)nIndex]);
    g_bProbeDispatchPending--;

    return (int)(int8_t)result;
}
#endif /* NW_UNIT_DispatchIndexedProbeEntry */

/* ==========================================================================
 * Unit: DispatchLockedProbeEntryIfArmed
 * Source: runtime/DispatchLockedProbeEntryIfArmed.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchLockedProbeEntryIfArmed
/*
 * Dispatch Locked Probe Entry If Armed.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-dispatchlockedprobeentryifarmed-c
 */

#include <stdint.h>
extern unsigned char nw_data_window[];

/* (nw_data_window + 0x6274): must be NON-ZERO for the dispatch path (DWORD global). */
#define g_probeTableDword (*(volatile uint32_t *)(uintptr_t)(nw_data_window + 0x6274))

/* (nw_data_window + 0x627f): must be EXACTLY 1 for the dispatch path (BYTE global). */
#define g_probeModeByte   (*(volatile uint8_t  *)(uintptr_t)(nw_data_window + 0x627f))

extern int __stdcall DispatchIndexedProbeEntry(int nIndex) __asm__("_DispatchIndexedProbeEntry");

int DispatchLockedProbeEntryIfArmed(void)
{
    if (g_probeTableDword != 0u && g_probeModeByte == 1u) {
        return DispatchIndexedProbeEntry(0);  /* __stdcall: callee pops the 0 */
    }
    return 1;
}
#endif /* NW_UNIT_DispatchLockedProbeEntryIfArmed */

/* ==========================================================================
 * Unit: ForwardStackArgToSlotHook
 * Source: runtime/ForwardStackArgToSlotHook.c
 * ========================================================================== */
#ifdef NW_UNIT_ForwardStackArgToSlotHook
/*
 * Forwards one stack argument to a slot hook.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-forwardstackargtoslothook-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef uint32_t (__stdcall *SlotHookFn)(uint32_t dwArg);

uint32_t __stdcall ForwardStackArgToSlotHook(uint32_t dwArg)
{

    return g_apfnSlotHooks[10](dwArg);  
}
#endif /* NW_UNIT_ForwardStackArgToSlotHook */

/* ==========================================================================
 * Unit: HandleSlotHookWithArg
 * Source: runtime/HandleSlotHookWithArg.c
 * ========================================================================== */
#ifdef NW_UNIT_HandleSlotHookWithArg
/*
 * Handle Slot Hook With Arg.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-handleslothookwitharg-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef uint32_t (*SlotHookFn)(uint32_t dwArg);   /* receives arg3 */

uint32_t __stdcall HandleSlotHookWithArg(uint32_t dwArg1, uint32_t dwArg2, uint32_t dwArg3)
{
    (void)dwArg1;   
    (void)dwArg2;   

    return g_apfnSlotHooks[1](dwArg3);   
}
#endif /* NW_UNIT_HandleSlotHookWithArg */

/* ==========================================================================
 * Unit: InvokeSlotHookThunk
 * Source: runtime/InvokeSlotHookThunk.c
 * ========================================================================== */
#ifdef NW_UNIT_InvokeSlotHookThunk
/*
 * Invoke Slot Hook Thunk.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-invokeslothookthunk-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef uint32_t (*SlotHookFn)(void);

uint32_t InvokeSlotHookThunk(void)
{
    return g_apfnSlotHooks[3]();   
}
#endif /* NW_UNIT_InvokeSlotHookThunk */

/* ==========================================================================
 * Unit: NativeProbeReturnsTrue
 * Source: runtime/NativeProbeReturnsTrue.c
 * ========================================================================== */
#ifdef NW_UNIT_NativeProbeReturnsTrue
/*
 * Runtime probe that reports whether the native path is available.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-nativeprobereturnstrue-c
 */

int NativeProbeReturnsTrue(void) { return 1; }
#endif /* NW_UNIT_NativeProbeReturnsTrue */

/* ==========================================================================
 * Unit: NoOpStackCleanupStub
 * Source: runtime/NoOpStackCleanupStub.c
 * ========================================================================== */
#ifdef NW_UNIT_NoOpStackCleanupStub
/*
 * No-op slot callback that cleans one stack argument.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-noopstackcleanupstub-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

NW_DIAG void __stdcall
NoOpStackCleanupStub(uint32_t dwUnused) __asm__("_FunNoOpRet4_00402010");

NW_DIAG void __stdcall
NoOpStackCleanupStub(uint32_t dwUnused)
{
    (void)dwUnused;
}
#endif /* NW_UNIT_NoOpStackCleanupStub */

/* ==========================================================================
 * Unit: ProcessSlotHookThunkArg
 * Source: runtime/ProcessSlotHookThunkArg.c
 * ========================================================================== */
#ifdef NW_UNIT_ProcessSlotHookThunkArg
/*
 * Process Slot Hook Thunk Arg.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-processslothookthunkarg-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef uint32_t (__stdcall *SlotHookFn)(uint32_t dwArg);

uint32_t __stdcall ProcessSlotHookThunkArg(uint32_t dwArg)
{

    return g_apfnSlotHooks[9](dwArg);   
}
#endif /* NW_UNIT_ProcessSlotHookThunkArg */

/* ==========================================================================
 * Unit: RetryLockedProbeDispatch
 * Source: runtime/RetryLockedProbeDispatch.c
 * ========================================================================== */
#ifdef NW_UNIT_RetryLockedProbeDispatch
/*
 * Retries a locked probe dispatch after re-reading the stream header.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-retrylockedprobedispatch-c
 */

#include <stdint.h>
#include <windows.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

extern unsigned char nw_data_window[] __asm__("nw_data_window");

extern int __stdcall ResetTileRecordStateAndSeekStream(LONG lRecordStreamOffset);
extern int __stdcall ReadExactChunkFromStream(DWORD cbRequestedBytes,
                                              void *lpBuffer);
extern int __stdcall SeekStreamFileSlotToOffset(DWORD dwMoveMethod,
                                                LONG lOffset);
extern uint32_t __cdecl DispatchSubsystemInitByOpcode(void);
extern uint32_t __cdecl ComposeFrameCellsFromStreamRecords(void);
extern BOOL __cdecl ReadPaletteBlockAndSetLogicalPalette(void);
extern int __cdecl UpdateCachedSlotStatus(void);
extern BOOL __cdecl ReadBlockAndLatchPendingSlotHook(void);
extern int __cdecl ResetSubsystemsAndFreeGlobalSlots(void);
extern void __cdecl ResetSubsystemsAndLatchStateFlag(void);

#define NW_PROBE_HEADER ((uint8_t *)(void *)(nw_data_window + 0x16f8))
#define NW_PROBE_SEEK_OFFSET \
    (*(LONG *)(void *)(nw_data_window + 0x16fc))
/* The action table is pointer-sized.  On 64-bit builds the native pointers
 * live in the window side table; reading the 32-bit snapshot would truncate
 * them to an unmapped offset (the startup probe then faults at that offset). */
#define NW_PROBE_ACTION_ENTRY(index) \
    ((NwProbeAction)(uintptr_t)NW_WINDOW_PTR_VALUE(0x2f8u + (uint32_t)(index) * 4u))

typedef uint8_t (__cdecl *NwProbeAction)(void);

/* The fixed jump table at window+0x2f8 contains these five labels. Keep the
 * table and its targets instead of folding the dispatch into a private switch,
 * so the data-window table remains live. Each target performs one action and
 * returns its AL status. The labels are retained for the data-window snapshot. */
uint8_t ProbeActionResetSubsystems(void) __asm__("WAnch_00406e84");
uint8_t ProbeActionResetSubsystems(void)
{
    return (uint8_t)DispatchSubsystemInitByOpcode();
}

uint8_t ProbeActionComposeFrame(void) __asm__("WAnch_00406e8f");
uint8_t ProbeActionComposeFrame(void)
{
    return (uint8_t)ComposeFrameCellsFromStreamRecords();
}

uint8_t ProbeActionReadPalette(void) __asm__("WAnch_00406e9a");
uint8_t ProbeActionReadPalette(void)
{
    return (uint8_t)ReadPaletteBlockAndSetLogicalPalette();
}

uint8_t ProbeActionUpdateSlot(void) __asm__("WAnch_00406ea5");
uint8_t ProbeActionUpdateSlot(void)
{
    return (uint8_t)UpdateCachedSlotStatus();
}

uint8_t ProbeActionReadBlock(void) __asm__("WAnch_00406eb0");
uint8_t ProbeActionReadBlock(void)
{
    return (uint8_t)ReadBlockAndLatchPendingSlotHook();
}

uint32_t __stdcall RetryLockedProbeDispatch(uint32_t uEntry)
    __asm__("_RetryLockedProbeDispatch");

NW_DIAG uint32_t __stdcall RetryLockedProbeDispatch(uint32_t uEntry)
{
    uint8_t *pHeader = NW_PROBE_HEADER;
    uint8_t uStatus;

    if (!ResetTileRecordStateAndSeekStream((LONG)uEntry)) {
        ResetSubsystemsAndLatchStateFlag();
        return 0u;
    }

    for (;;) {
        uStatus = 0u;
        if (ReadExactChunkFromStream(0x0cu, pHeader)) {
            uint8_t uSelector = pHeader[0];

            if (uSelector <= 4u) {
                NwProbeAction pAction = NW_PROBE_ACTION_ENTRY(uSelector);
                uStatus = pAction();
            }
        }

        for (;;) {
            int32_t nStatus = (int32_t)(int8_t)uStatus;

            if (nStatus == 2) {
                if (!ReadExactChunkFromStream(0x0cu, pHeader)) {
                    ResetSubsystemsAndLatchStateFlag();
                    return 0u;
                }
                if (pHeader[0] != 0u) {
                    if (!SeekStreamFileSlotToOffset(1u, NW_PROBE_SEEK_OFFSET)) {
                        ResetSubsystemsAndLatchStateFlag();
                        return 0u;
                    }
                    continue;
                }

                if (pHeader[1] == 0u || pHeader[1] == 1u) {
                    uStatus = (uint8_t)DispatchSubsystemInitByOpcode();
                }
                continue;
            }
            if (nStatus == 0) {
                ResetSubsystemsAndLatchStateFlag();
                return 0u;
            }
            if (nStatus == 3) {
                return (uint32_t)ResetSubsystemsAndFreeGlobalSlots();
            }
            break;
        }
    }
}
#endif /* NW_UNIT_RetryLockedProbeDispatch */
