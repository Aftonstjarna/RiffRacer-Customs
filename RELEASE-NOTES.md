# RiffRacer Customs 0.2.0

## In-game summary

- Preview painted cars, liveries and the car HUD in 3D.
- Edit imported mods, names, default paints and HUD placement.
- Import static GLB models and choose a suitable donor car.

## Full release notes

- Use one editor for stock cars and imported mod folders or ZIPs.
- Preview native car geometry with paint and PNG liveries without stretching
  when the window is resized. Save artwork and UV templates from the editor.
- Edit imported paint colors, PNG liveries and in-game car/paint names. Choose
  the default appearance while preserving existing saved paint selections.
- Adjust multiplier, boost-meter and NOS positions, rotation and scale. Move
  linked NOS outlets together or as a pair, and snap elements to the car surface.
- Add mouse dragging, axis constraints, fine adjustment, undo, middle-button pan,
  right-button orbit around the pointer, and closer scroll zoom during drags.
  Tilde cancels an adjustment or resets the view.
- Choose four multiplier brackets or Pulse's two top brackets. Preview the car
  HUD at its current placement with a static x4 multiplier and 70% boost meter.
  HUD artwork respects car depth, and bracket changes preserve the boost meter.
- Add experimental static GLB import with orientation/scale controls, donor
  suggestions based on width, editable part roles, conversion preview and
  validated native-model export. Open the result directly in the editor.
- Export adjusted copies to new mod folders and retain unrelated imported files.
  Imports and original game files remain unchanged; HUD-only mods can be reopened.

Validation: automated geometry, texture, HUD, snapping and editor export/reimport
checks. The Impala GLB converted with nine stock donors. Windows and Linux
portable packages are built and smoke-tested.

Limitations: model import replaces the selected donor slot and retains its
collision, handling, animation, shadow and effect meshes. Source PBR textures are
not imported; native donor materials are used. Compressed GLB geometry, animation,
skinning and unsupported layouts are refused. Converted cars need in-game testing.
The preview approximates lighting and does not simulate animated HUD glow or track
tint. Imported names update English text; shared name tokens change together.
Export edits before closing. Separate paint-configuration mods do not merge.
Loose exports require a ReRiff client with loose-asset mod loading support.

---

# RiffRacer Customs 0.1.1

## In-game summary

- Add PNG decals to paints that previously had none.
- Keep new decals when switching between paints in the editor.
- Reset a paint to remove its added decal and restore its colors.

## Full release notes

- Enable **Add PNG decal** for all 82 selectable paints without decals.
- Give each added decal its own unused texture ID, preserving stock liveries.
- Export added textures together with the required native paint configuration.
- Retain added decals during the editing session and include them in exports.
- Support up to 99 decal IDs per car, with collision checks.

Limitations: other cars still need visual in-game testing. PNGs must match the
shown texture dimensions. Export edits before closing the editor. Separate
paint-configuration mods do not merge. The preview does not simulate in-game
lighting, and this update does not add new garage slots.

---

# RiffRacer Customs 0.1.0

## In-game summary

- Create paint and PNG livery mods on Windows and Linux.
- Select cars and paints using their in-game names.
- Export native mods without modifying game saves.

## Full release notes

- Portable Windows x64 editor and Linux x86-64 AnyLinux AppImage.
- Bundled GTK runtimes; the Linux package also bundles glibc and fonts.
- Edit body, highlight and wheel paint colors using in-game car/paint names.
- Import exact-size PNG artwork and export native livery mods.
- Preview textures, export UV templates and retain the last game-folder path.
- Export through ordinary symlinked parent folders with overwrite safeguards.
- Include the native Lua paint resource required for in-game paint changes.

Extract the complete Windows ZIP and launch `RiffRacer-Customs.exe`.
Make the Linux AppImage executable and open it; FUSE is optional.
The Linux package uses AnyLinux sharun for glibc and musl compatibility.

Paint edits are retained during the session but must be exported before closing.
Shared decals affect every paint listed by the editor. Separate paint mods do
not merge. The preview does not simulate the game's shaders or lighting.
The editor replaces existing paints/liveries; it does not add new garage slots.
No original game files are included, and authoring does not modify game saves.

Validation: native Windows package launch; Linux Arch/glibc and Alpine/musl
AppImage launch with file/color dialogs and PNG output. Linux export checks
cover all 110 paints and 30 original decal layouts, PNG encoding, ownership
and symlink protections. Tests use X11; Wayland has not been tested separately.
The Linux AppImage was added after the initial Windows release; packaging
sources are on the repository's main branch. The original v0.1.0 tag and
Windows ZIP are unchanged.
