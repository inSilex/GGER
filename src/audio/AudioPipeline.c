/*
 * AudioPipeline -- MIDI decoding, MCI playback requests, and notification plumbing.
 *
 * Module: audio
 * Interface: include/AudioPipeline.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   DecodeMidiTrackVariableLengthQuantity                      audio/DecodeMidiTrackVariableLengthQuantity.c
 *   InitializeAudioSubsystemAndUtHandlerTable                  audio/InitializeAudioSubsystemAndUtHandlerTable.c
 *   InvokeUtNotifySlot8Thunk                                   audio/InvokeUtNotifySlot8Thunk.c
 *   PlayMciWavesCommandIfDeviceArmed                           audio/PlayMciWavesCommandIfDeviceArmed.c
 *   SendMciSetOnWithPointerToGlobalWord                        audio/SendMciSetOnWithPointerToGlobalWord.c
 *   UpdateMciAudioStateAndPlay                                 audio/UpdateMciAudioStateAndPlay.c
 */

/* ==========================================================================
 * Unit: DecodeMidiTrackVariableLengthQuantity
 * Source: audio/DecodeMidiTrackVariableLengthQuantity.c
 * ========================================================================== */
#ifdef NW_UNIT_DecodeMidiTrackVariableLengthQuantity
/*
 * Decode Midi Track Variable Length Quantity.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-decodemiditrackvariablelengthquantity-c
 */

#include <windows.h>
#include <stdint.h>

extern const unsigned char *g_pMidiTrackCursor;

/* No arguments; returns the decoded value zero-extended in EAX (0..0x7FFF). */
unsigned int DecodeMidiTrackVariableLengthQuantity(void)
{
    uint32_t value;                              /* EDX : accumulator, zero-extended      */
    const unsigned char *pCur;                   /* EAX : cursor for the first byte       */

    value = 0;                                   /* XOR EDX,EDX                           */
    pCur  = g_pMidiTrackCursor;                  
    value = (uint32_t)*pCur;                     
    g_pMidiTrackCursor = pCur + 1;               

    if (value & 0x80u) {                         
        const unsigned char *pCur2;              /* ECX : cursor for the second byte      */
        uint32_t second;                         /* EAX : second byte, zero-extended      */

        value &= 0x7fu;                          /* AND EDX,0x7f                          */
        pCur2 = g_pMidiTrackCursor;              
        second = (uint32_t)*pCur2;               
        second <<= 7;                            /* SHL EAX,0x7  (second byte's high bit   */
                                                 /*   is NOT masked, so it leaks into bit */
                                                 /*   14 of the result)                    */
        value |= second;                         /* OR EDX,EAX                            */
        g_pMidiTrackCursor = pCur2 + 1;          
    }

    return value;                                
}
#endif /* NW_UNIT_DecodeMidiTrackVariableLengthQuantity */

/* ==========================================================================
 * Unit: InitializeAudioSubsystemAndUtHandlerTable
 * Source: audio/InitializeAudioSubsystemAndUtHandlerTable.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeAudioSubsystemAndUtHandlerTable
/*
 * Opens waveOut/MIDI and publishes the audio notification handlers.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-initializeaudiosubsystemanduthandlertable-c
 */

#include <windows.h>
#include "nwglobals.h"   /* BOOL, UINT, FARPROC, DWORD_PTR, WINAPI */

/* The waveOut parameter block. Only its first 4 bytes are forwarded by this
 * function; the callee also uses the 0x4000-byte pool view of the same block. */
typedef union tagNETWALK_WAVEOUT_PARAMS {
    HANDLE hEvent;         /* +0x0000 — the waveOut CALLBACK_EVENT handle            */
    BYTE   abPool[0x4000]; /* +0x0000 — audio pool the 8 headers point into           */
} NETWALK_WAVEOUT_PARAMS;

extern BOOL OpenWaveOutAndPrepareHeaderRing(NETWALK_WAVEOUT_PARAMS *pParams);

extern BOOL InitializeMidiOutAndTimerResolution(int nWaveOutOpened);

extern void TearDownWaveOutAndMidiAudioSubsystem(void);

extern FARPROC g_apfnUtNotify[12];

extern void UtNotifyUnprepareCurrentWaveHeader(void); 

extern void UtNotifyWaveOutCommand2(void); 
extern void UtNotifyWaveOutCommand7(void); 
extern void UtNotifyWaveOutCommand8(void); 

