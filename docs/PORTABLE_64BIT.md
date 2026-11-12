# Portable 64-bit guest address space

This note records why NetWalk needs a 64-bit guest-address model, how the
model works, how it was validated on Windows x86-64, and what remains for
iOS. It is the design and history behind `src/include/nwarch.h`,
`windows/win64_compat.c`, and the iOS memory backend.

## Why this exists

The recovered engine was written for a 32-bit Windows process. It stores
pointers in 32-bit fields all over the data window, the VM stack, save-file
records, and the custom heap. On a 32-bit build, a pointer *is* a 32-bit
value, so this is invisible.

Most 64-bit ports of the engine can still place every guest object below
4 GiB (Linux `MAP_32BIT`, Windows low `VirtualAlloc`, and an Android build
whose shared library happens to load below 4 GiB). iOS arm64 cannot. A stock
iOS executable reserves the whole
`0x0..0x100000000` range as `__PAGEZERO`, and the kernel does not hand out
sub-4 GiB mappings to a process that has that reservation. That was
confirmed on the target device during development.

Shrinking `__PAGEZERO` lets the probe map a few low pages on some
configurations, but it is fragile, and the game needs a multi-megabyte arena
for the framebuffer and scratch buffer. The durable fix is to stop requiring
low addresses at all: keep the *guest* address space 32-bit, but map it
through a 4 GiB-aligned *host* window above 4 GiB.

## The two address spaces

`src/include/nwarch.h` is the only seam:

```c
typedef uint32_t  nw_guest_addr_t;  /* VM / save / stream / window fields */
typedef uintptr_t nw_host_ptr_t;    /* real process pointer               */

extern uintptr_t nw_guest_base;
```

- A **guest address** is a 32-bit offset from `nw_guest_base`.
- A **host pointer** is `nw_guest_base + guest` for guest objects.
- When `nw_guest_base == 0` (the original i686 oracle, the low Linux/Windows
  control, and an Android build whose library loaded below 4 GiB), the two
  spaces are identical and both conversions are identities.

```c
static inline nw_host_ptr_t nw_guest_to_host(nw_guest_addr_t guest)
{
    return guest != 0u ? (nw_host_ptr_t)(nw_guest_base + guest) : 0;
}

static inline nw_guest_addr_t nw_host_to_guest(const void *host)
{
    return host != 0u ? (nw_guest_addr_t)((uintptr_t)host - nw_guest_base) : 0;
}
```

**Guest zero is NULL.** This is not an optimisation; it is load-bearing.
Before the null preservation was added, `nw_guest_to_host(0)` returned
`nw_guest_base`, so every linked-list terminator became a non-null pointer
and walks ran into the unmapped start of the window. The window reserves its
first 64 KiB and never allocates a live object at guest offset 0, so the
special case cannot misclassify a real object.

Because the base is 4 GiB-aligned, `(uint32_t)(uintptr_t)host` is always the
guest offset for an in-window host pointer. That is what makes the *store*
direction cheap: a site that writes `(uint32_t)p` into a window field needs no
change. Only the *load* direction needs `nw_guest_to_host()`.

## The single 4 GiB window

All guest-visible state lives in one 4 GiB-aligned region:

```
host address
  0x100000000  +-----------------------------+  guest 0x00000000
               | 64 KiB reserved (guest 0)   |
  0x100010000  +-----------------------------+
               | image / static window /     |
               | side table / guest heaps /  |
               | framebuffer / scratch       |
  0x200000000  +-----------------------------+  guest 0x100000000 (end)
```

`nw_guest_base` is derived at startup from the data window itself:

```c
nw_guest_base = (uintptr_t)nw_data_window & ~(uintptr_t)0xFFFFFFFFu;
```

So the static window (and the pointer side table linked into the same image)
defines the window. Everything else — the main heap arena, the SDL
framebuffer, the scratch buffer that becomes the engine heap base, and the VM
native-call stacks — is allocated by scanning for free ranges **inside** that
window. Guest 0..0xFFFF is never handed out.

