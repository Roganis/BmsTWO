# BmsTWO bmson extensions

BmsTWO adds a few **non-standard fields** to the bmson format. They live inside
standard bmson structures, are all **optional**, and a player that doesn't
implement them can safely ignore them — the file still loads and plays, just
without the extra behavior (graceful fallback). Two affect playback (`up`,
`x_stop`); one is editor-cosmetic (`x_color`).

All tick/pulse values use the chart's `info.resolution` (pulses per quarter
note), same as standard bmson `y`/`l`. Where a duration must become real time,
convert pulses → seconds using the chart's BPM timeline **including bmson `bpm`
and `stop` events** (i.e., the same mapping you already use to schedule notes).

---

## 1. `up` — re-trigger the keysound at a long note's release

- **Where:** a note object inside `sound_channels[].notes[]`.
- **Type:** boolean. **Default:** `false`.
- **Applies when:** the note is a long note (`l > 0`).
- **Meaning:** in addition to the normal trigger at the note head (`y`), the
  channel's wav is **triggered again at the note's release**, i.e. at tick
  `y + l`, played from the start of the wav. (A normal long note plays/holds the
  head sound only; `up` adds a fresh hit on release.)
- **Reference implementation:** treat a note with `up:true` and `l>0` as if there
  were an additional normal note (`c:false`) on the **same channel** at tick
  `y + l`. If a real note already exists there, do nothing (the real note wins).

```json
{ "x": 1, "y": 480, "l": 240, "up": true }
```
→ plays the channel's wav at y=480 (held to 720) **and** re-plays it from the
start at y=720.

---

## 2. `x_stop` — cut a keysound's playback at a point

- **Where:** a note object inside `sound_channels[].notes[]`.
- **Type:** integer (pulses). **Default:** `0` (no cutoff).
- **Meaning:** the wav triggered by this note **stops (silence) `x_stop` pulses
  after the note**, i.e. it is cut at tick `y + x_stop`. The sample plays from
  `y` and ends at the **earliest** of: its natural wav end, the channel being
  retriggered by a later note, or `y + x_stop`. This lets one note play only a
  portion of its sample without needing a following note to bound it.
- **Reference implementation:** when you start the wav for this note, also
  schedule it to stop at real-time `T(y + x_stop)` where `T(pulse)` is your
  pulse→seconds mapping (BPM/stop aware). Equivalently, limit the number of wav
  frames to `(T(y + x_stop) - T(y)) * wavSampleRate`.
- **Notes:** bmson has no native keysound cutoff, which is why this exists. It is
  typically placed on the note that starts the sample (a `c:false` note). Players
  that ignore it play the sample to its natural end.
- **Editor status:** BmsTWO's own charting flow no longer *writes* this field —
  splitting a sample now inserts a native continuation note instead, which keeps
  the total audio unchanged. `x_stop` remains fully supported for playback,
  display, and round-trip of files that contain it.

```json
{ "x": 2, "y": 0, "l": 0, "x_stop": 960 }
```
→ the wav plays from y=0 and is silenced at y=960 (if it hasn't ended sooner).

---

## 3. `x_color` — per-channel display color (editor only)

- **Where:** a channel object inside `sound_channels[]`.
- **Type:** string, `"#RRGGBB"`. **Default:** none.
- **Meaning:** the color BmsTWO uses to draw that channel/keysound's notes. Purely
  cosmetic — **no playback effect**. A game can ignore it entirely (it only
  matters in an editor/visualizer).

```json
{ "name": "piano_03.wav", "x_color": "#ff8800", "notes": [ ... ] }
```

---

## Summary

| Field | On | Type | Affects audio? | Fallback if ignored |
|-------|----|------|----------------|---------------------|
| `up` | note | bool | yes (extra hit at `y+l`) | no release re-trigger |
| `x_stop` | note | int (pulses) | yes (cuts at `y+x_stop`) | sample plays full |
| `x_color` | sound_channel | string `#RRGGBB` | no (cosmetic) | default color |

These are forward-compatible: they only *add* optional keys, never change the
meaning of standard bmson fields.
