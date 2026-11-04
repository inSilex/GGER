/*
 * nwgroups.h -- umbrella for the source-group interfaces.
 *
 * Include this header when a caller wants the exported declarations for the
 * whole tree.  Individual group headers can be included on their own.
 */

#ifndef NWGROUPS_H
#define NWGROUPS_H

#include "AppLifecycle.h"
#include "AppShutdown.h"
#include "WinMain.h"
#include "AudioMix.h"
#include "AudioPipeline.h"
#include "AudioState.h"
#include "MciTransport.h"
#include "UtNotify.h"
#include "WaveOutMidi.h"
#include "nw_datablock.h"
#include "CrashHandling.h"
#include "Diagnostics.h"
#include "GameHandlers.h"
#include "GameState.h"
#include "WindowTables.h"
#include "LayoutGlue.h"
#include "GridCore.h"
#include "GridMapping.h"
#include "GridMarking.h"
#include "GridQueries.h"
#include "GridSpanLifecycle.h"
#include "GridSpanStreams.h"
#include "InputFields.h"
#include "InputRouting.h"
#include "WindowMessages.h"
#include "FileSlots.h"
#include "SaveFiles.h"
#include "StreamRecords.h"
#include "PathHelpers.h"
#include "StreamConversions.h"
#include "TrackedHandles.h"
#include "Heap.h"
#include "Hooks.h"
#include "PackedFields.h"
#include "Pool.h"
#include "RuntimeSupport.h"
#include "Slots.h"
#include "Strings.h"
#include "Tables.h"
#include "Time.h"
#include "Platform.h"
#include "Dialogs.h"
#include "WindowChrome.h"
#include "Blit.h"
#include "Palette.h"
#include "TextRender.h"
#include "VideoAssets.h"
#include "MoviePlayback.h"
#include "VideoGdi.h"
#include "OpcodeExecution.h"

#endif /* NWGROUPS_H */
