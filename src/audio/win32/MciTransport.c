/*
 * MciTransport -- MCI device open/close, command dispatch, and error reporting.
 *
 * Module: audio/win32
 * Interface: include/MciTransport.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   CloseMciCdAudioDeviceAndClearEnabledFlag                   audio/win32/CloseMciCdAudioDeviceAndClearEnabledFlag.c
 *   DispatchMciCommandViaGlobalHandleAndShowError              audio/win32/DispatchMciCommandViaGlobalHandleAndShowError.c
 *   DispatchMciPlaybackCommand                                 audio/win32/DispatchMciPlaybackCommand.c
 *   DispatchMciSetFlagsAndPlay                                 audio/win32/DispatchMciSetFlagsAndPlay.c
 *   InitializeAudioSubsystemByOsVersionAndOpenMciDevice        audio/win32/InitializeAudioSubsystemByOsVersionAndOpenMciDevice.c
 *   OpenMciCdAudioDeviceAndArmEnabledFlag                      audio/win32/OpenMciCdAudioDeviceAndArmEnabledFlag.c
 *   ShowMciErrorMessage                                        audio/win32/ShowMciErrorMessage.c
 */

/* ==========================================================================
 * Unit: CloseMciCdAudioDeviceAndClearEnabledFlag
 * Source: audio/win32/CloseMciCdAudioDeviceAndClearEnabledFlag.c
 * ========================================================================== */
#ifdef NW_UNIT_CloseMciCdAudioDeviceAndClearEnabledFlag
/*
 * Close Mci Cd Audio Device And Clear Enabled Flag.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-closemcicdaudiodeviceandclearenabledflag-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <windows.h>   /* pulls in mmsystem.h -> mciapi.h: MCI_CLOSE, MCIDEVICEID */
#include <mmsystem.h>

/* Persistent MCI transport state, owned by the low-0x40a audio cluster. Arm on open, clear on close. */

void CloseMciCdAudioDeviceAndClearEnabledFlag(void)
{
    /* Cross-platform stub: the audio CD transport is an unavailable device, so
     * nothing was ever opened and there is no mciSendCommandA(MCI_CLOSE) left in
     * the audio path.  The video MCI device in MoviePlayback.c is separate. */
    g_dwMciTaskId       = 0;
    g_dwMciAudioEnabled = 0;
}
#endif /* NW_UNIT_CloseMciCdAudioDeviceAndClearEnabledFlag */

/* ==========================================================================
 * Unit: DispatchMciCommandViaGlobalHandleAndShowError
 * Source: audio/win32/DispatchMciCommandViaGlobalHandleAndShowError.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchMciCommandViaGlobalHandleAndShowError
/*
 * Dispatch Mci Command Via Global Handle And Show Error.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-dispatchmcicommandviaglobalhandleandshowerror-c
 */

#include <stdint.h>

typedef uint32_t MCIERROR;

extern uint32_t g_mciDeviceId; 

extern MCIERROR __stdcall mciSendCommandA(uint32_t idDevice, uint32_t uCommand, uint32_t uMode, uint32_t dwParam);

extern int __stdcall ShowMciErrorMessage(MCIERROR uCode); 

int __stdcall DispatchMciCommandViaGlobalHandleAndShowError(uint32_t param_1, uint32_t param_2, uint32_t param_3) {
    if (g_mciDeviceId == 0u) return 0; // CMP/JZ — MCI path skipped entirely when no device armed
    MCIERROR err = mciSendCommandA(g_mciDeviceId, param_3, param_2, param_1);
    int result = ShowMciErrorMessage(err);
    return (int8_t)result; // MOVSX EAX,AL — sign-extend the low byte before returning
}
#endif /* NW_UNIT_DispatchMciCommandViaGlobalHandleAndShowError */

/* ==========================================================================
 * Unit: DispatchMciPlaybackCommand
 * Source: audio/win32/DispatchMciPlaybackCommand.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchMciPlaybackCommand
/*
 * Dispatch Mci Playback Command.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-dispatchmciplaybackcommand-c
 */

#include <stdint.h>
#include "nwglobals.h"
/* Win32 multimedia (winmm.dll). mciSendCommandA / MCIDEVICEID come from windows.h -> mmsystem.h. */
#include <windows.h>

