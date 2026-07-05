# Implementation Notes

For contributors. A map of where the fork's features live and the conventions
they follow.

## Architecture at a glance

* **`Document`** (`src/document/`) owns the chart model: info, bar lines, BPM /
  STOP events, BGA, and `SoundChannel`s. All edits go through **undoable
  `EditAction`s** (`History.*`, `HistoryUtil.h`); `EditValueAction<T>` is the
  workhorse for single-value changes.
* **`MainWindow`** wires actions, menus, toolbars, and the dock widgets, and
  owns the active `Document`.
* **`SequenceView`** (`src/sequence_view/`) is the central editor: rendering,
  the cursor, selection, and the mouse state machine
  (`SequenceViewEditMode.cpp`).
* **Dock views** (`InfoView`, `ChannelInfoView`, `BgaView`, `StopView`,
  `StatsView`, `SelectionView`, `ChartsView`, `KeysoundView`) each get a
  `ReplaceDocument(Document*)` call when the document changes.

### Dock / document-swap convention

`MainWindow::ReplaceDocument` **deletes the old document first**, then calls each
view's `ReplaceDocument`. Views must therefore **never** disconnect from the old
(now-freed) document — only bind to the new one. (A use-after-free here once
caused a Linux-only crash on load; the fix and the rule are why every view
follows the `InfoView` pattern.)

## Where features live

| Feature | Key files |
| --- | --- |
| STOP timing | `Document::GetAbsoluteTime` / `FromAbsoluteTime` |
| BGA model / dock | `Document` BGA API, `BgaView.*` |
| STOP dock | `StopView.*` |
| LN `up` field | `SoundNote` (`SoundChannel.h/.cpp`) |
| Sample-stop `x_stop` field | `SoundNote` (`SoundChannel.h`), `Bmson100.cpp` |
| Keysound lane lock | `EditConfig`, `SequenceViewEditMode.cpp` |
| Autosave / recovery | `MainWindow` (`SetupAutosave` … `CheckForCrashRecovery`), `EditConfig` |
| Configurable shortcuts | `util/ShortcutManager.*`, `PrefShortcuts.*` |
| Per-channel color | `SoundChannel` `x_color`, `SequenceView::SetNoteColor` |
| Statistics / Selection / Charts / Keysounds | `StatsView.* / SelectionView.* / ChartsView.* / KeysoundView.*` |
| Theme | `util/Theme.*` |

## Non-standard bmson fields

The fork's bmson extensions (`up`, `x_stop`, `x_color`) are specified in
**[bmson Extensions](Bmson-Extensions)** — keep that page authoritative when
adding or changing a field. They round-trip via each object's preserved
`bmsonFields`; standard players ignore unknown fields, so files stay
compatible.

## Qt 5 / Qt 6

The code builds on **both** Qt 5 and Qt 6. Portability shims live in
`util/KeySeq.h` (modifier+key) and the audio/PCM layer; Qt 6 additionally pulls
in `core5compat`. Avoid APIs removed in Qt 6.

## Crash handling

`util/CrashHandler.*` installs a dependency-free handler early in `main()` that
writes a stack trace to the temp location on a fatal signal.

## Testing

`tools/fuzz_history.cpp` is a **headless** harness (not part of the app build)
that drives `Document` / `EditHistory` under AddressSanitizer + UBSan: mass
edits, undo/redo storms, resolution conversion, BGA/STOP, the LN `up`
round-trip, STOP timing math, the autosave **snapshot round-trip**, the
**ShortcutManager**, and the **LN toggle** length update. Build a sanitized
target and run via `tools/run_fuzz_history.sh <build-dir>`.

```sh
# example: sanitized build + harness
qmake src/bmstwo.pro CONFIG+=debug CONFIG+=sanitizer CONFIG+=sanitize_address CONFIG+=sanitize_undefined
make
tools/run_fuzz_history.sh .
```

## CI / release

GitHub Actions builds runnable **Windows (Qt 6 / MinGW)** and **Linux (Qt 6
AppImage)** artifacts. The release workflow triggers on a pushed `v*` tag (or
manual dispatch with a tag input) and creates a **draft** GitHub Release.
