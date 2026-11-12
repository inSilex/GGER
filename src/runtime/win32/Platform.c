#include "nwarch.h"
/*
 * Platform -- Win32 thread, semaphore, SEH, and tracked-allocation support.
 *
 * Module: runtime/win32
 * Interface: include/Platform.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   AllocateTrackedBlock                                       runtime/win32/AllocateTrackedBlock.c
 *   ApiSlotPointers                                            runtime/win32/ApiSlotPointers.c
 *   CreateSlotSemaphore                                        runtime/win32/CreateSlotSemaphore.c
 *   DispatchPendingSignalHandler                               runtime/win32/DispatchPendingSignalHandler.c
 *   FindCurrentThreadContext                                   runtime/win32/FindCurrentThreadContext.c
 *   NwEntrySehShim                                             runtime/win32/NwEntrySehShim.c
 *   TranslateLastErrorToErrno                                  runtime/win32/TranslateLastErrorToErrno.c
 */

/* ==========================================================================
 * Unit: AllocateTrackedBlock
 * Source: runtime/win32/AllocateTrackedBlock.c
 * ========================================================================== */
#if defined(_WIN64)
#  define NW_CTX_IP(c) ((uintptr_t)(c)->Rip)
#  define NW_CTX_SP(c) ((uintptr_t)(c)->Rsp)
#  define NW_CTX_BP(c) ((uintptr_t)(c)->Rbp)
#  define NW_CTX_AX(c) ((uintptr_t)(c)->Rax)
#  define NW_CTX_BX(c) ((uintptr_t)(c)->Rbx)
#  define NW_CTX_CX(c) ((uintptr_t)(c)->Rcx)
#  define NW_CTX_DX(c) ((uintptr_t)(c)->Rdx)
#  define NW_CTX_SI(c) ((uintptr_t)(c)->Rsi)
#  define NW_CTX_DI(c) ((uintptr_t)(c)->Rdi)
#else
#  define NW_CTX_IP(c) ((uintptr_t)(c)->Eip)
#  define NW_CTX_SP(c) ((uintptr_t)(c)->Esp)
#  define NW_CTX_BP(c) ((uintptr_t)(c)->Ebp)
#  define NW_CTX_AX(c) ((uintptr_t)(c)->Eax)
#  define NW_CTX_BX(c) ((uintptr_t)(c)->Ebx)
#  define NW_CTX_CX(c) ((uintptr_t)(c)->Ecx)
#  define NW_CTX_DX(c) ((uintptr_t)(c)->Edx)
#  define NW_CTX_SI(c) ((uintptr_t)(c)->Esi)
#  define NW_CTX_DI(c) ((uintptr_t)(c)->Edi)
#endif

#ifdef NW_UNIT_AllocateTrackedBlock
/*
 * Allocate a 64 KiB-aligned-size VirtualAlloc block and append it to the process-lifetime tracking registry
 *
 * Module: runtime/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-win32-allocatetrackedblock-c
 */

#include <windows.h>
#include <stdint.h>
#include "nwglobals.h"

typedef struct NwTrackedBlockTable {
    struct NwTrackedBlockTable *pPrevious;
    uint32_t dwCount;
    void *apBlocks[1024];
} NwTrackedBlockTable;

#define g_pTrackedBlockHead \
    NW_WINDOW_PTR_REF(NwTrackedBlockTable, 0x1194)

#if defined(_WIN64)
extern void *NwVirtualAllocLow(size_t size, DWORD protect);
#define NW_ALLOC_LOW(size, protect) NwVirtualAllocLow((size), (protect))
#else
#define NW_ALLOC_LOW(size, protect) VirtualAlloc(NULL, (size), MEM_COMMIT, (protect))
#endif

void *AllocateTrackedBlock(uint32_t requested_size)
{
    uint32_t rounded_size = (requested_size + 0xffffu) & 0xffff0000u;
    void *block = NW_ALLOC_LOW(rounded_size, PAGE_READWRITE);
    NwTrackedBlockTable *table;

    if (block == NULL)
        return (void *)(uintptr_t)0xffffffffu;

    table = g_pTrackedBlockHead;
    if (table == NULL || table->dwCount >= 1024u) {
        NwTrackedBlockTable *new_table =
            (NwTrackedBlockTable *)NW_ALLOC_LOW(0x1008u, PAGE_READWRITE);

        if (new_table == NULL) {
            VirtualFree(block, 0, MEM_RELEASE);
            return (void *)(uintptr_t)0xffffffffu;
        }

        new_table->pPrevious = table;
        g_pTrackedBlockHead = new_table;
        new_table->dwCount = 0;
        table = new_table;
    }

    table->apBlocks[table->dwCount++] = block;
    return block;
}
#endif /* NW_UNIT_AllocateTrackedBlock */

