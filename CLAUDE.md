# CLAUDE.md

Guidance for Claude Code (and other AI assistants) working in this repository.

## What this project is

QuickImport is a Qt 6 desktop app (C++17) for photographers: it imports
photos from a memory card into project folders, with selective import, live
preview, MD5-verified copies, an optional simultaneous backup copy, and
token-based file/folder naming. Primary platform is macOS (app bundle);
there is partial Windows/Linux support.

## Building

```sh
cmake -S . -B build \
    -DCMAKE_PREFIX_PATH=/Users/herwin/Qt/6.11.0/macos \
    -DLIBRAW_ROOT=/Users/herwin/devel/LibRaw \
    -DEXIV2_ROOT=/Users/herwin/devel/exiv2
cmake --build build -j 8
open build/QuickImport.app
```

Important build facts:

- **Qt 6.11 or newer is required** (enforced in `find_package`). Older Qt
  draws the pre-Tahoe macOS control style: on macOS 26 push buttons come out
  square and the default button's focus ring detaches into two thick bars.
- **Deployment target is macOS 14.0** (forced in CMakeLists.txt). Homebrew
  libraries are built for the *host* macOS only, so linking against
  `/opt/homebrew` produces "built for newer version" warnings and binaries
  that won't run on macOS 14. Use the locally built LibRaw and Exiv2 via
  `LIBRAW_ROOT` / `EXIV2_ROOT` (both are built with minos 14.0).
- Exiv2 is linked **statically** from a source/build tree; the generated
  header `exiv2lib_export.h` lives in the tree root, which CMakeLists finds
  via `EXIV2_EXPORT_INCLUDE_DIR`.
- LibRaw is linked **dynamically** from `~/devel/LibRaw/lib/.libs/` and must
  be configured with `--disable-lcms`: the app never uses LibRaw's colour
  pipeline (only `open_file`, `unpack_thumb` and the EXIF callback), while
  Homebrew's `liblcms2` is built for the host macOS and would break the 14.0
  deployment target.
- `./package-macos.sh` builds the distributable bundle: clean release build in
  `build-release/`, macdeployqt, LibRaw copied into `Contents/Frameworks`,
  then it verifies that nothing resolves outside the bundle and that no
  binary needs a newer macOS than the deployment target, and ad-hoc signs.
  `--dmg` also produces a DMG. There is no Developer ID / notarisation step.
- Translations: `.ts` files (listed in `TS_FILES`) are compiled by lrelease
  into `build/translations/` and embedded at `:/translation` via a generated
  qrc, together with Qt's own `qtbase_<lang>.qm` copied from the Qt install.
  Languages: en, nl, de, es. Refresh with
  `lupdate -no-obsolete $(ls *.cpp *.h *.ui *.mm qdevicewatcher/*.cpp qdevicewatcher/*.h) -ts quickimport_*.ts`
  and keep every catalogue at zero unfinished entries.
- `MacOSXBundleInfo.plist.in` is the bundle's Info.plist template; its
  `CFBundleLocalizations` array must list the same languages as `TS_FILES`.
- The project directory is reachable via two paths
  (`~/devel/Desktop/Devel/...` is the real one). If CMake complains the
  cache was created in a different directory, delete `build/` and reconfigure.

## Architecture (key files)

| File | Role |
|---|---|
| `mainwindow.cpp` | Main window: card selection, settings, presets, import orchestration |
| `fileinfomodel.cpp/.h` | `QAbstractItemModel` tree (year/month/day/hour/file) built on a background thread via `QtConcurrent`; also the LibRaw EXIF callback (`exif_callback`) and the `TreeNode`/`imageInfoStruct`/`fileInfoStruct` types |
| `filecopydialog.cpp` | Copy progress dialog; workers pull from a shared `fileCopyQueue`. Starts with 1 worker, measures throughput over the first ~200 MB and starts a 2nd worker only on fast readers (~300+ MB/s); always 1 worker when destination paths collide |
| `filecopyworker.cpp` | The actual copy: chunked tee-copy (8 MB) that reads the source once, hashes while copying and writes import + backup temp files simultaneously → per-target size/MD5 verify → atomic rename; deletes source only after verification. `processNewFileName()` resolves the naming tokens |
| `imageloader.cpp` | Persistent preview loader on one worker thread: latest-wins request coalescing, ~100 MB LRU cache, neighbour prefetch (QImageReader for normal formats, LibRaw thumbnail for RAW) |
| `language.cpp/.h` | Interface language: the "language" QSettings key (`system` or `en`/`nl`/`de`/`es`), loading `quickimport_*.qm` plus Qt's own `qtbase_*.qm` from `:/translation`. `install()` is called at start-up and again from the picker in Import Settings; re-installing a translator posts `LanguageChange`, which `MainWindow::changeEvent` turns into `retranslateUi` + `retranslateDynamicText()` |
| `devicelist.cpp` | The tree view widget for the card contents |
| `qdevicewatcher/` | Third-party hotplug watcher (per-platform backends) |
| `externDriveFetcher.mm` | macOS-only: fetch the volume icon (Objective-C++) |

## Filename tokens

`processNewFileName()` in filecopyworker.cpp resolves: `{D}` day, `{m}` month,
`{y}`/`{Y}` year, `{W}` week, `{h}`/`{H}` hour, `{M}` minutes, `{i}` ISO,
`{c}` serial number, `{T}` camera name, `{O}` owner, `{o}` original filename,
`{r}` sequence number, `{e}` extension, `{J}` resolved project name.
The result may contain `/` — directories are created with `mkpath`.
Date tokens use the capture time via `fileCopyWorker::captureTimestamp()`
(EXIF `DateTimeOriginal`, falling back to `lastModified`); the tree grouping
uses the same rule — keep them consistent.

## Versioning

The single source of truth is `project(QuickImport VERSION ...)` in
CMakeLists.txt; it feeds `QUICKIMPORT_VERSION`, the window title and the
bundle's `CFBundleVersion` / `CFBundleShortVersionString`. Nothing else
hardcodes a version.

Released versions are the `vX.Y` git tags. Check the tags before bumping:
the 0.7.x numbers that briefly appeared in 2026 were *lower* than the
already-published `v0.95` from 2024, which would have made a new release
look older than the one users already had. Every release must be numbered
above the highest existing tag, and tagged `vX.Y` when it ships.

## Conventions & gotchas

- Settings live in `QSettings("HJ Steehouwer", "QuickImport")` (org/app also
  set in main.cpp, so plain `QSettings()` works too).
- EXIF numeric reads **must** respect the byte-order parameter `ord`
  (0x4949 = little-endian, 0x4D4D = big-endian) — use the `readExifU16/U32/
  URational` helpers in fileinfomodel.cpp.
- File paths passed to LibRaw must go through `QFile::encodeName()`, never
  `toLatin1()`.
- This is a **data-safety-critical** tool: never weaken the copy pipeline
  (temp file + verification + atomic rename; source deleted only after the
  copy — and the backup, when enabled — verified OK).
- Worker/thread teardown in filecopydialog.cpp nulls `m_worker`/`m_thread`
  asynchronously; always null-check those pointers before use.
- UI slots follow the Qt auto-connect naming (`on_<object>_<signal>`); don't
  rename UI objects without updating the slots.
- TODO.md tracks known cleanups; CHANGELOG.md must be updated for user-visible
  changes.

## Testing

There is no automated test suite. Verify changes by building and running the
app with a real (or mounted disk image) FAT/exFAT volume containing RAW files.
Big-endian EXIF behaviour is best checked with Nikon NEF files.
