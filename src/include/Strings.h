/*
 * Strings -- C string, character, number, and format parsing helpers.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_RUNTIME_STRINGS_C_H
#define NW_GROUP_RUNTIME_STRINGS_C_H

#include <stdint.h>
#include <stddef.h>
#include <windows.h>

#ifndef NW_GROUP_COMPAT_TYPES
#define NW_GROUP_COMPAT_TYPES
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef unsigned int uint;
typedef unsigned char byte;
typedef uint32_t dword;
typedef void *HANDLE_T;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Units in this group: */
/*   AppendCStringToDest                                        runtime/AppendCStringToDest.c */
/*   CompareBoundedAsciiStringsNoCase                           runtime/CompareBoundedAsciiStringsNoCase.c */
/*   ConvertAsciiCharToLowerCase                                runtime/ConvertAsciiCharToLowerCase.c */
/*   ConvertAsciiCharToUpperCase                                runtime/ConvertAsciiCharToUpperCase.c */
/*   ConvertAsciiStringToUpperCase                              runtime/ConvertAsciiStringToUpperCase.c */
/*   ConvertIntegerToString                                     runtime/ConvertIntegerToString.c */
/*   CopyBoundedStringNulPadded                                 runtime/CopyBoundedStringNulPadded.c */
/*   CopyNulTerminatedString                                    runtime/CopyNulTerminatedString.c */
/*   FindFirstCharInString                                      runtime/FindFirstCharInString.c */
/*   FindFirstSubstringInString                                 runtime/FindFirstSubstringInString.c */
/*   FindLastCharInString                                       runtime/FindLastCharInString.c */
/*   GetStringLength                                            runtime/GetStringLength.c */
/*   ParseBracketCharClassAndMatchChars                         runtime/ParseBracketCharClassAndMatchChars.c */
/*   ParseHmsTimeStringToSignedSeconds                          runtime/ParseHmsTimeStringToSignedSeconds.c */
/*   ParseSignedDecimalIntegerFromString                        runtime/ParseSignedDecimalIntegerFromString.c */
/*   ParseSignedLongFromStringBaseN                             runtime/ParseSignedLongFromStringBaseN.c */
/*   ScanFormatConversions                                      runtime/ScanFormatConversions.c */
/*   SkipDigitSignColonThenNonalnumToToken                      runtime/SkipDigitSignColonThenNonalnumToToken.c */
/*   SkipQuotedTokenThenControlCharsToToken                     runtime/SkipQuotedTokenThenControlCharsToToken.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern char *AppendCStringToDest (char *pDestBuf, const char *pszSrc);
extern int CompareBoundedAsciiStringsNoCase (const char *pszLeft, const char *pszRight, int nMaxCount);
extern int ConvertAsciiCharToLowerCase (int iCharCode);
extern int ConvertAsciiCharToUpperCase (int iCharCode);
extern char *ConvertAsciiStringToUpperCase (char *pszString);
extern char *ConvertIntegerToString (int iValue, char *pDest, unsigned int uBase);
extern char *CopyBoundedStringNulPadded (uint *puDest, uint *puSrc, uint dwCount);
extern char *CopyNulTerminatedString (char *pszDst, const char *pszSrc);
extern char *FindFirstCharInString (char *szString, char cSearch);
extern char *FindFirstSubstringInString (const char *pszHaystack, const char *pszNeedle);
extern char *FindLastCharInString (const char *pszSrc, unsigned char chTarget);
extern int GetStringLength (const char *pszStr);
extern int ParseHmsTimeStringToSignedSeconds (char *p);
extern int ParseSignedDecimalIntegerFromString (byte *pbStr);
extern long int ParseSignedLongFromStringBaseN (char *szString, char **ppEnd, int nBase);
extern char *SkipDigitSignColonThenNonalnumToToken (char *p);
extern char *SkipQuotedTokenThenControlCharsToToken (char *p);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_RUNTIME_STRINGS_C_H */
