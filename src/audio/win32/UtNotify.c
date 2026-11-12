#include "nwarch.h"

/*
 * UtNotify -- UT driver callbacks and MIDI/wave volume notification.
 *
 * Module: audio/win32
 * Interface: include/UtNotify.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   DispatchMidiSequencerEventsWithRunningStatus               audio/win32/DispatchMidiSequencerEventsWithRunningStatus.c
 *   InitializeUtDriverAndHandlerTable                          audio/win32/InitializeUtDriverAndHandlerTable.c
 *   ShutdownUtDriverAndFreeResources                           audio/win32/ShutdownUtDriverAndFreeResources.c
 *   UtNotifyAudioHandlers                                      audio/win32/UtNotifyAudioHandlers.c
 *   UtNotifyReadMidiVolume                                     audio/win32/UtNotifyReadMidiVolume.c
 *   UtNotifyReadWaveOutVolume                                  audio/win32/UtNotifyReadWaveOutVolume.c
 */

/* ==========================================================================
 * Unit: DispatchMidiSequencerEventsWithRunningStatus
 * Source: audio/win32/DispatchMidiSequencerEventsWithRunningStatus.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchMidiSequencerEventsWithRunningStatus
/*
 * Dispatch Midi Sequencer Events With Running Status.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-dispatchmidisequencereventswithrunningstatus-c
 */

#include <windows.h>   /* HMIDIOUT, DWORD, BOOL */
#include <mmsystem.h>  /* timeGetTime prototype */
#include <stdint.h>
#include "NwSdlMidi.h" /* NwMidiSink_ShortMsg -- cross-platform MIDI output */

/* ---- shared MIDI/audio-subsystem globals (data, not callees). All zero in the static image. ---- */
extern const unsigned char *g_pMidiTrackCursor;  
extern unsigned char        g_bMidiRunningStatus;
extern unsigned char        g_bMidiTimerActive;   
extern HMIDIOUT             g_hMidiOut;           
extern DWORD                g_dwMidiClockTarget;  
extern DWORD                g_dwMidiLoopGuard;    
extern DWORD                g_dwMidiEndMarker;    
extern DWORD                g_dwMidiSavedCount;   
extern const unsigned char *g_pMidiSavedCursor;   

extern unsigned int DecodeMidiTrackVariableLengthQuantity(void); 
extern void         ResetMidiOutAndTimerEvent(void);             