/* ==========================================================================
 * Unit: ApiSlotPointers
 * Source: runtime/win32/ApiSlotPointers.c
 * ========================================================================== */
#ifdef NW_UNIT_ApiSlotPointers
/*
 * Win32 API function-pointer slot table.
 *
 * Module: runtime/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-win32-apislotpointers-c
 */

#include <windows.h>

 DWORD   (WINAPI *pfnGetVersion)(void)                            = GetVersion;
 int     (WINAPI *pfnGetDeviceCaps)(HDC, int)                     = GetDeviceCaps;
 BOOL    (WINAPI *pfnInvalidateRect)(HWND, const RECT *, BOOL)    = InvalidateRect;
 BOOL    (WINAPI *pfnGetMessageA)(LPMSG, HWND, UINT, UINT)        = GetMessageA;
 BOOL    (WINAPI *pfnDestroyWindow)(HWND)                         = DestroyWindow;
 BOOL    (WINAPI *pfnPostMessageA)(HWND, UINT, WPARAM, LPARAM)    = PostMessageA;
 BOOL    (WINAPI *pfnShowWindow)(HWND, int)                       = ShowWindow;
 BOOL    (WINAPI *pfnSetCursorPos)(int, int)                      = SetCursorPos;
 BOOL    (WINAPI *pfnTranslateMessage)(const MSG *)               = TranslateMessage;
 LRESULT (WINAPI *pfnDispatchMessageA)(const MSG *)               = DispatchMessageA;
 BOOL    (WINAPI *pfnGetCursorPos)(LPPOINT)                       = GetCursorPos;
/* User32:UpdateWindow */ BOOL    (WINAPI *pfnUpdateWindow)(HWND)                 = UpdateWindow;
/* User32:SendMessageA */ LRESULT (WINAPI *pfnSendMessageA)(HWND, UINT, WPARAM, LPARAM) = SendMessageA;
#endif /* NW_UNIT_ApiSlotPointers */

/* ==========================================================================
 * Unit: CreateSlotSemaphore
 * Source: runtime/win32/CreateSlotSemaphore.c
 * ========================================================================== */
#ifdef NW_UNIT_CreateSlotSemaphore
/*
 * Create Slot Semaphore.
 *
 * Module: runtime/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-win32-createslotsemaphore-c
 */

#include <stdint.h>
#include "nwglobals.h"

#ifdef NWDEBUG_ENABLED
void nwtrace_hex(const char *, unsigned long);
#ifdef NWDEBUG_ENABLED
void nwtrace_asm(const char *);
#endif
#endif
#include <windows.h>

#define g_pawSlotSemaphoreHandles ((uint32_t *)(nw_data_window + 0x66d4))   

#define g_pszSlotSemNamePrefix ((char *)(nw_data_window + 0x6809))   

char * __cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc);

char * __cdecl AppendCStringToDest(char *pDestBuf, const char *pszSrc);

char * ConvertIntegerToString(int iValue, char *pDest, unsigned uBase);

void CreateSlotSemaphore(uint32_t uiSlotIndex)
{
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("apb_cs_enter", 0);
#endif

    char szSlotSemName[28]; 
    char szIndexHexBuf[20]; 

    CopyNulTerminatedString(szSlotSemName, g_pszSlotSemNamePrefix);
    AppendCStringToDest(
        szSlotSemName,
        ConvertIntegerToString((int)uiSlotIndex, szIndexHexBuf, 16u));

    /* Lazy-create: retry while the table slot is still empty. */
    if (g_pawSlotSemaphoreHandles[uiSlotIndex] == 0u) {
        HANDLE hCreated;
        do {
            hCreated = CreateSemaphoreA(NULL, 0, 1, szSlotSemName);
            if (hCreated != NULL) {
                g_pawSlotSemaphoreHandles[uiSlotIndex] = (uint32_t)(uintptr_t)hCreated;
            }
        } while (g_pawSlotSemaphoreHandles[uiSlotIndex] == 0u);
    }

    /* Block until slot uiSlotIndex's semaphore is released. */
    HANDLE hSlotSem = (HANDLE)(uintptr_t)g_pawSlotSemaphoreHandles[uiSlotIndex];
#ifdef NWDEBUG_ENABLED

    (void)WaitForSingleObject(hSlotSem, 200);
#else
    (void)WaitForSingleObject(hSlotSem, INFINITE);
#endif
#ifdef NWDEBUG_ENABLED
    nwtrace_asm("slotsem after_wait");
#endif
}
#endif /* NW_UNIT_CreateSlotSemaphore */

