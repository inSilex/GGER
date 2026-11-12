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
#include <stdio.h>
#include <stdlib.h>
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

/* ==========================================================================
 * Layout diagnostics (opt-in with NW_LAYOUT_LOG).
 *
 * The high-window layout was designed with the aid of layout and allocation
 * traces.  They are quiet by default now that the model is validated; set
 * NW_LAYOUT_LOG=1 to append them to nw_layout.log and stderr when triaging a
 * new target.  Crash reporting is separate and unaffected.
 */

static int nw_inst_enabled(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("NW_LAYOUT_LOG");
        cached = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
    }
    return cached;
}

static int nw_force_high(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("NW_FORCE_HIGH");
        int image_high = (uintptr_t)nw_data_window >= UINT64_C(0x100000000);
        /* A high image (iOS, or a high-window link) already places every
         * static guest object above 4 GiB, so allocations must follow it. */
        cached = image_high ||
                 (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
    }
    return cached;
}

static FILE *nw_inst_fp(void)
{
    static FILE *fp;
    if (fp == NULL) {
        fp = fopen("nw_layout.log", "a");
        if (fp != NULL)
            setvbuf(fp, NULL, _IONBF, 0); /* unbuffered: survive a crash */
    }
    return fp;
}

static void nw_inst_line(const char *line)
{
    FILE *fp;
    if (!nw_inst_enabled())
        return;
    fputs(line, stderr);
    fp = nw_inst_fp();
    if (fp != NULL) {
        fputs(line, fp);
        fflush(fp);
    }
}

void NwDumpLayout(const char *tag)
{
    uintptr_t dw_base   = (uintptr_t)nw_data_window;
    uintptr_t sl_base   = (uintptr_t)g_nw_window_ptr_slots;
    nw_host_ptr_t heap_base = NW_WINDOW_PTR_VALUE(0x2c18u); /* g_pHeapSlotBase */
    uint32_t cursor_src;
    char line[160];

    memcpy(&cursor_src, nw_data_window + 0x6318u, sizeof cursor_src);

    snprintf(line, sizeof line, "[layout:%s] data_window=0x%llx %s base=0x%llx\n", tag,
             (unsigned long long)dw_base,
             dw_base < UINT64_C(0x100000000) ? "LOW" : "HIGH",
             (unsigned long long)nw_guest_base);
    nw_inst_line(line);
    snprintf(line, sizeof line, "[layout:%s] side_table =0x%llx %s\n", tag,
             (unsigned long long)sl_base,
             sl_base < UINT64_C(0x100000000) ? "LOW" : "HIGH");
    nw_inst_line(line);
    snprintf(line, sizeof line, "[layout:%s] heap_slot  =0x%llx %s (32bit=0x%llx)\n", tag,
             (unsigned long long)heap_base,
             heap_base < UINT64_C(0x100000000) ? "LOW" : "HIGH",
             (unsigned long long)(uint32_t)heap_base);
    nw_inst_line(line);
    snprintf(line, sizeof line, "[layout:%s] cursor_src =0x%x\n", tag,
             (unsigned int)cursor_src);
    nw_inst_line(line);
}

typedef PVOID (WINAPI *NwVirtualAlloc2Fn)(HANDLE, PVOID, SIZE_T, ULONG,
                                          ULONG, MEM_EXTENDED_PARAMETER *,
                                          ULONG);

static NwVirtualAlloc2Fn nw_va2_probe(void)
{
    static NwVirtualAlloc2Fn pVirtualAlloc2;
    static int probed;

    if (!probed) {
        HMODULE kernel = GetModuleHandleA("kernel32.dll");
        probed = 1;
        if (kernel != NULL)
            pVirtualAlloc2 = (NwVirtualAlloc2Fn)(void *)
                GetProcAddress(kernel, "VirtualAlloc2");
        nw_inst_line(pVirtualAlloc2 ? "[probe] VirtualAlloc2 found\n"
                                     : "[probe] VirtualAlloc2 MISSING\n");
    }
    return pVirtualAlloc2;
}

/* Scan for a free range inside the single 4 GiB guest window.  Jumps over
 * reserved regions with VirtualQuery so the walk stays short even on sparse
 * address spaces.  Every result is verified to actually be in the window.
 * Guest address 0 stays invalid: the first 64 KiB of the window is never
 * handed out. */