void DispatchMidiSequencerEventsWithRunningStatus(void)
{
    unsigned char bLoopAgain;    /* [ESP+0x4] flag A : repeat without re-pacing               */
    unsigned char bSendMessage;  /* [ESP+0x5] flag B : emit this short message                */
    unsigned char bReadData2;    /* does this branch read a 2nd data byte?                    */
    unsigned char bEventByte;    /* the raw byte pulled from the cursor                       */
    unsigned char status;        /* the resolved status byte (fresh or running)              */
    unsigned char data1;         /* first operand byte                                        */
    unsigned char data2;         /* second operand byte                                       */
    DWORD         dwMsg;         /* packed midiOutShortMsg argument                           */
    DWORD         dwDelta;       /* decoded delta time                                        */
    DWORD         dwHi;          /* status high nibble                                        */
    DWORD         dwMarker;      

    for (;;) {
        bLoopAgain = 0u;                              

        bEventByte = *g_pMidiTrackCursor;             
        if (bEventByte > 0x7fu) {                     /* status byte                          */
            g_pMidiTrackCursor++;                     
            status = bEventByte;                      
        } else {                                       /* data byte -> reuse running status    */
            status = g_bMidiRunningStatus;            
        }
        g_bMidiRunningStatus = status;                

        /* ---- status dispatch ---- */
        if (status == 0xf0u || status == 0xf7u) {
            /* system exclusive: decode LENGTH with the VLV helper and SKIP that many bytes */
            g_pMidiTrackCursor += DecodeMidiTrackVariableLengthQuantity();  

        } else if (status == 0xffu) {
            dwMarker = g_dwMidiEndMarker;              
            if (dwMarker == 0xffffu) {                 
                ResetMidiOutAndTimerEvent();           
                return;                                
            }
            g_dwMidiSavedCount = dwMarker;             
            g_pMidiTrackCursor = g_pMidiSavedCursor;   
            return;                                    
        } else {
            /* ---- channel short message (0x80..0xEF) ---- */
            bSendMessage = 1u;                         
            data1  = *g_pMidiTrackCursor;              
            dwMsg  = ((DWORD)data1 << 8) | (DWORD)status; 
            g_pMidiTrackCursor++;                      

            dwHi = (DWORD)status & 0xf0u;              
            if (dwHi == 0x80u || dwHi == 0x90u) {      /* 0x8n note off / 0x9n note on        */

                bSendMessage = (g_bMidiTimerActive >= 1u) ? 1u : 0u;   
                bReadData2 = 1u;
            } else if (dwHi == 0xc0u || dwHi == 0xd0u) {
                /* 0xCn program change / 0xDn channel pressure: only ONE data byte */
                bReadData2 = 0u;                       
            } else {
                bReadData2 = 1u;                       /* 0xAn/0xBn/0xEn -> 3-byte default    */
            }

            if (bReadData2) {                          
                data2 = *g_pMidiTrackCursor;           
                g_pMidiTrackCursor++;                  
                dwMsg |= ((DWORD)(data2 & 0x7fu) << 16);          
                bLoopAgain = (unsigned char)(data2 & 0x80u);       
            }

            if (bSendMessage != 0u) {                  
                NwMidiSink_ShortMsg(dwMsg);            /* static synth, no midiOut* */
            }
        }

        /* ---- flag-A test: if set, grab the next event with no delta/pacing ---- */
        if (bLoopAgain != 0u) {                        
            continue;                                  
        }

        /* ---- delta time + 10 ms-per-tick pacing ---- */
        dwDelta = DecodeMidiTrackVariableLengthQuantity();   
        if (dwDelta == 0u) {                           
            continue;                                  
        }
        g_dwMidiClockTarget += dwDelta * 10u;          

        if (timeGetTime() >= g_dwMidiClockTarget) {    
            continue;                                  
        }
        return;                                        
    }
}
#endif /* NW_UNIT_DispatchMidiSequencerEventsWithRunningStatus */

/* ==========================================================================
 * Unit: InitializeUtDriverAndHandlerTable
 * Source: audio/win32/InitializeUtDriverAndHandlerTable.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeUtDriverAndHandlerTable
/*
 * Initialize Ut Driver And Handler Table.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-initializeutdriverandhandlertable-c
 */

#include <windows.h>
#include "nwglobals.h"

/* UT dispatch-hook notification codes (the immediate each thunk pushes). */
#define UT_NOTIFY_REGISTER   0x200u  /* sent inline from this function        */
#define UT_NOTIFY_SHUTDOWN   0x201u  /* sent by ShutdownUtDriverAndFreeRes... */
#define UT_CLIENT_BUF_SIZE   0xF000u /* GlobalAlloc size for the client buffer */

extern HMODULE g_hUtKernel;    
extern FARPROC g_pfnUtRegister;
extern FARPROC g_pfnUtUnReg;   
extern FARPROC g_pfnUtHook;    
extern HGLOBAL g_pcbUtBuffer;  


extern FARPROC g_apfnUtNotify[12];

extern void WINAPI UtNotifyCode0x202(void); 
extern void WINAPI UtNotifySendArgCode0x208(void);      
extern void WINAPI UtNotifyCopyArgToBuffer0x208(void);  
extern void WINAPI UtNotifySendArgPlus100(void);        
extern void WINAPI UtNotifyCode0x203(void); 
extern void WINAPI UtNotifyCode0x204(void); 
extern void WINAPI UtNotifyCode0x205(void); 
extern void WINAPI UtNotifyCode0x206(void); 
extern void WINAPI UtNotifyCode0x207(void); 
extern void WINAPI UtNotifyCode0x209(void); 
extern void WINAPI UtNotifyCode0x20a(void); 
extern void ShutdownUtDriverAndFreeResources(void);     

