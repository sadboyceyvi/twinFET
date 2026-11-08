# TwinFET — a FET limiting amplifier in the 1176 tradition (VST3 / AU / Standalone)

Two revisions — **BLACK** (the later low-noise unit) and **BLUE** (the early unit, more saturation) — and three ways of working:

| Mode | BLACK | BLUE |
|---|---|---|
| **1 · FAT** — linear, *inflates* the sound | linear compression plus density ("inflate"), even and odd harmonics, a little extra weight in the low end | the same, hotter: more harmonics, stronger inflation, a slight lift up top |
| **2 · CLEAN** — linear, precise | **no saturation at all** (harmonics < 0.0001 %), bass stays clean even at 7/7 | **high-grade saturation**: class-A stage plus transformers, dominated by the 2nd harmonic, higher harmonics fall away quickly |
| **3 · ANALOG** — non-linear circuit model | model of the low-noise revision | model of the early revision (FET amplifiers, ~5 dB hotter, more 2nd harmonic) |

![Measurements](docs/measurements.png)

---

## Which mode when

**FAT — "make it bigger".** The inflation stage lifts low- and mid-level detail while leaving the peaks alone, so a part sounds denser and louder at the same peak level, and the harder you drive it the thicker it gets. Its detector is feed-forward, so it reacts the same way on any material: fast, predictable results. Reach for it when a vocal, bass, snare or guitar has to sit on top of a busy mix. It is not transparent, so it is usually too much on a full mix.

**CLEAN — "control without character".** The same thresholds, ratios and times on an exact curve. BLACK adds nothing at all, which makes it right for dialogue and podcasts, acoustic instruments, a track that already has its vibe, or a chain that gets its colour elsewhere. BLUE adds a touch of expensive-sounding 2nd-harmonic sheen without grit: vocals and acoustic sources that want a little polish.

**ANALOG — "the real unit".** The detector listens to its own output, so it is smoother and more program-dependent, release slows down after long compression, fast settings make low notes ripple and growl, and gain staging changes the tone (INPUT hits the preamp and transformer, OUTPUT drives the line amp). ALL buttons is the full-on smash. Pick it when you want the feel of the hardware and set it by ear: vocals, bass, drums, room mics. It uses a bit more CPU.

**Revision.** BLACK is cleaner, quieter and tighter, the modern standard. BLUE has more harmonics (mostly 2nd), a slightly slower attack and a juicier, more vintage sound.

### Starting points

| Source / goal | Mode · revision | Settings |
|---|---|---|
| Lead vocal, pop or rock, up front | ANALOG · BLUE, or FAT · BLUE | 4:1, attack 3, release 6–7, 5–8 dB GR |
| Vocal that must stay natural (ballad, acoustic, podcast, voice-over) | CLEAN · BLACK (CLEAN · BLUE for a little polish) | 4:1, attack 3–4, release 5, 3–5 dB GR |
| Rap or aggressive vocal | FAT · BLUE, or ANALOG with ALL and MIX 30–50 % | attack 3, release 7 |
| Bass (DI or amp) | ANALOG · BLACK for punch, ANALOG · BLUE for growl, FAT for weight | 4:1 or 8:1, attack 2–3, release 5–6, 4–8 dB GR |
| Kick, snare | FAT · BLACK or ANALOG · BLACK | 4:1–8:1, attack 1–2 (slow, lets the transient through), release 6–7 |
| Room mics, parallel drum smash | ANALOG · BLUE, ALL | attack 3–4, release 7, 10–20 dB GR, MIX 20–50 % |
| Drum bus glue | CLEAN · BLACK or FAT · BLACK | 4:1, attack 1–2, release 6, 2–4 dB GR, SC HPF 80–120 Hz |
| Acoustic guitar, piano, strings | CLEAN · BLACK or BLUE | 4:1, attack 2–3, release 4–5, 2–4 dB GR |
| Electric guitars | FAT · BLUE or ANALOG · BLUE | 4:1–8:1, attack 3, release 5–6 |
| Catching peaks transparently | CLEAN · BLACK, 20:1 | attack 6–7, release 6–7, only the loudest peaks |
| Colour without compression | ANALOG · BLUE or FAT with COMP OFF | push INPUT and OUTPUT to taste |

A few rules of thumb:

