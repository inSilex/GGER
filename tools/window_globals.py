#!/usr/bin/env python3
"""Publish recovered data globals INTO the relocated data window.

Why this exists
---------------
The corpus declares many recovered globals with a plain `extern` declaration and
documents their original address in a trailing comment, e.g.

    extern uint32_t g_dwHeapCapacityWords;   /* @0x412610 (+8) - extent in words */

Nothing defines those names, so `tools/gen_stubs.py` (driven by the prelink
error log) emits a *private* definition for each one -- a 4/8-byte object in .bss.
That silently splits one machine-level global into two storages:

    * verbatim `__asm__` blobs and every `nw_data_window + 0xNNNN` expression
      touch the real relocated window (initial values copied from the original
      data block, shared with the C reconstructors that use the MEM() macro);
    * the `extern`-style C reconstructors touch a zero-initialised .bss mirror.

Anything that crosses that boundary breaks.  The concrete failure this was
written for: `PublishHeapStateTripleFromTwoSources` published the heap capacity
into its .bss mirror while `AllocateHeapCursorBlock` read `g_dwCursorWords`
(window + 0x1610), so the heap was permanently 0 words and every arena
allocation returned the out-of-memory sentinel -- `g_pArenaCursor` stayed NULL
and the opcode dispatcher dereferenced it.

How the fix works
-----------------
Recovered globals whose original address is known are published as *link-time
absolute aliases* into the window:

    -Wl,--defsym,_g_dwHeapCapacityWords=nw_data_window+0x1610

The alias is resolved by the linker against the final address of
`nw_data_window`, so it survives any layout shift and needs no source edit, no
type change and no per-file macro.  `nw_data_window` lands at a fixed VMA
(0x418030) because the image base is fixed, exactly like the original binary's
own baked absolute addresses.

Every name listed here is also removed from `build/stubs.c` (otherwise the
private stub and the alias would be two definitions of one symbol, which is a
hard link error).

Addresses are original-image VAs; the window base is the original 0x411000.
Only symbols whose original address is *explicitly documented* in the corpus
declaration are listed.  Group entries share one storage slot because the
original had one storage slot.

Usage (from CMake):
    python3 tools/window_globals.py emit          # -> build/window_globals.args
    python3 tools/window_globals.py filter build/stubs.c
"""
import re
import sys
import os

# --------------------------------------------------------------------------
# Curated map: corpus C name -> original VA (0x411000-based data window).
# Grouped by the machine-level slot they belong to.
# --------------------------------------------------------------------------
WINDOW_BASE = 0x411000

