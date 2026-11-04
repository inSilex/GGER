#include "nwdebug.h"
/*
 * Diagnostics -- opt-in VM and low-level tag tracing.
 *
 * Module: diag/win32
 * Interface: include/Diagnostics.h
 *
 * Units:
 *   NwVmTrace                                                  diag/win32/NwVmTrace.c
 *   nwtrace                                                    diag/win32/nwtrace.c
 */

#ifdef NW_UNIT_NwVmTrace
/*
 * NwVmTrace -- opt-in VM opcode/record trace.
 *
 * Set NW_DEBUG_VM_TRACE=1 for netwalk_vm.txt, or name a path explicitly.
 * Every caller is guarded by NW_DEBUG_TOOLS, so release builds never enter
 * this path; the trace itself is inert until the variable is set.
 */

#include <windows.h>
#include <stdint.h>
#include "nwarch.h"
#include "nwdebug.h"

#define NW_TRACE2_SECTION __attribute__((section(".nwdiag")))

static char *nw_vm_put_hex(char *o, uint32_t v)
{
    static const char digits[] = "0123456789abcdef";
    int sh;
    for (sh = 28; sh >= 0; sh -= 4)
        *o++ = digits[(v >> sh) & 0xFu];
    return o;
}

static NW_TRACE2_SECTION uint32_t nw_vm_record_hash(void)
{
    uint32_t tmp[4];
    uint32_t hash = 2166136261u;
    /* window+0x16d4 is g_pStreamRecordCursor, a pointer slot.  On 64-bit
     * portable targets the original 32-bit word is 0 and the live pointer
     * lives in the native pointer side table, so read it through the same
     * accessor the engine uses instead of dereferencing the guest word. */
    const uint32_t *p =
        (const uint32_t *)(uintptr_t)NW_WINDOW_PTR_VALUE(0x16d4);
    uintptr_t pa = (uintptr_t)p;
    int i;

    if (pa <= 0x10000u || pa >= 0x80000000u)
        return 0u;

    for (i = 0; i < 4; i++) tmp[i] = p[i];
    if (tmp[1] == 0x4469736bu) {
        uint32_t dbase = *(const uint32_t *)(nw_data_window + 0x16bc);
        if (dbase > 0x12800u) tmp[2] -= dbase - 0x12800u;
    }
    for (i = 0; i < 16; i++) {
        hash ^= ((const uint8_t *)tmp)[i];
        hash *= 16777619u;
    }
    return hash;
}

__attribute__((section(".nwleaf"))) void NwVmTrace(uint32_t opcode, uint32_t slot, uint32_t value)
{
    static HANDLE   h;
    static uint32_t seq;
    static int      gate;          /* 0 = not probed, 1 = tracing, 2 = off */
    char  line[64];
    char *o;
    DWORD wr;

    (void)value;                   /* may be a heap pointer: not reproducible */
    if (gate == 0) {
        const char *e = NW_DEBUG_GETENV("NW_DEBUG_VM_TRACE");
        gate = (e != NULL && *e != 0) ? 1 : 2;
        if (gate == 1) {
            const char *path = (e[0] == '1' && e[1] == 0) ? "netwalk_vm.txt" : e;
            h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) {
                gate = 2;
            } else {
                static const char hdr[] =
                    "# netwalk VM trace v1: opcode slot record_fnv1a seq\n";
                WriteFile(h, hdr, (DWORD)(sizeof hdr - 1u), &wr, NULL);
            }
        }
    }
    if (gate != 1) return;

    {
        uint32_t hash = nw_vm_record_hash();

        o = line;
        o = nw_vm_put_hex(o, opcode); *o++ = ' ';
        o = nw_vm_put_hex(o, slot);   *o++ = ' ';
        o = nw_vm_put_hex(o, hash);   *o++ = ' ';
        o = nw_vm_put_hex(o, seq++);  *o++ = '\n';
        WriteFile(h, line, (DWORD)(o - line), &wr, NULL);
    }
}

