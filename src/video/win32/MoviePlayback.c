/*
 * MoviePlayback -- AVI/BMP movie discovery and startup playback.
 *
 * Module: video/win32
 * Interface: include/MoviePlayback.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   BuildAviBmpPathsFromSourcePath                             video/win32/BuildAviBmpPathsFromSourcePath.c
 *   FindMovieFileAcrossDrivesAndEmitBmpAviPaths                video/win32/FindMovieFileAcrossDrivesAndEmitBmpAviPaths.c
 *   PlayStartupMovieWithMessagePumpAndTeardown                 video/win32/PlayStartupMovieWithMessagePumpAndTeardown.c
 *   PlayVideoTileMovieAndLoadBmp                               video/win32/PlayVideoTileMovieAndLoadBmp.c
 */

/* ==========================================================================
 * Unit: BuildAviBmpPathsFromSourcePath
 * Source: video/win32/BuildAviBmpPathsFromSourcePath.c
 * ========================================================================== */
#ifdef NW_UNIT_BuildAviBmpPathsFromSourcePath
/*
 * Copies the program's own path into a 64-byte scratch buffer, truncates it to its directory, appends the bare file name of the source path.
 *
 * Module: video/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-win32-buildavibmppathsfromsourcepath-c
 */

#include <stddef.h>
#include <windows.h>

extern char g_szAppPath[];

extern char *__cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc);
extern char *FindLastCharInString(const char *pszSrc, unsigned char chTarget);
extern char *__cdecl AppendCStringToDest(char *pDestBuf, const char *pszSrc);

int __cdecl BuildAviBmpPathsFromSourcePath(char *pszSrcPath,   /* in : any path, full or bare     */
                                           char *pszBmpPathOut, /* out: "<appdir>\<name>.BMP"     */
                                           char *pszAviPathOut) /* out: "<appdir>\<name>.AVI"     */
{
    char szPathBuf[64]; /* "<appdir>\<name>" assembled in place, then copied out twice */
    char *pcSep;        /* last backslash located in the string being truncated       */

    /* szPathBuf = program's own path, cut back to its directory (trailing '\' kept). */
    CopyNulTerminatedString(szPathBuf, g_szAppPath);
    pcSep = FindLastCharInString(szPathBuf, '\\');
    pcSep[1] = '\0'; 

    /* szPathBuf = "<app dir>\" + bare name of pszSrcPath (everything past its last '\'). */
    pcSep = FindLastCharInString(pszSrcPath, '\\');
    AppendCStringToDest(szPathBuf, pcSep + 1);

    /* pszAviPathOut = "<app dir>\<name>.AVI". */
    CopyNulTerminatedString(pszAviPathOut, szPathBuf);
    AppendCStringToDest(pszAviPathOut, ".AVI");

    /* pszBmpPathOut = "<app dir>\<name>.BMP". */
    CopyNulTerminatedString(pszBmpPathOut, szPathBuf);
    AppendCStringToDest(pszBmpPathOut, ".BMP");

    /* Only the .AVI is probed for existence; the .BMP path is merely produced, never tested. */
    return GetFileAttributesA(pszAviPathOut) != INVALID_FILE_ATTRIBUTES;
}
#endif /* NW_UNIT_BuildAviBmpPathsFromSourcePath */

/* ==========================================================================
 * Unit: FindMovieFileAcrossDrivesAndEmitBmpAviPaths
 * Source: video/win32/FindMovieFileAcrossDrivesAndEmitBmpAviPaths.c
 * ========================================================================== */
#ifdef NW_UNIT_FindMovieFileAcrossDrivesAndEmitBmpAviPaths
/*
 * Find Movie File Across Drives And Emit Bmp Avi Paths.
 *
 * Module: video/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-win32-findmoviefileacrossdrivesandemitbmpavipaths-c
 */

#include <windows.h>
#include <stdint.h>

extern char g_szAppPath[];

extern char g_szDriveRoot[];