/* ==========================================================================
 * Unit: DispatchPendingSignalHandler
 * Source: runtime/win32/DispatchPendingSignalHandler.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchPendingSignalHandler
/*
 * Dispatches pending Win32 exception handlers and falls back to UnhandledExceptionFilter.
 *
 * Module: runtime/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-win32-dispatchpendingsignalhandler-c
 */

#include <windows.h>
#include <stdint.h>

extern unsigned char nw_data_window[];

/* One-shot handler slots in the fixed data window. The FP slot is cleared
 * after it fires; the access and illegal-instruction slots are replaced with
 * the current secondary code. */
#define NW_FP_HANDLER_SLOT \
    (*(uintptr_t *)(void *)(nw_data_window + 0x0f90))
#define NW_ILLEGAL_HANDLER_SLOT \
    (*(uintptr_t *)(void *)(nw_data_window + 0x0f94))
#define NW_ACCESS_HANDLER_SLOT \
    (*(uintptr_t *)(void *)(nw_data_window + 0x0f98))

typedef void (__cdecl *NwSignalHandler)(int);
typedef void (__cdecl *NwSignalHandlerWithCode)(int, int);

LONG __cdecl DispatchPendingSignalHandler(EXCEPTION_POINTERS *pExceptionInfo)
    __asm__("_DispatchPendingSignalHandler");

LONG __cdecl DispatchPendingSignalHandler(EXCEPTION_POINTERS *pExceptionInfo)
{
    uint32_t dwExceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
    uintptr_t pfnHandler = 0;
    int nSignal = 0;
    int nCode = 0;

    switch (dwExceptionCode) {
    case 0xc000008du:
        nCode = 0x82;
        nSignal = 8;
        pfnHandler = NW_FP_HANDLER_SLOT;
        NW_FP_HANDLER_SLOT = 0;
        break;
    case 0xc000008eu:
        nCode = 0x83;
        nSignal = 8;
        pfnHandler = NW_FP_HANDLER_SLOT;
        NW_FP_HANDLER_SLOT = 0;
        break;
    case 0xc000008fu:
        nCode = 0x86;
        nSignal = 8;
        pfnHandler = NW_FP_HANDLER_SLOT;
        NW_FP_HANDLER_SLOT = 0;
        break;
    case 0xc0000090u:
        nCode = 0x81;
        nSignal = 8;
        pfnHandler = NW_FP_HANDLER_SLOT;
        NW_FP_HANDLER_SLOT = 0;
        break;
    case 0xc0000091u:
        nCode = 0x84;
        nSignal = 8;
        pfnHandler = NW_FP_HANDLER_SLOT;
        NW_FP_HANDLER_SLOT = 0;
        break;
    case 0xc0000092u:
        nCode = 0x8a;
        nSignal = 8;
        pfnHandler = NW_FP_HANDLER_SLOT;
        NW_FP_HANDLER_SLOT = 0;
        break;
    case 0xc0000093u:
        nCode = 0x85;
        nSignal = 8;
        pfnHandler = NW_FP_HANDLER_SLOT;
        NW_FP_HANDLER_SLOT = 0;
        break;
    case 0xc0000005u:
        nSignal = 0xb;
        pfnHandler = NW_ACCESS_HANDLER_SLOT;
        NW_ACCESS_HANDLER_SLOT = 0;
        break;
    case 0xc000001du:
    case 0xc0000096u:
        nSignal = 4;
        pfnHandler = NW_ILLEGAL_HANDLER_SLOT;
        NW_ILLEGAL_HANDLER_SLOT = 0;
        break;
    default:
        break;
    }

    if (pfnHandler != 0) {
        if (nSignal == 8) {
            ((NwSignalHandlerWithCode)pfnHandler)(nSignal, nCode);
        } else {
            ((NwSignalHandler)pfnHandler)(nSignal);
        }
        return -1;
    }

    return UnhandledExceptionFilter(pExceptionInfo);
}