GROUPS = {
    # --- heap state triple @0x412608..0x412613 (head / cursor / capacity) -----
    "0x00412608": ["g_pHeapFirstNode", "g_pHeapBlockList", "g_dwHeapListHead"],
    "0x0041260c": ["g_pCursorBlock", "g_pHeapSlotBase"],
    "0x00412610": ["g_dwHeapCapacityWords", "g_dwHeapRegionSizeInWords"],
    # --- heap descriptor pair @0x417314 (byte budget) / 0x417318 (fail result) -
    "0x00417314": ["g_dwFramebufferArenaBytes"],
    "0x00417318": ["g_dwAllocFailResult", "g_dwHeapCursorSource"],
    # --- bump allocator hand-off @0x412614 ------------------------------------
    "0x00412614": ["g_pGlobalBuffer"],
    # --- grid span chain bounds @0x417240 (top) / 0x417244 (region end) -------
    "0x00417240": ["g_pSlotsTop", "g_SlotChainTopPtr"],
    "0x00417244": ["g_pSlotsBottom", "g_dwHeapRegionEndSentinel", "g_SlotChainBottomPtr"],
    # --- span record region @0x417304 .. 0x417318 -----------------------------
    "0x00417304": ["g_SlotRegionBase", "g_dwBufferCount"],
    "0x0041730c": ["g_RecordArrayBase"],
    "0x00417310": ["g_buffer_block"],
    # --- record stream position mirrors @0x4126b4/0x4126c8/0x4126d4 -----------
    "0x004126b4": ["g_dwStreamRemainingBytes", "g_nBufferedBytesRemaining"],
    "0x004126c8": ["g_dwStreamRecordByteCount"],
    "0x004126d4": ["g_pStreamRecordCursor"],
    # --- misc recovered pointers ---------------------------------------------
    "0x00412874": ["g_pGridSpanStack"],
    "0x004121b4": ["g_pActiveSlot"],
    # --- read-only tables that live in the INITIALISED half of the window ------
    # The 13-entry cumulative-days table is image data at 0x411cac (window +0xcac),
    # read as `MOVZX ...,[ECX*2+0x411cac]`, so the bytes are already in
    # nw_datablock.bin: 0,31,59,90,120,151,181,212,243,273,304,334,365 (non-leap).
    # 0x411a4d is the same table under a third corpus spelling; all three names
    # are read by the time-zone name parser (original refs at 0x40f730/0x40f7bc/
    # 0x40f870 all name 0x411a4d).  Was a duplicate GROUPS KEY, which silently
    # kept only the last list -- so g_charClass/g_aucCharClass stayed .bss.
    "0x00411a4d": ["g_charClass", "g_aucCharClass", "g_charClassTable"],
    # 0x411cae (GetLastSundayDayOfYear) is +2 into the same cumulative table.
    "0x00411cac": ["wMonthStartDayOfYear", "g_days_before_month"],
    # --- MIDI-out handle: ONE slot, two names in the corpus --------------------
    # Writers (InitializeMidiOutAndTimerResolution, DispatchMidiSequencerEvents)
    # spell it g_hMidiOut; readers (ResetMidiOutAndTimerEvent,
    # ShutdownAudioSubsystem) spell it hMidiOutDevice.  Both declaration comments
    # give 0x412de0, and the original has a single slot there - so both names must
    # resolve to it or the writer and the reader talk to different storage.
    "0x00412de0": ["g_hMidiOut", "hMidiOutDevice"],
    # --- slots the corpus names TWICE (one original slot, two reconstructors) ---
    # Each of these has two or more recovered names whose DECLARATION COMMENTS give
    # the SAME original address.  Independent agreement on the address is what makes
    # them safe to publish: they are one machine-level slot, so the two spellings
    # must not end up in two storages (the writer would publish into .bss while the
    # verbatim blobs read the window).
    "0x004177f0": ["g_pfnGlobalHook"],
    # NOT a slot alias: g_pfnCmdDispatch3 was a function-SHAPED name wrongly published
    # on the +0x5f00 data slot (g_hdcMem/g_paletteDC); every use of it has been
    # replaced by the real API calls (SelectPalette/RealizePalette), see
    # src/input/win32/ProcessMainWindowMessages.c.
    "0x00416f00": ["g_paletteDC", "g_hdcMem", "g_dwDisplayDc"],
    # original PlayStartupMovieWithMessagePumpAndTeardown pushes 0x416f00 (as the
    # object address) to import *[0x401af4]; the corpus names that slot
    # g_dwDisplayDc, so publish the third spelling.
    "0x00416f04": ["g_00416f04", "g_palette", "g_hpalPalette"],
    "0x00416f14": ["g_pcFatalMessage", "g_pending_error_string"],
    "0x00416f20": ["g_clientWidth", "g_dwRequiredMin0", "g_cxGridClient"],
    "0x00416f24": ["g_clientHeight", "g_dwRequiredMin1", "g_cyGridClient"],
    "0x00417064": ["g_00417064", "g_hdcSrcMem", "g_hdcDib"],
    "0x004171e8": ["g_RecordStrideBytes", "gdwGridCellWidth"],
    "0x004171ec": ["g_RecordCountLimit", "gdwGridRowCount"],
    "0x00417214": ["g_vmState_0x417214", "g_pAltGridCtx", "g_vmContextD"],
    "0x0041721c": ["g_vmState_0x41721c", "g_wGridUserData", "g_vmContextB"],
    "0x00417250": ["g_BulkCopyBasePtr", "g_pCellEntryBase", "gpSourceCellTable",
                   "gpSourceCellWordBase", "gpTileCellTable"],
    "0x00417254": ["g_BulkCopyShiftAmount", "g_bCellIndexShift"],
    "0x00417300": ["DAT_00417300", "g_pGridRefTab", "g_pRefSelTable"],
    "0x00417345": ["g_bPlayRequestFlag",
                   "bVmAudioStateByte"],
    "0x004173d4": ["gpDisplayCellByteBase", "gpGridBase", "gpbGridCellArray"],
    "0x004173e8": ["g_base_ptr_b", "g_vmState_0x4173e8", "g_vmContextA"],
    "0x00417404": ["g_00417404", "g_hwndMain", "g_wGlobalWord_00417404"],
    # ---------------------------------------------------------------------------
    # Split slots found by the binary-side detector (see HANDOFF): every name below
    # is a .bss stub AND our built image contains a real `window+0xNNNN` operand at
    # the offset its own declaration comment documents.  That combination is the
    # proof: our code reads the window while this name's storage sits elsewhere.
    # Aligning the display geometry (0x412cc8..0x412cd0), the wave ring/handles
    # (0x412cd8/0x412e28/0x412ea8) and the MIDI/vm-state pair fixed the
    # OpenWaveOutAndPrepareHeaderRing fault at 0x40fd76.
    # ---------------------------------------------------------------------------
    "0x004111c8": ["dwRngState"],
    # (0x00411a4d published above; this section used to repeat the key, which
    #  silently dropped the two non-last spellings.)
    "0x004121b0": ["g_pRectListHead"],
    "0x00412c94": ["gdwLocatedCellX", "g_dwCoordHEnd"],
    "0x00412c98": ["gdwLocatedCellY", "g_dwCoordVEnd"],
    "0x00412cc8": ["g_DisplayBase"],
    "0x00412ccc": ["g_ScreenPitch"],
    "0x00412cd0": ["g_ScreenRows"],
    "0x00412cd8": ["g_hWaveOut", "g_wave_out_handle"],
    "0x00412e28": ["g_aAudioSlots"],
    "0x00412ea8": ["g_abWaveOutPool"],
    "0x00416eb4": ["g_szAppPath"],
    "0x00416ef4": ["g_pTileImageRecord"],
    # Scratch-buffer pair: CreatePlayerDIBBackBuffer.c uses MEM-style
    # `nw_data_window + 0x5f08 / 0x5f0c` defines for the GlobalAlloc handle and the
    # GlobalLock result (orig stores at 0x40c122/0x40c12e), while
    # ShutdownAndReleaseResources.c frees the global buffer and clears its flag
    # spellings; those stayed .bss mirrors, so the original 0x40ae86 GlobalFree path
    # freed nothing.  Bind both spellings to the one slot pair.
    "0x00416f08": ["g_00416f08", "g_hGlobalMemory"],
    "0x00416f0c": ["g_00416f0c", "g_bGlobalMemoryActive"],
    "0x00416f10": ["g_dwShowCommand"],
    "0x00416f28": ["g_minSizeX"],
    "0x00416f2c": ["g_minSizeY"],
    "0x00416f30": ["g_dwCurrent0"],
    # 0x416f34 is SM_CYSCREEN, stored by CreatePlayerMainWindow (orig 0x40bea5) and
    # read by TryPlayStartupMovieWithWindowFallback (orig 0x40bde1).  It was NOT the
    # main-window handle: /tmp/opencode/orig.asm has `a3 34 6f 41 00` at 0x40bea5
    # right after the GSM(1) call.  Aliasing it to 0x417404 made the screen-height
    # guard compare the HWND against the client height.
    "0x00416f34": ["g_dwCurrent1"],
    "0x00416f38": ["g_windowOffsetX"],
    "0x00416f3c": ["g_windowOffsetY"],
    "0x00416f5c": ["g_pszHelpFilePath"],
    "0x00417078": ["g_szMovieBaseName"],
    "0x004171f8": ["g_dwMinBufferLen"],
    "0x00417218": ["g_vmState_0x417218", "g_vmContextC"],
    "0x00417220": ["g_dwAxisModMinor", "g_vmOperandB"],
    "0x00417224": ["g_dwAxisModMajor", "g_vmOperandA"],
    "0x00417248": ["g_pStateTable"],
    "0x0041724c": ["g_pPoolTable"],
    "0x0041726c": ["g_dwScaleX"],
    "0x00417270": ["g_dwScaleY"],
    "0x00417340": ["g_bModeEnabled"],
    "0x00417342": ["g_bOpZeroOrOne"],
    "0x00417343": ["g_bModeSelector"],
    "0x00417344": ["g_bPlaybackArmed", "g_bMciPlaybackEnabled"],
    "0x0041737c": ["g_pSaveRegionDescPrimary"],
    "0x00417380": ["g_pSaveRegionDescSecondary"],
    "0x00417390": ["g_szRuntimeSourcePath"],
    "0x004173d8": ["gdwGridRightLimit"],
    "0x004173dc": ["gdwGridUpOffset"],
    "0x004173e0": ["gdwGridDownLimit"],
    "0x004173e4": ["g_base_ptr_a"],
    "0x004177f6": ["g_vmState_0x4177f6", "g_vmModeFlag"],
    # Gate A of UpdateMciAudioStateAndPlay (orig 0x4090d0: cmp BYTE PTR
    # ds:0x4177f8,0).  The corpus name guessed the VM cluster 0x417214; the
    # instruction is the authority.
    "0x004177f8": ["g_bMciAudioBusy"],
    "0x004177fa": ["g_bModeParam"],
    "0x004177fb": ["g_bModeAltParam"],
    "0x00417806": ["g_bConfigForMode"],
    "0x00417807": ["g_bConfigForAltMode"],
    # ---------------------------------------------------------------------------
    # Systematic sweep: every name that the corpus binds to a window address on
    # its own declaration line and that our build still kept as a 4-byte .bss
    # stub (names bound by "*nearest* address on the line", ambiguous names with
    # several candidate addresses excluded, one name per address verified).
    # These are the "one machine-level global, two storages" cases that make the
    # WINDOW the writer and the .bss mirror the reader (or vice versa).  The
    # pathological members are the ARRAY globals, because gen_stubs.py emits a
    # 4-byte object for them: g_apfnUtNotify (12 pointers, 48 bytes),
    # g_wave_hdr_table (8 x 32-byte WAVEHDRs), g_opcharClassTable and
    # g_opcode_dispatch_table -- indexing one of those reads into whichever stubs
    # happen to follow.  g_apfnUtNotify is the reason the startup path called a
    # trap stub: `g_apfnUtNotify[4]` loaded the storage of an unrelated global.
    # ---------------------------------------------------------------------------
    "0x00411074": ["DAT_00411074", "g_ActiveGridCellRecord"],
    "0x00411114": ["g_apPoolHandlers"],
    "0x00411178": ["g_apOpcodeHandlers"],
    "0x004111cc": ["g_szOutOfMemoryMessage"],
    "0x004112b8": ["g_abSaveFileHeaderMagic"],
    "0x004112c0": ["g_szSaveVariantSuffix"],
    "0x00411530": ["g_opcode_dispatch_table"],
    "0x00411610": ["g_aGridDeltaDispatch"],
    "0x00411620": ["g_pfnNotifyMixedWaveBuffer", "g_apfnUtNotify"],
    "0x00411638": ["g_pfnSelectWaveBufferIndex"],
    "0x004118d2": ["g_szHlpExtension"],
    "0x004118e8": ["g_szImageFileError"],
    "0x0041190f": ["g_abFailureMarker_0041190F", "g_failureMarker"],
    "0x00411eb4": ["g_aWin32ErrorToErrno"],
    "0x00411f50": ["g_nStandardBiasSeconds"],
    "0x00411f54": ["g_nDaylightBiasSeconds"],
    "0x00411f58": ["g_bDaylightPresent"],
    # InitializeTimeZoneStandardAndDaylightState writes these two 12-byte zone
    # name buffers; the reader side (nwglobals.h g_timezone/g_daylight) proves
    # the slot mapping.  They were tentative definitions in the writer's .bss.
    "0x00411f5c": ["g_szStandardName"],
    "0x00411f68": ["g_szDaylightName"],
    "0x00412200": ["g_bGridDirty"],
    "0x0041727e": ["g_bGridPenIndex"],
    "0x00412c8c": ["g_iCoordHEnd", "g_iCoordHStart", "g_dwCoordHStart"],
    "0x00412c90": ["g_iCoordVEnd", "g_iCoordVStart", "g_dwCoordVStart"],
    "0x00412c9c": ["g_dwOrientCodeA"],
    "0x00412ca0": ["g_dwOrientCodeB"],
    "0x00412ca4": ["szField32"],
    "0x00412cc4": ["cFieldTerminator"],
    "0x00412cdc": ["g_uWaveOutDeviceId"],
    "0x00412ce0": ["g_wave_hdr_table"],
    "0x00412de4": ["g_uMidiDevId"],
    "0x00412de8": ["dwTimerEventId"],
    "0x00412dec": ["g_dwMidiClockTarget"],
    "0x00412df4": ["g_dwMidiEndMarker"],
    "0x00412df8": ["g_dwMidiSavedCount"],
    "0x00412dfc": ["g_pMidiSavedCursor"],
    "0x00412e00": ["g_pMidiTrackCursor"],
    "0x00412e04": ["g_bMidiRunningStatus"],
    "0x00412e05": ["g_bMidiTimerActive"],
    "0x00412e08": ["g_uTimerPeriodMs", "uTimerPeriodMs"],
    # UT driver slot pairs: the corpus spelled the same four slots two ways and the
    # DAT_* spellings stayed private .bss mirrors, so InitializeUtDriverAndHandlerTable
    # (orig 0x40a684/0x40a7bc) filled the WINDOW slots while ShutdownUtDriverAndFreeResources
    # (orig 0x40a534: cmp/call ds:0x412e10, ds:0x412e18, ds:0x412e1c, ds:0x412e0c) read the
    # mirrors -- the teardown therefore never saw the handles and never closed the device.
    "0x00412e0c": ["g_pcbUtBuffer", "DAT_00412e0c"],
    "0x00412e10": ["g_pfnUtHook", "DAT_00412e10"],
    "0x00412e14": ["g_pfnUtRegister"],
    "0x00412e18": ["g_pfnUtUnReg", "DAT_00412e18"],
    "0x00412e1c": ["g_hUtKernel", "DAT_00412e1c"],
    "0x00416eb0": ["g_dwAudioSlotsMixed"],
    "0x004170b8": ["g_dwMciGlobalHandle", "g_mciDeviceId",
                   "MciDeviceGlobalArmed"],
    "0x004170c0": ["g_suspendRequested"],
    "0x004170c1": ["g_closeRequested"],
    "0x004170c2": ["g_bTileMediaFlag", "g_blitModeByte"],
    "0x004170c4": ["g_playfieldActive"],
    "0x004170c5": ["g_mouseBusyFlag"],
    "0x004170c6": ["g_childGeometryFixed"],
    "0x00417230": ["g_pfnCellOperandHook"],
    "0x0041727d": ["DAT_0041727d", "g_bActiveGridInstallGuard"],
    # Split storage found from the original bodies: InstallActiveGridCellRef (orig
    # 0x4034ab) stores the descriptor index into [0x417368] and
    # InitializeDispatchRecords (orig 0x406473) loads it back from the same slot.  The
    # corpus spelled the store with DAT_00417368 (a private .bss mirror) while the load
    # used the window, so the reader always saw 0.
    "0x00417368": ["DAT_00417368", "g_dwActiveGridDescriptorIndex"],
    "0x0041732c": ["g_abSaveFileHeaderRecord"],
    "0x00417408": ["g_pfnScanFloat"],
    "0x004177f4": ["g_last_msec_field"],
    "0x00417804": ["g_bOrientChanged"],
    # ---------------------------------------------------------------------------
    # Names whose window address is documented ONLY in a body/comment (no address
    # on the declaration line, so the sweep above cannot see them).  Found by
    # comparing the ORIGINAL body against our rebuilt body:
    #   src/app/SetFrameIntervalFromDivisor.c (orig 0x40c148) is MOV [0x00416f18],EAX,
    #   but the C form declares `extern DWORD g_dwFrameIntervalMs;` with a TODO
    #   admitting the address was unknown -- gen_stubs.py made it a .bss DWORD, so
    #   the writer stored the frame budget in .bss while RunMainMessageLoop (the
    #   verbatim blob at 0x40ca54) throttles on the WINDOW slot +0x5f18, which
    #   therefore stayed 0.  Result: one frame drawn, then a 100%-CPU spin whose
    #   `timeGetTime() <= [0x4170c8] + 0` test never lets the redraw path run.
    # ---------------------------------------------------------------------------
    "0x00416f18": ["g_dwFrameIntervalMs"],
    # --- .bss mirrors whose original address is documented in the declaring
    # file (or proven from the original body) but which the earlier sweep did
    # not see.  The first one crashed the frame path: our StepByteTowardTarget
    # read its divisor from a zeroed .bss stub (0x43837c) instead of window
    # +0x627b, so the IDIV raised STATUS_INTEGER_DIVIDE_BY_ZERO.
    "0x00411698": ["g_bmiGlobal"],
    "0x00411950": ["g_rgcrGridPenColors"],
    "0x004126b0": ["g_dwStreamBaseBytes"],
    "0x004126b8": ["g_pStreamSourceBuffer"],
    # 0x412c8c/0x412c90 published above (were duplicate keys here).
    "0x004171f4": ["g_nGridMaxY"],
    "0x00417260": ["g_dwStreamOriginValue"],
    "0x0041727b": ["g_frameStepDivisor"],
    # --- handle / string slots read by shutdown, save-key and grid code -------
    # Each verified against the original body that consumes the same name:
    #   0x412190  FindTableValueByKeyA (0x41008c)  `push 0x412190; call 0x40ffc4`
    #             -> the key string itself, read by the lookup helper.
    #   0x41705c  ShutdownAndReleaseResources (0x40ae71..): `cmpl $0,0x41705c`
    #             then `push 0x41705c; call *[0x401b14]` -> a guarded GDI handle.
    #   0x417400  ShutdownUtDriverAndFreeResources (0x40a555): the object address
    #             passed to the registered UT unregister function *[0x412e18].
    "0x00412190": ["g_szTableKeyA"],
    "0x0041705c": ["g_0041705c", "g_hShutdownGdiObject"],
    "0x00417400": ["DAT_00417400", "g_pUtRegistration"],
    # --- read-only/template tables read by leaves -----------------------------
    #   0x411298  ApplyRollingKeyXorToBuffer (orig 0x406054) indexes the 32-byte
    #             rolling key table.
    #   0x4118d8  FindMovieFileAcrossDrivesAndEmitBmpAviPaths (orig 0x40b2f8)
    #             rewrites byte 0 of the mutable "?:\" root template; sibling
    #             constants .BMP/.AVI/"\" follow at +4/+9/+0xe.
    #   0x41130c  the 62-entry VM native-call table, read by
    #             DecodeGridSpanIndexStream exactly as the original does at
    #             0x407acb `call *0x41130c(,%ecx,4)`.  Binding it makes our
    #             relocated window the single source of truth; NOTE the 13
    #             entries that point into the interiors of the multi-entry
    #             dispatcher slab (0x407664..0x408eac) are still emitted as
    #             `<owner>+delta` and are not yet faithful -- see HANDOFF.
    "0x00411298": ["g_abRollingXorKey"],
    "0x004118d8": ["g_szDriveRoot"],
    "0x0041130c": ["g_aVmNativeCallTargets"],
}

