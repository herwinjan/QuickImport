# Changelog

All notable changes to Quick Import are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [0.96] - 2026-08-29

### Added

- **`package-macos.sh`**: produces a self-contained, ad-hoc signed app
  bundle (optionally a DMG). It verifies that nothing in the bundle resolves
  outside it and that no bundled binary needs a newer macOS than the
  deployment target; either check failing aborts the build. LibRaw is now
  bundled instead of linked from the build tree.

- **German and Spanish translations**: the interface is now available in
  English, Dutch, German and Spanish. All 111 interface strings are
  translated in all four languages.
- **Language picker** in Import Settings: pick a language explicitly or
  leave it on "System language". The change is applied immediately -- no
  restart -- and is remembered in the "language" setting.
- Qt's own translations (standard dialog buttons, file dialog) are now
  bundled for the same four languages, so those strings follow the
  interface language too.
- The macOS bundle declares `CFBundleLocalizations`, which is what lets
  macOS offer a per-app language under System Settings > General >
  Language & Region.
- **Adjustable divider between the file table and the preview/settings
  pane** (QSplitter); divider position and window size/position are
  remembered across sessions. The preview now rescales with the available
  space instead of forcing a fixed minimum width on the window (which made
  the window impossible to shrink and the divider immovable), so dragging
  the divider left really does give you a bigger preview.

### Fixed

- **Buttons looked wrong on macOS 26**: square push buttons and a default
  button whose focus ring sat detached above and below it. Qt below 6.11
  draws the pre-Tahoe macOS control style; the build now requires Qt 6.11.
- **English source text corrections**: "Delete files afer import" ->
  "after", "Don't show on statup" -> "startup", "uncheck Selected" ->
  "Uncheck Selected", and the leftover Dutch label "verwijder" in the
  preset dialog is now "Delete".
- **Filename token help was wrong**: the shortcut dialog listed `{yy}` for
  the 4-digit year (the token is `{Y}`) and described `{h}` and `{H}` as
  2- and 4-digit hours; they are hour without and with a leading zero.
  "file extention" is now "file extension".

- **Missing import/backup folder handled properly**: free space showed
  "-0.00 GB" and the import was blocked with "not enough disk space" when
  the chosen folder did not exist. The free-space labels now use the
  nearest existing parent folder's volume and show "(new folder)", and the
  import asks to create the missing folder before starting.
- **Refresh icon invisible in dark mode**: the black glyph is now tinted to
  the palette's text colour at runtime and re-tinted when the system
  switches between light and dark mode.
- **File names truncated after expanding a group**: the interactive column
  sizing only measured the initially visible rows; columns are now
  re-measured whenever a branch is expanded.
- **App quit/ejected with a full card**: the quit-if-empty and
  eject-if-empty checks read the model's `rowCount()` immediately after the
  reload, but the tree is built asynchronously and still reports 0 rows at
  that moment — so the app quit (looking like a crash, but without a crash
  report) or ejected even though the card still held files. The checks now
  use the synchronous card scan's file count.
- **External application no longer opens when nothing was imported**: a
  re-import where every file already existed (0 copied) used to launch the
  review application anyway.
- **Error dialog flood**: every failed file opened its own modal error box —
  re-importing an existing set stacked dozens of dialogs on top of each
  other. Errors are now collected and shown once, in a single summary
  dialog after the import (first 12 inline, full list under Details).
- **Crash on quit right after an import** (SIGSEGV in
  `FileInfoModel::~FileInfoModel`): with quit-after-import/quit-if-empty
  enabled, the app quits before the freshly created model's deferred tree
  build ever ran; the destructor then called `result()` on a default future
  whose result store is empty. The destructor now checks `resultCount()`
  before touching the result.
- **Startup crash on the "No Card found" dialog (SIGBUS in ImageIO)**: the
  app icon (`QuickImportLogo-1024.icns`) contained a single 1024px
  representation encoded as legacy JPEG 2000; macOS 26's IconServices
  crashes decoding it whenever a native alert renders the app icon. The
  icon was regenerated with `iconutil` as a complete PNG-based icns (all
  ten standard sizes). Crash reports: QuickImport-2026-08-29-085813/095832.