/* The original entry published both the bare and C-spelling symbols. */
__asm__(".globl DispatchPendingSignalHandler\n"
        ".set DispatchPendingSignalHandler, _DispatchPendingSignalHandler\n");
#endif /* NW_UNIT_DispatchPendingSignalHandler */

/* ==========================================================================
 * Unit: FindCurrentThreadContext
 * Source: runtime/win32/FindCurrentThreadContext.c
 * ========================================================================== */
#ifdef NW_UNIT_FindCurrentThreadContext
/*
 * Find Current Thread Context.
 *
 * Module: runtime/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-win32-findcurrentthreadcontext-c
 */

#include <windows.h>
#include <stdint.h>
#include "nwarch.h"
extern unsigned char nw_data_window[] __asm__("nw_data_window");
#ifdef NWDEBUG_ENABLED
void nwtrace_hex(const char *, unsigned long);
#endif

void *FindCurrentThreadContext(DWORD dwUnusedParam1 __attribute__((unused)),
                         DWORD dwUnusedParam2 __attribute__((unused)))
{
    DWORD  dwThreadId;
    int    iSlot;
    int    iSlotLeft;
    uint32_t *pTable;                  /* guest 32-bit pointer slots */
    void  *pContext;

    dwThreadId = GetCurrentThreadId();
    iSlotLeft  = 0x400;                  /* 1024 slots */

#ifdef NWDEBUG_ENABLED
    nwtrace_hex("fctc_enter", (unsigned long)(void *)(nw_data_window + 0xE84));
#endif

    pTable = (uint32_t *)NW_WINDOW_PTR_REF(void, 0xE84);
#ifdef NWDEBUG_ENABLED
    {
        unsigned char *pb = (unsigned char *)pTable;
        int k;
        nwtrace_hex("fctc_base", (unsigned long)pTable);
        if (pb != NULL)
            for (k = 0; k < 8; k++) nwtrace_hex("fctc_word", ((unsigned long)k << 20) | *(uint32_t *)(pb + k * 4));
    }
#endif
    if (pTable == NULL) return NULL;     /* defensive: table not built yet */

    for (iSlot = 0; iSlot < 0x400; iSlot++) {
        /* The thread-buffer table stores 32-bit guest addresses.  Restore
         * the full host pointer before reading the thread id at +0x18; this
         * covers both the original low layout and the 64-bit guest window. */
        pContext = (void *)(uintptr_t)nw_guest_to_host(pTable[iSlot]);
#ifdef NWDEBUG_ENABLED
        if (pContext != NULL) nwtrace_hex("fctc_live_slot", ((unsigned long)iSlot << 20) | ((unsigned long)(void *)pContext & 0xFFFFF));
#endif
        if (pContext != NULL && dwThreadId == *(DWORD *)((BYTE *)pContext + 0x18)) {
            return pContext;             /* found this thread's record */
        }
    }

#ifdef NWDEBUG_ENABLED
    nwtrace_hex("fctc_notfound", 0);
#endif
    return NULL;                        /* no record for the calling thread */
}
#endif /* NW_UNIT_FindCurrentThreadContext */

/* ==========================================================================
 * Unit: NwEntrySehShim
 * Source: runtime/win32/NwEntrySehShim.c
 * ========================================================================== */
#ifdef NW_UNIT_NwEntrySehShim
/*
 * Nw Entry Seh Shim.
 *
 * Module: runtime/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-win32-nwentrysehshim-c
 */

#include <windows.h>

/* Thread id of the main (message-pump) thread, stamped by
 * InitializeGlobalsAndInstanceTag before the pipeline runs. */
unsigned long g_nwMainThreadId;

/* ------------------------------------------------------------------ writers */

static void put_u32(char *p, unsigned long v)
{
    const char d[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 8; i++)
        p[i] = d[(v >> ((7 - i) * 4)) & 0xF];
}

static void w(HANDLE h, const char *s)
{
    DWORD n = (DWORD)(lstrlenA(s)), wr;
    WriteFile(h, s, n, &wr, NULL);
}

static void wd(HANDLE h, unsigned long v)
{
    char b[8];
    DWORD wr;
    put_u32(b, v);
    WriteFile(h, b, 8, &wr, NULL);
}

static void wlabel(HANDLE h, const char *k, unsigned long v)
{
    w(h, " ");
    w(h, k);
    w(h, "=");
    wd(h, v);
}

