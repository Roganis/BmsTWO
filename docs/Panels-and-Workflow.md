# Panels & Workflow

BmsTWO adds several dock panels. All are **off by default** — enable them from
**View → Views**.

---

## Statistics

**What:** A read-only summary of the current chart.

**Shows:**

* Total / playable / BGM note counts and keysound-channel count.
* **Length** (mm:ss and seconds).
* **Average NPS** (playable notes ÷ length).
* **Peak NPS** — the most playable notes in any 1-second window.
* **Per-lane distribution** table.

**How it works:** Computed over all channels' notes; NPS uses **absolute
times** (`GetAbsoluteTime`), so BPM changes and stops are honored. Peak NPS is a
sliding-window scan over sorted note times. Recomputes (debounced) on any edit.

---

## Selection

**What:** A sortable list of the currently selected notes.

**Columns:** Pulse, Time (s), Lane, Channel. Click a header to sort
(numeric-aware); **double-click a row to jump** to that note.

**How it works:** Pulls selection info from the sequence view and rebuilds on
`SelectionChanged`. Pairs well with *Select All Notes in Channel*.

---

## Charts (sabun workflow)

**What:** Lists the sibling chart files (`.bmson`, `.bms`, `.bme`, `.bml`,
`.pms`) in the **current song folder**, so you can move between difficulty
variations that share a folder.

**How to use:** Save your chart into a song folder, open **View → Views →
Charts**. The open chart is shown in bold; **double-click another** to switch to
it (you'll be prompted to save unsaved changes first). **Refresh** rescans the
folder.

**How it works:** Scans `Document::GetProjectDirectory()` for chart files;
opening routes through the normal open path (so the save prompt fires).
Refreshes on document / file-path changes.

---

## Keysounds (sabun workflow)

**What:** Manage the shared **keysound list** (the sound channels) from one
panel, instead of per-channel context menus.

**Shows / does:** Each channel with its **note count**; buttons for **Add… /
Remove / Up / Down / Replace…**. Double-click a row to make it the channel
you're editing (shown in bold).

**How it works:** Operations call the existing undoable `Document` APIs
(`InsertNewSoundChannels` / `DestroySoundChannel` / `MoveSoundChannel` /
`SetSourceFile`). The list refreshes (debounced) on edits and tracks the current
channel.

---

## Multi-track sample preview

**What:** Preview several selected channels mixed together, rather than one at a
time.

**How to use:** Select multiple channels and trigger preview; their sources are
mixed for the preview playback.

**How it works:** The preview path gathers the selected channels and mixes their
sample sources.

---

## External viewer focus region

**What:** Loop-free A/B checking of a passage: pick a measure range and the
external viewer plays from its start and stops at its end.

**How to use:** In the External Viewer toolbar, set the *Focus* fields (first and
last measure, inclusive), or select notes in the chart and use *Set Focus Region
from Selection*. *Play Focus Region* (F7) plays it.

**How it works:** If the viewer's *Play Focus Region* argument template is set,
BmsTWO launches it with the region-end variables `$(timeEnd)`, `$(ticksEnd)` and
`$(measureEnd)` (alongside the usual start-position variables) so the viewer can
stop itself. If the template is empty, the *Play from Here* command is used and
the *Stop* command is sent automatically once the region's real (BPM/STOP-aware)
duration has elapsed — accurate up to the viewer's own startup latency.

---

## BGA & Stops docks

These editing docks are documented under
[Editing & Navigation](Editing-and-Navigation) (BGA editing, STOP events).
