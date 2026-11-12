#include "nwarch.h"
/*
 * OpcodeExecution -- VM opcode fetch, decode, and execution.
 *
 * Module: vm
 * Interface: include/OpcodeExecution.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   DispatchScriptOpcode                                       vm/DispatchScriptOpcode.c
 *   ExecuteOpcodeAndStoreOperand                               vm/ExecuteOpcodeAndStoreOperand.c
 *   ExecuteOpcodeStream                                        vm/ExecuteOpcodeStream.c
 *   ExecuteOpcodeStreamPreservingGlobals                       vm/ExecuteOpcodeStreamPreservingGlobals.c
 *   ExecuteScriptInstruction                                   vm/ExecuteScriptInstruction.c
 *   FetchNextStreamByteIntoSlotDescriptor                      vm/FetchNextStreamByteIntoSlotDescriptor.c
 *   InterpretOpcodeStreamUntilEnd                              vm/InterpretOpcodeStreamUntilEnd.c
 *   ResetStreamCursorState                                     vm/ResetStreamCursorState.c
 */

/* ==========================================================================
 * Unit: DispatchScriptOpcode
 * Source: vm/DispatchScriptOpcode.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchScriptOpcode
/*
 * Dispatch Script Opcode.
 *
 * Module: vm
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#vm-dispatchscriptopcode-c
 */

#include <stdint.h>
#include <windows.h>
#include "nwglobals.h"

/* -------------------------------------------------------------------------
 * VM state
 * ---------------------------------------------------------------------- */

typedef struct NwVm {

    uint8_t stack[8192 + 20];
    uint8_t *sp;                /* top of the VM value stack (lowest address) */

    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;

    uintptr_t esi;
    uintptr_t edi;              /* g_pVmBaseA, bound once at entry  */
    uintptr_t ebx;              /* g_pVmBaseB, bound once at entry  */
} NwVm;

static inline uint32_t nw_ld32(const void *p)
{
    uint32_t value;
    __builtin_memcpy(&value, p, sizeof value);
    return value;
}

static inline void nw_st32(void *p, uint32_t value)
{
    __builtin_memcpy(p, &value, sizeof value);
}

static inline void nw_vm_push(NwVm *vm, uint32_t value)
{
    vm->sp -= 4;
    nw_st32(vm->sp, value);
}

static inline uint32_t nw_vm_pop(NwVm *vm)
{
    uint32_t value = nw_ld32(vm->sp);
    vm->sp += 4;
    return value;
}


extern uint32_t NwSetLiveInputByteAndReturnOne(void);
extern uint32_t NwGridCtxYIsNone(void);
extern uint32_t __stdcall NwGridCtxSlotIndexEquals(uint32_t);
extern uint32_t NwGridCtxLayerHasMask90_10(void);
extern uint32_t NwGridCtxLayerHasMaskA0_20(void);
extern uint32_t __stdcall NwGridCtxLayerMaskB0Equals(uint32_t);
extern uint32_t __stdcall NwGridCtxLayerMask4FEquals(uint32_t);
extern uint32_t __stdcall NwGridCtxLayerIndexEquals(uint32_t);
extern uint32_t __stdcall NwGridCtxLayerEquals(uint32_t);
extern uint32_t __stdcall NwRunOpcodeStreamAtArgIndex(uint32_t);
extern uint32_t NwGridCtxPendingIsNone(void);
extern uint32_t __stdcall NwGridCtxPendingEquals(uint32_t);
extern uint32_t __stdcall NwCellPackedBitForArg(uint32_t);
extern uint32_t __stdcall NwStringContainsLiveInput(uint32_t);
extern uint32_t __stdcall NwDispatchPendingAndGlobalException(uint32_t);
extern uint32_t __stdcall NwLoadRecordsAndGlobalException(uint32_t);
extern uint32_t __stdcall DispatchPendingSlotHookForIndex(uint32_t);
extern uint32_t __stdcall NwDispatchPendingRefSelField(uint32_t);
extern uint32_t __stdcall DispatchIndexedProbeEntry(uint32_t)
    __asm__("_DispatchIndexedProbeEntry");
extern uint32_t __stdcall NwAcquireScaledSpanAndArm(uint32_t);
extern uint32_t __stdcall NwAcquireSpansAndRunStream(uint32_t);
extern uint32_t __stdcall NwInitSpanRunScaled(uint32_t, uint32_t);
extern uint32_t __stdcall NwInitSpanRunFromTable(uint32_t, uint32_t);
extern uint32_t __stdcall NwDecodeSpanRunScaled(uint32_t, uint32_t);
extern uint32_t __stdcall NwDecodeSpanRunFromTable(uint32_t, uint32_t);
extern uint32_t __stdcall StampGridContextLayerAndPublishDepthSlot(uint32_t);
extern uint32_t ReleaseCurrentContextSpanGroup(void);
extern uint32_t NwProcessPendingSpanEntry(void);
extern uint32_t __stdcall NwAdvancePendingSpanEntryKeepIndex(uint32_t);
extern uint32_t __stdcall NwAdvancePendingSpanEntryRedrawIndex(uint32_t);
extern uint32_t NwReleaseContextSpanByYIndex(void);
extern uint32_t __stdcall NwInstallActiveCellRefWithRefresh(uint32_t);
extern uint32_t NwInstallActiveCellRefZero(void);
extern uint32_t NwGridCtxLayerFromRowCount(void);
extern uint32_t __stdcall NwGridCtxReadLayer(uint32_t);
extern uint32_t __stdcall DispatchThreeToElevenCommandToSentinelGroupMapper(uint32_t);
extern uint32_t __stdcall DispatchOneBasedCommandToSentinelGroupMapper(uint32_t,
                                                                       uint32_t);
extern uint32_t __stdcall DispatchOneBasedCommandToGridRenderMapper(uint32_t,
                                                                    uint32_t);
extern uint32_t __stdcall NwReachedStepCodeEquals(uint32_t);
extern uint32_t __stdcall NwReachedBearingCodeEquals(uint32_t);
extern uint32_t __stdcall NwReachedStepCodeMatches(uint32_t);
extern uint32_t __stdcall NwReachedBearingCodeMatches(uint32_t);
extern uint32_t __stdcall NwReachedCellCommand(uint32_t);
extern uint32_t __stdcall NwReachedCellAllocFromArg(uint32_t);
extern uint32_t __stdcall NwReachedCellAllocFromArgOrOne(uint32_t);
extern uint32_t __stdcall NwGridCtxFlagsMatch(uint32_t);
extern uint32_t __stdcall NwCopyStringSwapArgs(uint32_t, uint32_t);
extern uint32_t __stdcall IsSettingActiveByIndex(uint32_t)
    __asm__("_IsSettingActiveByIndex");
extern uint32_t __stdcall HandleAudioTransportOpcode(uint32_t);
extern uint32_t __stdcall NwSaveDispatchByMode(uint32_t, uint32_t);
extern uint32_t __stdcall NwSelectGridRecordBase(uint32_t);
extern uint32_t NwClearGridRecordBase(void);
extern uint32_t __stdcall NwDispatchWinHelp(uint32_t);
extern uint32_t __stdcall NwStoreByteThroughWindowOffset(uint32_t, uint32_t);
extern uint32_t __stdcall NwRngScaleByArg(uint32_t);
extern uint32_t __stdcall ResetSlotTablesFormatApplyAndDispatchSettings(uint32_t)
    __asm__("_ResetSlotTablesFormatApplyAndDispatchSettings");
extern uint32_t __stdcall NwResetAndRunCommand(uint32_t);
extern uint32_t __stdcall NwInputFieldContainsArg(uint32_t);
extern uint32_t __stdcall UpdateMciAudioStateAndPlay(uint32_t);
extern uint32_t __stdcall DispatchAudioActionByIndex(uint32_t);
extern uint32_t __stdcall NwSetCellIndexFromArg(uint32_t);
extern uint32_t __stdcall ScanFourDecimalFieldsToByteOrWordGlobals(uint32_t,
                                                                   uint32_t);

#define NW_ARG(vm, index) nw_ld32((vm)->sp + 4u * (index))
#define NW_CALL0(fn) return fn()
#define NW_CALL1(fn)                                                       \
    do {                                                                   \
        uint32_t nw_ret = fn(NW_ARG(vm, 0));                               \
        vm->sp += 4u;                                                      \
        return nw_ret;                                                     \
    } while (0)
#define NW_CALL2(fn)                                                       \
    do {                                                                   \
        uint32_t nw_a0 = NW_ARG(vm, 0);                                    \
        uint32_t nw_a1 = NW_ARG(vm, 1);                                    \
        uint32_t nw_ret = fn(nw_a0, nw_a1);                                \
        vm->sp += 8u;                                                      \
        return nw_ret;                                                     \
    } while (0)