On Windows x86-64 the shipping image is linked at `0x400000`, so the guest
window starts at the low image base. Setting `NW_FORCE_HIGH=1` places the
arena above 4 GiB instead and exercises the same high-window path on that one
binary. The guest-window allocator starts scanning at `base + 0x10000` (the
guest-zero reserve) and skips reserved regions with `VirtualQuery`.

On Android the shared library may be loaded above 4 GiB by the dynamic
linker, so `nw_guest_base` is non-zero even though the build is not a
high-image link. `platforms/linux/linux_compat.c` therefore routes every
`VirtualAlloc(NULL, ...)` through the window when the base is non-zero
(`nw_mmap_window`): it starts at `base + 0x10000`, advances a page at a time,
and uses `MAP_FIXED_NOREPLACE` so the game, SDL, and system-library mappings
inside the window are skipped rather than replaced. A monotonic bump cursor
keeps the common case to one mapping attempt and the scan wraps once to reuse
freed holes. When the base is zero the low `nw_mmap_low` path is used
unchanged.

## Native pointer side table

`nw_data_window` keeps its recovered byte layout. Fields that hold pointers
are 4 bytes wide in the window; on 64-bit builds the real pointer lives in a
parallel side table:

```c
nw_host_ptr_t g_nw_window_ptr_slots[NW_WINDOW_BYTES / 4u];

#define NW_WINDOW_PTR_REF(type, off) \
    (*(type **)(void *)&g_nw_window_ptr_slots[(off) >> 2])
#define NW_WINDOW_PTR_VALUE(off) (g_nw_window_ptr_slots[(off) >> 2])
```

`tools/nw_portableize.py` generates `nw_window_data.c`, which:

- emits the 0x6820-byte `nw_data_window` snapshot;
- emits a fixup table for every `.long` in the snapshot that names a symbol;
- initialises `g_nw_window_ptr_slots` with the full host address of each
  symbol and writes its low half into the window;
- computes `nw_guest_base`;
- declares the global access macros in `nw_portable_globals.h`.

Rules that follow from this:

- Pointer-sized window fields must be accessed through
  `NW_WINDOW_PTR_REF`/`NW_WINDOW_PTR_VALUE`, not through a raw `uint32_t *`.
  The side-table entry is the real host pointer; the low window word is only
  a guest mirror.
- Side-table entries hold **native host pointers**. A consumer that needs a
  guest value must truncate with `(uint32_t)(uintptr_t)` or call
  `nw_host_to_guest()`.
- The one deliberate exception is `PUB_SCALED_CELL_BUF` /
  `g_dwRecordArrayBase` at window `+0x630c`, which stays a guest value because
  the VM and the save/stream code treat it as an offset.
- Aliases are a recurring trap. Several names map to the same side-table
  slot, e.g. `g_pSlotsTop`/`g_pGridSpanChainTop` at `+0x6240`,
  `g_dwHeapRegionEndSentinel`/`g_pSlotsBottom` at `+0x6244`,
  `g_pHeapFirstNode`/`g_pHeapBlockList` at `+0x2c10`,
  `g_pCurGridCtx`/`g_vmContextC` at `+0x6218`,
  `g_vmContextD`/`NW_CTX_D` at `+0x6214`,
  `g_pVmBaseB`/`g_vmContextA` at `+0x63e8`, and
  `g_pRectListHead`/`g_display_object` at `+0x11b0`.
  A file that declares one alias as a 32-bit scalar silently truncates the
  shared 64-bit slot.

## Conversion rules

The model is mechanical once the two spaces are clear:

1. **Guest store**: writing a guest address into a stream record, save
   buffer, VM stack slot, or window mirror needs no change. Widening to
   `uintptr_t` and truncating to `uint32_t` at the store is enough because
   the base is 4 GiB-aligned.
2. **Guest load used as an address**: wrap it:
   `(T *)(uintptr_t)nw_guest_to_host(guest)`.
3. **Side-table slot used as an address**: it is already a host pointer;
   cast through `uintptr_t` and dereference directly. Do not pass it through
   `nw_guest_to_host` unless you know it is a guest value.
