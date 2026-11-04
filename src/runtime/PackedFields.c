/*
 * PackedFields -- packed nibble/byte field transforms.
 *
 * Module: runtime
 * Interface: include/PackedFields.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   NegateBothPackedFieldBytesAndRotateHighNibble              runtime/NegateBothPackedFieldBytesAndRotateHighNibble.c
 *   NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs runtime/NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs.c
 *   NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble  runtime/NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble.c
 *   NegateMidByteAndSwapHighNibbleBit1AndBit3                  runtime/NegateMidByteAndSwapHighNibbleBit1AndBit3.c
 *   NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble    runtime/NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble.c
 *   SwapPackedFieldBytesAndReverseHighNibble                   runtime/SwapPackedFieldBytesAndReverseHighNibble.c
 *   SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble   runtime/SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble.c
 */

/* ==========================================================================
 * Unit: NegateBothPackedFieldBytesAndRotateHighNibble
 * Source: runtime/NegateBothPackedFieldBytesAndRotateHighNibble.c
 * ========================================================================== */
#ifdef NW_UNIT_NegateBothPackedFieldBytesAndRotateHighNibble
/*
 * Both bytes at +2/+3 are negated in place; high nibble of +1 rotated left 2
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-negatebothpackedfieldbytesandrotatehighnibble-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

static uint8_t nw_rotl4_2(uint8_t x)
{
    return (uint8_t)(((x << 2) | (x >> 2)) & 0x0fu);
}

NW_DIAG void __stdcall
NegateBothPackedFieldBytesAndRotateHighNibble(uint8_t *p) __asm__("_NegateBothPackedFieldBytesAndRotateHighNibble");

NW_DIAG void __stdcall
NegateBothPackedFieldBytesAndRotateHighNibble(uint8_t *p)
{
    uint8_t n;
    p[3] = (uint8_t)(0u - p[3]);
    p[2] = (uint8_t)(0u - p[2]);
    n = p[1];
    p[1] = (uint8_t)((n & 0x0fu) | (uint8_t)(nw_rotl4_2((uint8_t)(n >> 4)) << 4));
}
#endif /* NW_UNIT_NegateBothPackedFieldBytesAndRotateHighNibble */

/* ==========================================================================
 * Unit: NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs
 * Source: runtime/NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs.c
 * ========================================================================== */
#ifdef NW_UNIT_NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs
/*
 * Negate Both Packed Field Bytes Then Swap And Exchange High Nibble Bit Pairs.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-negatebothpackedfieldbytesthenswapandexchangehighnibblebitpairs-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

static uint8_t nw_swap_adjacent_pairs(uint8_t x)
{
    return (uint8_t)(((x & 0x05u) << 1) | ((x & 0x0au) >> 1));
}

NW_DIAG void __stdcall
NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs(uint8_t *p) __asm__("_NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs");

NW_DIAG void __stdcall
NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs(uint8_t *p)
{
    uint8_t n;
    uint8_t lo = (uint8_t)(0u - p[2]);
    uint8_t hi = (uint8_t)(0u - p[3]);
    p[2] = hi;
    p[3] = lo;
    n = p[1];
    p[1] = (uint8_t)((n & 0x0fu) | (uint8_t)(nw_swap_adjacent_pairs((uint8_t)(n >> 4)) << 4));
}
#endif /* NW_UNIT_NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs */

/* ==========================================================================
 * Unit: NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble
 * Source: runtime/NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble.c
 * ========================================================================== */
#ifdef NW_UNIT_NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble
/*
 * Negate High Byte Then Swap Packed Field Bytes And Rotate High Nibble.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-negatehighbytethenswappackedfieldbytesandrotatehighnibble-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

static uint8_t nw_rotl4_1(uint8_t x)
{
    return (uint8_t)(((x << 1) | (x >> 3)) & 0x0fu);
}

NW_DIAG void __stdcall
NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble(uint8_t *p) __asm__("_NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble");

NW_DIAG void __stdcall
NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble(uint8_t *p)
{
    uint8_t n;
    uint8_t t = (uint8_t)(0u - p[3]);
    p[3] = p[2];
    p[2] = t;
    n = p[1];
    p[1] = (uint8_t)((n & 0x0fu) | (uint8_t)(nw_rotl4_1((uint8_t)(n >> 4)) << 4));
}
#endif /* NW_UNIT_NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble */

/* ==========================================================================
 * Unit: NegateMidByteAndSwapHighNibbleBit1AndBit3
 * Source: runtime/NegateMidByteAndSwapHighNibbleBit1AndBit3.c
 * ========================================================================== */