- **Attack 1 is the slowest (800 µs), 7 the fastest (20 µs).** Low numbers keep the punch of drums and plucked notes; high numbers flatten peaks.
- **Release 7 is the fastest (50 ms).** Fast release adds energy and loudness (and, in ANALOG, grit on bass); 3–5 is smoother.
- **SC HPF** at 80–150 Hz stops kick and bass from making the compressor pump on buses and full-range sources.
- **Parallel compression:** squash hard (ALL, 15–20 dB GR) and bring it in with MIX at 20–40 %.
- **Gain staging in ANALOG:** set INPUT for the amount of gain reduction, then OUTPUT. For more grit, push OUTPUT and pull the channel fader down afterwards. In FAT and CLEAN, OUTPUT is just clean make-up gain.
- **NOISE** adds hiss and hum. Leave it off unless you want that vibe, because it builds up over many tracks.
- **Oversampling:** leave it on AUTO; it follows the project's sample rate. Use 1× only for low-latency tracking; 8× and 16× do not sound different.
- **Mix bus:** a FET limiter is not the usual bus compressor. If you try it: CLEAN · BLACK, 4:1, attack 1–2, release 5–6, 1–2 dB GR, SC HPF 100 Hz, or MIX 30–50 %.

---

## What ANALOG models

- **Feedback sidechain:** the detector sees the signal *after* the FET, as in the hardware. The loop is solved implicitly on every sample, so it is stable at any setting.
- **Rectifier and hold capacitor without extra smoothing:** at attack/release 7/7 low notes distort (≈5 % THD at 40 Hz with ~9 dB GR), the familiar dirt of a fast FET limiter. Slower settings are much cleaner.
- **Program-dependent release:** fast after a short hit, a slow tail after long compression.
- **Release linear in decibels** (the capacitor discharge drives the FET), not a plain exponential.
- **FET distortion** that grows with gain reduction; stronger in BLUE, several times stronger with ALL.
- **Preamp** (BLACK: low-noise bipolar; BLUE: FET, ~5 dB hotter), **class-A line amplifier** (the OUTPUT knob really drives it, as on the hardware: more output, more dirt), **input and output transformers** (flux saturation: the low end saturates first).
- **ALL buttons:** ~16:1 and beyond with a plateau, shifted operating points, attack lag on transients (hits "explode"), far more distortion.
- Thresholds depend on the ratio (higher ratio, higher threshold): 4:1 −18 dBFS, 8:1 −13.6, 12:1 −12, 20:1 −9.8, ALL −20 (with INPUT at 0 dB; 0 VU = −18 dBFS). The spacing comes from a bench measurement of a 1176LN (onset of compression for each ratio).
- Attack 20 µs…800 µs, release 50 ms…1.1 s; **7 is the fastest**, as on the hardware.
- Optional hiss and hum (NOISE button).

### Against the 1176LN data sheet (BLACK)

The BLACK model is calibrated to the published figures of the LN version; BLUE, the earlier revision, is deliberately dirtier and noisier.

| 1176LN data sheet | ANALOG · BLACK (measured) | ANALOG · BLUE |
|---|---|---|
| THD with limiting < 0.5 % (50 Hz…15 kHz, release 1.1 s) | 0.19–0.39 % (4:1 and 20:1, ~4–9 dB GR) | 1.4–2.1 % |
| +22 dBm output at THD ≤ 0.5 % | 0.36 % at +21 dBm (1 kHz) | ≈2 % |
| S/N > 81 dB (input at threshold, NOISE on) | 82 dB | 74 dB |
| Response ±1 dB 20 Hz…20 kHz | −1.0 dB at 20 Hz, −0.5 dB at 20 kHz | −1.2 / −0.6 dB |
| Attack 20…800 µs, release 50 ms…1.1 s | within measurement accuracy | the same |

Here 0 dBFS = +22 dBm and 0 VU (+4 dBm) = −18 dBFS. The data sheet gives no per-ratio thresholds or compression curves; the spacing of the thresholds comes from a lab measurement of a 1176LN (University of Rochester, 2016), the shape of the curves from the circuit description (the threshold is set by the detector bias).

In FAT and CLEAN the detector is **linear** (feed-forward, exact curve, same thresholds and times), so these modes are predictable and behave the same on any material.

## Controls