4. **Pointer through the VM**: `vm.eax`, `vm.edx`, `vm.ecx` and values
   popped from the VM stack are guest values and need conversion when used
   as addresses. `vm.esi`, `vm.edi`, `vm.ebx`, and `vm.sp` are host
   pointers.
5. **Function pointers** live in side-table slots as native host addresses.
   Their low window words are not callable on a 64-bit host.
6. **Width discipline**: every variable that holds an address must be
   `uintptr_t`, a pointer type, or `size_t`. `unsigned long` is 32-bit on
   Windows (LLP64) and `int`/`int32_t`/`DWORD` are always wrong for an
   address.

A quick audit for a side-table alias is to parse `build/window_globals.args`
for all names mapped to `_g_nw_window_ptr_slots+0x...`, then check every
`extern` declaration of those names. Any declaration with a scalar type is a
latent truncation.

## Validation harness on Windows

The shipping Windows build is the low control: image base `0x400000` and
`nw_guest_base == 0`. To reproduce the high-window layout on the same binary,
run it with `NW_FORCE_HIGH=1`, which moves the guest arena above 4 GiB. That
exercises `nw_guest_to_host`, the side table, and the window scan the same way
iOS does, while a normal run remains the behavioural oracle.

### Reading a crash

The Win32 crash handler writes `netwalk_crash.log` beside the executable.
The useful line for this work is the ASCII one:

```
fault64 eaddr=0x... inf1=0x... eip=0x... esp=0x... eax=... ...
```

- `eip` is the faulting instruction. Resolve it against the unstripped
  binary with `x86_64-w64-mingw32-nm -n` and `x86_64-w64-mingw32-objdump -d`.
- `inf1` is the faulting data address. If it is a small value such as
  `0x8880d` or `0x97858`, a guest offset was dereferenced without
  conversion. If it is `0x100000000` or another base-aligned value, a guest
  zero was treated as a real pointer.
- `eaddr`/`inf1` differ on execute faults; `eip` is still the instruction.

The layout instrumentation in `windows/win64_compat.c` writes
`nw_layout.log` only when `NW_LAYOUT_LOG` is set; it is silent by default.

## Fix log

The model was exercised in rounds on Windows and iOS; each round fixed one
crash class.

| Round | Symptom | Root cause | Fix |
|---|---|---|---|
| 1–4 | Instrumentation and scan bring-up | — | Window scan, allocator routing, layout dumps, force-high build |
| 5 | Fault in `DecodeTaggedRecordLengthToOutParam` reading `0x40309fe4` | Bias tables reached through `(unsigned long)` casts, 32-bit on Windows | `uintptr_t` casts; guest-pointer subops converted |
| 6 | Fault in `AddRectToExtentList` and width truncations through window aliases | Aliases of shared slots declared as 32-bit scalars (`g_pSlotsTop/Bottom`, `g_pHeapBlockList`, `g_vmContextA/D`, stream counters) | Pointer-width declarations and conversions |
| 7 | List terminators dereferenced at `0x100000000` | `nw_guest_to_host(0)` returned `nw_guest_base`, so guest zero became a non-null pointer | Null-preserving `nw_guest_to_host`/`nw_host_to_guest` |
| 8 | Fault in `InitializeAppSubsystemsFromSavedPath` at `0x8880d` | Window slot `0x6378` (`g_pVmFrameCtlRecord`) truncated to `uint32_t` before dereference | Pointer-width local and full-width dereference |
| 9 | Fault in `DecodeGridSpanIndexStream` at `0x97858` | VM-native span wrappers took a guest address in a `const uint8_t *` parameter | Widened all four span-run wrappers; converted `NwGridCtxReadLayer`, `NwStringContainsLiveInput`, `NwInputFieldContainsArg` |
| 10 | iOS portrait launch showed a cropped image with a large right-hand black bar, randomly per launch | `main()` set `SDL_HINT_ORIENTATIONS` to landscape after UIKit had already selected the launch orientation from `Info.plist`; SDL then swapped the window frame to landscape inside a portrait `UIWindow` | Removed the hint, allow both orientations, add `UIRequiresFullScreen`, and letterbox the fixed 4:3 canvas through the real window size |
| 11 | iOS first gameplay frame crashed with `far=0x373d158`, `pc` inside `CopyTextRowsUnrolledSkipNulToDisplayBuffer` | `CopySpriteRowsToDisplayBuffer` passed the sprite base pointer through an `int nSrcBase` argument; the 32-bit guest offset was then dereferenced as a host address. The Windows high proxy runs at render scale 4 and used the scaled blitter, so it never reached this scale-1 row-copy path | Treat `nSrcBase` as a guest address and convert with `nw_guest_to_host` in all four row-copy variants |
| — | Normal gameplay, no crash | — | Round 9 build; high proxy verified through level start, movement, firing, menu, save/load. Round 11 also rebuilds the iOS scale-1 path |