typedef int(WINAPI *PFN_UTREGISTER)(void *pRegistration, const char *pszDll,
                                    unsigned int, unsigned int,
                                    FARPROC *ppfnHook, unsigned int,
                                    unsigned int);
typedef int(WINAPI *PFN_UTHOK)(void *pContext, UINT uCode, void *pvData);

BOOL WINAPI InitializeUtDriverAndHandlerTable(void *pUtContext)
{
    int iResult;

    g_hUtKernel = LoadLibraryA("KERNEL32.DLL");
    if (g_hUtKernel == NULL) {
        return FALSE;
    }

    g_pfnUtRegister = GetProcAddress(g_hUtKernel, "UTRegister");
    if (g_pfnUtRegister == NULL) {
        return FALSE;
    }

    g_pfnUtUnReg = GetProcAddress(g_hUtKernel, "UTUnRegister");
    if (g_pfnUtUnReg == NULL) {
        return FALSE;
    }

    /* Register GMW.DLL with the UT layer; out-param 5 is the dispatch hook
     * through which every UT notification for this client is delivered. */
    iResult = ((PFN_UTREGISTER)(uintptr_t)g_pfnUtRegister)(g_pUtRegistration, "GMW.DLL",
                                                0, 2, &g_pfnUtHook, 0, 0);
    if (iResult == 0) {
        return FALSE;
    }

    iResult = ((PFN_UTHOK)(uintptr_t)g_pfnUtHook)(pUtContext, UT_NOTIFY_REGISTER, NULL);
    if (iResult == 0) {
        goto unwind;
    }

    g_pcbUtBuffer = GlobalAlloc(GMEM_FIXED, UT_CLIENT_BUF_SIZE);
    if (g_pcbUtBuffer == NULL) {
        goto unwind;
    }

    /* Publish the notification handlers. Store order below is the exact store
     * order in the binary, which is not ascending slot order. */
    g_apfnUtNotify[0]  = (FARPROC)UtNotifyCode0x202;
    g_apfnUtNotify[2]  = (FARPROC)UtNotifyCode0x203;
    g_apfnUtNotify[4]  = (FARPROC)UtNotifyCode0x204;
    g_apfnUtNotify[9]  = (FARPROC)UtNotifySendArgCode0x208;
    g_apfnUtNotify[6]  = (FARPROC)UtNotifyCode0x205;
    g_apfnUtNotify[7]  = (FARPROC)UtNotifyCode0x206;
    g_apfnUtNotify[8]  = (FARPROC)UtNotifyCode0x207;
    g_apfnUtNotify[1]  = (FARPROC)UtNotifyCopyArgToBuffer0x208;
    g_apfnUtNotify[3]  = (FARPROC)UtNotifyCode0x209;
    g_apfnUtNotify[5]  = (FARPROC)UtNotifyCode0x20a;
    g_apfnUtNotify[10] = (FARPROC)UtNotifySendArgPlus100;
    g_apfnUtNotify[11] = (FARPROC)ShutdownUtDriverAndFreeResources;

    return TRUE;

unwind:

    ShutdownUtDriverAndFreeResources();
    return FALSE;
}
#endif /* NW_UNIT_InitializeUtDriverAndHandlerTable */

/* ==========================================================================
 * Unit: ShutdownUtDriverAndFreeResources
 * Source: audio/win32/ShutdownUtDriverAndFreeResources.c
 * ========================================================================== */
#ifdef NW_UNIT_ShutdownUtDriverAndFreeResources
/*
 * Shutdown Ut Driver And Free Resources.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-shutdownutdriverandfreeresources-c
 */

#include <windows.h>

