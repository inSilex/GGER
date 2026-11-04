# Original-to-replacement function map

This is the address-ordered index from each original source unit to the
consolidated group file that now implements it.  The authoritative data is
`docs/reference/CONVERSION_MAP.tsv`; `docs/reference/FUNCTION_INDEX.tsv` provides the same index
in a direct original-file-to-implementation-file form.

* mapped entries: **361**
* files still carrying executable asm: **0**
* ratcheted C replacements: **49**
* executable asm blocks in the mapped files: **0**

The `unit selector` column is the preprocessor symbol used when the group
file is compiled as that unit.  One group file contains several independent
units, so the selector is the precise navigation key inside the file.

This is a historical recovery map.  Some diagnostic units listed here were
removed from the final SDL3 tree; `src/units.tsv` is the current build manifest.

## Replacement contracts worth preserving

| original | replacement | contract |
|---|---|---|
| `0x00405ab0` `DispatchPendingCommandLoop` | `src/app/AppLifecycle.c` (`NW_UNIT_DispatchPendingCommandLoop`) | `__stdcall`, `RET 4`; C callers use `_DispatchPendingCommandLoop`; the five window-table anchors `WAnch_00405cd3`/`d00`/`d12`/`d24`/`d78` remain defined in `.text` even though the C body switches directly. |
| `0x00406e48` `RetryLockedProbeDispatch` | `src/runtime/Hooks.c` (`NW_UNIT_RetryLockedProbeDispatch`) | `__stdcall`, undecorated symbol `_RetryLockedProbeDispatch`; the data-window table at `+0x2f8` still points at real `WAnch_00406e84`…`eb0` action functions. |
| `0x0040fb74` `AllocatePooledBlock` | `src/runtime/Pool.c` (`NW_UNIT_AllocatePooledBlock`) | cdecl, returns payload `block+4`; the slot lock uses C11 `__atomic_*` on the 16-bit counter at `+0x641a`. |
| `0x00402fb4` `ProcessQueuedTaskRecords` | `src/app/AppLifecycle.c` (`NW_UNIT_ProcessQueuedTaskRecords`) | defines only AL; callers must cast the result to `uint8_t` before comparing. |
| `0x0040fb20` `CallHookIfInstalled` | `src/runtime/Hooks.c` (`NW_UNIT_CallHookIfInstalled`) | documented deviation: returns 0 for a NULL slot, whereas the shipped asm left EAX untouched. |
| `0x00403d38` `FunAxisWrapClone` | `src/grid/GridCore.c` (`NW_UNIT_WrapGridAxesAndDispatch`) | observer slot is called through a cdecl-typed pointer even though the shipped slot is a `RET 4` no-op; the original does not adjust ESP after that call. |
| `0x0040f290` `InitializeGlobalThreadBufferOrDie` | `src/runtime/Heap.c` (`NW_UNIT_InitializeGlobalThreadBufferOrDie`) | returns the allocated buffer base in EAX, matching the original return value. |
| `0x0040ed04` `DispatchExitHooksAndClearShutdownFlags` | `src/app/AppLifecycle.c` (`NW_UNIT_DispatchExitHooksAndClearShutdownFlags`) | bare symbol (no leading underscore) because the asm exit path calls it by that name. |
| `0x0040f52c` `BuildQuotedCommandImage` | `src/app/win32/AppShutdown.c` (`NW_UNIT_BuildQuotedCommandImage`) | preserves the shipped store-through-the-allocation-result on the OOM path rather than silently converting it to a guard. |
| `0x004101b4` `BuildTokenPointerArrayFromGlobalString` | `src/runtime/RuntimeSupport.c` (`NW_UNIT_BuildTokenPointerArrayFromGlobalString`) | on a token-copy failure returns -1 with the partially populated array still installed. |

## Mapping