The low `netwalk.exe` control continued to run throughout.

## iOS wiring

iOS uses the same model, but the memory backend is different:

- `cmake/ios.cmake` builds the game as a normal iOS `.app` with the **linker
  default** `__PAGEZERO` (4 GiB), so the Mach-O image loads at
  `0x100000000`. That makes `nw_guest_base` non-zero and puts the static
  window and side table inside the guest window.
- `platforms/ios/ios_memory.c` gains a window allocator. On a non-zero
  `nw_guest_base`, `VirtualAlloc(NULL, ...)` and `NwHostAllocLow()` scan
  `[base + 0x10000, base + 0x100000000)` with `vm_allocate(VM_FLAGS_FIXED)`
  and `vm_region_64()`, skipping the image and the dyld shared cache, then
  protect and track the result the same way the old low allocator did.
- When `nw_guest_base == 0` (an explicitly low link), the old sub-4 GiB
  `vm_allocate` path is still available, but the shipped game always uses the
  window above 4 GiB.
- The game keeps the linker-default `__PAGEZERO`; the build has no size
  override and does not depend on shrinking it.

Device diagnostics stay quiet by default:

- The game writes no run log or crash file on device. The iOS entry point
  reports startup errors such as a missing bundled resource through
  `SDL_Log`, which reaches the unified log, but the engine has no per-stage
  trace, heartbeat, or fatal-signal handler. Use Xcode or the system crash
  report when a device fault needs investigation.
- `UIFileSharingEnabled` and `LSSupportsOpeningDocumentsInPlace` remain set
  so the Documents folder is reachable in the Files app; the game stores its
  saves in the preference directory as before.
- The shared `platform_boot_mark`/`platform_log_mark` stage hooks are compiled
  out for iOS, so the portable diagnostic paths cost nothing on device.

The remaining risk on device is not the conversion itself but which SDK
functions the allocator is allowed to call. `vm_allocate`, `vm_deallocate`,
`vm_protect`, `vm_region_64`, and `mach_port_deallocate` are all public in
the iPhoneOS SDK; `mach_vm.h` is only a stub there.

## Building and testing iOS

The iOS target is opt-in and runs only inside the build container:

```sh
# Game IPA (ad-hoc signed for a development device)
NW_IOS_SDK=/path/to/Xcode.xip docker/build-releases.sh \
    --no-build-image --ios --ios-sign adhoc
```

Outputs land in `dist/docker-releases/ios/`:

- `NetWalk.ipa` — the game
- `NetWalk.app` — the staged bundle

Install the IPA with the usual development tooling (Xcode devices window,
`ios-deploy`, or a sideloading tool). An ad-hoc signature will not satisfy a
stock, non-jailbroken device; that path needs `--ios-sign apple` with a
certificate and provisioning profile.

## Checklist for the next address-space change

1. Find every `extern` declaration of a side-table name and make it
   pointer-width.
2. Search for `(unsigned long)`, `(int)`, `(uint32_t)`, `(DWORD)` applied to a
   pointer, and for `long`/`int` locals that hold addresses.
3. Search for `*` loaded from a 32-bit field and used without
   `nw_guest_to_host`.
4. Check VM operands: everything the VM pushes is a guest value.
5. Re-run the Windows build with `NW_FORCE_HIGH=1` for the address model and
   without it for behaviour, and check the iOS build on a device.
6. Resolve any fault with the `fault64` line before guessing.
