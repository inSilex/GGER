# Saves and first-run behavior

The game keeps its high scores and persistent state in `NETWALK0.282`. No save
is shipped with the source or packaged with a release; the game creates one on
first run in the selected writable directory.

## Desktop

Linux and Windows use `NETWALK0.282` beside the executable. If the file is
absent, the game creates a fresh save during startup. It does not overwrite an
existing live save.

To reset a desktop installation, close the game and remove the live file from
the executable directory (or from the build output directory), then launch the
game. The first launch after deletion creates a fresh `NETWALK0.282` and reaches
the menu. Keep a backup if the scores matter.

## Android

The APK carries `overlay.nw` and `gm.sf2` as read-only assets.
`NetWalkActivity` copies them into the app's internal files directory
(`getFilesDir()`) before native startup. The game then runs with a synthetic
module path in that directory, so the overlay, SoundFont, temporary files, and
the `NETWALK0.282` it creates on first run all live in writable internal
storage.

`overlay.nw` and `gm.sf2` are refreshed when their bundled size differs from the
copy in internal storage. To reset an Android installation, use the system
**Clear storage** action for NetWalk (or uninstall the app) and launch again.
Uninstalling also removes the live save; back it up first if the scores matter.

Android writes no routine save log. Asset staging messages and native errors go
to logcat under the `NetWalk` tag.

## iOS

The `.app` bundle is read-only. `platforms/ios/ios_main.c` copies `overlay.nw`
and `gm.sf2` from the bundle into `SDL_GetPrefPath("Gamos", "NetWalk")` (the
app's Documents directory) when the copy is missing, changes size, or is older
than the bundled file. The game then changes into that directory and runs with
a synthetic `NETWALK.EXE` module path, so the overlay, SoundFont, temporary
files, and `NETWALK0.282` all live in writable app storage.

To reset an iOS installation, delete the app (or use the Files app to remove
its Documents data) and launch again. Deleting the app removes the live save;
back it up first if the scores matter. Startup asset messages and native errors
go to stderr, which appears in the device console.

## PS5

The native title probes writable directories in this order:

1. `/data/netwalk/PPSA99877`
2. `/download0/netwalk/PPSA99877`
3. `/app0/userdata/netwalk/PPSA99877`
4. `/tmp/netwalk/PPSA99877`

`/data/netwalk/PPSA99877` is preferred because it is visible over FTP and easy
to back up. `/download0` is title-private storage backed by `downloadDataSize`
in `param.json`. The loader controls which of these locations is visible inside
the title sandbox, so selection happens at runtime.

The first successful selection is reused until it becomes unusable. A failed
save operation calls `platform_save_path_failed()`, which advances the probe to
the next candidate on the following call.

### Migration

When the selected directory has no `NETWALK0.282`, the game first tries to
migrate a legacy save from these directories, in order:

```text
/download0
/data
/app0/userdata
/app0
/tmp
/data/netwalk
/download0/netwalk
/app0/userdata/netwalk
/download0/netwalk/PPSA99877
/app0/userdata/netwalk/PPSA99877
/data/homebrew/PPSA99877
/data/homebrew
```

It also tries the old payload convention of a save beside the module (`leaf`).
The copy is atomic: it writes `<destination>.tmp` and renames it into place. If
the destination already exists, migration is skipped. If no legacy save is
found, the game creates a fresh save in the selected directory.

Do not overwrite a live save when updating a title package.

### Clean-start recovery

If a stale or invalid `NETWALK0.282` is present in the selected directory, the
game will not replace it automatically. To recover:

1. Close the title.
2. Back up the live save if it may contain recoverable scores.
3. Remove `NETWALK0.282` from the selected live directory and from the legacy
   locations that could be migrated.
4. Launch once and confirm a fresh save is created.

Do not delete the only copy of a save without a backup. A fresh save is a
known-good default, not a user-data backup.

### SaveData note

The native-title baseline uses the writable title directories above. The
official PS5 SaveData location (`/savedata0`) requires a successful
`sceSaveDataInitialize3` / `sceSaveDataMount3` sequence, which the tested
fake-native/ShadowMount loader does not provide. If a future loader exposes a
usable SaveData mount, the platform layer can add it to the candidate list;
until then the fallback directories are intentional.

## Logging

Normal runs do not create a save log or any other log file. On PS5, error
messages go to klog/stderr. Optional save-path and boot markers are emitted only
in debug builds (`NW_DEBUG_TOOLS=ON`) and are off in release. The crash handler
may write a crash-only report when the process actually crashes; that is not a
routine log.
