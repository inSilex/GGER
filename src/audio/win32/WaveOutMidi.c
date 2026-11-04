/*
 * WaveOutMidi -- waveOut buffers, MIDI output, and audio shutdown.
 *
 * Module: audio/win32
 * Interface: include/WaveOutMidi.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   FindWaveHeaderInUseSlotIndex                               audio/win32/FindWaveHeaderInUseSlotIndex.c
 *   InitializeMidiOutAndTimerResolution                        audio/win32/InitializeMidiOutAndTimerResolution.c
 *   OpenWaveOutAndPrepareHeaderRing                            audio/win32/OpenWaveOutAndPrepareHeaderRing.c
 *   ResetMidiOutAndTimerEvent                                  audio/win32/ResetMidiOutAndTimerEvent.c
 *   ShutdownAudioSubsystem                                     audio/win32/ShutdownAudioSubsystem.c
 *   UnprepareAndCloseWaveOut                                   audio/win32/UnprepareAndCloseWaveOut.c
 */

/* ==========================================================================
 * Unit: FindWaveHeaderInUseSlotIndex
 * Source: audio/win32/FindWaveHeaderInUseSlotIndex.c
 * ========================================================================== */
#ifdef NW_UNIT_FindWaveHeaderInUseSlotIndex
/*
 * Find Wave Header In Use Slot Index.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-findwaveheaderinuseslotindex-c
 *
 * SDL port: SDL_PutAudioStreamData copies the mixed buffer synchronously, so
 * there is no reader-held buffer to rotate.  The mixer always blends into pool
 * buffer 0, and the notify handler submits that same buffer.
 */

unsigned char FindWaveHeaderInUseSlotIndex(void)
{
    return 0u;
}

__asm__(".globl _SelectWaveHeaderIndex\n"
        ".set   _SelectWaveHeaderIndex, _FindWaveHeaderInUseSlotIndex\n");
#endif /* NW_UNIT_FindWaveHeaderInUseSlotIndex */

/* ==========================================================================
 * Unit: InitializeMidiOutAndTimerResolution
 * Source: audio/win32/InitializeMidiOutAndTimerResolution.c
 * ========================================================================== */
#ifdef NW_UNIT_InitializeMidiOutAndTimerResolution
/*
 * Opens MIDI output and raises the timer resolution.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-initializemidioutandtimerresolution-c
 */

#include <windows.h>
#include <mmsystem.h>
#include "NwSdlMidi.h"

#define TIMER_PERIOD_FLOOR_MS 10u

extern UINT      g_uTimerPeriodMs; 
extern BYTE      g_bMidiTimerActive; 

extern void ShutdownAudioSubsystem(void);

BOOL InitializeMidiOutAndTimerResolution(void)
{
    TIMECAPS caps;
    UINT     uPeriodMs;
    MMRESULT mmr;

    /* Cross-platform MIDI: the statically linked TinySoundFont synth behind an
     * SDL3 stream.  There is no midiOutOpen here and no WinMM MIDI fallback;
     * when SDL3 or the bank is unavailable, MIDI is simply not armed. */
    if (!NwMidiSink_Init()) {
        return FALSE;
    }

    /* No timeBeginPeriod is in effect yet, so the shared period word starts
     * cleared; the active flag is raised now that the output is good. */
    g_uTimerPeriodMs = 0;
    g_bMidiTimerActive = 1;

    /* uSize = 8 is sizeof(TIMECAPS) for the Win9x-era _WIN32_WINNT layout
     * (wPeriodMin + wPeriodMax + dwCapabilities). */
    mmr = timeGetDevCaps(&caps, 8);
    if (mmr != MMSYSERR_NOERROR) {
        goto unwind;
    }

    /* Ask for the COARSEST resolution the timer driver allows: at least
     * TIMER_PERIOD_FLOOR_MS so playback keeps working on drivers that advertise
     * a uselessly fine minimum, but never coarser than wPeriodMax. */
    uPeriodMs = (caps.wPeriodMin > TIMER_PERIOD_FLOOR_MS) ? caps.wPeriodMin
                                                          : TIMER_PERIOD_FLOOR_MS;
    if (uPeriodMs >= caps.wPeriodMax) {
        uPeriodMs = caps.wPeriodMax;
    }
    g_uTimerPeriodMs = uPeriodMs;

    mmr = timeBeginPeriod(g_uTimerPeriodMs);
    if (mmr != MMSYSERR_NOERROR) {
        goto unwind;
    }

    return TRUE;

unwind:
    /* One tail for every failure after the handle was taken: closes the MIDI
     * device and cancels the period, both zero-guarded. */
    ShutdownAudioSubsystem();
    return FALSE;
}
#endif /* NW_UNIT_InitializeMidiOutAndTimerResolution */

/* ==========================================================================
 * Unit: OpenWaveOutAndPrepareHeaderRing
 * Source: audio/win32/OpenWaveOutAndPrepareHeaderRing.c
 * ========================================================================== */