extern FARPROC g_pfnUtHook; /* UT dispatch hook; invoked as (lpszDll, uCode, 0)   */
extern FARPROC g_pfnUtUnReg; /* UTUnRegister;         invoked as (registration)    */
extern HMODULE g_hUtKernel; /* module handle to release                             */
extern HGLOBAL g_pcbUtBuffer; /* 0xF000 scratch buffer, freed and cleared here        */

/* Registration object handed to UTUnRegister; written by unrelated startup code. */
extern void *g_pUtRegistration;

void ShutdownUtDriverAndFreeResources(void)
{
    if (g_pfnUtHook != NULL) {
        /* 0x200 is the "register" notification used by init, 0x201 the matching
         * teardown notification, so this tells the UT layer the DLL is going away. */
        ((void (WINAPI *)(const char *, UINT, void *))(uintptr_t)g_pfnUtHook)(NULL, 0x201, NULL);
    }

    if (g_pfnUtUnReg != NULL) {
        ((void (WINAPI *)(void *))(uintptr_t)g_pfnUtUnReg)(g_pUtRegistration);
    }

    if (g_hUtKernel != NULL) {
        FreeLibrary(g_hUtKernel);
    }

    if (g_pcbUtBuffer != NULL) {
        GlobalFree(g_pcbUtBuffer);
        g_pcbUtBuffer = NULL;
    }
}
#endif /* NW_UNIT_ShutdownUtDriverAndFreeResources */

/* ==========================================================================
 * Unit: UtNotifyAudioHandlers
 * Source: audio/win32/UtNotifyAudioHandlers.c
 * ========================================================================== */
#ifdef NW_UNIT_UtNotifyAudioHandlers
/*
 * Ut Notify Audio Handlers.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-utnotifyaudiohandlers-c
 */

#include <stdint.h>
#include "NwSdlMidi.h"
#include "NwSdlPcm.h"

extern unsigned char nw_data_window[];

/* Window slots this family touches (raw offsets so this file depends on nothing
 * but the window base contract nw_data_window + 0xNNNN). */
#define G_HWAVEOUT            (*(uint32_t *)(void *)(nw_data_window + 0x1cd8)) 
#define G_U_WAVEOUT_DEVICE_ID (*(uint32_t *)(void *)(nw_data_window + 0x1cdc)) 
#define G_AWAVEHEADERS_BASE   (nw_data_window + 0x1ce0)                        
#define G_HMIDIOUT            (*(uint32_t *)(void *)(nw_data_window + 0x1de0)) 
#define G_U_MIDI_DEV_ID       (*(uint32_t *)(void *)(nw_data_window + 0x1de4)) 
#define G_B_MIDI_TIMER_ACTIVE (*(uint8_t  *)(void *)(nw_data_window + 0x1e05)) 

extern void ResetMidiOutAndTimerEvent(void);              

uint8_t UtNotifyUnprepareCurrentWaveHeader(void)
{
    /* Submit the mixed pool buffer to the SDL sound-effect stream. */
    return (uint8_t)NwPcmSink_Submit();
}

uint8_t UtNotifyWaveOutCommand2(void)
{
    return (uint8_t)NwPcmSink_Reset();
}

uint8_t UtNotifyWaveOutCommand7(void)
{
    return (uint8_t)NwPcmSink_Pause();
}

uint8_t UtNotifyWaveOutCommand8(void)
{
    return (uint8_t)NwPcmSink_Resume();
}

uint8_t __stdcall UtNotifySetWaveOutVolume(uint32_t dwLevel)
{
    uint8_t  bLevel = (uint8_t)(dwLevel & 0xFFu);
    uint32_t dwVolume = ((((uint32_t)bLevel) << 8) | 0xFFu) * 0x00010001u;
    uint8_t  bOk = (uint8_t)NwPcmSink_SetVolume(dwVolume);

    if (bLevel == 0u)
        (void)NwPcmSink_Reset();                 /* CMPB [ESP+0xC],0 / JNE */

    return bOk;
}

uint8_t UtNotifyResetMidi(void)
{
    ResetMidiOutAndTimerEvent();
    return 1u;
}