static void *nw_scan_high(size_t size, DWORD protect)
{
    uintptr_t window = nw_guest_base != 0 ? nw_guest_base
                                          : UINT64_C(0x100000000);
    uintptr_t limit  = window + UINT64_C(0x100000000); /* one 4 GiB window */
    uintptr_t addr   = window + UINT64_C(0x10000);     /* skip guest 0..0xffff */
    int attempts;

    for (attempts = 0; ; attempts++) {
        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t next;
        void *p;
        char line[160];

        if (addr >= limit)
            break;
        p = VirtualAlloc((void *)addr, size, MEM_COMMIT | MEM_RESERVE, protect);
        snprintf(line, sizeof line, "[scan] try=0x%llx got=%p %s\n",
                 (unsigned long long)addr, (void *)(uintptr_t)p,
                 p != NULL && (uintptr_t)p >= window &&
                 (uintptr_t)p + size <= limit ? "ok" : "bad");
        nw_inst_line(line);
        if (p != NULL && (uintptr_t)p >= window &&
            (uintptr_t)p + size <= limit)
            return p;
        if (attempts + 1 >= 64) {
            snprintf(line, sizeof line, "[scan] gave up after %d attempts\n", attempts);
            nw_inst_line(line);
            break;
        }
        if (VirtualQuery((void *)addr, &mbi, sizeof mbi) == 0)
            break;
        /* 64-bit mask: ~0xFFFFu alone would truncate to 32 bits. */
        next = ((uintptr_t)mbi.BaseAddress + mbi.RegionSize + 0xFFFFu) &
               ~(uintptr_t)0xFFFFu;
        if (next <= addr || next < window + UINT64_C(0x10000))
            next = addr + UINT64_C(0x10000);
        addr = next;
    }
    return NULL;
}

/* Allocate at >= 4 GiB to simulate iOS (no sub-4GiB memory available). */
static void *nw_alloc_high(const char *tag, size_t size, DWORD protect)
{
    NwVirtualAlloc2Fn va2 = nw_va2_probe();
    char line[160];
    void *p;

    if (va2 != NULL) {
        uintptr_t window = nw_guest_base != 0 ? nw_guest_base
                                              : UINT64_C(0x100000000);
        MEM_ADDRESS_REQUIREMENTS req;
        MEM_EXTENDED_PARAMETER param;
        req.LowestStartingAddress = (PVOID)(window + UINT64_C(0x10000));
        req.HighestEndingAddress = (PVOID)(window + UINT64_C(0x100000000));
        req.Alignment = 0;
        memset(&param, 0, sizeof param);
        param.Type = MemExtendedParameterAddressRequirements;
        param.Pointer = &req;
        p = va2(GetCurrentProcess(), NULL, size, MEM_COMMIT | MEM_RESERVE,
                protect, &param, 1);
        if (p != NULL)
            return p;
        snprintf(line, sizeof line, "[high:%s] VirtualAlloc2 failed err=%lu\n",
                 tag, (unsigned long)GetLastError());
        nw_inst_line(line);
    } else {
        snprintf(line, sizeof line, "[high:%s] no VirtualAlloc2; scanning\n", tag);
        nw_inst_line(line);
    }

    p = nw_scan_high(size, protect);
    if (p == NULL) {
        snprintf(line, sizeof line, "[high:%s] scan exhausted err=%lu\n",
                 tag, (unsigned long)GetLastError());
        nw_inst_line(line);
    }
    return p;
}

/* Main guest arena (scratch / heap base).  Forced high under NW_FORCE_HIGH. */
void *NwGuestArenaAlloc(size_t size)
{
    void *p;
    char line[128];

    if (nw_force_high()) {
        p = nw_alloc_high("main-arena", size, PAGE_READWRITE);
        snprintf(line, sizeof line, "[alloc] main-arena forced-high size=0x%llx addr=0x%llx\n",
                 (unsigned long long)size, (unsigned long long)(uintptr_t)p);
    } else {
        p = malloc(size);
        snprintf(line, sizeof line, "[alloc] main-arena malloc      size=0x%llx addr=0x%llx\n",
                 (unsigned long long)size, (unsigned long long)(uintptr_t)p);
    }
    nw_inst_line(line);
    return p;
}

void *NwVirtualAllocLow(size_t size, DWORD protect)
{
    char line[128];
    void *p;

    if (nw_force_high()) {
        p = nw_alloc_high("low", size, protect);
        snprintf(line, sizeof line, "[alloc] NwVirtualAllocLow forced-high size=0x%llx addr=0x%llx\n",
                 (unsigned long long)size, (unsigned long long)(uintptr_t)p);
        nw_inst_line(line);
        return p;
    }

    {
        NwVirtualAlloc2Fn va2 = nw_va2_probe();
        if (va2 != NULL) {
            MEM_ADDRESS_REQUIREMENTS req;
            MEM_EXTENDED_PARAMETER param;
            req.LowestStartingAddress = (PVOID)(uintptr_t)0x10000u;
            req.HighestEndingAddress = (PVOID)(uintptr_t)0xffffffffu;
            req.Alignment = 0;
            memset(&param, 0, sizeof param);
            param.Type = MemExtendedParameterAddressRequirements;
            param.Pointer = &req;
            p = va2(GetCurrentProcess(), NULL, size, MEM_COMMIT | MEM_RESERVE,
                    protect, &param, 1);
        } else {
            /* Windows versions without VirtualAlloc2: reserve a low range. */
            uintptr_t addr;
            p = NULL;
            for (addr = 0x10000000u; addr < 0x80000000u; addr += 0x10000u) {
                void *q = VirtualAlloc((void *)addr, size, MEM_COMMIT | MEM_RESERVE,
                                       protect);
                if (q != NULL) { p = q; break; }
            }
        }
    }

    snprintf(line, sizeof line, "[alloc] NwVirtualAllocLow low         size=0x%llx addr=0x%llx\n",
             (unsigned long long)size, (unsigned long long)(uintptr_t)p);
    nw_inst_line(line);
    return p;
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