typedef DWORD MCIERROR;   /* winmm error code, low 16 bits are the error number */

/* Persistent audio-transport state owned by the VM-support cluster (data, not callees). */

/* MCI command numbers (mmsystem.h). Kept as raw values to match the binary; named for clarity. */
#define NW_MCI_STOP_CMD    0x808   /* MCI_STOP                          */
#define NW_MCI_PAUSE_CMD   0x809   /* MCI_PAUSE                         */
#define NW_MCI_RESUME_CMD  0x855   /* MCI_RESUME                        */
#define NW_MCI_SET_CMD     0x80d   /* MCI_SET                           */
#define NW_MCI_STATUS_CMD  0x814   /* MCI_STATUS (status query, op1/op2)*/

/* Status-mode result values read back by the STATUS query in ops 1/2 (MCI_STRING_OFFSET + N). */
#define NW_MODE_PLAYING    0x20e   /* STRING_OFFSET+14 == MCI_MODE_PLAY   */
#define NW_MODE_PAUSED     0x211   /* STRING_OFFSET+17 == MCI_MODE_PAUSE  */

/* Item/flag words used on the command path. */
#define NW_STATUS_ITEM_FLAG 0x100  /* MCI_STATUS_ITEM                     */
#define NW_STATUS_MODE_ITEM 4      /* MCI_STATUS_MODE (dwItem selector)   */

DWORD __stdcall DispatchMciPlaybackCommand(DWORD dwOpCode)
{
    MCI_STATUS_PARMS parms;   /* shared parms block: dwReturn@+4, dwItem@+8 */
    DWORD  dwResult = 1;      /* returned status; default success indicator */

    if (g_dwMciAudioEnabled != 0) {
        switch (dwOpCode) {
        case 0:
            /* Stop the transport unconditionally; parms contents are unused. */
            dwResult = mciSendCommandA(g_dwMciTaskId, NW_MCI_STOP_CMD, 0, (DWORD_PTR)&parms);
            break;

        case 1:

            parms.dwItem = NW_STATUS_MODE_ITEM;
            dwResult = mciSendCommandA(g_dwMciTaskId, NW_MCI_STATUS_CMD, NW_STATUS_ITEM_FLAG, (DWORD_PTR)&parms);
            if (parms.dwReturn == NW_MODE_PLAYING) {
                dwResult = mciSendCommandA(g_dwMciTaskId, NW_MCI_PAUSE_CMD, 0, (DWORD_PTR)&parms);
            }
            break;

        case 2:
            /* Resume — but only if currently PAUSED (status-mode == MCI_MODE_PAUSE). */
            parms.dwItem = NW_STATUS_MODE_ITEM;
            dwResult = mciSendCommandA(g_dwMciTaskId, NW_MCI_STATUS_CMD, NW_STATUS_ITEM_FLAG, (DWORD_PTR)&parms);
            if (parms.dwReturn == NW_MODE_PAUSED) {
                dwResult = mciSendCommandA(g_dwMciTaskId, NW_MCI_RESUME_CMD, 0, (DWORD_PTR)&parms);
            }
            break;

        case 3:
            /* Set parameters on the transport; parms contents are unused. */
            dwResult = mciSendCommandA(g_dwMciTaskId, NW_MCI_SET_CMD, NW_STATUS_ITEM_FLAG, (DWORD_PTR)&parms);
            break;
        }
    }

    return (dwResult & 0xFFFFFF00u) | 1u;
}
#endif /* NW_UNIT_DispatchMciPlaybackCommand */

/* ==========================================================================
 * Unit: DispatchMciSetFlagsAndPlay
 * Source: audio/win32/DispatchMciSetFlagsAndPlay.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchMciSetFlagsAndPlay
/*
 * Dispatch Mci Set Flags And Play.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-dispatchmcisetflagsandplay-c
 */

#include <stdint.h>
#include "nwglobals.h"
/* Win32 multimedia (winmm.dll). mciSendCommandA / MCIDEVICEID come from windows.h -> mmsystem.h. */
#include <windows.h>

typedef DWORD MCIERROR;   /* winmm error code, low 16 bits are the error number */

/* Persistent audio-transport state owned by the VM-support cluster (data, not callees). */

