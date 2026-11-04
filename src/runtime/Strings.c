/*
 * Strings -- C string, character, number, and format parsing helpers.
 *
 * Module: runtime
 * Interface: include/Strings.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   AppendCStringToDest                                        runtime/AppendCStringToDest.c
 *   CompareBoundedAsciiStringsNoCase                           runtime/CompareBoundedAsciiStringsNoCase.c
 *   ConvertAsciiCharToLowerCase                                runtime/ConvertAsciiCharToLowerCase.c
 *   ConvertAsciiCharToUpperCase                                runtime/ConvertAsciiCharToUpperCase.c
 *   ConvertAsciiStringToUpperCase                              runtime/ConvertAsciiStringToUpperCase.c
 *   ConvertIntegerToString                                     runtime/ConvertIntegerToString.c
 *   CopyBoundedStringNulPadded                                 runtime/CopyBoundedStringNulPadded.c
 *   CopyNulTerminatedString                                    runtime/CopyNulTerminatedString.c
 *   FindFirstCharInString                                      runtime/FindFirstCharInString.c
 *   FindFirstSubstringInString                                 runtime/FindFirstSubstringInString.c
 *   FindLastCharInString                                       runtime/FindLastCharInString.c
 *   GetStringLength                                            runtime/GetStringLength.c
 *   ParseBracketCharClassAndMatchChars                         runtime/ParseBracketCharClassAndMatchChars.c
 *   ParseHmsTimeStringToSignedSeconds                          runtime/ParseHmsTimeStringToSignedSeconds.c
 *   ParseSignedDecimalIntegerFromString                        runtime/ParseSignedDecimalIntegerFromString.c
 *   ParseSignedLongFromStringBaseN                             runtime/ParseSignedLongFromStringBaseN.c
 *   ScanFormatConversions                                      runtime/ScanFormatConversions.c
 *   SkipDigitSignColonThenNonalnumToToken                      runtime/SkipDigitSignColonThenNonalnumToToken.c
 *   SkipQuotedTokenThenControlCharsToToken                     runtime/SkipQuotedTokenThenControlCharsToToken.c
 */

/* ==========================================================================
 * Unit: AppendCStringToDest
 * Source: runtime/AppendCStringToDest.c
 * ========================================================================== */
#ifdef NW_UNIT_AppendCStringToDest
/*
 * Appends a NUL-terminated string and returns the destination buffer.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-appendcstringtodest-c
 */

#include <stdint.h>
#include <windows.h>

char * __cdecl AppendCStringToDest(char *pDestBuf, const char *pszSrc)
{
    uint32_t dwSrcBytes; /* length of pszSrc including its terminating NUL */
    uint32_t dwDstLen;   /* current length of pDestBuf (excluding the NUL) */

    /* Scan source to find its length (bytes up to and including the NUL). */
    for (dwSrcBytes = 0; *(pszSrc + dwSrcBytes) != '\0'; ++dwSrcBytes) { }
    ++dwSrcBytes; /* the terminating NUL itself is copied over as well */

    /* Scan destination to locate its tail (the existing NUL position). */
    for (dwDstLen = 0; pDestBuf[dwDstLen] != '\0'; ++dwDstLen) { }

    char *pTailPos = pDestBuf + dwDstLen;   /* first free byte of the destination */
    uint32_t dwWords = dwSrcBytes >> 2;     /* full 4-byte blocks to move      */
    uint32_t cRemain = dwSrcBytes & 3u;     /* trailing bytes after the blocks */

    for (uint32_t iWord = 0; iWord < dwWords; ++iWord) {
        *(uint32_t *)(pTailPos + iWord * 4u)      = *(const uint32_t *)(pszSrc + iWord * 4u);
    }
    for (uint32_t iByte = 0; iByte < cRemain; ++iByte) {
        pTailPos[dwWords * 4u + iByte]            = pszSrc[dwWords * 4u + iByte];
    }

    return pDestBuf;
}
#endif /* NW_UNIT_AppendCStringToDest */

/* ==========================================================================
 * Unit: CompareBoundedAsciiStringsNoCase
 * Source: runtime/CompareBoundedAsciiStringsNoCase.c
 * ========================================================================== */
#ifdef NW_UNIT_CompareBoundedAsciiStringsNoCase
/*
 * Case-insensitively compares up to nMaxCount bytes of two ASCII strings, folding each byte through ConvertAsciiCharToLowerCase.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-compareboundedasciistringsnocase-c
 */

extern int ConvertAsciiCharToLowerCase(int iCharCode);

int __cdecl CompareBoundedAsciiStringsNoCase(const char * pszLeft,
                                             const char * pszRight,
                                             int           nMaxCount)
{
    unsigned char ucFoldedLeft;                                  /* EBX, and the local */
    unsigned char ucFoldedRight;                                 /* the 1-byte local   */
    int           nRemaining = nMaxCount;                        /* EBP countdown      */

    ucFoldedLeft  = 0;                       /* XOR EBX,EBX ; MOV [ESP+0x10],BL */
    ucFoldedRight = 0;

    while (nRemaining != 0) {                
        nRemaining = nRemaining - 1;         /* DEC EBP — happens before the body runs */

        ucFoldedLeft = (unsigned char)ConvertAsciiCharToLowerCase((int)(unsigned char)*pszLeft);
        pszLeft      = pszLeft + 1;           /* INC ESI */
        ucFoldedRight = (unsigned char)ConvertAsciiCharToLowerCase((int)(unsigned char)*pszRight);
        pszRight     = pszRight + 1;          /* INC EDI */

        if (ucFoldedLeft == 0) {             
            break;
        }
        if (ucFoldedLeft != ucFoldedRight) { 
            break;
        }
    }

    return (int)ucFoldedLeft - (int)ucFoldedRight;   /* both zero-extended, SUB EAX,ECX */
}
#endif /* NW_UNIT_CompareBoundedAsciiStringsNoCase */

/* ==========================================================================
 * Unit: ConvertAsciiCharToLowerCase
 * Source: runtime/ConvertAsciiCharToLowerCase.c
 * ========================================================================== */
#ifdef NW_UNIT_ConvertAsciiCharToLowerCase
/*
 * Convert Ascii Char To Lower Case.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-convertasciichartolowercase-c
 */

int ConvertAsciiCharToLowerCase(int iCharCode)
{
    if ((iCharCode > 0x40) && (iCharCode < 0x5b)) {  /* CMP 0x41 / JL, CMP 0x5a / JG — 'A' <= c <= 'Z' */
        iCharCode = iCharCode + 0x20;                 /* LEA EAX,[ECX + 0x20] — set bit 0x20 */
    }

    return iCharCode;                                 
}
#endif /* NW_UNIT_ConvertAsciiCharToLowerCase */

/* ==========================================================================
 * Unit: ConvertAsciiCharToUpperCase
 * Source: runtime/ConvertAsciiCharToUpperCase.c
 * ========================================================================== */
#ifdef NW_UNIT_ConvertAsciiCharToUpperCase
/*
 * Convert Ascii Char To Upper Case.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-convertasciichartouppercase-c
 */

