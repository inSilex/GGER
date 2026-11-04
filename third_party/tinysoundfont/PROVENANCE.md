# TinySoundFont (vendored)

- Upstream: https://github.com/schellingb/TinySoundFont
- Version: v0.9
- File: `tsf.h` (single-header MIT SF2 synthesizer)
- sha256: `70d55963c98f60ebb81518eaa1f25d46888d5180eb5f5289fd6b74ffc177d197`
- License: MIT (see `LICENSES/TinySoundFont-MIT.txt`)

The header is included exactly once, from `src/audio/NwMidiSynth.c`, with
`TSF_IMPLEMENTATION` and `TSF_NO_STDIO`. It is compiled into the game and
statically linked; there is no synth DLL.
