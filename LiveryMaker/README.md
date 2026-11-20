# RiffRacer Customs editor guide

A C++17/GTK3 authoring tool for replacing existing vehicle decals. It reuses
`NativeLinux/native_assets.hpp` for TABT, DDS, model UVs and PNG output. It exports loose native assets; ReRiff builds its VFS cache locally.
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
5. Export into a named mod folder. The output contains an `assets/` tree and
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
assets are refused. Re-export requires the tool's ownership record and matching
resource hashes. Verified older tool-owned VFS packages are converted to loose
assets when re-exported. Complete, hash-verified ReRiff caches are discarded on
re-export and rebuilt at the next game start; unrelated files are refused. Files are built and checked in a sibling staging folder, then
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

With the ReRiff development loader that supports loose mods, fully quit the game
and copy the entire folder to:

```text
Riff Racer/mods/reriff-livery-alf-diagnostic/
    assets/graphics/vehicles/decals/alf_01/batchtextures.brres
    livery-maker.json
```

Keep the stock archive pair untouched. Start the game and select the
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

## Experimental: add a decal to an undecorated paint

Select a paint such as **Pulse → Arctic**, click **Add PNG decal**, then use
**Import PNG** or **Make test livery**. Export the mod as usual. New textures
use an unused ID and the same car's original decal format/UV layout. The
export includes the native paint configuration and Lua assignment, plus every
new decal created during this session. Switching paints retains added decals;
**Reset this paint** removes its added decal and restores its original colors.

Available for every selectable paint without a decal. Cars without a stock
decal use a blank 1024×1024 DXT5 texture container based on Pulse, with the
selected car’s own UV model. IDs 1 through 99 are allocated without collisions.
PNG dimensions must match the displayed template dimensions. Pulse Arctic has passed garage, race, isolation and removal checks. Other cars
still need visual in-game verification; the 2D preview is not a shader preview.
Separate mods that edit the full paint configuration do not merge.

## Painted car preview

The default view renders the selected car's original body and wheel geometry.
Body, highlight and wheel color controls update the preview immediately. Drag
the car horizontally to rotate it; scroll to zoom. The default **Painted + Livery** view combines paint and the current encoded
decal on the car. Choose **Car preview** for paint alone, or **Texture preview**,
**Original**, or **UV template** for the existing flat artwork views.

The car preview uses approximate studio lighting and material colors, not the
original game's D3D shaders, reflections or finish. Decals use the original UV0 coordinates with filtered alpha blending on painted
body materials; stock, imported and newly added decals update live. This is
an authoring approximation, not a reproduction of the original decal shader. No game meshes are bundled;
the preview reads them from your selected game folder.

## Loose mod compatibility

New exports require ReRiff’s loose-asset loader. Older releases that only load
VFS pairs cannot load these exports. The folder must be named `assets/`, not
`vfs/`. Zip the exported mod folder for sharing: there is no nested VFS ZIP.
Do not include `.reriff-cache-*` files generated after installing a mod.

Individual caches stay inside their mod folder. If the native archive-slot limit
is exceeded, ReRiff creates the overflow merge directly in `mods/`. The editor
exports native resource files and does not create or manage merged archives.
Disabled mods under `mods/.disabled/` are excluded from conflict detection.
Fully close the game before re-exporting an installed mod.

The original `native_probe.c` fixture mounts VFS pairs. For a loose export, run
ReRiff’s cache builder before using that archive-level fixture; do not pass the
`assets/` directory directly as a native archive.

### Import an existing car mod and adjust its HUD

Open the original game folder, then choose **Import mod folder** (the folder
containing `assets/`) or **Import mod ZIP** (one loose mod per archive). This
supports native EADB car mods such as the existing Impala test package. It is
not a GLB importer. The imported files remain untouched.

Choose the car and attachment: **Multiplier**, **Boost meter**, or a **Boost
outlet**. Change X/Y/Z position, pitch/yaw/roll, and local width/height/depth
scale. Rotations are world-axis deltas from the imported orientation, applied
pitch, then yaw, then roll; reset restores that attachment's imported transform.
The body preview uses approximate paint shading. Orange highlights the selected
attachment; cyan marks other attachments. Outlets show their local +Z direction.
Guides draw through geometry so they remain selectable/readable; they do not
simulate the game's HUD artwork, particle behavior, or clipping. Check the final
placement in-game. Orbit, elevation and zoom help inspect placement.

