#include "ios_memory.h"

#if defined(__APPLE__)

#include <errno.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <mach/vm_region.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/mman.h>

#define NW_IOS_LOW_BASE ((vm_address_t)0x10000000ULL)
#define NW_IOS_LOW_LIMIT ((vm_address_t)0x100000000ULL)
#define NW_IOS_PAGE ((vm_size_t)0x4000ULL)

/*
 * VirtualFree(MEM_RELEASE) is called with a zero size.  Remember the rounded
 * length of every low allocation so the Windows compatibility layer can
 * release those blocks without the caller repeating the size.  The table is
 * only touched by the game thread and the platforms that allocate here, but a
 * mutex keeps the bookkeeping safe if a worker frees a buffer at shutdown.
 */
#define NW_IOS_TRACK_MAX 4096

typedef struct NwIosTrack {
    vm_address_t addr;
    vm_size_t size;
} NwIosTrack;

static NwIosTrack g_nwIosTrack[NW_IOS_TRACK_MAX];
static pthread_mutex_t g_nwIosTrackLock = PTHREAD_MUTEX_INITIALIZER;

/*
 * A scan of the whole low range that found no usable page marks it exhausted.
 * While the flag is set, new allocations fail immediately instead of repeating
 * hundreds of thousands of vm_allocate calls (the case for a build whose
 * __PAGEZERO still covers the 32-bit mirror range).  Freeing low memory clears
 * it so an allocation can retry once space has been released.
 */
static int g_nwIosExhausted = 0;

static void NwIosTrackAdd(vm_address_t addr, vm_size_t size)
{
    int i;

    pthread_mutex_lock(&g_nwIosTrackLock);
    for (i = 0; i < NW_IOS_TRACK_MAX; i++) {
        if (g_nwIosTrack[i].size == 0) {
            g_nwIosTrack[i].addr = addr;
            g_nwIosTrack[i].size = size;
            break;
        }
    }
    pthread_mutex_unlock(&g_nwIosTrackLock);
}