extern unsigned char __cdecl FindWaveHeaderInUseSlotIndex(void); 

/* The remaining five code blocks fall outside the dumped byte range; their behaviour is
 * UNEXAMINED and deliberately not described. */
extern void UtNotifyReadWaveOutVolume(void); 
extern uint8_t __stdcall UtNotifySetWaveOutVolume(uint32_t dwLevel); 
extern uint8_t __stdcall UtNotifyStartMidiSequence(uint32_t dwTrackBlock); 
extern uint8_t UtNotifyResetMidi(void); 
extern uint8_t UtNotifyReadMidiVolume(void); 
extern uint8_t __stdcall UtNotifySetMidiVolume(uint32_t dwLevel); 

BOOL WINAPI InitializeAudioSubsystemAndUtHandlerTable(NETWALK_WAVEOUT_PARAMS *pParams)
{
    BOOL bWaveOutOk;
    BOOL bMidiTimerOk;

    bWaveOutOk = OpenWaveOutAndPrepareHeaderRing(pParams);

    bMidiTimerOk = InitializeMidiOutAndTimerResolution((int)bWaveOutOk);

    /* 3. OR, not AND — either half succeeding is enough to publish the table. */
    if ((bWaveOutOk | bMidiTimerOk) == 0) {

        /* Both halves failed. Both callees are internally guarded and zero the
         * handles they took, so this full teardown is safe after a partial setup and
         * is idempotent. */
        TearDownWaveOutAndMidiAudioSubsystem();
        return FALSE;
    }

    /* Publish the notification handlers. */
    g_apfnUtNotify[0]  = (FARPROC)UtNotifyUnprepareCurrentWaveHeader; 
    g_apfnUtNotify[2]  = (FARPROC)UtNotifyWaveOutCommand2;            
    g_apfnUtNotify[4]  = (FARPROC)UtNotifyReadWaveOutVolume;             
    g_apfnUtNotify[9]  = (FARPROC)UtNotifySetWaveOutVolume;             
    g_apfnUtNotify[6]  = (FARPROC)FindWaveHeaderInUseSlotIndex;      
    g_apfnUtNotify[7]  = (FARPROC)UtNotifyWaveOutCommand7;            
    g_apfnUtNotify[8]  = (FARPROC)UtNotifyWaveOutCommand8;            
    g_apfnUtNotify[1]  = (FARPROC)UtNotifyStartMidiSequence;          
    g_apfnUtNotify[3]  = (FARPROC)UtNotifyResetMidi;             
    g_apfnUtNotify[5]  = (FARPROC)UtNotifyReadMidiVolume;             
    g_apfnUtNotify[10] = (FARPROC)UtNotifySetMidiVolume;             

    /* Slot 11 is the whole-subsystem teardown: a UT "shut down" notification must
     * close waveOut before MIDI, which is exactly this function's failure path. */
    g_apfnUtNotify[11] = (FARPROC)TearDownWaveOutAndMidiAudioSubsystem; 

    return TRUE;
}
#endif /* NW_UNIT_InitializeAudioSubsystemAndUtHandlerTable */

/* ==========================================================================
 * Unit: InvokeUtNotifySlot8Thunk
 * Source: audio/InvokeUtNotifySlot8Thunk.c
 * ========================================================================== */
#ifdef NW_UNIT_InvokeUtNotifySlot8Thunk
/*
 * Invoke Ut Notify Slot8Thunk.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-invokeutnotifyslot8thunk-c
 */

#include <stdint.h>

typedef uint32_t (__cdecl *UtNotifyFn)(void);
extern UtNotifyFn g_apfnUtNotify[12];   

uint32_t InvokeUtNotifySlot8Thunk(void)
{

    return g_apfnUtNotify[8]();   
}
#endif /* NW_UNIT_InvokeUtNotifySlot8Thunk */

/* ==========================================================================
 * Unit: PlayMciWavesCommandIfDeviceArmed
 * Source: audio/PlayMciWavesCommandIfDeviceArmed.c
 * ========================================================================== */
#ifdef NW_UNIT_PlayMciWavesCommandIfDeviceArmed
/*
 * PlayMciWavesCommandIfDeviceArmed
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-playmciwavescommandifdevicearmed-c
 */

typedef unsigned long DWORD_PTR;
typedef unsigned short WORD_PTR;

extern DWORD_PTR MciDeviceGlobalArmed;

