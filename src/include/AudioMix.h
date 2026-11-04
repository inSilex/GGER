/*
 * AudioMix -- mixing, transport opcodes, and sample-buffer processing.
 *
 * Group interface.  The implementation lives in the units listed below;
 * each unit keeps its own file-scope declarations but exports the functions
 * declared here through the group header.
 */

#ifndef NW_GROUP_AUDIO_AUDIOMIX_C_H
#define NW_GROUP_AUDIO_AUDIOMIX_C_H

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
/*   ApplyRollingKeyXorToBuffer                                 audio/ApplyRollingKeyXorToBuffer.c */
/*   BlendSaturatedByteBuffers                                  audio/BlendSaturatedByteBuffers.c */
/*   CopyDwordsAndFillTailWithSilence                           audio/CopyDwordsAndFillTailWithSilence.c */
/*   DispatchAudioActionByIndex                                 audio/DispatchAudioActionByIndex.c */
/*   HandleAudioTransportOpcode                                 audio/HandleAudioTransportOpcode.c */
/*   MixActiveAudioSlotsIntoWaveOutBuffer                       audio/MixActiveAudioSlotsIntoWaveOutBuffer.c */

/* Exported functions.  Signatures use the unit's interface types;
 * functions whose interface depends on a unit-local type are intentionally
 * omitted from this overview. */
extern void __stdcall ApplyRollingKeyXorToBuffer(uint dwKeyIndex, int nCount, unsigned char *pBuf);
extern void BlendSaturatedByteBuffers (unsigned int dwByteCount, const unsigned int *pdwSrc, unsigned int *pdwDst);
extern void CopyDwordsAndFillTailWithSilence (uint32_t dwByteCount, const uint32_t *pdwSrc, uint32_t *pdwDst);
extern int __stdcall DispatchAudioActionByIndex(int action);
extern DWORD __stdcall HandleAudioTransportOpcode(DWORD dwOpCode);
extern void MixActiveAudioSlotsIntoWaveOutBuffer (void);

#ifdef __cplusplus
}
#endif

#endif /* NW_GROUP_AUDIO_AUDIOMIX_C_H */
