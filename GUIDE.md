# Raster Damage - User Guide

Raster Damage is a **scanline filter for audio**. It cuts the signal into short time slices ("frames", the audio version of video scanlines) and damages each slice independently - the way a corrupted video frame tears, drops lines, smears pixels and loses sync.

Every control belongs to one of those four metaphors, so you can think visually: *"I want the picture to tear a bit, lose every fourth line, and roll over on the downbeat."*

## The interface

**Top bar:** preset dropdown (categories, then *My presets*), ◀ ▶ step through every preset, **Save** (writes a `.rdpreset` to your preset folder), **Load**, **Init** (defaults), **Randomize** (rolls new values for the whole sound, within musical ranges; Mix, Output, Damage and tempo settings are left alone), **Undo** (restores the sound from before the last Randomize / RND / RST), **Bypass**.

**Layout follows the signal flow:** SCAN (cut into frames) > TEAR > SMEAR > SCANLINES > SYNC LOSS > OUTPUT. Each section has a mode dropdown at the top, its knobs underneath, and two small buttons in its header: **RND** randomises just that section, **RST** resets just that section. The big **DAMAGE** knob scales everything. **FIRE NOW** (hold) triggers a sync-loss burst on demand.

**Hint bar (bottom):** hover any control to see its full name, exact value and a one-line explanation.

Mouse: drag a knob, **Shift** for fine control, **double-click** to reset, wheel to nudge.

**Sound-design workflow:** start from a preset close to what you want, hit RND on one section at a time (Undo if you hate it), then refine by hand and Save. Lock a good seed, then change Slice or Loop to find rhythmic variations.

### Presets
Selecting a preset sets every control. Changing a control marks it *modified*; Save keeps your version as a user preset. User presets are text files (`format=1`, `name=`, `Parameter Name=value`); unknown keys are ignored and missing ones fall back to defaults. Folders: Windows `Documents\Raster Damage\Presets`, Linux `~/.local/share/Raster Damage/Presets`, macOS `~/Library/Application Support/Raster Damage/Presets`.

## 1. How it works (30 seconds)

```
audio in -> [delay line] -> TEAR -> SMEAR -> SCANLINES -> sync-loss snow -> BANDWIDTH -> DRIVE -> MIX -> out
                              ^ every slice decides for itself what happens to it ^
```

1. **Scan** decides where slice boundaries are (fixed milliseconds, or locked to the beat).
2. For each slice, a deterministic random roll (from **Seed**) decides whether it is torn, smeared, dropped, or part of a sync-loss burst. **Damage** scales all of those chances at once.
3. Because those decisions depend only on *Seed + slice number*, the result is **identical no matter the buffer size, and identical every time you bounce**. Set **Frame Loop** and the pattern repeats like a loop you can play with.

The plugin adds a fixed 2-sample latency (reported to your host) so the dry signal stays phase-aligned.

## 2. Parameter reference

### Scan - how the audio is cut into frames

| Parameter | Range | Default | What it does |
|---|---|---|---|
| **Slice Time** | 0.5 - 250 | 12.00 ms | Length of one "frame". Everything else happens per slice. 0.5 ms is a buzzy raster, 250 ms is big chunky blocks. |
| **Tempo Sync** | 0 - 1 | Off  | Lock slice length to the host tempo and the frame grid to the song position, so damage lands on the beat. |
| **Slice Division** | 0 - 9 | 1/16  | Slice length when Tempo Sync is on (1/64 ... 1/2, with triplets). |
| **Slice Jitter** | 0 - 100 | 0.0 % | Randomly lengthens/shortens slices so the raster stops feeling like a grid. |
| **Frame Loop** | 0 - 64 | Off slices | Repeat the damage pattern every N slices (Off = never repeats). Makes corruption rhythmic and playable. |
| **Seed** | 0 - 999 | 1  | Which corruption you get. Same seed + same position = same damage, every time you render. |
| **Damage** | 0 - 100 | 70.0 % | Master probability for all damage. 0 = untouched signal, 100 = every damage rule is always eligible. |
| **Input Follow** | -100 - 100 | 0 % | Ties Damage to the input level. + : louder = more damage (hit-triggered). - : quieter = more damage (corruption in the gaps). |
| **Interlace** | 0 - 3 | Off  | Which slices are allowed to be damaged: Even, Odd, or Weave (left channel gets even slices, right gets odd). |

