/*
 * AudioState -- audio slot state and subsystem lifecycle.
 *
 * Module: audio
 * Interface: include/AudioState.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   ClearAllSlotEntries                                        audio/ClearAllSlotEntries.c
 *   DispatchMciAudioReset                                      audio/DispatchMciAudioReset.c
 *   InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook audio/InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook.c
 *   TearDownWaveOutAndMidiAudioSubsystem                       audio/TearDownWaveOutAndMidiAudioSubsystem.c
 */

/* ==========================================================================
 * Unit: ClearAllSlotEntries
 * Source: audio/ClearAllSlotEntries.c
 * ========================================================================== */
#ifdef NW_UNIT_ClearAllSlotEntries
/*
 * Clear All Slot Entries.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-clearallslotentries-c
 */

#include <stdint.h>
#include "nwglobals.h"



/* Slot-hook function-pointer type for the 12-entry notify/hook table at
   window+0x620 (g_apfnSlotHooks in nwglobals.h). */
typedef uint32_t (*SlotHookFn)(void);

uint32_t ClearAllSlotEntries(void)
{
#ifdef NWDEBUG_ENABLED
extern void nwsafe_trace(const char *);
extern void nwtrace_hex(const char *, unsigned long);
extern unsigned char nw_data_window[];
    nwsafe_trace("cls_in");
    nwtrace_hex("cls_count", *(uint32_t *)(nw_data_window + 0x5ea8u));
    nwtrace_hex("cls_tagbyte", (unsigned)((nw_data_window[0x16d0]) & 0xFFu) << 16);
#endif

    uint32_t i = 0;

    do {
        *(uint32_t *)((nw_data_window + 0x1e28u) + i * 16u) = 0u; /* first DWORD of record i, stride 16 */
        ++i;
    } while (i < g_dwSlotCount);

#ifdef NWDEBUG_ENABLED
    nwsafe_trace("cls_pre_hook");
#endif
    uint32_t r = g_apfnSlotHooks[2]();   
#ifdef NWDEBUG_ENABLED
    nwsafe_trace("cls_post_hook");
#endif
    return r;   /* hook result flows through RET to the caller */
}
#endif /* NW_UNIT_ClearAllSlotEntries */

/* ==========================================================================
 * Unit: DispatchMciAudioReset
 * Source: audio/DispatchMciAudioReset.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchMciAudioReset
/*
 * Dispatch Mci Audio Reset.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-dispatchmciaudioreset-c
 */

extern unsigned long __stdcall DispatchMciPlaybackCommand(unsigned short uOpCode);

extern unsigned char bVmAudioStateByte;

void DispatchMciAudioReset(void)
{
#ifdef NWDEBUG_ENABLED
extern void nwsafe_trace(const char *);
    nwsafe_trace("mci_in");
#endif

    (void)DispatchMciPlaybackCommand(0u);

    bVmAudioStateByte = 0xff;
}
#endif /* NW_UNIT_DispatchMciAudioReset */

/* ==========================================================================
 * Unit: InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook
 * Source: audio/InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook
/*
 * Initialize Audio Subsystem Then Dispatch Playback And Pending Slot Hook.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-initializeaudiosubsystemthendispatchplaybackandpendingslothook-c
 */

#include <windows.h>
#include "nwglobals.h"   /* BYTE, BOOL, WINAPI */
#include <stdint.h>    /* uint8_t / uint32_t -- windows.h does not provide these here */

/* --- fixed-argument sources of the audio bring-up call --------------------------- */



/* --- the two guarded dispatch sources (names shared with the callees) ------------- */



extern BOOL WINAPI InitializeAudioSubsystemByOsVersionAndOpenMciDevice(
    BYTE  *pbOutSlot5,      
    BYTE  *pbOutSlot4,      
    DWORD  dwUnusedArg3,    /* ARG3 <- literal 0 (unused in the callee) */
    BYTE   bSlotCount);     

/* cdecl, one BYTE argument; the gate-ordered MCI playback sequencer. */
extern int __stdcall UpdateMciAudioStateAndPlay(unsigned char bMciAudioMode);   

extern int __stdcall DispatchPendingSlotHookForIndex(uint32_t dwSlotIndex);  

void InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook(void)
{

    InitializeAudioSubsystemByOsVersionAndOpenMciDevice(
        &g_bUtSlot5Result,                      
        &g_bUtSlot4Result,                      
        0u,                                     /* ARG3 = 0x0 (unused third arg)   */
        (BYTE)g_dwAudioSlotCountSource);        

    if (g_bVmAudioStateByte != 0xFFu) {
        /* MOVSX ECX,AL sign-extends the byte; the guard above compared the RAW byte, so
         * 0x80..0xfe pass and become negative here. The callee consumes only the low
         * byte, so the sign extension is harmless (see the file header, section (c)). */
        int nSelectedMode = (int)(signed char)g_bVmAudioStateByte;   /* MOVSX ECX,AL */
        UpdateMciAudioStateAndPlay((unsigned char)nSelectedMode);
    }

    if (g_dwPendingHookResult != 0xFFFFFFFFu) {
        DispatchPendingSlotHookForIndex(g_dwPendingHookResult);
    }

}
#endif /* NW_UNIT_InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook */

/* ==========================================================================
 * Unit: TearDownWaveOutAndMidiAudioSubsystem
 * Source: audio/TearDownWaveOutAndMidiAudioSubsystem.c
 * ========================================================================== */
#ifdef NW_UNIT_TearDownWaveOutAndMidiAudioSubsystem
/*
 * Full audio teardown: closes the waveOut device first, then shuts down the MIDI/timer audio subsystem
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-teardownwaveoutandmidiaudiosubsystem-c
 */

#include <stdint.h>   /* unused-in-body header kept for the fixed-width types the callees use */

extern void UnprepareAndCloseWaveOut(void); 
extern void ShutdownAudioSubsystem(void);   

void TearDownWaveOutAndMidiAudioSubsystem(void)
{
    /* Step 1 (waveOut device): unprepare the 8-header ring, then close the device handle. */
    UnprepareAndCloseWaveOut();

    /* Step 2 (MIDI + winmm timer): reset the MIDI event, end the timer period, close MIDI out. */
    ShutdownAudioSubsystem();

    /* Plain RET: no EAX forwarding, no stack cleanup, no arguments of its own. */
}
#endif /* NW_UNIT_TearDownWaveOutAndMidiAudioSubsystem */
