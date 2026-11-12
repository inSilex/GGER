#ifndef NW_IOS_MEMORY_H
#define NW_IOS_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Low-address memory helpers used by the iOS game.
 *
 * Two layouts are supported:
 *
 *   low   The recovered engine's original model: every guest object lives
 *         below 4 GiB.  Available for an explicitly low game link.  On a
 *         stock arm64 process the default __PAGEZERO segment occupies that
 *         range, so these helpers try explicit Mach VM mappings and report
 *         failure without aborting the process.
 *
 *   window The 64-bit model used by the game: guest addresses are offsets
 *         from a 4 GiB-aligned base (nw_guest_base) above 4 GiB.  The image,
 *         data window, side table, heap, and framebuffer all live inside that
 *         single window.  NwIosAllocWindow() scans the window for free ranges.
 *
 * The protection argument uses the usual PROT_READ / PROT_WRITE / PROT_EXEC
 * bits from <sys/mman.h>.
 */
void *NwIosAllocLow(size_t size, int prot);
void *NwIosAllocLowAt(void *hint, size_t size, int prot);
void *NwIosAllocWindow(uintptr_t base, size_t size, int prot);
void NwIosFreeLow(void *p, size_t size);
int NwIosProtectLow(void *p, size_t size, int prot);

/* True when the current process can reach addresses below 4 GiB. */
int NwIosHasLowAddressSpace(void);

#ifdef __cplusplus
}
#endif

#endif /* NW_IOS_MEMORY_H */
