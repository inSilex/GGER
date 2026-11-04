/*
 * Time -- local time, timezone state, and calendar conversion.
 *
 * Module: runtime
 * Interface: include/Time.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   CaptureLocalTimeAsEpochSeconds                             runtime/CaptureLocalTimeAsEpochSeconds.c
 *   ConvertGmtStructToEpochSeconds                             runtime/ConvertGmtStructToEpochSeconds.c
 *   GetDaylightSavingTimeOffsetSeconds                         runtime/GetDaylightSavingTimeOffsetSeconds.c
 *   GetFirstSundayDayOfYear                                    runtime/GetFirstSundayDayOfYear.c
 *   GetLastSundayDayOfYear                                     runtime/GetLastSundayDayOfYear.c
 *   GetZeroBasedDayOfYearFromTimeRecord                        runtime/GetZeroBasedDayOfYearFromTimeRecord.c
 *   InitializeTimeZoneStandardAndDaylightState                 runtime/InitializeTimeZoneStandardAndDaylightState.c
 *   InitializeTimeZoneStandardAndDaylightStateOnce             runtime/InitializeTimeZoneStandardAndDaylightStateOnce.c
 */

/* ==========================================================================
 * Unit: CaptureLocalTimeAsEpochSeconds
 * Source: runtime/CaptureLocalTimeAsEpochSeconds.c
 * ========================================================================== */
#ifdef NW_UNIT_CaptureLocalTimeAsEpochSeconds
/*
 * Capture Local Time As Epoch Seconds.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-capturelocaltimeasepochseconds-c
 */

#include <stdint.h>

/* Win32 SYSTEMTIME: eight consecutive 16-bit fields, exactly as the
 * GetLocalTime IAT thunk writes them at [ESP+0x04]..[ESP+0x13]. */
typedef struct {
    uint16_t year;
    uint16_t month;        /* 1-BASED, as Win32 defines it */
    uint16_t dayOfWeek;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint16_t milliseconds;
} SystemTime;

/* The consumer structure: 1-BASED month, ABSOLUTE year, NOT struct tm.
 * Only the first six fields are written here; the trailing three dwords
 * exist so the object is the full 36 bytes ConvertGmtStructToEpochSeconds
 * copies with REP MOVSD ECX=9, and they are left as-is (never read by it). */
typedef struct {
    int32_t second;
    int32_t minute;
    int32_t hour;
    int32_t dayOfMonth;
    int32_t month;
    int32_t year;
    int32_t copiedButUnused0;
    int32_t copiedButUnused1;
    int32_t copiedButUnused2;
} GmtBreakdown;

extern uint16_t g_last_msec_field;

/* stdcall: it pops its own 4-byte argument, so ESP is back to
 * entry_ESP-0x38 for every MOVZX that follows. */
extern void __stdcall GetLocalTime(SystemTime *st);

extern int32_t ConvertGmtStructToEpochSeconds(int32_t *pt);

int32_t CaptureLocalTimeAsEpochSeconds(int32_t *outSeconds)
{
    SystemTime   st;   
    GmtBreakdown gmt;  
    int32_t      epoch;

    GetLocalTime(&st);                                  

    /* Field shuffle: each WORD is zero-extended into a DWORD slot, and the
     * six fields ConvertGmtStructToEpochSeconds reads land at offsets
     * +0,+4,+8,+0xc,+0x10,+0x14 of the structure at [ESP+0x14].
     * wDayOfWeek ([ESP+0x08]) is deliberately not copied anywhere. */
    gmt.second     = st.second;     /* [ESP+0x10] -> [ESP+0x14]  +0x00 */
    gmt.minute     = st.minute;     /* [ESP+0x0e] -> [ESP+0x18]  +0x04 */
    gmt.hour       = st.hour;       /* [ESP+0x0c] -> [ESP+0x1c]  +0x08 */
    gmt.dayOfMonth = st.day;        /* [ESP+0x0a] -> [ESP+0x20]  +0x0c */
    gmt.month      = st.month;      /* [ESP+0x06] -> [ESP+0x24]  +0x10 */
    gmt.year       = st.year;       /* [ESP+0x04] -> [ESP+0x28]  +0x14 */

    g_last_msec_field = st.milliseconds;   

    epoch = ConvertGmtStructToEpochSeconds((int32_t *)&gmt);   

    if (outSeconds != 0)
        *outSeconds = epoch;

    return epoch;
}
#endif /* NW_UNIT_CaptureLocalTimeAsEpochSeconds */