static int NwIosTrackTake(vm_address_t addr, vm_size_t *size)
{
    int i;

    pthread_mutex_lock(&g_nwIosTrackLock);
    for (i = 0; i < NW_IOS_TRACK_MAX; i++) {
        if (g_nwIosTrack[i].size != 0 && g_nwIosTrack[i].addr == addr) {
            *size = g_nwIosTrack[i].size;
            g_nwIosTrack[i].addr = 0;
            g_nwIosTrack[i].size = 0;
            pthread_mutex_unlock(&g_nwIosTrackLock);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_nwIosTrackLock);
    return 0;
}

static vm_prot_t NwIosVmProt(int prot)
{
    vm_prot_t out = VM_PROT_NONE;
    if (prot & PROT_READ) {
        out |= VM_PROT_READ;
    }
    if (prot & PROT_WRITE) {
        out |= VM_PROT_WRITE;
    }
    if (prot & PROT_EXEC) {
        out |= VM_PROT_EXECUTE;
    }
    return out;
}

static int NwIosRoundSize(size_t size, vm_size_t *out)
{
    if (size == 0) {
        return 0;
    }
    if (size > SIZE_MAX - (NW_IOS_PAGE - 1)) {
        return 0;
    }
    *out = ((vm_size_t)size + NW_IOS_PAGE - 1) & ~(NW_IOS_PAGE - 1);
    return 1;
}

void *NwIosAllocLow(size_t size, int prot)
{
    vm_size_t rounded;
    vm_address_t base;

    if (!NwIosRoundSize(size, &rounded)) {
        return NULL;
    }
    if (g_nwIosExhausted) {
        errno = ENOMEM;
        return NULL;
    }

    // The public 64-bit Mach VM API on iOS is vm_allocate/vm_deallocate;
    // mach_vm.h exists only as an unsupported stub in the iPhoneOS SDK.
    for (base = NW_IOS_LOW_BASE; base + rounded <= NW_IOS_LOW_LIMIT;
         base += NW_IOS_PAGE) {
        vm_address_t addr = base;
        kern_return_t kr = vm_allocate(mach_task_self(), &addr, rounded,
                                       VM_FLAGS_FIXED);
        if (kr != KERN_SUCCESS) {
            continue;
        }
        if (vm_protect(mach_task_self(), addr, rounded, FALSE,
                       NwIosVmProt(prot)) != KERN_SUCCESS) {
            vm_deallocate(mach_task_self(), addr, rounded);
            continue;
        }
        NwIosTrackAdd(addr, rounded);
        return (void *)(uintptr_t)addr;
    }
    g_nwIosExhausted = 1;
    errno = ENOMEM;
    return NULL;
}

void *NwIosAllocLowAt(void *hint, size_t size, int prot)
{
    vm_size_t rounded;
    vm_address_t addr;

    if (hint == NULL) {
        return NwIosAllocLow(size, prot);
    }
    if (!NwIosRoundSize(size, &rounded)) {
        return NULL;
    }
    addr = (vm_address_t)(uintptr_t)hint;
    if ((addr & (NW_IOS_PAGE - 1)) != 0) {
        errno = EINVAL;
        return NULL;
    }
    /* The recovered engine stores pointers in 32-bit fields, so a mapping
     * that starts or ends at 4 GiB is just as unusable as a failed one. */
    if (addr >= NW_IOS_LOW_LIMIT || rounded > NW_IOS_LOW_LIMIT - addr) {
        errno = ENOMEM;
        return NULL;
    }
    if (vm_allocate(mach_task_self(), &addr, rounded, VM_FLAGS_FIXED) !=
        KERN_SUCCESS) {
        errno = ENOMEM;
        return NULL;
    }
    if (vm_protect(mach_task_self(), addr, rounded, FALSE,
                   NwIosVmProt(prot)) != KERN_SUCCESS) {
        vm_deallocate(mach_task_self(), addr, rounded);
        errno = EACCES;
        return NULL;
    }
    NwIosTrackAdd(addr, rounded);
    return (void *)(uintptr_t)addr;
}

/*
 * Return the first address after the VM region containing (or following)
 * `addr`.  Used to skip the image, the dyld shared cache, and other resident
 * mappings while scanning the guest window.  A failed query advances a page.
 */
static vm_address_t NwIosNextRegion(vm_address_t addr)
{
    vm_address_t region = addr;
    vm_size_t region_size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name = MACH_PORT_NULL;
    kern_return_t kr;

    kr = vm_region_64(mach_task_self(), &region, &region_size,
                      VM_REGION_BASIC_INFO_64,
                      (vm_region_info_t)(void *)&info, &count, &object_name);
    if (object_name != MACH_PORT_NULL) {
        (void)mach_port_deallocate(mach_task_self(), object_name);
    }
    if (kr == KERN_SUCCESS && region_size != 0) {
        vm_address_t next = region + region_size;
        if (next > addr) {
            return next;
        }
    }
    return addr + NW_IOS_PAGE;
}

/*
 * Allocate inside the 4 GiB guest window rooted at `base`.
 *
 * The window is [base, base + 4 GiB).  Guest addresses 0..0xffff are
 * reserved (guest zero must stay NULL), so allocation starts at base+0x10000.
 * vm_allocate(VM_FLAGS_FIXED) fails instead of replacing an existing mapping;
 * NwIosNextRegion() then jumps to the end of the blocking region.  Each
 * result is registered in the same table as the low allocator so
 * VirtualFree/NwIosProtectLow work unchanged.
 */
void *NwIosAllocWindow(uintptr_t base, size_t size, int prot)
{
    vm_size_t rounded;
    vm_address_t limit;
    vm_address_t cursor;

    if (base < NW_IOS_PAGE) {
        errno = EINVAL;
        return NULL;
    }
    if (!NwIosRoundSize(size, &rounded)) {
        return NULL;
    }
    limit = (vm_address_t)(base + UINT64_C(0x100000000));
    if (limit < (vm_address_t)base) {
        errno = EINVAL;
        return NULL;
    }
    cursor = (vm_address_t)(base + UINT64_C(0x10000));
    cursor = (cursor + (NW_IOS_PAGE - 1)) & ~(NW_IOS_PAGE - 1);

    while (cursor < limit && rounded <= limit - cursor) {
        vm_address_t addr = cursor;
        kern_return_t kr = vm_allocate(mach_task_self(), &addr, rounded,
                                       VM_FLAGS_FIXED);
        if (kr == KERN_SUCCESS) {
            if (vm_protect(mach_task_self(), addr, rounded, FALSE,
                           NwIosVmProt(prot)) == KERN_SUCCESS) {
                NwIosTrackAdd(addr, rounded);
                return (void *)(uintptr_t)addr;
            }
            (void)vm_deallocate(mach_task_self(), addr, rounded);
            cursor = addr + NW_IOS_PAGE;
            continue;
        }
        cursor = NwIosNextRegion(cursor);
    }
    errno = ENOMEM;
    return NULL;
}

void NwIosFreeLow(void *p, size_t size)
{
    vm_size_t rounded;
    vm_address_t addr;

    if (p == NULL) {
        return;
    }
    addr = (vm_address_t)(uintptr_t)p;
    if (!NwIosRoundSize(size, &rounded) || size == 0) {
        if (!NwIosTrackTake(addr, &rounded)) {
            return;
        }
    } else {
        vm_size_t ignored;
        (void)NwIosTrackTake(addr, &ignored);
    }
    (void)vm_deallocate(mach_task_self(), addr, rounded);
    /* Released space may satisfy an allocation that previously exhausted the
     * range scan. */
    g_nwIosExhausted = 0;
}

int NwIosProtectLow(void *p, size_t size, int prot)
{
    vm_size_t rounded;
    vm_address_t addr;

    if (p == NULL) {
        return -1;
    }
    addr = (vm_address_t)(uintptr_t)p;
    if (!NwIosRoundSize(size, &rounded) || size == 0) {
        if (!NwIosTrackTake(addr, &rounded)) {
            errno = EINVAL;
            return -1;
        }
        /* Put it back; protection changes must not consume the record. */
        NwIosTrackAdd(addr, rounded);
    }
    if (vm_protect(mach_task_self(), addr, rounded,
                   FALSE, NwIosVmProt(prot)) != KERN_SUCCESS) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

int NwIosHasLowAddressSpace(void)
{
    vm_address_t base = NW_IOS_LOW_BASE;
    vm_size_t rounded = NW_IOS_PAGE;
    kern_return_t kr = vm_allocate(mach_task_self(), &base, rounded,
                                   VM_FLAGS_FIXED);
    if (kr != KERN_SUCCESS) {
        return 0;
    }
    (void)vm_deallocate(mach_task_self(), base, rounded);
    return 1;
}

#else

#include <errno.h>
#include <stdint.h>

void *NwIosAllocLow(size_t size, int prot)
{
    (void)size;
    (void)prot;
    errno = ENOSYS;
    return NULL;
}

void *NwIosAllocLowAt(void *hint, size_t size, int prot)
{
    (void)hint;
    (void)size;
    (void)prot;
    errno = ENOSYS;
    return NULL;
}

void *NwIosAllocWindow(uintptr_t base, size_t size, int prot)
{
    (void)base;
    (void)size;
    (void)prot;
    errno = ENOSYS;
    return NULL;
}

void NwIosFreeLow(void *p, size_t size)
{
    (void)p;
    (void)size;
}

int NwIosProtectLow(void *p, size_t size, int prot)
{
    (void)p;
    (void)size;
    (void)prot;
    errno = ENOSYS;
    return -1;
}

int NwIosHasLowAddressSpace(void)
{
    return 0;
}

#endif