#endif /* NW_UNIT_NwVmTrace */

/* ==========================================================================
 * Unit: nwtrace
 * Source: diag/win32/nwtrace.c
 * ========================================================================== */
#ifdef NW_UNIT_nwtrace
/*
 * nwtrace -- low-level tag trace, enabled only by NW_DEBUG_TRACE.
 *
 * Module: diag/win32
 */

#include <stdint.h>
#include "nwdebug.h"
#include <windows.h>
#include <stdlib.h>

/* The trace is opt-in: set NW_DEBUG_TRACE to any non-empty value to enable
 * it.  Without it every entry point returns immediately, so normal runs produce
 * no netwalk_debug.log.  The structured VM trace is NwVmTrace(), selected by
 * NW_DEBUG_VM_TRACE. */
static int nw_trace_gate;
static int nw_trace_enabled(void)
{
    if (nw_trace_gate == 0) {
        const char *e = NW_DEBUG_GETENV("NW_DEBUG_TRACE");
        nw_trace_gate = (e != NULL && *e != 0) ? 1 : 2;
    }
    return nw_trace_gate == 1;
}

/* Keep the trace symbols available in every build.  The NWDEBUG_ENABLED
 * guards at the call sites decide whether a trace is emitted; in release the
 * environment lookup is a null constant, so these functions return immediately.
 * Declared here because nwtrace_c calls it. */
void nwsafe_trace(const char *tag);

void nwtrace_c(const char *tag) __asm__("nwtrace_c");
void nwtrace_c(const char *tag)
{
    if (!nw_trace_enabled()) return;
    /* Emit the tag directly; tracing deliberately avoids formatted output so
     * it stays bounded and safe in low-level code. */
    nwsafe_trace(tag);
}

__asm__(
    ".globl _nwtrace_c\n"
    "\t.set _nwtrace_c, nwtrace_c\n"
);

/* Direct tag emitter used by the trace helpers. */

void nwsafe_trace(const char *tag);
__attribute__((used)) void nwsafe_trace(const char *tag)
{
    static HANDLE h = INVALID_HANDLE_VALUE;
    DWORD cb = 0, wrote = 0;

    if (!nw_trace_enabled()) return;

    if (h == INVALID_HANDLE_VALUE) {
        h = CreateFileA("netwalk_debug.log", FILE_APPEND_DATA,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return;
    }
    while (tag[cb] && cb < 0x100) cb++;

    if (!WriteFile(h, tag, cb, &wrote, NULL)) {
        CloseHandle(h);
        h = INVALID_HANDLE_VALUE;
    }
}

void nwtrace_asm(const char *tag)
{
    nwtrace_c(tag);
}

/* The C half takes the tag and emits it through nwtrace_c.  The aliases below
 * keep the historical underscore-prefixed symbol names available. */

void nwtrace_hex(const char *tag, unsigned long v)
{
    char buf[128];
    if (!nw_trace_enabled()) return;
    unsigned long i = 0;
    const char *q = tag;
    while (*q && i < sizeof buf - 24) buf[i++] = *q++;
    buf[i++] = ' ';
    for (int sh = 28; sh >= 0; sh -= 4) {
        unsigned d = (unsigned)((v >> sh) & 0xF);
        buf[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
    }
    buf[i] = 0;
    nwtrace_c(buf);
}

/* The COFF naming asymmetry once more: a C definition is exported with one
 * leading underscore, while a GAS operand is passed through directly.  The
 * .set alias adds a symbol-table entry and zero bytes. */
__asm__(".text\n"
        ".globl nwtrace_hex\n"
        ".set nwtrace_hex, _nwtrace_hex\n");

__asm__(".globl nwtrace_asm\n"
        ".set nwtrace_asm, _nwtrace_asm\n");

#endif /* NW_UNIT_nwtrace */
