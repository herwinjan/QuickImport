# CLAUDE.md

Guidance for Claude Code (and other AI assistants) working in this repository.

## What this project is

Quick Import is a Qt 6 desktop app (C++17) for photographers: it imports
photos from a memory card into project folders, with selective import, live
preview, MD5-verified copies, an optional simultaneous backup copy, and
token-based file/folder naming. Primary platform is macOS (app bundle);
there is partial Windows/Linux support.

## Building

```sh
cmake -S . -B build \
    -DLIBRAW_ROOT=/Users/herwin/devel/LibRaw \
    -DEXIV2_ROOT=/Users/herwin/devel/exiv2
cmake --build build -j 8
open build/QuickImport.app
```

Important build facts:

- **Deployment target is macOS 14.0** (forced in CMakeLists.txt). Homebrew
  libraries are built for the *host* macOS only, so linking against
  `/opt/homebrew` produces "built for newer version" warnings and binaries
  that won't run on macOS 14. Use the locally built LibRaw and Exiv2 via
  `LIBRAW_ROOT` / `EXIV2_ROOT` (both are built with minos 14.0).
- Exiv2 is linked **statically** from a source/build tree; the generated
  header `exiv2lib_export.h` lives in the tree root, which CMakeLists finds
  via `EXIV2_EXPORT_INCLUDE_DIR`.
- LibRaw is linked **dynamically** from `~/devel/LibRaw/lib/.libs/`. For
  distribution the dylib must be bundled (macdeployqt / install_name_tool).
- Translations: `.ts` files are compiled by lrelease into `build/translations/`
  and embedded at `:/translation` via a generated qrc.
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
