/*
 * AppShutdown -- quoted command construction, resource release, and process exit.
 *
 * Module: app/win32
 * Interface: include/AppShutdown.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   BuildQuotedCommandImage                                    app/win32/BuildQuotedCommandImage.c
 *   ShutdownAndReleaseResources                                app/win32/ShutdownAndReleaseResources.c
 *   TerminateWithExitStatusCode                                app/win32/TerminateWithExitStatusCode.c
 *   WriteStdoutStringAndExit                                   app/win32/WriteStdoutStringAndExit.c
 */

/* ==========================================================================
 * Unit: BuildQuotedCommandImage
 * Source: app/win32/BuildQuotedCommandImage.c
 * ========================================================================== */
#ifdef NW_UNIT_BuildQuotedCommandImage
/*
 * Builds the command image string consumed by ParseCommandLineIntoArgcArgv: the executable path is quoted once as the program name, followed by a space
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-buildquotedcommandimage-c
 */

#include <stdint.h>
#include <windows.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

extern unsigned char nw_data_window[] __asm__("nw_data_window");

int __cdecl GetStringLength(const char *pszStr);
void *AllocatePooledBlock(uint32_t size);
char *FindFirstCharInString(const char *pszString, int cSearch);
char *CopyNulTerminatedString(char *pszDst, const char *pszSrc);
char *AppendCStringToDest(char *pDestBuf, const char *pszSrc);

NW_DIAG char *BuildQuotedCommandImage(const char *pszCommandLine)
{
    char szPath[0x104];
    uintptr_t uBuffer;
    char *pBuffer;
    const char *pszQuote = (const char *)(const void *)(nw_data_window + 0xeaa);
    const char *pszSpace = (const char *)(const void *)(nw_data_window + 0xeac);

    uBuffer = (uintptr_t)AllocatePooledBlock(
        (uint32_t)GetStringLength(pszCommandLine) + 0x108u);
    *(volatile char *)(void *)uBuffer = '\0';
    if (uBuffer == 0u) {
        return NULL;
    }
    pBuffer = (char *)(void *)uBuffer;

    if (GetModuleFileNameA(NULL, szPath, (DWORD)sizeof(szPath)) == 0u) {
        return NULL;
    }

    if (FindFirstCharInString(szPath, '"') != NULL) {
        CopyNulTerminatedString(pBuffer, szPath);
        AppendCStringToDest(pBuffer, pszSpace);
        AppendCStringToDest(pBuffer, pszCommandLine);
        return pBuffer;
    }

    AppendCStringToDest(pBuffer, pszQuote);
    AppendCStringToDest(pBuffer, szPath);
    AppendCStringToDest(pBuffer, pszQuote);
    AppendCStringToDest(pBuffer, pszSpace);
    AppendCStringToDest(pBuffer, pszCommandLine);
    return pBuffer;
}
#endif /* NW_UNIT_BuildQuotedCommandImage */

/* ==========================================================================
 * Unit: ShutdownAndReleaseResources
 * Source: app/win32/ShutdownAndReleaseResources.c
 * ========================================================================== */
#ifdef NW_UNIT_ShutdownAndReleaseResources
/*
 * Releases the main-window handles, palette, memory DC, and global buffer.
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-shutdownandreleaseresources-c
 */

#include <stdint.h>

/* Genuine imports through .idata. */
extern int __stdcall DeleteObject(void *hObject); 
extern int __stdcall DeleteDC(void *hdc);         
extern void *__stdcall GlobalFree(void *hMem);      

/* Direct callees. */
extern void InitializeHookAndFlagGlobals(void); 
extern void CloseSaveFileSlot(void);                  

extern uintptr_t g_hwndMain;            /* cleared unconditionally up front */
extern uintptr_t g_hpalPalette;         /* GDI object handle -> DeleteObject */
extern uintptr_t g_hShutdownGdiObject;  /* GDI object handle -> DeleteObject */
extern uintptr_t g_hdcSrcMem;           /* GDI device context -> DeleteDC */
extern void     *g_hGlobalMemory;       /* GlobalAlloc block -> GlobalFree */
extern uintptr_t g_bGlobalMemoryActive; /* companion flag, cleared with the block */

void ShutdownAndReleaseResources(void)
{
    g_hwndMain = 0;
    InitializeHookAndFlagGlobals();
    CloseSaveFileSlot();

    if (g_hpalPalette != 0) {
        DeleteObject((void *)g_hpalPalette);
        g_hpalPalette = 0;
    }
    if (g_hdcSrcMem != 0) {
        DeleteDC((void *)g_hdcSrcMem); /* handle intentionally left dangling */
    }
    if (g_hShutdownGdiObject != 0) {
        DeleteObject((void *)g_hShutdownGdiObject); /* handle intentionally left dangling */
    }
    if (g_hGlobalMemory != 0) {
        GlobalFree(g_hGlobalMemory);
        g_hGlobalMemory = 0;
        g_bGlobalMemoryActive = 0;
    }
}
#endif /* NW_UNIT_ShutdownAndReleaseResources */

/* ==========================================================================
 * Unit: TerminateWithExitStatusCode
 * Source: app/win32/TerminateWithExitStatusCode.c
 * ========================================================================== */
#ifdef NW_UNIT_TerminateWithExitStatusCode
/*
 * Terminate With Exit Status Code.
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-terminatewithexitstatuscode-c
 */

#include <windows.h>
#include "nwdebug.h"
extern unsigned char nw_data_window[];

/* Terminates the calling process immediately with the given exit status code.
   Declared noreturn in behavior, not just by convention: the body contains no
   RET and its only callee, ExitProcess, never returns. */
void TerminateWithExitStatusCode(UINT uExitCode);

void TerminateWithExitStatusCode(UINT uExitCode)
{
    extern void nwtrace_hex(const char *, unsigned long);
    nwtrace_hex("term_code", (unsigned long)uExitCode);
    nwtrace_hex("term_ra", (unsigned long)(uintptr_t)__builtin_return_address(0));
    NWDEBUG("enter TerminateWithExitStatusCode");
    ExitProcess(uExitCode);
}
#endif /* NW_UNIT_TerminateWithExitStatusCode */

/* ==========================================================================
 * Unit: WriteStdoutStringAndExit
 * Source: app/win32/WriteStdoutStringAndExit.c
 * ========================================================================== */
#ifdef NW_UNIT_WriteStdoutStringAndExit
/*
 * Writes the NUL-terminated string it is given to the process standard error handle with WriteFile.
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-writestdoutstringandexit-c
 */

#include <windows.h>

void TerminateWithExitStatusCode(UINT uExitCode);

void WriteStdoutStringAndExit(char *szMessage)
{
    HANDLE hStdErr;
    char  *pcCursor;
    DWORD  dwLength;
    DWORD  dwBytesWritten;

    dwLength = 0;
    pcCursor = szMessage;
    while (*pcCursor != '\0') {
        dwLength++;
        pcCursor++;
    }

    hStdErr = GetStdHandle(STD_ERROR_HANDLE);

    dwBytesWritten = 0;
    WriteFile(hStdErr, szMessage, dwLength, &dwBytesWritten, NULL);

    /* WriteFile's BOOL result is discarded; the exit status is always 1. */
    TerminateWithExitStatusCode(1);
}
#endif /* NW_UNIT_WriteStdoutStringAndExit */