| original VA | original symbol | replacement file | unit selector | state | blocks | module |
|---|---|---|---|---|---:|---|
| `0x00400000` | `PlayMciWavesCommandIfDeviceArmed` | `src/audio/AudioPipeline.c` | `NW_UNIT_PlayMciWavesCommandIfDeviceArmed` | C (ratcheted) | 0 | audio |
| `0x00402010` | `FunNoOpRet4Stub` | `src/runtime/Hooks.c` | `NW_UNIT_NoOpStackCleanupStub` | C (ratcheted) | 0 | runtime |
| `0x00402014` | `NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble` | `src/runtime/PackedFields.c` | `NW_UNIT_NegateHighByteThenSwapPackedFieldBytesAndRotateHighNibble` | C (ratcheted) | 0 | runtime |
| `0x00402044` | `NegateBothPackedFieldBytesAndRotateHighNibble` | `src/runtime/PackedFields.c` | `NW_UNIT_NegateBothPackedFieldBytesAndRotateHighNibble` | C (ratcheted) | 0 | runtime |
| `0x0040206c` | `SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble` | `src/runtime/PackedFields.c` | `NW_UNIT_SwapPackedFieldBytesNegateNewHighByteAndRotateHighNibble` | C (ratcheted) | 0 | runtime |
| `0x0040209c` | `NegateMidByteAndSwapHighNibbleBit1AndBit3` | `src/runtime/PackedFields.c` | `NW_UNIT_NegateMidByteAndSwapHighNibbleBit1AndBit3` | C (ratcheted) | 0 | runtime |
| `0x004020c0` | `NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs` | `src/runtime/PackedFields.c` | `NW_UNIT_NegateBothPackedFieldBytesThenSwapAndExchangeHighNibbleBitPairs` | C (ratcheted) | 0 | runtime |
| `0x004020f0` | `NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble` | `src/runtime/PackedFields.c` | `NW_UNIT_NegatePackedFieldHighByteAndSwapLowPairBitsOfHighNibble` | C (ratcheted) | 0 | runtime |
| `0x00402114` | `SwapPackedFieldBytesAndReverseHighNibble` | `src/runtime/PackedFields.c` | `NW_UNIT_SwapPackedFieldBytesAndReverseHighNibble` | C (ratcheted) | 0 | runtime |
| `0x00402140` | `WindowTableStubs` | `src/game/WindowTables.c` | `NW_UNIT_WindowTableStubs` | C (ratcheted) | 0 | game |
| `0x00402154` | `RngPaletteHandlers` | `src/video/Palette.c` | `NW_UNIT_RngPaletteHandlers` | C (ratcheted) | 0 | video |
| `0x004023bc` | `ReleaseGridCellSpanRecord` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_ReleaseGridCellSpanRecord` | C (ratcheted) | 0 | grid |
| `0x004023d8` | `ReleaseSameDepthGridCellSpans` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_ReleaseSameDepthGridCellSpans` | C (ratcheted) | 0 | grid |
| `0x00402438` | `ReleaseSpanRecordByDepthIndex` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_ReleaseSpanRecordByDepthIndex` | C (ratcheted) | 0 | grid |
| `0x004025d0` | `ProcessPendingGridCellSpanEntry` | `src/grid/GridSpanStreams.c` | `NW_UNIT_ProcessPendingGridCellSpanEntry` | C (ratcheted) | 0 | grid |
| `0x00402654` | `ReleaseGridCellSpanRecordsAndDispatchEntry` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_ReleaseGridCellSpanRecordsAndDispatchEntry` | C (ratcheted) | 0 | grid |
| `0x0040279c` | `AdvancePendingGridCellSpanEntryAndDispatch` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_AdvancePendingGridCellSpanEntryAndDispatch` | C (ratcheted) | 0 | grid |
| `0x0040283c` | `AllocateGridCellSpanRecordAndDispatch` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_AllocateGridCellSpanRecordAndDispatch` | C (ratcheted) | 0 | grid |
| `0x00402a68` | `WrapGridAxisCursorsStampCellKeyAndRetireSpans` | `src/grid/GridMarking.c` | `NW_UNIT_WrapGridAxisCursorsStampCellKeyAndRetireSpans` | C (ratcheted) | 0 | grid |
| `0x00402bc4` | `ProcessInputAdvanceFrameInterval` | `src/input/InputFields.c` | `NW_UNIT_ProcessInputAdvanceFrameInterval` | C (ratcheted) | 0 | input |
| `0x00402c2c` | `SelectGridSpanRecordByPointAndPublishSelectionState` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_SelectGridSpanRecordByPointAndPublishSelectionState` | C (ratcheted) | 0 | grid |
| `0x00402e98` | `StepByteTowardTarget` | `src/input/InputFields.c` | `NW_UNIT_StepByteTowardTarget` | C (ratcheted) | 0 | input |
| `0x00402ef4` | `ProcessTwoBytesEaseAndFireHookOnChange` | `src/input/InputFields.c` | `NW_UNIT_ProcessTwoBytesEaseAndFireHookOnChange` | C (ratcheted) | 0 | input |
| `0x00402f34` | `AdvanceGridCellSubSpan` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_AdvanceGridCellSubSpan` | C (ratcheted) | 0 | grid |
| `0x00402fb4` | `ProcessQueuedTaskRecords` | `src/app/AppLifecycle.c` | `NW_UNIT_ProcessQueuedTaskRecords` | C (ratcheted) | 0 | app |
| `0x00403314` | `WalkGridSpanChainAndDispatchInputByte` | `src/input/InputRouting.c` | `NW_UNIT_WalkGridSpanChainAndDispatchInputByte` | C (ratcheted) | 0 | input |
| `0x004033a8` | `AdvanceActiveGridCellSpanToOffset` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_AdvanceActiveGridCellSpanToOffset` | C (ratcheted) | 0 | grid |
| `0x00403484` | `InstallActiveGridCellRef` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_InstallActiveGridCellRef` | C (ratcheted) | 0 | grid |
| `0x00403868` | `ResetGridSpanGeometryIfModeFlagClear` | `src/grid/GridSpanStreams.c` | `NW_UNIT_ResetGridSpanGeometryIfModeFlagClear` | C (ratcheted) | 0 | grid |
| `0x004038b8` | `AnimateScrollTowardSelectedGridCell` | `src/grid/GridMarking.c` | `NW_UNIT_AnimateScrollTowardSelectedGridCell` | C (ratcheted) | 0 | grid |
| `0x00403b84` | `InitGlobalRngAndResetStateFlags` | `src/game/GameState.c` | `NW_UNIT_InitGlobalRngAndResetStateFlags` | C (ratcheted) | 0 | game |
| `0x00403c28` | `ClearGlobalFlag417808` | `src/game/GameState.c` | `NW_UNIT_ClearGridActiveFlag` | C (ratcheted) | 0 | game |
| `0x00403c30` | `SeedRandomFromClockAndClearWorkQueue` | `src/game/GameState.c` | `NW_UNIT_SeedRandomFromClockAndClearWorkQueue` | C (ratcheted) | 0 | game |
| `0x00403c70` | `FlushPendingSurfaceRectsAndReseedRandom` | `src/video/VideoAssets.c` | `NW_UNIT_FlushPendingSurfaceRectsAndReseedRandom` | C (ratcheted) | 0 | video |
| `0x00403d00` | `DispatchExceptionOnGlobalScopeRecord` | `src/runtime/Hooks.c` | `NW_UNIT_DispatchExceptionOnGlobalScopeRecord` | C (ratcheted) | 0 | runtime |
| `0x00403d10` | `NotifyCellOperandHookAndAllocateGridSpanRecord` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_NotifyCellOperandHookAndAllocateGridSpanRecord` | C (ratcheted) | 0 | grid |
| `0x00403d38` | `FunAxisWrapClone_00403d38` | `src/grid/GridCore.c` | `NW_UNIT_WrapGridAxesAndDispatch` | C (ratcheted) | 0 | grid |
| `0x00403d8c` | `ExecuteOpcodeStream` | `src/vm/OpcodeExecution.c` | `NW_UNIT_ExecuteOpcodeStream` | C (ratcheted) | 0 | vm |
| `0x0040440c` | `AddRectToExtentList` | `src/video/Blit.c` | `NW_UNIT_AddRectToExtentList` | C (ratcheted) | 0 | video |
| `0x00404590` | `InitializeAppSubsystemsFromSavedPath` | `src/app/AppLifecycle.c` | `NW_UNIT_InitializeAppSubsystemsFromSavedPath` | C (ratcheted) | 0 | app |
| `0x0040461c` | `StoreArgsAndProcessQueuedRecords` | `src/input/InputRouting.c` | `NW_UNIT_StoreArgsAndProcessQueuedRecords` | C (ratcheted) | 0 | input |
| `0x00404724` | `AddTranslatedRectToExtentList` | `src/video/Blit.c` | `NW_UNIT_AddTranslatedRectToExtentList` | C (ratcheted) | 0 | video |
| `0x00404758` | `RebuildGridPaletteAndResetExtent` | `src/video/Palette.c` | `NW_UNIT_RebuildGridPaletteAndResetExtent` | C (ratcheted) | 0 | video |
| `0x00404800` | `InitializeHookAndFlagGlobals` | `src/app/AppLifecycle.c` | `NW_UNIT_InitializeHookAndFlagGlobals` | C (ratcheted) | 0 | app |
| `0x0040487c` | `ClearGridSpanChainBoundsAndHeapState` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_ClearGridSpanChainBoundsAndHeapState` | C (ratcheted) | 0 | grid |
| `0x004048fc` | `AllocHeapBlock` | `src/runtime/Heap.c` | `NW_UNIT_AllocHeapBlock` | C (ratcheted) | 0 | runtime |
| `0x00404a00` | `InitializeGlobalHeapBuffer` | `src/runtime/Heap.c` | `NW_UNIT_InitializeGlobalHeapBuffer` | C (ratcheted) | 0 | runtime |
| `0x00404a28` | `FreeGlobalHeapBlock` | `src/runtime/Heap.c` | `NW_UNIT_FreeGlobalHeapBlock` | C (ratcheted) | 0 | runtime |
| `0x00404aa8` | `FUN_00404aa8` | `src/runtime/Heap.c` | `NW_UNIT_InstallInitialHeapRegionBounds` | C (ratcheted) | 0 | runtime |
| `0x00404b84` | `GetStreamLogicalBytePosition` | `src/io/StreamRecords.c` | `NW_UNIT_GetStreamLogicalBytePosition` | C (ratcheted) | 0 | io |
| `0x00404b90` | `SeekStreamLogicalOffset` | `src/io/StreamRecords.c` | `NW_UNIT_SeekStreamLogicalOffset` | C (ratcheted) | 0 | io |
| `0x00404bbc` | `ReadStreamBytesToDestBuffer` | `src/io/StreamRecords.c` | `NW_UNIT_ReadStreamBytesToDestBuffer` | C (ratcheted) | 0 | io |
| `0x00404c4c` | `CloseSaveSlotAndLatchInvalidDataError` | `src/io/SaveFiles.c` | `NW_UNIT_CloseSaveSlotAndLatchInvalidDataError` | C (ratcheted) | 0 | io |
| `0x00404c5c` | `FetchNextStreamByteIntoSlotDescriptor` | `src/vm/OpcodeExecution.c` | `NW_UNIT_FetchNextStreamByteIntoSlotDescriptor` | C (ratcheted) | 0 | vm |
| `0x00404c6c` | `CheckIndexedStreamEntryAndReadRecordTag` | `src/io/StreamRecords.c` | `NW_UNIT_CheckIndexedStreamEntryAndReadRecordTag` | C (ratcheted) | 0 | io |
| `0x00404ce8` | `DecodeTaggedRecordLengthToOutParam` | `src/io/StreamRecords.c` | `NW_UNIT_DecodeTaggedRecordLengthToOutParam` | C (ratcheted) | 0 | io |
| `0x00404d94` | `ReadTaggedRecordDeltaCountAndLatchStreamPosition` | `src/io/StreamRecords.c` | `NW_UNIT_ReadTaggedRecordDeltaCountAndLatchStreamPosition` | C (ratcheted) | 0 | io |
| `0x00404df4` | `ReadLzssOrRawTaggedRecordBlock` | `src/io/StreamRecords.c` | `NW_UNIT_ReadLzssOrRawTaggedRecordBlock` | C (ratcheted) | 0 | io |
| `0x00404f00` | `LoadAndVerifySaveFileHeaderChunk` | `src/io/win32/StreamConversions.c` | `NW_UNIT_LoadAndVerifySaveFileHeaderChunk` | C (ratcheted) | 0 | io |
| `0x0040524c` | `RebuildGridHeapAndCursorStateFromStream` | `src/grid/GridSpanStreams.c` | `NW_UNIT_RebuildGridHeapAndCursorStateFromStream` | C (ratcheted) | 0 | grid |
| `0x00405318` | `InitializeGridArenaFromMapHeader` | `src/grid/GridSpanStreams.c` | `NW_UNIT_InitializeGridArenaFromMapHeader` | C (ratcheted) | 0 | grid |
| `0x004054b8` | `ExecuteOpcodeAndStoreOperand` | `src/vm/OpcodeExecution.c` | `NW_UNIT_ExecuteOpcodeAndStoreOperand` | C (ratcheted) | 0 | vm |
| `0x0040587c` | `InterpretOpcodeStreamUntilEnd` | `src/vm/OpcodeExecution.c` | `NW_UNIT_InterpretOpcodeStreamUntilEnd` | C (ratcheted) | 0 | vm |
| `0x00405ab0` | `DispatchPendingCommandLoop` | `src/app/AppLifecycle.c` | `NW_UNIT_DispatchPendingCommandLoop` | C (ratcheted) | 0 | app |
| `0x00405ebc` | `LoadRecordBlocksIntoTileCache` | `src/io/SaveFiles.c` | `NW_UNIT_LoadRecordBlocksIntoTileCache` | C (ratcheted) | 0 | io |
| `0x00405fcc` | `LoadDiskSpriteImageFromStream` | `src/video/VideoAssets.c` | `NW_UNIT_LoadDiskSpriteImageFromStream` | C (ratcheted) | 0 | video |
| `0x0040604c` | `ForwardCloseSaveFileSlot` | `src/io/SaveFiles.c` | `NW_UNIT_ForwardCloseSaveFileSlot` | C (ratcheted) | 0 | io |
| `0x00406080` | `LoadDecryptRegionsFromDescriptor` | `src/io/SaveFiles.c` | `NW_UNIT_LoadDecryptRegionsFromDescriptor` | C (ratcheted) | 0 | io |
| `0x004060ec` | `WriteBufferRegionsScrambledToStream` | `src/io/SaveFiles.c` | `NW_UNIT_WriteBufferRegionsScrambledToStream` | C (ratcheted) | 0 | io |
| `0x00406164` | `ClearBufferRegionsFromDescriptor` | `src/io/SaveFiles.c` | `NW_UNIT_ClearBufferRegionsFromDescriptor` | C (ratcheted) | 0 | io |
| `0x004061a0` | `ReadStreamHeaderRecordIntoGlobal` | `src/io/StreamRecords.c` | `NW_UNIT_ReadStreamHeaderRecordIntoGlobal` | C (ratcheted) | 0 | io |
| `0x004061b0` | `WriteSaveFileHeaderAndBufferRegions` | `src/io/SaveFiles.c` | `NW_UNIT_WriteSaveFileHeaderAndBufferRegions` | C (ratcheted) | 0 | io |
| `0x004061e0` | `WriteStreamChunkFromStackArgumentDword` | `src/io/StreamRecords.c` | `NW_UNIT_WriteStreamChunkFromStackArgumentDword` | C (ratcheted) | 0 | io |
| `0x004061f0` | `ApplyDeltaToUnmarkedEntries` | `src/grid/GridMarking.c` | `NW_UNIT_ApplyDeltaToUnmarkedEntries` | C (ratcheted) | 0 | grid |
| `0x0040621c` | `LoadOrRepairSaveFileRegions` | `src/io/SaveFiles.c` | `NW_UNIT_LoadOrRepairSaveFileRegions` | C (ratcheted) | 0 | io |
| `0x00406314` | `WriteSaveFileHeaderAndScrambledRegions` | `src/io/SaveFiles.c` | `NW_UNIT_WriteSaveFileHeaderAndScrambledRegions` | C (ratcheted) | 0 | io |
| `0x00406378` | `DeleteNetWalkSaveVariantByIndex` | `src/io/FileSlots.c` | `NW_UNIT_DeleteNetWalkSaveVariantByIndex` | C (ratcheted) | 0 | io |
| `0x00406390` | `InitializeDispatchRecords` | `src/io/SaveFiles.c` | `NW_UNIT_InitializeDispatchRecords` | C (ratcheted) | 0 | io |
| `0x0040660c` | `WriteSaveFileBuffersAndLoadTileCache` | `src/io/SaveFiles.c` | `NW_UNIT_WriteSaveFileBuffersAndLoadTileCache` | C (ratcheted) | 0 | io |
| `0x00406774` | `ResetTileRecordStateAndSeekStream` | `src/grid/GridSpanStreams.c` | `NW_UNIT_ResetTileRecordStateAndSeekStream` | C (ratcheted) | 0 | grid |
| `0x004067fc` | `FreeHeapBlockIfNotNull` | `src/runtime/Heap.c` | `NW_UNIT_FreeHeapBlockIfNotNull` | C (ratcheted) | 0 | runtime |
| `0x00406868` | `ResetSubsystemsAndLatchStateFlag` | `src/app/AppLifecycle.c` | `NW_UNIT_ResetSubsystemsAndLatchStateFlag` | C (ratcheted) | 0 | app |
| `0x00406878` | `ReadStreamBlockRawOrLzss` | `src/io/StreamRecords.c` | `NW_UNIT_ReadStreamBlockRawOrLzss` | C (ratcheted) | 0 | io |
| `0x004068cc` | `DispatchSubsystemInitByOpcode` | `src/app/AppLifecycle.c` | `NW_UNIT_DispatchSubsystemInitByOpcode` | C (ratcheted) | 0 | app |
| `0x00406a68` | `ComposeFrameCellsFromStreamRecords` | `src/grid/GridCore.c` | `NW_UNIT_ComposeFrameCellsFromStreamRecords` | C (ratcheted) | 0 | grid |
| `0x00406d54` | `ReadPaletteBlockAndSetLogicalPalette` | `src/video/Palette.c` | `NW_UNIT_ReadPaletteBlockAndSetLogicalPalette` | C (ratcheted) | 0 | video |
| `0x00406d8c` | `UpdateCachedSlotStatus` | `src/io/FileSlots.c` | `NW_UNIT_UpdateCachedSlotStatus` | C (ratcheted) | 0 | io |
| `0x00406dd0` | `ReadBlockAndLatchPendingSlotHook` | `src/io/StreamRecords.c` | `NW_UNIT_ReadBlockAndLatchPendingSlotHook` | C (ratcheted) | 0 | io |
| `0x00406e48` | `RetryLockedProbeDispatch` | `src/runtime/Hooks.c` | `NW_UNIT_RetryLockedProbeDispatch` | C (ratcheted) | 0 | runtime |
| `0x00406f28` | `DispatchLockedProbeEntryIfArmed` | `src/runtime/Hooks.c` | `NW_UNIT_DispatchLockedProbeEntryIfArmed` | C (ratcheted) | 0 | runtime |
| `0x00406f4c` | `SetPendingFlagFromSlotHookResult` | `src/runtime/Slots.c` | `NW_UNIT_SetPendingFlagFromSlotHookResult` | C (ratcheted) | 0 | runtime |
| `0x00406f64` | `ResetPendingSlotHookState` | `src/runtime/Slots.c` | `NW_UNIT_ResetPendingSlotHookState` | C (ratcheted) | 0 | runtime |
| `0x00406f90` | `GridContextQueries` | `src/grid/GridQueries.c` | `NW_UNIT_GridContextQueries` | C (ratcheted) | 0 | grid |
| `0x00406fa4` | `NativeCallAdapters` | `src/game/GameHandlers.c` | `NW_UNIT_NativeCallAdapters` | C (ratcheted) | 0 | game |
| `0x0040705c` | `IsRemappedIndexArgEqual` | `src/grid/GridMapping.c` | `NW_UNIT_IsRemappedIndexArgEqual` | C (ratcheted) | 0 | grid |
| `0x004070f8` | `ExecuteOpcodeStreamPreservingGlobals` | `src/vm/OpcodeExecution.c` | `NW_UNIT_ExecuteOpcodeStreamPreservingGlobals` | C (ratcheted) | 0 | vm |
| `0x00407254` | `GridSpanStreamWrappers` | `src/grid/GridSpanStreams.c` | `NW_UNIT_GridSpanStreamWrappers` | C (ratcheted) | 0 | grid |
| `0x004072e4` | `DispatchPendingSlotHookForIndex` | `src/runtime/Slots.c` | `NW_UNIT_DispatchPendingSlotHookForIndex` | C (ratcheted) | 0 | runtime |
| `0x00407354` | `DispatchIndexedProbeEntry` | `src/runtime/Hooks.c` | `NW_UNIT_DispatchIndexedProbeEntry` | C (ratcheted) | 0 | runtime |
| `0x0040738c` | `AcquireGridCellSpanRecordAndRetirePrevious` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_AcquireGridCellSpanRecordAndRetirePrevious` | C (ratcheted) | 0 | grid |
| `0x00407588` | `AllocGridCellSpanRecord` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_AllocGridCellSpanRecord` | C (ratcheted) | 0 | grid |
| `0x0040760c` | `RestartCurrentGridSpanGroup` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_RestartCurrentGridSpanGroup` | C (ratcheted) | 0 | grid |
| `0x00407624` | `ReleaseTopGridCellSpans` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_ReleaseTopGridCellSpans` | C (ratcheted) | 0 | grid |
| `0x00407664` | `ProcessGridSpanCommandStream` | `src/grid/GridSpanStreams.c` | `NW_UNIT_ProcessGridSpanCommandStream` | C (ratcheted) | 0 | grid |
| `0x004079a0` | `InitializeGridSpanCommandStreamRun` | `src/grid/GridSpanStreams.c` | `NW_UNIT_InitializeGridSpanCommandStreamRun` | C (ratcheted) | 0 | grid |
| `0x00407a68` | `DecodeGridSpanIndexStream` | `src/grid/GridSpanStreams.c` | `NW_UNIT_DecodeGridSpanIndexStream` | C (ratcheted) | 0 | grid |
| `0x00407c7c` | `StampGridContextLayerAndPublishDepthSlot` | `src/grid/GridQueries.c` | `NW_UNIT_StampGridContextLayerAndPublishDepthSlot` | C (ratcheted) | 0 | grid |
| `0x00407ce4` | `ReleaseCurrentContextSpanGroup` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_ReleaseCurrentContextSpanGroup` | C (ratcheted) | 0 | grid |
| `0x00407cf8` | `GridSpanEntryActions` | `src/grid/GridSpanStreams.c` | `NW_UNIT_GridSpanEntryActions` | C (ratcheted) | 0 | grid |
| `0x00407f70` | `RouteGridPathToTargetCell` | `src/grid/GridMarking.c` | `NW_UNIT_RouteGridPathToTargetCell` | C (ratcheted) | 0 | grid |
| `0x004081b8` | `MarkZeroCellsAdjacentToSixCellsAndRecordTwo` | `src/grid/GridMarking.c` | `NW_UNIT_MarkZeroCellsAdjacentToSixCellsAndRecordTwo` | C (ratcheted) | 0 | grid |
| `0x00408284` | `MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo` | `src/grid/GridMarking.c` | `NW_UNIT_MarkEmptyGridCellsAdjacentToTypeFourAndLocateTypeTwo` | C (ratcheted) | 0 | grid |
| `0x00408350` | `MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo` | `src/grid/GridMarking.c` | `NW_UNIT_MarkZeroCellsAdjacentToFiveCellsWithSixAndRecordTwo` | C (ratcheted) | 0 | grid |
| `0x0040841c` | `SequenceGridMarkingSweepsUntilMatchAndDispatch` | `src/grid/GridMarking.c` | `NW_UNIT_SequenceGridMarkingSweepsUntilMatchAndDispatch` | C (ratcheted) | 0 | grid |
| `0x004084bc` | `MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero` | `src/grid/GridMapping.c` | `NW_UNIT_MapGridCellsAgainstCallerSentinelToTwoOnMatchElseZero` | C (ratcheted) | 0 | grid |
| `0x00408510` | `MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree` | `src/grid/GridMapping.c` | `NW_UNIT_MapGridTilesByFeAndCallerSentinelToDisplayZeroTwoThree` | C (ratcheted) | 0 | grid |
| `0x0040856c` | `MapGridTilesViaSentinelFeToDisplayGrid` | `src/grid/GridMapping.c` | `NW_UNIT_MapGridTilesViaSentinelFeToDisplayGrid` | C (ratcheted) | 0 | grid |
| `0x004085d8` | `MapGridCellsAgainstCallerSentinelToDisplayByte` | `src/grid/GridMapping.c` | `NW_UNIT_MapGridCellsAgainstCallerSentinelToDisplayByte` | C (ratcheted) | 0 | grid |
| `0x00408648` | `SelectAndDispatchGridRenderMappingBySentinelGroup` | `src/grid/GridMapping.c` | `NW_UNIT_SelectAndDispatchGridRenderMappingBySentinelGroup` | C (ratcheted) | 0 | grid |
| `0x004086e4` | `ExpandCellTileIndexBitmapBitsToGridBytes` | `src/grid/GridMapping.c` | `NW_UNIT_ExpandCellTileIndexBitmapBitsToGridBytes` | C (ratcheted) | 0 | grid |
| `0x00408778` | `MapGridTilesViaBitmapToDisplayGrid` | `src/grid/GridMapping.c` | `NW_UNIT_MapGridTilesViaBitmapToDisplayGrid` | C (ratcheted) | 0 | grid |
| `0x0040881c` | `MapTileBitmapBitToGridZeroOrThree` | `src/grid/GridMapping.c` | `NW_UNIT_MapTileBitmapBitToGridZeroOrThree` | C (ratcheted) | 0 | grid |
| `0x004088cc` | `SelectAndDispatchGridRenderMappingByCommand` | `src/grid/GridMapping.c` | `NW_UNIT_SelectAndDispatchGridRenderMappingByCommand` | C (ratcheted) | 0 | grid |
| `0x004089b4` | `DispatchThreeToElevenCommandToSentinelGroupMapper` | `src/grid/GridMapping.c` | `NW_UNIT_DispatchThreeToElevenCommandToSentinelGroupMapper` | C (ratcheted) | 0 | grid |
| `0x00408a58` | `DispatchOneBasedCommandToSentinelGroupMapper` | `src/grid/GridMapping.c` | `NW_UNIT_DispatchOneBasedCommandToSentinelGroupMapper` | C (ratcheted) | 0 | grid |
| `0x00408b38` | `DispatchOneBasedCommandToGridRenderMapper` | `src/grid/GridMapping.c` | `NW_UNIT_DispatchOneBasedCommandToGridRenderMapper` | C (ratcheted) | 0 | grid |
| `0x00408c18` | `GridStateHandlers` | `src/grid/GridQueries.c` | `NW_UNIT_GridStateHandlers` | C (ratcheted) | 0 | grid |
| `0x00408dd8` | `IsSettingActiveByIndex` | `src/game/GameState.c` | `NW_UNIT_IsSettingActiveByIndex` | C (ratcheted) | 0 | game |
| `0x00408e1c` | `HandleAudioTransportOpcode` | `src/audio/AudioMix.c` | `NW_UNIT_HandleAudioTransportOpcode` | C (ratcheted) | 0 | audio |
| `0x00408eac` | `SaveAndCommandHandlers` | `src/game/GameHandlers.c` | `NW_UNIT_SaveAndCommandHandlers` | C (ratcheted) | 0 | game |
| `0x00408f2c` | `WindowTableTailHandlers` | `src/game/WindowTables.c` | `NW_UNIT_WindowTableTailHandlers` | C (ratcheted) | 0 | game |
| `0x00408f78` | `ResetSlotAndHookTablesToDefaults` | `src/runtime/Slots.c` | `NW_UNIT_ResetSlotAndHookTablesToDefaults` | C (ratcheted) | 0 | runtime |
| `0x00408f8c` | `InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook` | `src/audio/AudioState.c` | `NW_UNIT_InitializeAudioSubsystemThenDispatchPlaybackAndPendingSlotHook` | C (ratcheted) | 0 | audio |
| `0x00408fcc` | `ResetSlotTablesFormatApplyAndDispatchSettings` | `src/runtime/Slots.c` | `NW_UNIT_ResetSlotTablesFormatApplyAndDispatchSettings` | C (ratcheted) | 0 | runtime |
| `0x0040905c` | `InitializeBlankSpaceFilledField32` | `src/input/InputFields.c` | `NW_UNIT_InitializeBlankSpaceFilledField32` | C (ratcheted) | 0 | input |
| `0x00409078` | `AppendByteToShiftedField32` | `src/input/InputFields.c` | `NW_UNIT_AppendByteToShiftedField32` | C (ratcheted) | 0 | input |
| `0x004090d0` | `UpdateMciAudioStateAndPlay` | `src/audio/AudioPipeline.c` | `NW_UNIT_UpdateMciAudioStateAndPlay` | C (ratcheted) | 0 | audio |
| `0x00409108` | `DispatchAudioActionByIndex` | `src/audio/AudioMix.c` | `NW_UNIT_DispatchAudioActionByIndex` | C (ratcheted) | 0 | audio |
| `0x00409184` | `ScanFourDecimalFieldsToByteOrWordGlobals` | `src/runtime/RuntimeSupport.c` | `NW_UNIT_ScanFourDecimalFieldsToByteOrWordGlobals` | C (ratcheted) | 0 | runtime |
| `0x0040931c` | `AddGridCellSpanToExtentList` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_AddGridCellSpanToExtentList` | C (ratcheted) | 0 | grid |
| `0x00409360` | `PublishGridCellSpanByDepthIndexToExtentList` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_PublishGridCellSpanByDepthIndexToExtentList` | C (ratcheted) | 0 | grid |
| `0x00409378` | `ResolveGridReferenceAndAddCellSpan` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_ResolveGridReferenceAndAddCellSpan` | C (ratcheted) | 0 | grid |
| `0x00409568` | `FindPredecessorDescriptorPointerByBackwardScan` | `src/grid/GridQueries.c` | `NW_UNIT_FindPredecessorDescriptorPointerByBackwardScan` | C (ratcheted) | 0 | grid |
| `0x004095a0` | `PublishCursorSlotSpanAndFlagContextOnOperandMismatch` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_PublishCursorSlotSpanAndFlagContextOnOperandMismatch` | C (ratcheted) | 0 | grid |
| `0x00409600` | `TestPointInsideGridSpanRecord` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_TestPointInsideGridSpanRecord` | C (ratcheted) | 0 | grid |
| `0x00409664` | `FillMemoryWithZeroBytes` | `src/runtime/Heap.c` | `NW_UNIT_FillMemoryWithZeroBytes` | C (ratcheted) | 0 | runtime |
| `0x0040967c` | `DispatchScriptOpcode` | `src/vm/OpcodeExecution.c` | `NW_UNIT_DispatchScriptOpcode` | C (ratcheted) | 0 | vm |
| `0x00409898` | `PublishScreenGeometryAndClaimFrameBuffer` | `src/video/VideoAssets.c` | `NW_UNIT_PublishScreenGeometryAndClaimFrameBuffer` | C (ratcheted) | 0 | video |
| `0x004098dc` | `CopyRowsBulkWithTailToDisplayBuffer` | `src/video/Blit.c` | `NW_UNIT_CopyRowsBulkWithTailToDisplayBuffer` | C (ratcheted) | 0 | video |
| `0x0040998c` | `CopyTextRowsUnrolledSkipNulToDisplayBuffer` | `src/video/TextRender.c` | `NW_UNIT_CopyTextRowsUnrolledSkipNulToDisplayBuffer` | C (ratcheted) | 0 | video |
| `0x00409af8` | `CopyTextRowsToDisplayBuffer` | `src/video/TextRender.c` | `NW_UNIT_CopyTextRowsToDisplayBuffer` | C (ratcheted) | 0 | video |
| `0x00409b80` | `CopyTextRowsForwardSkipNulToDisplayBuffer` | `src/video/TextRender.c` | `NW_UNIT_CopyTextRowsForwardSkipNulToDisplayBuffer` | C (ratcheted) | 0 | video |
| `0x00409c0c` | `CopyTextRowsReverseSkipNulToDisplayBuffer` | `src/video/TextRender.c` | `NW_UNIT_CopyTextRowsReverseSkipNulToDisplayBuffer` | C (ratcheted) | 0 | video |
| `0x00409c98` | `FillActiveSurfaceWithReplicatedIndexDword` | `src/video/Blit.c` | `NW_UNIT_FillActiveSurfaceWithReplicatedIndexDword` | C (ratcheted) | 0 | video |
| `0x00409e04` | `ComputeCellAddressAndDispatchGridDelta` | `src/grid/GridCore.c` | `NW_UNIT_ComputeCellAddressAndDispatchGridDelta` | C (ratcheted) | 0 | grid |
| `0x00409e48` | `DecompressLzssBlockToBuffer` | `src/io/StreamRecords.c` | `NW_UNIT_DecompressLzssBlockToBuffer` | C (ratcheted) | 0 | io |
| `0x00409ea4` | `BlitRectOrTileGridToPlayerSurface` | `src/video/Blit.c` | `NW_UNIT_BlitRectOrTileGridToPlayerSurface` | C (ratcheted) | 0 | video |
| `0x00409ee0` | `UnprepareAndCloseWaveOut` | `src/audio/win32/WaveOutMidi.c` | `NW_UNIT_UnprepareAndCloseWaveOut` | C (ratcheted) | 0 | audio |
| `0x00409f10` | `OpenWaveOutAndPrepareHeaderRing` | `src/audio/win32/WaveOutMidi.c` | `NW_UNIT_OpenWaveOutAndPrepareHeaderRing` | C (ratcheted) | 0 | audio |
| `0x0040a030` | `FindWaveHeaderInUseSlotIndex` | `src/audio/win32/WaveOutMidi.c` | `NW_UNIT_FindWaveHeaderInUseSlotIndex` | C (ratcheted) | 0 | audio |
| `0x0040a060` | `UtNotifyAudioHandlers` | `src/audio/win32/UtNotify.c` | `NW_UNIT_UtNotifyAudioHandlers` | C (ratcheted) | 0 | audio |
| `0x0040a0c4` | `UtNotifyStub0x0040a0c4` | `src/audio/win32/UtNotify.c` | `NW_UNIT_UtNotifyReadWaveOutVolume` | C (ratcheted) | 0 | audio |
| `0x0040a144` | `ResetMidiOutAndTimerEvent` | `src/audio/win32/WaveOutMidi.c` | `NW_UNIT_ResetMidiOutAndTimerEvent` | C (ratcheted) | 0 | audio |
| `0x0040a170` | `DecodeMidiTrackVariableLengthQuantity` | `src/audio/AudioPipeline.c` | `NW_UNIT_DecodeMidiTrackVariableLengthQuantity` | C (ratcheted) | 0 | audio |
| `0x0040a1a4` | `DispatchMidiSequencerEventsWithRunningStatus` | `src/audio/win32/UtNotify.c` | `NW_UNIT_DispatchMidiSequencerEventsWithRunningStatus` | C (ratcheted) | 0 | audio |
| `0x0040a334` | `ShutdownAudioSubsystem` | `src/audio/win32/WaveOutMidi.c` | `NW_UNIT_ShutdownAudioSubsystem` | C (ratcheted) | 0 | audio |
| `0x0040a378` | `InitializeMidiOutAndTimerResolution` | `src/audio/win32/WaveOutMidi.c` | `NW_UNIT_InitializeMidiOutAndTimerResolution` | C (ratcheted) | 0 | audio |
| `0x0040a494` | `UtNotifyStub0x0040a494` | `src/audio/win32/UtNotify.c` | `NW_UNIT_UtNotifyReadMidiVolume` | C (ratcheted) | 0 | audio |
| `0x0040a51c` | `ReturnBareRetStub` | `src/runtime/Slots.c` | `NW_UNIT_SlotHookReturnVoid` | C (ratcheted) | 0 | runtime |
| `0x0040a520` | `Fun40A520HookReturnsTrueStub` | `src/runtime/Slots.c` | `NW_UNIT_ReturnTrueSlotHook` | C (ratcheted) | 0 | runtime |
| `0x0040a534` | `ShutdownUtDriverAndFreeResources` | `src/audio/win32/UtNotify.c` | `NW_UNIT_ShutdownUtDriverAndFreeResources` | C (ratcheted) | 0 | audio |
| `0x0040a684` | `InitializeUtDriverAndHandlerTable` | `src/audio/win32/UtNotify.c` | `NW_UNIT_InitializeUtDriverAndHandlerTable` | C (ratcheted) | 0 | audio |
| `0x0040a7b0` | `TearDownWaveOutAndMidiAudioSubsystem` | `src/audio/AudioState.c` | `NW_UNIT_TearDownWaveOutAndMidiAudioSubsystem` | C (ratcheted) | 0 | audio |
| `0x0040a7bc` | `InitializeAudioSubsystemAndUtHandlerTable` | `src/audio/AudioPipeline.c` | `NW_UNIT_InitializeAudioSubsystemAndUtHandlerTable` | C (ratcheted) | 0 | audio |
| `0x0040a860` | `CopyDwordsAndFillTailWithSilence` | `src/audio/AudioMix.c` | `NW_UNIT_CopyDwordsAndFillTailWithSilence` | C (ratcheted) | 0 | audio |
| `0x0040a88c` | `BlendSaturatedByteBuffers` | `src/audio/AudioMix.c` | `NW_UNIT_BlendSaturatedByteBuffers` | C (ratcheted) | 0 | audio |
| `0x0040a90c` | `OpenMciCdAudioDeviceAndArmEnabledFlag` | `src/audio/win32/MciTransport.c` | `NW_UNIT_OpenMciCdAudioDeviceAndArmEnabledFlag` | C (ratcheted) | 0 | audio |
| `0x0040a958` | `DispatchMciSetFlagsAndPlay` | `src/audio/win32/MciTransport.c` | `NW_UNIT_DispatchMciSetFlagsAndPlay` | C (ratcheted) | 0 | audio |
| `0x0040aab4` | `CloseMciCdAudioDeviceAndClearEnabledFlag` | `src/audio/win32/MciTransport.c` | `NW_UNIT_CloseMciCdAudioDeviceAndClearEnabledFlag` | C (ratcheted) | 0 | audio |
| `0x0040aae0` | `InitializeAudioSubsystemByOsVersionAndOpenMciDevice` | `src/audio/win32/MciTransport.c` | `NW_UNIT_InitializeAudioSubsystemByOsVersionAndOpenMciDevice` | C (ratcheted) | 0 | audio |
| `0x0040ab78` | `HandleSlotHookWithArg` | `src/runtime/Hooks.c` | `NW_UNIT_HandleSlotHookWithArg` | C (ratcheted) | 0 | runtime |
| `0x0040ac30` | `CallSlotHookWithStackArgRet4` | `src/runtime/Hooks.c` | `NW_UNIT_ForwardStackArgToSlotHook` | C (ratcheted) | 0 | runtime |
| `0x0040ac40` | `ProcessSlotHookThunkArg` | `src/runtime/Hooks.c` | `NW_UNIT_ProcessSlotHookThunkArg` | C (ratcheted) | 0 | runtime |
| `0x0040ac50` | `InvokeSlotHookThunk` | `src/runtime/Hooks.c` | `NW_UNIT_InvokeSlotHookThunk` | C (ratcheted) | 0 | runtime |
| `0x0040ac80` | `MixActiveAudioSlotsIntoWaveOutBuffer` | `src/audio/AudioMix.c` | `NW_UNIT_MixActiveAudioSlotsIntoWaveOutBuffer` | C (ratcheted) | 0 | audio |
| `0x0040ad08` | `CallGmwDispatch206` | `src/runtime/Hooks.c` | `NW_UNIT_CallGmwDispatch206` | C (ratcheted) | 0 | runtime |
| `0x0040ad10` | `InvokeUtNotifySlot8Thunk` | `src/audio/AudioPipeline.c` | `NW_UNIT_InvokeUtNotifySlot8Thunk` | C (ratcheted) | 0 | audio |
| `0x0040ad18` | `ResetHookTableToDefaults` | `src/runtime/Slots.c` | `NW_UNIT_ResetHookTableToDefaults` | C (ratcheted) | 0 | runtime |
| `0x0040ad74` | `ShowMainWindowErrorBoxAfterBeep` | `src/ui/win32/WindowChrome.c` | `NW_UNIT_ShowMainWindowErrorBoxAfterBeep` | C (ratcheted) | 0 | ui |
| `0x0040ad98` | `ShowMainWindowErrorBoxWithFixedCaption` | `src/ui/Dialogs.c` | `NW_UNIT_ShowMainWindowErrorBoxWithFixedCaption` | C (ratcheted) | 0 | ui |
| `0x0040ae00` | `DisplayHelpAndDestroyMainWindow` | `src/ui/win32/WindowChrome.c` | `NW_UNIT_DisplayHelpAndDestroyMainWindow` | C (ratcheted) | 0 | ui |
| `0x0040ae2c` | `ShutdownAndReleaseResources` | `src/app/win32/AppShutdown.c` | `NW_UNIT_ShutdownAndReleaseResources` | C (ratcheted) | 0 | app |
| `0x0040aea8` | `GetActiveArrowCursor` | `src/input/win32/WindowMessages.c` | `NW_UNIT_GetActiveArrowCursor` | C (ratcheted) | 0 | input |
| `0x0040AF90` | `HandleMainWindowMessage` | `src/input/win32/WindowMessages.c` | `NW_UNIT_HandleMainWindowMessage` | C (ratcheted) | 0 | input |
| `0x0040b264` | `FindFirstDriveTypeMatchAndEmitLetter` | `src/io/FileSlots.c` | `NW_UNIT_FindFirstDriveTypeMatchAndEmitLetter` | C (ratcheted) | 0 | io |
| `0x0040b2a8` | `FindBestAvailableDriveAndEmitLabel` | `src/io/win32/PathHelpers.c` | `NW_UNIT_FindBestAvailableDriveAndEmitLabel` | C (ratcheted) | 0 | io |
| `0x0040b2f8` | `FindMovieFileAcrossDrivesAndEmitBmpAviPaths` | `src/video/win32/MoviePlayback.c` | `NW_UNIT_FindMovieFileAcrossDrivesAndEmitBmpAviPaths` | C (ratcheted) | 0 | video |
| `0x0040b404` | `BuildAviBmpPathsFromSourcePath` | `src/video/win32/MoviePlayback.c` | `NW_UNIT_BuildAviBmpPathsFromSourcePath` | C (ratcheted) | 0 | video |
| `0x0040b498` | `LoadNetwalkTileImageFile` | `src/video/VideoAssets.c` | `NW_UNIT_LoadNetwalkTileImageFile` | C (ratcheted) | 0 | video |
| `0x0040b5fc` | `PlayVideoTileMovieAndLoadBmp` | `src/video/win32/MoviePlayback.c` | `NW_UNIT_PlayVideoTileMovieAndLoadBmp` | C (ratcheted) | 0 | video |
| `0x0040b6f8` | `DispatchQueuedNonInputMessages` | `src/input/win32/WindowMessages.c` | `NW_UNIT_DispatchQueuedNonInputMessages` | C (ratcheted) | 0 | input |
| `0x0040B770` | `PlayStartupMovieWithMessagePumpAndTeardown` | `src/video/win32/MoviePlayback.c` | `NW_UNIT_PlayStartupMovieWithMessagePumpAndTeardown` | C (ratcheted) | 0 | video |
| `0x0040b778` | `zApiSlotPointers` | `src/runtime/win32/Platform.c` | `NW_UNIT_ApiSlotPointers` | C (ratcheted) | 0 | runtime |
| `0x0040b958` | `ProcessMainWindowMessages` | `src/input/win32/WindowMessages.c` | `NW_UNIT_ProcessMainWindowMessages` | C (ratcheted) | 0 | input |
| `0x0040BDD4` | `TryPlayStartupMovieWithWindowFallback` | `src/video/VideoAssets.c` | `NW_UNIT_TryPlayStartupMovieWithWindowFallback` | C (ratcheted) | 0 | video |
| `0x0040be54` | `CreatePlayerMainWindow` | `src/ui/win32/WindowChrome.c` | `NW_UNIT_CreatePlayerMainWindow` | C (ratcheted) | 0 | ui |
| `0x0040c058` | `CreatePlayerDIBBackBuffer` | `src/video/win32/VideoGdi.c` | `NW_UNIT_CreatePlayerDIBBackBuffer` | C (ratcheted) | 0 | video |
| `0x0040c148` | `SetFrameIntervalFromDivisor` | `src/input/InputFields.c` | `NW_UNIT_SetFrameIntervalFromDivisor` | C (ratcheted) | 0 | input |
| `0x0040c174` | `SetLogicalPaletteDrawGrid` | `src/video/win32/VideoGdi.c` | `NW_UNIT_SetLogicalPaletteDrawGrid` | C (ratcheted) | 0 | video |
| `0x0040c410` | `BlitRegionFromDcOrDib` | `src/video/win32/VideoGdi.c` | `NW_UNIT_BlitRegionFromDcOrDib` | C (ratcheted) | 0 | video |
| `0x0040c480` | `BlitPlayerRegionOrTileGrid` | `src/video/Blit.c` | `NW_UNIT_BlitPlayerRegionOrTileGrid` | C (ratcheted) | 0 | video |
| `0x0040c54c` | `FlushGdiDrawingBatch` | `src/video/Blit.c` | `NW_UNIT_FlushGdiDrawingBatch` | C (ratcheted) | 0 | video |
| `0x0040c554` | `ScrollCachedMainWindowByDeltaAndRepaint` | `src/ui/win32/WindowChrome.c` | `NW_UNIT_ScrollCachedMainWindowByDeltaAndRepaint` | C (ratcheted) | 0 | ui |
| `0x0040c588` | `DispatchQueuedMessagesAndStampStartTime` | `src/input/win32/WindowMessages.c` | `NW_UNIT_DispatchQueuedMessagesAndStampStartTime` | C (ratcheted) | 0 | input |
| `0x0040c614` | `DispatchWinHelpByPrefix` | `src/ui/win32/WindowChrome.c` | `NW_UNIT_DispatchWinHelpByPrefix` | C (ratcheted) | 0 | ui |
| `0x0040c684` | `RunCommandLineAndWait` | `src/app/win32/WinMain.c` | `NW_UNIT_RunCommandLineAndWait` | C (ratcheted) | 0 | app |
| `0x0040c728` | `OpenTrackedSlotBinaryAndStoreResult` | `src/io/FileSlots.c` | `NW_UNIT_OpenTrackedSlotBinaryAndStoreResult` | C (ratcheted) | 0 | io |
| `0x0040c74c` | `GetTrackedSlotFilePosition` | `src/io/FileSlots.c` | `NW_UNIT_GetTrackedSlotFilePosition` | C (ratcheted) | 0 | io |
| `0x0040c760` | `SeekStreamFileSlotToOffset` | `src/io/StreamRecords.c` | `NW_UNIT_SeekStreamFileSlotToOffset` | C (ratcheted) | 0 | io |
| `0x0040c788` | `ReadExactChunkFromStream` | `src/io/StreamRecords.c` | `NW_UNIT_ReadExactChunkFromStream` | C (ratcheted) | 0 | io |
| `0x0040c7b0` | `ReadStreamChunkCountFromGlobalStream` | `src/io/StreamRecords.c` | `NW_UNIT_ReadStreamChunkCountFromGlobalStream` | C (ratcheted) | 0 | io |
| `0x0040c7cc` | `CloseSaveFileSlot` | `src/io/SaveFiles.c` | `NW_UNIT_CloseSaveFileSlot` | C (ratcheted) | 0 | io |
| `0x0040c7ec` | `BuildExeVariantNameFromBaseName` | `src/io/FileSlots.c` | `NW_UNIT_BuildExeVariantNameFromBaseName` | C (ratcheted) | 0 | io |
| `0x0040c850` | `BuildTempPathForSourceFile` | `src/io/win32/PathHelpers.c` | `NW_UNIT_BuildTempPathForSourceFile` | C (ratcheted) | 0 | io |
| `0x0040c89c` | `OpenTempPathOrExeVariantSourceFile` | `src/io/FileSlots.c` | `NW_UNIT_OpenTempPathOrExeVariantSourceFile` | C (ratcheted) | 0 | io |
| `0x0040c928` | `DeleteExeVariantFileWithTempFallback` | `src/io/win32/PathHelpers.c` | `NW_UNIT_DeleteExeVariantFileWithTempFallback` | C (ratcheted) | 0 | io |
| `0x0040c998` | `ReadStreamChunkFromDescriptorVerifyCount` | `src/io/StreamRecords.c` | `NW_UNIT_ReadStreamChunkFromDescriptorVerifyCount` | C (ratcheted) | 0 | io |
| `0x0040c9c0` | `WriteStreamChunkCheckComplete` | `src/io/StreamRecords.c` | `NW_UNIT_WriteStreamChunkCheckComplete` | C (ratcheted) | 0 | io |
| `0x0040c9e8` | `RewindGlobalSlotFileToStart` | `src/io/FileSlots.c` | `NW_UNIT_RewindGlobalSlotFileToStart` | C (ratcheted) | 0 | io |
| `0x0040c9fc` | `SeekTrackedHandleAbsoluteOffset` | `src/io/FileSlots.c` | `NW_UNIT_SeekTrackedHandleAbsoluteOffset` | C (ratcheted) | 0 | io |
| `0x0040ca20` | `TruncateGlobalSlotFileToZeroLength` | `src/io/FileSlots.c` | `NW_UNIT_TruncateGlobalSlotFileToZeroLength` | C (ratcheted) | 0 | io |
| `0x0040ca34` | `CloseGlobalSlotAndResetToUnused` | `src/io/FileSlots.c` | `NW_UNIT_CloseGlobalSlotAndResetToUnused` | C (ratcheted) | 0 | io |
| `0x0040ca54` | `RunMainMessageLoop` | `src/app/win32/WinMain.c` | `NW_UNIT_RunMainMessageLoop` | C (ratcheted) | 0 | app |
| `0x0040ce9c` | `CaptureLocalTimeAsEpochSeconds` | `src/runtime/Time.c` | `NW_UNIT_CaptureLocalTimeAsEpochSeconds` | C (ratcheted) | 0 | runtime |
| `0x0040cffc` | `ParseSignedDecimalIntegerFromString` | `src/runtime/Strings.c` | `NW_UNIT_ParseSignedDecimalIntegerFromString` | C (ratcheted) | 0 | runtime |
| `0x0040d10c` | `FindFirstCharInString` | `src/runtime/Strings.c` | `NW_UNIT_FindFirstCharInString` | C (ratcheted) | 0 | runtime |
| `0x0040d1f0` | `CopyBoundedStringNulPadded` | `src/runtime/Strings.c` | `NW_UNIT_CopyBoundedStringNulPadded` | C (ratcheted) | 0 | runtime |
| `0x0040d280` | `ScanStringFormatToVarArgs` | `src/runtime/RuntimeSupport.c` | `NW_UNIT_ScanStringFormatToVarArgs` | C (ratcheted) | 0 | runtime |
| `0x0040d2a4` | `ScanFormatConversions` | `src/runtime/Strings.c` | `NW_UNIT_ScanFormatConversions` | C (ratcheted) | 0 | runtime |
| `0x0040da80` | `ParseBracketCharClassAndMatchChars` | `src/runtime/Strings.c` | `NW_UNIT_ParseBracketCharClassAndMatchChars` | C (ratcheted) | 0 | runtime |
| `0x0040dce0` | `FindFirstSubstringInString` | `src/runtime/Strings.c` | `NW_UNIT_FindFirstSubstringInString` | C (ratcheted) | 0 | runtime |
| `0x0040dde8` | `FillMemoryWithDword` | `src/runtime/Heap.c` | `NW_UNIT_FillMemoryWithDword` | C (ratcheted) | 0 | runtime |
| `0x0040de1c` | `FillMemoryWithByte` | `src/runtime/Heap.c` | `NW_UNIT_FillMemoryWithByte` | C (ratcheted) | 0 | runtime |
| `0x0040def8` | `InitializeAndRunWinMain` | `src/app/win32/WinMain.c` | `NW_UNIT_InitializeAndRunWinMain` | C (ratcheted) | 0 | app |
| `0x0040dfb0` | `InitializeStandardHandlesInTrackedTable` | `src/app/win32/WinMain.c` | `NW_UNIT_InitializeStandardHandlesInTrackedTable` | C (ratcheted) | 0 | app |
| `0x0040e1dc` | `CloseTrackedHandleSlot` | `src/io/win32/TrackedHandles.c` | `NW_UNIT_CloseTrackedHandleSlot` | C (ratcheted) | 0 | io |
| `0x0040e230` | `DeleteFileAndCacheErrno` | `src/io/win32/PathHelpers.c` | `NW_UNIT_DeleteFileAndCacheErrno` | C (ratcheted) | 0 | io |
| `0x0040e24c` | `SeekTrackedSlotHandleOffset` | `src/io/win32/TrackedHandles.c` | `NW_UNIT_SeekTrackedSlotHandleOffset` | C (ratcheted) | 0 | io |
| `0x0040e278` | `TruncateTrackedSlotFileToSize` | `src/io/win32/TrackedHandles.c` | `NW_UNIT_TruncateTrackedSlotFileToSize` | C (ratcheted) | 0 | io |
| `0x0040e2c0` | `WriteStreamChunkLfToCrLfExpanded` | `src/io/win32/StreamConversions.c` | `NW_UNIT_WriteStreamChunkLfToCrLfExpanded` | C (ratcheted) | 0 | io |
| `0x0040e4f0` | `ReadStreamChunkCrLfNormalized` | `src/io/win32/StreamConversions.c` | `NW_UNIT_ReadStreamChunkCrLfNormalized` | C (ratcheted) | 0 | io |
| `0x0040e72c` | `ConvertGmtStructToEpochSeconds` | `src/runtime/Time.c` | `NW_UNIT_ConvertGmtStructToEpochSeconds` | C (ratcheted) | 0 | runtime |
| `0x0040e7e8` | `GetDaylightSavingTimeOffsetSeconds` | `src/runtime/Time.c` | `NW_UNIT_GetDaylightSavingTimeOffsetSeconds` | C (ratcheted) | 0 | runtime |
| `0x0040e86c` | `GetFirstSundayDayOfYear` | `src/runtime/Time.c` | `NW_UNIT_GetFirstSundayDayOfYear` | C (ratcheted) | 0 | runtime |
| `0x0040e8d8` | `GetLastSundayDayOfYear` | `src/runtime/Time.c` | `NW_UNIT_GetLastSundayDayOfYear` | C (ratcheted) | 0 | runtime |
| `0x0040e940` | `GetZeroBasedDayOfYearFromTimeRecord` | `src/runtime/Time.c` | `NW_UNIT_GetZeroBasedDayOfYearFromTimeRecord` | C (ratcheted) | 0 | runtime |
| `0x0040e980` | `InitializeTimeZoneStandardAndDaylightStateOnce` | `src/runtime/Time.c` | `NW_UNIT_InitializeTimeZoneStandardAndDaylightStateOnce` | C (ratcheted) | 0 | runtime |
| `0x0040ea80` | `WriteStdoutStringAndExit` | `src/app/win32/AppShutdown.c` | `NW_UNIT_WriteStdoutStringAndExit` | C (ratcheted) | 0 | app |
| `0x0040eac0` | `MultiplyLow64Product` | `src/runtime/Tables.c` | `NW_UNIT_MultiplyLow64Product` | C (ratcheted) | 0 | runtime |
| `0x0040ebe0` | `FillMemoryWithDwordFromStackArgs` | `src/runtime/Heap.c` | `NW_UNIT_FillMemoryWithDwordFromStackArgs` | C (ratcheted) | 0 | runtime |
| `0x0040ec00` | `InitializeCrtRuntimeAndCtors` | `src/app/AppLifecycle.c` | `NW_UNIT_InitializeCrtRuntimeAndCtors` | C (ratcheted) | 0 | app |
| `0x0040ec4c` | `RunInitTablesAndStoreBlockSizeInWords` | `src/app/AppLifecycle.c` | `NW_UNIT_RunInitTablesAndStoreBlockSizeInWords` | C (ratcheted) | 0 | app |
| `0x0040ecc7` | `DrainHandlerTableAndCallExitHooks` | `src/app/AppLifecycle.c` | `NW_UNIT_DrainHandlerTableAndCallExitHooks` | C (ratcheted) | 0 | app |
| `0x0040ed04` | `DispatchExitHooksAndClearShutdownFlags` | `src/app/AppLifecycle.c` | `NW_UNIT_DispatchExitHooksAndClearShutdownFlags` | C (ratcheted) | 0 | app |
| `0x0040ed9c` | `DispatchPendingSignalHandler` | `src/runtime/win32/Platform.c` | `NW_UNIT_DispatchPendingSignalHandler` | C (ratcheted) | 0 | runtime |
| `0x0040ee90` | `TerminateWithExitStatusCode` | `src/app/win32/AppShutdown.c` | `NW_UNIT_TerminateWithExitStatusCode` | C (ratcheted) | 0 | app |
| `0x0040ef90` | `ClearWritableDataSpanOnce` | `src/app/AppLifecycle.c` | `NW_UNIT_ClearWritableDataSpanOnce` | C (ratcheted) | 0 | app |
| `0x0040efc4` | `InitializeGlobalsAndInstanceTag` | `src/app/win32/WinMain.c` | `NW_UNIT_InitializeGlobalsAndInstanceTag` | C (ratcheted) | 0 | app |
| `0x0040f118` | `LoadWin32sIniVersionPackMajorMinor` | `src/app/win32/WinMain.c` | `NW_UNIT_LoadWin32sIniVersionPackMajorMinor` | C (ratcheted) | 0 | app |
| `0x0040f250` | `VerifyWin32sVersionOrReportStartupError` | `src/app/win32/WinMain.c` | `NW_UNIT_VerifyWin32sVersionOrReportStartupError` | C (ratcheted) | 0 | app |
| `0x0040f290` | `InitializeGlobalThreadBufferOrDie` | `src/runtime/Heap.c` | `NW_UNIT_InitializeGlobalThreadBufferOrDie` | C (ratcheted) | 0 | runtime |
| `0x0040f2be` | `FindCurrentThreadContext` | `src/runtime/win32/Platform.c` | `NW_UNIT_FindCurrentThreadContext` | C (ratcheted) | 0 | runtime |
| `0x0040f370` | `CallRegisteredGlobalHookIfInstalled` | `src/runtime/Hooks.c` | `NW_UNIT_CallRegisteredGlobalHookIfInstalled` | C (ratcheted) | 0 | runtime |
| `0x0040f380` | `ParseCommandLineIntoArgcArgv` | `src/app/AppLifecycle.c` | `NW_UNIT_ParseCommandLineIntoArgcArgv` | C (ratcheted) | 0 | app |
| `0x0040f52c` | `BuildQuotedCommandImage` | `src/app/win32/AppShutdown.c` | `NW_UNIT_BuildQuotedCommandImage` | C (ratcheted) | 0 | app |
| `0x0040f5e8` | `SkipQuotedTokenThenControlCharsToToken` | `src/runtime/Strings.c` | `NW_UNIT_SkipQuotedTokenThenControlCharsToToken` | C (ratcheted) | 0 | runtime |
| `0x0040f644` | `IsTrackedHandleCharDevice` | `src/io/win32/TrackedHandles.c` | `NW_UNIT_IsTrackedHandleCharDevice` | C (ratcheted) | 0 | io |
| `0x0040f664` | `SetThreadContextValueAndClearFlag` | `src/runtime/Slots.c` | `NW_UNIT_SetThreadContextValueAndClearFlag` | C (ratcheted) | 0 | runtime |
| `0x0040f68c` | `TranslateLastErrorToErrno` | `src/runtime/win32/Platform.c` | `NW_UNIT_TranslateLastErrorToErrno` | C (ratcheted) | 0 | runtime |
| `0x0040f730` | `SkipDigitSignColonThenNonalnumToToken` | `src/runtime/Strings.c` | `NW_UNIT_SkipDigitSignColonThenNonalnumToToken` | C (ratcheted) | 0 | runtime |
| `0x0040f7bc` | `ParseHmsTimeStringToSignedSeconds` | `src/runtime/Strings.c` | `NW_UNIT_ParseHmsTimeStringToSignedSeconds` | C (ratcheted) | 0 | runtime |
| `0x0040f870` | `InitializeTimeZoneStandardAndDaylightState` | `src/runtime/Time.c` | `NW_UNIT_InitializeTimeZoneStandardAndDaylightState` | C (ratcheted) | 0 | runtime |
| `0x0040faf0` | `CallRegisteredGlobalHookWithTrue` | `src/runtime/Hooks.c` | `NW_UNIT_CallRegisteredGlobalHookWithTrue` | C (ratcheted) | 0 | runtime |
| `0x0040fb08` | `DisableHookCallback` | `src/runtime/Hooks.c` | `NW_UNIT_DisableHookCallback` | C (ratcheted) | 0 | runtime |
| `0x0040fb20` | `CallHookIfInstalled` | `src/runtime/Hooks.c` | `NW_UNIT_CallHookIfInstalled` | C (ratcheted) | 0 | runtime |
| `0x0040fb30` | `RelinkMaskedBlockHeaderNodes` | `src/runtime/RuntimeSupport.c` | `NW_UNIT_RelinkMaskedBlockHeaderNodes` | C (ratcheted) | 0 | runtime |
| `0x0040fb74` | `AllocatePooledBlock` | `src/runtime/Pool.c` | `NW_UNIT_AllocatePooledBlock` | C (ratcheted) | 0 | runtime |
| `0x0040fd1c` | `EnsurePooledBlockCapacity` | `src/runtime/Pool.c` | `NW_UNIT_EnsurePooledBlockCapacity` | C (ratcheted) | 0 | runtime |
| `0x0040fda8` | `ParseSignedLongFromStringBaseN` | `src/runtime/Strings.c` | `NW_UNIT_ParseSignedLongFromStringBaseN` | C (ratcheted) | 0 | runtime |
| `0x0040ff74` | `RelinkBlockHeaderNode` | `src/runtime/RuntimeSupport.c` | `NW_UNIT_RelinkBlockHeaderNode` | C (ratcheted) | 0 | runtime |
| `0x0040ff88` | `AllocateAndZeroPooledBlockArray` | `src/runtime/Pool.c` | `NW_UNIT_AllocateAndZeroPooledBlockArray` | C (ratcheted) | 0 | runtime |
| `0x0040ffc4` | `FindTableEntryValueByKeyNoCase` | `src/runtime/Tables.c` | `NW_UNIT_FindTableEntryValueByKeyNoCase` | C (ratcheted) | 0 | runtime |
| `0x0041008c` | `FindTableValueByKeyA` | `src/runtime/Tables.c` | `NW_UNIT_FindTableValueByKeyA` | C (ratcheted) | 0 | runtime |
| `0x004100a0` | `AllocateTrackedBlock` | `src/runtime/win32/Platform.c` | `NW_UNIT_AllocateTrackedBlock` | C (ratcheted) | 0 | runtime |
| `0x00410148` | `GetPooledBlockUsableSize` | `src/runtime/Pool.c` | `NW_UNIT_GetPooledBlockUsableSize` | C (ratcheted) | 0 | runtime |
| `0x00410158` | `CompareBoundedAsciiStringsNoCase` | `src/runtime/Strings.c` | `NW_UNIT_CompareBoundedAsciiStringsNoCase` | C (ratcheted) | 0 | runtime |
| `0x004101b4` | `BuildTokenPointerArrayFromGlobalString` | `src/runtime/RuntimeSupport.c` | `NW_UNIT_BuildTokenPointerArrayFromGlobalString` | C (ratcheted) | 0 | runtime |
| `0x00410278` | `DuplicateCStringIntoPooledBlock` | `src/runtime/Pool.c` | `NW_UNIT_DuplicateCStringIntoPooledBlock` | C (ratcheted) | 0 | runtime |
| `0x00411000` | `nw_datablock` | `src/data/nw_datablock.c` | `NW_UNIT_nw_datablock` | C / alias | 0 | data |
| `-` | `AcquireNextFreeSlot` | `src/runtime/Slots.c` | `NW_UNIT_AcquireNextFreeSlot` | C (ratcheted) | 0 | runtime |
| `-` | `AllocateHeapCursorBlock` | `src/runtime/Heap.c` | `NW_UNIT_AllocateHeapCursorBlock` | C (ratcheted) | 0 | runtime |
| `-` | `AppendCStringToDest` | `src/runtime/Strings.c` | `NW_UNIT_AppendCStringToDest` | C (ratcheted) | 0 | runtime |
| `-` | `ApplyGridDeltaPlain` | `src/grid/GridCore.c` | `NW_UNIT_ApplyGridDeltaPlain` | C (ratcheted) | 0 | grid |
| `-` | `ApplyGridDeltaSignedExt` | `src/grid/GridCore.c` | `NW_UNIT_ApplyGridDeltaSignedExt` | C (ratcheted) | 0 | grid |
| `-` | `ApplyGridDeltaSignedLoop` | `src/grid/GridCore.c` | `NW_UNIT_ApplyGridDeltaSignedLoop` | C (ratcheted) | 0 | grid |
| `-` | `ApplyGridDeltaZeroSkip` | `src/grid/GridCore.c` | `NW_UNIT_ApplyGridDeltaZeroSkip` | C (ratcheted) | 0 | grid |
| `-` | `ApplyRollingKeyXorToBuffer` | `src/audio/AudioMix.c` | `NW_UNIT_ApplyRollingKeyXorToBuffer` | C (ratcheted) | 0 | audio |
| `-` | `BuildHlpSiblingPath` | `src/io/win32/PathHelpers.c` | `NW_UNIT_BuildHlpSiblingPath` | C (ratcheted) | 0 | io |
| `-` | `CallOpenTrackedHandleSlotWithFixedModeBits` | `src/io/FileSlots.c` | `NW_UNIT_CallOpenTrackedHandleSlotWithFixedModeBits` | C (ratcheted) | 0 | io |
| `-` | `ClassifyAxisSpanOrientation` | `src/grid/GridMarking.c` | `NW_UNIT_ClassifyAxisSpanOrientation` | C (ratcheted) | 0 | grid |
| `-` | `ClearAllSlotEntries` | `src/audio/AudioState.c` | `NW_UNIT_ClearAllSlotEntries` | C (ratcheted) | 0 | audio |
| `-` | `ClearTopSlotStatusAndRebuildRegionBounds` | `src/runtime/Slots.c` | `NW_UNIT_ClearTopSlotStatusAndRebuildRegionBounds` | C (ratcheted) | 0 | runtime |
| `-` | `ComputeScaledGridOffset` | `src/grid/GridMarking.c` | `NW_UNIT_ComputeScaledGridOffset` | C (ratcheted) | 0 | grid |
| `-` | `ConvertAsciiCharToLowerCase` | `src/runtime/Strings.c` | `NW_UNIT_ConvertAsciiCharToLowerCase` | C (ratcheted) | 0 | runtime |
| `-` | `ConvertAsciiCharToUpperCase` | `src/runtime/Strings.c` | `NW_UNIT_ConvertAsciiCharToUpperCase` | C (ratcheted) | 0 | runtime |
| `-` | `ConvertAsciiStringToUpperCase` | `src/runtime/Strings.c` | `NW_UNIT_ConvertAsciiStringToUpperCase` | C (ratcheted) | 0 | runtime |
| `-` | `ConvertIntegerToString` | `src/runtime/Strings.c` | `NW_UNIT_ConvertIntegerToString` | C (ratcheted) | 0 | runtime |
| `-` | `CopyNulTerminatedString` | `src/runtime/Strings.c` | `NW_UNIT_CopyNulTerminatedString` | C (ratcheted) | 0 | runtime |
| `-` | `CopyRecordsToSlotChain` | `src/runtime/Slots.c` | `NW_UNIT_CopyRecordsToSlotChain` | C (ratcheted) | 0 | runtime |
| `-` | `CopySpriteRowsToDisplayBuffer` | `src/video/Blit.c` | `NW_UNIT_CopySpriteRowsToDisplayBuffer` | C (ratcheted) | 0 | video |
| `-` | `CopyStreamRecordToBuffer` | `src/io/StreamRecords.c` | `NW_UNIT_CopyStreamRecordToBuffer` | C (ratcheted) | 0 | io |
| `-` | `CrashDumpFilter` | `src/diag/win32/CrashHandling.c` | `NW_UNIT_CrashDumpFilter` | C (ratcheted) | 0 | diag |
| `-` | `CreateSlotSemaphore` | `src/runtime/win32/Platform.c` | `NW_UNIT_CreateSlotSemaphore` | C (ratcheted) | 0 | runtime |
| `-` | `CrtInitApiThunks` | `src/app/AppLifecycle.c` | `NW_UNIT_CrtInitApiThunks` | C (ratcheted) | 0 | app |
| `-` | `DispatchMciAudioReset` | `src/audio/AudioState.c` | `NW_UNIT_DispatchMciAudioReset` | C (ratcheted) | 0 | audio |
| `-` | `DispatchMciCommandViaGlobalHandleAndShowError` | `src/audio/win32/MciTransport.c` | `NW_UNIT_DispatchMciCommandViaGlobalHandleAndShowError` | C (ratcheted) | 0 | audio |
| `-` | `DispatchMciPlaybackCommand` | `src/audio/win32/MciTransport.c` | `NW_UNIT_DispatchMciPlaybackCommand` | C (ratcheted) | 0 | audio |
| `-` | `ExecuteScriptInstruction` | `src/vm/OpcodeExecution.c` | `NW_UNIT_ExecuteScriptInstruction` | C (ratcheted) | 0 | vm |
| `-` | `FindLastCharInString` | `src/runtime/Strings.c` | `NW_UNIT_FindLastCharInString` | C (ratcheted) | 0 | runtime |
| `-` | `FlushDeferredErrorString` | `src/ui/Dialogs.c` | `NW_UNIT_FlushDeferredErrorString` | C (ratcheted) | 0 | ui |
| `-` | `FreeArenaSlot` | `src/runtime/Heap.c` | `NW_UNIT_FreeArenaSlot` | C (ratcheted) | 0 | runtime |
| `-` | `FreeHeapBlock` | `src/runtime/Heap.c` | `NW_UNIT_FreeHeapBlock` | C (ratcheted) | 0 | runtime |
| `-` | `Fun0x409891ReturnsTrueStub` | `src/runtime/Hooks.c` | `NW_UNIT_NativeProbeReturnsTrue` | C (ratcheted) | 0 | runtime |
| `-` | `GetHeapArenaBlockEnd` | `src/runtime/Heap.c` | `NW_UNIT_GetHeapArenaBlockEnd` | C (ratcheted) | 0 | runtime |
| `-` | `GetStringLength` | `src/runtime/Strings.c` | `NW_UNIT_GetStringLength` | C (ratcheted) | 0 | runtime |
| `-` | `InitializeAxisSpanFromSavedPoint` | `src/grid/GridSpanStreams.c` | `NW_UNIT_InitializeAxisSpanFromSavedPoint` | C (ratcheted) | 0 | grid |
| `-` | `MoveMemoryRegion` | `src/runtime/Heap.c` | `NW_UNIT_MoveMemoryRegion` | C (ratcheted) | 0 | runtime |
| `-` | `NwCrashFilter` | `src/diag/win32/CrashHandling.c` | `NW_UNIT_NwCrashFilter` | C (ratcheted) | 0 | diag |
| `-` | `NwEntrySehShim` | `src/runtime/win32/Platform.c` | `NW_UNIT_NwEntrySehShim` | C (ratcheted) | 0 | runtime |
| `-` | `NwStateDump` | `src/diag/win32/Diagnostics.c` | `NW_UNIT_NwStateDump` | C (ratcheted) | 0 | diag |
| `-` | `nwtrace` | `src/diag/win32/Diagnostics.c` | `NW_UNIT_nwtrace` | C (ratcheted) | 0 | diag |
| `-` | `OpenTrackedHandleSlot` | `src/io/win32/TrackedHandles.c` | `NW_UNIT_OpenTrackedHandleSlot` | C (ratcheted) | 0 | io |
| `-` | `PublishHeapStateTripleFromTwoSources` | `src/runtime/Heap.c` | `NW_UNIT_PublishHeapStateTripleFromTwoSources` | C (ratcheted) | 0 | runtime |
| `-` | `RebaseInlineImageRecord` | `src/io/FileSlots.c` | `NW_UNIT_RebaseInlineImageRecord` | C (ratcheted) | 0 | io |
| `-` | `ReleaseSlotSemaphore` | `src/runtime/Slots.c` | `NW_UNIT_ReleaseSlotSemaphore` | C (ratcheted) | 0 | runtime |
| `-` | `RelinkListNodeUnderSlotLock` | `src/runtime/RuntimeSupport.c` | `NW_UNIT_RelinkListNodeUnderSlotLock` | C (ratcheted) | 0 | runtime |
| `-` | `ResetStreamCursorState` | `src/vm/OpcodeExecution.c` | `NW_UNIT_ResetStreamCursorState` | C (ratcheted) | 0 | vm |
| `-` | `ResetSubsystemsAndFreeGlobalSlots` | `src/app/AppLifecycle.c` | `NW_UNIT_ResetSubsystemsAndFreeGlobalSlots` | C (ratcheted) | 0 | app |
| `-` | `ReturnConstantOne` | `src/runtime/Slots.c` | `NW_UNIT_ReturnConstantOne` | C (ratcheted) | 0 | runtime |
| `-` | `ReturnOneRetFour` | `src/runtime/Slots.c` | `NW_UNIT_SlotHookReturnOne` | C (ratcheted) | 0 | runtime |
| `-` | `ReturnOneRetFourCaseD45` | `src/runtime/Slots.c` | `NW_UNIT_SlotHookReturnOneAlt` | C (ratcheted) | 0 | runtime |
| `-` | `SelectNextActiveSlot` | `src/runtime/Slots.c` | `NW_UNIT_SelectNextActiveSlot` | C (ratcheted) | 0 | runtime |
| `-` | `SendMciSetOnWithPointerToGlobalWord` | `src/audio/AudioPipeline.c` | `NW_UNIT_SendMciSetOnWithPointerToGlobalWord` | C (ratcheted) | 0 | audio |
| `-` | `SetDefaultGridCellOffsetAndExtent` | `src/grid/GridSpanLifecycle.c` | `NW_UNIT_SetDefaultGridCellOffsetAndExtent` | C (ratcheted) | 0 | grid |
| `-` | `SetHeapRegionBounds` | `src/runtime/Heap.c` | `NW_UNIT_SetHeapRegionBounds` | C (ratcheted) | 0 | runtime |
| `-` | `SetOutOfMemoryFlag` | `src/game/GameState.c` | `NW_UNIT_SetOutOfMemoryFlag` | C (ratcheted) | 0 | game |
| `-` | `SetStateFlagOnce` | `src/game/GameState.c` | `NW_UNIT_SetStateFlagOnce` | C (ratcheted) | 0 | game |
| `-` | `ShowMciErrorMessage` | `src/audio/win32/MciTransport.c` | `NW_UNIT_ShowMciErrorMessage` | C (ratcheted) | 0 | audio |
| `-` | `WindowTableCodeTargets` | `src/generated/LayoutGlue.c` | `NW_UNIT_WindowTableCodeTargets` | C (ratcheted) | 0 | generated |
| `-` | `zNwDiagLoopHook` | `src/diag/win32/Diagnostics.c` | `NW_UNIT_NwDiagLoopHook` | C (ratcheted) | 0 | diag |
| `-` | `zNwTextPad` | `src/generated/LayoutGlue.c` | `NW_UNIT_NwTextPad` | C (ratcheted) | 0 | generated |

## Regenerating

```sh
cd <repository root>
python3 tools/gen_function_map.py
python3 tools/gen_function_map.py --check
```

`--check` fails when the checked-in document differs from the generated one.
Keep `docs/reference/CONVERSION_MAP.tsv`, `docs/reference/FUNCTION_INDEX.tsv`,
`docs/reference/ASM_INVENTORY.tsv`, and `docs/reference/asm_converted.txt`
in the same commit as every source change.
