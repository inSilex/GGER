#include "nwarch.h"
/*
 * FileSlots -- tracked file slots and save-variant path selection.
 *
 * Module: io
 * Interface: include/FileSlots.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   BuildExeVariantNameFromBaseName                            io/BuildExeVariantNameFromBaseName.c
 *   CallOpenTrackedHandleSlotWithFixedModeBits                 io/CallOpenTrackedHandleSlotWithFixedModeBits.c
 *   CloseGlobalSlotAndResetToUnused                            io/CloseGlobalSlotAndResetToUnused.c
 *   DeleteNetWalkSaveVariantByIndex                            io/DeleteNetWalkSaveVariantByIndex.c
 *   FindFirstDriveTypeMatchAndEmitLetter                       io/FindFirstDriveTypeMatchAndEmitLetter.c
 *   GetTrackedSlotFilePosition                                 io/GetTrackedSlotFilePosition.c
 *   OpenTempPathOrExeVariantSourceFile                         io/OpenTempPathOrExeVariantSourceFile.c
 *   OpenTrackedSlotBinaryAndStoreResult                        io/OpenTrackedSlotBinaryAndStoreResult.c
 *   RebaseInlineImageRecord                                    io/RebaseInlineImageRecord.c
 *   RewindGlobalSlotFileToStart                                io/RewindGlobalSlotFileToStart.c
 *   SeekTrackedHandleAbsoluteOffset                            io/SeekTrackedHandleAbsoluteOffset.c
 *   TruncateGlobalSlotFileToZeroLength                         io/TruncateGlobalSlotFileToZeroLength.c
 *   UpdateCachedSlotStatus                                     io/UpdateCachedSlotStatus.c
 */

/* ==========================================================================
 * Unit: BuildExeVariantNameFromBaseName
 * Source: io/BuildExeVariantNameFromBaseName.c
 * ========================================================================== */
#ifdef NW_UNIT_BuildExeVariantNameFromBaseName
/*
 * Build Exe Variant Name From Base Name.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-buildexevariantnamefrombasename-c
 */

#include <windows.h>
#include <stdint.h>
extern unsigned char nw_data_window[];

#define VARIANT_NAME_FORMAT   ((const char *)(nw_data_window + 0xa38))
#define EXE_SUFFIX_LITERAL    ((const char *)(nw_data_window + 0xa40))

extern char *__cdecl CopyNulTerminatedString(char *pszDst, const void *pszSrc);
extern char *__cdecl ConvertAsciiStringToUpperCase(char *pszString);
extern char *__cdecl FindFirstSubstringInString(const char *pszHaystack,
                                                const char *pszNeedle);

void __stdcall BuildExeVariantNameFromBaseName(int   nVariantNumber,
                                             const char *pszSuffix,
                                             const char *pszSourcePath,
                                             char *szOutputName)
{
    /* Zero-initialised only to keep the compiler quiet about passing the
     * address of an uninitialised array; CopyNulTerminatedString writes a
     * NUL-terminated string over it before anything reads it.
     *
     * The original stack slot is 128 bytes.  A native install path can be
     * longer than that, which truncates the ".EXE" leaf and made the original
     * unconditional `*suffix = 0` fault at eip 0x402f92.  Keep a full Path
     * buffer and make the suffix optional instead of dereferencing NULL. */
    char  szUpperBase[512] = { 0 };
    char *pszExeSuffix;
    char *p;
    char *q;

    /* In place: ConvertAsciiStringToUpperCase returns its own argument.
     * Windows paths are case-insensitive; portable hosts are not, so keep the
     * original spelling there or the save-open path will not resolve. */
    CopyNulTerminatedString(szUpperBase, pszSourcePath);
#ifndef NW_PLATFORM_SDL
    ConvertAsciiStringToUpperCase(szUpperBase);
#endif

    pszExeSuffix = FindFirstSubstringInString(szUpperBase, EXE_SUFFIX_LITERAL);
    if (pszExeSuffix == NULL) {
        /* A long install path can lose the executable leaf.  Fall back to a
         * local variant name; the save/open path uses this stem. */
        CopyNulTerminatedString(szUpperBase, "NETWALK.EXE");
        pszExeSuffix = FindFirstSubstringInString(szUpperBase,
                                                  EXE_SUFFIX_LITERAL);
    }
    if (pszExeSuffix != NULL)
        *pszExeSuffix = '\0';

    wsprintfA(szOutputName, VARIANT_NAME_FORMAT,
              szUpperBase, nVariantNumber, pszSuffix);

    /* The native title supplies a data home ending in a backslash while the
     * game's variant format inserts a forward slash.  The resulting "\\/"
     * sequence is accepted by Win32 but not by every console path resolver;
     * collapse adjacent separators so the save open does not fail with 1022. */
    p = q = szOutputName;
    while (*p != '\0') {
        if ((*p == '/' || *p == '\\') && q > szOutputName &&
            (q[-1] == '/' || q[-1] == '\\')) {
            p++;
            continue;
        }
        *q++ = *p++;
    }
    *q = '\0';
}
#endif /* NW_UNIT_BuildExeVariantNameFromBaseName */

/* ==========================================================================
 * Unit: CallOpenTrackedHandleSlotWithFixedModeBits
 * Source: io/CallOpenTrackedHandleSlotWithFixedModeBits.c
 * ========================================================================== */
#ifdef NW_UNIT_CallOpenTrackedHandleSlotWithFixedModeBits
/*
 * Call Open Tracked Handle Slot With Fixed Mode Bits.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-callopentrackedhandleslotwithfixedmodebits-c
 */

#include <windows.h>

extern int OpenTrackedHandleSlot(const char *pszFilename, DWORD dwAccessWord, DWORD dwModeBits70src, DWORD dwAttrInput);

extern void nwtrace_hex(const char *tag, unsigned long v);

int CallOpenTrackedHandleSlotWithFixedModeBits(const char *pszFilename, DWORD dwAccessWord, DWORD dwAttrInput)
{
    /* push#4 pszFilename (ARG1), push#3 dwAccessWord (ARG2), push#2 0x40, push#1 dwAttrInput (ARG3). */
    return OpenTrackedHandleSlot(pszFilename, dwAccessWord, 0x40u, dwAttrInput);
}
#endif /* NW_UNIT_CallOpenTrackedHandleSlotWithFixedModeBits */

/* ==========================================================================
 * Unit: CloseGlobalSlotAndResetToUnused
 * Source: io/CloseGlobalSlotAndResetToUnused.c
 * ========================================================================== */
#ifdef NW_UNIT_CloseGlobalSlotAndResetToUnused
/*
 * Close Global Slot And Reset To Unused.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-closeglobalslotandresettounused-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

#define GLOBAL_SLOT_INDEX_VAR (nw_data_window + 0x5f58)

extern int __fastcall CloseTrackedHandleSlot(DWORD dwUnusedArg1,
                                             DWORD dwUnusedArg2,
                                             int   nSlotIndex);

#define g_nGlobalSlotIndex (*(int *)GLOBAL_SLOT_INDEX_VAR)

void __fastcall CloseGlobalSlotAndResetToUnused(DWORD dwUnusedArg1,
                                                 DWORD dwUnusedArg2)
{
    int nSlotIndex = g_nGlobalSlotIndex;

    if (nSlotIndex > 0) {
        (void)CloseTrackedHandleSlot(dwUnusedArg1, dwUnusedArg2, nSlotIndex);
        g_nGlobalSlotIndex = -1;
    }
}

#if !defined(__ELF__) && !defined(_WIN64)
__asm__(".globl _CloseGlobalSlotAndResetToUnused\n" ".set _CloseGlobalSlotAndResetToUnused, @CloseGlobalSlotAndResetToUnused@8\n");
#endif
#define _FIXABI 1
#endif /* NW_UNIT_CloseGlobalSlotAndResetToUnused */

/* ==========================================================================
 * Unit: DeleteNetWalkSaveVariantByIndex
 * Source: io/DeleteNetWalkSaveVariantByIndex.c
 * ========================================================================== */
#ifdef NW_UNIT_DeleteNetWalkSaveVariantByIndex
/*
 * Delete Net Walk Save Variant By Index.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-deletenetwalksavevariantbyindex-c
 */

#include <windows.h>
#include <stdint.h>

extern int __stdcall DeleteExeVariantFileWithTempFallback(int   nVariantNumber,
                                                          LPCSTR pszSuffix,
                                                          LPCSTR pszSourcePath);

extern char g_szSaveVariantSuffix[4]; 

extern char g_szRuntimeSourcePath[]; 

void __stdcall DeleteNetWalkSaveVariantByIndex(int nVariantNumber)
{
    DeleteExeVariantFileWithTempFallback(nVariantNumber,
                                         g_szSaveVariantSuffix,
                                         g_szRuntimeSourcePath);
    /* result deliberately dropped -- no EAX handling in the body */
}
#endif /* NW_UNIT_DeleteNetWalkSaveVariantByIndex */

/* ==========================================================================
 * Unit: FindFirstDriveTypeMatchAndEmitLetter
 * Source: io/FindFirstDriveTypeMatchAndEmitLetter.c
 * ========================================================================== */
#ifdef NW_UNIT_FindFirstDriveTypeMatchAndEmitLetter
/*
 * Find First Drive Type Match And Emit Letter.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-findfirstdrivetypematchandemitletter-c
 */

#include <windows.h>
#include <stdint.h>

int FindFirstDriveTypeMatchAndEmitLetter(char           bDriveType,
                                         unsigned char *pbDriveTypeTable,
                                         char          *szLabelOut)
{
    int iIndex;

    for (iIndex = 0; iIndex < 0x20; iIndex++) {
        if (pbDriveTypeTable[iIndex] == (unsigned char)bDriveType) {
            pbDriveTypeTable[iIndex] = 0;              /* clear (consume) the matched slot */
            szLabelOut[0] = (char)('A' + iIndex);      /* drive letter, e.g. 'C'           */
            szLabelOut[1] = ':';                       /* ':' == 0x3a                      */
            szLabelOut[2] = '\0';                      /* NUL terminator                   */
            return 1;                                   /* only AL is defined on this path  */
        }
    }

    return 0;                                           /* nothing written on a miss        */
}
#endif /* NW_UNIT_FindFirstDriveTypeMatchAndEmitLetter */

/* ==========================================================================
 * Unit: GetTrackedSlotFilePosition
 * Source: io/GetTrackedSlotFilePosition.c
 * ========================================================================== */
#ifdef NW_UNIT_GetTrackedSlotFilePosition
/*
 * Returns the current OS file position of the file held in the program-global tracked slot index g_nFileSlot.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-gettrackedslotfileposition-c
 */

#include <windows.h>
#include "nwglobals.h"

extern DWORD __cdecl SeekTrackedSlotHandleOffset(int   nSlotIndex,
                                                 LONG  lDistanceToMove,
                                                 DWORD dwMoveMethod);


DWORD __cdecl GetTrackedSlotFilePosition(void)
{
    /* Distance 0 + FILE_CURRENT: query the current position, do not move it. */
    return SeekTrackedSlotHandleOffset(g_nFileSlot, 0, FILE_CURRENT);
}
#endif /* NW_UNIT_GetTrackedSlotFilePosition */

/* ==========================================================================
 * Unit: OpenTempPathOrExeVariantSourceFile
 * Source: io/OpenTempPathOrExeVariantSourceFile.c
 * ========================================================================== */
#ifdef NW_UNIT_OpenTempPathOrExeVariantSourceFile
/*
 * Open Temp Path Or Exe Variant Source File.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-opentemppathorexevariantsourcefile-c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include "nwglobals.h"

typedef uint32_t DWORD;
typedef const char *LPCSTR;

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
/* Save/open diagnostics happen after the boot screen is gone.  Console
 * builds have no log files; markers go to klog/stderr only. */