/* MCI commands (mmsystem.h, MCI_FIRST = 0x800). */
#define NW_MCI_SET_CMD       0x80d  /* MCI_SET                    */
#define NW_MCI_PLAY_CMD      0x806  /* MCI_PLAY                   */

/* Item word on the MCI_SET call. */
#define NW_MCI_SET_VIDEO_ALL 0x400  /* MCI_SET_VIDEO_ALL: flags go in the buffer, not the item word */

/* Flag word written at buffer+4 before the SET: 10 == AVIIF_HIRES(0x02) | AVIIF_FRAMERATEAVG(0x08). */
#define NW_MCI_SET_VIDEO_FLAGS 10u

/* dwParam of the PLAY call; not a MCI_WAIT/MCI_NOTIFY value, passed through as a mode word. */
#define NW_MCI_PLAY_PARAM     4u

DWORD DispatchMciSetFlagsAndPlay(BYTE bFlag, DWORD dwInEaxImage)
{

    DWORD dwSetBuf[2]  = { 0, 0 };
    DWORD dwPlayBuf[2] = { 0, 0 };
    DWORD dwResult;

    if (g_dwMciAudioEnabled == 0) {
        return dwInEaxImage;
    }

    dwSetBuf[1] = NW_MCI_SET_VIDEO_FLAGS;
    dwResult = mciSendCommandA(g_dwMciTaskId, NW_MCI_SET_CMD, NW_MCI_SET_VIDEO_ALL, (DWORD_PTR)dwSetBuf);

    if (dwResult != 0) {
        return dwResult;
    }

    dwPlayBuf[1] = (DWORD)bFlag;
    dwResult = mciSendCommandA(g_dwMciTaskId, NW_MCI_PLAY_CMD, NW_MCI_PLAY_PARAM, (DWORD_PTR)dwPlayBuf);

    /* NEG / SBB / INC: return 1 when the PLAY succeeded, 0 when it failed. */
    return (dwResult == 0) ? 1u : 0u;
}
#endif /* NW_UNIT_DispatchMciSetFlagsAndPlay */

/* ==========================================================================
 * Unit: InitializeAudioSubsystemByOsVersionAndOpenMciDevice
 * Source: audio/win32/InitializeAudioSubsystemByOsVersionAndOpenMciDevice.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeAudioSubsystemByOsVersionAndOpenMciDevice
/*
 * Initialize Audio Subsystem By Os Version And Open Mci Device.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-initializeaudiosubsystembyosversionandopenmcidevice-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <windows.h>   /* BYTE, BOOL, DWORD, HANDLE, HWND, WINAPI, GetVersion */



/* --- slot-table globals (same cells src/audio/ClearAllSlotEntries.c already consumes) --- */


/* src/audio/win32/InitializeUtDriverAndHandlerTable.c declares this table as `FARPROC[12]`; here it
 * is declared with the handler signature the UT dispatch layer actually uses (no
 * arguments, result in AL) so the two indirect calls need no function-pointer cast.
 * Same 12 slots either way. */
typedef BYTE (WINAPI *PFN_UT_SAMPLE_HANDLER)(void);
extern PFN_UT_SAMPLE_HANDLER g_apfnUtNotify[12];

/* --- the two initialisers (chosen at run time) and the MCI leaf --- */

extern BOOL WINAPI InitializeUtDriverAndHandlerTable(void *pUtContext);        
extern BOOL WINAPI InitializeAudioSubsystemAndUtHandlerTable(void *pParams);   
extern DWORD OpenMciCdAudioDeviceAndArmEnabledFlag(void);                      

