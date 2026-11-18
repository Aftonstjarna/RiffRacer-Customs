# RiffRacer Customs

A graphical paint and PNG livery editor for Riff Racer, for Windows and Linux.
It is a separate authoring tool, not part of the ReRiff client/server packages.

## Windows

Download the Windows x64 ZIP from [Releases](https://github.com/Aftonstjarna/RiffRacer-Customs/releases).
Extract the entire ZIP, then launch **RiffRacer-Customs.exe**. GTK and its runtime
dependencies are included; keep the DLLs, `lib` and `share` folders alongside it.
Open the game folder containing `vfs.zip`, select a car and paint, and export a
named mod folder. Copy that folder into the game's `mods` directory and restart.

## Linux and development

Dependencies: C++17 compiler, GTK3 development libraries, libpng, zlib and OpenSSL.

```sh
make -C LiveryMaker all test
build/livery-maker/reriff-livery-maker
```

See [the editor guide](LiveryMaker/README.md) for PNG dimensions, shared decals,
paint export, settings and limitations. Windows builds use MSYS2 UCRT64:

```sh
make -C LiveryMaker OUT=../build/windows APP=RiffRacer-Customs EXEEXT=.exe LDFLAGS=-mwindows all test
python scripts/package_windows.py --prefix "$MINGW_PREFIX"
```

The CI workflow builds on Windows and Linux, tests PNG/export safeguards, and
opens the unpacked Windows package with MSYS2 removed from PATH. Game-dependent
tests run locally against an owner's installation; no game assets are in CI.

## Scope and provenance

Body paint and diagnostic decals have been tested in the garage and races.
Paint mods replace the runtime paint script; separate paint mods do not merge.
The 2D preview does not simulate native lighting. Export edits before closing.

Native format readers are reused from [ReRiff](https://github.com/Aftonstjarna/ReRiff)
under its MIT license, with platform-specific adaptations for this editor.
Third-party JSON code retains its included MIT license. Windows runtime licenses
and exact dependency versions are included in the package. No original game
binaries or assets are distributed.

The optional `native_probe.c` check requires a separate ReRiff checkout and an
original Game.exe. Define `RERIFF_ENGINE_SOURCE` as the absolute path to that
checkout's `ExperimentalMenu/engine_menu.c` when compiling it. This original-code
probe is not needed to build or use the editor.