- **App would not start outside Qt Creator**: the locally built LibRaw
  dylib advertises `/usr/local/lib/libraw_r.24.dylib` as its install name,
  which does not exist. A post-build `install_name_tool` step now rewrites
  the reference to the dylib's real path.

- **Card-insertion detection was timing-dependent**: the disk event fires
  before macOS mounts the volume, and the old check only asked to open the
  card when the volume was *not* yet mounted — so a fast mount was silently
  ignored and a slow mount led to "No Card found". The app now polls for
  the mount (up to ~10 s) and only then asks, and only for writable
  FAT/exFAT volumes that are not already loaded. Startup enumeration of
  already-present disks no longer triggers the prompt.
- **Insertion events from Thunderbolt/PCIe readers (e.g. CFexpress) were
  never detected**: the protocol whitelist only accepted USB/SD; any
  removable or ejectable medium is now accepted too.
- **Duplicate insertion prompts**: whole-disk container objects
  (GUID/FDisk/Apple partition schemes) are now filtered, so a card no
  longer produces an event for both the disk and its partition; a guard
  also prevents two prompts from stacking.
- **DiskArbitration callback leaked and could crash**: the disk description
  dictionary was never released (leak on every disk event) and a NULL
  description (disk vanished mid-callback) was dereferenced.

- **Crash when cancelling a copy**: clicking *Cancel* in the copy dialog after
  the first worker had already finished dereferenced a null pointer. The
  cancel handler now checks that a worker still exists before cancelling it.
- **EXIF values wrong for big-endian files**: the EXIF parser callback ignored
  the byte-order marker (`ord`) and read all numeric values as little-endian.
  ISO, shutter speed, aperture, resolution, compression and focal length are
  now byte-swapped correctly for big-endian EXIF (e.g. Nikon). A denominator
  of 0 in rational values is also guarded against.
- **Non-ASCII file paths**: paths passed to LibRaw used `toLatin1()`, which
  silently broke paths containing accented or non-Latin characters. They now
  use `QFile::encodeName()`.
- **Division by zero in the preview overlay**: images without a known shutter
  speed showed a bogus value; the overlay now shows `-` instead. Exposures of
  1 second or longer are now shown as e.g. `2.0s` instead of an incorrect
  `1/x` fraction.
- **Duplicate default filename format**: `resetFileNameFomat()` appended the
  default `{J}/{o}` format to the list on every reset, growing the list with
  duplicates. It is now only added when not already present.
- **Build**: removed a stray character in `filecopydialog.cpp` that broke
  compilation.

### Performance / data safety

- **Preview loader reworked**: one persistent worker thread for the lifetime
  of the window (no thread churn per preview), with latest-wins request
  coalescing, an ~100 MB LRU cache of decoded previews and prefetching of
  neighbouring images — browsing with the arrow keys is now near-instant for
  recently seen and adjacent images. The cache is cleared on card reload so
  a modified card cannot show stale previews. Images are also no longer
  scaled twice in the loader.
- **Grouping and naming now use the capture time**: the tree (year/month/
  day/hour) and the date-based naming tokens (`{Y}`, `{m}`, `{D}`, …) use
  EXIF `DateTimeOriginal` when available, falling back to the file's
  modification time. For JPEG/HEIC/TIFF (which LibRaw cannot open) the EXIF
  data — capture time, ISO, shutter, aperture, camera, lens — is read with
  Exiv2, so RAW+JPEG pairs always share the same timestamp and can never
  split across folders. Note: for files whose EXIF date differs from the
  file date (e.g. copied cards), imports now land in folders matching the
  actual capture moment.
- **Free-space labels no longer query the disk on every keystroke**: the
  QStorageInfo values are cached for a few seconds (refreshed immediately
  when the folders or backup setting change).
- **Faster tree view**: columns use interactive sizing with a one-shot
  fit-to-contents after loading, instead of continuously re-measuring every
  row (`ResizeToContents`) on large trees.
- **Card loading is now parallel**: EXIF parsing (previously one file at a
  time) runs across the whole thread pool via `QtConcurrent::blockingMapped`;
  loading a full card is roughly *N-cores* times faster. Status-bar progress
  updates are throttled to one per 25 files instead of one per file, so the
  GUI event loop is no longer flooded during the scan.
