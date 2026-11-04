/*
 * NwMidiSynth -- static TinySoundFont wrapper for cross-platform MIDI.
 *
 * Module: audio
 * Interface: include/NwMidiSynth.h
 *
 * Unit:
 *   NwMidiSynth                                                 audio/NwMidiSynth.c
 *
 * This is the only translation unit that instantiates TinySoundFont.  It is
 * compiled with -Os -DTSF_NO_STDIO -ffunction-sections -fdata-sections and the
 * TSF `.text$tsf*` sections are placed into the pinned `.nwdiag` section by
 * tools/nw_sdl.ld.  The wrapper itself is annotated NW_DIAG.
 *
 * Two pieces of GS-style behaviour are layered on top of TSF:
 *
 *   - Channel volume follows the GM/GS square-law curve instead of TSF's cubic
 *     one.  The Microsoft GS Wavetable Synth this port stands in for uses the
 *     square law, which keeps quiet MIDI parts (CC7 down to ~60 in NetWalk's
 *     songs) audible.
 *   - A small stereo reverb supplies the GS default ambience.  TSF has no
 *     effects, and the Windows synth's reverb is part of what the game sounds
 *     like.
 */

#ifdef NW_UNIT_NwMidiSynth

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "tsf.h"

#include "NwMidiSynth.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

/* Master headroom.  TSF clamps its fixed-point output to int16, so the sum of
 * up to 128 voices needs room.  -10.5 dB leaves space for the reverb send on
 * top of the previously measured -9 dB peak.  Individual MIDI channel volumes
 * still shape the mix. */
#define NW_SYNTH_GAIN_DB (-10.5f)

/* Pre-allocate voices so the audio callback never reallocs. */
#define NW_SYNTH_MAX_VOICES 128u

/* ------------------------------------------------------------------------- */
/* GS-style reverb: four parallel combs into two allpasses, per channel.      */
/* Tunings are Freeverb's 44.1 kHz values; the right channel is offset by 23   */
/* samples for width.  The wet path is mono-summed so the effect is stable    */
/* regardless of pan.                                                         */
/* ------------------------------------------------------------------------- */

#define NW_REV_COMBS    4
#define NW_REV_APS      2
#define NW_REV_FEEDBACK 0.84f
#define NW_REV_DAMP     0.20f
#define NW_REV_INPUT    0.015f
#define NW_REV_WET      0.28f
#define NW_REV_DRY      0.90f

typedef struct {
    float *buf;
    int    size;
    int    pos;
    float  store;
} NwRevComb;

typedef struct {
    float *buf;
    int    size;
    int    pos;
} NwRevAllpass;

typedef struct {
    NwRevComb    comb[NW_REV_COMBS];
    NwRevAllpass ap[NW_REV_APS];
} NwRevChannel;

static const int g_revCombTuning[NW_REV_COMBS] = { 1116, 1188, 1277, 1356 };
static const int g_revApTuning[NW_REV_APS]     = {  556,  441 };

struct NwSynth {
    tsf           *font;
    NwRevChannel   rev[2];
    int            reverb;
    unsigned char  cc7[16];
    unsigned char  cc11[16];
};

NW_DIAG static void NwSynth_ReverbFree(NwSynth *synth)
{
    int ch;
    int i;

    for (ch = 0; ch < 2; ch++) {
        for (i = 0; i < NW_REV_COMBS; i++) {
            free(synth->rev[ch].comb[i].buf);
            synth->rev[ch].comb[i].buf = 0;
        }
        for (i = 0; i < NW_REV_APS; i++) {
            free(synth->rev[ch].ap[i].buf);
            synth->rev[ch].ap[i].buf = 0;
        }
    }
}

NW_DIAG static int NwSynth_ReverbInit(NwSynth *synth, int sample_rate)
{
    float scale = (float)sample_rate / 44100.0f;
    int   ch;
    int   i;

    if (scale < 0.25f)
        scale = 0.25f;
    if (scale > 4.0f)
        scale = 4.0f;

    for (ch = 0; ch < 2; ch++) {
        for (i = 0; i < NW_REV_COMBS; i++) {
            int n = (int)((float)g_revCombTuning[i] * scale) + (ch ? 23 : 0);
            synth->rev[ch].comb[i].buf = (float *)calloc((size_t)n, sizeof(float));
            if (synth->rev[ch].comb[i].buf == 0)
                return 0;
            synth->rev[ch].comb[i].size = n;
        }
        for (i = 0; i < NW_REV_APS; i++) {
            int n = (int)((float)g_revApTuning[i] * scale) + (ch ? 23 : 0);
            synth->rev[ch].ap[i].buf = (float *)calloc((size_t)n, sizeof(float));
            if (synth->rev[ch].ap[i].buf == 0)
                return 0;
            synth->rev[ch].ap[i].size = n;
        }
    }
    return 1;
}

