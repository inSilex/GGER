# Default Windows MIDI SoundFont (vendored)

- File: `gm.sf2`
- Size: 3,208,108 bytes
- sha256: `c6d6d1d91995d49187510de5de536054b410456a05065672d331a9afab26c274`
- Upstream: musical-artifacts artifact 713, "Default Windows MIDI Soundfont",
  `https://musical-artifacts.com/artifacts/713` (`/artifacts/713/gm.sf2`).
  The artifact's published `file_hash` is the same sha256.
- Origin: Microsoft Windows `gm.dls`
  (`C:\Windows\System32\drivers\gm.dls`), the Roland GS Wavetable SW Synth
  sample set, `Copyright 1996 Roland Corporation U.S.`
- Conversion: `dls_cnv v0.27y` (recorded in the SF2 `ISFT` chunk).

This is the bank the Windows MIDI mapper feeds to the Microsoft GS Wavetable
Synth: 235 presets covering GM programs, the GS variation banks (1-9), and nine
drum kits (bank 128).

## Licensing

The artifact is marked "gray" and the samples are Roland/Microsoft material;
they are not open source. This copy is vendored for a private, non-commercial
port at the project owner's request. Do not redistribute it commercially or
outside this tree. It is loaded at runtime, not linked, and can be replaced
with any other General MIDI SF2 via `NW_SF2`.
