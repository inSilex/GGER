/*
 * PathHelpers -- Win32 path construction, deletion, and drive selection.
 *
 * Module: io/win32
 * Interface: include/PathHelpers.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   BuildHlpSiblingPath                                        io/win32/BuildHlpSiblingPath.c
 *   BuildTempPathForSourceFile                                 io/win32/BuildTempPathForSourceFile.c
 *   DeleteExeVariantFileWithTempFallback                       io/win32/DeleteExeVariantFileWithTempFallback.c
 *   DeleteFileAndCacheErrno                                    io/win32/DeleteFileAndCacheErrno.c
 *   FindBestAvailableDriveAndEmitLabel                         io/win32/FindBestAvailableDriveAndEmitLabel.c
 */

/* ==========================================================================
 * Unit: BuildHlpSiblingPath
 * Source: io/win32/BuildHlpSiblingPath.c
 * ========================================================================== */
#ifdef NW_UNIT_BuildHlpSiblingPath
/*
 * Derives the WinHelp sibling path by replacing the source extension with .HLP.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-buildhlpsiblingpath-c
 */

#include <stdint.h>

extern char *CopyNulTerminatedString(char *pszDst, const char *pszSrc);        
extern char *FindLastCharInString(const char *pszSrc, unsigned char chTarget); 
extern char *AppendCStringToDest(char *pDestBuf, const char *pszSrc);          

extern char g_pszHelpFilePath[];        

extern const char g_szHlpExtension[];   

/* Derives the ".HLP" sibling path of pszSrcPath (typically the module path
   returned by GetModuleFileNameA) into the global buffer: copy, cut at the last
   '.', append ".HLP". No return value; the result is reachable only via the
   global.

   HAZARD (faithfully reproduced, not fixed): when pszSrcPath contains NO '.',
   FindLastCharInString returns NULL and the following NUL store writes to
   address 0, so the binary faults on such an input. */

void __stdcall BuildHlpSiblingPath(const char *) __asm__("_BuildHlpSiblingPath");
void __stdcall BuildHlpSiblingPath(const char *pszSrcPath)
{
    char *pszExtension;                                            /* strrchr result: last '.' or NULL */

    CopyNulTerminatedString(g_pszHelpFilePath, pszSrcPath);        /* strcpy(buf, src)               */
    pszExtension = FindLastCharInString(g_pszHelpFilePath, '.');   /* strrchr(buf, '.')              */
    *pszExtension = '\0';                                          /* truncate, removing the '.'     */
    AppendCStringToDest(g_pszHelpFilePath, g_szHlpExtension);      /* strcat(buf, ".HLP")            */
}
#endif /* NW_UNIT_BuildHlpSiblingPath */

/* ==========================================================================
 * Unit: BuildTempPathForSourceFile
 * Source: io/win32/BuildTempPathForSourceFile.c
 * ========================================================================== */
#ifdef NW_UNIT_BuildTempPathForSourceFile
/*
 * Builds a temp path from the source file leaf name.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-buildtemppathforsourcefile-c
 */

#include <windows.h>

extern char *FindLastCharInString(const char *pszSrc, unsigned char chTarget);

static void nw_collapse_path_separators(char *pszPath)
{
    char *p = pszPath;
    char *q = pszPath;

    while (*p != '\0') {
        if ((*p == '/' || *p == '\\') && q > pszPath &&
            (q[-1] == '/' || q[-1] == '\\')) {
            p++;
            continue;
        }
        *q++ = *p++;
    }
    *q = '\0';
}

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern int platform_save_path(const char *source_file_path, char *out,
                              size_t out_size);
#endif

void __stdcall BuildTempPathForSourceFile(char *pszSrcPath, LPSTR pszDestBuf)
{
    CHAR szTempPath[128];   /* 0x80-byte stack buffer filled by GetTempPathA */
    char *pcLeaf;           /* tail of pszSrcPath beginning at its last separator */

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
    /* The console save is a game slot, not a scratch file.  Resolve its leaf
     * under the persistent save directory before considering GETTEMP. */
    if (platform_save_path(pszSrcPath, pszDestBuf, 128)) {
        nw_collapse_path_separators(pszDestBuf);
        return;
    }
#endif

    GetTempPathA(0x80, szTempPath);
#ifdef NW_PLATFORM_SDL
    /* The portable host still speaks Windows-style paths (Z:\...), while a
     * save variant may be a bare relative name.  Accept either separator so
     * the temp fallback gets the leaf rather than the whole path. */
    {
        char *pcBack = FindLastCharInString(pszSrcPath, '\\');

        pcLeaf = FindLastCharInString(pszSrcPath, '/');
        if (pcBack != NULL && (pcLeaf == NULL || pcBack > pcLeaf))
            pcLeaf = pcBack;
    }
    wsprintfA(pszDestBuf, "%s/%s", szTempPath,
              (pcLeaf != NULL) ? pcLeaf : pszSrcPath);
#else
    pcLeaf = FindLastCharInString(pszSrcPath, '\\');
    wsprintfA(pszDestBuf, "%s\\%s", szTempPath, pcLeaf);
#endif
    nw_collapse_path_separators(pszDestBuf);
}
#endif /* NW_UNIT_BuildTempPathForSourceFile */

/* ==========================================================================
 * Unit: DeleteExeVariantFileWithTempFallback
 * Source: io/win32/DeleteExeVariantFileWithTempFallback.c
 * ========================================================================== */