__attribute__((section(".nwdiag")))
static uint32_t nw_vm_call_native(NwVm *vm, uint32_t index)
{
    switch (index) {
    case 0:  NW_CALL0(NwSetLiveInputByteAndReturnOne);
    case 1:  NW_CALL0(NwGridCtxYIsNone);
    case 2:  NW_CALL1(NwGridCtxSlotIndexEquals);
    case 3:  NW_CALL0(NwGridCtxLayerHasMask90_10);
    case 4:  NW_CALL0(NwGridCtxLayerHasMaskA0_20);
    case 5:  NW_CALL1(NwGridCtxLayerMaskB0Equals);
    case 6:  NW_CALL1(NwGridCtxLayerMask4FEquals);
    case 7:  NW_CALL1(NwGridCtxLayerIndexEquals);
    case 8:  NW_CALL1(NwGridCtxLayerEquals);
    case 9:  NW_CALL1(NwRunOpcodeStreamAtArgIndex);
    case 10: NW_CALL0(NwGridCtxPendingIsNone);
    case 11: NW_CALL1(NwGridCtxPendingEquals);
    case 12: NW_CALL1(NwCellPackedBitForArg);
    case 13: NW_CALL1(NwStringContainsLiveInput);
    case 14: NW_CALL1(NwDispatchPendingAndGlobalException);
    case 15: NW_CALL1(NwLoadRecordsAndGlobalException);
    case 16: NW_CALL1(DispatchPendingSlotHookForIndex);
    case 17: NW_CALL1(NwDispatchPendingRefSelField);
    case 18: NW_CALL1(DispatchIndexedProbeEntry);
    case 19: NW_CALL1(NwAcquireScaledSpanAndArm);
    case 20: NW_CALL1(NwAcquireSpansAndRunStream);
    case 21: NW_CALL2(NwInitSpanRunScaled);
    case 22: NW_CALL2(NwInitSpanRunFromTable);
    case 23: NW_CALL2(NwDecodeSpanRunScaled);
    case 24: NW_CALL2(NwDecodeSpanRunFromTable);
    case 25: NW_CALL1(StampGridContextLayerAndPublishDepthSlot);
    case 26: NW_CALL0(ReleaseCurrentContextSpanGroup);
    case 27: NW_CALL0(NwProcessPendingSpanEntry);
    case 28: NW_CALL1(NwAdvancePendingSpanEntryKeepIndex);
    case 29: NW_CALL1(NwAdvancePendingSpanEntryRedrawIndex);
    case 30: NW_CALL0(NwReleaseContextSpanByYIndex);
    case 31: NW_CALL1(NwInstallActiveCellRefWithRefresh);
    case 32: NW_CALL0(NwInstallActiveCellRefZero);
    case 33: NW_CALL0(NwGridCtxLayerFromRowCount);
    case 34: NW_CALL1(NwGridCtxReadLayer);
    case 35: NW_CALL1(DispatchThreeToElevenCommandToSentinelGroupMapper);
    case 36: NW_CALL2(DispatchOneBasedCommandToSentinelGroupMapper);
    case 37: NW_CALL2(DispatchOneBasedCommandToGridRenderMapper);
    case 38: NW_CALL1(NwReachedStepCodeEquals);
    case 39: NW_CALL1(NwReachedBearingCodeEquals);
    case 40: NW_CALL1(NwReachedStepCodeMatches);
    case 41: NW_CALL1(NwReachedBearingCodeMatches);
    case 42: NW_CALL1(NwReachedCellCommand);
    case 43: NW_CALL1(NwReachedCellAllocFromArg);
    case 44: NW_CALL1(NwReachedCellAllocFromArgOrOne);
    case 45: NW_CALL1(NwGridCtxFlagsMatch);
    case 46: NW_CALL2(NwCopyStringSwapArgs);
    case 47: NW_CALL1(IsSettingActiveByIndex);
    case 48: NW_CALL1(HandleAudioTransportOpcode);
    case 49: NW_CALL2(NwSaveDispatchByMode);
    case 50: NW_CALL1(NwSelectGridRecordBase);
    case 51: NW_CALL0(NwClearGridRecordBase);
    case 52: NW_CALL1(NwDispatchWinHelp);
    case 53: NW_CALL2(NwStoreByteThroughWindowOffset);
    case 54: NW_CALL1(NwRngScaleByArg);
    case 55: NW_CALL1(ResetSlotTablesFormatApplyAndDispatchSettings);
    case 56: NW_CALL1(NwResetAndRunCommand);
    case 57: NW_CALL1(NwInputFieldContainsArg);
    case 58: NW_CALL1(UpdateMciAudioStateAndPlay);
    case 59: NW_CALL1(DispatchAudioActionByIndex);
    case 60: NW_CALL1(NwSetCellIndexFromArg);
    case 61: NW_CALL2(ScanFourDecimalFieldsToByteOrWordGlobals);
    default: return 0u;
    }
}

/* -------------------------------------------------------------------------
 * Dispatcher
 * ---------------------------------------------------------------------- */