int __stdcall DispatchMciCommandViaGlobalHandleAndShowError(DWORD_PTR param_1,
                                                  DWORD_PTR param_2,
                                                  DWORD_PTR param_3);

int __stdcall PlayMciWavesCommandIfDeviceArmed(DWORD_PTR param_1, DWORD_PTR param_2)
{
    DWORD_PTR saved_eax;          /* occupies the slot the callee gets a pointer to */

    saved_eax = 0;

    if (MciDeviceGlobalArmed == 0) {
        return 0;
    }

    (void)saved_eax;

    DispatchMciCommandViaGlobalHandleAndShowError((DWORD_PTR)&saved_eax,
                                                  param_1, param_2);

    return 1;
}
#endif /* NW_UNIT_PlayMciWavesCommandIfDeviceArmed */

/* ==========================================================================
 * Unit: SendMciSetOnWithPointerToGlobalWord
 * Source: audio/SendMciSetOnWithPointerToGlobalWord.c
 * ========================================================================== */
#ifdef NW_UNIT_SendMciSetOnWithPointerToGlobalWord
/*
 * Send Mci Set On With Pointer To Global Word.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-sendmcisetonwithpointertoglobalword-c
 */

#include <stdint.h>
#include <windows.h>
#include "nwglobals.h"

//

// 0f b7 05 04 74 41 00).  The same slot is a full 32-bit HWND everywhere else:

// assumption (USER handles fit in 16 bits on the systems the game shipped for),
// not a width mismatch on our side.  Reproduce it exactly.
//
// ISSUE section 67 once replaced this with the full handle on the theory that a
// 16-bit callback loses MM_MCINOTIFY (0x3B9) on Win32 and the startup-movie
// pump spins forever.  That was an unverified behavioural deviation and it has
// been reverted: with no tile-video media present (no .AVI/.BMP beside the
// program or on any drive) PlayVideoTileMovieAndLoadBmp returns before
// MCI_OPEN, so the pump never enters its loop -- and when the media IS present
// the reference binary performs this same truncation.  A hang there would be

// standalone declaration kept out of the way; the value comes from nwglobals.h
// (g_hwndMainWindow @ window+0x6404).

extern int __stdcall DispatchMciCommandViaGlobalHandleAndShowError(uint32_t param_1,
                                                        uint32_t param_2,
                                                        uint16_t param_3); 

int SendMciSetOnWithPointerToGlobalWord(void) {
    // 12-byte frame; only the leading dword is ever written. The remaining 8 bytes are
    // left uninitialized here and are never read by anything this function calls.
    uint32_t local_dwValue = (uint32_t)(uint16_t)(uintptr_t)g_hwndMainWindow;

    

    
    // EAX (the dispatcher's 1/0 result) passes through untouched -- the caller

    return DispatchMciCommandViaGlobalHandleAndShowError((uint32_t)(uintptr_t)&local_dwValue,
                                                          0x1u, 0x806u);
}
#endif /* NW_UNIT_SendMciSetOnWithPointerToGlobalWord */

/* ==========================================================================
 * Unit: UpdateMciAudioStateAndPlay
 * Source: audio/UpdateMciAudioStateAndPlay.c
 * ========================================================================== */
#ifdef NW_UNIT_UpdateMciAudioStateAndPlay
/*
 * Update Mci Audio State And Play.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-updatemciaudiostateandplay-c
 */

#include <stdint.h>
#include "nwglobals.h"

extern unsigned char g_bMciAudioBusy;    

extern unsigned char g_bMciPlaybackEnabled; 


extern void DispatchMciAudioReset(void);   

extern int DispatchMciSetFlagsAndPlay(unsigned char bFlag); 

int __stdcall UpdateMciAudioStateAndPlay(unsigned char bMciAudioMode)
{
    /* Gate A: busy audio subsystem -> the transport is left to its owner, just report 1. */
    if (g_bMciAudioBusy != 0) {
        return 1;
    }

    DispatchMciAudioReset();

    /* Gate B: playback not enabled -> stop here, the reset above still stands. */
    if (g_bMciPlaybackEnabled == 0) {
        return 1;
    }

    g_bVmAudioStateByte = bMciAudioMode;

    return (int)(signed char)DispatchMciSetFlagsAndPlay(bMciAudioMode);
}
#endif /* NW_UNIT_UpdateMciAudioStateAndPlay */
