#include "nwarch.h"
/*
 * TrackedHandles -- Win32 tracked-handle open/close/seek/truncate helpers.
 *
 * Module: io/win32
 * Interface: include/TrackedHandles.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   CloseTrackedHandleSlot                                     io/win32/CloseTrackedHandleSlot.c
 *   IsTrackedHandleCharDevice                                  io/win32/IsTrackedHandleCharDevice.c
 *   OpenTrackedHandleSlot                                      io/win32/OpenTrackedHandleSlot.c
 *   SeekTrackedSlotHandleOffset                                io/win32/SeekTrackedSlotHandleOffset.c
 *   TruncateTrackedSlotFileToSize                              io/win32/TruncateTrackedSlotFileToSize.c
 */

/* ==========================================================================
 * Unit: CloseTrackedHandleSlot
 * Source: io/win32/CloseTrackedHandleSlot.c
 * ========================================================================== */
#ifdef NW_UNIT_CloseTrackedHandleSlot
/*
 * Close Tracked Handle Slot.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-closetrackedhandleslot-c
 */

#include <windows.h>
#include "nwglobals.h"

extern void __fastcall SetThreadContextValueAndClearFlag(DWORD dwUnusedArg1,
                                                         DWORD dwUnusedArg2,
                                                         DWORD dwErrno);

extern DWORD __fastcall TranslateLastErrorToErrno(DWORD dwUnusedArg1,
                                                  DWORD dwUnusedArg2);

/* Tracked-handle slot arrays, 60 slots (0x00..0x3b). */
#define SLOT_FLAG_TABLE   ((void *)(nw_data_window + 0xB7C))  
#define SLOT_HANDLE_TABLE NW_WINDOW_PTR_SLOTS(0xBB8)
#define SLOT_COUNT        0x3c       /* valid nSlotIndex range: 0 <= i < 60 */

#define ERRNO_EBADF 9                /* value pushed for an out-of-range slot */

int __fastcall CloseTrackedHandleSlot(DWORD dwUnusedArg1,
                                      DWORD dwUnusedArg2,
                                      int   nSlotIndex)
{
    HANDLE hSlotHandle;

    if (nSlotIndex < 0 || nSlotIndex >= SLOT_COUNT) {
        SetThreadContextValueAndClearFlag(dwUnusedArg1, dwUnusedArg2, ERRNO_EBADF);
        return -1;
    }

    /* Clear the slot's flag and handle, then close the handle that was just removed. */
    *(BYTE   *)((uintptr_t)SLOT_FLAG_TABLE   + nSlotIndex)    = 0;
    hSlotHandle = ((HANDLE *)SLOT_HANDLE_TABLE)[nSlotIndex];
    ((HANDLE *)SLOT_HANDLE_TABLE)[nSlotIndex] = NULL;

    if (CloseHandle(hSlotHandle))
        return 0;

    TranslateLastErrorToErrno(dwUnusedArg1, dwUnusedArg2);
    return -1;
}

#if !defined(__ELF__) && !defined(_WIN64)
__asm__(".globl _CloseTrackedHandleSlot\n" ".set _CloseTrackedHandleSlot, @CloseTrackedHandleSlot@12\n");
#endif
#define _FIXABI 1
#endif /* NW_UNIT_CloseTrackedHandleSlot */

/* ==========================================================================
 * Unit: IsTrackedHandleCharDevice
 * Source: io/win32/IsTrackedHandleCharDevice.c
 * ========================================================================== */
#ifdef NW_UNIT_IsTrackedHandleCharDevice
/*
 * Is Tracked Handle Char Device.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-istrackedhandlechardevice-c
 */

#include <windows.h>
#include "nwglobals.h"
#include <stdint.h>

#define SLOT_HANDLE_TABLE_BASE NW_WINDOW_PTR_SLOTS(0xBB8)

BOOL IsTrackedHandleCharDevice(int nSlotIndex)
{
    HANDLE hSlotHandle =
        ((HANDLE *)SLOT_HANDLE_TABLE_BASE)[nSlotIndex];

    return GetFileType(hSlotHandle) == FILE_TYPE_CHAR;
}
#endif /* NW_UNIT_IsTrackedHandleCharDevice */

/* ==========================================================================
 * Unit: OpenTrackedHandleSlot
 * Source: io/win32/OpenTrackedHandleSlot.c
 * ========================================================================== */
#ifdef NW_UNIT_OpenTrackedHandleSlot
/*
 * Open Tracked Handle Slot.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-opentrackedhandleslot-c
 */

#include <stdint.h>
#include "nwglobals.h"

typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned int    DWORD;
typedef void           *HANDLE_T; /* Win32 HANDLE */

/* These live in the data window (orig VAs below); BSS extern stubs would
 * silently zero them, so bind directly. */
#define g_bSlotInUse               ((BYTE *)(nw_data_window + 0xB7C))           
#define g_hSlotHandle              ((HANDLE_T *)NW_WINDOW_PTR_SLOTS(0xBB8))
#define g_wSlot3LockCounter        (*(volatile unsigned short *)(nw_data_window + 0x641E)) 
#define g_dwMask_411eb0            (*(DWORD *)(nw_data_window + 0xEB0))          
#define g_bGlobalFlag_41740d       (*(volatile BYTE *)(nw_data_window + 0x640D))     

/* ---- Callees (signatures: parent-resolved where noted) ------------------ */
static HANDLE_T WINAPI_CreateFileA(const char *lpFileName, DWORD dwDesiredAccess, void *lpSecurityDesc,
                                   DWORD dwShareMode, DWORD dwCreationFlags, DWORD dwFlagsAndAttributes,
                                   const void *lpTemplateFile); 

extern void     TranslateLastErrorToErrno(void);                 
extern int      IsTrackedHandleCharDevice(int nSlotIndex);       

extern void __fastcall SetThreadContextValueAndClearFlag(DWORD dwUnusedArg1,
                                                         DWORD dwUnusedArg2,
                                                         DWORD dwValue);   
extern void     CreateSlotSemaphore(int nIndex);                 
extern void     ReleaseSlotSemaphore(int nIndex);     