int ConvertAsciiCharToUpperCase(int iCharCode)
{
    if ((iCharCode > 0x60) && (iCharCode < 0x7b)) {  /* CMP 0x61 / JL, CMP 0x7a / JG — 'a' <= c <= 'z' */
        iCharCode = iCharCode - 0x20;                 /* LEA EAX,[ECX + -0x20] — clear bit 0x20 */
    }

    return iCharCode;                                 
}
#endif /* NW_UNIT_ConvertAsciiCharToUpperCase */

/* ==========================================================================
 * Unit: ConvertAsciiStringToUpperCase
 * Source: runtime/ConvertAsciiStringToUpperCase.c
 * ========================================================================== */
#ifdef NW_UNIT_ConvertAsciiStringToUpperCase
/*
 * Uppercases a NUL-terminated ASCII string in place, byte by byte via ConvertAsciiCharToUpperCase (only 'a'..'z' are folded.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-convertasciistringtouppercase-c
 */

extern int ConvertAsciiCharToUpperCase(int iCharCode);

char * ConvertAsciiStringToUpperCase(char * pszString)
{
    char * pcCursor = pszString;   
    char   cNext;                  /* current byte, reloaded after every increment */

    cNext = *pcCursor;

    while (cNext != '\0') {        
        *pcCursor = (char)ConvertAsciiCharToUpperCase((int)*pcCursor);
        pcCursor  = pcCursor + 1;  /* INC EBX */
        cNext     = *pcCursor;
    }

    return pszString;              
}
#endif /* NW_UNIT_ConvertAsciiStringToUpperCase */

/* ==========================================================================
 * Unit: ConvertIntegerToString
 * Source: runtime/ConvertIntegerToString.c
 * ========================================================================== */
#ifdef NW_UNIT_ConvertIntegerToString
/*
 * Convert Integer To String.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-convertintegertostring-c
 */

#include <stdint.h>

//
// Parameter mapping (verified against the prologue / frame offsets):
//   iValue = [EBP+8]    first argument, signed magnitude source
//   pDest  = [EBP+0xC]  second argument, output buffer; returned on exit
//   uBase  = [EBP+0x10] third argument, radix (binary supports base <= 16)
//

//   - Sign is honored only for base==10 and a negative value; otherwise the
//     raw unsigned bits are used. Magnitude negation is done in two's
//     complement on an unsigned type so INT_MIN wraps exactly like `NEG ECX`.
//   - Digits are produced LSB-first by repeated div/mod into a scratch buffer

//     forward and are emitted reversed, producing identical bytes).
//   - Output to pDest = optional '-' + digits MSB->LSB + NUL. pDest is returned.

char *ConvertIntegerToString(int iValue, char *pDest, unsigned uBase) {
    // Upper-case digit table: index 0x0A..0x0F yields 'A'..'F'. The binary
    // emits a digit as (rem + 0x30), or (rem + 0x37) when that exceeds '9',
    // which is exactly azDigits[rem] for rem in [0,15]. Hence radix <= 16.
    static const char azDigits[16] = "0123456789ABCDEF";

    int      bNeg;       // 1 when a sign byte ('-') is emitted (base==10 && value<0)
    uint32_t uiMag;      // unsigned magnitude fed to the div/mod loop
    char     pbScratch[16]; // up to 16 hex digits, built LSB-first (reversed on emit)
    int      n = 0;

    bNeg   = ((uBase == 10u) && (iValue < 0)) ? 1 : 0;
    uiMag  = (uint32_t)iValue;
    if (bNeg) uiMag = -uiMag; // two's-complement negation on unsigned: wraps like NEG ECX

    do {
        unsigned uiRem = (unsigned)(uiMag % uBase);
        uiMag /= uBase;
        pbScratch[n++] = azDigits[uiRem];   // least-significant digit first
    } while (uiMag != 0u);

    int iOut = 0;
    if (bNeg) pDest[iOut++] = '-';          // sign byte, base==10 negatives only
    for (int k = n - 1; k >= 0; --k)        // digits MSB->LSB
        pDest[iOut++] = pbScratch[k];
    pDest[iOut] = '\0';                      // NUL terminator

    return pDest;                            // returns the destination base pointer
}
#endif /* NW_UNIT_ConvertIntegerToString */

/* ==========================================================================
 * Unit: CopyBoundedStringNulPadded
 * Source: runtime/CopyBoundedStringNulPadded.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyBoundedStringNulPadded
/*
 * Copies at most dwCount bytes from puSrc to puDest, stopping early at the source NUL, then zero-pads the remainder of the destination; returns puDest
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-copyboundedstringnulpadded-c
 */

#include <windows.h>
#include <stddef.h>
#include <string.h>

typedef unsigned int uint;

/* char *strncpy(char *dest, const char *src, size_t count) -- MSVC inline form */
char * __cdecl CopyBoundedStringNulPadded(uint *puDest, uint *puSrc, uint dwCount);

char * __cdecl CopyBoundedStringNulPadded(uint *puDest, uint *puSrc, uint dwCount)
{
    const unsigned char *pszSrc = (const unsigned char *)puSrc;
    unsigned char       *pszDest = (unsigned char *)puDest;
    size_t               dwSrcLen;
    size_t               dwCopied;

    dwSrcLen = strlen((const char *)puSrc);

    dwCopied = (dwCount < dwSrcLen) ? (size_t)dwCount : dwSrcLen;

    if (dwCopied != 0) {
        memcpy(pszDest, pszSrc, dwCopied);
    }

    if (dwCount > dwCopied) {
        memset(pszDest + dwCopied, 0, (size_t)dwCount - dwCopied);
    }

    return (char *)puDest;
}
#endif /* NW_UNIT_CopyBoundedStringNulPadded */

/* ==========================================================================
 * Unit: CopyNulTerminatedString
 * Source: runtime/CopyNulTerminatedString.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyNulTerminatedString
/*
 * Copies a NUL-terminated string and returns the destination.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-copynulterminatedstring-c
 */

#include <stdint.h>

char * __cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc)
{
    const unsigned char *pCursor = (const unsigned char *)pszSrc; /* byte scan cursor */
    const uint32_t *puWord;                                       /* DWORD-chunk cursor */
    char *pszRet = pszDst;                              /* always returns dst base */

    while (((uintptr_t)pCursor & 3u) != 0u) {
        uint8_t bByte = *pCursor++;
        *(char *)pszDst++ = (char)bByte;
        if (bByte == 0u) return pszRet;                 /* NUL copied: stop */
    }

    puWord = (const uint32_t *)pCursor;

    for (;;) {
        uint32_t dwChunk = *puWord;                     /* load next DWORD from src */
        puWord++;                                         /* advance source by 4 bytes */

        if ((dwChunk & 0x000000FFu) == 0u) {             /* byte 0 is NUL */
            *pszDst = '\0';                              /* write terminator only   */
            return pszRet;
        }
        if (((dwChunk >> 8) & 0xFFu) == 0u) {            /* byte 1 is NUL          */
            *(char *)pszDst++ = (char)(uint8_t)dwChunk;  /* copy one character     */
            *pszDst = '\0';                              /* write terminator       */
            return pszRet;
        }
        if ((dwChunk & 0x00FF0000u) == 0u) {             /* byte 2 is NUL          */
            *(char *)pszDst = (char)(uint8_t)dwChunk;    /* dst[0] = byte 0       */
            *(char *)(pszDst + 1) = (char)((dwChunk >> 8) & 0xFFu); /* dst[1] = byte 1 */
            *(char *)(pszDst + 2) = '\0';                /* dst[2] = terminator   */
            return pszRet;
        }
        *(uint32_t *)pszDst = dwChunk;                   /* copy the full DWORD    */
        pszDst += 4;
        if ((dwChunk & 0xFF000000u) == 0u) {             /* byte 3 was NUL: terminator included above */
            return pszRet;
        }
        /* no zero in this chunk: continue with next DWORD */
    }
}
#endif /* NW_UNIT_CopyNulTerminatedString */

