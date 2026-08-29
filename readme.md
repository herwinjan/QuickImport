# Quick Import #

![alt text](https://raw.githubusercontent.com/herwinjan/QuickImport/master/images/QuickImport-1.png)


**Streamline your workflow**  
Quick Import makes it fast and simple to transfer photos from your memory card straight into your project folders. It’s a lightweight utility designed for photographers who want efficiency without the clutter.  

**Effortless photo workflow**  
1. Capture your shots  
2. Import them directly to your hard disk with Quick Import  
3. Cull your selection in your favorite review tool  
4. Edit seamlessly in your preferred image editor  

**Key Features**  
- **Selective Import:** Bring in only the photos you need  
- **Quick Preview:** Realtime or Hit *Enter* or double-click to check images instantly (optional)  
- **Flexible Destination:** Organize files into the right project folder  
- **Data Integrity:** MD5 checksum for safe, verified transfers  
- **Multi-Location Import:** Copy files to multiple destinations at once  
- **Custom Naming:** Rename files and folders with flexible templates  

Experience a smoother, safer, and faster photo import process with **Quick Import**.

## Building from source

Requirements: CMake ≥ 3.21, Qt 6 (Core, Gui, Concurrent, Widgets, LinguistTools), [LibRaw](https://www.libraw.org/) and [Exiv2](https://exiv2.org/).

```sh
cmake -S . -B build \
    -DLIBRAW_ROOT=/path/to/libraw \
    -DEXIV2_ROOT=/path/to/exiv2
cmake --build build
```

`LIBRAW_ROOT` and `EXIV2_ROOT` point at an installation prefix or a
source/build tree; without them CMake falls back to common locations such as
`/opt/homebrew` and `/usr/local` (the environment variables `LIBRAW_ROOT` /
`EXIV2_ROOT` work too).

**macOS note:** the project targets macOS 14.0, but Homebrew builds its
libraries for your current macOS version only. To produce a binary that runs
on macOS 14, build LibRaw and Exiv2 yourself with
`MACOSX_DEPLOYMENT_TARGET=14.0` and point the `*_ROOT` options at those
builds. For distribution, bundle the LibRaw dylib into the app bundle
(e.g. with `macdeployqt`).