int OpenTrackedHandleSlot(const char *pszFilename,   /* [EBP+8]    */
                          DWORD dwAccessWord,        
                          DWORD dwModeBits70src,     /* [EBP+0x10] bits 0x70 OR'd into the access word */
                          DWORD dwAttrInput)         /* [EBP+0x14] ANDed with ~g_dwMask_411eb0 for the flags-and-attrs slot */
{
    DWORD       dwAccess;      
    int         nSlotBitsIdx;  /* [EBP-8]   bits 4-7 mapping -> 0..3 */
    DWORD       dwConst1to5;   /* [EBP-0x14] per-index constant {1..5} pushed as CreateFileA arg[4] */
    DWORD       dwAttrOut;     /* [EBP-0x18] ~mask & input, overridden to 0x80/0x1 by high bit */
    HANDLE_T    hResult;       /* [EBP-4]   temp for the CreateFileA return */
    int         nSlotCursor = 3; /* [EBP-0x10] scan cursor / returned slot index */

    dwAccess = (dwAccessWord & 0xff8fu) | (dwModeBits70src & 0x70u);

    DWORD dwAccessConst;
    if (dwAccess & 0x2u)        dwAccessConst = 0xc0000000u; /* live for the 0x8102 callers */
    else if (dwAccess & 0x1u)   dwAccessConst = 0x40000000u;
    else                        dwAccessConst = 0x80000000u;

    {
        unsigned u = dwAccess & 0xf0u;
        if (u == 0x00u || u == 0x10u)      nSlotBitsIdx = 0;
        else if (u == 0x20u)               nSlotBitsIdx = 1;
        else if (u == 0x30u)               nSlotBitsIdx = 2;
        else if (u == 0x40u)               nSlotBitsIdx = 3;
        else                               nSlotBitsIdx = 0; 
    }

    /* --- Attributes output [EBP-0x18] (e07a-e09b) ---------------------------
     * dwAttrOut = (~g_dwMask_411eb0) & dwAttrInput; then if its LOW byte's high bit is set
     * the value collapses to 0x80, else to 0x1. */
    dwAttrOut = (~g_dwMask_411eb0) & dwAttrInput;
    if (dwAttrOut & 0x80u) dwAttrOut = 0x80u; else dwAttrOut = 0x1u;

    /* --- Per-index constant {1..5} [EBP-0x14] from high-byte bits (e09b-e0df)
     * All three tests hit the SAME normalized DWORD's second byte ([EBP+0xd], i.e. bits
     * 8-15; normalization never touches it): bit8 = val 0x1, bit9 = val 0x2, bit10 = val 0x4. */
    {
        unsigned hi = (unsigned)(dwAccess >> 8) & 0xffu;
        if (hi & 0x1u) {                    /* access-word bit 8 */
            if (hi & 0x4u)      dwConst1to5 = 1;   /* bit 10 also set */
            else if (hi & 0x2u) dwConst1to5 = 2;   /* bit 9 only */
            else                dwConst1to5 = 4;
        } else {
            if (hi & 0x2u)      dwConst1to5 = 5;   /* bit 9 without bit 8 */
            else                dwConst1to5 = 3;
        }
    }

    {
        {
            unsigned short usAfter = (unsigned short)__atomic_add_fetch(
                &g_wSlot3LockCounter, 1u, __ATOMIC_SEQ_CST);
            if (usAfter != 0u) {
                CreateSlotSemaphore(3);                    
            }
        }
    }

    nSlotCursor = 3;
    for (;;) {
        if (nSlotCursor == 60) {            /* CMP [EBP-0x10], 0x3c -- no free slot */
            SetThreadContextValueAndClearFlag(0u, 0u, 0x18u);  
            nSlotCursor = -1;                    
            goto release_cs_and_return;          /* JMP e1b9 */
        }
        if (g_hSlotHandle[nSlotCursor] != NULL) {  
            ++nSlotCursor;                     /* INC [EBP-0x10]; JMP e0ff */
            continue;
        }
        break;                                /* free slot found at i (e12a) */
    }

    hResult = WINAPI_CreateFileA(pszFilename, dwAccessConst, (void *)(uintptr_t)nSlotBitsIdx,
                                 0u, dwConst1to5, dwAttrOut, NULL);

    g_bSlotInUse[nSlotCursor] = 0;             /* e146-e149: reset the flag byte BEFORE outcome branch */

    /* INVALID_HANDLE_VALUE is all-bits-one, not the 32-bit 0xffffffff.
     * Comparing only the low dword accepted a failed 64-bit CreateFileA as a
     * live slot, so the save fallback never ran and every later read/write
     * used the invalid handle. */
    if (hResult == (HANDLE_T)(intptr_t)-1) {
        TranslateLastErrorToErrno();            
        nSlotCursor = -1;                       
        goto release_cs_and_return;             /* JMP e1b9 */
    }

    g_hSlotHandle[nSlotCursor] = hResult;      /* e164-e16a: record the handle in the slot */

    if (dwAccess & 0x8u)                       
        g_bSlotInUse[nSlotCursor] |= 0x4;      /* e175-e17a: flag bit 2 mirrors the request */

    if (IsTrackedHandleCharDevice(nSlotCursor)) {   
        g_bSlotInUse[nSlotCursor] |= 0x8;      /* e190-e193: bit 3 = character device (GetFileType) */
    }

    {
        unsigned hi = (unsigned)(dwAccess >> 8) & 0xffu;
        if ((hi & 0x40u) != 0u ||
            ((hi & 0x80u) == 0u && (g_bGlobalFlag_41740d & 0x80u) == 0u)) {
            g_bSlotInUse[nSlotCursor] |= 0x10;
        }
    }

release_cs_and_return:

    {
        unsigned short usAfter = (unsigned short)__atomic_sub_fetch(
            &g_wSlot3LockCounter, 1u, __ATOMIC_SEQ_CST);
        if ((int16_t)usAfter >= 0) {
            ReleaseSlotSemaphore(3);
        }
    }

    return nSlotCursor;                        
}

extern void *__attribute__((dllimport)) __stdcall CreateFileA(const char *, unsigned int, const void *,
                                                             unsigned int, unsigned int, unsigned int,
                                                             const void *);
static HANDLE_T WINAPI_CreateFileA(const char *lpFileName, DWORD dwDesiredAccess, void *lpSecurityDesc,
                                   DWORD dwShareMode, DWORD dwCreationFlags, DWORD dwFlagsAndAttributes,
                                   const void *lpTemplateFile)
{

    return (HANDLE_T)CreateFileA(lpFileName, dwDesiredAccess,
                                 (void *)(uintptr_t)lpSecurityDesc,
                                 dwShareMode, dwCreationFlags,
                                 dwFlagsAndAttributes,
                                 (const void *)(uintptr_t)lpTemplateFile);
}
#endif /* NW_UNIT_OpenTrackedHandleSlot */

/* ==========================================================================
 * Unit: SeekTrackedSlotHandleOffset
 * Source: io/win32/SeekTrackedSlotHandleOffset.c
 * ========================================================================== */
#ifdef NW_UNIT_SeekTrackedSlotHandleOffset
/*
 * Seeks the Win32 handle held in tracked-handle slot nSlotIndex to lDistanceToMove using dwMoveMethod.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-seektrackedslothandleoffset-c
 */

#include <windows.h>
#include "nwglobals.h"

extern DWORD __fastcall TranslateLastErrorToErrno(DWORD dwUnusedArg1,
                                                  DWORD dwUnusedArg2);