/* ==========================================================================
 * Unit: ConvertGmtStructToEpochSeconds
 * Source: runtime/ConvertGmtStructToEpochSeconds.c
 * ========================================================================== */
#ifdef NW_UNIT_ConvertGmtStructToEpochSeconds
/*
 * Convert Gmt Struct To Epoch Seconds.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-convertgmtstructtoepochseconds-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define SECONDS_PER_DAY    0x15180   /* 86400 */
#define SECONDS_PER_HOUR   0x0e10    /* 3600  */
#define SECONDS_PER_MINUTE 0x3c      /* 60    */
#define DAYS_PER_YEAR      0x16d     /* 365   */
#define EPOCH_YEAR         0x7b2     /* 1970  */

extern uint16_t g_days_before_month[12]; 

void InitializeTimeZoneStandardAndDaylightStateOnce(void);

int32_t GetDaylightSavingTimeOffsetSeconds(int32_t *localTtm);

int32_t ConvertGmtStructToEpochSeconds(int32_t *pt)
{
    int32_t localTtm[9];
    int32_t year, month, day, hour, minute, second;
    int32_t monthIndex, dayOfYear, daysFromEpoch, seconds;
    int32_t i;

    second = pt[0];
    minute = pt[1];
    hour   = pt[2];
    day    = pt[3];
    month  = pt[4];
    year   = pt[5];

    if (year < EPOCH_YEAR)
        return -1;

    monthIndex = month - 1;
    dayOfYear  = (int32_t)g_days_before_month[monthIndex] + day - 1;

    if ((year % 4) == 0 && monthIndex > 1)
        dayOfYear += 1;

    daysFromEpoch = (year - EPOCH_YEAR) * DAYS_PER_YEAR
                  + ((year - EPOCH_YEAR + 1) >> 2)   /* SAR ECX,2 */
                  + dayOfYear;

    seconds = daysFromEpoch * SECONDS_PER_DAY
            + (hour * SECONDS_PER_MINUTE + minute) * SECONDS_PER_MINUTE
            + second;

    /* Unconditional: the timezone globals are about to be read below. */
    InitializeTimeZoneStandardAndDaylightStateOnce();

    seconds += g_timezone;

    if (g_daylight != 0) {
        for (i = 0; i < 9; i++)                 /* MOVSD REP ES:EDI, ECX=9 */
            localTtm[i] = pt[i];
        localTtm[4] -= 1;                       /* DEC dword ptr [ESP+0x20]  */
        localTtm[5] -= 1900;                    /* ADD dword ptr [ESP+0x24],-0x76c */
        seconds -= GetDaylightSavingTimeOffsetSeconds(localTtm);

    }

    return seconds;
}
#endif /* NW_UNIT_ConvertGmtStructToEpochSeconds */

/* ==========================================================================
 * Unit: GetDaylightSavingTimeOffsetSeconds
 * Source: runtime/GetDaylightSavingTimeOffsetSeconds.c
 * ========================================================================== */
#ifdef NW_UNIT_GetDaylightSavingTimeOffsetSeconds
/*
 * Returns the daylight-saving offset in seconds (3600 while daylight saving is in effect, else 0): rejects months outside April..October.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-getdaylightsavingtimeoffsetseconds-c
 */

#include <windows.h>
#include <stdint.h>

typedef struct {
    int32_t nSec;    // +0x00 seconds
    int32_t nMin;    // +0x04 minutes
    int32_t nHour;   // +0x08 hours
    int32_t nMday;   // +0x0c day of month  (not read here)
    int32_t nMon;    // +0x10 month index (0-based: January == 0)  <-- read here
    int32_t nYear;   // +0x14 years since 1900  (not read directly here)
} NETWALK_TIMEREC;

extern int GetZeroBasedDayOfYearFromTimeRecord(NETWALK_TIMEREC *pTime);
extern int GetFirstSundayDayOfYear(int nMonthIndex, NETWALK_TIMEREC *pTime);
extern int GetLastSundayDayOfYear(int nMonthIndex, NETWALK_TIMEREC *pTime);