extern void platform_log_mark(const char *stage);
extern int platform_save_path(const char *source_file_path, char *out,
                              size_t out_size);
extern void platform_save_path_failed(void);
static void nw_save_markf(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    platform_log_mark(line);
}
#else
#define nw_save_markf(...) ((void)0)
#endif

extern void __stdcall BuildExeVariantNameFromBaseName(int   nVariantNumber,
                                                      const char *pszSuffix,
                                                      const char *pszSourcePath,
                                                      char  *szOutputName);
extern void __stdcall BuildTempPathForSourceFile(char *pszSrcPath,
                                                 char *pszDestBuf);
extern int  CallOpenTrackedHandleSlotWithFixedModeBits(const char *pszFilename,
                                                       DWORD dwAccessWord,
                                                       DWORD dwAttrInput);

/* PUSH 0x8102 -> O_BINARY|O_CREAT|O_RDWR: the variant file is CREATED here.
 * PUSH 0x180  -> FILE_ATTRIBUTE_TEMPORARY|FILE_ATTRIBUTE_NORMAL. */
#define OPEN_ACCESS_WORD 0x8102u
#define OPEN_ATTR_INPUT  0x180u

int __stdcall OpenTempPathOrExeVariantSourceFile(int    nVariantNumber,
                                                 LPCSTR pszSuffix,
                                                 LPCSTR pszSourcePath)
{

    char szVariantName[128] = { 0 };
    char szTempName[128]    = { 0 };
    int  iSlot;

    BuildExeVariantNameFromBaseName(nVariantNumber, pszSuffix, pszSourcePath,
                                    szVariantName);

#if defined(NW_PORTABLE_PS5) || defined(NW_PS5_NATIVE_TITLE)
    /* High scores belong in the console's persistent save directory, not in
     * the read-only application image or a transient current directory.  Try
     * every native candidate before the legacy module-relative location, so a
     * writable current directory cannot hide the intended fallback. */
    for (;;) {
        if (!platform_save_path(szVariantName, szTempName, sizeof szTempName)) {
            break;
        }
        nw_save_markf("save_path_%s", szTempName);
        iSlot = CallOpenTrackedHandleSlotWithFixedModeBits(szTempName,
                                                           OPEN_ACCESS_WORD,
                                                           OPEN_ATTR_INPUT);
        g_dwSourceFileOpenSlot = (DWORD)iSlot;
        nw_save_markf("save_path_open_%d_err_%lu", iSlot,
                      (unsigned long)GetLastError());
        if (iSlot > 0) {
            return 1;
        }
        platform_save_path_failed();
    }
#endif

    nw_save_markf("save_variant_%s", szVariantName);
    iSlot = CallOpenTrackedHandleSlotWithFixedModeBits(szVariantName,
                                                       OPEN_ACCESS_WORD,
                                                       OPEN_ATTR_INPUT);
    g_dwSourceFileOpenSlot = (DWORD)iSlot;
    nw_save_markf("save_variant_open_%d_err_%lu", iSlot,
                  (unsigned long)GetLastError());

    if (iSlot <= 0) {
        BuildTempPathForSourceFile(szVariantName, szTempName);
        nw_save_markf("save_temp_%s", szTempName);
        iSlot = CallOpenTrackedHandleSlotWithFixedModeBits(szTempName,
                                                           OPEN_ACCESS_WORD,
                                                           OPEN_ATTR_INPUT);
        g_dwSourceFileOpenSlot = (DWORD)iSlot;
        nw_save_markf("save_temp_open_%d_err_%lu", iSlot,
                      (unsigned long)GetLastError());
    }

    return ((int)g_dwSourceFileOpenSlot > 0) ? 1 : 0;
}
#endif /* NW_UNIT_OpenTempPathOrExeVariantSourceFile */

/* ==========================================================================
 * Unit: OpenTrackedSlotBinaryAndStoreResult
 * Source: io/OpenTrackedSlotBinaryAndStoreResult.c
 * ========================================================================== */