# Deliberately NOT published (kept as a trap stub until its address is proven):
#   ResolveArenaAnchorFromSlotCountOffset  - function-shaped name; the 0x417304
#                                            vote came from a context line, not a
#                                            declaration.
EXCLUDED = {"ResolveArenaAnchorFromSlotCountOffset"}

ARGS_PATH = "build/window_globals.args"

# Groups whose storage is a native pointer.  The list is a name-shape heuristic
# plus explicit exceptions; on a 64-bit target the alias must point at the
# side-table entry, not at the four-byte slot in nw_data_window.
_PTR_NAME = re.compile(
    r"^(g_p|g_ap|g_h|g_pfn|g_pp|g_base_ptr|gp|g_display_object|g_DisplayBase|"
    r"g_ActiveGridCellRecord|DAT_00417400|DAT_00417300)")


def is_pointer_group(names):
    return any(_PTR_NAME.match(n) for n in names)


def entries():
    """[(symbol, window_offset)] for every published name."""
    out = []
    for va, names in GROUPS.items():
        off = int(va, 16) - WINDOW_BASE
        assert off >= 0, va
        for n in names:
            if n in EXCLUDED:
                continue
            out.append((n, off))
    return out


def emit(side_table=False):
    args = []
    for va, names in GROUPS.items():
        off = int(va, 16) - WINDOW_BASE
        if side_table and is_pointer_group(names):
            expr = "_g_nw_window_ptr_slots+0x%x" % ((off >> 2) * 8)
        else:
            expr = "nw_data_window+0x%x" % off
        for n in names:
            if n in EXCLUDED:
                continue
            args.append("-Wl,--defsym,_%s=%s" % (n, expr))
            args.append("-Wl,--defsym,%s=%s" % (n, expr))
    os.makedirs("build", exist_ok=True)
    with open(ARGS_PATH, "w") as fh:
        fh.write("\n".join(args) + "\n")
    print("   window_globals: %d aliases -> %s" % (len(entries()), ARGS_PATH))