/* ASCII 64-bit hex, so a text search of the crash log still finds full
 * addresses on the high proxy (wlabel/wd only carry the low 32 bits). */
static void whex64(HANDLE h, const char *k, unsigned long long v)
{
    static const char digits[] = "0123456789abcdef";
    char buf[17];
    int i;

    for (i = 15; i >= 0; --i) {
        buf[i] = digits[(unsigned)(v & 15u)];
        v >>= 4;
    }
    buf[16] = 0;
    w(h, " ");
    w(h, k);
    w(h, "=0x");
    w(h, buf);
}

/* ------------------------------------------------------------------ modules */

static char *nw_basename(char *p)
{
    char *b = p, *q = p;
    while (*q != 0) { if (*q == '\\' || *q == '/') b = q + 1; q++; }
    return b;
}

/* If `a` lies inside a loaded image module (any module, ours included), print
 * " <k>=<module-basename>+0x<offset>" (or " k[<slot>]=..." when k is NULL) and
 * return 1.  Otherwise print nothing and return 0. */
static int wres(HANDLE h, const char *k, int slot, unsigned long long a)
{
    MEMORY_BASIC_INFORMATION mbi;
    char path[MAX_PATH];
    char rev[8], one[2];
    unsigned long long base;
    int r = 0, m = slot;

    if (a < 0x10000ULL)
        return 0;
    if (VirtualQuery((LPCVOID)(uintptr_t)a, &mbi, sizeof mbi) != sizeof(mbi))
        return 0;
    if (mbi.Type != MEM_IMAGE)
        return 0;

    base = (unsigned long long)(uintptr_t)mbi.AllocationBase;
    path[0] = 0;
    if (GetModuleFileNameA((HMODULE)mbi.AllocationBase, path, sizeof path) == 0)
        path[0] = 0;

    if (k != NULL) {
        w(h, " "); w(h, k);
    } else {
        if (m == 0) rev[r++] = '0';
        while (m > 0) { rev[r++] = (char)('0' + (m % 10)); m /= 10; }
        w(h, " k[");
        one[1] = 0;
        while (r > 0) { one[0] = rev[--r]; w(h, one); }
        w(h, "]");
    }
    w(h, "=");
    w(h, nw_basename(path));
    w(h, "+"); wd(h, (unsigned long)(a - base));
    return 1;
}

/* -------------------------------------------------------------- one record */