/* ==========================================================================
 * Unit: FindFirstCharInString
 * Source: runtime/FindFirstCharInString.c
 * ========================================================================== */
#ifdef NW_UNIT_FindFirstCharInString
/*
 * Returns the first occurrence of a character in a string.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-findfirstcharinstring-c
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

char *FindFirstCharInString(char *szString, char cSearch)
{
    char   *pScan;       /* cursor: byte being tested during the next read     */
    uint32_t dwWord;     /* the four bytes at pScan, tested in address order    */

    /* Phase 1 — byte-at-a-time until the cursor is 4-byte aligned.           */
    for (pScan = szString; (((uintptr_t)pScan) & 3u) != 0u; pScan++) {
        if (*pScan == cSearch) {
            return pScan;
        }
        if (*pScan == '\0') {
            return NULL;
        }
    }

    /* Phase 2 — four bytes per iteration.                                     */
    for (;;) {
        memcpy(&dwWord, pScan, sizeof dwWord);
        pScan += 4;

        if ((char)(dwWord & 0xFFu) == cSearch) {
            return pScan - 4;
        }
        if ((char)(dwWord & 0xFFu) == '\0') {
            return NULL;
        }

        if ((char)((dwWord >> 8) & 0xFFu) == cSearch) {
            return pScan - 3;
        }
        if ((char)((dwWord >> 8) & 0xFFu) == '\0') {
            return NULL;
        }

        if ((char)((dwWord >> 16) & 0xFFu) == cSearch) {
            return pScan - 2;
        }
        if ((char)((dwWord >> 16) & 0xFFu) == '\0') {
            return NULL;
        }

        if ((char)((dwWord >> 24) & 0xFFu) == cSearch) {
            return pScan - 1;
        }
        if ((char)((dwWord >> 24) & 0xFFu) == '\0') {
            return NULL;
        }
    }
}
#endif /* NW_UNIT_FindFirstCharInString */

/* ==========================================================================
 * Unit: FindFirstSubstringInString
 * Source: runtime/FindFirstSubstringInString.c
 * ========================================================================== */
#ifdef NW_UNIT_FindFirstSubstringInString
/*
 * Find First Substring In String.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-findfirstsubstringinstring-c
 */

#include <stdint.h>
#include <windows.h>

extern int  __cdecl GetStringLength(const char *pszStr);
extern void *__cdecl FillMemoryWithDwordFromStackArgs(void *pDst,
                                                      unsigned int dwFill,
                                                      unsigned int dwCount);

char *__cdecl FindFirstSubstringInString(const char *pszHaystack,
                                         const char *pszNeedle)
{
    /* [E0-0x408] .. [E0-0x8]: 256 dwords, pre-seeded by the fill adapter
     * and then overwritten for every character present in the needle. */
    uint32_t adwSkipTable[256];

    const char *pszScan;     /* [EAX]  -- scans FORWARD over the haystack */
    char *pNeedleEnd;        /* [EBX]  -- walks BACKWARD down the needle   */
    char *pNeedleEndSaved;   /* [E0-0x8] */

    const unsigned int dwNeedleLen =
        (unsigned int)GetStringLength(pszNeedle);
    const unsigned int dwHaystackLen =
        (unsigned int)GetStringLength(pszHaystack);

    unsigned int dwOffset;
    unsigned int dwSkip;
    unsigned int dwPrefix;

    if (dwNeedleLen == 0u) {

        return (char *)pszHaystack;
    }

    if (dwNeedleLen > dwHaystackLen) {

        return NULL;
    }

    /* Pre-seed every slot with the needle length, then let the needle's
     * own characters override -- Horspool's default skip distance. */
    FillMemoryWithDwordFromStackArgs(adwSkipTable, dwNeedleLen, 256);

    pNeedleEnd = (char *)pszNeedle + dwNeedleLen - 1u;
    for (pszScan = pszNeedle; *pszScan != '\0'; ++pszScan) {
        dwOffset = (unsigned int)(pszScan - pszNeedle);
        adwSkipTable[(unsigned char)*pszScan] = dwNeedleLen - 1u - dwOffset;
    }

    pNeedleEndSaved = pNeedleEnd;                         /* [E0-0x8]  */
    pszScan = pszHaystack + dwNeedleLen - 1u;             /* [EAX]    */

    for (;;) {
        if (*pszScan == *pNeedleEnd) {
            if (pNeedleEnd == pszNeedle) {

                return (char *)pszScan;
            }
            --pNeedleEnd;
            --pszScan;
            continue;
        }

        dwPrefix = dwNeedleLen - (unsigned int)(pNeedleEnd - pszNeedle);
        dwSkip = adwSkipTable[(unsigned char)*pszScan];
        if (dwSkip < dwPrefix) {
            dwSkip = dwPrefix;
        }

        pszScan += dwSkip;
        pNeedleEnd = pNeedleEndSaved;

        if ((unsigned int)(pszScan - pszHaystack) >= dwHaystackLen) {
            return NULL;
        }
    }
}
#endif /* NW_UNIT_FindFirstSubstringInString */

/* ==========================================================================
 * Unit: FindLastCharInString
 * Source: runtime/FindLastCharInString.c
 * ========================================================================== */
#ifdef NW_UNIT_FindLastCharInString
/*
 * Scans a NUL-terminated string backward from its final byte; returns a pointer to the highest-index occurrence of chTarget in indices [n..0] (NUL...
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-findlastcharinstring-c
 */

#include <stddef.h>

extern int GetStringLength(const char *pszSrc);

/*
 * Equivalent to CRT strrchr(pszSrc, chTarget): find the last (rightmost)
 * occurrence of a character in a C string, including the terminating NUL.
 * The binary walks from index n down to 0 and stops at the first hit, so
 * the highest matching index wins; NULL is returned when the character
 * occurs nowhere.
 */
char *FindLastCharInString(const char *pszSrc, unsigned char chTarget)
{
    const char *pcCur;   /* byte currently inspected, walks from end of string down to start */
    ptrdiff_t lIdx;      /* index (0..n) of pcCur within pszSrc */

    lIdx = GetStringLength(pszSrc);                        
    for (pcCur = pszSrc + lIdx; ; pcCur--, lIdx--) {
        if (*pcCur == chTarget)
            return (char *)(pszSrc + lIdx);         /* first match while scanning down = highest index */
        if (lIdx <= 0)
            break;                                  /* byte at index 0 has been checked */
    }
    return NULL;                                    /* character not present in string or terminator */
}
#endif /* NW_UNIT_FindLastCharInString */

/* ==========================================================================
 * Unit: GetStringLength
 * Source: runtime/GetStringLength.c
 * ========================================================================== */
#ifdef NW_UNIT_GetStringLength
/*
 * Returns the byte length of a NUL-terminated string.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-getstringlength-c
 */