#ifdef NW_UNIT_OpenWaveOutAndPrepareHeaderRing
/*
 * Open Wave Out And Prepare Header Ring.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-openwaveoutandprepareheaderring-c
 *
 * SDL port: this is now the sound-effect stream bring-up.  The 0x4000-byte pool
 * still lives at nw_data_window+0x1ea8 (the caller-supplied parameter block) and
 * the mixer still blends into it; the waveOut ring is replaced by one U8 mono
 * 11025 Hz SDL stream, and there is no waveOut call left here.
 */

#include <stdint.h>
#include "nwglobals.h"
#include <windows.h>   /* BOOL / WINAPI for the fixed signature */

#include "NwSdlPcm.h"

/* Caller-supplied parameter block: the same 0x4000-byte pool the mixer uses.
 * The first four bytes were the waveOut callback handle; the SDL sink does not
 * read it, and the first mix overwrites it, exactly as before. */
typedef union tagNETWALK_WAVEOUT_PARAMS {
    HANDLE hEvent;         /* +0x0000 — unused now            */
    BYTE   abPool[0x4000]; /* +0x0000 — audio pool            */
} NETWALK_WAVEOUT_PARAMS;

BOOL OpenWaveOutAndPrepareHeaderRing(NETWALK_WAVEOUT_PARAMS *pParams)
{
    (void)pParams;
    return NwPcmSink_Init() ? TRUE : FALSE;
}
#endif /* NW_UNIT_OpenWaveOutAndPrepareHeaderRing */

/* ==========================================================================
 * Unit: ResetMidiOutAndTimerEvent
 * Source: audio/win32/ResetMidiOutAndTimerEvent.c
 * ========================================================================== */
#ifdef NW_UNIT_ResetMidiOutAndTimerEvent
/*
 * Reset Midi Out And Timer Event.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-resetmidioutandtimerevent-c
 */

#include <stdint.h>
/* Win32 multimedia (winmm.dll): timeKillEvent / MMRESULT come from windows.h ->
   mmsystem.h.  MIDI output itself is the SDL3 + TinySoundFont sink. */
#include <windows.h>
#include "NwSdlMidi.h"

/* Persistent audio-subsystem state owned by the VM-support cluster (data, not callees). */
extern MMRESULT dwTimerEventId;      

void ResetMidiOutAndTimerEvent(void)
{
    /* Kill the outstanding multimedia timer event, but only if one is armed. */
    if (dwTimerEventId != 0) {
        (void)timeKillEvent(dwTimerEventId);
        dwTimerEventId = 0;
    }

    /* Silence the synth unconditionally (no midiOutReset device call). */
    NwMidiSink_Reset();
}
#endif /* NW_UNIT_ResetMidiOutAndTimerEvent */

/* ==========================================================================
 * Unit: ShutdownAudioSubsystem
 * Source: audio/win32/ShutdownAudioSubsystem.c
 * ========================================================================== */
#ifdef NW_UNIT_ShutdownAudioSubsystem
/*
 * Top-level audio teardown: resets the MIDI/timer-event state, ends the winmm timer period, then closes and clears the MIDI output handle
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-shutdownaudiosubsystem-c
 */

#include <stdint.h>
/* Win32 multimedia (winmm.dll): timeEndPeriod / UINT come from windows.h ->
   mmsystem.h.  The MIDI output is the SDL3 + TinySoundFont sink. */
#include <windows.h>
#include "NwSdlMidi.h"

extern UINT     uTimerPeriodMs;   

extern void ResetMidiOutAndTimerEvent(void);

void ShutdownAudioSubsystem(void)
{
    /* Step 1 (unconditional, first): kill the multimedia timer event and silence
       the synth so no notes are still sounding when the stream is torn down. */
    ResetMidiOutAndTimerEvent();

    /* Step 2: release the fine-grained winmm timer resolution, but only if one was armed. */
    if (uTimerPeriodMs != 0) {
        (void)timeEndPeriod(uTimerPeriodMs);
        uTimerPeriodMs = 0;
    }

    /* Step 3 (last): stop the SDL3 stream and free the synth. */
    NwMidiSink_Shutdown();
}
#endif /* NW_UNIT_ShutdownAudioSubsystem */

/* ==========================================================================
 * Unit: UnprepareAndCloseWaveOut
 * Source: audio/win32/UnprepareAndCloseWaveOut.c
 * ========================================================================== */
#ifdef NW_UNIT_UnprepareAndCloseWaveOut
/*
 * Unprepare And Close Wave Out.
 *
 * Module: audio/win32
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-win32-unprepareandclosewaveout-c
 *
 * SDL port: tear down the sound-effect stream.  Safe after a partial init and
 * idempotent, matching the old zero-guarded waveOut teardown.
 */

#include <stdint.h>
#include "NwSdlPcm.h"

void UnprepareAndCloseWaveOut(void)
{
    NwPcmSink_Shutdown();
}
#endif /* NW_UNIT_UnprepareAndCloseWaveOut */