#ifdef NW_UNIT_OpenTrackedSlotBinaryAndStoreResult
/*
 * Open Tracked Slot Binary And Store Result.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-opentrackedslotbinaryandstoreresult-c
 */

#include <windows.h>
#include "nwglobals.h"

extern int CallOpenTrackedHandleSlotWithFixedModeBits(const char *pszFilename, DWORD dwAccessWord, DWORD dwAttrInput);

#define g_dwLastTrackedOpenResult (*(DWORD *)(nw_data_window + 0x5F54))

extern void nwtrace_hex(const char *tag, unsigned long v);

int __stdcall OpenTrackedSlotBinaryAndStoreResult(const char *pszFilename)
{
    /* The machine passes OUR return address as dwAttrInput (the callee's [S+0xC] slot); reproduce it. */
    int iTrackedResult = CallOpenTrackedHandleSlotWithFixedModeBits(pszFilename, 0x8000u,
                                                                     (DWORD)(uintptr_t)__builtin_return_address(0));

    g_dwLastTrackedOpenResult = (DWORD)iTrackedResult;

    /* NEG / SBB / SHR: 1 iff the callee returned a strictly positive value; 0 for 0 and for all negatives. */
    return (iTrackedResult > 0) ? 1 : 0;
}
#endif /* NW_UNIT_OpenTrackedSlotBinaryAndStoreResult */

/* ==========================================================================
 * Unit: RebaseInlineImageRecord
 * Source: io/RebaseInlineImageRecord.c
 * ========================================================================== */
#ifdef NW_UNIT_RebaseInlineImageRecord
/*
 * Rebase Inline Image Record.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-rebaseinlineimagerecord-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define NWP32(off) (*(uint32_t *)(intptr_t)(nw_data_window + (off)))

NW_DIAG uint8_t * __cdecl RebaseInlineImageRecordAndLoadCursor(void);

NW_DIAG uint8_t * __cdecl RebaseInlineImageRecordAndLoadCursor(void)
{
    uint8_t *pCursor = NW_WINDOW_PTR_REF(uint8_t, 0x16d4);
    uint32_t dbase;

    if (*(uint32_t *)(void *)(pCursor + 4) == 0x4469736bu) { /* "Disk" */
        dbase = NWP32(0x16bc);
        if (dbase > 0x12800u) {
            *(uint32_t *)(void *)(pCursor + 8) += dbase - 0x12800u;
        }
    }
    return pCursor;
}

__asm__(".globl RebaseInlineImageRecordAndLoadCursor\n"
        ".set RebaseInlineImageRecordAndLoadCursor, "
        "_RebaseInlineImageRecordAndLoadCursor\n");
#endif /* NW_UNIT_RebaseInlineImageRecord */

/* ==========================================================================
 * Unit: RewindGlobalSlotFileToStart
 * Source: io/RewindGlobalSlotFileToStart.c
 * ========================================================================== */
#ifdef NW_UNIT_RewindGlobalSlotFileToStart
/*
 * Rewind Global Slot File To Start.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-rewindglobalslotfiletostart-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

extern DWORD __cdecl SeekTrackedSlotHandleOffset(int   nSlotIndex,
                                                 LONG  lDistanceToMove,
                                                 DWORD dwMoveMethod);

#define GLOBAL_SLOT_INDEX_VAR ((DWORD *)(nw_data_window + 0x5f58))

DWORD __cdecl RewindGlobalSlotFileToStart(void)
{
    DWORD dwSlotIndex;
    DWORD dwNewFilePos;

    dwSlotIndex = *GLOBAL_SLOT_INDEX_VAR;

    
    dwNewFilePos = SeekTrackedSlotHandleOffset((int)dwSlotIndex,
                                               0L,
                                               FILE_CURRENT);
    return dwNewFilePos;
}
#endif /* NW_UNIT_RewindGlobalSlotFileToStart */

/* ==========================================================================
 * Unit: SeekTrackedHandleAbsoluteOffset
 * Source: io/SeekTrackedHandleAbsoluteOffset.c
 * ========================================================================== */
#ifdef NW_UNIT_SeekTrackedHandleAbsoluteOffset
/*
 * Seek Tracked Handle Absolute Offset.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-seektrackedhandleabsoluteoffset-c
 */

#include <windows.h>
#include "nwglobals.h"

extern DWORD __cdecl SeekTrackedSlotHandleOffset(int   nSlotIndex,
                                                 LONG  lDistanceToMove,
                                                 DWORD dwMoveMethod);

#define TRACKED_SLOT_INDEX_PTR  ((void *)(nw_data_window + 0x5F58))  
#define SETFILEPOINTER_INVALID  ((DWORD)0xffffffffL)         /* seek-failure return   */

BOOL __stdcall SeekTrackedHandleAbsoluteOffset(LONG lDistanceToMove)
{
    int   nTrackedSlotIndex;
    DWORD dwNewFilePos;

    nTrackedSlotIndex = *(int *)TRACKED_SLOT_INDEX_PTR;

    dwNewFilePos = SeekTrackedSlotHandleOffset(nTrackedSlotIndex,
                                               lDistanceToMove,
                                               FILE_BEGIN);   /* PUSH 0x0: absolute */

    return (dwNewFilePos != SETFILEPOINTER_INVALID) ? TRUE : FALSE;
}
#endif /* NW_UNIT_SeekTrackedHandleAbsoluteOffset */

/* ==========================================================================
 * Unit: TruncateGlobalSlotFileToZeroLength
 * Source: io/TruncateGlobalSlotFileToZeroLength.c
 * ========================================================================== */
#ifdef NW_UNIT_TruncateGlobalSlotFileToZeroLength
/*
 * Truncate Global Slot File To Zero Length.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-truncateglobalslotfiletozerolength-c
 */

#include <windows.h>
extern unsigned char nw_data_window[];

extern int __cdecl TruncateTrackedSlotFileToSize(int   nSlotIndex,
                                                  DWORD dwNewSize);

#define GLOBAL_SLOT_INDEX_VAR (nw_data_window + 0x5f58)

int __cdecl TruncateGlobalSlotFileToZeroLength(void)
{
    int nSlotIndex;

    nSlotIndex = *(int *)GLOBAL_SLOT_INDEX_VAR;

    return TruncateTrackedSlotFileToSize(nSlotIndex, 0);
}
#endif /* NW_UNIT_TruncateGlobalSlotFileToZeroLength */

/* ==========================================================================
 * Unit: UpdateCachedSlotStatus
 * Source: io/UpdateCachedSlotStatus.c
 * ========================================================================== */
#ifdef NW_UNIT_UpdateCachedSlotStatus
/*
 * Update Cached Slot Status.
 *
 * Module: io
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-updatecachedslotstatus-c
 */

#include <stdint.h>
#include "nwglobals.h"

extern uint32_t CallGmwDispatch206(void);
extern int __stdcall ReadStreamBlockRawOrLzss(uint32_t cbUnusedBlockSize,
                                              uint8_t *pbDestBuf);
extern int8_t __stdcall NwPublishAudioSlotAndMix(int arg1, int arg2,
                                                 int arg3,
                                                 const void *arg4);

int UpdateCachedSlotStatus(void)
{
    int result;

    if (*(uint8_t *)(void *)(nw_data_window + 0x16f9) != 0)
        (void)CallGmwDispatch206();

    if (ReadStreamBlockRawOrLzss(
            *(uint32_t *)(void *)(nw_data_window + 0x1728),
            NW_WINDOW_PTR_REF(uint8_t, 0x172c)) != 0) {
        result = (int)NwPublishAudioSlotAndMix(
            -127, 0, -1,
            NW_WINDOW_PTR_REF(void, 0x172c));
    } else {
        result = 0;
    }

    *(uint8_t *)(void *)(nw_data_window + 0x1738) = (uint8_t)result;
    return result;
}
#endif /* NW_UNIT_UpdateCachedSlotStatus */