#include <stdint.h>
#include <windows.h>

int __cdecl GetStringLength(const char *pszStr)
{
    const char *pszCur = pszStr;
    uint32_t dwChunk;

    /* Walk byte-wise until the scan pointer is 4-byte aligned. */
    while (((uintptr_t)pszCur & 3u) != 0u) {
        if (*pszCur == '\0') {
            return (int)(pszCur - pszStr);
        }
        ++pszCur; /* advance one byte */
    }

    for (;;) {
        const uint32_t *puWord = (const uint32_t *)pszCur;
        dwChunk = *puWord;
        if ((dwChunk & 0x000000FFu) == 0u) { /* byte 0 is NUL */
            return (int)(pszCur - pszStr);
        }
        if (((dwChunk >> 8) & 0xFFu) == 0u) { /* byte 1 is NUL */
            return (int)((const char *)puWord + 1 - pszStr);
        }
        if ((dwChunk & 0x00FF0000u) == 0u) { /* byte 2 is NUL */
            return (int)((const char *)puWord + 2 - pszStr);
        }
        if ((dwChunk & 0xFF000000u) != 0u) { /* byte 3 set: next DWORD chunk */
            pszCur = (const char *)puWord + 4;
            continue;
        }
        return (int)((const char *)puWord + 3 - pszStr);
    }
}
#endif /* NW_UNIT_GetStringLength */

/* ==========================================================================
 * Unit: ParseBracketCharClassAndMatchChars
 * Source: runtime/ParseBracketCharClassAndMatchChars.c
 * ========================================================================== */
#ifdef NW_UNIT_ParseBracketCharClassAndMatchChars
/*
 * Parse Bracket Char Class And Match Chars.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-parsebracketcharclassandmatchchars-c
 */

extern void *FillMemoryWithByte(void *pDst, unsigned char bFill, unsigned int dwCount);

/* Callback that yields the next input character; its full dword return is
 * stored into the context's dwCurChar field. */
typedef unsigned int (*PFN_NEXTCHAR)(void *pArg);

typedef struct {
    unsigned int  dwEmittedCount;   /* +0x00 */
    unsigned int  dwMatchTotal;     /* +0x04 */
    int           iRemaining;       /* +0x08 */
    unsigned char bFlags;           /* +0x0C */
    unsigned int  dwCurChar;        /* +0x10 */
    char        **ppcOut;           /* +0x14 */
    char        **ppcPattern;       /* +0x18 */
    void         *pNextCharArg;     /* +0x1C */
    PFN_NEXTCHAR  pfnNextChar;      /* +0x20 */
} ScanCtx;

/* bFlags bit1: collect nothing, just count. */
#define SCAN_CTX_QUIET 0x02u

static unsigned char ReadPatternChar(ScanCtx *pCtx)
{
    unsigned char ch = (unsigned char)**pCtx->ppcPattern;

    (*pCtx->ppcPattern)++;
    return ch;
}

static void EmitCharToOut(ScanCtx *pCtx, unsigned char ch)
{
    char *pOut = *pCtx->ppcOut;

    pOut[0] = (char)ch;
    pOut[1] = '\0';
    (*pCtx->ppcOut)++;
}

int ParseBracketCharClassAndMatchChars(ScanCtx *pCtx)
{
    unsigned char abCharClass[256];
    unsigned char bCur;
    unsigned char bPrev;
    unsigned char bRangeLo;
    unsigned char bRangeHi;
    unsigned char bRangePending;
    unsigned int  bClassValue;      /* frame local_c: 1 = set, 0 = negated */
    int bMatched;

    bRangeLo = 0;
    bRangeHi = 0;
    bPrev = 0;

    /* A leading '^' negates the class; the character after it starts the
     * body. (The '[' opener was consumed by the caller's %[ branch.) */
    bCur = ReadPatternChar(pCtx);
    bClassValue = 1u;
    if (bCur == '^') {
        bClassValue = 0u;
        bCur = ReadPatternChar(pCtx);
    }
    bPrev = bCur;

    /* A leading '-' is a literal, not a range: the latch is set before the
     * loop ever tests it. */
    bRangePending = (bCur == '-') ? (unsigned char)1u : (unsigned char)0u;

    /* Background: the complement of the class polarity, so a negated class
     * matches everything except its members. */
    (void)FillMemoryWithByte(abCharClass,
                             (unsigned char)(bClassValue ^ 1u),
                             0x100u);

    for (;;) {
        /* '-' with no range pending opens a new range. */
        if ((bCur == '-') && (bRangePending == 0u)) {
            bRangeLo = bPrev;
            bCur = ReadPatternChar(pCtx);
            bRangeHi = bCur;
            if (bCur == ']') {
                break;              /* unterminated range: class ends here */
            }
        }

        bPrev = bCur;
        abCharClass[bCur] = (unsigned char)bClassValue;

        {
            int iLo = (int)(signed char)bRangeLo;
            int iHi = (int)(signed char)bRangeHi;
            int iMin = (iLo < iHi) ? iLo : iHi;
            int iMax = (iLo < iHi) ? iHi : iLo;
            unsigned char uc = (unsigned char)iMin;

            if (iMax >= iMin) {     
                do {
                    abCharClass[uc] = (unsigned char)bClassValue;
                    uc++;
                } while (iMax >= (int)(signed char)uc);
            }
        }

        bCur = ReadPatternChar(pCtx);
        bRangeHi = bCur;
        bRangeLo = bCur;
        bRangePending = 0u;
        if (bCur == ']') {
            break;
        }
    }

    /* Emit every in-class character the remaining budget allows. */
    bMatched = 0;
    if ((pCtx->iRemaining-- != 0) && (abCharClass[pCtx->dwCurChar] != 0u)) {
        bMatched = 1;
        do {
            if ((pCtx->bFlags & SCAN_CTX_QUIET) == 0u) {
                EmitCharToOut(pCtx, (unsigned char)pCtx->dwCurChar);
            }
            pCtx->dwEmittedCount++;
            pCtx->dwCurChar = pCtx->pfnNextChar(pCtx->pNextCharArg);
        } while ((pCtx->iRemaining-- != 0) &&
                 (abCharClass[pCtx->dwCurChar] != 0u));
    }

    if ((pCtx->bFlags & SCAN_CTX_QUIET) == 0u) {
        pCtx->dwMatchTotal += (unsigned int)bMatched;
    }
    return bMatched;
}
#endif /* NW_UNIT_ParseBracketCharClassAndMatchChars */

/* ==========================================================================
 * Unit: ParseHmsTimeStringToSignedSeconds
 * Source: runtime/ParseHmsTimeStringToSignedSeconds.c
 * ========================================================================== */
#ifdef NW_UNIT_ParseHmsTimeStringToSignedSeconds
/*
 * Parses up to three numeric fields scaled 3600/60/1 (hours*3600 + minutes*60 + seconds) from a NUL-terminated string into a signed second count.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-parsehmstimestringtosignedseconds-c
 */

#include <windows.h>
#include <stdint.h>

extern char *SkipDigitSignColonThenNonalnumToToken(char *p);   
extern int ParseSignedDecimalIntegerFromString(byte *param_1);                  
extern const unsigned char g_charClass[];                      

