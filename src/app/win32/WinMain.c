#include "nwdebug.h"
#include "nwarch.h"
#include <string.h>
/*
 * WinMain -- Win32 entry point, instance/global setup, and the main message loop.
 *
 * Module: app/win32
 * Interface: include/WinMain.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   InitializeAndRunWinMain                                    app/win32/InitializeAndRunWinMain.c
 *   InitializeGlobalsAndInstanceTag                            app/win32/InitializeGlobalsAndInstanceTag.c
 *   InitializeStandardHandlesInTrackedTable                    app/win32/InitializeStandardHandlesInTrackedTable.c
 *   LoadWin32sIniVersionPackMajorMinor                         app/win32/LoadWin32sIniVersionPackMajorMinor.c
 *   RunCommandLineAndWait                                      app/win32/RunCommandLineAndWait.c
 *   RunMainMessageLoop                                         app/win32/RunMainMessageLoop.c
 *   VerifyWin32sVersionOrReportStartupError                    app/win32/VerifyWin32sVersionOrReportStartupError.c
 */

/* ==========================================================================
 * Unit: InitializeAndRunWinMain
 * Source: app/win32/InitializeAndRunWinMain.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeAndRunWinMain
/*
 * Normal path, in the exact shipped order: install the crash reporter (portable-build diagnostic, inert normally)
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-initializeandrunwinmain-c
 */

#include <windows.h>
#include <stdint.h>

#ifdef NW_PLATFORM_SDL
/* Phase 0 smoke path: prove that the statically linked SDL3 video, events, and
 * timer subsystems initialize on this target.  Compiled only for the SDL
 * backend, so the Win32 oracle build never references these objects. */
#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#define NW_DIAG __attribute__((section(".nwdiag")))
#define NW32(off) (*(volatile uint32_t *)(nw_data_window + (off)))

extern unsigned char nw_data_window[] __asm__("nw_data_window");
extern void NwInstallCrashFilter(void);
#if defined(_WIN64)
extern void NwDumpLayout(const char *tag); /* round-1 instrumentation */
#endif
extern void ClearWritableDataSpanOnce(void);
extern void InitializeGlobalsAndInstanceTag(void);
extern int VerifyWin32sVersionOrReportStartupError(void);
extern void *InitializeGlobalThreadBufferOrDie(void);
extern void CallRegisteredGlobalHookIfInstalled(void);
extern void ParseCommandLineIntoArgcArgv(void);
extern uint32_t InitializeCrtRuntimeAndCtors(void);
extern int __stdcall
RunMainMessageLoop(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow);
extern uint32_t DispatchExitHooksAndClearShutdownFlags(uint32_t exit_code)
    __asm__("DispatchExitHooksAndClearShutdownFlags");

#ifdef NW_PLATFORM_SDL
/*
 * Development-only platform smoke test.  NW_DEBUG_SMOKE=1 makes the process
 * initialize SDL video/events/timer, create a hidden window and a software
 * renderer, pump events briefly, and exit before any game state is touched.
 * It exists to validate the static link and the SDL section layout; the real
 * window/render backends replace it in later phases.
 */
static int NwSdlPlatformSmoke(void)
{
    SDL_Window   *win;
    SDL_Renderer *ren;
    SDL_Event     ev;
    Uint64        start;

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "nw-sdl-smoke: init failed: %s\n", SDL_GetError());
        return 11;
    }

    win = SDL_CreateWindow("nw-sdl-smoke", 64, 64, SDL_WINDOW_HIDDEN);
    if (win == NULL) {
        fprintf(stderr, "nw-sdl-smoke: window failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
        return 12;
    }

    ren = SDL_CreateRenderer(win, "software");
    if (ren == NULL) {
        fprintf(stderr, "nw-sdl-smoke: renderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
        return 13;
    }

    start = SDL_GetTicks();
    while ((SDL_GetTicks() - start) < 100u) {
        while (SDL_PollEvent(&ev))
            ;
        SDL_Delay(5);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    return 0;
}
#endif

NW_DIAG int InitializeAndRunWinMain(void) __asm__("_InitializeAndRunWinMain");

__asm__(".globl WAnch_0040df7e\nWAnch_0040df7e:\n");
__asm__(".globl WAnch_0040df8a\nWAnch_0040df8a:\n");

NW_DIAG int InitializeAndRunWinMain(void)
{
    HINSTANCE hInstance;
    LPSTR pszCommandLine;
    int nLoopResult;

#ifdef NW_PLATFORM_SDL
    {
        const char *smoke = NW_DEBUG_GETENV("NW_DEBUG_SMOKE");
        if (smoke != NULL && smoke[0] != '\0' && smoke[0] != '0')
            return NwSdlPlatformSmoke();
    }
#endif

    NwInstallCrashFilter();
#if defined(_WIN64)
    NwDumpLayout("startup");
#endif

    ClearWritableDataSpanOnce();
    InitializeGlobalsAndInstanceTag();
    (void)VerifyWin32sVersionOrReportStartupError();

    pszCommandLine = GetCommandLineA();
    NW_WINDOW_PTR_REF(char, 0x0e7cu) = pszCommandLine;

    (void)InitializeGlobalThreadBufferOrDie();
    CallRegisteredGlobalHookIfInstalled();
    ParseCommandLineIntoArgcArgv();
    (void)InitializeCrtRuntimeAndCtors();
#if defined(_WIN64)
    NwDumpLayout("post-init");
#endif

    hInstance = (HINSTANCE)GetModuleHandleA(NULL);
    nLoopResult = RunMainMessageLoop(hInstance, (HINSTANCE)0, pszCommandLine,
                                     SW_SHOWDEFAULT);
    (void)DispatchExitHooksAndClearShutdownFlags((uint32_t)nLoopResult);
    return 0;
}
#endif /* NW_UNIT_InitializeAndRunWinMain */

/* ==========================================================================
 * Unit: InitializeGlobalsAndInstanceTag
 * Source: app/win32/InitializeGlobalsAndInstanceTag.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeGlobalsAndInstanceTag
/*
 * Initialize Globals And Instance Tag.
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-initializeglobalsandinstancetag-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];
#include "nwdebug.h"

extern char *ConvertIntegerToString(int iValue, char *pDest, unsigned int uBase);
extern void  *FillMemoryWithByte(void *pDst, unsigned char bFill, unsigned int dwCount);

#define g_rgbUnsetTable ((unsigned char *)(nw_data_window + 0x6418))

#define g_rgbZeroTableA ((unsigned char *)(nw_data_window + 0x64a4))

#define g_rgbZeroTableB ((unsigned char *)(nw_data_window + 0x65bc))

#define g_apvSemaphoreTable ((void * *)(nw_data_window + 0x66d4))

#define g_szInstanceTag ((char *)(nw_data_window + 0x6809))

void InitializeGlobalsAndInstanceTag(void)
{
    /* Install process-wide crash reporting: entry()'s own SEH frame only covers
     * the main thread, while the MIDI sequencer callback runs on a multimedia
     * thread.  The handler writes netwalk_crash.log only if a fault occurs. */
    {
        extern LONG __cdecl NwUnhandledExceptionFilter(EXCEPTION_POINTERS *);
        extern LONG WINAPI NwVectoredHandler(PEXCEPTION_POINTERS);
        extern unsigned long g_nwMainThreadId;
        g_nwMainThreadId = (unsigned long)GetCurrentThreadId();
        (void)SetUnhandledExceptionFilter(NwUnhandledExceptionFilter);

        (void)AddVectoredExceptionHandler(1, NwVectoredHandler);
    }
    NWDEBUG("enter InitializeGlobalsAndInstanceTag");
    /* Four distinct globals cleared to their initial state, in address order. */
    NWDEBUG("  fill unset table");
    FillMemoryWithByte(g_rgbUnsetTable,      0xFF, 0x8c);
    NWDEBUG("  fill zero table A");
    FillMemoryWithByte(g_rgbZeroTableA,      0x00, 0x118);
    NWDEBUG("  fill zero table B");
    FillMemoryWithByte(g_rgbZeroTableB,      0x00, 0x118);
    NWDEBUG("  fill semaphore table");
    FillMemoryWithByte(g_apvSemaphoreTable,  0x00, 0x118);

    NWDEBUG("  build instance tag");
    ConvertIntegerToString((int)GetTickCount(), g_szInstanceTag, 16u);
    NWDEBUG("  done");
}
#endif /* NW_UNIT_InitializeGlobalsAndInstanceTag */

/* ==========================================================================
 * Unit: InitializeStandardHandlesInTrackedTable
 * Source: app/win32/InitializeStandardHandlesInTrackedTable.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeStandardHandlesInTrackedTable
/*
 * Initialize Standard Handles In Tracked Table.
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-initializestandardhandlesintrackedtable-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

#define STD_HANDLE_TABLE_BASE ((unsigned char *)(nw_data_window + 0xBB8))   

void InitializeStandardHandlesInTrackedTable(void)
{
    *(HANDLE *)(intptr_t)(STD_HANDLE_TABLE_BASE + 0) = GetStdHandle(STD_INPUT_HANDLE);
    *(HANDLE *)(intptr_t)(STD_HANDLE_TABLE_BASE + 4) = GetStdHandle(STD_OUTPUT_HANDLE);
    *(HANDLE *)(intptr_t)(STD_HANDLE_TABLE_BASE + 8) = GetStdHandle(STD_ERROR_HANDLE);
}
#endif /* NW_UNIT_InitializeStandardHandlesInTrackedTable */

/* ==========================================================================
 * Unit: LoadWin32sIniVersionPackMajorMinor
 * Source: app/win32/LoadWin32sIniVersionPackMajorMinor.c
 * ========================================================================== */
#ifdef NW_UNIT_LoadWin32sIniVersionPackMajorMinor
/*
 * Load Win32s Ini Version Pack Major Minor.
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-loadwin32siniversionpackmajorminor-c
 */

#include <windows.h>
#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

extern void *AllocatePooledBlock(uint32_t byte_count);
extern void *EnsurePooledBlockCapacity(void *block, unsigned int min_bytes);
extern int __cdecl GetStringLength(const char *pszStr);
extern char *__cdecl AppendCStringToDest(char *pDestBuf, const char *pszSrc);
extern long __cdecl ParseSignedLongFromStringBaseN(char *szString, char **ppEnd,
                                                   int nBase);
extern void __cdecl RelinkListNodeUnderSlotLock(void *pUnlinkSlot);

#define NW_WIN32S_SENTINEL 0x10fu
#define NW_WIN32S_SUFFIX   "\\win32s.ini"

NW_DIAG uint32_t LoadWin32sIniVersionPackMajorMinor(void)
{
    DWORD dwVersion;
    void *pBlock;
    UINT cbSystemDir;
    uint32_t cbPath;
    char *pEnd;
    long lMajor;
    long lMinor;
    char szVersion[0x80];

    dwVersion = GetVersion();
    if (((dwVersion >> 24) & 0x80u) == 0u || (dwVersion & 0xffu) >= 4u) {
        return NW_WIN32S_SENTINEL;
    }

    pBlock = AllocatePooledBlock(1u);
    if (pBlock == NULL) {
        return NW_WIN32S_SENTINEL;
    }

    cbSystemDir = GetSystemDirectoryA((LPSTR)pBlock, 0u);
    cbPath = (uint32_t)cbSystemDir + (uint32_t)GetStringLength(NW_WIN32S_SUFFIX) + 1u;
    pBlock = EnsurePooledBlockCapacity(pBlock, cbPath);
    if (pBlock == NULL) {

        return NW_WIN32S_SENTINEL;
    }

    (void)GetSystemDirectoryA((LPSTR)pBlock, cbSystemDir);
    if (((char *)pBlock)[GetStringLength((const char *)pBlock) - 1] != '\\') {
        (void)AppendCStringToDest((char *)pBlock, "\\");
    }
    (void)AppendCStringToDest((char *)pBlock, "win32s.ini");

    (void)GetPrivateProfileStringA("Win32s", "Version", "1.0.0",
                                   szVersion, (DWORD)sizeof szVersion,
                                   (LPCSTR)pBlock);

    pEnd = szVersion;
    lMajor = ParseSignedLongFromStringBaseN(pEnd, &pEnd, 10);
    ++pEnd;                                  /* skip the '.' separator */
    lMinor = ParseSignedLongFromStringBaseN(pEnd, &pEnd, 10);

    RelinkListNodeUnderSlotLock(pBlock);
    return (uint32_t)((lMajor << 8) + lMinor);
}
#endif /* NW_UNIT_LoadWin32sIniVersionPackMajorMinor */

/* ==========================================================================
 * Unit: RunCommandLineAndWait
 * Source: app/win32/RunCommandLineAndWait.c
 * ========================================================================== */
#ifdef NW_UNIT_RunCommandLineAndWait
/*
 * Run Command Line And Wait.
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-runcommandlineandwait-c
 */

#include <stdint.h>
#include <windows.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

/* Callers currently pass the command-line pointer through an int parameter. */
NW_DIAG int __stdcall
RunCommandLineAndWait(int commandLine)
    __asm__("RunCommandLineAndWait");

NW_DIAG int __stdcall
RunCommandLineAndWait(int commandLine)
{
    STARTUPINFOA        si;
    PROCESS_INFORMATION pi;
    DWORD               waitResult;

    __builtin_memset(&si, 0, sizeof si);
    __builtin_memset(&pi, 0, sizeof pi);
    si.cb = sizeof si;

    if (!CreateProcessA(NULL, (LPSTR)(uintptr_t)commandLine,
                        NULL, NULL, TRUE, 0x20u,
                        NULL, NULL, &si, &pi)) {
        return 0;
    }

    waitResult = WaitForSingleObject(pi.hProcess, INFINITE);
    return (waitResult == WAIT_FAILED) ? 0 : 1;
}

/* Export the bare name used by the original asm entry path, and keep the
 * C spelling as an alias. Both are symbol-table only. */
__asm__(".globl _RunCommandLineAndWait\n"
        ".set _RunCommandLineAndWait, RunCommandLineAndWait\n");
#endif /* NW_UNIT_RunCommandLineAndWait */

/* ==========================================================================
 * Unit: RunMainMessageLoop
 * Source: app/win32/RunMainMessageLoop.c
 * ========================================================================== */
#ifdef NW_UNIT_RunMainMessageLoop
/*
 * Run Main Message Loop.
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-runmainmessageloop-c
 */

#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

extern unsigned char nw_data_window[] __asm__("nw_data_window");

#define NW32(off) (*(volatile uint32_t *)(nw_data_window + (off)))
#define NW8(off)  (*(volatile uint8_t  *)(nw_data_window + (off)))

/* Window slots used by the shipped message loop. */
#define W_HINSTANCE       0x6400u
#define W_SHOWCMD         0x5f10u
#define W_IDLE_OR_ERROR   0x5f14u
#define W_FRAME_INTERVAL  0x5f18u
#define W_LAST_FRAME_TIME 0x60c8u
#define W_MAIN_HWND       0x6404u
#define W_CURSOR_FLAGS    0x5efcu
#define W_WIDTH           0x5f20u
#define W_HEIGHT          0x5f24u
#define W_ANCHOR_X        0x5f48u
#define W_ANCHOR_Y        0x5f4cu
#define W_CURSOR_X        0x5f40u
#define W_CURSOR_Y        0x5f44u
#define W_CLICK_BYTE      0x5f50u
#define W_MODIFIER_BYTE   0x5f51u
#define W_KEY_BYTE        0x5f52u
#define W_APP_PATH        0x5eb4u
#define W_APP_PATH_BYTES  0x40u

#define NW_INPUT_CLEAR 0x0eu

extern char *__cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc);
extern uint32_t __stdcall
InitializeAppSubsystemsFromSavedPath(const char *pszPath);
extern void __stdcall BuildHlpSiblingPath(const char *pszSrcPath);
extern void FlushDeferredErrorString(void);
extern void DisplayHelpAndDestroyMainWindow(void);
extern void ShutdownAndReleaseResources(void);
extern uint32_t __stdcall
StoreArgsAndProcessQueuedRecords(uint32_t bPointerInsideClient,
                                 uint32_t bKeyCode,
                                 uint32_t bModifierSlot,
                                 uint32_t bClickAction,
                                 uint32_t dwAnchorY,
                                 uint32_t dwAnchorX,
                                 uint32_t dwCursorY,
                                 uint32_t dwCursorX,
                                 uint32_t dwSizeHighWord,
                                 uint32_t dwSizeLowWord);

NW_DIAG int __stdcall
RunMainMessageLoop(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
    __asm__("_RunMainMessageLoop");

NW_DIAG int __stdcall
RunMainMessageLoop(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    char szPath[0x100];
    MSG msg = {0};
    DWORD dwNow;
    uint32_t bPointerInsideClient;

    (void)hPrevInstance;
    (void)lpCmdLine;

    (void)GetModuleFileNameA(hInstance, szPath, (DWORD)sizeof szPath);

    NW_WINDOW_PTR_REF(HINSTANCE, W_HINSTANCE) = hInstance;
    NW32(W_SHOWCMD) = (uint32_t)nCmdShow;

    NW32(0x5f00u) = 0u;
    NW32(0x5f04u) = 0u;
    NW32(0x5f08u) = 0u;
    NW32(0x5f0cu) = 0u;
    NW32(W_IDLE_OR_ERROR) = 0u;
    NW32(W_FRAME_INTERVAL) = 0u;
    NW32(0x605cu) = 0u;
    NW32(0x6060u) = 0u;
    NW32(0x6064u) = 0u;
    NW32(0x60bcu) = 0u;
    NW32(W_MAIN_HWND) = 0u;
    NW32(W_ANCHOR_Y) = 0u;
    NW32(W_ANCHOR_X) = 0u;
    NW32(W_CURSOR_Y) = 0u;
    NW32(W_CURSOR_X) = 0u;
    NW32(W_HEIGHT) = 0u;
    NW32(W_WIDTH) = 0u;

    NW32(0x5f58u) = 0xffffffffu;
    NW32(0x5f54u) = 0xffffffffu;
    NW8(W_KEY_BYTE) = NW_INPUT_CLEAR;
    NW8(W_MODIFIER_BYTE) = NW_INPUT_CLEAR;
    NW8(W_CLICK_BYTE) = NW_INPUT_CLEAR;

    /* The shipped app-path slot is only 0x40 bytes.  Truncate the copy that
     * legacy movie/save helpers read, but initialize from the full module path
     * so overlay lookup and long install directories keep working. */
    (void)strncpy((char *)(nw_data_window + W_APP_PATH), szPath,
                  W_APP_PATH_BYTES - 1u);
    ((char *)(nw_data_window + W_APP_PATH))[W_APP_PATH_BYTES - 1u] = '\0';
    if (InitializeAppSubsystemsFromSavedPath(szPath) == 0u) {
        FlushDeferredErrorString();
        DisplayHelpAndDestroyMainWindow();
        ShutdownAndReleaseResources();
        return 0;
    }

    BuildHlpSiblingPath(szPath);

    for (;;) {

        if (PeekMessageA(&msg, (HWND)0, 0u, 0u, PM_REMOVE) != FALSE) {
            if (msg.message == WM_QUIT) {
                ShutdownAndReleaseResources();
                return (int)msg.wParam;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            continue;
        }

        if (NW32(W_IDLE_OR_ERROR) == 1u) {
            ShutdownAndReleaseResources();
            return (int)msg.wParam;
        }
        if (NW32(W_IDLE_OR_ERROR) != 0u) {
            FlushDeferredErrorString();
            DisplayHelpAndDestroyMainWindow();
            continue;
        }

        if (IsIconic((HWND)(uintptr_t)NW32(W_MAIN_HWND)) != FALSE) {
            continue;
        }

        dwNow = timeGetTime();
        if ((uint32_t)(NW32(W_LAST_FRAME_TIME) + NW32(W_FRAME_INTERVAL)) >= dwNow) {
            continue;
        }
        NW32(W_LAST_FRAME_TIME) = dwNow;
        bPointerInsideClient = 1u;

        if ((((const uint8_t *)NW_WINDOW_PTR_VALUE(W_CURSOR_FLAGS))[4] & 0x02u) != 0u) {
            POINT pt;

            (void)GetCursorPos(&pt);
            (void)ScreenToClient((HWND)(uintptr_t)NW32(W_MAIN_HWND), &pt);
            if (pt.x < 0 || pt.x >= (LONG)NW32(W_WIDTH) ||
                pt.y < 0 || pt.y >= (LONG)NW32(W_HEIGHT)) {
                bPointerInsideClient = 0u;
            }
        }

        if (StoreArgsAndProcessQueuedRecords(
                bPointerInsideClient,
                NW32(W_KEY_BYTE),
                NW32(W_MODIFIER_BYTE),
                NW32(W_CLICK_BYTE),
                NW32(W_ANCHOR_Y),
                NW32(W_ANCHOR_X),
                NW32(W_CURSOR_Y),
                NW32(W_CURSOR_X),
                NW32(W_HEIGHT),
                NW32(W_WIDTH)) != 0u) {

            NW8(W_KEY_BYTE) = NW_INPUT_CLEAR;
            NW8(W_MODIFIER_BYTE) = NW_INPUT_CLEAR;
            NW8(W_CLICK_BYTE) = NW_INPUT_CLEAR;
            continue;
        }

        DisplayHelpAndDestroyMainWindow();
        ShutdownAndReleaseResources();
        return (int)msg.wParam;
    }
}
#endif /* NW_UNIT_RunMainMessageLoop */

/* ==========================================================================
 * Unit: VerifyWin32sVersionOrReportStartupError
 * Source: app/win32/VerifyWin32sVersionOrReportStartupError.c
 * ========================================================================== */
#ifdef NW_UNIT_VerifyWin32sVersionOrReportStartupError
/*
 * Verify Win32s Version Or Report Startup Error.
 *
 * Module: app/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#app-win32-verifywin32sversionorreportstartuperror-c
 */

#include <windows.h>
#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

extern unsigned char nw_data_window[] __asm__("nw_data_window");
extern uint32_t LoadWin32sIniVersionPackMajorMinor(void);
extern void __cdecl WriteStdoutStringAndExit(const char *pszMessage);

NW_DIAG int VerifyWin32sVersionOrReportStartupError(void)
{
    uint32_t dwVersion = LoadWin32sIniVersionPackMajorMinor();

    if (dwVersion >= 0x10fu) {
        return 1;
    }

    (void)MessageBoxA(NULL,
                      (LPCSTR)(nw_data_window + 0xdf3u),
                      (LPCSTR)(nw_data_window + 0xe6du),
                      MB_ICONERROR);
    WriteStdoutStringAndExit((const char *)(nw_data_window + 0xdf2u));

    /* Dead in the shipped flow: WriteStdoutStringAndExit does not return. */
    return 0;
}
#endif /* NW_UNIT_VerifyWin32sVersionOrReportStartupError */