**Export adjusted copy** writes a new mod folder, preserving unrelated resources
and attribution. Existing destinations are refused. Disable the old mod before
installing the adjusted copy, then restart the game. Changes are kept only for
the current editing session until exported; reimport the exported copy to resume.
ReRiff generated archive caches are omitted. Symlinked input files and unsafe
archive paths are refused. Imports are limited to 512 MiB total and 256 MiB per
file. Paint/livery-only imports use the original game geometry and attachment nodes.
Custom geometry is loaded from the mod when present.

The reusable node writer validates the native model, patches only selected
48-byte transforms and recalculated file bounds, then decodes the result again.
No-op and reset preserve the original bytes exactly. It does not rewrite mesh
geometry, materials, collision, or native configuration.

Native-file integration checks (test output directory must not exist):

```
make ../build/livery-maker/test-mod-editor
../build/livery-maker/test-mod-editor MOD_FOLDER ORIGINAL_GAME/vfs.zip NEW_TEST_DIRECTORY
```

Attachment preview mouse and keyboard controls:

- Left-drag a marker's center to select and move it in the view plane.
- With the preview focused, press **G** to move, **R** to rotate, or **S** to scale,
  then move the pointer. Press **X**, **Y**, or **Z** to constrain the operation.
- **R X** adjusts pitch, **R Y** yaw, and **R Z** roll. Unconstrained **R** also
  adjusts roll. Rotation follows the existing world-axis delta fields.
- Hold **Shift** while moving for fine adjustment. **Enter** or left-click applies
  a keyboard operation; **~** (or the unshifted backtick key) cancels it. Releasing a direct
  marker drag applies it. Leaving the preview cancels an unfinished operation.
- Numeric fields update with every adjustment. Shortcuts apply only while the
  preview has focus, leaving normal text editing in the numeric fields intact.

The raster, attachment guides, marker picking and view-plane dragging share the
same aspect-preserving projection, including in maximized windows.

Camera navigation in the attachment preview:

- Hold the **middle mouse button** and drag to pan.
- **Scroll** to zoom; smooth trackpad scrolling is supported too.
- Hold the **right mouse button** and drag horizontally to orbit, vertically to
  change elevation. The camera sliders stay synchronized.
- **Reset view** restores the centered camera and default zoom.

Camera navigation changes only the view. While an attachment transform is active,
**~** cancels it. Wheel zoom remains available while dragging attachments,
panning or orbiting, and zooms around the pointer. Middle/right camera drags
wait until an attachment transform is applied or cancelled.

Press **~** (or backtick) with the preview focused to cancel an active attachment
adjustment. When no adjustment is active, the same key resets the camera view.
Escape remains an alternative for cancelling an active adjustment.

**Ctrl+Z** undoes attachment changes in the imported-mod editor. A completed drag
or keyboard transform is one undo step; numeric changes and attachment resets
are also recorded. If a transform is still active, Ctrl+Z cancels that transform
first. History follows the edited car/attachment and retains the last 200 steps
for the current import session. Camera navigation is not part of edit history.

### Imported paints, liveries, names and defaults

Import opens the paint/livery editor. **Adjust HUD / boosters** opens placement
controls and guides only when requested; turn it off to return to paint preview.

- Choose a paint, then change its six body/highlight/wheel colors.
- **Import PNG livery** adds or replaces artwork for that paint. Use the displayed
  exact dimensions; **Save PNG / blank template** exports artwork or a transparent
  template. Each edited paint gets an unused texture ID, preserving shared stock
  or imported liveries. **Remove livery** clears its decal. **Reset paint and
  livery** restores that paint's imported appearance.
- Edit the **In-game car name** and **In-game paint name**, then click **Apply
  names**. This updates the US and GB English localized strings while retaining
  internal IDs.
  Names sharing a localization token change together. UTF-8 names are converted
  to the native UTF-16 string bank; unrelated strings remain intact.
