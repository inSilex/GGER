/*
 * nwarch.h -- architecture and address-space boundary.
 *
 * The recovered engine has two address spaces:
 *
 *   guest  32-bit offsets and addresses used by the VM, save files, the
 *          record stream and the original data-window layout;
 *   host   native pointers and sizes used by the current process.
 *
 * The Win32 oracle keeps its original i686 layout.  Portable targets must not
 * assume that sizeof(void *) == 4 or that a pointer can be stored in a 32-bit
 * slot.  This header is the single place where those two models meet.
 *
 * NW_WINDOW_PTR_REF() and NW_WINDOW_PTR_ARRAY() are the only supported way to
 * access a pointer-sized field inside nw_data_window.  On 32-bit builds they
 * address the original slot directly.  On 64-bit builds they address a native
 * pointer side table whose entries are initialized from the 32-bit snapshot.
 * Scalar fields keep their recovered byte offsets in both cases.
 */

#ifndef NW_ARCH_H
#define NW_ARCH_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef NW_WINDOW_BYTES
#define NW_WINDOW_BYTES 0x6820u
#endif

#ifndef NW_WINDOW_PTR_SIDETABLE
# if UINTPTR_MAX > 0xffffffffu
#  define NW_WINDOW_PTR_SIDETABLE 1
# else
#  define NW_WINDOW_PTR_SIDETABLE 0
# endif
#endif

typedef uint32_t nw_guest_addr_t;  /* VM / save / stream address space */
typedef uintptr_t nw_host_ptr_t;   /* native host pointer               */
typedef size_t nw_size_t;          /* host allocation and count type    */

extern unsigned char nw_data_window[];

#if NW_WINDOW_PTR_SIDETABLE
extern nw_host_ptr_t g_nw_window_ptr_slots[NW_WINDOW_BYTES / 4u];

/*
 * The side table is word-indexed so the original 4-byte offsets stay valid.
 * Each entry is one native pointer.
 */
#define NW_WINDOW_PTR_REF(type, off) \
    (*(type **)(void *)&g_nw_window_ptr_slots[(off) >> 2])

#define NW_WINDOW_PTR_ARRAY(type, off) \
    (*(type *(*)[1])(void *)&g_nw_window_ptr_slots[(off) >> 2])

#define NW_WINDOW_PTR_SLOTS(off) \
    ((void **)(void *)&g_nw_window_ptr_slots[(off) >> 2])

#define NW_WINDOW_PTR_VALUE(off) \
    (g_nw_window_ptr_slots[(off) >> 2])
#else
#define NW_WINDOW_PTR_REF(type, off) \
    (*(type **)(void *)(nw_data_window + (off)))

#define NW_WINDOW_PTR_ARRAY(type, off) \
    (*(type *(*)[1])(void *)(nw_data_window + (off)))

#define NW_WINDOW_PTR_SLOTS(off) \
    ((void **)(void *)(nw_data_window + (off)))

#define NW_WINDOW_PTR_VALUE(off) \
    (*(nw_host_ptr_t *)(void *)(nw_data_window + (off)))
#endif

/*
 * Base of the single 4 GiB guest window.
 *
 * Zero while every guest object lives below 4 GiB: the original 32-bit
 * layout, and the Linux/Android/Windows-low targets.  Guest addresses are
 * then plain host addresses and both conversions are identities.
 *
 * iOS cannot obtain memory below 4 GiB, so the whole guest world (data
 * window, pointer side table, heap arenas and every guest allocation) is
 * placed in one 4 GiB-aligned window and a guest address becomes an offset
 * from this base.  On Windows, NW_FORCE_HIGH=1 exercises the same layout on
 * the desktop build.
 *
 * The base is always 4 GiB-aligned, so `(uint32_t)(uintptr_t)host` still
 * yields the guest offset for any host pointer inside the window: the store
 * direction needs no change, only loads need nw_guest_to_host().
 */
extern uintptr_t nw_guest_base;

/*
 * Guest zero is the null pointer, never an object: the window reserves its
 * first 64 KiB and every guest allocation starts above it.  Preserving the
 * null through both conversions keeps every `if (p)` / `p != NULL` test on a
 * converted guest pointer correct.  Without this, nw_guest_to_host(0) would
 * yield nw_guest_base (a non-null address) and list terminators would walk
 * into the unmapped start of the window.
 *
 * Note that passing an already-converted host pointer here is idempotent
 * because nw_guest_base is 4 GiB-aligned, and no live object sits at guest
 * offset 0, so the null special case cannot misclassify one.
 */
static inline nw_host_ptr_t nw_guest_to_host(nw_guest_addr_t guest)
{
    return guest != 0u
        ? (nw_host_ptr_t)(nw_guest_base + (uintptr_t)guest)
        : (nw_host_ptr_t)0;
}

static inline nw_guest_addr_t nw_host_to_guest(const void *host)
{
    return (uintptr_t)host != 0u
        ? (nw_guest_addr_t)((uintptr_t)host - nw_guest_base)
        : 0u;
}

static inline uint32_t nw_guest_ld32(const void *p)
{
    uint32_t value;
    memcpy(&value, p, sizeof value);
    return value;
}

static inline void nw_guest_st32(void *p, uint32_t value)
{
    memcpy(p, &value, sizeof value);
}

/*
 * Compile-time width checks.  These are deliberately plain typedefs rather
 * than _Static_assert so the same header works with the older corpus
 * compilers used by the oracle build.
 */
typedef char nw_arch_guest_is_32[(sizeof(nw_guest_addr_t) == 4u) ? 1 : -1];
typedef char nw_arch_host_ptr_fits_size[(sizeof(nw_host_ptr_t) == sizeof(void *)) ? 1 : -1];

#endif /* NW_ARCH_H */