extern char *__cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc);
extern char *__cdecl AppendCStringToDest(char *pDestBuf, const char *pszSrc);
extern int   GetStringLength(const char *pszStr);
extern char *FindLastCharInString(const char *pszSrc, unsigned char chTarget);
extern int   FindBestAvailableDriveAndEmitLabel(unsigned char *pbDriveTypeTable, char *szLabelOut);

int __stdcall FindMovieFileAcrossDrivesAndEmitBmpAviPaths(const char *pszName,
                                                          char       *pszBmpPathOut,
                                                          char       *pszAviPathOut)
{
    unsigned char abDriveType[32];   /* [ESP+0x10] per-letter GetDriveTypeA result (0 = unmapped) */
    uint32_t      dwDriveMask;       /* [ESP+0x30] GetLogicalDrives bitmask                     */
    uint32_t      dwDriveIndex;      /* [ESP+0x34] drive-letter index, 0..31                    */
    uint32_t      dwBaseLen;         /* EBX: length of the string currently in pszAviPathOut    */
    uint32_t      dwNameLen;         /* [ESP+0x34-after-adjust]: GetStringLength(pszName)       */

    /* --- (1) Fill the 32-entry drive-type table for every set bit of the drive mask -------- */
    dwDriveMask = GetLogicalDrives();
    for (dwDriveIndex = 0; dwDriveIndex < 32u; ++dwDriveIndex) {
        abDriveType[dwDriveIndex] = 0;                 /* a skipped letter keeps a zero slot */
        if (dwDriveMask & (1u << dwDriveIndex)) {
            g_szDriveRoot[0] = (char)('A' + dwDriveIndex);   /* "?:\" -> "X:\"            */
            abDriveType[dwDriveIndex] = (unsigned char)GetDriveTypeA(g_szDriveRoot);
        }
    }

    /* --- (2) Seed the working buffer with this module's own directory ---------------------- */
    CopyNulTerminatedString(pszAviPathOut, g_szAppPath);
    *FindLastCharInString(pszAviPathOut, '\\') = '\0';   /* cut back to the directory         */
    dwBaseLen = (uint32_t)GetStringLength(pszAviPathOut);
    dwNameLen = (uint32_t)GetStringLength(pszName);      

    /* --- (3) Try each candidate root until "<root>\<name>.AVI" exists --------------------- */
    for (;;) {

        if (dwNameLen + dwBaseLen >= 0x48u)
            goto pickNextDrive;

        AppendCStringToDest(pszAviPathOut, "\\");          /* "<root>" + "\"                */
        AppendCStringToDest(pszAviPathOut, pszName);       /* "<root>\<name>"               */
        CopyNulTerminatedString(pszBmpPathOut, pszAviPathOut); /* BMP takes the same stem   */
        AppendCStringToDest(pszAviPathOut, ".AVI");        /* "<root>\<name>.AVI"  (probed) */
        AppendCStringToDest(pszBmpPathOut, ".BMP");        /* "<root>\<name>.BMP"  (output) */

        if (GetFileAttributesA(pszAviPathOut) != INVALID_FILE_ATTRIBUTES)
            return 1;                                      /* only AL is defined on this path */

        dwBaseLen = 2u;                                    /* next root is "X:" (2 chars)   */

    pickNextDrive:
        if (!FindBestAvailableDriveAndEmitLabel(abDriveType, pszAviPathOut))
            return 0;                                      /* no wanted drive left          */
        /* pszAviPathOut now holds "<letter>:"; loop to rebuild and re-probe */
    }
}
#endif /* NW_UNIT_FindMovieFileAcrossDrivesAndEmitBmpAviPaths */

/* ==========================================================================
 * Unit: PlayStartupMovieWithMessagePumpAndTeardown
 * Source: video/win32/PlayStartupMovieWithMessagePumpAndTeardown.c
 * ========================================================================== */
#ifdef NW_UNIT_PlayStartupMovieWithMessagePumpAndTeardown
/*
 * Startup-movie player: opens the tile/movie media, arms the global MCI device, then pumps window messages until the media loop flag clears.
 *
 * Module: video/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-win32-playstartupmoviewithmessagepumpandteardown-c
 */

