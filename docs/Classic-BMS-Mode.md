# Classic BMS Mode

*(formerly “Grouped BGM Lanes”)*

For songs whose samples are **already cut** (often hundreds of one-shot
keysound channels) — the classic BMS way of charting — the normal
one-column-per-channel layout becomes unusably wide. **Classic BMS Mode**
reorganizes those channels into compact, name-grouped background lanes so you
can read the whole song and chart from it.

**Enable:** **View → Classic BMS Mode** (persisted).

## What it does

* **Name grouping.** Channels are grouped by the leading part of their name
  before a trailing number — `piano_03`, `piano_04` → one **piano** lane;
  `key_0348` → **key**. Each group is one background lane.
* **Overlap sub-lanes.** Notes within a group that overlap in time are split
  into the minimum number of side-by-side sub-lanes so nothing collides; a
  group widens only as much as it needs to.
* **Editor-native look.** Same dark background, bar/beat/fine gridlines,
  per-channel colors (with custom-color overrides), time cursor line, and
  hover outlines as the rest of the editor. A label strip at the top names
  each group; the current sample's group is tinted and its label highlighted,
  just like the current channel's column in the standard view.
* **Live.** The grouping rebuilds automatically as you edit, and the right-edge
  minimap (whole-chart scroll overview) stays available.

## Working in this view

* **Hover** → the standard time cursor and status-bar readout (measure / beat /
  real time); hovering a note outlines it and shows its info, exactly like the
  standard lanes.
* **Click a note** → selects that sample as the current channel **and auditions
  it** (so you hear the keysound without opening the channel tab).
* **Right-click a lane** → the same channel context menu as a standard column:
  preview, move left/right, select notes, set/clear color, delete.
* **Write a key note** (write mode, on the playfield) → keys **the sample the
  music actually has at that time**, not the last-selected one. Because a channel
  holds one note per time, the existing sample note is *moved* onto the key lane
  — the music is preserved, only what triggers it changes. As you move down the
  timeline each note grabs the correct sample automatically.
* **Delete a keyed note** → **un-keys** it: the sample returns to the BGM lane
  rather than being deleted, so deleting a chart note never deletes the music.
* **Right-click on the playfield** (write mode, empty spot) → **splits** the
  sounding sample at that tick: the remainder returns to the background as a
  continuation note, so a keyed note above the split only triggers the first
  part — the total audio stays identical. Right-click an existing split to heal
  it. (Right-clicking a legacy `x_stop` marker clears the stop, un-silencing
  the remainder.)
* **Draw a long note** → the sample is **split automatically at the LN's
  release**: the long note keys the head slice, the remainder returns to the
  background — no manual right-click needed. (Skipped if another note already
  sits at the release tick, if the sample has ended by then, or for `up`
  notes.) Right-click the split to heal it.
* **Reading the lanes:** every continuation/cut note carries a **red cross** at
  its trigger tick — the previous sound is cut there and the marker under the
  cross is its remainder. A marker *without* a cross is a genuinely new
  keysound.
* **No Shift needed.** In this view a freshly placed note defaults to a
  *new-sample* trigger (normally Shift); Shift instead requests a continuation.
* **Snap to Sample.** The magnet (**Snap to Grid**, Ctrl+T) changes function in
  this mode: instead of locking the cursor to BPM fractions, it locks to the
  **nearest sample in the currently selected lane** (the highlighted name-group).
  Select a sample in a group, then chart on the playfield — the cursor jumps
  from sample to sample, so every written note lands exactly on a keysound even
  when the song is off-grid. With no sample selected (or an empty group) the
  magnet falls back to the normal grid.
* **Moving notes** between lanes is still done in **Edit mode** (write mode no
  longer shows a misleading move cursor).

## Pairs with

* The **[Keysounds panel](Panels-and-Workflow)** — list/add/remove/reorder the
  shared keysound list and jump to a sample.
* **Channel Find** (Channel → Find) — filter the channels by name.

## Notes & limitations

* Sample matching on write is by the **exact (snapped) time**. With the magnet
  on, snapping targets the selected lane's samples directly, so off-grid
  keysounds are matched exactly; only with the magnet off (or nothing selected)
  can a click miss a sample and fall back to the sounding channel.
* Group/lane widths are fixed (14 px per sub-lane) in this first version.