NW_DIAG static void NwSynth_ReverbClear(NwSynth *synth)
{
    int ch;
    int i;

    for (ch = 0; ch < 2; ch++) {
        for (i = 0; i < NW_REV_COMBS; i++) {
            if (synth->rev[ch].comb[i].buf != 0)
                memset(synth->rev[ch].comb[i].buf, 0,
                       (size_t)synth->rev[ch].comb[i].size * sizeof(float));
            synth->rev[ch].comb[i].pos   = 0;
            synth->rev[ch].comb[i].store = 0.0f;
        }
        for (i = 0; i < NW_REV_APS; i++) {
            if (synth->rev[ch].ap[i].buf != 0)
                memset(synth->rev[ch].ap[i].buf, 0,
                       (size_t)synth->rev[ch].ap[i].size * sizeof(float));
            synth->rev[ch].ap[i].pos = 0;
        }
    }
}

NW_DIAG static float NwSynth_Comb(NwRevComb *c, float in)
{
    float out = c->buf[c->pos];

    c->store = out * (1.0f - NW_REV_DAMP) + c->store * NW_REV_DAMP;
    c->buf[c->pos] = in + c->store * NW_REV_FEEDBACK;
    if (++c->pos >= c->size)
        c->pos = 0;
    return out;
}

NW_DIAG static float NwSynth_Allpass(NwRevAllpass *a, float in)
{
    float bufout = a->buf[a->pos];
    float out    = -in + bufout;

    a->buf[a->pos] = in + bufout * 0.5f;
    if (++a->pos >= a->size)
        a->pos = 0;
    return out;
}

NW_DIAG static void NwSynth_ApplyReverb(NwSynth *synth, int16_t *out, int frames)
{
    int f;

    for (f = 0; f < frames; f++) {
        float l = (float)out[f * 2 + 0] * (1.0f / 32768.0f);
        float r = (float)out[f * 2 + 1] * (1.0f / 32768.0f);
        float in = (l + r) * 0.5f * NW_REV_INPUT;
        float wet[2];
        int   ch;

        for (ch = 0; ch < 2; ch++) {
            float acc = 0.0f;
            int   i;
            for (i = 0; i < NW_REV_COMBS; i++)
                acc += NwSynth_Comb(&synth->rev[ch].comb[i], in);
            for (i = 0; i < NW_REV_APS; i++)
                acc = NwSynth_Allpass(&synth->rev[ch].ap[i], acc);
            wet[ch] = acc;
        }

        l = l * NW_REV_DRY + wet[0] * NW_REV_WET;
        r = r * NW_REV_DRY + wet[1] * NW_REV_WET;
        if (l > 1.0f) l = 1.0f; else if (l < -1.0f) l = -1.0f;
        if (r > 1.0f) r = 1.0f; else if (r < -1.0f) r = -1.0f;
        out[f * 2 + 0] = (int16_t)(l * 32767.0f);
        out[f * 2 + 1] = (int16_t)(r * 32767.0f);
    }
}

/* ------------------------------------------------------------------------- */
/* GM square-law channel volume.                                              */
/* ------------------------------------------------------------------------- */

NW_DIAG static float NwSynth_ChannelGain(const NwSynth *synth, int channel)
{
    float v = ((float)synth->cc7[channel] / 127.0f) *
              ((float)synth->cc11[channel] / 127.0f);
    return v * v;   /* 40 dB/decade, the GM/GS channel-volume curve */
}

NW_DIAG static void NwSynth_UpdateChannelGain(NwSynth *synth, int channel)
{
    (void)tsf_channel_set_volume(synth->font, channel,
                                 NwSynth_ChannelGain(synth, channel));
}

/* ------------------------------------------------------------------------- */

