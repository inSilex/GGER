/*
 * AudioMix -- mixing, transport opcodes, and sample-buffer processing.
 *
 * Module: audio
 * Interface: include/AudioMix.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   ApplyRollingKeyXorToBuffer                                 audio/ApplyRollingKeyXorToBuffer.c
 *   BlendSaturatedByteBuffers                                  audio/BlendSaturatedByteBuffers.c
 *   CopyDwordsAndFillTailWithSilence                           audio/CopyDwordsAndFillTailWithSilence.c
 *   DispatchAudioActionByIndex                                 audio/DispatchAudioActionByIndex.c
 *   HandleAudioTransportOpcode                                 audio/HandleAudioTransportOpcode.c
 *   MixActiveAudioSlotsIntoWaveOutBuffer                       audio/MixActiveAudioSlotsIntoWaveOutBuffer.c
 */

/* ==========================================================================
 * Unit: ApplyRollingKeyXorToBuffer
 * Source: audio/ApplyRollingKeyXorToBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_ApplyRollingKeyXorToBuffer
/*
 * Apply Rolling Key Xor To Buffer.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-applyrollingkeyxortobuffer-c
 */

#include <stdint.h>

typedef unsigned int uint;

extern unsigned char g_abRollingXorKey[32];

void __stdcall ApplyRollingKeyXorToBuffer(uint dwKeyIndex, int nCount, unsigned char *pBuf)
{
    /* do/while: the JNZ back-edge makes nCount == 0 still transform one byte. */
    while (nCount != 0) {
        dwKeyIndex++;                                /* INC EDX — index advances first */
        *pBuf = *pBuf ^ g_abRollingXorKey[dwKeyIndex & 0x1f];
        pBuf++;                                      /* INC ESI — walk the buffer       */
        nCount--;                                    /* DEC EBX — countdown            */
    }
}
#endif /* NW_UNIT_ApplyRollingKeyXorToBuffer */

/* ==========================================================================
 * Unit: BlendSaturatedByteBuffers
 * Source: audio/BlendSaturatedByteBuffers.c
 * ========================================================================== */
#ifdef NW_UNIT_BlendSaturatedByteBuffers
/*
 * Blend Saturated Byte Buffers.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-blendsaturatedbytebuffers-c
 */

static unsigned char BlendSaturatedByte(unsigned char bSrc, unsigned char bDst)
{
    /* The SUB AL,BL / SUB DL,BL pair puts both operands into signed [-128,127]
     * as (byte - 0x80); ADD AL,DL is therefore an ordinary signed 8-bit add of
     * those biased values, and JO reports whether it overflowed. */
    int iSum = (int)bSrc - 0x80 + ((int)bDst - 0x80);   /* SUB,SUB then ADD */
    unsigned char bWrapped = (unsigned char)iSum;

    if (iSum > 127 || iSum < -128) {            /* JO taken                */
        return ((signed char)bWrapped < 0) ? 0xF0u  
                                          : 0x00u; /*     -> XOR AL,AL  */
    }
    return (unsigned char)(iSum + 0x80);         /* ADD AL,BL             */
}

void BlendSaturatedByteBuffers(unsigned int  dwByteCount,
                               const unsigned int *pdwSrc,
                               unsigned int        *pdwDst)
{
    unsigned int iDwords = dwByteCount >> 2;    /* SHR ECX,0x2 */

    for (; iDwords != 0; --iDwords)             /* DEC ECX ; JNZ loop */
    {
        unsigned int dwSrc = *pdwSrc++;         /* LODSD      */
        unsigned int dwOld = *pdwDst;           
        unsigned int dwOut;

        dwOut  = (unsigned int)BlendSaturatedByte((unsigned char)(dwSrc      ),
                                                  (unsigned char)(dwOld      ));
        dwOut |= (unsigned int)BlendSaturatedByte((unsigned char)(dwSrc >>  8),
                                                  (unsigned char)(dwOld >>  8)) <<  8;
        dwOut |= (unsigned int)BlendSaturatedByte((unsigned char)(dwSrc >> 16),
                                                  (unsigned char)(dwOld >> 16)) << 16;
        dwOut |= (unsigned int)BlendSaturatedByte((unsigned char)(dwSrc >> 24),
                                                  (unsigned char)(dwOld >> 24)) << 24;

        *pdwDst++ = dwOut;                      /* STOSD ES:EDI */
    }
}
#endif /* NW_UNIT_BlendSaturatedByteBuffers */

/* ==========================================================================
 * Unit: CopyDwordsAndFillTailWithSilence
 * Source: audio/CopyDwordsAndFillTailWithSilence.c
 * ========================================================================== */
#ifdef NW_UNIT_CopyDwordsAndFillTailWithSilence
/*
 * Copy Dwords And Fill Tail With Silence.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-copydwordsandfilltailwithsilence-c
 */

#include <stdint.h>   /* uint32_t, intptr_t */