int GetDaylightSavingTimeOffsetSeconds(NETWALK_TIMEREC *pTime)
{
    int nMonth = pTime->nMon;          

    // Months outside April (3) .. October (9) never have daylight saving here.
    if (nMonth < 3 || nMonth > 9)      
        return 0;

    // May..September lie wholly inside the window -> fixed one hour (3600 s).
    if (nMonth > 3 && nMonth < 9)      
        return 0xe10;                  

    // April (3) or October (9): seconds-of-year of the record.
    int nSecondsOfYear = GetZeroBasedDayOfYearFromTimeRecord(pTime) * 0x15180  // day * 86400
                       + pTime->nHour * 0xe10                                    // hour * 3600
                       + pTime->nMin  * 0x3c                                     // min  * 60
                       + pTime->nSec;                                            // sec

    // DST starts 02:00 on the first Sunday of April (month index 3)...
    int nStart = GetFirstSundayDayOfYear(3, pTime) * 0x15180 + 0x1c20;          // + 7200 s (02:00)
    // ...and ends 02:00 on the last Sunday of October (month index 9).
    int nEnd   = GetLastSundayDayOfYear(9, pTime)  * 0x15180 + 0x1c20;          // + 7200 s (02:00)

    if (nSecondsOfYear < nStart || nSecondsOfYear > nEnd)
        return 0;

    return 0xe10;                      // 3600 s: daylight saving in effect
}
#endif /* NW_UNIT_GetDaylightSavingTimeOffsetSeconds */

/* ==========================================================================
 * Unit: GetFirstSundayDayOfYear
 * Source: runtime/GetFirstSundayDayOfYear.c
 * ========================================================================== */
#ifdef NW_UNIT_GetFirstSundayDayOfYear
/*
 * Returns the day-of-year of the first Sunday in a month.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-getfirstsundaydayofyear-c
 */

#include <windows.h>
#include <stdint.h>

typedef struct {
    int32_t nSec;    // +0x00 seconds
    int32_t nMin;    // +0x04 minutes
    int32_t nHour;   // +0x08 hours
    int32_t nMday;   // +0x0c day of month
    int32_t nMon;    // +0x10 month index (0-based) - NOT read by this function
    int32_t nYear;   // +0x14 tm_year: years since 1900 (the field this function uses)
} NETWALK_TIMEREC;

// by a 0-based month index. Entry m = days elapsed before the 1st of month m,
// entry 12 (=365) is the day count before January of the next year (used by the

// Words (decimal): 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
extern const uint16_t wMonthStartDayOfYear[13];

int GetFirstSundayDayOfYear(int nMonthIndex, NETWALK_TIMEREC *pTime)
{
    int nCumDays = (int)wMonthStartDayOfYear[nMonthIndex];  // ESI = days before month (non-leap)
    int nYear    = pTime->nYear + 1900;  // 0x76c: tm_year -> absolute year (field epoch is 1900)
    int nSince70 = pTime->nYear - 70;    // 0x46  : absolute year - 1900 - 70 = years since 1970

    // Leap-day correction: only for months that follow February (index > 1). Divisor is
    // the ABSOLUTE year taken modulo 4 (simple rule; no /100 or /400 Gregorian correction).
    if ((nYear % 4) == 0 && nMonthIndex > 1) {
        nCumDays++;                       // INC ESI
    }

    int nDays = nSince70 * 365;           // 0x16d
    nDays += (nSince70 + 1) >> 2;         // (tm_year - 69) / 4  -- SAR 2, number of leap years since 1970
    nDays += nCumDays;                    // day-of-year of the 1st of the month
    nDays += 4;                           // Jan 1 1970 was a Thursday (Sunday == 0)

    int nRem = nDays % 7;                 // weekday of the 1st (Sunday == 0)
    int nAdd = (nRem == 0) ? 0 : (7 - nRem);  // days forward to the first Sunday

    return nCumDays + nAdd;               // day-of-year (0-based) of the first Sunday
}
#endif /* NW_UNIT_GetFirstSundayDayOfYear */

/* ==========================================================================
 * Unit: GetLastSundayDayOfYear
 * Source: runtime/GetLastSundayDayOfYear.c
 * ========================================================================== */
#ifdef NW_UNIT_GetLastSundayDayOfYear
/*
 * Returns the day-of-year of the last Sunday in a month.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-getlastsundaydayofyear-c
 */

#include <windows.h>
#include <stdint.h>