BOOL WINAPI InitializeAudioSubsystemByOsVersionAndOpenMciDevice(
    BYTE  *pbOutSlot5,   
    BYTE  *pbOutSlot4,   
    DWORD  dwUnusedArg3, /* ARG3 @[ESP+0x10] — pushed as literal 0 by both callers; NEVER read */
    BYTE   bSlotCount)   
{
    DWORD     dwOsVersion;
    BOOL      bInitOk;
    uint8_t  *pCurEntry;

    /* ARG3 has no use in the body; the slot exists only in the caller's frame. The
     * cast keeps -Wextra's -Wunused-parameter quiet without inventing a read that the
     * machine code does not perform. */
    (void)dwUnusedArg3;

    dwOsVersion = GetVersion();

    g_hAudioInitContext = g_hwndPlayerOrMainUnknown;

    if (((dwOsVersion & 0x80000000u) != 0u) && ((BYTE)dwOsVersion < 4u)) {
        bInitOk = InitializeUtDriverAndHandlerTable(&g_hAudioInitContext);
    } else {
        bInitOk = InitializeAudioSubsystemAndUtHandlerTable(&g_hAudioInitContext);
    }

    if (bInitOk) {

        *pbOutSlot4 = g_apfnUtNotify[4]();

        *pbOutSlot5 = g_apfnUtNotify[5]();

        g_dwSlotCount = (uint32_t)(BYTE)bSlotCount;

        /* Two DWORD globals zeroed (stores are in this order in the binary). */
        g_dwSlotAuxZeroedB = 0u;   
        g_dwSlotAuxZeroedA = 0u;   

        pCurEntry = (uint8_t *)g_adwSlotTable;   
        do {
            *(uint32_t *)pCurEntry = 0u;         
            pCurEntry += 0x10;                   /* ADD EBX,0x10 */
        } while (((intptr_t)g_adwSlotTable
                  + (intptr_t)g_dwSlotCount * 0x10) > (intptr_t)pCurEntry);
    }

    OpenMciCdAudioDeviceAndArmEnabledFlag();

    return TRUE;
}
#endif /* NW_UNIT_InitializeAudioSubsystemByOsVersionAndOpenMciDevice */

/* ==========================================================================
 * Unit: OpenMciCdAudioDeviceAndArmEnabledFlag
 * Source: audio/win32/OpenMciCdAudioDeviceAndArmEnabledFlag.c
 * ========================================================================== */
#ifdef NW_UNIT_OpenMciCdAudioDeviceAndArmEnabledFlag
/*
 * Open Mci Cd Audio Device And Arm Enabled Flag.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-openmcicdaudiodeviceandarmenabledflag-c
 */

#include <stdint.h>
#include "nwglobals.h"
#include <windows.h>   /* pulls in mmsystem.h -> mciapi.h: MCI_OPEN_PARMSA, MCI_OPEN*, MCIDEVICEID */
#include <mmsystem.h>

/* Persistent MCI transport state, owned by this cluster. Set here, read by every sibling. */

DWORD OpenMciCdAudioDeviceAndArmEnabledFlag(void)
{
    /* Cross-platform stub: the audio CD transport is an unavailable device.
     * There is no mciSendCommandA("cdaudio") open, so CD audio is silent on
     * every platform and no error box appears on a machine without cdaudio.
     * The gate flag stays 0, which short-circuits DispatchMciPlaybackCommand
     * and DispatchMciSetFlagsAndPlay before they reach winmm.  The video MCI
     * path in MoviePlayback.c is untouched. */
    g_dwMciTaskId       = 0;
    g_dwMciAudioEnabled = 0;

    return (DWORD)MCIERR_DEVICE_NOT_INSTALLED;   /* non-zero: open reports failure */
}
#endif /* NW_UNIT_OpenMciCdAudioDeviceAndArmEnabledFlag */

/* ==========================================================================
 * Unit: ShowMciErrorMessage
 * Source: audio/win32/ShowMciErrorMessage.c
 * ========================================================================== */
#ifdef NW_UNIT_ShowMciErrorMessage
/*
 * Reports the last MCI error through the diagnostic output.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-showmcierrormessage-c
 */

int __stdcall mciGetErrorStringA(unsigned int mcierr, char *pszText, unsigned int cchText);   
void __stdcall ShowMainWindowErrorBoxWithFixedCaption(const char *text);       

int __stdcall ShowMciErrorMessage(unsigned int uCode)
{
    char msg[256];                 /* SUB ESP,0x100 : 256-byte local buffer; base = [ESP] right after the sub */

    if (uCode == 0)                /* CMP [..+0x104],0 ; JZ -> no-error fast path                             */
        return 1;                  

    mciGetErrorStringA(uCode, msg, 256);           
    ShowMainWindowErrorBoxWithFixedCaption(msg); 
    return 0;                      
}
#endif /* NW_UNIT_ShowMciErrorMessage */
