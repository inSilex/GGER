/*
 * Windows x64 compatibility support for the portable guest layout.
 *
 * The recovered engine stores guest pointers in 32-bit words.  On a 64-bit
 * Windows process those addresses must stay below 4 GiB, and native pointers
 * for data-window slots live in the same side table used by the ELF target.
 */

#include "nwarch.h"

#include <windows.h>
#include <stdint.h>
#include <string.h>

nw_host_ptr_t g_nw_window_ptr_slots[NW_WINDOW_BYTES / 4u];

void NwWin64Init(void)
{
    unsigned i;

    for (i = 0; i < NW_WINDOW_BYTES / 4u; i++) {
        uint32_t value;
        memcpy(&value, nw_data_window + i * 4u, sizeof value);
        g_nw_window_ptr_slots[i] = (nw_host_ptr_t)(uintptr_t)value;
    }
}

typedef PVOID (WINAPI *NwVirtualAlloc2Fn)(HANDLE, PVOID, SIZE_T, ULONG,
                                          ULONG, MEM_EXTENDED_PARAMETER *,
                                          ULONG);

void *NwVirtualAllocLow(size_t size, DWORD protect)
{
    static NwVirtualAlloc2Fn pVirtualAlloc2;
    static int probed;

    if (!probed) {
        HMODULE kernel = GetModuleHandleA("kernel32.dll");
        probed = 1;
        if (kernel != NULL)
            pVirtualAlloc2 = (NwVirtualAlloc2Fn)(void *)
                GetProcAddress(kernel, "VirtualAlloc2");
    }
    if (pVirtualAlloc2 != NULL) {
        MEM_ADDRESS_REQUIREMENTS req;
        MEM_EXTENDED_PARAMETER param;
        req.LowestStartingAddress = (PVOID)(uintptr_t)0x10000u;
        req.HighestEndingAddress = (PVOID)(uintptr_t)0xffffffffu;
        req.Alignment = 0;
        memset(&param, 0, sizeof param);
        param.Type = MemExtendedParameterAddressRequirements;
        param.Pointer = &req;
        return pVirtualAlloc2(GetCurrentProcess(), NULL, size,
                              MEM_COMMIT | MEM_RESERVE, protect, &param, 1);
    }

    /* Windows versions without VirtualAlloc2: reserve a low range explicitly. */
    {
        uintptr_t addr;
        for (addr = 0x10000000u; addr < 0x80000000u; addr += 0x10000u) {
            void *p = VirtualAlloc((void *)addr, size,
                                   MEM_COMMIT | MEM_RESERVE, protect);
            if (p != NULL)
                return p;
        }
    }
    return NULL;
}

/* Diagnostic counters and the legacy UT notify table.  The SDL audio backend
 * publishes its real handlers from AudioPipeline.c; these placeholders keep
 * the recovered driver-initialization path link-complete. */
volatile unsigned int g_nwCsrIn;
volatile unsigned int g_nwCsrOut;
volatile unsigned int g_nwCsrIter;

DWORD (__cdecl *g_pfStandardStreamRedirect)(DWORD, const void *, DWORD);

void UtNotifyCode0x202(void) {}
void UtNotifyCode0x203(void) {}
void UtNotifyCode0x204(void) {}
void UtNotifyCode0x205(void) {}
void UtNotifyCode0x206(void) {}
void UtNotifyCode0x207(void) {}
void UtNotifyCode0x209(void) {}
void UtNotifyCode0x20a(void) {}
void UtNotifyCopyArgToBuffer0x208(void) {}
void UtNotifySendArgCode0x208(void) {}
void UtNotifySendArgPlus100(void) {}