typedef struct {
    int32_t nSec;    // +0x00 seconds
    int32_t nMin;    // +0x04 minutes
    int32_t nHour;   // +0x08 hours
    int32_t nMday;   // +0x0c day of month
    int32_t nMon;    // +0x10 month index (0-based) - NOT read by this function
    int32_t nYear;   // +0x14 tm_year: years since 1900 (the field this function uses)
} NETWALK_TIMEREC;

// Entry m = days elapsed before the 1st of month m; entry 12 (=365) is the day count
// before January of the next year. Words (decimal):
//   0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
// This function indexes it at +2, i.e. element (nMonthIndex + 1): the day count before
// the FIRST of the FOLLOWING month, which is the 0-based day-of-year of the LAST day of

// nMonthIndex, the day count before the first of the month itself.)
extern const uint16_t wMonthStartDayOfYear[13];

int GetLastSundayDayOfYear(int nMonthIndex, NETWALK_TIMEREC *pTime)
{

    int nLastDayOfMonth = (int)wMonthStartDayOfYear[nMonthIndex + 1] - 1;

    int nYear    = pTime->nYear + 1900;   // 0x76c: tm_year -> absolute year (epoch 1900)
    int nSince70 = pTime->nYear - 70;     // 0x46 : absolute year - 1900 - 70 = years since 1970

    // Leap-day correction: only for months that follow February (index > 1). Divisor is the
    // ABSOLUTE year modulo 4 (simple /4 rule; no /100 or /400 term). NOTE: this gate leaves
    // FEBRUARY itself uncorrected, so in a leap year February is treated as 28 days -- see the

    // from the top and is never clobbered before the compare).
    if ((nYear % 4) == 0 && nMonthIndex > 1) {
        nLastDayOfMonth++;                // INC EBX
    }

    int nDays = nSince70 * 365;           // 0x16d
    nDays += (nSince70 + 1) >> 2;         // (tm_year - 69) / 4, leap years since 1970 (SAR 2)
    nDays += nLastDayOfMonth;             // ADD EAX,EBX
    nDays += 4;                           // ADD EAX,EDI (Jan 1 1970 was a Thursday; Sunday == 0)

    int nRem = nDays % 7;                 // weekday (Sunday == 0) of the month's last day
    return nLastDayOfMonth - nRem;        // POP EAX ; SUB EAX,EDX : step back to the Sunday on/before it

    // DEFECT, stated plainly: because the leap increment is skipped for month index 1, February
    // of a leap year is treated as 28 days, so when Feb 29 falls on a Sunday this returns the
    // Sunday a week early. Over (year, month) 1970-2049 that is exactly 3 of 960 pairs
    // (Feb 1976, Feb 2004, Feb 2032); every other pair matches the real calendar's last Sunday.
    // The sole caller passes month index 9 (October), where the function is exact.
}
#endif /* NW_UNIT_GetLastSundayDayOfYear */

/* ==========================================================================
 * Unit: GetZeroBasedDayOfYearFromTimeRecord
 * Source: runtime/GetZeroBasedDayOfYearFromTimeRecord.c
 * ========================================================================== */
#ifdef NW_UNIT_GetZeroBasedDayOfYearFromTimeRecord
/*
 * Get Zero Based Day Of Year From Time Record.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-getzerobaseddayofyearfromtimerecord-c
 */

#include <windows.h>
#include <stdint.h>

typedef struct {
    int32_t nSec;    // +0x00 seconds
    int32_t nMin;    // +0x04 minutes
    int32_t nHour;   // +0x08 hours
    int32_t nMday;   // +0x0c day of month (1-based)
    int32_t nMon;    // +0x10 month index (0-based: January == 0)
    int32_t nYear;   // +0x14 years since 1900
} NETWALK_TIMEREC;

// 0-BASED month index. Entry m = number of days elapsed before the 1st of month m; entry 12
// (=365) is the day count before January of the next year.
// Words (decimal): 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
// (The word immediately after entry 12 is 0, but it is the start of a following zero-filled
// region, not a coherent 14th month entry -- a 13-entry table already spans the year.)
extern const uint16_t wMonthStartDayOfYear[13];