#ifdef NW_UNIT_DeleteExeVariantFileWithTempFallback
/*
 * Deletes an EXE variant file, retrying in the temp directory.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-deleteexevariantfilewithtempfallback-c
 */

#include <windows.h>
#include <stdint.h>

extern void __stdcall BuildExeVariantNameFromBaseName(int      nVariantNumber,
                                                     LPCSTR   pszSuffix,
                                                     LPCSTR   pszSourcePath,
                                                     char    *szOutputName);
extern int  __cdecl DeleteFileAndCacheErrno(LPCSTR pszFilename);
extern void __stdcall BuildTempPathForSourceFile(char  *pszSourcePath,
                                                 LPSTR pszDestBuf);
#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
extern int platform_delete_save_path(const char *source_file_path);
#endif

int __stdcall DeleteExeVariantFileWithTempFallback(int    nVariantNumber,
                                                   LPCSTR pszSuffix,
                                                   LPCSTR pszSourcePath)
{
    /* Two separate 128-byte buffers: the SUB ESP,0x100 frame is split
     * between them, not one 256-byte object. Both are zeroed only to
     * silence -Wmaybe-uninitialized; each is fully written by its
     * callee before it is read. */
    char szVariantName[128] = { 0 };   /* E0-0x100 */
    char szTempName[128]    = { 0 };   /* E0-0x80  */

    BuildExeVariantNameFromBaseName(nVariantNumber, pszSuffix,
                                    pszSourcePath, szVariantName);

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
    /* Remove both the new persistent save and any legacy copies so a
     * deleted slot cannot be resurrected by save migration on the next run. */
    if (platform_delete_save_path(szVariantName)) {
        return 1;
    }
#endif

    /* 0 means "deleted" -- success. Any nonzero result means the file
     * was not where we expected, so try the temp-directory name. */
    if (DeleteFileAndCacheErrno(szVariantName) == 0) {
        return 1;
    }

    /* Same leaf fragment, resynthesised under GetTempPathA. */
    BuildTempPathForSourceFile(szVariantName, szTempName);

    if (DeleteFileAndCacheErrno(szTempName) == 0) {
        return 1;
    }

    return 0;
}
#endif /* NW_UNIT_DeleteExeVariantFileWithTempFallback */

/* ==========================================================================
 * Unit: DeleteFileAndCacheErrno
 * Source: io/win32/DeleteFileAndCacheErrno.c
 * ========================================================================== */
#ifdef NW_UNIT_DeleteFileAndCacheErrno
/*
 * Deletes the file named by pszFilename through DeleteFileA; on failure caches the translated errno in the calling thread's context record
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-deletefileandcacheerrno-c
 */

#include <windows.h>

extern DWORD __fastcall TranslateLastErrorToErrno(DWORD dwUnusedArg1,
                                                  DWORD dwUnusedArg2);

int __cdecl DeleteFileAndCacheErrno(LPCSTR pszFilename)
{
    if (DeleteFileA(pszFilename) != 0) {
        return 0;                       /* file deleted */
    }

    TranslateLastErrorToErrno(0, 0);    /* caches GetLastError() -> errno in the context */

    return -1;                          /* the callee's return value is deliberately dropped */
}
#endif /* NW_UNIT_DeleteFileAndCacheErrno */

/* ==========================================================================
 * Unit: FindBestAvailableDriveAndEmitLabel
 * Source: io/win32/FindBestAvailableDriveAndEmitLabel.c
 * ========================================================================== */
#ifdef NW_UNIT_FindBestAvailableDriveAndEmitLabel
/*
 * Selects a drive by preferred type and emits its letter.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-findbestavailabledriveandemitlabel-c
 */

#include <windows.h>
#include <stdint.h>

// GetDriveTypeA return codes: 0=UNKNOWN 1=NO_ROOT_DIR 2=REMOVABLE 3=FIXED 4=REMOTE 5=CDROM
// 6=RAMDISK.
extern int FindFirstDriveTypeMatchAndEmitLetter(char           bDriveType,
                                                unsigned char *pbDriveTypeTable,
                                                char          *szLabelOut);

int FindBestAvailableDriveAndEmitLabel(unsigned char *pbDriveTypeTable, char *szLabelOut)
{
    /* Attempt 1 (immediate 0x3): a fixed (hard) disk is the most preferred. */
    if (FindFirstDriveTypeMatchAndEmitLetter(DRIVE_FIXED,   pbDriveTypeTable, szLabelOut))
        return 1;

    /* Attempt 2 (immediate 0x5): a CD-ROM, the usual game-media carrier. */
    if (FindFirstDriveTypeMatchAndEmitLetter(DRIVE_CDROM,   pbDriveTypeTable, szLabelOut))
        return 1;

    /* Attempt 3 (immediate 0x4): a network (remote) drive. */
    if (FindFirstDriveTypeMatchAndEmitLetter(DRIVE_REMOTE,  pbDriveTypeTable, szLabelOut))
        return 1;

    /* Attempt 4 (immediate 0x6): a RAM disk, the last resort. */
    if (FindFirstDriveTypeMatchAndEmitLetter(DRIVE_RAMDISK, pbDriveTypeTable, szLabelOut))
        return 1;

    /* No drive of any of the four wanted types exists. The label buffer was never written. */
    return 0;
}
#endif /* NW_UNIT_FindBestAvailableDriveAndEmitLabel */