/*
 * Pure leaf: no callees, no imports, no globals. Everything is stack-resident.
 * ESI = source, EDI = destination, ECX = element count, EAX = filler/count.
 */
void CopyDwordsAndFillTailWithSilence(uint32_t        dwByteCount,  /* ARG1 [EBP+0x8]  */
                                      const uint32_t *pdwSrc,       /* ARG2 [EBP+0xc]  */
                                      uint32_t       *pdwDst)       /* ARG3 [EBP+0x10] */
{

    uint32_t dwDwords = dwByteCount >> 2;
    uint32_t i;

    for (i = 0u; i < dwDwords; ++i)         /* CLD ; MOVSD.REP ES:EDI,ESI */
    {
        pdwDst[i] = pdwSrc[i];
    }

    uint32_t dwFill = 0x200u - dwDwords;

    if (dwFill != 0u)                        
    {
        uint32_t       *pdwTail   = pdwDst + dwDwords;  /* EDI already advanced past the copy */
        const uint32_t  dwSilence = 0x80808080u;        

        for (i = 0u; i < dwFill; ++i)        /* STOSD.REP ES:EDI */
        {
            pdwTail[i] = dwSilence;
        }
    }
}
#endif /* NW_UNIT_CopyDwordsAndFillTailWithSilence */

/* ==========================================================================
 * Unit: DispatchAudioActionByIndex
 * Source: audio/DispatchAudioActionByIndex.c
 * ========================================================================== */
#ifdef NW_UNIT_DispatchAudioActionByIndex
/*
 * Dispatch Audio Action By Index.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-dispatchaudioactionbyindex-c
 */

#include <stdint.h>
#include "nwglobals.h"

extern void DispatchMciAudioReset(void);
extern uint32_t __stdcall DispatchMciPlaybackCommand(uint32_t dwOpCode);

int __stdcall DispatchAudioActionByIndex(int action)
{
    switch (action) {
    case 0:
        DispatchMciAudioReset();
        break;

    case 1:
        if (g_bVmAudioStateByte != 0xffu)
            (void)DispatchMciPlaybackCommand(1u);
        break;

    case 2:
        if (g_bVmAudioStateByte != 0xffu)
            (void)DispatchMciPlaybackCommand(2u);
        break;

    case 3:
        DispatchMciAudioReset();
        (void)DispatchMciPlaybackCommand(3u);
        break;

    default:
        break;
    }

    return 1;
}
#endif /* NW_UNIT_DispatchAudioActionByIndex */

/* ==========================================================================
 * Unit: HandleAudioTransportOpcode
 * Source: audio/HandleAudioTransportOpcode.c
 * ========================================================================== */
#ifdef NW_UNIT_HandleAudioTransportOpcode
/*
 * Handle Audio Transport Opcode.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-handleaudiotransportopcode-c
 */

#include <stdint.h>
#include <windows.h>   /* DWORD; the MCI helpers are internal to this project, not Win32 imports */

/* Sibling helpers in this project — they issue the actual mciSendCommandA calls. */
extern DWORD __stdcall DispatchMciPlaybackCommand(DWORD dwOpCode);        
extern DWORD DispatchMciSetFlagsAndPlay(BYTE bFlag, DWORD dwInEaxImage); 

/* Latched audio-transport state. Every one of these is a single byte, read or written directly. */
extern BYTE g_bOpZeroOrOne;        
extern BYTE g_bModeEnabled;        
extern BYTE g_bModeParam;          
extern BYTE g_bModeAltParam;       
extern BYTE g_bModeSelector;       
extern BYTE g_bPlaybackArmed;      
extern BYTE g_bPlayRequestFlag;    
extern BYTE g_bConfigForMode;      
extern BYTE g_bConfigForAltMode;   

/* Opcode 8 passes literal 0, which the helper maps to MCI_STOP (0x808). */
#define NW_OPCODE_STOP_TRANSPORT 0u

/* Opcode 9 is skipped when the stored request byte holds this sentinel. */
#define NW_PLAY_REQUEST_NONE 0xff

DWORD __stdcall HandleAudioTransportOpcode(DWORD dwOpCode)
{
    switch (dwOpCode) {

    case 0:
    case 1:

        g_bOpZeroOrOne = (BYTE)dwOpCode;
        break;

    case 2:
        g_bModeEnabled = 0;
        g_bModeParam   = 0;
        break;

    case 3:
        g_bModeEnabled = 1;
        g_bModeParam   = g_bConfigForMode;   
        break;

    case 4:
        g_bModeAltParam = 0;
        break;

    case 5:
        g_bModeAltParam = g_bConfigForAltMode; 
        break;

    case 6:
    case 7:
        /* ADD AL,0xfa == ADD 250 == SUB 6, truncated to a byte: 6 -> 0, 7 -> 1. */
        g_bModeSelector = (BYTE)((BYTE)dwOpCode + 0xfa);
        break;

    case 8:

        (void)DispatchMciPlaybackCommand(NW_OPCODE_STOP_TRANSPORT);
        g_bPlaybackArmed = 0;
        break;

    case 9:

        if (g_bPlayRequestFlag != NW_PLAY_REQUEST_NONE) {
            (void)DispatchMciSetFlagsAndPlay((BYTE)(signed char)g_bPlayRequestFlag, 0);
        }
        /* Reached whether or not the helper ran — the JZ skips only the CALL. */
        g_bPlaybackArmed = 1;
        break;

    default:

        break;
    }

    return 1;
}
#endif /* NW_UNIT_HandleAudioTransportOpcode */