__attribute__((section(".nwdiag")))
uint32_t __stdcall DispatchScriptOpcode(uint8_t *pCursor)
{
    NwVm vm;
    uint32_t trace_next = 0u;
#if NW_WINDOW_PTR_SIDETABLE
    uint8_t *guest_stack;
    /* The VM bytecode treats ESP as a 32-bit guest address.  The original
     * 8192-byte stack lives in this frame on 32-bit builds; on 64-bit hosts a
     * native stack address would be truncated.  Allocate a small low-address
     * stack once per nesting level and reuse it.  Freeing/allocating 0x3000
     * bytes for every script opcode made each click progressively slower. */
    static uint8_t *nw_script_stacks[8];
    static int      nw_script_depth;
    int             nw_script_slot = nw_script_depth;
#endif

#if NW_WINDOW_PTR_SIDETABLE
    if (nw_script_slot >= (int)(sizeof nw_script_stacks /
                                sizeof nw_script_stacks[0]))
        return 0u;
    if (nw_script_stacks[nw_script_slot] == NULL) {
#if defined(_WIN64)
        extern void *NwVirtualAllocLow(size_t size, DWORD protect);
        nw_script_stacks[nw_script_slot] =
            (uint8_t *)NwVirtualAllocLow(0x3000u, PAGE_READWRITE);
#else
        nw_script_stacks[nw_script_slot] =
            (uint8_t *)VirtualAlloc(NULL, 0x3000u, MEM_COMMIT,
                                    PAGE_READWRITE);
#endif
    }
    guest_stack = nw_script_stacks[nw_script_slot];
    if (guest_stack == NULL)
        return 0u;
    nw_script_depth = nw_script_slot + 1;
    vm.sp = guest_stack + 8192u;
#else
    vm.sp = vm.stack + 8192u;
#endif
    nw_st32(vm.sp + 0u, 0u);
    nw_st32(vm.sp + 4u, 0u);    /* saved EDI */
    nw_st32(vm.sp + 8u, 0u);    /* saved EBX */
    nw_st32(vm.sp + 12u, 0u);   /* return address (not read by bytecode) */
    nw_st32(vm.sp + 16u, (uint32_t)(uintptr_t)pCursor);
    vm.eax = 0u;
    vm.ecx = 0u;
    vm.edx = 0u;
    vm.esi = (uintptr_t)pCursor;
    vm.edi = (uintptr_t)g_pVmBaseA;
    vm.ebx = (uintptr_t)g_pVmBaseB;

    for (;;) {
        uint32_t op;

        /* Tracing is disabled in this build, but the fetch must remain
           unconditional: the old trace hook used to be the body of this
           conditional.  Keeping opcode fetch inside it skipped every
           instruction following one that cleared trace_next. */
        (void)trace_next;

        op = *(const uint8_t *)vm.esi;
        vm.esi += 1u;

        vm.ecx = op;

        switch (op) {
        case 0x00:                              /* return */
#if NW_WINDOW_PTR_SIDETABLE
            nw_script_depth = nw_script_slot;
#endif
            return vm.eax;

        case 0x01:                              /* edx == eax */
            vm.eax = (vm.edx == vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;
        case 0x02:                              /* edx != eax */
            vm.eax = (vm.edx != vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;
        case 0x03:                              /* edx <  eax, signed */
            vm.eax = ((int32_t)vm.edx < (int32_t)vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;
        case 0x04:                              /* edx <= eax, signed */
            vm.eax = ((int32_t)vm.edx <= (int32_t)vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;
        case 0x05:                              /* edx >  eax, signed */
            vm.eax = ((int32_t)vm.edx > (int32_t)vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;
        case 0x06:                              /* edx >= eax, signed */
            vm.eax = ((int32_t)vm.edx >= (int32_t)vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;
        case 0x07:                              /* edx <  eax, unsigned */
            vm.eax = (vm.edx < vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;
        case 0x08:                              /* edx <= eax, unsigned */
            vm.eax = (vm.edx <= vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;
        case 0x09:                              /* edx >  eax, unsigned */
            vm.eax = (vm.edx > vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;
        case 0x0a:                              /* edx >= eax, unsigned */
            vm.eax = (vm.edx >= vm.eax) ? 1u : 0u;
            trace_next = 0u;
            break;

        case 0x0b:                              /* conditional relative jump */
            if (vm.eax != 0u)
                vm.esi += 4u;
            else
                vm.esi = (uintptr_t)((intptr_t)vm.esi +
                                     (int32_t)nw_ld32((const void *)vm.esi));
            trace_next = 0u;
            break;
        case 0x0c:                              /* relative jump */
            vm.esi = (uintptr_t)((intptr_t)vm.esi +
                                 (int32_t)nw_ld32((const void *)vm.esi));
            trace_next = 0u;
            break;
        case 0x0d:                              /* discard stack bytes */
            vm.sp = (uint8_t *)((intptr_t)vm.sp +
                                (int32_t)nw_ld32((const void *)vm.esi));
            vm.esi += 4u;
            trace_next = 0u;
            break;

        case 0x0e:                              /* store AL at base A + dword */
        case 0x0f: {                            /* store AL at base B + dword */
            uint32_t offset = nw_ld32((const void *)vm.esi);
            uintptr_t base = (op == 0x0e) ? vm.edi : vm.ebx;
            vm.ecx = offset;
            *(uint8_t *)(base + offset) = (uint8_t)vm.eax;
            vm.esi += 4u;
            trace_next = 0u;
            break;
        }
        case 0x10:                              /* store EAX at base A + dword */
        case 0x11: {                            /* store EAX at base B + dword */
            uint32_t offset = nw_ld32((const void *)vm.esi);
            uintptr_t base = (op == 0x10) ? vm.edi : vm.ebx;
            vm.ecx = offset;
            nw_st32((void *)(base + offset), vm.eax);
            vm.esi += 4u;
            trace_next = 0u;
            break;
        }

        case 0x12:
            vm.esi = (uintptr_t)nw_guest_to_host(nw_vm_pop(&vm));
            vm.esi += 4u;
            trace_next = 0u;
            break;
        case 0x13: {                            /* pop pointer, discard stack bytes, skip dword */
            uint32_t ret = nw_vm_pop(&vm);
            vm.ecx = ret;
            vm.sp = (uint8_t *)((intptr_t)vm.sp +
                                (int32_t)nw_ld32((const void *)vm.esi));
            vm.esi = (uintptr_t)nw_guest_to_host(ret) + 4u;
            trace_next = 0u;
            break;
        }

        case 0x14:                              /* edx = eax */
            vm.edx = vm.eax;
            trace_next = 1u;
            break;
        case 0x15:                              /* eax += edx */
            vm.eax += vm.edx;
            trace_next = 1u;
            break;
        case 0x16: {                            /* unsigned multiply */
            uint64_t product = (uint64_t)vm.eax * (uint64_t)vm.edx;
            vm.eax = (uint32_t)product;
            vm.edx = (uint32_t)(product >> 32);
            trace_next = 1u;
            break;
        }
        case 0x17:                              /* eax |= edx */
            vm.eax |= vm.edx;
            trace_next = 1u;
            break;
        case 0x18:                              /* eax ^= edx */
            vm.eax ^= vm.edx;
            trace_next = 1u;
            break;
        case 0x19:                              /* eax &= edx */
            vm.eax &= vm.edx;
            trace_next = 1u;
            break;
        case 0x1a:                              /* neg eax */
            vm.eax = 0u - vm.eax;
            trace_next = 1u;
            break;
        case 0x1b: {                            /* eax = edx >> (old eax low byte) */
            uint32_t count = vm.eax & 0xffu;
            vm.ecx = count;
            vm.eax = (uint32_t)((int32_t)vm.edx >> (count & 31u));
            trace_next = 1u;
            break;
        }
        case 0x1c: {                            /* eax = edx << (old eax low byte) */
            uint32_t count = vm.eax & 0xffu;
            vm.ecx = count;
            vm.eax = vm.edx << (count & 31u);
            trace_next = 1u;
            break;
        }
        case 0x1d:                              /* eax = dword at cursor */
            vm.eax = nw_ld32((const void *)vm.esi);
            vm.esi += 4u;
            trace_next = 1u;
            break;
        case 0x1e:                              /* ++eax */
            vm.eax += 1u;
            trace_next = 1u;
            break;
        case 0x1f:                              /* --eax */
            vm.eax -= 1u;
            trace_next = 1u;
            break;
        case 0x20: {                            /* swap eax, edx */
            uint32_t tmp = vm.eax;
            vm.eax = vm.edx;
            vm.edx = tmp;
            trace_next = 1u;
            break;
        }
        case 0x21:
            nw_vm_push(&vm, vm.eax);
            trace_next = 1u;
            break;
        case 0x22:                              /* pop edx */
            vm.edx = nw_vm_pop(&vm);
            trace_next = 1u;
            break;
        case 0x23:                              /* eax = base A + dword at cursor */
        case 0x24:
            vm.eax = nw_ld32((const void *)vm.esi);
            vm.esi += 4u;
            vm.eax += (uint32_t)vm.edi;
            trace_next = 1u;
            break;
        case 0x25:                              /* eax = base B + dword at cursor */
            vm.eax = nw_ld32((const void *)vm.esi);
            vm.esi += 4u;
            vm.eax += (uint32_t)vm.ebx;
            trace_next = 1u;
            break;
        case 0x26:                              /* eax = stack pointer + dword at cursor */
            vm.eax = nw_ld32((const void *)vm.esi);
            vm.esi += 4u;
            vm.eax += (uint32_t)(uintptr_t)vm.sp;
            trace_next = 1u;
            break;
        case 0x27:                              /* *(u8*)edx = al */
            *(uint8_t *)(uintptr_t)nw_guest_to_host(vm.edx) = (uint8_t)vm.eax;
            trace_next = 1u;
            break;
        case 0x28:                              /* *(u32*)edx = eax */
            nw_st32((void *)(uintptr_t)nw_guest_to_host(vm.edx), vm.eax);
            trace_next = 1u;
            break;
        case 0x29:                              /* eax <<= 2 */
            vm.eax <<= 2;
            trace_next = 1u;
            break;
        case 0x2a:                              /* eax += 4 */
            vm.eax += 4u;
            trace_next = 1u;
            break;
        case 0x2b:                              /* eax -= 4 */
            vm.eax -= 4u;
            trace_next = 1u;
            break;
        case 0x2c: {                            /* swap eax with stack top */
            uint32_t tmp = nw_ld32(vm.sp);
            nw_st32(vm.sp, vm.eax);
            vm.eax = tmp;
            trace_next = 1u;
            break;
        }
        case 0x2d:                              /* eax = edx - eax */
            vm.eax = vm.edx - vm.eax;
            trace_next = 1u;
            break;
        case 0x2e: {                            /* signed divide old edx by old eax */

            uint32_t old_eax = vm.eax;
            int64_t dividend = (int32_t)vm.edx;
            int32_t divisor = (int32_t)old_eax;
            int64_t quotient = dividend / divisor;
            int64_t remainder = dividend % divisor;
            vm.ecx = old_eax;
            vm.eax = (uint32_t)quotient;
            vm.edx = (uint32_t)remainder;
            trace_next = 1u;
            break;
        }
        case 0x2f:                              /* eax = sign-extended byte at base A + eax */
            vm.eax = nw_ld32((const void *)vm.esi);
            vm.esi += 4u;
            vm.eax = (uint32_t)(int32_t)(int8_t)
                *(const uint8_t *)(vm.edi + vm.eax);
            trace_next = 1u;
            break;
        case 0x30:                              /* eax = sign-extended byte at base B + eax */
            vm.eax = nw_ld32((const void *)vm.esi);
            vm.esi += 4u;
            vm.eax = (uint32_t)(int32_t)(int8_t)
                *(const uint8_t *)(vm.ebx + vm.eax);
            trace_next = 1u;
            break;
        case 0x31:                              /* eax = dword at base A + eax */
            vm.eax = nw_ld32((const void *)vm.esi);
            vm.esi += 4u;
            vm.eax = nw_ld32((const void *)(vm.edi + vm.eax));
            trace_next = 1u;
            break;
        case 0x32:                              /* eax = dword at base B + eax */
            vm.eax = nw_ld32((const void *)vm.esi);
            vm.esi += 4u;
            vm.eax = nw_ld32((const void *)(vm.ebx + vm.eax));
            trace_next = 1u;
            break;
        case 0x33:                              /* eax = sign-extended byte at eax */
            vm.eax = (uint32_t)(int32_t)(int8_t)
                *(const uint8_t *)(uintptr_t)nw_guest_to_host(vm.eax);
            trace_next = 1u;
            break;
        case 0x34:                              /* eax = dword at eax */
            vm.eax = nw_ld32((const void *)(uintptr_t)nw_guest_to_host(vm.eax));
            trace_next = 1u;
            break;

        case 0x35:
            nw_vm_push(&vm, (uint32_t)vm.esi);
            vm.esi = vm.edi + nw_ld32((const void *)vm.esi);
            trace_next = 1u;
            break;
        case 0x36: {
            uint32_t index = nw_ld32((const void *)vm.esi);
            vm.esi += 4u;
            vm.eax = nw_vm_call_native(&vm, index);
            trace_next = 1u;
            break;
        }
        case 0x37:
            nw_vm_push(&vm, (uint32_t)vm.esi);
            vm.esi = vm.edi + vm.edx;
            trace_next = 1u;
            break;

        default:
#if NW_WINDOW_PTR_SIDETABLE
            nw_script_depth = nw_script_slot;
#endif
            return 0u;
        }
    }
}
#endif /* NW_UNIT_DispatchScriptOpcode */

/* ==========================================================================
 * Unit: ExecuteOpcodeAndStoreOperand
 * Source: vm/ExecuteOpcodeAndStoreOperand.c
 * ========================================================================== */
#ifdef NW_UNIT_ExecuteOpcodeAndStoreOperand
/*
 * Execute Opcode And Store Operand.
 *
 * Module: vm
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#vm-executeopcodeandstoreoperand-c
 */

#include <stdint.h>
#include <windows.h>
#include "nwglobals.h"

typedef uint8_t u8;

#define NW_DIAG __attribute__((section(".nwdiag")))

typedef struct NwFrameCtl {
    uint8_t  bRunning;
    uint8_t  bFrameDivisor;
    uint8_t  bCharPending;
    uint8_t  bCharLatched;
    uint32_t dwFrameCount;
} NwFrameCtl, *PNwFrameCtl;

#define g_pVmStateTable      NW_WINDOW_PTR_REF(uint8_t, 0x6248)

#define g_pVmContextTable    NW_WINDOW_PTR_REF(uint8_t, 0x624c)

#define g_pVmDescTable2fc    NW_WINDOW_PTR_REF(uint8_t, 0x62fc)

#define g_pVmRefTable300     NW_WINDOW_PTR_REF(uint8_t, 0x6300)

#define g_pVmRecordArray30c  NW_WINDOW_PTR_REF(uint8_t, 0x630c)

#define g_pVmAuxTable324     NW_WINDOW_PTR_REF(uint8_t, 0x6324)

#define g_pVmSlotArgTable328 NW_WINDOW_PTR_REF(uint8_t, 0x6328)

#define g_dwVmOperand37c     (*(uint32_t *)(intptr_t)(nw_data_window + 0x637c))
#define g_dwVmOperand380     (*(uint32_t *)(intptr_t)(nw_data_window + 0x6380))
#define g_dwVmOperand384     (*(uint32_t *)(intptr_t)(nw_data_window + 0x6384))

#define g_dwVmStreamField16dc (*(uint32_t *)(intptr_t)(nw_data_window + 0x16dc))
#define g_dwVmStreamField16e0 (*(uint32_t *)(intptr_t)(nw_data_window + 0x16e0))
#define g_bVmStreamFlag16e5   (*(uint8_t  *)(intptr_t)(nw_data_window + 0x16e5))

#define g_pVmStreamRecord16d4 NW_WINDOW_PTR_REF(uint8_t, 0x16d4)
#define g_pVmFrameCtlRecord  NW_WINDOW_PTR_REF(NwFrameCtl, 0x6378)

/*
 * The VM tables are part of the recovered 32-bit image: each pointer entry is
 * a four-byte guest address.  A native pointer load would consume the next
 * field as the upper half on a 64-bit host, so make the guest width explicit.
 */
static uint8_t *NwLoadGuestPointer(const void *pEntry)
{
    return (uint8_t *)(uintptr_t)nw_guest_to_host(*(const uint32_t *)pEntry);
}

#ifdef NW_DEBUG_TOOLS
extern void NwVmTrace(uint32_t opcode, uint32_t slot, uint32_t value);
#endif

extern int32_t __stdcall
ExecuteOpcodeStream(uint32_t dwMode, uint32_t dwStream)
    __asm__("_ExecuteOpcodeStream");

extern void *__stdcall CopyStreamRecordToBuffer(void *pDest);
extern void  __stdcall CopyRecordsToSlotChain(int index);
extern int   __stdcall SetFrameIntervalFromDivisor(uint8_t bDivisor);

NW_DIAG uint8_t __stdcall
ExecuteOpcodeAndStoreOperand(uint32_t arg1_operand, uint32_t arg2_addend,
                             uint32_t arg3_index, uint32_t arg4_index,
                             uint32_t arg5_slot, uint32_t arg6_opcode)
    __asm__("_ExecuteOpcodeAndStoreOperand");

NW_DIAG uint8_t __stdcall
ExecuteOpcodeAndStoreOperand(uint32_t arg1_operand, uint32_t arg2_addend,
                             uint32_t arg3_index, uint32_t arg4_index,
                             uint32_t arg5_slot, uint32_t arg6_opcode)
{
    uint8_t *p;

#ifdef NW_DEBUG_TOOLS
    NwVmTrace(arg6_opcode, arg5_slot, arg1_operand);
#endif

    switch (arg6_opcode) {
    case 0x11:
    case 0x13:
        break;

    case 0x12:
        /* Publish the operand as the frame-control record, stamp its divisor
         * byte from the latched frame divisor, then re-arm the frame timer. */
        g_pVmFrameCtlRecord = (NwFrameCtl *)(uintptr_t)nw_guest_to_host(arg1_operand);
        ((uint8_t *)(uintptr_t)nw_guest_to_host(arg1_operand))[1] = g_bLastDivisor;
        (void)SetFrameIntervalFromDivisor(g_bLastDivisor);
        break;

    case 0x18:
        /* Refresh the 16-byte record selected by arg5. */
        /* +0x630c carries a guest base (save-file record array); widen it. */
        p = (uint8_t *)(uintptr_t)nw_guest_to_host(
            (uint32_t)(uintptr_t)g_pVmRecordArray30c +
            (uint32_t)arg5_slot * 16u);
        *(uint32_t *)(void *)(p + 0)  = (uint32_t)(uintptr_t)g_pColorTableBlock;
        *(uint32_t *)(void *)(p + 4)  = g_dwVmStreamField16dc;
        *(uint32_t *)(void *)(p + 8)  = 0;
        *(uint32_t *)(void *)(p + 12) = 0;
        if (g_dwVmStreamField16e0 == 0xffffffffu &&
            (*(const uint32_t *)(const void *)(uintptr_t)nw_guest_to_host(arg1_operand) & 0x80000000u) != 0u) {
            g_dwVmStreamField16e0 = arg5_slot;
        }
        break;

    case 0x19: {
        uint16_t *pCell;
        uint32_t count;

        if (g_bSaveLoadActive != 0u) {
            break;
        }
        pCell = g_pCellGrid;
        count = (uint32_t)g_nGridRowCount << (g_dwAxisShift & 31u);
        do {
            *pCell++ = 0xf0feu;
        } while (--count != 0u);

        g_bDispatchAllowedFlag = 1;
        (void)ExecuteOpcodeStream(1u, arg1_operand);
        g_bDispatchAllowedFlag = 0;
        CopyRecordsToSlotChain((int)arg5_slot);
        break;
    }

    case 0x20:
        (void)CopyStreamRecordToBuffer(g_pVmStateTable + (uint32_t)arg5_slot * 16u);
        break;

    case 0x21:
        *(uint32_t *)(void *)(g_pVmStateTable + (uint32_t)arg5_slot * 16u + 4u) =
            arg1_operand + arg2_addend;
        break;

    case 0x22:
        *(uint32_t *)(void *)(g_pVmStateTable + (uint32_t)arg5_slot * 16u + 12u) =
            arg1_operand + arg2_addend;
        break;

    case 0x23:
        *(uint32_t *)(void *)(g_pVmStateTable + (uint32_t)arg5_slot * 16u + 8u) =
            arg1_operand;
        break;

    case 0x2a:
        *(uint32_t *)(void *)((uintptr_t)nw_guest_to_host(
            *(const uint32_t *)(const void *)
            (g_pVmStateTable + (uint32_t)arg5_slot * 16u + 8u)) +
            (uint32_t)arg4_index * 4u) = arg1_operand;
        break;

    case 0x2b: {
        uint8_t *pNode = NwLoadGuestPointer(
            g_pVmStateTable + (uint32_t)arg5_slot * 16u + 8u);
        uint8_t *pSlot = NwLoadGuestPointer(
            pNode + (uint32_t)arg4_index * 4u);
        *(uint32_t *)(void *)(pSlot + 4u) = arg1_operand + arg2_addend;
        break;
    }

    case 0x2c: {
        uint8_t *pNode = NwLoadGuestPointer(
            g_pVmStateTable + (uint32_t)arg5_slot * 16u + 8u);
        uint8_t *pSlot = NwLoadGuestPointer(
            pNode + (uint32_t)arg4_index * 4u);
        uint32_t idx = 1;

        if ((*(const uint32_t *)(const void *)pSlot & 1u) != 0u) {
            idx++;
        }
        if ((*(const uint8_t *)(const void *)pSlot & 2u) != 0u) {
            uint32_t v;
            do {
                v = *(const uint32_t *)(const void *)(pSlot + idx * 4u);
                idx++;
                idx += v & 0xffffu;
            } while ((v & 0x10000u) == 0u);
        }
        *(uint32_t *)(void *)(pSlot + idx * 4u) = arg1_operand + arg2_addend;
        break;
    }

    case 0x38:
        *(uint32_t *)(void *)(g_pVmContextTable + (uint32_t)arg5_slot * 16u + 0u) =
            arg1_operand;
        break;

    case 0x39:
        *(uint32_t *)(void *)(g_pVmContextTable + (uint32_t)arg5_slot * 16u + 4u) =
            arg1_operand;
        break;

    case 0x3a:
        *(uint32_t *)(void *)(g_pVmContextTable + (uint32_t)arg5_slot * 16u + 8u) =
            arg1_operand;
        break;

    case 0x40:
        (void)CopyStreamRecordToBuffer(g_pVmRefTable300 + (uint32_t)arg5_slot * 8u);
        g_bVmStreamFlag16e5 = (uint8_t)(g_pVmStreamRecord16d4[1] & 0x80u);
        break;

    case 0x41:
        *(uint32_t *)(void *)(g_pVmRefTable300 + (uint32_t)arg5_slot * 8u + 4u) =
            arg1_operand;
        break;

    case 0x42: {
        uint8_t *pEntry = g_pVmRefTable300 + (uint32_t)arg5_slot * 8u;
        if (pEntry[0] == 0u && pEntry[2] > 1u) {
            *(uint32_t *)(void *)((uintptr_t)nw_guest_to_host(
                *(const uint32_t *)(const void *)(pEntry + 4)) +
                (uint32_t)arg4_index * 4u) = arg1_operand;
        } else {
            *(uint32_t *)(void *)(pEntry + 4) = arg1_operand;
        }
        break;
    }

    case 0x43: {
        uint8_t *pEntry = g_pVmRefTable300 + (uint32_t)arg5_slot * 8u;
        uint8_t *pDest = NwLoadGuestPointer(pEntry + 4);
        if (pEntry[0] == 0u && pEntry[2] > 1u) {
            pDest = NwLoadGuestPointer(
                pDest + (uint32_t)arg4_index * 4u);
        }
        *(uint32_t *)(void *)(pDest + (uint32_t)arg3_index * 8u) = arg1_operand;
        break;
    }

    case 0x50:
        (void)CopyStreamRecordToBuffer(g_pVmDescTable2fc + (uint32_t)arg5_slot * 8u);
        break;

    case 0x51:
        *(uint32_t *)(void *)(g_pVmDescTable2fc + (uint32_t)arg5_slot * 8u + 4u) =
            arg1_operand;
        break;

    case 0x52:
        *(uint32_t *)(void *)(g_pVmSlotArgTable328 + (uint32_t)arg5_slot * 4u) =
            arg1_operand;
        break;

    case 0x60:
        *(uint32_t *)(void *)(g_pVmAuxTable324 + (uint32_t)arg5_slot * 8u + 0u) =
            arg1_operand;
        break;

    case 0x61:
        *(uint32_t *)(void *)(g_pVmAuxTable324 + (uint32_t)arg5_slot * 8u + 4u) =
            arg1_operand;
        break;

    case 0x7c:
        g_dwVmOperand37c = arg1_operand;
        NW_WINDOW_PTR_VALUE(0x637c) = nw_guest_to_host(arg1_operand);
        break;

    case 0x7d:
        g_dwVmOperand380 = arg1_operand;
        NW_WINDOW_PTR_VALUE(0x6380) = nw_guest_to_host(arg1_operand);
        break;

    case 0x7e:
        g_dwVmOperand384 = arg1_operand;
        NW_WINDOW_PTR_VALUE(0x6384) = nw_guest_to_host(arg1_operand);
        break;

    default:
        break;
    }

    return 1u;
}

/* Keep the bare and underscored symbol spellings used by the fixed VM and
 * by the data-window relocations.  This emits no executable bytes. */
__asm__(".globl ExecuteOpcodeAndStoreOperand\n"
        ".set ExecuteOpcodeAndStoreOperand, _ExecuteOpcodeAndStoreOperand\n");
#endif /* NW_UNIT_ExecuteOpcodeAndStoreOperand */

/* ==========================================================================
 * Unit: ExecuteOpcodeStream
 * Source: vm/ExecuteOpcodeStream.c
 * ========================================================================== */
#ifdef NW_UNIT_ExecuteOpcodeStream
/*
 * Execute Opcode Stream.
 *
 * Module: vm
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#vm-executeopcodestream-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define NW8(off)  (*(uint8_t  *)(intptr_t)(nw_data_window + (off)))
#define NW16(off) (*(uint16_t *)(intptr_t)(nw_data_window + (off)))
#define NW32(off) (*(uint32_t *)(intptr_t)(nw_data_window + (off)))

#define G_GRID_AUX           NW_WINDOW_PTR_VALUE(0x6214)
#define G_CUR_CTX            NW_WINDOW_PTR_VALUE(0x6218)
#define G_GRID_USER          NW32(0x621c)
#define G_AXIS_MINOR_OFFSET  NW32(0x6220)
#define G_AXIS_MAJOR_OFFSET  NW32(0x6224)
#define G_AXIS_MINOR_COPY    NW32(0x6228)
#define G_AXIS_MAJOR_COPY    NW32(0x622c)
#define G_WORD_OBSERVER      NW_WINDOW_PTR_VALUE(0x6230)
#define G_CELL_OPERAND_HOOK  NW_WINDOW_PTR_VALUE(0x6234)
#define G_SAVE_38            NW32(0x6238)
#define G_SAVE_3c            NW32(0x623c)
#define G_VM_BASE_B          NW_WINDOW_PTR_VALUE(0x63e8)
#define G_FLAG_67f6          NW8(0x67f6)
#define G_FLAG_67fc          NW8(0x67fc)
#define G_FLAG_1200          NW8(0x1200)
#define G_WORD_TABLE_1208    ((uint16_t *)(intptr_t)(nw_data_window + 0x1208))
#define G_POOL_TABLE_BASE    ((uint8_t  *)(uintptr_t)NW_WINDOW_PTR_VALUE(0x624c))
#define G_CELL_GRID          ((uint16_t *)(uintptr_t)NW_WINDOW_PTR_VALUE(0x6250))
#define G_AXIS_SHIFT         NW32(0x6254)

#define G_COLUMNS            (*(int *)(intptr_t)(nw_data_window + 0x61e8))
#define G_ROWS               (*(int *)(intptr_t)(nw_data_window + 0x61ec))

/* The two 16-slot relocated tables: pool handlers at window+0x114 and opcode
 * handlers at window+0x178.  Their entries are stdcall, one pointer argument. */
extern void (__stdcall *g_apPoolHandlers[16])(uint32_t *);
extern void (__stdcall *g_apOpcodeHandlers[16])(uint32_t *);

extern uint32_t dwRngState;

extern uint32_t __stdcall DispatchScriptOpcode(uint8_t *pCursor);
extern void __stdcall WrapGridAxisCursorsStampCellKeyAndRetireSpans(uint32_t block);
extern void __stdcall PublishGridCellSpanByDepthIndexToExtentList(int nIndex);
extern void __stdcall PublishCursorSlotSpanAndFlagContextOnOperandMismatch(void *pCtx);

extern void __stdcall NoOpStackCleanupStub(uint32_t dwUnused)
    __asm__("_FunNoOpRet4_00402010");
extern int  __stdcall NotifyCellOperandHookAndAllocateGridSpanRecord(uint32_t dwBlock);
extern int  __stdcall WrapGridAxesAndDispatch(uint32_t dwBlock);

#define NW_RNG_STEP() \
    (dwRngState = dwRngState * 0x41c64e6dU + 0x3039U)
#define NW_RNG_HIGH() ((uint32_t)(uint16_t)(dwRngState >> 16))

NW_DIAG int __stdcall
ExecuteOpcodeStream(uint32_t dwMode, uint32_t dwStream)
    __asm__("_ExecuteOpcodeStream");

NW_DIAG int __stdcall
ExecuteOpcodeStream(uint32_t dwMode, uint32_t dwStream)
{
    uint32_t *cursor = (uint32_t *)(uintptr_t)nw_guest_to_host(dwStream);
    uint32_t *blockEnd;
    uint32_t  blockHeader;
    uint32_t  topFlags;
    uint32_t  word;
    uint32_t  localE;
    uint32_t  oldByte1;
    uint32_t  currentByte1;
    uint32_t  cellWord;
    int32_t   depth;
    int32_t   sum = 0;

    uint32_t  lowStack[8];
    uint32_t  highStack[8];
    uint32_t  dummyLow = 0;
    uint32_t  dummyHigh = 0;
    uint32_t *pLow;
    uint32_t *pHigh;

    if (dwMode == 0u) {
        uint8_t *ctx = (uint8_t *)(uintptr_t)G_CUR_CTX;
        G_AXIS_MINOR_OFFSET = ctx[6];
        G_AXIS_MINOR_COPY   = ctx[6];
        G_AXIS_MAJOR_OFFSET = ctx[7];
        G_AXIS_MAJOR_COPY   = ctx[7];
        G_CELL_OPERAND_HOOK = (nw_host_ptr_t)(uintptr_t)&WrapGridAxesAndDispatch;
    } else {
        G_CUR_CTX            = 0u;
        G_GRID_USER          = 0xffffu;
        G_GRID_AUX           = 0u;
        G_SAVE_38            = 0u;
        G_SAVE_3c            = 0xffffu;
        G_AXIS_MAJOR_OFFSET  = 0u;
        G_AXIS_MINOR_OFFSET  = 0u;
        G_AXIS_MAJOR_COPY    = 0u;
        G_AXIS_MINOR_COPY    = 0u;
        G_FLAG_67f6          = 0x10u;
        G_WORD_OBSERVER      = (nw_host_ptr_t)(uintptr_t)&NoOpStackCleanupStub;
        G_CELL_OPERAND_HOOK  = (nw_host_ptr_t)(uintptr_t)
                               &NotifyCellOperandHookAndAllocateGridSpanRecord;
        G_VM_BASE_B          = 0u;
    }

    topFlags = *cursor++;
    if ((topFlags & 1u) != 0u) {
        /* The stream slot holds the script pointer itself, not its address. */
        if (DispatchScriptOpcode((uint8_t *)(uintptr_t)nw_guest_to_host(*cursor)) == 0u) {
            return 0;
        }
        cursor++;
    }

    if ((topFlags & 2u) != 0u) {
        depth = -1;
        for (;;) {
            blockHeader = *cursor++;
            blockEnd = cursor + (blockHeader & 0xffffu);

            if ((blockHeader >> 24) == 4u) {
                depth++;
                if (depth == 0) {
                    lowStack[0] = 0u;
                    highStack[0] = 0u;
                } else {
                    lowStack[depth] = highStack[depth - 1];
                    highStack[depth] = highStack[depth - 1];
                }
            } else {
                depth = -1;
            }

            if (depth >= 0) {
                pLow = &lowStack[depth];
                pHigh = &highStack[depth];
            } else {

                pLow = &dummyLow;
                pHigh = &dummyHigh;
            }

            for (;;) {
                word = *cursor++;
                ((void (__stdcall *)(uint32_t *))(uintptr_t)G_WORD_OBSERVER)(
                    &word);

                if (dwMode != 0u) {
                    uint32_t minor = (word >> 16) & 0xffu;
                    uint32_t major = (word >> 24) & 0xffu;
                    uint32_t index;

                    index = ((major << (G_AXIS_SHIFT & 31u)) + minor) & 0xffffu;
                    cellWord = (uint32_t)G_CELL_GRID[index];
                } else {
                    int32_t minor = (int32_t)(int8_t)(word >> 16);
                    int32_t major = (int32_t)(int8_t)(word >> 24);
                    int32_t index;

                    minor = (int32_t)((uint32_t)minor + G_AXIS_MINOR_OFFSET +
                                      (uint32_t)G_COLUMNS);
                    minor = minor % G_COLUMNS;
                    major = (int32_t)((uint32_t)major + G_AXIS_MAJOR_OFFSET +
                                      (uint32_t)G_ROWS);
                    major = major % G_ROWS;
                    index = minor + (major << (G_AXIS_SHIFT & 31u));
                    cellWord = (uint32_t)G_CELL_GRID[(uint32_t)index];
                }

                localE = 0u;
                oldByte1 = (word >> 8) & 0xffu;
                if ((oldByte1 & 1u) != 0u) {
                    uint32_t cellIndex = cellWord & 0xffu;
                    if (cellIndex != 0xfeu) {
                        uint8_t *entry =
                            G_POOL_TABLE_BASE + ((word & 0xffu) << 4);
                        /* Pool-table pointer fields keep the original 32-bit
                         * guest layout.  The portable build keeps those values
                         * as low host addresses, so widen four bytes rather
                         * than reading an eight-byte pointer. */
                        uint8_t *keyBits = (uint8_t *)(uintptr_t)
                            nw_guest_to_host(*(uint32_t *)(void *)entry);
                        uint32_t mask = 1u << (cellIndex & 7u);

                        if (keyBits != NULL &&
                            (keyBits[cellIndex >> 3] & mask) != 0u) {
                            uint8_t *handlers = (uint8_t *)(uintptr_t)
                                nw_guest_to_host(*(uint32_t *)(void *)(entry + 8));
                            if (handlers != NULL) {
                                uint8_t newByte1 =
                                    (uint8_t)((oldByte1 & 0x0fu) |
                                              handlers[cellIndex]);
                                word = (word & 0xffff00ffu) |
                                       ((uint32_t)newByte1 << 8);
                                g_apPoolHandlers[oldByte1 >> 4](&word);
                            }
                        } else {
                            goto evaluate_status;
                        }
                    } else {
                        goto evaluate_status;
                    }
                } else {
                    if ((word & 0xffu) != (cellWord & 0xffu)) {
                        goto evaluate_status;
                    }
                }

                currentByte1 = (word >> 8) & 0xffu;
                if (((cellWord >> 8) & currentByte1 & 0xf0u) != 0u) {
                    localE = 2u;
                }

evaluate_status:
                currentByte1 = (word >> 8) & 0xffu;
                if ((currentByte1 & 2u) == localE) {
                    uint32_t control = currentByte1 & 0x0cu;
                    if (control == 0u) {
                        cursor = blockEnd;
                        goto next_outer_block;
                    }
                    if (control == 4u) {
                        return 0;
                    }
                    if (control == 8u) {
                        goto skip_group;
                    }
                    /* control == 0xc: record this item's high word in the
                       window+0x1208 key table and advance that depth's writer. */
                    G_WORD_TABLE_1208[*pHigh] =
                        (uint16_t)(word >> 16);
                    (*pHigh)++;
                } else {
                    if (cursor == blockEnd) {
                        if (depth >= 0 && *pLow == *pHigh) {
                            return 0;
                        }
                    }
                }

                if (cursor != blockEnd) {
                    continue;
                }
                if ((blockHeader & 0x10000u) != 0u) {
                    goto phase_two;
                }
                goto next_outer_block;
            }

next_outer_block:
            ;
        }
    }

phase_two:
    G_FLAG_1200 = 0u;
    if ((topFlags & 4u) != 0u) {
        uint8_t savedByte1 = (uint8_t)(topFlags >> 8);
        ((void (__stdcall *)(uint32_t *))(uintptr_t)G_WORD_OBSERVER)(
            &topFlags);
        g_apOpcodeHandlers[((topFlags >> 8) & 0xffu) >> 4](&topFlags);
        NW_RNG_STEP();
        topFlags = (topFlags & 0xffff00ffu) |
                   ((uint32_t)(uint8_t)((uint8_t)(topFlags >> 8) & 0xf0u) |
                    (uint32_t)(uint8_t)(savedByte1 & 0x0fu)) << 8;
        WrapGridAxisCursorsStampCellKeyAndRetireSpans(topFlags);
    }

    G_FLAG_67fc = 0u;
    if ((topFlags & 8u) != 0u) {
        uint8_t savedSlot = 0u;
        uint8_t *ctx = (uint8_t *)(uintptr_t)G_CUR_CTX;
        if (ctx != NULL) {
            savedSlot = ctx[5];
        }
        (void)DispatchScriptOpcode((uint8_t *)(uintptr_t)nw_guest_to_host(*cursor));
        cursor++;

        ctx = (uint8_t *)(uintptr_t)G_CUR_CTX;
        if (G_FLAG_67fc == 0u && G_FLAG_1200 == 0u && ctx != NULL &&
            ctx[5] != savedSlot &&
            *(uint16_t *)(void *)(ctx + 0x0a) != 0xffffu) {
            PublishGridCellSpanByDepthIndexToExtentList(
                (int)*(uint16_t *)(void *)(ctx + 0x0a));
        }
    }

    if (G_FLAG_67fc == 0u && G_FLAG_1200 != 0u) {
        PublishCursorSlotSpanAndFlagContextOnOperandMismatch(
            (void *)(uintptr_t)G_CUR_CTX);
    }

    if ((topFlags & 0x10u) == 0u) {
        return sum + 1;
    }

    depth = -1;
    blockHeader = *cursor++;
    blockEnd = cursor + (blockHeader & 0xffffu);

    for (;;) {
        uint32_t kind = blockHeader >> 24;
        switch (kind) {
        case 0u: {
            uint32_t sel = (NW_RNG_HIGH() * ((topFlags >> 8) & 3u)) >> 16;
            uint32_t *p;

            if (sel == 2u) {
                blockEnd = (blockEnd + 1) +
                           (*(uint32_t *)(void *)blockEnd & 0xffffu);
            }
            if (sel == 1u || sel == 2u) {
                p = blockEnd + 1;
                blockEnd = p + (*(uint32_t *)(void *)blockEnd & 0xffffu);
            } else {
                p = cursor;
            }
            NW_RNG_STEP();
            do {
                sum += ((int (__stdcall *)(uint32_t))(uintptr_t)
                        G_CELL_OPERAND_HOOK)(*p);
                p++;
            } while (p != blockEnd);
            return sum + 1;
        }

        case 1u: {
            uint32_t count = ((blockHeader & 0xffffu) * NW_RNG_HIGH()) >> 16;
            NW_RNG_STEP();
            for (;;) {
                if (count != 0u) {
                    sum += ((int (__stdcall *)(uint32_t))(uintptr_t)
                            G_CELL_OPERAND_HOOK)(*cursor);
                }
                cursor++;
                count--;
                if (cursor == blockEnd) {
                    break;
                }
            }
            break;
        }

        case 2u: {
            uint32_t count = ((blockHeader & 0xffffu) * NW_RNG_HIGH()) >> 16;
            NW_RNG_STEP();
            cursor += count;
            sum += ((int (__stdcall *)(uint32_t))(uintptr_t)
                    G_CELL_OPERAND_HOOK)(*cursor);
            break;
        }

        case 3u: {
            for (;;) {
                uint32_t sel = (NW_RNG_HIGH() + NW_RNG_HIGH()) >> 16;
                NW_RNG_STEP();
                if (sel != 0u) {
                    sum += ((int (__stdcall *)(uint32_t))(uintptr_t)
                            G_CELL_OPERAND_HOOK)(*cursor);
                }
                cursor++;
                if (cursor == blockEnd) {
                    break;
                }
            }
            break;
        }

        default: {
            uint32_t lo;
            uint32_t hi;
            uint32_t index;
            uint32_t target;

            depth++;
            lo = lowStack[depth];
            hi = highStack[depth];
            index = (((hi - lo) * NW_RNG_HIGH()) + lo) >> 16;
            target = (uint32_t)G_WORD_TABLE_1208[index] << 16;
            NW_RNG_STEP();
            for (;;) {
                if ((*cursor & 0xffff0000u) == target) {
                    sum += ((int (__stdcall *)(uint32_t))(uintptr_t)
                            G_CELL_OPERAND_HOOK)(*cursor);
                    break;
                }
                cursor++;
                if (cursor == blockEnd) {
                    break;
                }
            }
            break;
        }
        }

        if ((blockHeader & 0x10000u) != 0u) {
            return sum + 1;
        }
        cursor = blockEnd + 1;
        blockHeader = *blockEnd;
        blockEnd = cursor + (blockHeader & 0xffffu);
    }

skip_group:
    if ((blockHeader & 0x10000u) == 0u) {
        do {
            cursor = blockEnd + 1;
            blockHeader = *blockEnd;
            blockEnd = cursor + (blockHeader & 0xffffu);
        } while ((blockHeader & 0x10000u) == 0u);
    }
    cursor = blockEnd;
    goto phase_two;
}

/* Bare-name alias for the linker and for the function map. */
__asm__(
    "\n.globl ExecuteOpcodeStream\n"
    ".set ExecuteOpcodeStream, _ExecuteOpcodeStream\n"
);
#endif /* NW_UNIT_ExecuteOpcodeStream */

/* ==========================================================================
 * Unit: ExecuteOpcodeStreamPreservingGlobals
 * Source: vm/ExecuteOpcodeStreamPreservingGlobals.c
 * ========================================================================== */
#ifdef NW_UNIT_ExecuteOpcodeStreamPreservingGlobals
/*
 * Execute Opcode Stream Preserving Globals.
 *
 * Module: vm
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#vm-executeopcodestreampreservingglobals-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define NWP32(off) (*(uint32_t *)(intptr_t)(nw_data_window + (off)))
#define NWP8(off)  (*(uint8_t  *)(intptr_t)(nw_data_window + (off)))

extern int32_t __stdcall
ExecuteOpcodeStream(uint32_t dwMode, uint32_t dwStream)
    __asm__("_ExecuteOpcodeStream");

NW_DIAG uint32_t __stdcall
ExecuteOpcodeStreamPreservingGlobals(const uint32_t *pOpcodeStream)
{
    const uintptr_t saved6218 = NW_WINDOW_PTR_VALUE(0x6218);
    const uint32_t saved621c = NWP32(0x621c);
    const uint32_t saved6214 = NW_WINDOW_PTR_VALUE(0x6214);
    const uint32_t saved6238 = NWP32(0x6238);
    const uint32_t saved623c = NWP32(0x623c);
    const uint32_t saved6228 = NWP32(0x6228);
    const uint32_t saved622c = NWP32(0x622c);
    const uint32_t saved6220 = NWP32(0x6220);
    const uint32_t saved6224 = NWP32(0x6224);
    const uint8_t  saved67f6 = NWP8(0x67f6);
    const nw_host_ptr_t saved6230 = NW_WINDOW_PTR_VALUE(0x6230);
    const nw_host_ptr_t saved6234 = NW_WINDOW_PTR_VALUE(0x6234);
    const uintptr_t saved63e8 = NW_WINDOW_PTR_VALUE(0x63e8);
    const uint8_t  saved67fc = NWP8(0x67fc);

    const int32_t result = ExecuteOpcodeStream(1u, *pOpcodeStream);

    NWP8(0x67fc)  = saved67fc;
    NW_WINDOW_PTR_VALUE(0x63e8) = saved63e8;
    NW_WINDOW_PTR_VALUE(0x6234) = saved6234;
    NW_WINDOW_PTR_VALUE(0x6230) = saved6230;
    NWP8(0x67f6)  = saved67f6;
    NWP32(0x6224) = saved6224;
    NWP32(0x6220) = saved6220;
    NWP32(0x622c) = saved622c;
    NWP32(0x6228) = saved6228;
    NWP32(0x623c) = saved623c;
    NWP32(0x6238) = saved6238;
    NW_WINDOW_PTR_VALUE(0x6214) = saved6214;
    NWP32(0x621c) = saved621c;
    NW_WINDOW_PTR_VALUE(0x6218) = saved6218;

    return (uint32_t)result; /* EAX survives the restore block unchanged */
}
#endif /* NW_UNIT_ExecuteOpcodeStreamPreservingGlobals */

/* ==========================================================================
 * Unit: ExecuteScriptInstruction
 * Source: vm/ExecuteScriptInstruction.c
 * ========================================================================== */
#ifdef NW_UNIT_ExecuteScriptInstruction
/*
 * Executes one bytecode instruction.
 *
 * Module: vm
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#vm-executescriptinstruction-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <stddef.h>

typedef unsigned char byte;

extern uint8_t __stdcall DispatchScriptOpcode(byte *pCursor);

/* g_vmContextD aliases the NW_CTX_D side-table entry (window slot 0x6214) and
 * g_vmContextA aliases g_pVmBaseB (window slot 0x63e8): both are native host
 * pointers in the portable build, so they must stay pointer-width. */
extern uintptr_t g_vmContextD; /* installed from arg 6 (dwCtxD) */
extern uint32_t g_vmContextC; /* installed from arg 5 (dwCtxC); its OLD value is passed as DispatchScriptOpcode arg2 */
extern uint32_t g_vmContextB; /* installed from arg 4 (dwCtxB) *//* = arg 3 (bOperandB), zero-extended */
/* = arg 2 (bOperandA), zero-extended */

extern uintptr_t g_vmContextA; /* g_pVmBaseB: VM context word DispatchScriptOpcode binds */
extern uint8_t  g_vmModeFlag; /* byte flag, installed from arg 1 (bFlag); its OLD value is passed as DispatchScriptOpcode arg1 */

#define NW_CTX_C NW_WINDOW_PTR_VALUE(0x6218)
#define NW_CTX_D NW_WINDOW_PTR_VALUE(0x6214)

void ExecuteScriptInstruction(byte bFlag, byte bOperandA, byte bOperandB,
                              uint32_t dwCtxA, uint32_t dwCtxB, uint32_t dwCtxC,
                              uint32_t dwCtxD, byte *pCursor)
{
    /* Assembly: CMP [cursor],0 / JZ epilogue — a null cursor skips the save/install/
     * dispatch/restore entirely. */
    if (pCursor == NULL)
        return;

    /* Save all nine persistent state words before disturbing them. */
    uintptr_t savedContextD = NW_CTX_D;
    uintptr_t savedContextC = NW_CTX_C;
    uint32_t savedContextB = g_vmContextB;
    uint32_t savedOperandB = g_vmOperandB;
    uint32_t savedOperandA = g_vmOperandA;
    uint32_t savedOperandBKey = g_vmOperandBKey;
    uint32_t savedOperandAKey = g_vmOperandAKey;
    uintptr_t savedContextA = g_vmContextA;
    uint8_t  savedModeFlag = g_vmModeFlag;

    /* Install the new VM context from the arguments. */
    NW_CTX_D = (uintptr_t)nw_guest_to_host(dwCtxD);
    NW_CTX_C = (uintptr_t)nw_guest_to_host(dwCtxC);
    g_vmContextB = dwCtxB;
    g_vmContextA = (uintptr_t)nw_guest_to_host(dwCtxA);
    g_vmOperandB = bOperandB;   /* zero-extended byte */
    g_vmOperandBKey = bOperandB;
    g_vmOperandA = bOperandA;
    g_vmOperandAKey = bOperandA;
    g_vmModeFlag = bFlag;

    DispatchScriptOpcode(pCursor);

    NW_CTX_D = savedContextD;
    NW_CTX_C = savedContextC;
    g_vmContextB = savedContextB;
    g_vmOperandB = savedOperandB;
    g_vmOperandA = savedOperandA;
    g_vmOperandBKey = savedOperandBKey;
    g_vmOperandAKey = savedOperandAKey;
    g_vmContextA = savedContextA;
    g_vmModeFlag = savedModeFlag;
}
#endif /* NW_UNIT_ExecuteScriptInstruction */

/* ==========================================================================
 * Unit: FetchNextStreamByteIntoSlotDescriptor
 * Source: vm/FetchNextStreamByteIntoSlotDescriptor.c
 * ========================================================================== */
#ifdef NW_UNIT_FetchNextStreamByteIntoSlotDescriptor
/*
 * Fetch Next Stream Byte Into Slot Descriptor.
 *
 * Module: vm
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#vm-fetchnextstreambyteintoslotdescriptor-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

#define SLOT_DESCRIPTOR_BYTE (*(BYTE *)(nw_data_window + 0x16d0))

extern int __stdcall ReadStreamBytesToDestBuffer(DWORD dwRequestedBytes, char *lpDest);

int __cdecl FetchNextStreamByteIntoSlotDescriptor(void)
{
#ifdef NWDEBUG_ENABLED
    extern void nwsafe_trace(const char *);
    extern void nwtrace_hex(const char *, unsigned long);
    nwsafe_trace("fn_in");
#endif

    return ReadStreamBytesToDestBuffer(1, (char *)&SLOT_DESCRIPTOR_BYTE);
}
#endif /* NW_UNIT_FetchNextStreamByteIntoSlotDescriptor */

/* ==========================================================================
 * Unit: InterpretOpcodeStreamUntilEnd
 * Source: vm/InterpretOpcodeStreamUntilEnd.c
 * ========================================================================== */
#ifdef NW_UNIT_InterpretOpcodeStreamUntilEnd
/*
 * Interprets the opcode stream until its end marker.
 *
 * Module: vm
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#vm-interpretopcodestreamuntilend-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define NWP32(off) (*(uint32_t *)(intptr_t)(nw_data_window + (off)))

/* The recovered VM keeps the current and saved record pointers in four-byte
 * guest slots.  On a 64-bit host those slots live in the native pointer table,
 * so all uses must go through the accessors rather than NWP32. */
#define G_VM_RECORD_PTR  NW_WINDOW_PTR_REF(uint8_t, 0x16d4)
#define G_VM_SAVED_PTR   NW_WINDOW_PTR_REF(uint8_t, 0x16d8)
#define G_VM_RECORD_ADDR ((uint32_t)(uintptr_t)G_VM_RECORD_PTR)
#define G_VM_SAVED_ADDR  ((uint32_t)(uintptr_t)G_VM_SAVED_PTR)
#define G_VM_BASE_ADDR   ((uint32_t)(uintptr_t)NW_WINDOW_PTR_REF(uint8_t, 0x63e4))

extern int __stdcall ReadStreamBytesToDestBuffer(uint32_t dwRequestedBytes,
                                                 char *lpDest);
extern int __stdcall SeekStreamLogicalOffset(int nOrigin, int nOffset);
extern int FetchNextStreamByteIntoSlotDescriptor(void);
extern uint32_t __stdcall ReadLzssOrRawTaggedRecordBlock(uint8_t *pbDestBuf);

extern void *__cdecl MoveMemoryRegion(void *pDest, const void *pSrc,
                                      uint32_t dwByteCount);
extern uint32_t __stdcall AllocateHeapCursorBlock(uint32_t dwSizeBytes);
extern uint8_t * __cdecl RebaseInlineImageRecordAndLoadCursor(void);

extern uint8_t __stdcall ExecuteOpcodeAndStoreOperand(uint32_t arg1_operand,
                                                      uint32_t arg2_addend,
                                                      uint32_t arg3_index,
                                                      uint32_t arg4_index,
                                                      uint32_t arg5_slot,
                                                      uint32_t arg6_opcode)
    __asm__("_ExecuteOpcodeAndStoreOperand");

NW_DIAG uint8_t InterpretOpcodeStreamUntilEnd(void);

#if defined(NW_PS5_NATIVE_TITLE)
extern void platform_boot_mark(const char *stage);
extern void platform_log_mark(const char *stage);
#define NW_NATIVE_BOOT(stage) platform_boot_mark(stage)
#define NW_NATIVE_LOG(stage) platform_log_mark(stage)
#else
#define NW_NATIVE_BOOT(stage) ((void)0)
#define NW_NATIVE_LOG(stage) ((void)0)
#endif

NW_DIAG uint8_t InterpretOpcodeStreamUntilEnd(void)
{
    uint32_t readValue;
    uint32_t local1 = 0;              /* window-local opcode argument */
    uint32_t local2 = 0;              /* window-local opcode argument */
    uint32_t ebpValue = 0;            /* fourth opcode argument */
    uint32_t opcode;
    uint32_t nwIter = 0;              /* native diagnostic loop counter */
    uint8_t  exitFlag = 0;            /* 0x00 arm */
    uint8_t  relocateFlag = 1;        /* cleared by the 0x40 literal arm */
    uint8_t *cursor;

    if (!ReadStreamBytesToDestBuffer(4u, (char *)&readValue)) {
        return 0;
    }
    if (!SeekStreamLogicalOffset(1, (int)readValue)) { /* SEEK_CUR */
        return 0;
    }
    if (!FetchNextStreamByteIntoSlotDescriptor()) {
        return 0;
    }
    if (*(uint8_t *)(intptr_t)(nw_data_window + 0x16d0) != 7u) {
        return 0;
    }

    cursor = NW_WINDOW_PTR_REF(uint8_t, 0x16d4);
    if (!ReadLzssOrRawTaggedRecordBlock(cursor)) {
        return 0;
    }
    NW_NATIVE_BOOT("op_stream_data");

    for (;;) {

        relocateFlag = 1;

        opcode = *cursor++;

        nwIter++;
        if (nwIter == 1000000u) {
            if (opcode == 0xffu)
                NW_NATIVE_LOG("op_loop_1m_ff");
            else if (opcode == 0x43u)
                NW_NATIVE_LOG("op_loop_1m_43");
            else
                NW_NATIVE_LOG("op_loop_1m_other");
        } else if (nwIter == 10000000u) {
            if (opcode == 0xffu)
                NW_NATIVE_LOG("op_loop_10m_ff");
            else if (opcode == 0x43u)
                NW_NATIVE_LOG("op_loop_10m_43");
            else
                NW_NATIVE_LOG("op_loop_10m_other");
        } else if (nwIter == 100000000u) {
            NW_NATIVE_LOG("op_loop_limit");
            return 0;
        }

        switch (opcode) {
        case 0x80u:
            ebpValue = 0;
            local1 = 0;
            local2 = *(const uint32_t *)(const void *)cursor;
            cursor += 4;
            goto loop_test;

        case 0x01u:
            ebpValue = *(const uint32_t *)(const void *)cursor;
            cursor += 4;
            goto loop_test;

        case 0x02u:
            local1 = *(const uint32_t *)(const void *)cursor;
            cursor += 4;
            goto loop_test;

        case 0x40u: {
            NWP32(0x16c8) = 4u;
            G_VM_SAVED_PTR = G_VM_RECORD_PTR;
            (void)MoveMemoryRegion((void *)(uintptr_t)nw_guest_to_host(G_VM_RECORD_ADDR),
                                   cursor, 4u);
            cursor += 4;
            relocateFlag = 0;
            break;
        }

        case 0x41u:
        case 0x42u: {
            uint32_t length = *(const uint32_t *)(const void *)cursor;
            cursor += 4;
            NWP32(0x16c8) = length;
            G_VM_SAVED_PTR = G_VM_RECORD_PTR;
            (void)MoveMemoryRegion((void *)(uintptr_t)nw_guest_to_host(G_VM_RECORD_ADDR),
                                   cursor, length);
            cursor += length;
            break;
        }

        case 0x43u: {
            uint8_t *pRecord;
            uint32_t size;

            NWP32(0x16c8) = 0x10u;
            G_VM_SAVED_PTR = G_VM_RECORD_PTR;
            (void)MoveMemoryRegion((void *)(uintptr_t)nw_guest_to_host(G_VM_RECORD_ADDR),
                                   cursor, 0x10u);
            cursor += 0x10;

            pRecord = RebaseInlineImageRecordAndLoadCursor();
            size = (uint32_t)*(const uint16_t *)(const void *)pRecord *
                   (uint32_t)*(const uint16_t *)(const void *)(pRecord + 2);
            if (*(const uint32_t *)(const void *)(pRecord + 0x0c) != 0u) {
                size += 4u;
            }
            if (size > NWP32(0x16ec)) {
                NWP32(0x16ec) = size;
            }
            break;
        }

        case 0xffu:
            if (!ExecuteOpcodeAndStoreOperand(G_VM_SAVED_ADDR, 0u,
                                              local1, ebpValue, local2,
                                              opcode)) {
                return 0;
            }
            goto loop_test;

        case 0x00u:
            exitFlag = 1;
            /* FALLTHROUGH: the shipped 0x00 arm continues into 0x07. */
            __attribute__((fallthrough));
        case 0x07u: {
            uint32_t expected = *(const uint32_t *)(const void *)cursor;
            uint32_t current;

            cursor += 4;
            current = G_VM_RECORD_ADDR - G_VM_BASE_ADDR;
            if (current != expected) {
                return 0;
            }
            goto loop_test;
        }

        default:
            return 0;
        }

        if (!ExecuteOpcodeAndStoreOperand(G_VM_RECORD_ADDR, 0u,
                                          local1, ebpValue, local2,
                                          opcode)) {
            return 0;
        }

        if (relocateFlag != 0u) {
            G_VM_RECORD_PTR = (uint8_t *)(uintptr_t)nw_guest_to_host(
                AllocateHeapCursorBlock(NWP32(0x16c8)));
        }

loop_test:
        if (exitFlag != 0u) {
            return 1;
        }
    }
}

__asm__(".globl InterpretOpcodeStreamUntilEnd\n"
        ".set InterpretOpcodeStreamUntilEnd, "
        "_InterpretOpcodeStreamUntilEnd\n");
#endif /* NW_UNIT_InterpretOpcodeStreamUntilEnd */

/* ==========================================================================
 * Unit: ResetStreamCursorState
 * Source: vm/ResetStreamCursorState.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetStreamCursorState
/*
 * Reset Stream Cursor State.
 *
 * Module: vm
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#vm-resetstreamcursorstate-c
 */

extern unsigned int g_dwStreamBaseBytes;

extern unsigned int g_dwStreamRemainingBytes;

extern unsigned char *g_pStreamSourceBuffer;

extern unsigned int g_dwStreamOriginValue;

void __stdcall ResetStreamCursorState(unsigned char *pStreamSource)
{
#ifdef NWDEBUG_ENABLED
extern void nwsafe_trace(const char *);
    nwsafe_trace("rscs_in");
#endif

    g_dwStreamBaseBytes = 0u;
    g_dwStreamRemainingBytes = 0u;
    g_pStreamSourceBuffer = pStreamSource;

    if (pStreamSource != 0) {
        g_dwStreamBaseBytes = g_dwStreamOriginValue;   /* seed base counter from origin value */
    }
}
#endif /* NW_UNIT_ResetStreamCursorState */
