# Baseline data-window input

`window_snapshot.s` is the relocatable copy of the original 32-bit data
window. It is checked-in generator input for the fixed-layout build, not a
linked assembly translation unit.

`tools/nw_portableize.py` reads it at configure time to resolve the
data-window contents and emits the native pointer side table used by the
portable C tree. The file keeps the original byte layout, including the
absolute-pointer slots that become symbolic references to recovered code.

The snapshot is deterministic for a given original image. If the recovered
layout changes, regenerate or update it as part of the same source change;
do not treat it as a second runtime data source. It is not copied into the
build output and is not needed beside the executable.