| Control | What it does |
|---|---|
| INPUT | how hard the signal is driven into the fixed threshold (−24…+36 dB) |
| OUTPUT | output level (−24…+24 dB); in ANALOG it also drives the output amplifier |
| ATTACK / RELEASE | 1…7, 7 is the fastest |
| RATIO | 4, 8, 12, 20, ALL |
| METER: GR / +4 / +8 / INPUT | what the VU meter shows (+4: 0 VU = −18 dBFS, +8: 0 VU = −14 dBFS) |
| **ADVANCED** — click any screw on the unit's edge | the rack-ear screws open the unit: below the faceplate you look inside it, with the settings below mounted on small boards among the circuit; any of these screws (or the board's corner screws) closes it again |
| ↳ MODE | FAT · CLEAN · ANALOG (switches without clicks, 30 ms crossfade); the current mode also shows on the faceplate (lamp and name) |
| ↳ REVISION | BLACK / BLUE |
| ↳ OVERSAMPLING | AUTO (follows the project's sample rate: 4× at 44.1/48 kHz, 2× at 88.2/96 kHz, none at 176.4/192 kHz) · 1× · 2× · 4× · 8× · 16× |
| ↳ DISPLAYS: VALUES | shows or hides all the numeric read-outs, leaving just the knobs and the needle |
| ↳ DISPLAYS: POP-UPS | turns the tool-tips and the value shown while turning a knob on or off |
| ↳ MIX | parallel compression; the dry signal runs through the same filters, so the two stay time-aligned |
| ↳ SC HPF | high-pass filter in the sidechain (Off…500 Hz) |
| ↳ LINK | stereo link of the detectors |
| ↳ COMP OFF | circuit colour only, no compression (like the attack-off trick) |
| ↳ NOISE | hardware hiss and hum (ANALOG only) |

Every knob and button is labelled on the panel. Under the knobs sit numeric read-outs, at the top the GAIN REDUCTION and OUTPUT LEVEL displays. Hover over anything for a tool-tip; while a knob turns, its exact value (for example "3.0 (234 µs)") floats above it. Double-click a knob to reset it.

### Window size, displays and pop-ups

- The window **always opens compact (70 %)**. Drag the bottom-right corner to make it larger, up to 200 %. An instance you enlarged reopens at its own size (saved with the project); new instances still open compact.
- **VALUES** and whether the unit is open are saved with the project.
- **POP-UPS OFF** hides the tool-tips and the value pop-up. The choice is saved with the project and also becomes the default for new windows (`%APPDATA%\TwinFET\TwinFET.settings` on Windows, `~/Library/Application Support/TwinFET` on macOS).

### Oversampling

Saturation and inflation create harmonics that would otherwise fold back into the audible range. Measured on a hot 9.1 kHz tone (worst non-harmonic component, 48 kHz):

| | 1× | 2× | 4× | 8× | 16× |
|---|---|---|---|---|---|
| aliasing (FAT·BLUE / ANALOG·BLUE) | −29 / −33 dB | −93 dB | −102 / −105 dB | −102 / −106 dB | −101 / −106 dB |
| latency, samples | 0 | 3 | 4 | 5 | 5 |
| CPU (one core, stereo) | ≈0.5 % | ≈1 % | ≈2–3 % | ≈4–5 % | ≈9 % |

So 4× (and AUTO at 44.1/48 kHz) is already practically perfect; 8× and 16× are there for peace of mind. Switching mutes the sound for a moment while the engine is rebuilt and the new latency is reported to the DAW.

### The look

![Interface](docs/ui_preview.png)

- **The revision changes the unit itself:** BLACK is black anodised brushed aluminium with light legends, BLUE silver-blue anodised aluminium with dark legends.
- **The mode changes the lighting:** FAT amber, CLEAN cool white (even light from two lamps in the corners of the scale); in **ANALOG you see the bulbs themselves** (two along the bottom of the window and one behind the middle of the scale), each throwing its own pool of light, and the meter glass is scratched; the jewel turns red.
- **ANALOG is a unit that has seen some sessions:** scratches, paint worn around the knobs and along the rack ears, marks from the rack screws, rubbed legends, grime in the corners, yellowed meter paper, worn buttons and lenses, polished knob rims. In FAT and CLEAN the unit is new.
- **Different buttons:** RATIO white caps, METER and OVERSAMPLING black caps (unlit; a pressed cap sinks and darkens), MODE / REVISION / switches lit lamps under frosted lenses, all in thin polished bezels.
- **Deep black phenolic knobs** with a polished band and spun-aluminium inserts; a final colour grade gives the whole panel rich blacks, bright highlights and vivid lamps.
- **Metal details:** chrome rack handles, chrome bezels around buttons and displays, polished knob bands, screws at the corners of the meter.
- **The opened unit (ADVANCED):** the screws in the rack ears glint under the mouse and look backed out while the unit is open. Inside: zinc chassis walls (gold chromate on the early BLUE unit), the main board (green glass-epoxy on BLACK, brown phenolic with tinned tracks on BLUE) with resistors, capacitors, transistors, the FET in its metal can, the input and output transformers, a ribbon cable and the wiring loom. It is all kept dark and matte, as it would be inside a case, so only the settings on their small black boards — and the edge screws — read as things to click.
- **Numeric read-outs** are seven-segment LED modules behind tinted glass in a metal frame (the unlit "8.8.8" segments show through; a little dust and hairline scratches on the glass, more in ANALOG, where some segments look tired): attack in microseconds, release in milliseconds, input/output in dB, SC HPF in Hz or "oFF"; units are printed on the glass. When the output is silent, OUTPUT LEVEL shows "---".
- **Pop-ups** are smoked-glass plates in a thin polished frame: the title set like the panel's silkscreen, the explanation in a clean UI face.
- Textures (brushed metal, frosted lenses, meter paper) are generated for the real pixel density of the screen, so they stay sharp on 4K and Retina displays. At small window sizes the panel and the meter are rendered 2–3× supersampled and scaled down, so fine print, scales and textures stay crisp.
- **Switching is instant.** The panel artwork is rendered once per look and window size and kept (shared by every TwinFET in the session, bounded in memory; several open windows never push out each other's artwork). While the window sits idle, the looks you may switch to next are rendered in short slices (around 10 ms), so changing revision or mode, opening the unit or reopening the window shows up at once. In very large windows (more than about 2250 physical pixels wide) nothing is rendered in the background, because the texture work there would make the meter stutter: a look is drawn the first time you pick it (a fraction of a second) and kept from then on, with room for at least two complete looks. None of this touches the audio thread: no latency, no extra DSP load.

---

## Getting a ready-made VST3

### Option A — nothing to install (GitHub Actions)

1. Create a repository on github.com (it can be private).
2. Upload **everything** in the archive, including the hidden `.github` folder (easiest with GitHub Desktop or `git push`; in the web interface drag all the folders in).
3. Open the **Actions** tab — the build starts by itself (10–20 min).
4. At the bottom of the run page download the **TwinFET-Windows** artifact (or macOS / Linux).

Inside **TwinFET-Windows**:

| File | What it is |
|---|---|
| `TwinFET-Setup-Windows.exe` | **installer**, puts the plug-in where it belongs (recommended) |
| `TwinFET.vst3` | **a single file (DLL)** for manual installation: drop it into the VST3 folder |
| `Standalone\TwinFET.exe` | standalone application, works without a DAW |
| `README.md` | this guide |

### Option B — locally on Windows, one click

1. Install the free [Visual Studio Community](https://visualstudio.microsoft.com/downloads/) and tick **Desktop development with C++** in its installer.
2. Optionally install [Inno Setup 6](https://jrsoftware.org/isdl.php) to get the installer as well.
3. Double-click **`build_windows.bat`** in the project folder. It finds Visual Studio, downloads JUCE once (no git needed), builds the plug-in and offers to install it into `C:\Program Files\Common Files\VST3`.

The result lands in `dist`: `TwinFET.vst3` (a single file), `Standalone\TwinFET.exe` and, with Inno Setup, `TwinFET-Setup-Windows.exe`. The first build takes 5–15 minutes. If Windows warns about a file from the internet, click **More info → Run anyway**.

By hand: `cmake -B build -A x64` and `cmake --build build --config Release`; the plug-in is `build\TwinFET_artefacts\Release\VST3\TwinFET.vst3`.

### macOS

Xcode Command Line Tools and CMake:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

You get VST3, AU (`TwinFET.component`) and Standalone.

### Linux

Packages: `libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev`, then as on macOS (without `CMAKE_OSX_ARCHITECTURES`).

If you already have JUCE locally: `cmake -B build -DJUCE_PATH=/path/to/JUCE`.

## Installation

### Windows

**Installer (easiest):** run `TwinFET-Setup-Windows.exe` → Next → Install. The plug-in goes to `C:\Program Files\Common Files\VST3\TwinFET.vst3` (where every DAW looks), optionally with the standalone application and a Start-menu entry. It uninstalls like any other program (Settings → Apps). Running a newer installer simply replaces the old version.

The installer is not code-signed, so Windows SmartScreen may say "Windows protected your PC" — click **More info → Run anyway**.

**By hand, a single file:** copy `TwinFET.vst3` (one DLL file) into `C:\Program Files\Common Files\VST3\` (Windows asks for administrator rights — allow it).

Then rescan plug-ins in your DAW (FL Studio: *Options → Manage plugins → Find installed plugins*; Ableton: *Options → Preferences → Plug-ins → Rescan*; Reaper: *Options → Preferences → VST → Re-scan*). Close the DAW before updating the plug-in, otherwise Windows will not let you replace the file.

If you build it yourself (option B): `cmake --build build --config Release`, then the installer with [Inno Setup 6.3+](https://jrsoftware.org/isinfo.php): `ISCC /DArtefacts=build\TwinFET_artefacts\Release installer.iss` (output in `dist\`).

### macOS / Linux

- **macOS**: `TwinFET.vst3` → `~/Library/Audio/Plug-Ins/VST3/`, `TwinFET.component` → `~/Library/Audio/Plug-Ins/Components/`. The build is not signed, so run once:
  `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/TwinFET.vst3 ~/Library/Audio/Plug-Ins/Components/TwinFET.component`
- **Linux**: the `TwinFET.vst3` folder → `~/.vst3/`.

---

## Measurements (the plug-in itself, 48 kHz, 4× oversampling)

THD:

| | 0 VU, 4:1 | ~9 dB GR | 40 Hz at 7/7 | ALL, ~13 dB GR |
|---|---|---|---|---|
| FAT · BLACK | 0.60 % | 1.13 % | 1.38 % | 1.40 % |
| FAT · BLUE | 1.05 % | 1.93 % | 2.06 % | 2.40 % |
| CLEAN · BLACK | < 0.0001 % | < 0.0001 % | < 0.0001 % | < 0.0001 % |
| CLEAN · BLUE | 0.34 % (H2) | 0.55 % (H2) | 0.68 % | 0.30 % |
| ANALOG · BLACK | 0.05 % | 0.25 % | 5.1 % | 1.0 % |
| ANALOG · BLUE | 0.49 % | 1.56 % (H2) | 5.9 % | 3.9 % |

- Attack and release times match the published 1176 figures (20 µs…800 µs, 50 ms…1.1 s) within measurement accuracy; after long compression the release is 1.2–2.8 times slower (by design, as in the hardware).
- Aliasing (4×): worst non-harmonic component −95 dBc (ANALOG on a hot 9 kHz tone with OUTPUT drive), below −135 dBc in FAT and CLEAN. The table for every oversampling setting is above.
- Small-signal response: CLEAN flat; ANALOG −1 dB at 20 Hz and −0.5 dB at 20 kHz; FAT adds +0.4 dB (BLACK) / +0.8 dB (BLUE) in the low end.
- Latency (AUTO): 5 samples at 44.1 kHz, 4 at 48 kHz, 2 at 88.2/96 kHz (reported to the DAW for compensation; bypass is delayed by the same amount).
- CPU: ≈1.5–2.5 % of one core per stereo instance (48 kHz).
- Self-test: random automation, silence, +20 dB noise, DC, link, click-free mode switching — all pass (`twinfet_measure robust`).

No comparison with a real 1176 or with other plug-ins has been made — only the plug-in itself is measured here. The model is built from the circuit topology and the published UREI / Universal Audio data.

## Tweaking the sound

All the "sound" constants live in `dsp/TwinFetEngine.h`:

- `struct Voicing` — thresholds, ratios, knees, ALL behaviour, detector hold, time calibration;
- `applyConfig()` — the colour of each mode and revision: `fatE0/fatE1/fatE2` (amount of inflation), `premium` (CLEAN·BLUE saturation), `fetD2/fetD3` (FET distortion), `preAmp`/`lineAmp` (drive and asymmetry of the amplifiers), transformer saturation, noise.

The look is in `Source/TwinPanel.h`; `kGradeContrast`, `kGradeLift` and `kGradeSaturation` set the final colour grade.

Offline measurements without JUCE:

```sh
cmake -B build -DTWINFET_BUILD_TESTS=ON && cmake --build build --target twinfet_measure
./build/twinfet_measure all results
python3 tests/analyze.py results docs/measurements.png
```

## Layout

```
dsp/            dependency-free DSP core (oversampling, detectors, saturation, 3 modes × 2 revisions)
Source/         JUCE wrapper: parameters, state, interface
                TwinPanel.h — all the panel artwork (the same code draws the plug-in and the offline preview)
                FontData.h  — embedded Big Shoulders font (SIL OFL 1.1)
fonts/          the font file and its licence
tests/          measurement bench (C++), plots (Python), ui_preview.cpp — renders the interface to PNG
installer.iss   Windows installer (Inno Setup), built automatically by GitHub Actions
build_windows.bat  one-click build on Windows
.github/        automatic Windows / macOS / Linux builds plus the installer
```

## Licences and names

The plug-in is built on JUCE 8 (AGPLv3 or a commercial JUCE licence) — fine for personal use; if you want to distribute or sell it, check the JUCE licence terms. "1176", "UREI" and "Universal Audio" are trademarks of their owners and are mentioned here only to describe the sound. TwinFET contains no code from, or captures of, those products; the panel design and the name are its own (no third-party logos or names on the panel). The Big Shoulders font is distributed under the SIL Open Font License 1.1 (`fonts/BigShoulders-OFL.txt`).