- **Use this paint as default** copies the selected appearance into the native
  first paint slot. Colors, finish parameters and livery are copied, while the
  first slot's name and price stay unchanged. Its decal gets a separate texture
  ID. Existing saves retain their chosen paint; select the first paint in-game
  to test the new default appearance. Reset the first paint to undo this copy.
- **Export adjusted copy** includes all pending paint, livery, name and placement
  changes, retaining the imported mod's other files. Paint changes include both
  native CFG and Lua resources. Changes survive switching paints/cars during the
  session; export before closing. Reimport an exported copy to continue editing.

Paint configs from another mod still do not merge at game load. The preview uses
approximate shading and decal blending, not all original material textures.
Ctrl+Z history currently covers attachment edits; use paint reset for appearance.
The new default/name combinations need visual in-game confirmation.

Imported-mod integration checks:

```
make ../build/livery-maker/test-mod-appearance
../build/livery-maker/test-mod-appearance IMPALA_MOD ORIGINAL_GAME/vfs.zip NEW_OUTPUT
python verify_imported_paint.py NEW_OUTPUT
python verify_imported_paint.py NEW_OUTPUT-second
```

The last checks independently evaluate the supported straight-line Lua 5.1
instructions and compare every paint value with CFG. They do not execute the
game or claim in-game visual validation.

For linked booster placement, select a boost outlet and enable **Link booster /
NOS movement**. Choose **All outlets** or **Selected outlet + partner**, then
choose the second outlet. Numeric XYZ edits, direct dragging and G moves apply
the same translation to the linked outlets, preserving their spacing. Rotation
and scale still affect the selected outlet only. HUD markers are never linked.
Ctrl+Z undoes a linked move as one action; tilde cancels the entire active group
move. Linking is an editor-session setting, not an in-game effect setting.

Right-drag captures the point under the pointer as its orbit pivot. Clicking on
the rendered model uses that surface's depth; empty space uses the view plane.
The captured point stays at the initial mouse location while changing orbit or
elevation. Wheel zoom still works during the drag.

**Snap to car** in the placement controls aligns the selected HUD element or
boost outlet to a nearby exposed body triangle. It points local +Z outward,
preserves scale, and adds 0.02 model units of clearance. HUD panels also sample
a 5×5 footprint to lift the panel over nearby curvature. Wheels, effects and
identified interior materials are excluded; candidate surfaces must have an
unobstructed outward ray. Place the element near the intended panel first.

Snapping affects only the selected element, even with linked NOS movement on.
Ctrl+Z restores its previous placement. This is a local surface fit, not a
curved decal projection: inspect the preview and check the exported result
in-game, especially around open windows, seams and sharply curved bodywork.

### One main editor

Normal startup now opens the unified paint/livery/attachment editor for stock
cars using the remembered game folder. On the first run, choose the game folder.
The old standalone paint window is no longer part of the normal interface.

The top row contains **Open game folder**, **Import mod folder**, **Import mod
ZIP**, **Stock cars / new mod**, and **Updates…** at the right. Opening a
different source replaces the current session after prompting about any
unexported changes. Window size and
maximized state carry over. Importing no longer opens a second editor window.

The sidebar also includes **Add PNG decal**, **Make test livery**, and **Save UV
template**, alongside paint colors, names, defaults, PNG import/save and reset.
The preview selector sits between **Paint** and **Adjust HUD / boosters** in the
toolbar and supports Painted + Livery, Car preview, Texture preview, Original
artwork, and UV template. Placement stays behind Adjust HUD / boosters.
**Export mod** writes the current stock edits or the combined imported-mod edits.

**Updates…** opens the update window. Stable is the default channel
and follows GitHub's latest non-prerelease release; beta is opt-in and follows
prereleases only. The window shows the release notes, the verified package
size and the SHA-256 check before installing. Updates replace the running
AppImage on Linux or the portable folder on Windows and restart the editor, so
export pending edits first. Startup checks are optional and cached for
15 minutes; a failed check never blocks editing.


