# BmsTWO

BmsTWO is an editor for [bmson](https://bmson.nekokan.dyndns.info/) files, the
music-game format derived from BMS.

BmsTWO is a continuation fork of BmsONE. Its lineage is:

* [`excln/BmsONE`](https://github.com/excln/BmsONE) — original upstream
  (last binary release: beta 0.2.1, Sept 2017).
* [`djkero/BmsONE`](https://github.com/djkero/BmsONE) — 2020 fork that added
  **EZ2 (EZ2ON / EZ2AC) mode support** and skins for both player sides, the
  5K ONLY / 14K ANDROMEDA modes, and an updated `mode_hint` spec. This is the
  base of the present fork.
* **This fork** continues from djkero with compiler/deprecation cleanup,
  ASan-detected leak fixes, and ongoing stability work.

## EZ2 support

The EZ2 mode support inherited from djkero is **kept as-is** in this fork:

* The default `mode_hint` on a new file is **5K STD**.
* EZ2 skins are bundled, with player-side selection under
  **Toolbar → View → Scratch → Player X**.
* EZ2 modes ignore `judge_rank` and `total` (EZ2 handles timing/life its own
  way — see `EZ2_INFO.txt` for the judgement/life delta reference).
* `EZ2_TEMPLATE.bmson` is provided as a starting template.

## What's new in BmsTWO

Everything below was added on top of the `djkero/BmsONE` base. End-user
documentation for these lives in the [project wiki](../../wiki) (source under
[`docs/`](docs/)).

### Editing & navigation

* **Copy / cut / paste** of sound notes, pasting at the cursor and snapping to
  the grid when snap is on.
* **Fill notes** evenly between two selected notes.
* **Custom bar division** — toggle a bar line at the cursor to split/merge
  measures.
* **BGA editing** — a dockable editor for BGA headers and the base / layer /
  poor event lanes.
* **STOP events** — editable, and now honored by the time/playback engine
  (playback and WAV export pause at stops).
* **Long notes** — a one-key **Toggle Long Note** command, plus an optional
  keysound re-trigger at an LN's release (the `up` extension — see
  [Format](#format) below).
* **Keysound / lane lock** — stop note drags from accidentally reassigning a
  note's lane (keysound).
* **MIDI import** — drop a MIDI file onto a channel to lay out slice notes.
* **Go To** — jump to a measure, pulse, or time.
* **Richer selection** — select every note in a channel, and inspect/sort the
  current selection in a dedicated panel.

### Panels & workflow

* **Classic BMS Mode** — a usable layout *and* charting flow for pre-cut-sample
  songs (hundreds of keysound channels): channels are merged into name-grouped,
  overlap-packed background lanes; clicking a note auditions its sample, writing
  a key note keys the sample the music actually has at that time (or slices an
  ongoing one), deleting a keyed note un-keys it, and right-clicking splits a
  sample (the remainder returns to the background) — so charting never alters
  the music. **Reset All Notes to BGM** un-charts back to a blank canvas of
  samples.
* **Statistics** dock — note counts, average / peak NPS, per-lane distribution.
* **Charts** dock — list and switch between the charts in a song folder (sabun
  workflow).
* **Keysounds** panel — add / remove / reorder / replace the shared keysound
  list in one place.
* **Selection** dock — sortable list of the current selection; double-click to
  jump.
* **Autosave & crash recovery** — periodic snapshots of unsaved work, offered
  back after an abnormal exit.
* **Configurable keyboard shortcuts** — rebind any menu command, with conflict
  detection, in Preferences.
* **Multi-track sample preview** — preview several selected channels mixed
  together.

### Appearance

* **Modern dark theme** (toggle in Preferences; *Classic* restores the original
  look): Fusion palette, tiered gridlines, alternating measure shading, rounded
  notes / long-note capsules, per-channel color-coding with custom overrides,
  monospace numeric fields, and crisp high-DPI rendering.

### Format

* **bmson extensions** — a few optional non-standard fields (`up` LN-release
  re-trigger, `x_stop` keysound cutoff, `x_color` channel color) that degrade
  gracefully in other players. See [`docs/Bmson-Extensions.md`](docs/Bmson-Extensions.md).

### Under the hood

* **Qt 6 port**, a dependency-free **crash handler**,
  numerous crash/UB/leak fixes, smart-pointer ownership refactors, faster bulk
  channel operations, a headless ASan/UBSan fuzz harness, and CI that produces
  Windows and Linux builds.

## Building

This software is built with [Qt](https://www.qt.io/) 6 and C++17, using qmake
(the `qt5compat` module is required for `core5compat`).

```sh
qmake6 src/bmstwo.pro
make
```

Or open `src/bmstwo.pro` in Qt Creator.

libogg and libvorbis are bundled in the source code (under `src/libogg` and
`src/libvorbis`), so no external audio libraries are required.

## AI development & transparency

In the interest of transparency, the BmsTWO fork was developed as a
human-directed, AI-assisted project:

* **The maintainer ([Roganis](https://github.com/Roganis))** owns and directs
  the project: setting goals and priorities, making the design and scope
  decisions, reviewing the work, running and validating the application
  (interactive/GUI testing), and owning releases and overall responsibility for
  the project.
* **An AI coding assistant (Anthropic's Claude, via Claude Code)** did the
  hands-on implementation under that direction: writing the feature code, bug
  fixes, refactors, and this documentation, and verifying each change with
  compiler builds and the headless AddressSanitizer/UBSan test harness.

In short: the human decides *what* gets built and confirms it works in practice;
the AI does most of the *writing* and automated checking. Automated verification
(builds, sanitizer/fuzz harness) does **not** replace real-world use — please
report anything that misbehaves.

## License

BmsTWO is distributed under the **GNU General Public License v3.0**. See the
[`LICENSE`](LICENSE) file for the full text.

Third-party components:

* **Qt** is used under the GNU LGPL.
* **libogg** and **libvorbis** are bundled and used under the Xiph.org
  BSD-style license. See the [`COPYING`](COPYING) directory for their license
  texts.

## Acknowledgments

* Original BmsONE by excln; EZ2 mode support by djkero.
* Qt — Copyright (C) The Qt Company Ltd and other contributors.
* libogg / libvorbis — Copyright (c) 2002–2015 Xiph.org Foundation,
  available at [Xiph.org](https://www.xiph.org/).