- **Copy pipeline reworked (tee-copy)**: the copy is now a chunked
  read/write loop (8 MB) instead of `QFile::copy`. The source card is read
  **exactly once** per file: the MD5 hash is computed while copying (no
  second read for verification) and, with backup enabled, import and backup
  are written simultaneously from the same read (previously the card was
  read twice, or up to four times with MD5 + backup). Each temp copy is
  still verified on its own disk (size + MD5) before the atomic rename, so
  data safety is unchanged. Cancelling now takes effect within one chunk
  instead of only after the current (possibly huge) file, and the progress
  bar is byte-accurate instead of jumping per file.
- **Adaptive parallel copying**: workers now pull files from one shared
  queue instead of a fixed 50/50 split (no more idle worker while the other
  still has a pile of large files). The import always starts with a single
  worker; after ~200 MB the measured throughput decides whether a second
  worker is started (threshold ~300 MB/s card reads, MD5 double-read
  compensated). Slow SD readers stay single-threaded — friendlier for the
  card and usually faster; fast readers (CFexpress) get the second worker.
  With duplicate destination paths the copy stays single-threaded, as
  before.
- **Import up to 2× faster with MD5 enabled**: the redundant second MD5 pass
  after the atomic rename was removed; the copy is already verified (size +
  MD5) on the temp file before the rename.
- **Flush to physical storage before deleting the source**: with *delete
  after import* enabled, each copy (and backup copy) is now flushed to disk
  (`F_FULLFSYNC` on macOS) before the atomic rename, so a power loss cannot
  leave you with a deleted source and a copy that only existed in the OS
  write cache.

### Changed

- **Code cleanup (no behaviour change intended)**: the ten identical
  settings-checkbox slots now share one `saveBoolSetting()` helper; the
  check/uncheck/flip selection loops are merged into
  `applyCheckToSelection()`; the backup-UI enable/disable block became
  `setBackupUiEnabled()`; the duplicate `findOrCreateNode` in FileInfoModel
  was removed; all `QSettings("HJ Steehouwer", "QuickImport")` instances use
  the default constructor (org/app are set in main.cpp); the double `show()`
  call at startup was removed; dead code, stale comments and debug noise
  ("here4", "Here2", …) were cleaned up.
- **Model change notifications**: bulk check-state changes now emit a proper
  `layoutChanged` (via `FileInfoModel::refreshChecks()`) instead of
  `dataChanged` with invalid indexes; MainWindow no longer emits the model's
  signals itself. The selected-count label listens to both signals.
- **Typo fixes in identifiers**: `finshedImageLoading` →
  `finishedImageLoading`, `resetFileNameFomat` → `resetFileNameFormat`,
  `shortcutWindowFinisched` → `shortcutWindowFinished`.
- **Eject** is now explicitly macOS-only (`Q_OS_MACOS` guard with a warning
  on other platforms) instead of silently calling `diskutil` everywhere.
- **Translations**: combo-box placeholders ("--Location not set--" etc.) are
  now translatable; the "inserter card" typo in the card-inserted dialog was
  fixed; `.ts` files refreshed with lupdate and the 12 new/changed strings
  translated to Dutch.
- **readme.md**: added a "Building from source" section documenting
  `LIBRAW_ROOT` / `EXIV2_ROOT` and the macOS deployment-target caveat.

- **CMake**: added a `find_path` for the generated `exiv2lib_export.h` so the
  project builds against an Exiv2 source/build tree (where that header lives
  outside `include/`), not just against an installed Exiv2.
- **.gitignore**: replaced the long list of individual `build/` files with a
  single `build/` rule, and removed two accidental ignore patterns
  (`filecopydialog.cpp`, `translations.qrc`) that could hide real source
  files from git.

### Notes

- To link against macOS 14.0-compatible libraries instead of Homebrew's
  (which are built for the host macOS only), configure with
  `-DLIBRAW_ROOT=<path>` and `-DEXIV2_ROOT=<path>` pointing at your own
  builds. See CLAUDE.md / UITLEG.md.

## [0.1.0]

- First versioned release: version handling added, focus bug fixed,
  Dutch translations fixed.