#define SLOT_HANDLE_TABLE NW_WINDOW_PTR_SLOTS(0xBB8)

#define SETFILEPOINTER_INVALID ((DWORD)0xffffffffL)   /* return value on failure */

DWORD __cdecl SeekTrackedSlotHandleOffset(int   nSlotIndex,
                                          LONG  lDistanceToMove,
                                          DWORD dwMoveMethod)
{
    HANDLE hSlotHandle;
    DWORD  dwNewFilePos;

#ifdef NWDEBUG_ENABLED
    extern void nwsafe_trace(const char *);
    extern void nwtrace_hex(const char *, unsigned long);
    nwsafe_trace("stsho_in");
#endif

    hSlotHandle = ((HANDLE *)SLOT_HANDLE_TABLE)[nSlotIndex];
#ifdef NWDEBUG_ENABLED
    { int _si = nSlotIndex; unsigned long _h, _m, _d, _r;
      nwtrace_hex("stsho_slotidx", (unsigned long)_si);
      nwtrace_hex("stsho_off", (unsigned long)lDistanceToMove & 0xFFFFFFFu ? ((unsigned long)lDistanceToMove & 0xFFFFFFFFu) : 4294967285u);
      nwtrace_hex("stsho_meth", (unsigned long)dwMoveMethod + (dwMoveMethod==2)?1:0x10000u);
      nwtrace_hex("stsho_handle", (_h=(unsigned long)hSlotHandle)?_h:12345678u);
    }
#endif

#ifdef NWDEBUG_ENABLED
    nwsafe_trace("stsho_pre_sfp");
#endif

    dwNewFilePos = SetFilePointer(hSlotHandle,
                                  lDistanceToMove,
                                  NULL,
                                  dwMoveMethod);
#ifdef NWDEBUG_ENABLED
    nwtrace_hex("stsho_pos", (unsigned long)dwNewFilePos ? (unsigned long)dwNewFilePos : 4294967285u);
#endif
#ifdef NWDEBUG_ENABLED
    nwsafe_trace("stsho_done");
#endif
    if (dwNewFilePos == SETFILEPOINTER_INVALID) {
        TranslateLastErrorToErrno(0, 0);   /* ECX/EDX pass-through args, never read */
    }

    return dwNewFilePos;
}
#endif /* NW_UNIT_SeekTrackedSlotHandleOffset */

/* ==========================================================================
 * Unit: TruncateTrackedSlotFileToSize
 * Source: io/win32/TruncateTrackedSlotFileToSize.c
 * ========================================================================== */
#ifdef NW_UNIT_TruncateTrackedSlotFileToSize
/*
 * Truncates the file in tracked-handle slot nSlotIndex to dwNewSize bytes by seeking there with SetFilePointer(FILE_BEGIN) and calling SetEndOfFile.
 *
 * Module: io/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#io-win32-truncatetrackedslotfiletosize-c
 */

#include <windows.h>
#include "nwglobals.h"

extern void __fastcall TranslateLastErrorToErrno(DWORD dwUnusedArg1,
                                                 DWORD dwUnusedArg2);

#define SLOT_HANDLE_TABLE NW_WINDOW_PTR_SLOTS(0xBB8)

#define SETFILEPOINTER_INVALID ((DWORD)0xffffffffL)   /* seek-failure return   */
#define MOVE_METHOD_BEGIN      0                     /* FILE_BEGIN / SEEK_SET */

int __cdecl TruncateTrackedSlotFileToSize(int   nSlotIndex,
                                          DWORD dwNewSize)
{
    HANDLE hSlotHandle;
    DWORD  dwNewFilePos;

    /* No bounds check here: raw index into hSlotHandle[60]. */
    hSlotHandle = ((HANDLE *)SLOT_HANDLE_TABLE)[nSlotIndex];

    /* Absolute seek to the new end-of-file position; NULL high dword => zero-extended. */
    dwNewFilePos = SetFilePointer(hSlotHandle,
                                  (LONG)dwNewSize,
                                  NULL,
                                  MOVE_METHOD_BEGIN);
    if (dwNewFilePos == dwNewSize) {
        if (SetEndOfFile(hSlotHandle)) {   /* cut the file at the position just set */
            return 0;
        }
    }

    /* Seek failed, or the truncate failed: one shared edge caches errno in the context. */
    TranslateLastErrorToErrno(0, 0);       /* ECX/EDX pass-through args, never read */
    return -1;
}
#endif /* NW_UNIT_TruncateTrackedSlotFileToSize */