#include <stdint.h>
#include "nwglobals.h"
extern unsigned char nw_data_window[];   /* uintptr_t: 32-bit flat addresses widened for mingw's 64-bit host */

typedef unsigned long  DWORD;
typedef long           LONG;
typedef unsigned short WORD;

/* ---- globals ------------------------------------------------------------ */
/* Every slot below is shared program state; see the binding notes per name. */

#define g_bWin9xFlag         (*(unsigned char *)(nw_data_window + 0x60c5)) 
#define g_bMediaActive       (*(unsigned char *)(nw_data_window + 0x60c3)) 
#define g_bQuitRequested     (*(unsigned char *)(nw_data_window + 0x60c1)) 
#define g_bKeepPlaying       (*(unsigned char *)(nw_data_window + 0x60c0)) 
#define g_bVideoCapsLatched  (*(unsigned char *)(nw_data_window + 0x60c4)) 
#define g_bSpareC6           (*(unsigned char *)(nw_data_window + 0x60c6)) 

#define g_dwMciDeviceId      (*(DWORD *)(nw_data_window + 0x60b8))         
#define g_dwMciWindow        (*(DWORD *)(nw_data_window + 0x60bc))         

#define g_dwMovieWindowX (*(DWORD *)(nw_data_window + 0x6068))
#define g_dwMovieWindowY (*(DWORD *)(nw_data_window + 0x606c)) 

#define g_dwMovieRectWord2 (*(DWORD *)(nw_data_window + 0x6070))

#define g_dwMovieRectWord3 (*(DWORD *)(nw_data_window + 0x6074))

#define g_szMovieBasePath ((char *)(nw_data_window + 0x6078))
typedef unsigned int   PTRWORD;            /* a 32-bit flat address held in a dword     */
typedef uintptr_t     PTRADDR;            /* widened only so the address cast is legal */

extern DWORD  g_dwDisplayDc;              

#define g_hwndMain (*(DWORD *)(nw_data_window + 0x6404))

/* ---- imports (IAT slots resolved by reading the import table) ------------ */
typedef DWORD  (__stdcall *PFN_GetVersion)(void);            
typedef DWORD  (__stdcall *PFN_GetDeviceCaps)(DWORD, int);   
typedef int    (__stdcall *PFN_InvalidateRect)(DWORD, void *, DWORD); 
typedef int    (__stdcall *PFN_GetMessageA)(void *, DWORD, DWORD, DWORD); 
typedef int    (__stdcall *PFN_DestroyWindow)(DWORD);        
typedef int    (__stdcall *PFN_PostMessageA)(DWORD, DWORD, DWORD, DWORD); 
typedef int    (__stdcall *PFN_ShowWindow)(DWORD, int);      
typedef int    (__stdcall *PFN_SetCursorPos)(int, int);      
typedef int    (__stdcall *PFN_TranslateMessage)(void *);    
typedef int    (__stdcall *PFN_DispatchMessageA)(void *);     
typedef int    (__stdcall *PFN_GetCursorPos)(void *);        

extern PFN_GetVersion       pfnGetVersion;
extern PFN_GetDeviceCaps    pfnGetDeviceCaps;
extern PFN_InvalidateRect   pfnInvalidateRect;
extern PFN_GetMessageA      pfnGetMessageA;
extern PFN_DestroyWindow    pfnDestroyWindow;
extern PFN_PostMessageA     pfnPostMessageA;
extern PFN_ShowWindow       pfnShowWindow;
extern PFN_SetCursorPos     pfnSetCursorPos;
extern PFN_TranslateMessage pfnTranslateMessage;
extern PFN_DispatchMessageA pfnDispatchMessageA;
extern PFN_GetCursorPos     pfnGetCursorPos;

/* ---- callees ------------------------------------------------------------ */
extern char *__cdecl CopyNulTerminatedString(char *, const char *);          
extern char *__cdecl          AppendCStringToDest(char *, const char *);    
extern void   __cdecl          PlayVideoTileMovieAndLoadBmp(void);          