int ParseHmsTimeStringToSignedSeconds(char *p)
{
    int seconds;
    int sign;
    char *q;

    seconds = 0;
    if (p == NULL || *p == 0)
        return seconds;

    /* Skip a leading run of alphabetic bytes (class mask 0x3 = _UPPER | _LOWER). */
    while ((g_charClass[(int8_t)*p] & 0x3u) != 0u)
        p++;
    if (*p == 0)
        return seconds;

    sign = 1;
    seconds = ParseSignedDecimalIntegerFromString((byte *)p) * 3600;
    if (seconds < 0)
        sign = -1;

    q = SkipDigitSignColonThenNonalnumToToken(p);
    if (*q != 0 && *q != ';' && *q != ',') {
        seconds += ParseSignedDecimalIntegerFromString((byte *)q) * 60 * sign;
        q = SkipDigitSignColonThenNonalnumToToken(q);
        if (*q != 0 && *q != ';' && *q != ',')
            seconds += ParseSignedDecimalIntegerFromString((byte *)q) * sign;
    }

    SkipDigitSignColonThenNonalnumToToken(q);   /* result discarded; no effect on the return value */
    return seconds;
}
#endif /* NW_UNIT_ParseHmsTimeStringToSignedSeconds */

/* ==========================================================================
 * Unit: ParseSignedDecimalIntegerFromString
 * Source: runtime/ParseSignedDecimalIntegerFromString.c
 * ========================================================================== */
#ifdef NW_UNIT_ParseSignedDecimalIntegerFromString
/*
 * Parse Signed Decimal Integer From String.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-parsesigneddecimalintegerfromstring-c
 */

#include <windows.h>
#include <stdint.h>

int ParseSignedDecimalIntegerFromString(byte *pbStr)
{
    unsigned int value;      /* ECX: base-10 accumulator */
    unsigned int signMask;   
    byte b;                  /* AL: the byte currently under test */

    do {
        b = *pbStr++;
    } while (b == 0x20u || (b >= 0x09u && b <= 0x0du));

    value = 0u;
    signMask = 0u;
    if (b == '+') {
        b = *pbStr++;
    } else if (b == '-') {
        signMask = 0xffffffffu;   /* DEC EBX from 0 */
        b = *pbStr++;
    }

    for (;;) {
        byte digit = (byte)(b - 0x30u);
        if (b < 0x30u || digit >= 0x0au)
            break;
        value = value * 10u + digit;
        b = *pbStr++;
    }

    return (int)((value ^ signMask) - signMask);
}
#endif /* NW_UNIT_ParseSignedDecimalIntegerFromString */

/* ==========================================================================
 * Unit: ParseSignedLongFromStringBaseN
 * Source: runtime/ParseSignedLongFromStringBaseN.c
 * ========================================================================== */
#ifdef NW_UNIT_ParseSignedLongFromStringBaseN
/*
 * Parses a signed long from a NUL-terminated string in a caller-chosen base (0 = auto).
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-parsesignedlongfromstringbasen-c
 */

#include <windows.h>

extern void __fastcall SetThreadContextValueAndClearFlag(DWORD dwUnusedParam1,
                                                         DWORD dwUnusedParam2,
                                                         DWORD dwErrno);

/* ------------------------------------------------------------------ */
/* Globals                                                              */
/* ------------------------------------------------------------------ */

extern const unsigned char g_charClassTable[256];   

#define CC_ISSPACE  0x08
#define CC_ISDIGIT  0x04
#define CC_ISALPHA  0x03

#define STOL_ERANGE 0x22

/* ------------------------------------------------------------------ */
/* Interface                                                            */
/* ------------------------------------------------------------------ */

long __cdecl ParseSignedLongFromStringBaseN(char *szString,
                                           char **ppEnd,
                                           int nBase)
{
    char  *pCur;          /* cursor: walks szString in place */
    char  *pStart;        
    long   lAccum;        /* accumulator, held NEGATED (see below) */
    int    nDigitValue;   /* current character folded to a digit value */
    LONG   lDivisor;      /* overflow-check intermediate */
    int    bNegative;     /* a '-' was consumed */
    int    bAnyDigits;    /* at least one digit was consumed */
    int    bOverflow;     /* the value did not fit in a long */

    pCur      = szString;
    bNegative = 1;
    bAnyDigits = 0;
    bOverflow = 0;
    lAccum    = 0;

    while ((g_charClassTable[(signed char)*pCur] & CC_ISSPACE) != 0)
        pCur++;

    if (*pCur == '-') {
        bNegative = 0;
        pCur++;
    } else {
        if (*pCur == '+')
            pCur++;
        bNegative = 1;
    }

    if (nBase == 0) {
        nBase = 10;
        if (*pCur == '0') {
            nBase = 8;
            pCur++;
            bAnyDigits = 1;            /* the leading '0' counts as consumed */
            if ((pCur[0] == 'x') || (pCur[0] == 'X')) {
                nBase = 16;
                pCur++;
                bAnyDigits = 0;        /* "0x" alone is NOT a conversion */
            } else if ((pCur[0] == 'b') || (pCur[0] == 'B')) {
                nBase = 2;
                pCur++;
                bAnyDigits = 0;        /* "0b" alone is NOT a conversion */
            }
            /* anything else after '0' stays base 8, and the '0' stands as a
             * consumed digit that contributes nothing to the value */
        }
    } else if (nBase == 16) {
        if ((*pCur != '0') || ((pCur[1] != 'x') && (pCur[1] != 'X')))
            goto digits;               /* prefix is optional, not required */
        pCur += 2;
    } else if (nBase == 2) {
        if ((*pCur != '0') || ((pCur[1] != 'b') && (pCur[1] != 'B')))
            goto digits;
        pCur += 2;
    }

digits:

    for (;;) {
        if ((g_charClassTable[(signed char)*pCur] & CC_ISDIGIT) != 0) {
            nDigitValue = (int)*pCur - '0';       
        } else if ((g_charClassTable[(signed char)*pCur] & CC_ISALPHA) != 0) {
            nDigitValue = (((int)*pCur) & ~0x20) - ('A' - 10);  
        } else {
            break;
        }
        if (nDigitValue >= nBase)                
            break;

        lDivisor = (LONG)(0x80000000u + (unsigned long)nDigitValue);
        if (lDivisor / nBase > lAccum)
            bOverflow = 1;

        lAccum = nBase * lAccum - nDigitValue;
        pCur++;
        bAnyDigits = 1;
    }

    pStart = szString;
    if (bAnyDigits == 0)
        pCur = pStart;
    if (ppEnd != NULL)
        *ppEnd = pCur;                            

    if (((bNegative == 0) || (lAccum != (LONG)0x80000000)) && (bOverflow == 0)) {
        if (bNegative)                               /* flag != 0 = no '-': value is -acc */
            return (LONG)(0u - (unsigned long)lAccum);   
        return lAccum;                               /* '-' consumed: acc already negated */
    }

    SetThreadContextValueAndClearFlag(0, 0, STOL_ERANGE);
    return (bNegative != 0) ? 0x7fffffffl : (LONG)0x80000000l;
}

/* ------------------------------------------------------------------ */
/* Notes                                                                */
/* ------------------------------------------------------------------ */
#endif /* NW_UNIT_ParseSignedLongFromStringBaseN */

/* ==========================================================================
 * Unit: ScanFormatConversions
 * Source: runtime/ScanFormatConversions.c
 * ========================================================================== */
