/*
 * GameState -- game-mode flags, random seeding, and mode selection.
 *
 * Module: game
 * Interface: include/GameState.h
 *
 * This file groups related units.  Every unit is a separately
 * compiled translation unit selected by its NW_UNIT_* macro;
 * the units share one source file but keep independent file-scope
 * declarations, so the layout and external symbols are unchanged.
 *
 * Units:
 *   ClearGridActiveFlag                                        game/ClearGridActiveFlag.c
 *   InitGlobalRngAndResetStateFlags                            game/InitGlobalRngAndResetStateFlags.c
 *   IsSettingActiveByIndex                                     game/IsSettingActiveByIndex.c
 *   SeedRandomFromClockAndClearWorkQueue                       game/SeedRandomFromClockAndClearWorkQueue.c
 *   SetOutOfMemoryFlag                                         game/SetOutOfMemoryFlag.c
 *   SetStateFlagOnce                                           game/SetStateFlagOnce.c
 */

/* ==========================================================================
 * Unit: ClearGridActiveFlag
 * Source: game/ClearGridActiveFlag.c
 * ========================================================================== */
#ifdef NW_UNIT_ClearGridActiveFlag
/*
 * Clears the grid active-cell ready flag.
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-cleargridactiveflag-c
 */

#include <stdint.h>
#include "nwglobals.h"

void ClearGridActiveFlag(void)
{
    g_bActiveCellReady = 0;
}
#endif /* NW_UNIT_ClearGridActiveFlag */

/* ==========================================================================
 * Unit: InitGlobalRngAndResetStateFlags
 * Source: game/InitGlobalRngAndResetStateFlags.c
 * ========================================================================== */
#ifdef NW_UNIT_InitGlobalRngAndResetStateFlags
/*
 * InitGlobalRngAndResetStateFlags
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-initglobalrngandresetstateflags-c
 */

#include <stdint.h>
#include "nwarch.h"
extern unsigned char nw_data_window[];

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define REG8(a)  (*(volatile uint8_t *)(uintptr_t)(a))

extern uint32_t CaptureLocalTimeAsEpochSeconds(uint32_t arg0);

extern uint32_t __stdcall DispatchPendingCommandLoop(uint32_t arg0);

/* Optional trace. */
extern void nwsafe_trace(const char *);

uint32_t InitGlobalRngAndResetStateFlags(void)
{
    uint32_t zero;

#ifdef NWDEBUG_ENABLED
    nwsafe_trace("s2_in");
#endif

    REG32((nw_data_window + 0x1c8)) = CaptureLocalTimeAsEpochSeconds(0u);

    REG32((nw_data_window + 0x1c8)) = REG32((nw_data_window + 0x1c8)) * 0x41c64e6du;
    REG32((nw_data_window + 0x1c8)) = REG32((nw_data_window + 0x1c8)) + 0x3039u;

    /* Two bounds set to 0xffff. */
    REG32((nw_data_window + 0x623c)) = 0xffffu;
    REG32((nw_data_window + 0x621c)) = 0xffffu;

    /* Pointer state dwords use the native side table on 64-bit builds. */
    NW_WINDOW_PTR_VALUE(0x6218) = 0u;
    NW_WINDOW_PTR_VALUE(0x6214) = 0u;
    REG32((nw_data_window + 0x6238)) = 0u;
    NW_WINDOW_PTR_VALUE(0x6384) = 0u;
    NW_WINDOW_PTR_VALUE(0x6380) = 0u;
    NW_WINDOW_PTR_VALUE(0x637c) = 0u;

    /* XOR EAX,EAX -- AL is 0 for the flag writes below. */
    zero = 0u;

    /* Flag bytes cleared from AL, then one flag set to 1. */
    REG8((nw_data_window + 0x6801)) = (uint8_t)zero;
    REG8((nw_data_window + 0x6802)) = (uint8_t)zero;
    REG8((nw_data_window + 0x6808)) = (uint8_t)zero;
    REG8((nw_data_window + 0x67f9)) = 0x1u;
    REG8((nw_data_window + 0x6807)) = (uint8_t)zero;   
    REG8((nw_data_window + 0x6806)) = (uint8_t)zero;
    REG8((nw_data_window + 0x67fb)) = (uint8_t)zero;
    REG8((nw_data_window + 0x67fa)) = (uint8_t)zero;
    REG8((nw_data_window + 0x1202)) = (uint8_t)zero;

#ifdef NWDEBUG_ENABLED
    nwsafe_trace("s2_before_dpc");
#endif

    return DispatchPendingCommandLoop(zero);
}
#endif /* NW_UNIT_InitGlobalRngAndResetStateFlags */

/* ==========================================================================
 * Unit: IsSettingActiveByIndex
 * Source: game/IsSettingActiveByIndex.c
 * ========================================================================== */