uint8_t __stdcall UtNotifySetMidiVolume(uint32_t dwLevel)
{
    uint8_t  bLevel = (uint8_t)(dwLevel & 0xFFu);
    uint32_t dwVolume = ((((uint32_t)bLevel) << 8) | 0xFFu) * 0x00010001u;
    uint8_t  bOk;

    G_B_MIDI_TIMER_ACTIVE = bLevel;              
    bOk = (uint8_t)NwMidiSink_SetVolume(dwVolume);   /* static synth stream gain */

    if (bLevel == 0u)
        NwMidiSink_Reset();                      /* CMPB [ESP+0xC],0 / JNE */

    return bOk;
}

#define G_DW_TIMER_EVENT_ID   (*(uint32_t *)(void *)(nw_data_window + 0x1de8)) 
#define G_DW_MIDI_CLOCK       (*(uint32_t *)(void *)(nw_data_window + 0x1dec)) 
#define G_DW_MIDI_IN_CALLBACK (*(uint32_t *)(void *)(nw_data_window + 0x1df0)) 
#define G_DW_MIDI_END_MARKER  (*(uint32_t *)(void *)(nw_data_window + 0x1df4)) 
#define G_DW_MIDI_STOP_COUNT  (*(uint32_t *)(void *)(nw_data_window + 0x1df8)) 
#define G_DW_MIDI_LOOP_START  NW_WINDOW_PTR_VALUE(0x1dfc)
#define G_DW_MIDI_CURSOR      NW_WINDOW_PTR_VALUE(0x1e00)
#define G_DW_TIMER_USER_DATA  (*(uint32_t *)(void *)(nw_data_window + 0x1e08)) 

typedef void (__stdcall *NwTimeCallback)(uint32_t uTimerID, uint32_t uMsg, uint32_t dwUser,
                                         uint32_t dwParam1, uint32_t dwParam2);

extern uint32_t __stdcall timeGetTime(void);
extern uint32_t __stdcall timeSetEvent(uint32_t uDelay, uint32_t uResolution,
                                       NwTimeCallback lpTimeProc, uint32_t dwUser,
                                       uint32_t fuEvent);
extern unsigned int DecodeMidiTrackVariableLengthQuantity(void);   
extern void DispatchMidiSequencerEventsWithRunningStatus(void);    
void __stdcall MidiSequencerTimerProc(uint32_t uTimerID, uint32_t uMsg, uint32_t dwUser,
                                      uint32_t dwParam1, uint32_t dwParam2);

uint8_t __stdcall UtNotifyStartMidiSequence(uint32_t dwTrackBlock)
{
    uint32_t dwCursor = dwTrackBlock + 4u;

    ResetMidiOutAndTimerEvent();
    G_DW_MIDI_CURSOR = (void *)(uintptr_t)nw_guest_to_host(dwCursor);

    if (*(const uint8_t *)(uintptr_t)nw_guest_to_host(dwCursor) == 0xF8u) {
        G_DW_MIDI_CURSOR = (void *)(uintptr_t)nw_guest_to_host(dwCursor + 1u);
        (void)DecodeMidiTrackVariableLengthQuantity();  /* advances G_DW_MIDI_CURSOR itself */
        G_DW_MIDI_CURSOR = (void *)((uintptr_t)G_DW_MIDI_CURSOR + 1u);
    }

    G_DW_MIDI_LOOP_START  = G_DW_MIDI_CURSOR;
    G_DW_MIDI_END_MARKER  = 1u;
    G_DW_MIDI_STOP_COUNT  = 1u;
    G_DW_MIDI_IN_CALLBACK = 0u;

    G_DW_TIMER_EVENT_ID = timeSetEvent(10u, 0u, MidiSequencerTimerProc,
                                       G_DW_TIMER_USER_DATA, 1u /* TIME_PERIODIC */);
    if (G_DW_TIMER_EVENT_ID == 0u) {
        ResetMidiOutAndTimerEvent();
        return 0u;
    }
    return 1u;
}

