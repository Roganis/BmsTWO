# BmsONE (BmsTWO fork)

BmsONE is an editor for [bmson](https://bmson.nekokan.dyndns.info/) files, the
music-game format derived from BMS.

This repository is **BmsTWO**, a continuation fork. Its lineage is:

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

## Building

This software is built with [Qt](https://www.qt.io/) (Qt 5; the project also
compiles under Qt 6) and C++17, using qmake.

```sh
qmake src/bmsone.pro
make
```

Or open `src/bmsone.pro` in Qt Creator.

libogg and libvorbis are bundled in the source code (under `src/libogg` and
`src/libvorbis`), so no external audio libraries are required.

## License

BmsONE is distributed under the **GNU General Public License v3.0**. See the
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