#ifdef NW_UNIT_ScanFormatConversions
/*
 * Runs a scanf-style format string against a pull-style input stream, converting each conversion specification into its stored value and returning the...
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-scanformatconversions-c
 */

#include <stdint.h>

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */

/* src/runtime/ParseBracketCharClassAndMatchChars.c - the %[ handler. Its ScanCtx view
 * of the block above is the same record this file builds: the byte it treats
 * as "collect nothing, just count" really is this function's FLAG_SUPPRESS,
 * and the cursor it advances really is this function's ppVaSlot. */
extern int ParseBracketCharClassAndMatchChars(void *pScanCtx);

extern uint64_t MultiplyLow64Product(uint64_t multiplicand, uint64_t multiplier);

/* src/app/win32/WriteStdoutStringAndExit.c - prints its argument and kills the process.
 * Reached only from the "no floating-point scanner registered" abort. */
extern void WriteStdoutStringAndExit(const char *szMessage);

/* ------------------------------------------------------------------ */
/* Globals                                                              */
/* ------------------------------------------------------------------ */

extern const unsigned char g_charClassTable[256];   

extern int (*g_pfnScanFloat)(void *pScanCtx);      

/* ------------------------------------------------------------------ */
/* Interface                                                            */
/* ------------------------------------------------------------------ */

/* Pulls one character from the stream; returns -1 at end of input. */
typedef int (*PFN_GETC)(void *pStream);
/* Pushes one character back; returns it. */
typedef int (*PFN_UNGETC)(int ch, void *pStream);

/* Conversion flags, held in the low byte of the context's flags word. */
#define FLAG_NEG      0x01u  /* a '-' was seen: skip the final negate       */
#define FLAG_SUPPRESS 0x02u  /* '*' was seen: consume nothing, store nowhere */
#define FLAG_SHORT    0x04u  /* 'h'  - store the low 16 bits                */
#define FLAG_LONG     0x08u  /* 'l'  - store 32 bits (long is 32-bit here) */
#define FLAG_LDOUBLE  0x10u  /* 'L'  - long double; never tested in this
                                        function, carried in the flags word
                                        for the (absent) float scanner       */
#define FLAG_LONGLONG 0x20u  /* "ll" - store 64 bits                       */

/* Character-class table bits (see g_charClassTable). */
#define CHCLASS_DIGIT  0x04u
#define CHCLASS_SPACE  0x08u
#define CHCLASS_XDIGIT 0x80u

#define SCAN_EOF (-1)

/* The scan-context record, laid out exactly as it sits in this frame. */
typedef struct ScanCtx {
    int          iCharsRead;    
    int          iAssigned;     
    int          iFieldWidth;   /* iStack_2c */
    unsigned int dwFlags;       /* uStack_28 (only the low byte is tested) */
    int          iCurChar;      
    void       **ppVaSlot;      /* plStack_20 */
    const char **ppFormat;      
    void        *pStream;       
    PFN_GETC     pfnGetc;       
} ScanCtx;

/* How ConvertIntegerField ended. */
enum {
    CONV_CONTINUE,   /* conversion done: carry on with the format      */
    CONV_STOP,       
    CONV_END_INPUT   /* no match, input is spent: EOF accounting       */
};

/* ------------------------------------------------------------------ */
/* Helpers mirroring the inlined code blocks                            */
/* ------------------------------------------------------------------ */

static char ReadFormatChar(ScanCtx *pCtx)
{
    char ch = **pCtx->ppFormat;

    (*pCtx->ppFormat)++;
    return ch;
}

static int GetNextChar(ScanCtx *pCtx)
{
    pCtx->iCharsRead++;
    pCtx->iCurChar = pCtx->pfnGetc(pCtx->pStream);
    return pCtx->iCurChar;
}

static int CharClass(int ch, unsigned int uBit)
{
    if (ch == SCAN_EOF)
        return 0;
    return (g_charClassTable[ch] & uBit) != 0;
}

static int DigitValue(int ch)
{
    if (ch >= 'a')                     /* 0x61 */
        return ch - 0x57;              /* a..f -> 10..15 */
    if (ch >= 'A')                     /* 0x41 */
        return ch - 0x37;              /* A..F -> 10..15 */
    return ch - '0';                   /* '0'..'9' -> 0..9 */
}

static void ConsumeOptionalSign(ScanCtx *pCtx)
{
    if (pCtx->iCurChar == '-') {
        pCtx->dwFlags |= FLAG_NEG;
    } else if (pCtx->iCurChar != '+') {
        return;
    }
    (void)GetNextChar(pCtx);
    pCtx->iFieldWidth--;               
}

static int ConvertIntegerField(ScanCtx *pCtx, int iBase, PFN_UNGETC pfnUngetc)
{
    uint64_t acc = 0;
    int bGotDigit = 0;
    int bNeedDigit = 1;
    int bFinished = 0;

    if (pCtx->iFieldWidth == 0)
        bFinished = 1;
    else
        pCtx->iFieldWidth--;

    while (!bFinished) {

        if (!CharClass(pCtx->iCurChar, CHCLASS_XDIGIT)) {
            if (bNeedDigit != 0) {
                if (pCtx->iCurChar != SCAN_EOF)
                    return CONV_STOP;
                return CONV_END_INPUT;
            }
            break;
        }
        bNeedDigit = 0;

        {
            int iDigit = DigitValue(pCtx->iCurChar);

            if ((iDigit < 0) || (iDigit >= iBase))
                break;                  /* digit out of range for this base */
            bGotDigit = 1;
            acc = MultiplyLow64Product(acc, (uint64_t)(unsigned int)iBase);
            acc = acc - (uint64_t)(unsigned int)iDigit;
        }

        (void)GetNextChar(pCtx);

        if (pCtx->iFieldWidth == 0)
            break;
        pCtx->iFieldWidth--;
    }

    pCtx->iCharsRead--;
    pfnUngetc(pCtx->iCurChar, pCtx->pStream);

    if (bGotDigit != 0) {
        if ((pCtx->dwFlags & FLAG_SUPPRESS) == 0u) {
            void *pSlot = *pCtx->ppVaSlot;

            if ((pCtx->dwFlags & FLAG_NEG) == 0u)
                acc = 0u - acc;        /* NEG lo / NEG hi / SBB hi,0 */
            if ((pCtx->dwFlags & FLAG_SHORT) != 0u) {
                *(uint16_t *)pSlot = (uint16_t)(acc & 0xFFFFu);
            } else if (((pCtx->dwFlags & FLAG_LONG) == 0u) &&
                       ((pCtx->dwFlags & FLAG_LONGLONG) != 0u)) {
                /* long long: two adjacent 32-bit va_list slots */
                *(uint32_t *)pSlot       = (uint32_t)(acc & 0xFFFFFFFFu);
                *((uint32_t *)pSlot + 1) = (uint32_t)(acc >> 32);
            } else {
                *(uint32_t *)pSlot = (uint32_t)(acc & 0xFFFFFFFFu);
            }
            pCtx->iAssigned++;
        }
        return CONV_CONTINUE;
    }

    if (pCtx->iCurChar != SCAN_EOF)
        return CONV_CONTINUE;
    return CONV_END_INPUT;
}

/* ------------------------------------------------------------------ */