static void NwDumpFault(HANDLE h, EXCEPTION_RECORD *rec, CONTEXT *ctx)
{
    unsigned char ib[16];
    SIZE_T got = 0;
    int i;

    wlabel(h, "main", (unsigned long)g_nwMainThreadId);
    if (GetCurrentThreadId() == g_nwMainThreadId)
        w(h, " iam=main");
    else
        w(h, " iam=secondary");

    /* Full-width ASCII summary first: the binary fields below are 32-bit. */
    w(h, "\r\nfault64");
    if (rec != NULL) {
        whex64(h, "eaddr", (unsigned long long)(uintptr_t)rec->ExceptionAddress);
        if (rec->NumberParameters >= 2) {
            whex64(h, "inf1", (unsigned long long)rec->ExceptionInformation[1]);
        }
    }
    if (ctx != NULL) {
        whex64(h, "eip",  (unsigned long long)NW_CTX_IP(ctx));
        whex64(h, "esp",  (unsigned long long)NW_CTX_SP(ctx));
        whex64(h, "ebp",  (unsigned long long)NW_CTX_BP(ctx));
        whex64(h, "eax",  (unsigned long long)NW_CTX_AX(ctx));
        whex64(h, "ebx",  (unsigned long long)NW_CTX_BX(ctx));
        whex64(h, "ecx",  (unsigned long long)NW_CTX_CX(ctx));
        whex64(h, "edx",  (unsigned long long)NW_CTX_DX(ctx));
        whex64(h, "esi",  (unsigned long long)NW_CTX_SI(ctx));
        whex64(h, "edi",  (unsigned long long)NW_CTX_DI(ctx));
    }
    w(h, "\r\n");

    if (rec != NULL) {
        wlabel(h, "code", (unsigned long)rec->ExceptionCode);
        wlabel(h, "eflags", (unsigned long)rec->ExceptionFlags);
        wlabel(h, "eaddr", (unsigned long)(uintptr_t)rec->ExceptionAddress);
        wlabel(h, "nparam", (unsigned long)rec->NumberParameters);
        if (rec->NumberParameters >= 2) {
            wlabel(h, "inf0", (unsigned long)rec->ExceptionInformation[0]);
            wlabel(h, "inf1", (unsigned long)rec->ExceptionInformation[1]);
        }
    }
    if (ctx == NULL)
        return;

    wlabel(h, "eip", (unsigned long)NW_CTX_IP(ctx));
    wres(h, "eipmod", 0, (unsigned long long)NW_CTX_IP(ctx));

    if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)NW_CTX_IP(ctx),
                          ib, sizeof ib, &got) && got >= 4) {
        const char d[] = "0123456789abcdef";
        int n = (got < (SIZE_T)16) ? (int)got : 16;
        w(h, " ib=");
        for (i = 0; i < n; i++) {
            char b[3];
            b[0] = d[ib[i] >> 4]; b[1] = d[ib[i] & 15]; b[2] = 0;
            w(h, b);
        }
    }

    wlabel(h, "esp", (unsigned long)NW_CTX_SP(ctx));
    wlabel(h, "ebp", (unsigned long)NW_CTX_BP(ctx));
    wlabel(h, "eax", (unsigned long)NW_CTX_AX(ctx));
    wlabel(h, "ebx", (unsigned long)NW_CTX_BX(ctx));
    wlabel(h, "ecx", (unsigned long)NW_CTX_CX(ctx));
    wlabel(h, "edx", (unsigned long)NW_CTX_DX(ctx));
    wlabel(h, "esi", (unsigned long)NW_CTX_SI(ctx));
    wlabel(h, "edi", (unsigned long)NW_CTX_DI(ctx));

    wres(h, "eaxmod", 0, (unsigned long long)NW_CTX_AX(ctx));
    wres(h, "ebxmod", 0, (unsigned long long)NW_CTX_BX(ctx));
    wres(h, "ecxmod", 0, (unsigned long long)NW_CTX_CX(ctx));
    wres(h, "edxmod", 0, (unsigned long long)NW_CTX_DX(ctx));
    wres(h, "esimod", 0, (unsigned long long)NW_CTX_SI(ctx));
    wres(h, "edimod", 0, (unsigned long long)NW_CTX_DI(ctx));
    wres(h, "ebpmod", 0, (unsigned long long)NW_CTX_BP(ctx));
    if (rec != NULL && rec->NumberParameters >= 2)
        wres(h, "inf1mod", 0, (unsigned long long)rec->ExceptionInformation[1]);

    if (NW_CTX_SP(ctx) >= 0x1000UL && NW_CTX_SP(ctx) < 0x80000000UL) {
        const unsigned long *sp = (const unsigned long *)(uintptr_t)NW_CTX_SP(ctx);
        int shown = 0;

        w(h, " sp[");
        for (i = 0; i < 24; i++) { wd(h, sp[i]); }
        w(h, "]");

        for (i = 0; i < 160 && shown < 28; i++) {
            if (wres(h, NULL, i, (unsigned long long)sp[i]))
                shown++;
        }

        /* Deep scan restricted to our own image, for long-lived threads whose
         * stack is mostly data. */
        for (i = 0; i < 512; i++) {
            unsigned long v = sp[i];
            if (v >= 0x401000UL && v < 0x417900UL) {
                w(h, " ra["); wd(h, (unsigned long)i); w(h, "]="); wd(h, v);
            }
        }
    }

    /* Legacy absolute-address snapshot.  Only valid for the low oracle image;
     * a high image base (the iOS/high-window proxy) would fault on these. */
    if (nw_guest_base == 0) {
        unsigned long o = *(unsigned long *)(uintptr_t)0x4191E0; /* g_display_object */
        wlabel(h, "sbase",   *(unsigned long *)(uintptr_t)0x419CF8); 
        wlabel(h, "spitch",  *(unsigned long *)(uintptr_t)0x419CFC); 
        wlabel(h, "srows",   *(unsigned long *)(uintptr_t)0x419D00); 
        wlabel(h, "arenasz", *(unsigned long *)(uintptr_t)0x41E344); 
        wlabel(h, "arenaptr",*(unsigned long *)(uintptr_t)0x41E348); 
        wlabel(h, "chunk",   *(unsigned long *)(uintptr_t)0x41E34C); 
        wlabel(h, "alloccur",*(unsigned long *)(uintptr_t)0x41E350); 
        wlabel(h, "dsc0",    *(unsigned long *)(uintptr_t)0x41E288); 
        wlabel(h, "dsc4kb",  *(unsigned long *)(uintptr_t)0x41E28C); 
        wlabel(h, "csur",    *(unsigned long *)(uintptr_t)0x41E3A0); /* color block */
        wlabel(h, "pctbl",   *(unsigned long *)(uintptr_t)0x41E340); 
        wlabel(h, "cxb",     *(unsigned long *)(uintptr_t)0x41E378); 
        wlabel(h, "cyb",     *(unsigned long *)(uintptr_t)0x41E37C); 
        wlabel(h, "cla",     *(unsigned long *)(uintptr_t)0x419228); 
        wlabel(h, "clb",     *(unsigned long *)(uintptr_t)0x41922C); 
        wlabel(h, "obj",     o);
        if (o >= 0x10000UL && o < 0x10000000UL) {
            const unsigned long *q = (const unsigned long *)(uintptr_t)o;
            wlabel(h, "o0", q[0]); wlabel(h, "o4", q[1]);
            wlabel(h, "o8", q[2]); wlabel(h, "oc", q[3]);
            wlabel(h, "o10", q[4]);
        }
    }

    /* Dispatch-loop frame bookkeeping (see the globals above). */
}