NW_DIAG NwSynth *NwSynth_Create(const void *sf2_data, int sf2_size, int sample_rate)
{
    tsf     *font;
    NwSynth *synth;
    int      channel;

    if (sf2_data == 0 || sf2_size <= 0 || sample_rate <= 0)
        return 0;

    font = tsf_load_memory(sf2_data, sf2_size);
    if (font == 0)
        return 0;

    tsf_set_output(font, TSF_STEREO_INTERLEAVED, sample_rate, NW_SYNTH_GAIN_DB);

    if (tsf_set_max_voices(font, (int)NW_SYNTH_MAX_VOICES) == 0) {
        tsf_close(font);
        return 0;
    }

    /* Create all channels up front; channel 9 uses the MIDI drum bank. */
    for (channel = 0; channel < 16; channel++)
        tsf_channel_set_presetnumber(font, channel, 0, (channel == 9) ? 1 : 0);

    synth = (NwSynth *)malloc(sizeof(NwSynth));
    if (synth == 0) {
        tsf_close(font);
        return 0;
    }
    memset(synth, 0, sizeof(*synth));
    synth->font = font;
    for (channel = 0; channel < 16; channel++) {
        synth->cc7[channel]  = 127;
        synth->cc11[channel] = 127;
    }
    synth->reverb = NwSynth_ReverbInit(synth, sample_rate);
    if (!synth->reverb) {
        NwSynth_ReverbFree(synth);
        memset(synth->rev, 0, sizeof(synth->rev));
    }
    return synth;
}

NW_DIAG void NwSynth_Destroy(NwSynth *synth)
{
    if (synth == 0)
        return;
    NwSynth_ReverbFree(synth);
    tsf_close(synth->font);
    free(synth);
}

NW_DIAG void NwSynth_ShortMsg(NwSynth *synth, uint32_t msg)
{
    tsf          *font;
    unsigned char status;
    unsigned char data1;
    unsigned char data2;
    unsigned char high;
    int           channel;

    if (synth == 0)
        return;

    font    = synth->font;
    status  = (unsigned char)(msg & 0xFFu);
    data1   = (unsigned char)((msg >> 8) & 0x7Fu);
    data2   = (unsigned char)((msg >> 16) & 0x7Fu);
    high    = (unsigned char)(status & 0xF0u);
    channel = (int)(status & 0x0Fu);

    switch (high) {
    case 0x80u:                                    /* note off */
        tsf_channel_note_off(font, channel, (int)data1);
        break;
    case 0x90u:                                    /* note on (velocity 0 = off) */
        if (data2 == 0u)
            tsf_channel_note_off(font, channel, (int)data1);
        else
            tsf_channel_note_on(font, channel, (int)data1, (float)data2 / 127.0f);
        break;
    case 0xA0u:                                    /* polyphonic aftertouch: ignore */
        break;
    case 0xB0u:                                    /* control change */
        if (data1 == 7u) {                         /* channel volume */
            synth->cc7[channel] = data2;
            NwSynth_UpdateChannelGain(synth, channel);
        } else if (data1 == 11u) {                 /* expression */
            synth->cc11[channel] = data2;
            NwSynth_UpdateChannelGain(synth, channel);
        } else if (data1 == 121u) {                /* all controllers off */
            synth->cc7[channel]  = 127;
            synth->cc11[channel] = 127;
            (void)tsf_channel_midi_control(font, channel, (int)data1, (int)data2);
            NwSynth_UpdateChannelGain(synth, channel);
        } else {
            (void)tsf_channel_midi_control(font, channel, (int)data1, (int)data2);
        }
        break;
    case 0xC0u:                                    /* program change */
        (void)tsf_channel_set_presetnumber(font, channel, (int)data1,
                                           (channel == 9) ? 1 : 0);
        break;
    case 0xD0u:                                    /* channel pressure: ignore */
        break;
    case 0xE0u:                                    /* pitch wheel */
        (void)tsf_channel_set_pitchwheel(font, channel,
                                         ((int)data2 << 7) | (int)data1);
        break;
    default:
        break;
    }
}

NW_DIAG void NwSynth_Reset(NwSynth *synth)
{
    int channel;

    if (synth == 0)
        return;

    for (channel = 0; channel < 16; channel++) {
        tsf_channel_sounds_off_all(synth->font, channel);
        (void)tsf_channel_set_sustain(synth->font, channel, 0);
        (void)tsf_channel_set_pitchwheel(synth->font, channel, 8192);
        synth->cc7[channel]  = 127;
        synth->cc11[channel] = 127;
        NwSynth_UpdateChannelGain(synth, channel);
    }
    if (synth->reverb)
        NwSynth_ReverbClear(synth);
}

NW_DIAG void NwSynth_Render(NwSynth *synth, int16_t *out, int frames)
{
    if (frames <= 0)
        return;
    if (synth == 0) {
        memset(out, 0, (size_t)frames * 2u * sizeof(int16_t));
        return;
    }
    tsf_render_short(synth->font, out, frames, 0);
    if (synth->reverb)
        NwSynth_ApplyReverb(synth, out, frames);
}

NW_DIAG int NwSynth_PresetCount(const NwSynth *synth)
{
    if (synth == 0)
        return 0;
    return tsf_get_presetcount(synth->font);
}

#endif /* NW_UNIT_NwMidiSynth */
