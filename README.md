# VoxForge

All-in-one Vocal-Plugin für macOS (VST3 + AU) von **JustCreateit.ch**, funktional angelehnt an Neural DSP Mantra –
plus **automatische Sound-Erkennung** vor dem Autotune.

Läuft in Cubase, Ableton Live, Logic Pro X und jedem anderen VST3/AU-Host. Universal Binary (Apple Silicon + Intel), macOS 11+.

## Funktionen

### Erkennung (läuft immer mit)
| | |
|---|---|
| **Tonart/Skala** | Pitch-Klassen-Histogramm (Melodie) + Chroma (Akkorde) → Krumhansl-Profile, 24 Tonarten. Mit `Key = Auto` / `Scale = Auto` nutzt Autotune direkt die erkannte Tonart. „Re-learn Key“ setzt zurück. |
| **Quelle** | Vocal / Synth / Guitar / Bass / Drums aus Pitch-Stabilität, Voicing, Spektrum, Transienten. |
| **Auto-Analyse** | ~8 s singen/abspielen → setzt Gate-Threshold (Noise-Floor), HP-Filter (tiefster Ton), Mud-Cut, Präsenz/Air, De-Esser, Sculpt-Tilt, Kompressor-Input, Doubler-Charakter (hohe/tiefe Stimme) und fixiert die erkannte Tonart. Optional: passende Modul-Settings für die erkannte Quelle laden. |

### Correction
- **Tune** – Echtzeit-Pitch-Korrektur (TD-PSOLA, formanterhaltend): Key, Scale (14 Skalen + Custom mit 12 Noten-Tasten), Retune-Speed, Tolerance, Amount, Humanize, Transpose, **MIDI-Target** (Lead folgt gehaltener MIDI-Note).
- **Gate** – Threshold, Range, Attack, Release (mit Hysterese + Hold).
- **Sculpt** – adaptiver EQ (7 Bänder gegen Vokal-Zielkurve) + Tilt; im Mixing-Mode zusätzlich dynamische Unterdrückung von Mulm/Härte.
- **De-Esser** – adaptiv, ohne Threshold.
- **EQ** – HP, LP, Low-/High-Shelf, 4 Bell-Bänder, Solo pro Band.
- **Compressor** – Tube (opto, programmabhängig), Stress (schnell, Ratio 2–20:1, Farbe), Multi (3-Band up/down, OTT-Stil).
- **Saturation** – Tape, Tube (asymmetrisch), Digital (Bitcrusher + Downsampling).

### Harmonies
Bis zu 8 Stimmen, Modi **Scale** (Intervall in Skalenstufen), **Chromatic** (Halbtöne), **MIDI** (Stimmen folgen gehaltenen Noten / Akkorden). Level und Pan pro Stimme, Link Pairs, DI Mute, Humanize (Pitch- und Timing-Variation), Width.

### Effects
- **Delay** – Analog, Circular (Ping-Pong/diffus), Tape (Wow/Flutter), Dual (L/R getrennt); Tempo-Sync; Ducker.
- **Reverb** – Hall, Plate, Ambiance (FDN), Pre-Delay, Damping, Low-Cut, Ducker.
- **Modulation** – Chorus (Dual), Flanger, Phaser.
- **Doubler** – simulierter zweiter Take, Male/Female.

### Workflow
14 Factory-Presets (auch als Host-Programme), User-Presets (`~/Library/Audio/Presets/JustCreateit/VoxForge`), Undo/Redo, Tracking-/Mixing-Mode, Input/Output/Mix.

**Latenz:** Mixing ≈ 34 ms (Pitch bis 60 Hz), Tracking ≈ 21 ms (Pitch ab 95 Hz). Wird dem Host gemeldet und kompensiert.

## Installation (Mac)
1. Unter **Actions → letzter Lauf → Artifacts** `VoxForge-macOS` laden (oder unter Releases).
2. `VoxForge-x.y.z-macOS.pkg` per Rechtsklick → **Öffnen** installieren (nicht notarisiert).
   Alternativ die ZIPs entpacken nach `~/Library/Audio/Plug-Ins/VST3` bzw. `.../Components`.
3. Falls macOS blockiert:
   ```bash
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/VoxForge.vst3 /Library/Audio/Plug-Ins/VST3/VoxForge.vst3
   xattr -cr ~/Library/Audio/Plug-Ins/Components/VoxForge.component /Library/Audio/Plug-Ins/Components/VoxForge.component
   ```
4. Cubase: Plug-in-Manager → neu scannen. Logic: AU wird beim Start validiert.

**MIDI (Harmonies/MIDI-Target):** In Cubase eine MIDI-Spur auf den VoxForge-Insert routen, in Logic den Sidechain-MIDI-Eingang (Music-Effect) nutzen, in Ableton MIDI-Spur → „MIDI To“ → Vocal-Spur / VoxForge.

## Selbst bauen
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target VoxForge_VST3 VoxForge_AU -j
./build/VoxForgeTests   # Offline-DSP-Tests
```

## Aufbau
```
Source/dsp/        reines C++ (ohne JUCE): Pitch-Detektor, PSOLA, Analyse, alle Module, Engine
Source/            JUCE-Plugin: Parameter, Presets, Processor, Editor
tests/             Offline-Tests (Tune-Genauigkeit, Harmonie, Tonart, Quelle, Stabilität, CPU)
```