int GetZeroBasedDayOfYearFromTimeRecord(NETWALK_TIMEREC *pTime)
{
    int nMonth     = pTime->nMon;                          // ECX = [ESI+0x10], used as index (0-based)
    int nYear      = pTime->nYear + 1900;                  // EAX = [ESI+0x14] + 0x76c (tm-epoch 1900)
    int nDayOfYear = (int)wMonthStartDayOfYear[nMonth]     
                     + pTime->nMday - 1;                   // ADD EBX,[ESI+0xc] ; DEC EBX

    // Leap fold: absolute year divisible by 4, and month index > 1 (March onward, i.e. past

    if ((nYear % 4) == 0 && nMonth > 1) {
        nDayOfYear++;                                      // INC EBX
    }

    return nDayOfYear;                                     
}
#endif /* NW_UNIT_GetZeroBasedDayOfYearFromTimeRecord */

/* ==========================================================================
 * Unit: InitializeTimeZoneStandardAndDaylightState
 * Source: runtime/InitializeTimeZoneStandardAndDaylightState.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeTimeZoneStandardAndDaylightState
/*
 * Initialize Time Zone Standard And Daylight State.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-initializetimezonestandardanddaylightstate-c
 */

#include <windows.h>
#include <stdint.h>

char * __cdecl FindTableEntryValueByKeyNoCase(char *pszKey);

int    __cdecl ConvertAsciiCharToUpperCase(int iCharCode);

char * __cdecl CopyNulTerminatedString(char *pszDst, const char *pszSrc);

int    __cdecl ParseHmsTimeStringToSignedSeconds(char *pszTime);

extern const unsigned char g_aucCharClass[];

extern int  g_nStandardBiasSeconds;

extern int  g_nDaylightBiasSeconds;

extern int  g_bDaylightPresent;

extern char g_szStandardName[12];

extern char g_szDaylightName[12];

static const char s_szKeyTZ[]  = "TZ";

static const char s_szDefaultStandard[] = "PST";

static const char s_szDefaultDaylight[] = "PDT";

/** One hour expressed in the seconds unit used by the two bias globals. */
#define ONE_HOUR_SECONDS 3600
/** Default standard offset applied by the hard fallback: 0x7080 s = 8 h (US Pacific). */
#define DEFAULT_STANDARD_SECONDS 0x7080

/**
 * True when the byte c must end a zone-name copy run, matching the two inlined stop
 * tests in the body: NUL, a DIGIT (ctype mask 0x4), '-', or '+'. The table index is
 * sign-extended exactly as the machine's MOVSX does, so bytes >= 0x80 index below the
 * table base (meaningless but faithfully reproduced).
 */
static int IsZoneNameStopByte(unsigned char c)
{
    return (c == 0u)
        || ((g_aucCharClass[(int)(int8_t)c] & 0x4u) != 0u)
        || (c == (unsigned char)'-')
        || (c == (unsigned char)'+');
}