int ScanFormatConversions(PFN_UNGETC pfnUngetcChar, PFN_GETC pfnGetcChar,
                          void *pScanStream, const char *pszFormat,
                          void **ppVaArgs)
{
    ScanCtx sCtx;
    char chSpec;

    sCtx.iCharsRead = 0;
    sCtx.iAssigned = 0;
    sCtx.iFieldWidth = 0;
    sCtx.dwFlags = 0u;
    sCtx.iCurChar = ' ';        /* 0x20: not EOF, so the first pre-fetch runs */
    sCtx.ppVaSlot = NULL;
    sCtx.ppFormat = &pszFormat;
    sCtx.pStream = pScanStream;
    sCtx.pfnGetc = pfnGetcChar;

    for (;;) {
LoopTop:                            
        /* --- next format character ---------------------------------- */
        chSpec = ReadFormatChar(&sCtx);
        if (chSpec == '\0')
            return sCtx.iAssigned;      

        if (sCtx.iCurChar != SCAN_EOF) {
            /* Fetch the next input character unless input is already spent. */
            if (GetNextChar(&sCtx) == SCAN_EOF)
                goto EndOfInput;

            while ((chSpec == ' ') || (chSpec == '\t') || (chSpec == '\n')) {
                chSpec = ReadFormatChar(&sCtx);
                while (CharClass(sCtx.iCurChar, CHCLASS_SPACE))
                    (void)GetNextChar(&sCtx);
                if (chSpec == '\0')
                    goto PushbackAndReturn;
            }
            if (sCtx.iCurChar == SCAN_EOF)
                goto EndOfInput;
        }

        /* --- ordinary literal: must match the current input character -- */
        if (chSpec != '%') {
            if (chSpec != (char)sCtx.iCurChar)
                goto PushbackAndReturn;
            continue;                   
        }

        /* --- '%' : flags, field width, length modifiers -------------- */
        sCtx.dwFlags = 0u;

        chSpec = ReadFormatChar(&sCtx);
        if (chSpec == '*') {            /* 0x2a */
            sCtx.dwFlags |= FLAG_SUPPRESS;
            chSpec = ReadFormatChar(&sCtx);
        }

        if (CharClass((unsigned char)chSpec, CHCLASS_DIGIT)) {
            int iWidth = 0;

            do {
                iWidth = iWidth * 10 + (int)(unsigned char)chSpec - '0';
                chSpec = ReadFormatChar(&sCtx);
            } while (CharClass((unsigned char)chSpec, CHCLASS_DIGIT));

            if (iWidth == 0)
                goto PushbackAndReturn;
            sCtx.iFieldWidth = iWidth;
        } else {
            sCtx.iFieldWidth = 0x7fff;  /* no explicit width */
        }

        if (chSpec == 'h') {            /* 0x68 */
            sCtx.dwFlags |= FLAG_SHORT;
            chSpec = ReadFormatChar(&sCtx);
        } else if (chSpec == 'l') {     /* 0x6c */
            sCtx.dwFlags |= FLAG_LONG;
            chSpec = ReadFormatChar(&sCtx);
            if (chSpec == 'l') {        /* "ll" */
                sCtx.dwFlags = (sCtx.dwFlags & ~FLAG_LONG) | FLAG_LONGLONG;
                chSpec = ReadFormatChar(&sCtx);
            }
        } else if (chSpec == 'L') {     /* 0x4c */
            sCtx.dwFlags |= FLAG_LDOUBLE;
            chSpec = ReadFormatChar(&sCtx);
        }

        if ((chSpec != '%') && ((sCtx.dwFlags & FLAG_SUPPRESS) == 0u)) {
            sCtx.ppVaSlot = ppVaArgs;
            ppVaArgs++;
        }

        if ((chSpec != '[') && (chSpec != 'c') && (chSpec != 'n')) {
            while (CharClass(sCtx.iCurChar, CHCLASS_SPACE))
                (void)GetNextChar(&sCtx);
        }

        /* --- dispatch on the conversion letter ---------------------- */
        switch (chSpec) {

        case 'i': {                      /* 0x69 - base from the prefix */
            int iBase = 10;

            ConsumeOptionalSign(&sCtx);

            if (sCtx.iCurChar == '0') {
                (void)GetNextChar(&sCtx);
                if ((sCtx.iCurChar == 'b') || (sCtx.iCurChar == 'B')) {
                    iBase = 2;
                    (void)GetNextChar(&sCtx);       /* eat the 'b' */
                } else if ((sCtx.iCurChar == 'x') ||
                           (sCtx.iCurChar == 'X')) {
                    iBase = 16;
                    (void)GetNextChar(&sCtx);       /* eat the 'x' */
                } else if ((sCtx.iCurChar < '1') ||
                           (sCtx.iCurChar > '8')) {

                    sCtx.iCharsRead--;
                    pfnUngetcChar(sCtx.iCurChar, sCtx.pStream);
                    sCtx.iCurChar = '0';
                    iBase = 8;
                } else {
                    iBase = 8;
                }
            }

            switch (ConvertIntegerField(&sCtx, iBase, pfnUngetcChar)) {
            case CONV_STOP:      goto PushbackAndReturn;
            case CONV_END_INPUT: goto EndOfInput;
            default:             continue;
            }
        }

        case 'b':                        /* 0x62 - binary */
        case 'o':                        /* 0x6f - octal */
        case 'd':                        /* 0x64 - signed decimal */
        case 'u':                        /* 0x75 - unsigned decimal */
        case 'x':                        /* 0x78 - signed hexadecimal */
        case 'p':                        /* 0x70 - pointer, same as %x */
        case 'X':                        /* 0x58 */
            ConsumeOptionalSign(&sCtx);
            switch (ConvertIntegerField(&sCtx,
                                        (chSpec == 'b') ? 2 :
                                        (chSpec == 'o') ? 8 :
                                        ((chSpec == 'x') || (chSpec == 'X') ||
                                         (chSpec == 'p')) ? 16 : 10,
                                        pfnUngetcChar)) {
            case CONV_STOP:      goto PushbackAndReturn;
            case CONV_END_INPUT: goto EndOfInput;
            default:             continue;
            }

        case 's': {                      /* 0x73 - whitespace-delimited string */
            unsigned char *pDest;

            while (CharClass(sCtx.iCurChar, CHCLASS_SPACE))
                (void)GetNextChar(&sCtx);
            if (sCtx.iCurChar == SCAN_EOF)
                goto EndOfInput;

            if ((sCtx.dwFlags & FLAG_SUPPRESS) == 0u)
                sCtx.iAssigned++;        /* counted before the token is read */

            for (;;) {
                int iPrevWidth = sCtx.iFieldWidth;

                sCtx.iFieldWidth--;
                if ((iPrevWidth == 0) || (sCtx.iCurChar == ' ') ||
                    (sCtx.iCurChar == '\n') || (sCtx.iCurChar == '\t'))
                    break;
                if ((sCtx.dwFlags & FLAG_SUPPRESS) == 0u) {
                    pDest = (unsigned char *)*sCtx.ppVaSlot;
                    (*(unsigned char **)sCtx.ppVaSlot)++;
                    *pDest = (unsigned char)sCtx.iCurChar;
                }
                (void)GetNextChar(&sCtx);
                if (sCtx.iCurChar == SCAN_EOF)
                    break;
            }
            if ((sCtx.dwFlags & FLAG_SUPPRESS) == 0u) {
                pDest = (unsigned char *)*sCtx.ppVaSlot;
                *pDest = '\0';           /* cursor not advanced past the NUL */
            }
            goto HandBackLastChar;
        }

        case 'c': {                      /* 0x63 - exactly N characters, raw */
            unsigned char *pDest;
            int bStored = 0;
            int bFinished = 0;

            if (sCtx.iFieldWidth == 0x7fff)
                sCtx.iFieldWidth = 1;    /* no width means a single character */
            if (sCtx.iFieldWidth == 0)
                bFinished = 1;           
            else
                sCtx.iFieldWidth--;      

            while (!bFinished) {
                if (sCtx.iCurChar == SCAN_EOF)
                    return sCtx.iAssigned;   

                bStored = 1;                
                if ((sCtx.dwFlags & FLAG_SUPPRESS) == 0u) {
                    pDest = (unsigned char *)*sCtx.ppVaSlot;
                    (*(unsigned char **)sCtx.ppVaSlot)++;
                    *pDest = (unsigned char)sCtx.iCurChar;
                }
                if (sCtx.iFieldWidth != 0)  
                    (void)GetNextChar(&sCtx);

                bFinished = (sCtx.iFieldWidth == 0);  
                sCtx.iFieldWidth--;
            }

            if ((bStored == 0) && (sCtx.iCurChar == SCAN_EOF))
                goto EndOfInput;
            if ((sCtx.dwFlags & FLAG_SUPPRESS) == 0u)
                sCtx.iAssigned++;
            continue;
        }

        case 'n':                        /* 0x6e - characters consumed so far */
            sCtx.iCharsRead--;           /* the skipped blank is given back */
            pfnUngetcChar(sCtx.iCurChar, sCtx.pStream);
            if (sCtx.iCurChar == SCAN_EOF)
                sCtx.iCharsRead++;
            if ((sCtx.dwFlags & FLAG_SUPPRESS) == 0u)
                *(int *)*sCtx.ppVaSlot = sCtx.iCharsRead;
            continue;

        case 'e':                        /* 0x65 */
        case 'E':                        /* 0x45 */
        case 'f':                        /* 0x66 */
        case 'g':                        /* 0x67 */
        case 'G':                        /* 0x47 */

            if (g_pfnScanFloat == NULL)
                WriteStdoutStringAndExit("Floating point not loaded");
            if (g_pfnScanFloat(&sCtx) != 0)
                goto HandBackLastChar;
            goto NoMatch;

        case '[':                        
            if (ParseBracketCharClassAndMatchChars(&sCtx) != 0)
                goto HandBackLastChar;
            goto NoMatch;

        case '%':                        /* a literal percent sign */
            if (chSpec != (char)sCtx.iCurChar)
                goto PushbackAndReturn;
            continue;

        default:                         /* unknown directive */
            if (chSpec == '\0')
                return sCtx.iAssigned;   
            goto PushbackAndReturn;
        }
    }

    /* --- exit paths ------------------------------------------------- */

HandBackLastChar:
    sCtx.iCharsRead--;
    pfnUngetcChar(sCtx.iCurChar, sCtx.pStream);
    goto LoopTop;

    /* A conversion produced nothing and input is not at end: the same
     * pushback-and-stop, still with no count increment. */
NoMatch:
    if (sCtx.iCurChar != SCAN_EOF)
        goto PushbackAndReturn;

    /* End of input: EOF only when nothing at all was converted. */
EndOfInput:
    if (sCtx.iAssigned == 0)
        return -1;
    return sCtx.iAssigned;

PushbackAndReturn:
    pfnUngetcChar(sCtx.iCurChar, sCtx.pStream);
    return sCtx.iAssigned;
}
#endif /* NW_UNIT_ScanFormatConversions */

/* ==========================================================================
 * Unit: SkipDigitSignColonThenNonalnumToToken
 * Source: runtime/SkipDigitSignColonThenNonalnumToToken.c
 * ========================================================================== */
#ifdef NW_UNIT_SkipDigitSignColonThenNonalnumToToken
/*
 * Two-phase character-class skip over a NUL-terminated byte string: phase 1 advances over a run of decimal digits and the characters '+', '-', ':'.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-skipdigitsigncolonthennonalnumtotoken-c
 */

#include <stdint.h>

extern const unsigned char g_charClass[];   

char *SkipDigitSignColonThenNonalnumToToken(char *p)
{
    uint8_t c;

    /* Phase 1: skip digits and '+', '-', ':'. */
    c = (uint8_t)*p;
    while (c != 0u) {
        if (c == '+' || c == '-' || c == ':') {
            p++;
        } else if ((g_charClass[(int8_t)c] & 0x04u) != 0u) {   /* _DIGIT */
            p++;
        } else {
            break;
        }
        c = (uint8_t)*p;
    }

    /* Phase 2: skip bytes that are neither alnum nor one of ';', ',', '+', '-'. */
    c = (uint8_t)*p;
    while (c != 0u
           && c != ';' && c != ',' && c != '+' && c != '-'
           && (g_charClass[(int8_t)c] & 0x07u) == 0u) {        /* _UPPER|_LOWER|_DIGIT */
        p++;
        c = (uint8_t)*p;
    }

    return p;   /* first byte not consumed by phase 2 */
}
#endif /* NW_UNIT_SkipDigitSignColonThenNonalnumToToken */

/* ==========================================================================
 * Unit: SkipQuotedTokenThenControlCharsToToken
 * Source: runtime/SkipQuotedTokenThenControlCharsToToken.c
 * ========================================================================== */
#ifdef NW_UNIT_SkipQuotedTokenThenControlCharsToToken
/*
 * Skip Quoted Token Then Control Chars To Token.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-skipquotedtokenthencontrolcharstotoken-c
 */

#include <stdint.h>

char *SkipQuotedTokenThenControlCharsToToken(char *p)
{
    uint8_t c;
    int     bInQuotes = 0;   /* EBX: 0/1, toggled on every '"' */
    int     bDone     = 0;   /* EDX: only ever set to 1 */

    /* Phase 1: walk one token; '"' toggles bInQuotes; stop on NUL, or on a space/tab seen with
       bInQuotes == 0 (advancing past that one delimiter). */
    for (;;) {
        c = (uint8_t)*p;

        if (c == 0x22u) {                       /* '"' */
            bInQuotes ^= 1;
            p++;
        } else if (c == 0x00u) {                /* NUL */
            bDone = 1;                          /* stop WITHOUT advancing */
        } else if (c == 0x20u || c == 0x09u) {  /* ' ' or TAB */
            if (bInQuotes != 0) {
                p++;                            /* inside quotes: not a delimiter */
            } else {
                bDone = 1;                      /* outside quotes: stop ... */
                p++;                            /* ... and advance (fall-through) */
            }
        } else {
            p++;                                /* anything else: advance */
        }

        if (bDone != 0) {
            break;
        }
    }

    c = (uint8_t)*p;
    while ((int8_t)c <= 0x20 && c != 0x00u) {
        p++;
        c = (uint8_t)*p;
    }

    return p;   /* first byte that is NUL or signed > 0x20 */
}
#endif /* NW_UNIT_SkipQuotedTokenThenControlCharsToToken */
