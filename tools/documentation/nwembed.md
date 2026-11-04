# nwembed.py

`nwembed.py` appends an external `overlay.nw` to a desktop NetWalk executable
so the executable no longer needs a separate overlay file. The runtime already
falls back to its own module when neither `overlay.nw` nor `overlay.bin` is
present, and that fallback reads the `=VS=` trailer from the end of the file.

Embedding works for the Linux and Windows executables. It is not used for the
PS5 title: appending to an FSELF `eboot.bin` needs separate packaging and
hardware validation.

## Requirements

Python 3 and the standard library. No Pillow or other package is needed.

## Usage

```sh
# Copy first if you want to keep the unmodified executable.
cp build/linux-x86_64/NETWALK.EXE build/linux-x86_64/NETWALK-embedded.EXE
python3 tools/nwembed.py \
    --exe build/linux-x86_64/NETWALK-embedded.EXE \
    --overlay assets/overlay.nw
```

The output is modified in place and keeps the executable's permission bits.
The tool reports the pre-append executable size, the overlay's source `dbase`,
the raw stream length, and the final file size.

`--force` appends another copy even when the executable already appears to end
in a valid embedded trailer. Do not use it unless you deliberately want a
second copy; the newest trailer is the one the runtime reads.

## Layout and the dbase rewrite

The compact layout is:

```text
[executable][raw overlay][38-byte trailer]
```

`raw overlay` starts at the external overlay's `dbase`, so the `0x12800` zero
pad in `overlay.nw` is not copied. The trailer's `dbase` field is rewritten to
the executable size before the append. The runtime then resolves the index
entries from the new location.

The layout preserves an overlay's high-resolution section: the section remains
immediately before the trailer, and the tool does not rewrite or validate the
overlay contents. The overlay must remain the final data in the file.

Do not run `strip` or another PE/ELF rewriter after embedding. The trailer must
remain the last 38 bytes. A detached signature over the final file is fine, but
Windows Authenticode appends its certificate table behind the image, which
would put data after the trailer and break the runtime fallback. Embedded
Windows builds are therefore intended to remain unsigned unless the loader is
changed.

## Runtime precedence

An external `overlay.nw` or `overlay.bin` beside the executable still takes
precedence over the embedded copy. This allows a local overlay override. To
verify the embedded copy, run the executable from a directory with no external
overlay.

The embedded data is only the VM overlay. `gm.sf2` remains an external
supporting file; saves are created at runtime.

## Docker release wrapper

`docker/build-releases.sh --embed-overlay` invokes this tool while staging the
Linux and Windows release directories. Those directories then contain the
embedded executables and `gm.sf2`, but no `overlay.nw`. The PS5 title, when
built, still receives an external `overlay.nw`.
