# Editing & Navigation

Note-editing and navigation features added in BmsTWO. Each entry lists **what it
does**, **how to use it**, and **how it's implemented**.

---

## Copy / Cut / Paste

**What:** Standard clipboard operations for sound notes.

**How to use:** Select notes, then **Edit → Cut/Copy/Paste** (or the usual
shortcuts). Paste drops the notes at the **cursor** (your last clicked
location); when **snap-to-grid** is on, pasted notes snap to the grid.

**How it works:** Notes are serialized to/from an internal clipboard. Paste
computes its anchor from the cursor location rather than the viewport, and runs
the same snap step as manual placement so pasted material lands on the grid.

---

## Fill

**What:** Fills evenly spaced notes between two selected notes.

**How to use:** Select two notes in a lane and choose **Edit → Fill Notes**. The
gap is divided using the current grid.

**How it works:** The span between the two endpoints is subdivided and a note is
inserted at each grid step via the document's note-update action (undoable).

---

## Custom bar division

**What:** Add or remove a bar line at an arbitrary position, letting you split
or merge measures.

**How to use:** Place the cursor and choose **Edit → Toggle Bar Line**.

**How it works:** A bar line is inserted/removed at the cursor location through
an undoable bar-line action; measure geometry is derived from the bar-line map.

---

## BGA editing

**What:** Edit the background-animation layers of a chart — the picture headers
and the **base / layer / poor** event timelines.

**How to use:** Open **View → Views → BGA**. The *Pictures* tab manages image
headers (id → file); the three lane tabs hold timed events (location → picture
id). Add/Remove with the buttons; edit cells inline.

**How it works:** The BGA model lives in the `Document` with an undoable API
(`InsertBgaHeader` / `InsertBgaEvent` / …). The dock (`BgaView`) reflects the
model and is rebuilt on `BgaChanged`. It round-trips through the bmson `bga`
object.

---

## STOP events

**What:** Create and edit STOP events, which pause scrolling for a duration.
Unlike older BmsONE, BmsTWO **honors stops in the timing engine**.

**How to use:** Open **View → Views → Stops**, add a stop (location + duration
in pulses). Playback and WAV export now pause for the stop's duration.

**How it works:** Stops are an undoable map in the `Document`.
`GetAbsoluteTime` / `FromAbsoluteTime` merge-iterate BPM and STOP events, adding
each stop's `duration` pulses' worth of time (at the BPM in effect) so the
real-time clock and rendered audio both pause. Charts without stops are
unaffected.

---

## Long notes

**What:** Two long-note (LN) conveniences:

1. A non-standard **`up`** flag that re-triggers the channel's keysound at the
   LN **release**.
2. A discrete **Toggle Long Note** command.

**How to use:**

* `up`: check **"Sound at release"** in the note editor for a long note.
* Toggle: select notes and press **Ctrl+L** (Edit → Toggle Long Note) to flip
  each between a normal note and a one-beat long note. You can still **Shift +
  drag** an LN edge to resize its length precisely.

**How it works:** `up` is stored as a non-standard `up` field on the note (only
written when true, so standard files are unaffected). Toggle sets each selected
note's `length` to `0` ↔ one beat via the undoable multi-note update.

---

## Keysound / lane lock

**What:** Prevents a note drag from accidentally changing a note's lane
(keysound assignment) — the integrity guarantee relied on in µBMSC.

**How to use:** **Edit → Lock Keysound (Lane Move)** (Alt+Shift+V), or the
checkbox in **Preferences → Edit**. While on, a plain drag won't move notes
between lanes; Shift+drag length edits still work.

**How it works:** A plain note drag only ever reassigns lanes, so when the lock
(an `EditConfig` flag) is set, the drag handler skips the lane-reassignment
step.

---

## MIDI import

**What:** Import a MIDI file's notes onto a channel as slice notes ("leave the
sequence as is").

**How to use:** **Channel → Import MIDI…**, pick a `.mid`. Notes are placed on
the channel; tempo changes are imported as BPM events.

**How it works:** The MIDI track is parsed into timed events mapped onto the
chart's resolution; note-ons become slice notes on the target channel.

---

## Go To

**What:** Jump the view to a specific location.

**How to use:** **Edit → Go To…** (Ctrl+G), choose **Measure**, **Pulse
(tick)**, or **Time (seconds)**, enter a value.

**How it works:** Measure → the bar-line at that index; Time → `FromAbsoluteTime`
(BPM/stop-aware); the view scrolls so the target is centered.

---

## Richer selection

**What:** Faster ways to select and inspect notes.

**How to use:**

* **Select all in a channel:** right-click a channel header → **Select All
  Notes in Channel**.
* **Inspect / sort:** open the **Selection** dock (see
  [Panels & Workflow](Panels-and-Workflow)) to list the current selection,
  sortable by position / time / lane / channel, with double-click to jump.

**How it works:** The channel view selects every one of its note views; the
Selection dock reads the selection from the sequence view (`GetSelectedNotesInfo`)
and refreshes on `SelectionChanged`.