### Static model import (development build)

Use **Import 3D model** in the unified editor, select an embedded GLB 2.0 file,
and review the source orientation, scale, donor and part roles. The initial scale
fits the source length to the median supported stock visual length; it is an
editable estimate, not a units measurement. **Suggest donor by width** ranks
supported stock collision widths against the oriented model at that scale;
height and length break ties. It does not compare collision shape or handling.

Assign each primitive to painted body, glass, metal/chrome, interior/plastic,
wheels or ignore. Suggestions use names and must be reviewed. **Fit to donor**
uses four wheel groups to fit wheelbase and track, or body length if no wheels
are assigned. This may change proportions. With Fit off, source scale is retained
and the model is centered and grounded. Donor wheel pivots are retained; their
alignment with the body must be checked in game. Without imported wheels, stock
wheel geometry is retained.

**Preview conversion** validates and displays the resulting native geometry.
Changing settings invalidates that preview. **Create mod and edit** exports to a
new folder and opens it in the existing paint/livery/HUD/booster editor. Source
files and the game installation are not modified. Choose **Replace donor car**
or **Add as a new car**; the latter requires ReRiff automatic car-slot support.
Export subsequent editor adjustments to another folder and install only that final copy.

The converter imports base-color textures for chrome, front-chrome, interior and
wheel-rim parts. Each image is resized to a native power-of-two size, given a full
mip chain, encoded as DXT1 (opaque) or DXT5 (alpha), and added to the donor's
texture atlas. Material records are cloned from a suitable donor material, renamed,
and pointed at the new texture; shader parameters are retained. The import dialog
lists the resulting texture names and sizes. Painted body panels stay on the
native car-paint material, and glass and tyres keep native materials. Source
metallic/roughness, normal, occlusion and emissive maps, vertex colors and base
color factors are not converted. UV0 is retained, UV1 duplicates UV0, supplied
normals are transformed correctly (or generated when absent), and tangent bases
are regenerated. Missing UVs become zero UVs, so add a proper UV layout before
authoring a PNG livery. Required glTF extensions, compressed/sparse/normalized
accessors, animations, skins and morph targets are refused. External .gltf files
and FBX are not supported; export a static GLB first.

The native writer clones donor material and parameter records, patches names and
texture slots, rebuilds the material, parameter and texture tables, and decodes
its output for validation. It supports understood 24/36-byte generated vertex
layouts and enforces the native 16-bit absolute index limit; simplify an oversized
model instead of truncating it. Donors without the required named body/four-wheel
layout or compatible car-paint layout are omitted. A missing material role stops
conversion with an explanation. The donor's original collision, driving setup,
animation, shadow and effect meshes are retained. Use the placement editor for
HUD/NOS adjustments; collision, effects, wheel animation, texture orientation and
visual alignment require an in-game test.

`model-import.json` records input/donor/output hashes, conversion options, part
assignments, dimensions and limitations. Include the model author's attribution
and license with any mod you distribute. The importer does not infer usage rights.

Validation commands (new output folders required):

```sh
make -C LiveryMaker test
build/livery-maker/test-model-import GAME_FOLDER MODEL.glb NEW_MOD_FOLDER
GDK_BACKEND=x11 xvfb-run -a build/livery-maker/reriff-livery-maker \
  --smoke-model-import GAME_FOLDER MODEL.glb NEW_MOD_FOLDER
```

The GUI smoke check captures `NEW_MOD_FOLDER.png`, tests stale-preview rejection,
exports a converted mod and reimports it. The integration test checks all donor
candidates and verifies name editing, paint access, unchanged collision and
no-overwrite behavior. Offline conversion success is not an in-game rendering claim.


### Multiplier bracket style

Under **Adjust HUD / boosters**, choose **Four brackets** or **Two top brackets
(Pulse)**. A small preview shows the native bracket artwork. This applies to the
selected car across all paints; it does not move or resize the multiplier. Choose
**Keep imported / original** to discard the style edit and restore the source
atlas. Switching cars retains pending choices. Export the mod to save them.

The editor copies only the original native multiplier DDS payload. The boost
meter, other HUD data and attachment transforms remain unchanged. A mod containing
only a bracket style change can also be imported. Unknown/incompatible HUD atlas
layouts are refused rather than recompressed. Regression command:

```sh
make -C LiveryMaker ../build/livery-maker/test-multiplier
build/livery-maker/test-multiplier GAME_FOLDER
```


**Show car HUD** is enabled by default in car preview modes. It draws a static
×4 multiplier and 70% boost meter from the current car's HUD atlas, using the
current attachment position, rotation and scale. Changing bracket style updates
this preview immediately. Turning placement controls off removes the guides but
keeps the HUD artwork visible. Uncheck Show car HUD for a clean car view.
The HUD is depth-tested against the car to expose clipping. Track tint, gameplay
visibility and animated glow are not simulated; confirm the final result in game.

### Donor metadata and export mode

Choose **Replace donor car** or **Add as a new car** in the editor or model
importer. The choice applies to every authored car in the package. Existing
replacement projects default to Replace; imported choices are retained.

Exports include `origin.txt` beside `assets/`:

```ini
[mod]
id=customs-<stable unique package identity>
overwrite=0

[origins]
alf=alf

[fallbacks]
alf=fab
```

Each mapping is **authored car ID = stock donor ID**. `alf` is Pulse; model
conversion uses the chosen donor's actual ID. Imported mappings are retained.
**Replace donor car** writes `overwrite=1`; **Add as a new car** writes
`overwrite=0`. The package ID is generated once and survives edits, repeated
exports, reimports and folder renames. Separate new projects receive different IDs.

`[fallbacks]` is optional and names the stock car shown when the custom car's
slot is unavailable, for example after its mod is disabled or removed. Choose it
under **Missing-car fallback** in the editor, or as **Same as donor** to omit it.
It is independent of the donor: the donor still supplies copied assets,
configuration, save identity and ghost identity. An absent, invalid or
unavailable fallback uses the donor. Keep it a stock car; runtime IDs and custom
IDs are refused.

Complete imported metadata stays byte-for-byte unchanged until its policy or
mappings change. Legacy metadata receives missing identity and replacement policy.
Ownership receipts are refreshed with the exported metadata hash; updating an
owned output first verifies its existing hashes.

Older mod folders and ZIPs without `origin.txt` can be imported for migration.
Known stock resource IDs receive their stock donor automatically. For unknown
custom IDs, use **Legacy car migration** to choose each stock donor by its
in-game name. The editor opens before these mappings are complete; export is
enabled once every donor is chosen. Choose either export mode and export a new
copy to write the stable package identity and mappings. The imported files remain
untouched. Donors are never guessed from a custom car's geometry or name.

Customs exports loose assets using authored IDs. It does not assign numeric slots
or bake runtime IDs into assets. ReRiff allocates slots, supplies missing donor
assets and migrates saves. Install only one copy of a package at a time; renamed
or edited exports remain the same package. New-car mode requires a ReRiff client
with automatic car-slot support and an available custom slot.

`make -C LiveryMaker test` includes metadata policy and ownership regressions.
The mod-editor and GUI smoke tests also cover reimported export choices.

## Unlock and prices

Open **Unlock and prices**, choose the car unlock type, edit the relevant fields,
and click **Apply unlock and prices**. Car price applies only to **Shop purchase**.
**Artist** matches the song's artist text; **Artist – Title** requires both fields.
Matches ignore case and accept substrings, as the original Police/Monaco checks do.
Available-from-start and play-a-track choices use the native unlock settings.

The selected paint and its PNG livery share the native pack price. Zero is free.
Appearance resets/default-paint changes retain prices. Existing saved ownership
is not revoked by changing rules or prices. Export a copy and restart the game.

Custom music rules require the matching ReRiff Customs-unlock client extension;
the original game hard-codes Police and Monaco and ignores arbitrary artist/title
configuration. Exports include paired native CFG/Lua files and a hash manifest.
The extension refuses music rules if another mod overrides only part of that pair.
Shop/paint prices use the existing native game implementation. A car with a single
paint receives that paint when the car unlocks, regardless of its pack price.
