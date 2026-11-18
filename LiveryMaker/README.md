# RiffRacer Customs editor guide

A C++17/GTK3 authoring tool for replacing existing vehicle decals. It reuses
`NativeLinux/native_assets.hpp` for TABT, DDS, model UVs and PNG output. Its
archive writer ports the verified `packaging/native_archive.py` layout into C++.
The existing tools had no PNG-to-DDS encoder, archive exporter or authoring UI;
those are intentional new tooling, not matching-source reconstruction.

## Build and run

Dependencies: C++17 compiler, GTK3 development libraries, libpng, zlib, OpenSSL.

```sh
make -C LiveryMaker all test
build/livery-maker/reriff-livery-maker '/path/to/Riff Racer'
```

Without an argument, use **Open game folder** to select the folder containing
the original `vfs.zip`. No installation or changes to the game are needed to
author a livery. Windows users can use the portable x64 package; Linux users build from source.

The editor remembers the last successfully opened game folder and reopens it
on the next launch. An explicit command-line folder takes precedence. Settings
are stored in `$XDG_CONFIG_HOME/reriff-livery-maker/settings.ini` (normally
`~/.config/reriff-livery-maker/settings.ini`). If the saved folder has moved or
is unavailable, use **Open game folder** to choose another location. Invalid
folders do not replace the saved path. Headless exports and GUI screenshot
checks do not change this preference.

## Author a livery

1. Select a **Car**, then a **Paint**, such as **Pulse / Carmine**. The selectors
   follow the original vehicle-selection and PC paint configuration: 110 paints
   across 15 selectable cars, in native order, with original English names.
   Every paint has its own entry, including paints that share a decal. Unused
   archive textures and the unselectable Goliath assets are excluded.
   Paints without a separate decal support paint colors; PNG artwork controls
   are disabled for those selections.
   Shared textures list all affected paints in the status and require confirmation
   before export. Selecting one such paint does not isolate it from the others.
   The status also shows dimensions, encoding, mip levels and resource ID.
2. Save the original artwork PNG and UV template as starting points. UV0 includes
   all model meshes, without flipping; it is not a verified body-only decal mask.
3. Import an exact-size PNG, or choose **Make test livery**. PNG input is decoded
   to RGBA8 with libpng's simplified API. Wrong-size, corrupt or oversized input
   is rejected. There is no silent resizing. Imported alpha is retained as a
   channel, without an additional premultiplication operation.
4. View the encoded texture preview, original, or UV template. The checkerboard
   shows transparency. This is a 2D texture preview, not game shader rendering.
5. Export into a named mod folder. The output contains `vfs.dat`, `vfs.zip`, and
   `livery-maker.json` (tool ownership/provenance, not a required game manifest).
   Keep editable artwork separately using **Save artwork PNG**.

The program supports the 30 original single-texture decal batches: DXT3/DXT5,
1024/2048 dimensions and their original mip layouts. It preserves TABT metadata
and DDS headers. An unchanged export copies the original batch byte-for-byte.
Importing a PNG recompresses the texture and generates all its stored mip levels.
The encoder uses simple bounding-box endpoints and channel-wise box-filtered
mips: photographic quality and shader-correct filtering are not promised.
Models, unlocks and save data are never edited. Native finish/interpolation
parameters are preserved; paint RGB colors can be changed as described below.

Parent-directory symlinks (such as `/home` pointing to `/var/home`) are resolved
before export. Existing unrelated folders, output-folder symlinks, extra files and externally changed owned
archives are refused. Re-export requires the tool's ownership record and matching
archive hashes. Files are built and checked in a sibling staging folder, then
published together. During replacement the old folder is temporarily retained as
`.previous-PID`; after a crash during that step, inspect that backup rather than
deleting it. The program reports duplicate resource paths in sibling mod folders
and legacy ZIPs. Staging elsewhere cannot detect conflicts in your installation;
check the installed mods too. Native volume ID determines precedence, and later
ordinal named folders normally win. Do not use names to hide unresolved conflicts.

## Test package for the owner

The generated `build/test-package/reriff-livery-alf-diagnostic` folder targets
`alf_01`: **ALF, seventh paint selection (`alfPack7`, saved pack index 6)**.
It is 1024×1024 DXT5 with eleven stored mip levels. Its asymmetric red, green,
blue, yellow and magenta markers retain the original base-level alpha before
compression. The original configuration prices this pack at 7500; availability
depends on your save. The tool does not unlock it.

With ReRiff beta.11 or newer, fully quit the game and copy the entire folder to:

```text
Riff Racer/mods/reriff-livery-alf-diagnostic/
    vfs.dat
    vfs.zip
    livery-maker.json
```

Do not copy either archive over the stock pair. Start the game and select the
ALF seventh paint. Check the garage and a race for the colored markers, correct
transparency, seams, and distant appearance. Check another paint stays unchanged.
Restart to confirm persistence. To undo, remove only this test mod folder and
restart; the original appearance should return. There is no hot reload.

## Validation

- Warnings-as-errors C++ build and unit tests: BC2/BC3 color/alpha, mip layout,
  PNG round trip and malformed/dimension rejection, byte-identical no-op export,
  metadata preservation, output ownership and symlink/path refusal.
- Optional original-asset test: `build/livery-maker/test-livery ORIGINAL_DIR`.
- `--smoke-gui ORIGINAL_DIR ARTWORK.png SCREENSHOT.png` exercises actual GTK
  selection, PNG import/encoding, preview and UV export on a virtual display.
- `native_probe.c` reuses the existing original-code fixture's PE mapping,
  relocation and native stream helpers. It mounts the immutable stock archive,
  then this generated package, and compares every byte read through original
  native VFS against an independently extracted expected batch. This passed for
  the generated ALF diagnostic. It does not execute texture shaders or the game.
- Garage/race appearance, alpha interpretation and UV orientation await the
  owner's test. Native reads prove archive compatibility, not rendering parity.

Headless reproducible export uses the same C++ code as the GUI:

```sh
build/livery-maker/reriff-livery-maker --export ORIGINAL_DIR alf_01 \
  diagnostic /absolute/staging/MOD_NAME /absolute/new-artwork-folder
```

Replace `diagnostic` with a PNG path or `noop`. Original-derived assets stay in
ignored local build output; none are included in the source distribution.

## Paint colors

Six pickers edit the selected native paint: three body colors, highlight color,
and two wheel colors. Swatches show RGB inputs, not simulated vehicle lighting.
Color fields absent from a source paint are disabled rather than synthesized.

Paint edits stay in memory while switching car/paint selections. **Reset this
paint** restores only that paint's colors. Export before closing or opening
another game folder; this version does not save an editable paint project.

Export includes all pending paint edits, plus the current decal if its PNG was
edited. A paint-only edit affects the selected pack's colors even when its decal
is shared. Decal edits still affect every pack sharing that texture.

Paint mods override both `scripts/config/paint_pc.cfg` and the runtime resource
`scripts/config/paint_pc.luac`. CFG-only packages do not change runtime paint.
The editor changes only existing RGB float values in CFG. It preserves the stock
Lua instructions/constants and appends assignments for edited colors before the
final return. Prices, names, finish values and other colors retain their original
values. This supports the original Windows 32-bit Lua chunk, not luac64.
**Separate paint-config mods do not merge.** Build multiple paint edits in one
session/export, or disable conflicting paint-config mods before testing. Mods
that change prices in this configuration also conflict. The export dialog explains
this and sibling conflicts are reported.

The paint-only test at `build/paint-validation/reriff-paint-test` changes Pulse's
Arctic paint's three body colors to green. Fully restart after installing it.
Native VFS reads, independent archive checks, exact unrelated-byte preservation,
per-paint reset, invalid RGB rejection, combined paint/decal export and graphical
color-edit/reselection tests passed. The corrected Lua-backed body-paint export is owner-verified in both garage
and races; wheel/highlight controls and paint-mod removal need separate testing.

The corrected paint package was executed in the original Windows game's Lua
runtime: all 3,373 scalar values matched expectations from the exported CFG.
`native_paint_check.py MOD_FOLDER OUTPUT_FOLDER` generates `expected.bin` and
`check.lua` for that check. Pass these to `native-probe.exe` after the game EXE,
fixture mods path and `scripts\config\paint_pc.luac` resource path (check.lua is
the final optional argument). Run only against an isolated fixture/prefix.
The earlier CFG-only green tests remained blue in the owner's game; runtime Lua
export corrects the missing resource and subsequently passed owner garage/race testing.
