# Grouped BGM Lanes (pre-cut samples)

For songs whose samples are **already cut** (often hundreds of one-shot
keysound channels), the normal one-column-per-channel layout becomes
unusably wide. The **Grouped BGM Lanes** view reorganizes those channels into
compact, name-grouped background lanes so you can read the whole song and chart
from it.

**Enable:** **View → Grouped BGM Lanes (pre-cut samples)** (persisted).

## What it does

* **Name grouping.** Channels are grouped by the leading part of their name
  before a trailing number — `piano_03`, `piano_04` → one **piano** lane;
  `key_0348` → **key**. Each group is one background lane.
* **Overlap sub-lanes.** Notes within a group that overlap in time are split
  into the minimum number of side-by-side sub-lanes so nothing collides; a
  group widens only as much as it needs to.
* **Editor-native look.** Same dark background, bar/beat/fine gridlines, and
  per-channel colors (with custom-color overrides) as the rest of the editor;
  the current sample's notes get the white selected outline. A label strip at
  the top names each group.
* **Live.** The grouping rebuilds automatically as you edit, and the right-edge
  minimap (whole-chart scroll overview) stays available.

## Working in this view

* **Click a note** → selects that sample as the current channel **and auditions
  it** (so you hear the keysound without opening the channel tab).
* **Write a key note** (write mode, on the playfield) → keys **the sample the
  music actually has at that time**, not the last-selected one. Because a channel
  holds one note per time, the existing sample note is *moved* onto the key lane
  — the music is preserved, only what triggers it changes. As you move down the
  timeline each note grabs the correct sample automatically.
* **Delete a keyed note** → **un-keys** it: the sample returns to the BGM lane
  rather than being deleted, so deleting a chart note never deletes the music.
* **No Shift needed.** In this view a freshly placed note defaults to a
  *new-sample* trigger (normally Shift); Shift instead requests a continuation.
* **Moving notes** between lanes is still done in **Edit mode** (write mode no
  longer shows a misleading move cursor).

## Pairs with

* The **[Keysounds panel](Panels-and-Workflow)** — list/add/remove/reorder the
  shared keysound list and jump to a sample.
* **Channel Find** (Channel → Find) — filter the channels by name.

## Notes & limitations

* Sample matching on write is by the **exact (snapped) time**; if a sample isn't
  on the write grid it falls back to the selected channel.
* Group/lane widths are fixed (14 px per sub-lane) in this first version.