/* ==========================================================================
 * Unit: MixActiveAudioSlotsIntoWaveOutBuffer
 * Source: audio/MixActiveAudioSlotsIntoWaveOutBuffer.c
 * ========================================================================== */
#ifdef NW_UNIT_MixActiveAudioSlotsIntoWaveOutBuffer
/*
 * Mix Active Audio Slots Into Wave Out Buffer.
 *
 * Module: audio
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#audio-mixactiveaudioslotsintowaveoutbuffer-c
 */

#include <windows.h>
#include "nwglobals.h"   /* BYTE, DWORD, FARPROC, WINAPI -- the flat win32 surface */
#include <stdint.h>    /* uint8_t, uint32_t, uintptr_t */

typedef struct tagNETWALK_AUDIO_SLOT {
    uint32_t dwActive;          /* +0x0  zero = free, nonzero = active      */
    uint32_t dwSrcCursor;       /* +0x4  source byte cursor (streaming)     */
    uint32_t dwRemainingBytes;  /* +0x8  remaining BYTE count, clamp 0x800  */
    uint32_t dwAuxFlags;        /* +0xc  byte fields written by the inserter */
} NETWALK_AUDIO_SLOT;

extern NETWALK_AUDIO_SLOT g_aAudioSlots[];     
extern uint32_t           g_dwAudioSlotsMixed; 

extern uint8_t g_abWaveOutPool[];              

extern BYTE (WINAPI *g_pfnSelectWaveBufferIndex)(void);   
extern void (WINAPI *g_pfnNotifyMixedWaveBuffer)(void);   

extern void CopyDwordsAndFillTailWithSilence(uint32_t       dwByteCount,
                                             const uint32_t *pdwSrc,
                                             uint32_t       *pdwDst);
extern void BlendSaturatedByteBuffers(unsigned int        dwByteCount,
                                      const unsigned int *pdwSrc,
                                      unsigned int        *pdwDst);

void MixActiveAudioSlotsIntoWaveOutBuffer(void)
{
    uint32_t  dwMixed  = 0u;        /* EDI: active slots mixed this pass   */
    uint32_t  dwBufIdx;             /* low byte of the handler's return    */
    uint32_t *pdwDst;               /* EBP: destination pool buffer        */
    uintptr_t cur;                  /* EBX: walking slot pointer           */
    uintptr_t end;                  /* ECX: base + count*16, the loop bound */

    dwBufIdx = g_pfnSelectWaveBufferIndex();

    pdwDst = (uint32_t *)((uintptr_t)g_abWaveOutPool + ((uintptr_t)dwBufIdx << 11));

    cur = (uintptr_t)g_aAudioSlots;
    end = (uintptr_t)g_aAudioSlots + (uintptr_t)g_dwAudioSlotCount * 16u;

    do {
        NETWALK_AUDIO_SLOT *pSlot = (NETWALK_AUDIO_SLOT *)cur;

        if (pSlot->dwActive != 0u) {            
            uint32_t dwBytes = pSlot->dwRemainingBytes;

            if (dwBytes > 0x800u) {             
                dwBytes = 0x800u;               /*   unsigned clamp to 0x800      */
            }

            if (dwMixed == 0u) {                
                CopyDwordsAndFillTailWithSilence(/* f(count, src, dst)            */
                    dwBytes,
                    (const uint32_t *)(uintptr_t)nw_guest_to_host(
                        pSlot->dwSrcCursor),
                    pdwDst);
            } else {
                BlendSaturatedByteBuffers(
                    dwBytes,
                    (const unsigned int *)(uintptr_t)nw_guest_to_host(
                        pSlot->dwSrcCursor),
                    (unsigned int *)pdwDst);
            }

            pSlot->dwRemainingBytes -= dwBytes; /* SUB [EBX+0x8],ESI              */
            if (pSlot->dwRemainingBytes != 0u) { 
                pSlot->dwSrcCursor += dwBytes;   /* ADD [EBX+0x4],ESI            */
            } else {
                pSlot->dwActive = 0u;            /* MOV [EBX],0                  */
            }

            dwMixed++;                           /* INC EDI                      */
        }

        cur += 16u;                              /* ADD EBX,0x10                 */
    } while (end > cur);                         

    g_dwAudioSlotsMixed = dwMixed;               

    if (dwMixed != 0u) {
        g_pfnNotifyMixedWaveBuffer();        
    }
}
#endif /* NW_UNIT_MixActiveAudioSlotsIntoWaveOutBuffer */