### Tear - horizontal tearing (short delay offsets between slices)

| Parameter | Range | Default | What it does |
|---|---|---|---|
| **Tear Amount** | 0 - 100 | 60.0 % | How many slices get a delay offset. |
| **Tear Depth** | 0 - 100 | 8.00 ms | Maximum delay offset of a torn slice (Squared response: fine control for small tears). |
| **Tear Shape** | 0 - 4 | Random  | Random offsets, Stair (4 steps), Skew (ramps across slices like a slanted image), Sine (wobble), Zigzag (alternating). |
| **Tear Slew** | 0 - 100 | 20.0 % | How smoothly the offset glides between slices. 0 = hard tears (clicky, digital), 100 = tape-like wobble. |
| **Ghosting** | 0 - 90 | 0.0 % | Feeds the torn signal back into the delay line - echoes of the damaged frame, like a TV ghost. |

### Scanlines - missing lines (periodic dropouts)

| Parameter | Range | Default | What it does |
|---|---|---|---|
| **Line Period** | 2 - 32 | 4 lines | Scanline pattern length in slices (e.g. 4 = every 4th slice is a candidate). |
| **Missing Lines** | 0 - 32 | 1 lines | How many slices at the start of each period are dropped. |
| **Line Phase** | 0 - 31 | 0 lines | Shifts the dropout pattern against the slice grid. |
| **Line Chaos** | 0 - 100 | 0.0 % | Replaces the strict pattern with random dropouts at the same average density. |
| **Dropout Fill** | 0 - 4 | Black  | What replaces a missing line: Black (silence), Snow (noise), Previous Line (the audio of one slice ago), Negative (polarity flip), Hold (freezes the last value). |
| **Dropout Depth** | 0 - 100 | 100.0 % | How completely the line is removed (100 = fully, lower = partly damaged). |
| **Line Edge** | 0 - 10 | 0.50 ms | Fade time at the edges of a dropout. 0 = hard digital cut, a little = no click. |

### Smear - pixel smearing (short-window averaging / sample-and-hold)

| Parameter | Range | Default | What it does |
|---|---|---|---|
| **Smear Amount** | 0 - 100 | 40.0 % | How many slices are smeared. |
| **Smear Mode** | 0 - 3 | Average  | Average (box blur), Sample-Hold (stair-step), Bleed (peaks smear out slowly), Crunch (hold + average). |
| **Smear Size** | 0.02 - 20 | 0.400 ms | Width of the smear window. Tiny = grit, large = blur. |
| **Color Depth** | 2 - 16 | 16 bit | Bit depth applied to smeared slices (2-16 bit). Low = posterised, crunchy audio. |

### Sync Loss - controlled bursts of timing disruption

| Parameter | Range | Default | What it does |
|---|---|---|---|
| **Sync Loss Rate** | 0 - 100 | 10.0 % | How often a sync-loss burst starts. |
| **Burst Length** | 1 - 64 | 4 slices | How many slices a burst lasts. |
| **Sync Intensity** | 0 - 100 | 60.0 % | How violent the burst is. |
| **Sync Style** | 0 - 3 | Jitter  | Jitter (timing shakes), Roll (picture rolls vertically = audio slides through the delay line), Stutter (repeats a piece of audio), Chaos (picks one at random per burst). |
| **Sync Loss Now** | 0 - 1 | Off  | Switch on to fire one burst right now, on demand (automate it or map it to a pad). |

### Output