extern DWORD  __stdcall        SendMciSetOnWithPointerToGlobalWord(void);
/* the two constants are passed through unchanged; their meaning is not in these    */

extern void   __stdcall        PlayMciWavesCommandIfDeviceArmed(DWORD, DWORD);

extern void   __cdecl          RebuildGridPaletteAndResetExtent(void);
extern void   __cdecl          DispatchQueuedNonInputMessages(void);        

/* ---- local record shapes ------------------------------------------------ */
typedef struct { LONG x; LONG y; } PointRecord;                 /* 8 bytes */
typedef struct { DWORD hwnd; DWORD message; DWORD wParam;
                 DWORD lParam; DWORD time; PointRecord pt; } MsgRecord; /* 0x1C */

int __stdcall PlayStartupMovieWithMessagePumpAndTeardown(
    DWORD dwMovieRectWord3,       
    DWORD dwMovieRectWord2,       
    DWORD dwMovieWindowY,         
    DWORD dwMovieWindowX,         
    const char *pszMovieStem)     /* [ESP+0x40] -> EBX, NULL means "intro" */
{
    MsgRecord msg;
    PointRecord pt;
    DWORD      dwVersion;
    DWORD      dwCaps;
    char      *pBufEnd;
    const char *pszStem;
    extern void nwtrace_hex(const char *, unsigned long);

    nwtrace_hex("psm_in", (unsigned long)(uintptr_t)__builtin_return_address(0));

    dwVersion = pfnGetVersion();

    g_bWin9xFlag = (((dwVersion & 0x80000000UL) != 0UL) &&
                    ((dwVersion & 0xFFUL) < 4UL)) ? 1 : 0;

    g_bMediaActive      = 1;        
    g_dwMciDeviceId     = 0;        
    g_bQuitRequested    = 0;        
    g_bKeepPlaying      = 0;        
    g_bSpareC6          = 0;        
    g_bVideoCapsLatched = 0;        

    g_dwMovieRectWord3  = dwMovieRectWord3; 
    g_dwMovieRectWord2  = dwMovieRectWord2; 
    g_dwMovieWindowY    = dwMovieWindowY;   
    g_dwMovieWindowX    = dwMovieWindowX;   

    pszStem = (pszMovieStem != (const char *)0) ? pszMovieStem : "intro";

    pBufEnd = CopyNulTerminatedString(g_szMovieBasePath,
                                      (const char *)(PTRADDR)(PTRWORD)(g_pdwTileRecord + 0x38UL));
    pBufEnd = AppendCStringToDest(pBufEnd, "\\");
    AppendCStringToDest(pBufEnd, pszStem);

    PlayVideoTileMovieAndLoadBmp();
    nwtrace_hex("psm_devid", (unsigned long)g_dwMciDeviceId);

    if (g_dwMciDeviceId == 0UL) {
        nwtrace_hex("psm_ret", 0);
        g_bMediaActive = 0;                      
        return 0;                                 
    }

    if (SendMciSetOnWithPointerToGlobalWord() == 0UL) {
        goto teardown;
    }

    if (pszMovieStem != (const char *)0) {
        pfnInvalidateRect(g_hwndMain, (void *)0, 0UL);
    } else {
        pfnShowWindow(g_hwndMain, 1);
    }

    if (g_bVideoCapsLatched == 0) {
        dwCaps = pfnGetDeviceCaps(g_dwDisplayDc, 0x26);
        if ((dwCaps & 0x100UL) != 0UL) {
            pfnPostMessageA(g_hwndMain, 0x30FUL, 0UL, 0UL);
        } else {
            g_bVideoCapsLatched = 1;
        }
    }

    g_bKeepPlaying = 1;

    nwtrace_hex("psm_pump", 1);
    do {
        pfnGetMessageA(&msg, 0UL, 0UL, 0UL);
        pfnTranslateMessage(&msg);
        pfnDispatchMessageA(&msg);
    } while (g_bKeepPlaying != 0);

teardown:
    nwtrace_hex("psm_teardown", (unsigned long)g_bQuitRequested);

    PlayMciWavesCommandIfDeviceArmed(0UL, 0x804UL);
    pfnDestroyWindow(g_dwMciWindow);
    g_dwMciWindow     = 0UL;
    g_bMediaActive    = 0;

    if (g_bQuitRequested != 0) {
        pfnPostMessageA(g_hwndMain, 0x10UL, 0UL, 0UL);   /* WM_CLOSE */
    } else {
        RebuildGridPaletteAndResetExtent();
        DispatchQueuedNonInputMessages();
        /* POINT at locals+0x24 aliases the saved EBX slot; EBX is dead here. */
        pfnGetCursorPos(&pt);
        pfnSetCursorPos((int)pt.x, (int)pt.y);
    }
    nwtrace_hex("psm_ret", 1);
    return 1;                                         
}
#endif /* NW_UNIT_PlayStartupMovieWithMessagePumpAndTeardown */

