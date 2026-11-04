# Static PulseAudio client for the portable Linux x86-64 target

These archives are built from PulseAudio 17.0 with the daemon, ALSA,
PipeWire, D-Bus, X11, GLib, systemd, and resampler features disabled.  They
contain the client library only and are linked into the Linux x86-64 binary,
so the executable can keep a static glibc and still play audio through a
running PulseAudio/WSLg server.

Source and build notes:

- `libpulse.a`
- `libpulsecommon-17.0.a`

The headers come from the host PulseAudio development headers
(`/usr/include/pulse`).  No PulseAudio shared object is loaded at runtime.