void __stdcall MidiSequencerTimerProc(uint32_t uTimerID, uint32_t uMsg, uint32_t dwUser,
                                      uint32_t dwParam1, uint32_t dwParam2)
{
    (void)uTimerID; (void)uMsg; (void)dwUser; (void)dwParam1; (void)dwParam2;

    if (G_DW_MIDI_STOP_COUNT != 0u) {
        G_DW_MIDI_STOP_COUNT--;
        if (G_DW_MIDI_STOP_COUNT != 0u)
            return;
        G_DW_MIDI_CLOCK = timeGetTime();
    }

    if (G_DW_MIDI_IN_CALLBACK != 0u)
        return;
    if ((uint32_t)timeGetTime() < G_DW_MIDI_CLOCK)
        return;

    G_DW_MIDI_IN_CALLBACK++;
    DispatchMidiSequencerEventsWithRunningStatus();
    G_DW_MIDI_IN_CALLBACK--;
}
#endif /* NW_UNIT_UtNotifyAudioHandlers */

/* ==========================================================================
 * Unit: UtNotifyReadMidiVolume
 * Source: audio/win32/UtNotifyReadMidiVolume.c
 * ========================================================================== */
#ifdef NW_UNIT_UtNotifyReadMidiVolume
/*
 * Reads the MIDI volume and returns the louder channel level.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-utnotifyreadmidivolume-c
 */

#include <stdint.h>
#include "NwSdlMidi.h"

extern unsigned char nw_data_window[];

#define G_U_MIDI_DEV_ID (*(uint32_t *)(void *)(nw_data_window + 0x1de4))

uint8_t UtNotifyReadMidiVolume(void)
{
    uint32_t dwPackedVolume = 0;                 
    uint16_t wLow, wHigh, wSelected;

    /* The sink keeps the packed level the MIDI volume handler last set. */
    dwPackedVolume = NwMidiSink_GetVolume();

    wLow  = (uint16_t)(dwPackedVolume & 0xFFFFu);
    wHigh = (uint16_t)(dwPackedVolume >> 16);
    /* CMP [ESP],AX computes LOW - HIGH; JBE (LOW <= HIGH) selects the HIGH arm,
     * otherwise the LOW arm: the LARGER of the two packed 16-bit levels. */
    wSelected = (wLow > wHigh) ? wLow : wHigh;

    return (uint8_t)((int32_t)wSelected >> 8);    /* SAR EAX,8 ; result in AL   */
}
#endif /* NW_UNIT_UtNotifyReadMidiVolume */

/* ==========================================================================
 * Unit: UtNotifyReadWaveOutVolume
 * Source: audio/win32/UtNotifyReadWaveOutVolume.c
 * ========================================================================== */
#ifdef NW_UNIT_UtNotifyReadWaveOutVolume
/*
 * Reads the waveOut volume and returns the louder channel level.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-utnotifyreadwaveoutvolume-c
 */

#include <stdint.h>
#include "NwSdlPcm.h"

extern unsigned char nw_data_window[];

#define G_U_WAVEOUT_DEVICE_ID (*(uint32_t *)(void *)(nw_data_window + 0x1cdc))

uint8_t UtNotifyReadWaveOutVolume(void)
{
    uint32_t dwPackedVolume;                     
    uint16_t wLow, wHigh, wSelected;

    /* The sink keeps the packed level the SFX volume handler last set. */
    dwPackedVolume = NwPcmSink_GetVolume();

    wLow  = (uint16_t)(dwPackedVolume & 0xFFFFu);
    wHigh = (uint16_t)(dwPackedVolume >> 16);
    /* CMP [ESP],AX computes LOW - HIGH; JBE (LOW <= HIGH) selects the HIGH arm,
     * otherwise the LOW arm: the LARGER of the two packed 16-bit levels. */
    wSelected = (wLow > wHigh) ? wLow : wHigh;

    return (uint8_t)((int32_t)wSelected >> 8);    /* SAR EAX,8 ; result in AL   */
}
#endif /* NW_UNIT_UtNotifyReadWaveOutVolume */