| Parameter | Range | Default | What it does |
|---|---|---|---|
| **Bandwidth** | 200 - 20000 | 20.00 k Hz | Low-pass filter on the damaged signal - cheap video-bandwidth loss. |
| **Static Level** | -60 - 0 | -30.0 dB | Level of the snow/noise used by Snow dropouts and sync-loss bursts. -60 = off. |
| **Burn-In Drive** | 0 - 24 | 0.0 dB | Soft saturation at the end of the chain (gain-compensated). |
| **Field Split** | 0 - 100 | 30.0 % | How often the right channel gets its own, independent damage (stereo decorrelation). |
| **Mix** | 0 - 100 | 100.0 % | Dry/wet. The dry path is delay-matched, so partial mixes stay phase-coherent. |
| **Output** | -24 - 12 | 0.0 dB | Output gain. |
| **Bypass** | 0 - 1 | Off  | Host bypass. |

## 3. Presets (77 factory presets)

Pick them from the preset dropdown at the top of the plugin (or your host's program selector). Every preset is 100% wet. Names starting with *Guitar / Vocal / Drums / Bass / Synth / Mix Bus* are tuned for that source; **Scenes** are whole-sound character pieces; **Rhythm** presets lock to the host tempo.

**Basics**

- 0. Init (Clean Signal)

**Broken video**

- 1. Dropped Frames
- 2. Tracking Error VHS
- 3. Dead Pixel Row
- 4. Scanline Comb
- 5. Interlace Flutter
- 6. CRT Warm-Up
- 7. Rolling Shutter
- 8. Vertical Hold Lost

**Corrupted media**

- 9. Corrupt MP3
- 10. Scratched CD
- 11. Warped Cassette
- 12. Eaten Tape
- 13. Floppy Disk Read Error
- 14. Dial-Up Handshake
- 15. Bad Sector

**Guitar**

- 16. Guitar: Fraying Amp
- 17. Guitar: Shorting Cable
- 18. Guitar: Tracking Chorus
- 19. Guitar: Glitch Chug Gate
- 20. Guitar: Ghost Solo
- 21. Guitar: Digital Rust

**Vocals**

- 22. Vocal: Broadcast Interference
- 23. Vocal: Lost Transmission
- 24. Vocal: Dead Satellite
- 25. Vocal: Pixel Throat
- 26. Vocal: Haunted Answering Machine

**Drums**

- 27. Drums: Frame Skipper
- 28. Drums: Stutter Roll
- 29. Drums: Sample-Hold Crush
- 30. Drums: Tape Tear Break
- 31. Drums: Dead Frame Gate
- 32. Drums: Overdriven Scan

**Sync loss / extremes**

- 33. Sync Loss: Light Jitter
- 34. Sync Loss: Roll Over
- 35. Sync Loss: Total Collapse
- 36. Sync Loss: Heartbeat
- 37. Sync Loss: Beat-Locked Roll

**Textures**

- 38. Input Follow: Hit Triggered
- 39. Input Follow: Quiet Corruption
- 40. Frozen Frame Loop
- 41. Static Bath
- 42. Heat Haze
- 43. Datamosh Smear
- 44. Glass Shatter Fragments
- 45. Subtle Wear
- 46. Mono Sum Murder

**Scenes**

- 47. VHS Rental Tape
- 48. Cassette Warble Dream
- 49. Security Camera Loop
- 50. Satellite Link Failing
- 51. Haunted Broadcast
- 52. Datamosh Frames
- 53. Blue Screen Stutter
- 54. Fax Handshake
- 55. Dying Hard Drive
- 56. Cathedral of Static

**Rhythm (tempo sync)**

- 57. Sixteenth Stutter Grid
- 58. Triplet Gate Tear
- 59. Half-Time Dropout
- 60. Breakbeat Shredder
- 61. Rolling Sync Pulse
- 62. Gated Ghosts
- 63. Static Between Notes

**Instruments**

- 64. Guitar: Lo-Fi Ambient
- 65. Guitar: Djent Digital Chop
- 66. Guitar: Broken Pedal
- 67. Vocal: Radio Dispatch
- 68. Vocal: Glitch Choir
- 69. Vocal: Whispering Datamosh
- 70. Drums: Snare Shatter
- 71. Drums: Kick Brick
- 72. Drums: Hat Static Rain
- 73. Bass: Sub Tear
- 74. Synth: Pad Disintegration
- 75. Mix Bus: Worn Glue
- 76. Mix Bus: Tape Eater

Selecting a preset sets every parameter; tweak from there and use **Save** to keep your version.

## 4. Recipes

**Guitar - "dying amp"**: start at *Guitar: Fraying Amp*. Raise **Line Chaos** for random crackle, lower **Mix** to 70-80 to keep the pick attack clean.

**Vocals - broadcast interference**: *Vocal: Broadcast Interference*. Keep **Slice Time** at 10-20 ms so words stay intelligible; use **Bandwidth** around 6-7 kHz for the radio feel. Automate **Damage** up in choruses.

**Drums - frame-skipping groove**: *Drums: Frame Skipper*. With **Tempo Sync** on and **Slice Division** 1/16, the dropouts land on the grid. Set **Frame Loop** to 16 (one bar of 1/16ths) so the same fills return every bar; change **Seed** for a different bar.

**Hit-triggered damage**: *Input Follow: Hit Triggered* - the corruption blooms on loud hits and clears in between. Negative **Input Follow** does the opposite.

**Beat-locked sync loss**: *Sync Loss: Beat-Locked Roll*. Automate **Sync Loss Now** from a pad/MIDI-mapped button for a manual "oh no" moment.

## 5. Tips

- **Click-free vs digital**: *Line Edge* and *Tear Slew* at 0 give hard digital glitches; add a little for something that works on full mixes.
- **Seed hunting**: with *Frame Loop* on, step **Seed** until you like the damage pattern - it is reproducible.
- **Stereo**: *Field Split* at 0 damages both channels identically (mono-compatible); higher values decorrelate them. *Interlace: Weave* sends alternate slices to left/right.
- **Sub-bass**: sub-bass material may get clicks from hard dropouts; raise *Line Edge* to 1-3 ms.
- **CPU**: the engine uses well under 1% of one core on a modern CPU, so stack as many as you like.

## 6. Offline preview tool (no DAW needed)

`rd_render` (built alongside the plugin) runs audio through the same engine:

```
rd_render --list
rd_render --demo demo.wav --preset "Tracking Error VHS"
rd_render in.wav out.wav --preset 12 --set "Tear Amount=90" --set Mix=80 --bpm 140
```

## 7. Troubleshooting

| Symptom | Fix |
|---|---|
| Plugin not listed | Copy the **whole** `RasterDamage.vst3` folder to `C:\Program Files\Common Files\VST3\`, then rescan. Make sure your DAW scans that folder. |
| "Not a valid plugin" | Hosts that only support VST2 cannot load VST3. Use a VST3-capable host. |
| No effect heard | Check **Damage** > 0, **Mix** > 0 and **Bypass** is off. |
| Different result each play | Make sure **Seed** is not being automated and (if using Tempo Sync) the transport is playing. |

## Plugin not showing up in Reaper / another host

1. The folder must be copied as a whole: `C:\Program Files\Common Files\VST3\RasterDamage.vst3\Contents\x86_64-win\RasterDamage.vst3`. A loose `RasterDamage.vst3.dll` is **not** scanned (hosts look for the `.vst3` extension).
2. In Reaper: Preferences > Plug-ins > VST, confirm that VST3 folder is listed, then **Clear cache/re-scan** (Reaper remembers a failed scan and skips the plugin until you clear it).
3. Run `check_plugin.bat` from the `dist` folder. It loads the DLL the way a host does and prints the exact Windows error if it cannot (missing dependency, blocked file, wrong bitness). Windows may also mark a downloaded DLL as blocked: right-click the .zip before extracting > Properties > Unblock.
4. Reaper must be the 64-bit version (the plugin is x86_64 only).