#ifdef NW_UNIT_NegateMidByteAndSwapHighNibbleBit1AndBit3
/*
 * Negate Mid Byte And Swap High Nibble Bit1And Bit3.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-negatemidbyteandswaphighnibblebit1andbit3-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

static uint8_t nw_swap13(uint8_t x)
{
    return (uint8_t)((x & 0x05u) | ((x & 0x02u) << 2) | ((x & 0x08u) >> 2));
}

NW_DIAG void __stdcall
NegateMidByteAndSwapHighNibbleBit1AndBit3(uint8_t *p) __asm__("_NegateMidByteAndSwapHighNibbleBit1AndBit3");

NW_DIAG void __stdcall
NegateMidByteAndSwapHighNibbleBit1AndBit3(uint8_t *p)
{
    uint8_t n;
    p[2] = (uint8_t)(0u - p[2]);
    n = p[1];
    p[1] = (uint8_t)((n & 0x0fu) | (uint8_t)(nw_swap13((uint8_t)(n >> 4)) << 4));
}
#endif /* NW_UNIT_NegateMidByteAndSwapHighNibbleBit1AndBit3 */

/* ==========================================================================
 * Unit: NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble
 * Source: runtime/NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble.c
 * ========================================================================== */
#ifdef NW_UNIT_NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble
/*
 * Negate Packed Field High Byte And Swap Low Pair Bits Of High Nibble.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-negatepackedfieldhighbyteandswaplowpairbitsofhighnibble-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

static uint8_t nw_swap02(uint8_t x)
{
    return (uint8_t)((x & 0x0au) | ((x & 0x01u) << 2) | ((x & 0x04u) >> 2));
}

NW_DIAG void __stdcall
NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble(uint8_t *p) __asm__("_NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble");

NW_DIAG void __stdcall
NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble(uint8_t *p)
{
    uint8_t n;
    p[3] = (uint8_t)(0u - p[3]);
    n = p[1];
    p[1] = (uint8_t)((n & 0x0fu) | (uint8_t)(nw_swap02((uint8_t)(n >> 4)) << 4));
}
#endif /* NW_UNIT_NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble */

/* ==========================================================================
 * Unit: SwapPackedFieldBytesAndReverseHighNibble
 * Source: runtime/SwapPackedFieldBytesAndReverseHighNibble.c
 * ========================================================================== */
#ifdef NW_UNIT_SwapPackedFieldBytesAndReverseHighNibble
/*
 * The bytes at +2/+3 are exchanged; high nibble of +1 is bit-reversed
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-swappackedfieldbytesandreversehighnibble-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

static uint8_t nw_reverse4(uint8_t x)
{
    return (uint8_t)(((x & 0x01u) << 3) | ((x & 0x02u) << 1) | ((x & 0x04u) >> 1) | ((x & 0x08u) >> 3));
}

NW_DIAG void __stdcall
SwapPackedFieldBytesAndReverseHighNibble(uint8_t *p) __asm__("_SwapPackedFieldBytesAndReverseHighNibble");

NW_DIAG void __stdcall
SwapPackedFieldBytesAndReverseHighNibble(uint8_t *p)
{
    uint8_t n;
    uint8_t t = p[2];
    p[2] = p[3];
    p[3] = t;
    n = p[1];
    p[1] = (uint8_t)((n & 0x0fu) | (uint8_t)(nw_reverse4((uint8_t)(n >> 4)) << 4));
}
#endif /* NW_UNIT_SwapPackedFieldBytesAndReverseHighNibble */

/* ==========================================================================
 * Unit: SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble
 * Source: runtime/SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble.c
 * ========================================================================== */
#ifdef NW_UNIT_SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble
/*
 * Swap Packed Field Bytes Negate New High Byte And Rotate High Nibble.
 *
 * Module: runtime
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#runtime-swappackedfieldbytesnegatenewhighbyteandrotatehighnibble-c
 */

#include <stdint.h>

#define NW_DIAG __attribute__((section(".nwdiag")))

static uint8_t nw_rotl4_3(uint8_t x)
{
    return (uint8_t)(((x << 3) | (x >> 1)) & 0x0fu);
}

NW_DIAG void __stdcall
SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble(uint8_t *p) __asm__("_SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble");

NW_DIAG void __stdcall
SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble(uint8_t *p)
{
    uint8_t n;
    uint8_t t = (uint8_t)(0u - p[2]);
    p[2] = p[3];
    p[3] = t;
    n = p[1];
    p[1] = (uint8_t)((n & 0x0fu) | (uint8_t)(nw_rotl4_3((uint8_t)(n >> 4)) << 4));
}
#endif /* NW_UNIT_SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble */
