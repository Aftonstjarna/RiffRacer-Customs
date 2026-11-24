# Riff Racer Customs

A graphical car, paint, PNG livery and HUD editor for Riff Racer, for Windows and Linux.
It is a separate authoring tool, not part of the ReRiff client/server packages.

## Windows

Download the Windows x64 ZIP from [Releases](https://github.com/Aftonstjarna/RiffRacer-Customs/releases).
Extract the entire ZIP, then launch **RiffRacer-Customs.exe**. GTK and its runtime
dependencies are included; keep the DLLs, `lib` and `share` folders alongside it.
Open the game folder containing `vfs.zip`, select a car and paint, and export a
named mod folder. Exports use `assets/` and require ReRiff’s
loose-asset loader. Copy that folder into the game's `mods` directory and restart.

## Linux

Download `RiffRacer-Customs-0.3.5-anylinux-x86_64.AppImage` from
[Releases](https://github.com/Aftonstjarna/RiffRacer-Customs/releases/tag/v0.3.5),
make it executable in your file manager, and open it. Or run:

```sh
chmod +x RiffRacer-Customs-0.3.5-anylinux-x86_64.AppImage
./RiffRacer-Customs-0.3.5-anylinux-x86_64.AppImage
```

The AnyLinux package includes GTK, glibc and fonts using
[pkgforge's sharun tooling](https://github.com/pkgforge-dev/Anylinux-AppImages).
It supports glibc and musl distributions and falls back to extraction when
FUSE is unavailable. It requires an x86-64 Linux desktop (X11 or Wayland).

## Updates

Riff Racer Customs checks GitHub and installs a newer published package from
inside the editor. The installed version is shown in the update window.

- **Stable** is the default channel and only follows GitHub's latest
  non-prerelease release. **Beta** is opt-in and only follows prereleases;
  publishing a beta never moves the stable channel.
- Releases are ordered by publication time, not by tag order. Tags use the
  `vX.Y.Z` release version, the package filename is derived from it, and a
  channel switch reinstalls that channel even when the version text matches.
- The updater accepts only the package named for the selected release version
  and this platform. It downloads to a temporary file and verifies the package
  size and SHA-256 against the release `SHA256SUMS`. A missing checksum entry,
  ambiguous asset, size mismatch or digest mismatch stops the update.
- On Linux the verified AppImage replaces the running file and restarts the
  editor; keep the AppImage in a writable folder. On Windows the editor closes,
  the verified package replaces the portable folder contents with PowerShell,
  and the editor restarts. Keep that folder writable.
- Cancelled or failed updates leave the installed editor unchanged. Automatic
  checks are cached for 15 minutes and can be disabled; manual checks always
  contact GitHub, and the editor remains fully usable offline.
- Edits are held in memory until exported. Export pending work before
  installing an update, because the editor closes to replace its files.

The release check can also run without the window:
`./reriff-livery-maker --check-updates [stable|beta]`.

## Development

Dependencies: C++17 compiler, GTK3 development libraries, libpng, zlib, OpenSSL and libcurl.

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

### Building the AnyLinux AppImage

```sh
podman build -t customs-anylinux -f packaging/Containerfile.anylinux .
podman run --rm --userns=keep-id:uid=0,gid=0 --security-opt label=disable -v "$PWD:/src" customs-anylinux
```

The output is in `dist/`. The packaging script pins and checksums quick-sharun;
that script verifies its downloaded runtime tools. Arch dependency versions
and license notices are recorded inside the bundle. The base image and Arch
packages receive updates, so rebuilds are not guaranteed byte-identical.
CI also tests the AppImage in Alpine without host GTK or FUSE.