#ifdef NW_UNIT_IsSettingActiveByIndex
/*
 * Is Setting Active By Index.
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-issettingactivebyindex-c
 */

#include <stdint.h>
#include "nwglobals.h"

#define NW_DIAG __attribute__((section(".nwdiag")))

#define NWP8(off) (*(uint8_t *)(intptr_t)(nw_data_window + (off)))

NW_DIAG int __stdcall
IsSettingActiveByIndex(uint32_t index)
    __asm__("_IsSettingActiveByIndex");

NW_DIAG int __stdcall
IsSettingActiveByIndex(uint32_t index)
{
    uint8_t value;

    switch (index) {
    case 0: value = NWP8(0x6342); break; 
    case 1: value = NWP8(0x6340); break; 
    case 2: value = NWP8(0x67fb); break; 
    case 3: value = NWP8(0x6343); break; 
    case 4: value = NWP8(0x6344); break; 
    default: value = 0u; break;          /* invalid index: inactive */
    }

    return (value >= 1u) ? 1 : 0;
}

__asm__(".globl IsSettingActiveByIndex\n"
        ".set IsSettingActiveByIndex, _IsSettingActiveByIndex\n");
#endif /* NW_UNIT_IsSettingActiveByIndex */

/* ==========================================================================
 * Unit: SeedRandomFromClockAndClearWorkQueue
 * Source: game/SeedRandomFromClockAndClearWorkQueue.c
 * ========================================================================== */
#ifdef NW_UNIT_SeedRandomFromClockAndClearWorkQueue
/*
 * Seed Random From Clock And Clear Work Queue.
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-seedrandomfromclockandclearworkqueue-c
 */

#include <stdint.h>
#include <stdlib.h>
#include "nwarch.h"

extern unsigned char nw_data_window[] __asm__("nw_data_window");
extern int32_t CaptureLocalTimeAsEpochSeconds(int32_t *outSeconds);

/* Local decimal/0x-hex parser.  Calling strtoul here pulls a multi-kilobyte
 * msvcrt/libgcc helper into .text and moves nw_data_window off its pinned
 * address; the parser must not add a libc dependency to the shipping image. */
void SeedRandomFromClockAndClearWorkQueue(void)
{
    uint32_t *rng = (uint32_t *)(void *)(nw_data_window + 0x1c8);
    uint32_t seed = (uint32_t)CaptureLocalTimeAsEpochSeconds(NULL);

    *rng = seed;
    *rng = seed * 0x41c64e6du + 0x3039u;

    NW_WINDOW_PTR_VALUE(0x11b4) = 0;
    NW_WINDOW_PTR_VALUE(0x11b0) = 0;
}
#endif /* NW_UNIT_SeedRandomFromClockAndClearWorkQueue */

/* ==========================================================================
 * Unit: SetOutOfMemoryFlag
 * Source: game/SetOutOfMemoryFlag.c
 * ========================================================================== */
#ifdef NW_UNIT_SetOutOfMemoryFlag
/*
 * Set Out Of Memory Flag.
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-setoutofmemoryflag-c
 */

#include <stdint.h>

typedef uint32_t DWORD;

/* Out-of-memory error message (readable in image, .rsrc-adjacent data region). */
extern const char g_szOutOfMemoryMessage[]; 

extern DWORD __stdcall SetStateFlagOnce(DWORD dwValue); 

DWORD SetOutOfMemoryFlag(void)
{

    (void)SetStateFlagOnce((DWORD)(uintptr_t)g_szOutOfMemoryMessage);
    return 0;
}
#endif /* NW_UNIT_SetOutOfMemoryFlag */

/* ==========================================================================
 * Unit: SetStateFlagOnce
 * Source: game/SetStateFlagOnce.c
 * ========================================================================== */
#ifdef NW_UNIT_SetStateFlagOnce
/*
 * Latches the first fatal-diagnostic message and returns zero.
 *
 * Module: game
 * Conversion record: docs/reference/CONVERSION_PROVENANCE.md#game-setstateflagonce-c
 */

#include <stdint.h>

typedef uint32_t DWORD;

extern const char *g_pcFatalMessage; 

/* Latches pcMessage as the fatal diagnostic if none is latched yet. Always returns 0,
 * which is the "do not treat this as a value result" convention the callers rely on --
 * they branch on the pointer global, never on the return. */
DWORD __stdcall SetStateFlagOnce(const char *pcMessage)
{
    extern void nwtrace_hex(const char *, unsigned long);
    nwtrace_hex("latch_msg", (unsigned long)(uintptr_t)pcMessage);
    nwtrace_hex("latch_ra", (unsigned long)(uintptr_t)__builtin_return_address(0));
    if (g_pcFatalMessage == 0) {
        g_pcFatalMessage = pcMessage;
    }
    return 0;
}
#endif /* NW_UNIT_SetStateFlagOnce */