/* ==========================================================================
 * Unit: PlayVideoTileMovieAndLoadBmp
 * Source: video/win32/PlayVideoTileMovieAndLoadBmp.c
 * ========================================================================== */
#ifdef NW_UNIT_PlayVideoTileMovieAndLoadBmp
/*
 * Movie/video loader for a tile whose record says its media kind is 2
 *
 * Module: video/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#video-win32-playvideotilemovieandloadbmp-c
 */

#include <stdint.h>
#include "nwglobals.h"

/* ---------------------------------------------------------------- globals */
extern const unsigned char *g_pTileImageRecord; 
extern char                g_szMovieBaseName[]; 
extern uint32_t            g_dwMciGlobalHandle; 
extern unsigned char       g_bTileMediaFlag;    

/* ------------------------------------------------- callees, same campaign */
extern int __stdcall LoadNetwalkTileImageFile(const char *pszPath);                    
extern int __stdcall DispatchMciCommandViaGlobalHandleAndShowError(uint32_t p1,       
                                                                  uint32_t p2,
                                                                  uint16_t p3);
extern int __stdcall ShowMciErrorMessage(uint32_t uErr);                              
extern int __cdecl  BuildAviBmpPathsFromSourcePath(char *pszSrcPath,                  
                                                   char *pszBmpPathOut,
                                                   char *pszAviPathOut);
extern int __stdcall FindMovieFileAcrossDrivesAndEmitBmpAviPaths(const char *pszName,
                                                                char *pszBmpPathOut,
                                                                char *pszAviPathOut);

extern uint32_t __stdcall mciSendCommandA(uint32_t idDevice,          
                                          uint32_t uCommand,
                                          uint32_t uMode,
                                          uintptr_t dwParam);
extern int      __stdcall MoveWindow(uintptr_t hWnd,                  
                                     int      nX,
                                     int      nY,
                                     int      nWidth,
                                     int      nHeight,
                                     int      bRepaint);

/* ------------------------------------------------------------- constants */
#define TILE_RECORD_KIND_VIDEO  0x02u     /* tile record byte +0x27 must equal this */

#define MCI_OPEN                0x0803u   /* mciapi.h MCI_OPEN                              */
#define MCI_STATUS              0x0814u   /* mciapi.h MCI_STATUS                            */
#define MCI_STATUS_ITEM         0x0100u   /* mciapi.h MCI_STATUS_ITEM                       */
#define RAW_OPEN_MSG_ARG        0x30200u  /* 3rd MCI_OPEN argument; no matching MCIOPEN_* name in the SDK headers */
#define RAW_OPEN_FLAG_WORD      0x40000000u /* word stored beside the .AVI pointer */
#define RAW_STATUS_ITEM_0x4001  0x4001u   /* item code stored before the status query */

/* ------------------------------------------- MCI parameter blocks (32-bit) */
typedef struct NetwalkMciOpenBlock {
    uint32_t dwUnused00;   /* frame +0x90 - never written by this function       */
    uint32_t dwDeviceId;   /* frame +0x94 - device id returned by MCI_OPEN      */
    uint32_t dwUnused08;   /* frame +0x98 - never written by this function       */
    uint32_t pszAviPath;   /* frame +0x9C - 32-bit pointer to szAviPath         */
    uint32_t dwUnused10;   /* frame +0xA0 - never written by this function       */
    uint32_t dwFlags;      
    uint32_t dwExtra;      
} NetwalkMciOpenBlock;

