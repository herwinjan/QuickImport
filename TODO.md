# TODO

Known issues and planned cleanups, from the code review of 2026-08-29.
Ordered roughly by impact.

## Features

- [ ] Option: start the import even when there is not enough free space —
      copy until the disk is full (idea from an old inline TODO).
- [x] ~~Adaptive worker count~~ (done 2026-08-29: shared work queue; starts
      with 1 worker, measures throughput over the first ~200 MB and starts a
      second worker only above ~300 MB/s card reads. Single-worker fallback
      when destination paths collide is kept. Tune the constants in
      `fileCopyDialog::handleFileProcessed` if needed.)
- [ ] Verify the throughput threshold in practice: compare an SD reader and
      a CFexpress reader with the qDebug "Copy throughput probe" output.

## Performance

- [x] ~~Parallel EXIF parsing on card load~~ (done 2026-08-29:
      `QtConcurrent::blockingMapped` over the file list; status updates
      throttled to every 25 files).
- [x] ~~Copy pipeline rework~~ (done 2026-08-29: chunked tee-copy reads the
      card once per file — hash-while-copying, simultaneous import+backup
      write, mid-file cancel, byte-level progress bar).
- [x] ~~Preview loader rework~~ (done 2026-08-29: persistent thread,
      latest-wins coalescing, ~100 MB LRU cache, neighbour prefetch, single
      scaling; cache cleared on card reload).
- [x] ~~Free-space query per keystroke~~ (done 2026-08-29: cached ~3 s,
      refreshed on folder/backup changes).
- [x] ~~Group/name by EXIF DateTimeOriginal~~ (done 2026-08-29: capture time
      with lastModified fallback, via `fileCopyWorker::captureTimestamp`;
      used consistently by the tree, the naming tokens and the
      duplicate-destination check. JPEG/HEIC/TIFF get their EXIF via Exiv2
      since LibRaw cannot open them — RAW+JPEG pairs never split. Videos
      still use the file date.)
- [x] ~~Header ResizeToContents on big trees~~ (done 2026-08-29: interactive
      mode + one-shot fit after loading).
- [ ] Quick view (fullscreen) still decodes with its own LibRaw call
      (`requestImage` in mainwindow.cpp); could reuse the preview loader's
      cache.
- [x] ~~Avoid redundant MD5 passes~~ (done 2026-08-29: the post-rename MD5
      check in `copyImages()` was removed; `copyIntoPlace()` already verifies
      the temp file before the atomic rename).
- [x] ~~Explicit flush/fsync for durability~~ (done 2026-08-29: with
      *delete after import* enabled, the temp file is flushed to physical
      storage — F_FULLFSYNC on macOS — before the rename).

## Correctness / robustness

- [x] ~~Proper `dataChanged` ranges~~ (done 2026-08-29: bulk check changes go
      through `FileInfoModel::refreshChecks()` → `layoutChanged`; MainWindow
      no longer emits the model's signals).
- [x] ~~`doEject()` platform pretense~~ (done 2026-08-29: now explicitly
      macOS-only behind `Q_OS_MACOS`; other platforms get a warning. A real
      Windows/Linux implementation is still a feature idea.)
- [x] ~~Stale TODO block in mainwindow.cpp~~ (removed; the open feature idea
      moved to the Features section above).

## Code cleanup (done 2026-08-29)

- [x] Settings slots deduplicated via `saveBoolSetting()`.
- [x] Check/uncheck/flip merged into `applyCheckToSelection()`.
- [x] Backup-UI enable/disable extracted to `setBackupUiEnabled()`.
- [x] Duplicate `FileInfoModel::findOrCreateNode` removed.
- [x] `QSettings()` default constructor everywhere.
- [x] Double `show()` at startup removed.
- [x] Dead code and debug noise removed.
- [x] Identifier typos fixed (`finishedImageLoading`, `resetFileNameFormat`,
      `shortcutWindowFinished`) and the "inserter card" string corrected.
- [x] Placeholders now go through `tr()`; .ts files refreshed and the new
      strings translated to Dutch.

Still open:

- [ ] Replace remaining `qDebug()` output with `qCDebug` logging categories
      so it can be silenced in release builds.

## Build / packaging

- [x] ~~Bundle `libraw_r.dylib` into the app bundle for distribution~~ (done
      2026-08-29: `package-macos.sh` deploys Qt, copies LibRaw into
      `Contents/Frameworks` and verifies the bundle is self-contained).
- [ ] Notarisation: the packaged app is only ad-hoc signed, so other people
      hit Gatekeeper. Needs a "Developer ID Application" certificate and a
      `notarytool` step in `package-macos.sh`.
- [x] ~~Document `LIBRAW_ROOT` / `EXIV2_ROOT` in readme.md~~ (done
      2026-08-29: "Building from source" section added).
- [ ] Exiv2 is linked but barely used (`xmpengine.cpp` only) — decide whether
      the XMP feature stays; if not, drop the dependency.

## Verification

- [ ] Test EXIF parsing with big-endian files (e.g. Nikon NEF) after the
      byte-order fix: ISO / aperture / shutter speed in the preview overlay.