/* ------------------------------------------------------- entry()'s SEH frame */

int __cdecl
NwEntrySehHandler(EXCEPTION_RECORD *rec, void *frame, CONTEXT *ctx, void *dispatch)
{
    static HANDLE h;
    static int count = 0;
    (void)frame;
    (void)dispatch;

    if (!h)
        h = CreateFileA("netwalk_crash.log", FILE_WRITE_DATA | FILE_APPEND_DATA, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!h)
        return 0; /* EXCEPTION_CONTINUE_SEARCH */

    count++;
    if (count > 3)
        return 0;

    w(h, "\r\nSHIM#"); wd(h, (unsigned long)count);
    w(h, " tid="); wd(h, GetCurrentThreadId());
    NwDumpFault(h, rec, ctx);
    w(h, "\r\n");
    FlushFileBuffers(h);
    return 0; /* EXCEPTION_CONTINUE_SEARCH */
}

__asm__(".globl NwEntrySehHandler\n" ".set NwEntrySehHandler, _NwEntrySehHandler\n");

/* --------------------------------------------------------- vectored handler */

LONG WINAPI NwVectoredHandler(PEXCEPTION_POINTERS ep)
{
    static int busy;
    static int count;
    EXCEPTION_RECORD *rec;
    HANDLE h;
    unsigned long code;

    if (busy)
        return EXCEPTION_CONTINUE_SEARCH; /* re-entrancy guard: never recurse */

    rec = (ep != NULL) ? ep->ExceptionRecord : NULL;
    code = (rec != NULL) ? (unsigned long)rec->ExceptionCode : 0UL;

    switch (code) {
    case 0xC0000005UL:  /* ACCESS_VIOLATION */
    case 0xC0000006UL:  /* IN_PAGE_ERROR */
    case 0xC000001DUL:  
    case 0xC000001EUL:  /* INVALID_LOCK_SEQUENCE */
    case 0xC000008CUL:  /* ARRAY_BOUNDS_EXCEEDED */
    case 0xC000008DUL:  /* FLT_DENORMAL_OPERAND */
    case 0xC000008EUL:  /* FLT_DIVIDE_BY_ZERO */
    case 0xC000008FUL:  /* FLT_INEXACT_RESULT */
    case 0xC0000090UL:  /* FLT_INVALID_OPERATION */
    case 0xC0000091UL:  /* FLT_OVERFLOW */
    case 0xC0000092UL:  /* FLT_STACK_CHECK */
    case 0xC0000093UL:  /* FLT_UNDERFLOW */
    case 0xC0000094UL:  /* INT_DIVIDE_BY_ZERO */
    case 0xC0000095UL:  /* INT_OVERFLOW */
    case 0xC0000096UL:  
    case 0xC00000FDUL:  /* STACK_OVERFLOW */
    case 0xC0000409UL:  /* STACK_BUFFER_OVERRUN */
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (count >= 8)
        return EXCEPTION_CONTINUE_SEARCH;
    count++;

    busy = 1;
    h = CreateFileA("netwalk_crash.log", FILE_WRITE_DATA | FILE_APPEND_DATA, 0, NULL,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        w(h, "\r\nVEH# tid="); wd(h, GetCurrentThreadId());
        NwDumpFault(h, rec, (ep != NULL) ? ep->ContextRecord : NULL);
        w(h, "\r\n");
        FlushFileBuffers(h);
        CloseHandle(h);
    }
    busy = 0;
    return EXCEPTION_CONTINUE_SEARCH;
}

/* --------------------------------------------------- process-wide last chance */

LONG __cdecl
NwUnhandledExceptionFilter(EXCEPTION_POINTERS *ep)
{
    static int count;
    HANDLE h;

    if (count >= 4)
        return EXCEPTION_EXECUTE_HANDLER;
    count++;

    /* OPEN_ALWAYS + FILE_APPEND, never CREATE_ALWAYS: an earlier SHIM# record
     * must survive. */
    h = CreateFileA("netwalk_crash.log", FILE_WRITE_DATA | FILE_APPEND_DATA, 0, NULL,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return EXCEPTION_EXECUTE_HANDLER;

    w(h, "\r\nUNH# tid="); wd(h, GetCurrentThreadId());
    NwDumpFault(h, (ep != NULL) ? ep->ExceptionRecord : NULL,
                   (ep != NULL) ? ep->ContextRecord   : NULL);
    w(h, "\r\n");
    FlushFileBuffers(h);
    CloseHandle(h);
    return EXCEPTION_EXECUTE_HANDLER;
}

__asm__(".globl NwUnhandledExceptionFilter\n"
        ".set NwUnhandledExceptionFilter, _NwUnhandledExceptionFilter\n");
#endif /* NW_UNIT_NwEntrySehShim */

/* ==========================================================================
 * Unit: TranslateLastErrorToErrno
 * Source: runtime/win32/TranslateLastErrorToErrno.c
 * ========================================================================== */
#ifdef NW_UNIT_TranslateLastErrorToErrno
/*
 * Captures the calling thread's last Win32 error, converts it to a C runtime errno value, and caches that errno in the thread's context record
 *
 * Module: runtime/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-win32-translatelasterrortoerrno-c
 */

#include <windows.h>

extern void *FindCurrentThreadContext(DWORD dwUnusedParam1, DWORD dwUnusedParam2);

extern const BYTE g_aWin32ErrorToErrno[];

#define REC_FIELD_LASTERROR 0x8c        /* offset of the Win32 error field   */
#define REC_FIELD_ERRNO     0x10        /* offset of the cached errno field */
#define ERRNO_DEFAULT       22          /* EINVAL                            */
#define ERRNO_ACCES_BUCKET  13          /* EACCES range 0x13..0x24           */
#define ERRNO_NOENT_BUCKET   8          /* ENOENT range 0xbc..0xca           */

DWORD TranslateLastErrorToErrno(DWORD dwUnusedArg1, DWORD dwUnusedArg2)
{
    BYTE *pContext;
    const BYTE *pbTable;
    DWORD  dwLastError;
    BYTE   bKey;
    BYTE   bErrno;

    pContext = (BYTE *)FindCurrentThreadContext(dwUnusedArg1, dwUnusedArg2);
    if (pContext == NULL) {
        return ERRNO_DEFAULT;                    /* no record for this thread */
    }

    dwLastError = GetLastError();
    *(DWORD *)(pContext + REC_FIELD_LASTERROR) = dwLastError;

    /* Exact-match pass over the win32-error -> errno table. */
    pbTable = g_aWin32ErrorToErrno;
    do {
        bKey = pbTable[0];
        if ((DWORD)bKey == dwLastError) {
            bErrno = pbTable[1];
            *(DWORD *)(pContext + REC_FIELD_ERRNO) = bErrno;
            return dwLastError;
        }
        pbTable += 2;
    } while (pbTable < g_aWin32ErrorToErrno + 0x58);   

    /* Miss: fall back to coarse range buckets. */
    if ((dwLastError >= 0x13) && (dwLastError <= 0x24)) {
        *(DWORD *)(pContext + REC_FIELD_ERRNO) = ERRNO_ACCES_BUCKET;
    } else if ((dwLastError >= 0xbc) && (dwLastError <= 0xca)) {
        *(DWORD *)(pContext + REC_FIELD_ERRNO) = ERRNO_NOENT_BUCKET;
    } else {
        *(DWORD *)(pContext + REC_FIELD_ERRNO) = ERRNO_DEFAULT;
    }

    return dwLastError;
}
#endif /* NW_UNIT_TranslateLastErrorToErrno */
