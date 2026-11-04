/*
 * Grid-delta helpers shared by the VM dispatcher.
 *
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#include-griddeltaapply-h
 */

#ifndef NW_GRID_DELTA_APPLY_H
#define NW_GRID_DELTA_APPLY_H

#include <stdint.h>

/* Copy n distinct stream bytes into n consecutive destination cells. */
static inline void NwGridDeltaAddRun(uintptr_t *pDst, uintptr_t *pStream,
                                     uint32_t n)
{
    do {
        uint8_t byte = *(const uint8_t *)(*pStream);
        *pStream += 1u;
        *(uint8_t *)(*pDst) = (uint8_t)(*(uint8_t *)(*pDst) + byte);
        *pDst += 1u;
    } while (--n != 0u);
}

static inline void NwGridDeltaFillRun(uintptr_t *pDst, uintptr_t *pStream,
                                      uint32_t n)
{
    uint8_t byte = *(const uint8_t *)(*pStream);
    *pStream += 1u;
    do {
        *(uint8_t *)(*pDst) = (uint8_t)(*(uint8_t *)(*pDst) + byte);
        *pDst += 1u;
    } while (--n != 0u);
}

#endif /* NW_GRID_DELTA_APPLY_H */