typedef struct NetwalkMciStatusBlock {
    uint32_t dwBlockAddr;  /* frame +0xAC - address of the block itself          */
    uint32_t dwReply;      /* frame +0xB0 - driver's dword reply, used as HWND   */
    uint32_t dwItem;       /* frame +0xB4 - 0x4001                              */
} NetwalkMciStatusBlock;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(NetwalkMciOpenBlock) == 0x1C, "open block must cover frame +0x90..+0xAB");
_Static_assert(sizeof(NetwalkMciStatusBlock) == 0x0C, "status block must cover frame +0xAC..+0xB7");
#endif

/* ======================================================================= */
void __cdecl PlayVideoTileMovieAndLoadBmp(void)
{
    const unsigned char  *pRecord  = g_pTileImageRecord;            
    unsigned char         byFlag26 = pRecord[0x26];                  
    unsigned char         byKind27 = pRecord[0x27];                  
    char                  szAviPath[0x48];                          /* frame +0x00 */
    char                  szBmpPath[0x48];                          /* frame +0x48 */
    NetwalkMciOpenBlock   open;                                     /* frame +0x90 */
    NetwalkMciStatusBlock status;                                   /* frame +0xAC */

    g_bTileMediaFlag = byFlag26;                                    

#ifdef NW_PLATFORM_SDL
    /* The SDL backend has no MCI transport; tile media is optional and the
     * built-in frame-group startup path remains the fallback. */
    (void)byKind27;
    return;
#else
    if (byKind27 != TILE_RECORD_KIND_VIDEO)                         
        return;                                                     

    if (BuildAviBmpPathsFromSourcePath(g_szMovieBaseName,
                                       szBmpPath,                   /* arg2 -> frame +0x48 */
                                       szAviPath) == 0 &&           /* arg3 -> frame +0x00 */
        FindMovieFileAcrossDrivesAndEmitBmpAviPaths(g_szMovieBaseName,
                                                    szBmpPath,
                                                    szAviPath) == 0)
        return;                                                     

    open.dwUnused00 = 0;                                            /* frame +0x90 */
    open.dwUnused08 = 0;                                            /* frame +0x98 */
    open.dwUnused10 = 0;                                            /* frame +0xA0 */
    open.pszAviPath = (uint32_t)(uintptr_t)szAviPath;               /* frame +0x9C */
    open.dwFlags    = RAW_OPEN_FLAG_WORD;                           /* frame +0xA4 */
    open.dwExtra    = g_dwOpenExtraSource;                          /* frame +0xA8 */

    if (ShowMciErrorMessage(mciSendCommandA(0u,                    /* idDevice = 0    */
                                            MCI_OPEN,             /* uMsg  = 0x0803  */
                                            RAW_OPEN_MSG_ARG,     /* dwFlags, raw     */
                                            (uintptr_t)&open)    /* dwParam          */) == 0)
        return;                                                     

    g_dwMciGlobalHandle = open.dwDeviceId;                          /* frame +0x94 */

    status.dwBlockAddr = (uint32_t)(uintptr_t)&status;              /* frame +0xAC */
    status.dwItem      = RAW_STATUS_ITEM_0x4001;                    /* frame +0xB4 */
    (void)DispatchMciCommandViaGlobalHandleAndShowError((uintptr_t)&status,
                                                        MCI_STATUS_ITEM,  /* 0x0100 */
                                                        MCI_STATUS);      /* 0x0814 */
    g_dwMciStatusReply = status.dwReply;                            /* frame +0xB0, used as HWND */

    (void)MoveWindow((uintptr_t)status.dwReply, g_childX, g_childY, 0, 0, 0);

    LoadNetwalkTileImageFile(szBmpPath);
#endif
}
#endif /* NW_UNIT_PlayVideoTileMovieAndLoadBmp */