def filter_stubs(path):
    """Drop stub definitions for aliased symbols (they are defined by --defsym)."""
    names = {n for n, _ in entries()}
    rx = re.compile(r"^\s*(?:[A-Za-z_][A-Za-z0-9_]*[ \t]+)+\**"
                    r"([A-Za-z_][A-Za-z0-9_]*)(\[[0-9]+\])?(\s*=.*)?;\s*$")
    kept, dropped = [], []
    for line in open(path, encoding="latin1").read().splitlines():
        m = rx.match(line)
        if m and m.group(1) in names:
            dropped.append(m.group(1))
            continue
        kept.append(line)
    with open(path, "w") as fh:
        fh.write("\n".join(kept) + "\n")
    if dropped:
        print("   window_globals: removed %d data stubs: %s"
              % (len(dropped), " ".join(sorted(dropped))))
    others = sorted(names - set(dropped))
    if others:
        print("   window_globals: note - not stubbed in this build: %s"
              % " ".join(others))


def main():
    if len(sys.argv) < 2 or sys.argv[1] == "emit":
        emit("--ptr-side-table" in sys.argv[2:])
    elif sys.argv[1] == "filter":
        filter_stubs(sys.argv[2])
    elif sys.argv[1] == "list":
        for n, off in entries():
            print("%-36s window+0x%04x" % (n, off))
    else:
        sys.exit("usage: window_globals.py [emit|filter <stubs.c>|list]")


if __name__ == "__main__":
    main()