void InitializeTimeZoneStandardAndDaylightState(void)
{
    char *pszValue;            
    char *pCursor;             /* EDX: the split cursor                        */
    int   iIndex;              /* EBX: running copy index / loop counter       */
    unsigned char cByte;
    int   iFirstUpper;
    TIME_ZONE_INFORMATION tzi;
    int   iResult;

    pszValue = FindTableEntryValueByKeyNoCase((char *)s_szKeyTZ);   
    if (pszValue == NULL || *pszValue == '\0')                      
        goto SystemFallback;

    /* Shape gate: the value must begin with an ASCII letter (its upper case in 'A'..'Z'). */
    iFirstUpper = ConvertAsciiCharToUpperCase((int)(int8_t)*pszValue);
    if (iFirstUpper < (int)'A')                                    
        goto SystemFallback;
    iFirstUpper = ConvertAsciiCharToUpperCase((int)(int8_t)*pszValue);
    if (iFirstUpper > (int)'Z')                                    
        goto SystemFallback;

    /* ---- Phase (b): copy the STANDARD zone name (at most 9 bytes). ---- */
    pCursor = pszValue;
    iIndex  = 0;
    while (iIndex < 9) {
        cByte = (unsigned char)pCursor[iIndex];
        if (IsZoneNameStopByte(cByte))
            break;
        g_szStandardName[iIndex] = (char)cByte;
        iIndex++;
    }
    g_szStandardName[iIndex] = '\0';           
    pCursor += iIndex;                          /* EDX advanced once per copied byte */

    /* ---- Phase (c): optional DAYLIGHT zone name. ---- */
    cByte = (unsigned char)*pCursor;
    if (cByte != 0u) {
        int bSkip = ((g_aucCharClass[(int)(int8_t)cByte] & 0x4u) != 0u)   /* digit */
                 || (cByte == (unsigned char)':')
                 || (cByte == (unsigned char)'-')
                 || (cByte == (unsigned char)'+')
                 || (cByte == (unsigned char)';');
        if (bSkip) {
            /* Skip a run of { digit, ':', '-', '+', ';' }; the entry consumes the lead byte. */
            do {
                pCursor++;
                cByte = (unsigned char)*pCursor;
            } while (cByte != 0u
                  && (((g_aucCharClass[(int)(int8_t)cByte] & 0x4u) != 0u)
                      || (cByte == (unsigned char)':')
                      || (cByte == (unsigned char)'-')
                      || (cByte == (unsigned char)'+')
                      || (cByte == (unsigned char)';')));
        }
    }

    /* Boolean: non-zero iff the cursor now sits on a non-NUL byte (NEG/SBB/NEG idiom). */
    g_bDaylightPresent = (*pCursor != '\0') ? 1 : 0;     
    if (g_bDaylightPresent != 0) {
        iIndex = 0;
        while (iIndex < 9) {
            cByte = (unsigned char)pCursor[iIndex];
            if (IsZoneNameStopByte(cByte))
                break;
            g_szDaylightName[iIndex] = (char)cByte;
            iIndex++;
        }
        g_szDaylightName[iIndex] = '\0';                 
    } else {
        g_szDaylightName[0] = '\0';                      
    }

    g_nStandardBiasSeconds = ParseHmsTimeStringToSignedSeconds(pszValue);   
    goto PublishDaylightOffset;

SystemFallback:
    /* ---- Phase (e): ask the OS. ---- */
    iResult = (int)GetTimeZoneInformation(&tzi);          
    if (iResult == -1 || iResult == 0)
        goto HardFallback;

    g_nStandardBiasSeconds = tzi.Bias * 60;               /* minutes -> seconds */
    if (tzi.StandardDate.wMonth != 0)                     /* a standard rule is specified */
        g_nStandardBiasSeconds += tzi.StandardBias * 60;
    g_bDaylightPresent = ((tzi.DaylightDate.wMonth != 0) && (tzi.DaylightBias != 0)) ? 1 : 0;
    g_szStandardName[0] = '\0';                           
    g_szDaylightName[0] = '\0';                           
    goto PublishDaylightOffset;

HardFallback:
    CopyNulTerminatedString(g_szStandardName, s_szDefaultStandard);   /* "PST" */
    CopyNulTerminatedString(g_szDaylightName, s_szDefaultDaylight);   /* "PDT" */
    g_bDaylightPresent     = 1;
    g_nStandardBiasSeconds = DEFAULT_STANDARD_SECONDS;    /* 0x7080 = 28800 s = 8 h */
    goto PublishDaylightOffset;

PublishDaylightOffset:

    if (g_bDaylightPresent != 0)
        g_nDaylightBiasSeconds = g_nStandardBiasSeconds - ONE_HOUR_SECONDS;   
}
#endif /* NW_UNIT_InitializeTimeZoneStandardAndDaylightState */

/* ==========================================================================
 * Unit: InitializeTimeZoneStandardAndDaylightStateOnce
 * Source: runtime/InitializeTimeZoneStandardAndDaylightStateOnce.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeTimeZoneStandardAndDaylightStateOnce
/*
 * Initialize Time Zone Standard And Daylight State Once.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-initializetimezonestandardanddaylightstateonce-c
 */

#include <stdint.h>
extern unsigned char nw_data_window[];
#include <windows.h>

/* NetWalk global: "time-zone standard/daylight state has been initialized" flag. */
#define NETWALK_TZ_INIT_FLAG ((DWORD *)(intptr_t)(nw_data_window + 0xcc8))

extern void InitializeTimeZoneStandardAndDaylightState(void);

void InitializeTimeZoneStandardAndDaylightStateOnce(void)
{
    DWORD *pFlag = NETWALK_TZ_INIT_FLAG;

    if (*pFlag != 0) {
        return; 
    }

    InitializeTimeZoneStandardAndDaylightState(); 
    *pFlag += 1;                                   
}
#endif /* NW_UNIT_InitializeTimeZoneStandardAndDaylightStateOnce */
