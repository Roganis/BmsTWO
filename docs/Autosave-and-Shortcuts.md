# Autosave & Shortcuts

---

## Autosave & crash recovery

**What:** BmsTWO periodically snapshots your **unsaved** changes and offers to
restore them if it didn't close normally (e.g. a crash or power loss).

**How to use:**

* Configure in **Preferences → Edit → Autosave**: an **Enable autosave**
  checkbox and an **interval** (15 s – 1 h; default 180 s).
* If the app exits abnormally with unsaved work, the **next launch** asks
  whether to recover it. Choosing *Yes* opens the snapshot pointed at the
  original file and marked as modified, so you can re-save over the original.
* A clean exit, or a successful save, removes the snapshot — recovery is only
  offered when there is genuinely unsaved work.

**How it works:**

* On a timer, if the document is dirty, the document is serialized with
  `Document::ExportTo` to `<AppData>/Recovery/` (this does **not** disturb the
  real save state). A small registry entry (original path + timestamp) is stored
  in settings and `sync()`d so it survives a crash.
* Each session holds a **`QLockFile`**. On startup the app scans for orphaned
  snapshots and tries to take their lock: if it can (owner gone → crash) it
  offers recovery; if it can't (a live instance still holds it) it skips them.
  This avoids a second running instance being mistaken for a crash.

---

## Configurable keyboard shortcuts

**What:** Rebind any menu command to your own keys.

**How to use:** **Preferences → Shortcuts**. Every command is listed by
category; click the field next to one and press the keys to **capture** a new
shortcut. Conflicting bindings are highlighted and listed. **Reset All to
Defaults** restores the shipped bindings. Changes apply immediately and persist.

**How it works:** A `ShortcutManager` walks the menu bar once at startup,
records each action's compiled-in shortcut as its **default**, and applies any
saved override from the `Shortcuts` settings group (keyed by a stable
`<Category>/<Command>` id). The Preferences page uses `QKeySequenceEdit` for
capture; rebinds apply to the live `QAction` and persist. Resetting a command to
its default drops the stored override.

> Currently the rebinding page covers **menu commands**. Context-only actions
> that never appear in a menu are not yet listed.
