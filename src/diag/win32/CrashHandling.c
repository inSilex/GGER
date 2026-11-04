/*
 * CrashHandling -- fatal-exception reporting and crash filters.
 *
 * Module: diag/win32
 * Interface: include/CrashHandling.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   CrashDumpFilter                                            diag/win32/CrashDumpFilter.c
 *   NwCrashFilter                                              diag/win32/NwCrashFilter.c
 */

/* ==========================================================================
 * Unit: CrashDumpFilter
 * Source: diag/win32/CrashDumpFilter.c
 * ========================================================================== */
#if defined(_WIN64)
#  define NW_CTX_IP(c) ((uintptr_t)(c)->Rip)
#  define NW_CTX_SP(c) ((uintptr_t)(c)->Rsp)
#else
#  define NW_CTX_IP(c) ((uintptr_t)(c)->Eip)
#  define NW_CTX_SP(c) ((uintptr_t)(c)->Esp)
#endif

#ifdef NW_UNIT_CrashDumpFilter
/*
 * InstallCrashDumper -- SEH filter that writes a crash report to netwalk_crash.log
 *
 * Module: diag/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#diag-win32-crashdumpfilter-c
 */

#include <windows.h>

static void cd_write(HANDLE h, const char *s)
{
    DWORD n = (DWORD)(lstrlenA(s));
    DWORD w;
    WriteFile(h, s, n, &w, NULL);
}

static LONG WINAPI cd_handler(EXCEPTION_POINTERS *ep)
{
    static HANDLE h;
    EXCEPTION_RECORD *ei = ep->ExceptionRecord;
    CONTEXT *c = ep->ContextRecord;
    char buf[64];
    int i;

    if (h == INVALID_HANDLE_VALUE || h == NULL) {
        h = CreateFileA("netwalk_crash.log", FILE_WRITE_DATA, 0,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (h == INVALID_HANDLE_VALUE || h == NULL)
        return 0;

    cd_write(h, "crashdump exception: code=");
    {
        unsigned long v = (unsigned long)ei->ExceptionCode;
        /* manual hex to stay out of the printf family */
        for (i = 0; i < 8; i++) {
            int d = (int)(v >> ((7 - i) * 4)) & 0xF;
            buf[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        }
        buf[8] = 0;
        cd_write(h, buf);
    }
    cd_write(h, " addr=");
    {
        unsigned long v = (unsigned long)(uintptr_t)ei->ExceptionAddress;
        for (i = 0; i < 8; i++) {
            int d = (int)(v >> ((7 - i) * 4)) & 0xF;
            buf[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        }
        buf[8] = 0;
        cd_write(h, buf);
    }
    cd_write(h, " esp=");
    {
        unsigned long v = (unsigned long)NW_CTX_SP(c);
        for (i = 0; i < 8; i++) {
            int d = (int)(v >> ((7 - i) * 4)) & 0xF;
            buf[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        }
        buf[8] = 0;
        cd_write(h, buf);
    }
    /* first four stack DWORDs as hex pairs */
    for (i = 0; i < 4; i++) {
        unsigned long v = *(unsigned long *)((unsigned char *)NW_CTX_SP(c) + i * sizeof(uintptr_t));
        cd_write(h, " st=");
        for (int j = 0; j < 8; j++) {
            int d = (int)(v >> ((7 - j) * 4)) & 0xF;
            buf[j] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        }
        buf[8] = 0;
        cd_write(h, buf);
    }
    cd_write(h, "\r\n");
    return 0; /* let teardown proceed */
}

void InstallCrashDumper(void)
{
    SetUnhandledExceptionFilter(cd_handler);
}
#endif /* NW_UNIT_CrashDumpFilter */

/* ==========================================================================
 * Unit: NwCrashFilter
 * Source: diag/win32/NwCrashFilter.c
 * ========================================================================== */
#ifdef NW_UNIT_NwCrashFilter
/*
 * Debug-only top-level exception filter.
 *
 * Module: diag/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#diag-win32-nwcrashfilter-c
 */

#include <windows.h>

/* Writes 8 hex digits of v into p[0..7] (little-endian digit order, i.e. prints the
 * value in the natural left-to-right order). No allocation, no libc math beyond shifts. */
static void NwPutU32(char *p, unsigned long v)
{
    const char d[] = "0123456789abcdef";
    p[0] = d[(v >> 28) & 0xF];
    p[1] = d[(v >> 24) & 0xF];
    p[2] = d[(v >> 20) & 0xF];
    p[3] = d[(v >> 16) & 0xF];
    p[4] = d[(v >> 12) & 0xF];
    p[5] = d[(v >> 8)   & 0xF];
    p[6] = d[(v >> 4)   & 0xF];
    p[7] = d[v           & 0xF];
}

static long WINAPI NwTopLevelFilter(PEXCEPTION_POINTERS ep)
{
    char buf[96];

    /* Layout: "NWEXC code=XXXXXXXX addr=XXXXXXXX eip=XXXXXXXX esp=XXXXXXXX\n" */
    buf[0]='N';buf[1]='W';buf[2]='E';buf[3]='X';buf[4]='C';buf[5]=' ';buf[6]='c';
    buf[7]='o';buf[8]='d';buf[9]='e';buf[10]='=';
    NwPutU32(buf + 11, ep ? (unsigned long)(uintptr_t)ep->ExceptionRecord->ExceptionCode : 0xFFFFFFFFUL);
    buf[19]=' ';buf[20]='a';buf[21]='d';buf[22]='d';buf[23]='r';buf[24]='=';
    NwPutU32(buf + 25, ep ? (unsigned long)ep->ExceptionRecord->ExceptionAddress : 0UL);
    buf[33]=' ';buf[34]='e';buf[35]='i';buf[36]='p';buf[37]='=';
    NwPutU32(buf + 38, ep ? (unsigned long)NW_CTX_IP(ep->ContextRecord) : 0UL);
    buf[46]=' ';buf[47]='e';buf[48]='s';buf[49]='p';buf[50]='=';
    NwPutU32(buf + 51, ep ? (unsigned long)NW_CTX_SP(ep->ContextRecord) : 0UL);
    buf[59]='\n';
    buf[60]=0;

    {
        DWORD n = 0;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), buf, 61, &n, NULL);
    }
    return 0; /* not handled: terminate as usual */
}

void NwInstallCrashFilter(void)
{
    SetUnhandledExceptionFilter(NwTopLevelFilter);
}
#endif /* NW_UNIT_NwCrashFilter */
