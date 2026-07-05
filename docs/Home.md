# BmsTWO Wiki

BmsTWO is a continuation fork of **BmsONE**, an editor for
[bmson](https://bmson.nekokan.dyndns.info/) charts. This wiki documents the
features added in the BmsTWO fork — what they do, how to use them, and a short
note on how each is implemented.

> These pages are maintained in the repository under [`docs/`](https://github.com/Roganis/BmsTWO/tree/master/docs)
> and mirrored to the GitHub wiki. Edit the files in `docs/` and the wiki is
> updated from them.

## Contents

* **[Editing & Navigation](Editing-and-Navigation)** — copy/paste, fill, bar
  division, BGA, STOP, long notes, keysound lock, MIDI import, Go To, richer
  selection.
* **[Panels & Workflow](Panels-and-Workflow)** — Statistics, Charts, Keysounds,
  and Selection docks; the sabun (chart-variation) workflow.
* **[Classic BMS Mode](Classic-BMS-Mode)** — a usable layout and charting flow
  for pre-cut-sample songs with hundreds of keysound channels.
* **[bmson Extensions](Bmson-Extensions)** — the non-standard fields BmsTWO adds
  (`up`, `x_stop`, `x_color`) and how to implement them in a player.
* **[Autosave & Shortcuts](Autosave-and-Shortcuts)** — autosave / crash
  recovery and configurable keyboard shortcuts.
* **[Appearance](Appearance)** — the modern dark theme and rendering options.
* **[Implementation Notes](Implementation-Notes)** — architecture, the Qt 6
  port, crash handling, and testing (for contributors).

## A note on docks

Most of the new panels are **dock widgets**. They are **off by default**; enable
any of them from **View → Views**, then drag/tab/float them like any Qt dock.
Your layout is remembered between sessions.

## How this fork is built (transparency)

BmsTWO is developed as a **human-directed, AI-assisted** project: the maintainer
sets the goals, makes the design decisions, and validates the app in practice,
while an AI coding assistant (Anthropic's Claude, via Claude Code) does most of
the implementation and automated checking. See the
[README](https://github.com/Roganis/BmsTWO#ai-development--transparency) for the
full disclaimer.

## A note on "Classic" vs "Modern"

BmsTWO ships a **Modern dark theme** that is on by default, plus a **Classic**
mode that restores the original BmsONE look exactly. Toggle it in
**Preferences → General → Modern dark theme** (requires restart). See
[Appearance](Appearance).

(Not to be confused with **[Classic BMS Mode](Classic-BMS-Mode)**, which is a
charting workflow for pre-cut-sample songs, unrelated to the theme.)
