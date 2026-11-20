# RiffRacer Customs 0.3.0-beta.4

## In-game summary

- Move the preview selector into the main toolbar.
- Keep the sidebar scrollbar clear of its controls.

## Full release notes

- Move the **Painted + Livery** preview selector from the left sidebar into the
  top toolbar, between **Paint** and **Adjust HUD / boosters**. All five preview
  modes are unchanged and still update the preview immediately.
- Shorten the toolbar update button to **Updates…** so the added selector still
  fits the default window at 100% scale. The installed version, channel,
  release notes and verification details remain in the update window.
- Give the sidebar's vertical scrollbar its own gutter with spacing on both
  sides, and widen the sidebar by that gutter so the controls keep their
  previous width and no horizontal scrollbar appears.
- Disconnect the updater cleanly when the editor window closes.
- Keep the stable and beta update channels, package verification and install
  behavior from 0.3.0-beta.3 unchanged.

Validation: updater, origin, model import, HUD, snapping and export tests pass.
The unified-editor smoke test passes, and the toolbar measures 1076 px inside
the default 1100 px window. Windows, Linux and AnyLinux packages are built and
smoke-tested; both packaged update windows are checked in CI.

Limitations: the wider sidebar takes 20 px more of the default window; drag the
divider to adjust it. Preview rendering and its approximate lighting are
unchanged. Install this release from the update window on 0.3.0-beta.3 or later, or
manually from the release page.

---

# RiffRacer Customs 0.3.0-beta.3

## In-game summary

- Update RiffRacer Customs from inside the editor.
- Choose stable releases or opt in to beta prereleases.
- Every update is verified by SHA-256 before installing.

## Full release notes

- Add **Check for updates…** to the main toolbar. The update window shows the
  installed version, update channel, release notes, download progress and an
  install button.
- Stable is the default channel and follows GitHub's latest non-prerelease
  release. Beta is opt-in and follows prereleases only. A beta build starts on
  the beta channel.
- Order releases by publication time, not tag order. The package filename is
  derived from the release version, and a channel switch reinstalls that
  channel even when the version text matches.
- Verify the selected package against the release size, the published SHA-256
  digest and `SHA256SUMS`. A missing or duplicate asset, size mismatch or
  digest mismatch stops the update and leaves the installed editor unchanged.
- Replace the running AppImage atomically on Linux and restart it. On Windows,
  close the editor, swap the portable folder with rollback, then restart. Keep
  the program folder writable.
- Cache automatic startup checks for 15 minutes and allow disabling them.
  Manual checks always contact GitHub, and a failed check never blocks editing.
- Bundle the Windows TLS certificate authorities and add
  `--check-updates [stable|beta]` for headless diagnostics.

Validation: updater tests cover channel isolation, publication ordering,
drafts, asset selection, checksum parsing and install decisions. A real release
download was checksum-verified, the Linux replacement path was exercised end to
end, and both packaged update windows are smoke-tested in CI. The Windows
package includes its certificate bundle.

Limitations: install this release manually once; later releases update in
place. Windows updates require PowerShell 5 and a writable program folder, and
Linux self-updates require the AppImage build. Export pending edits before
installing, because the editor closes to replace its files.

---

# RiffRacer Customs 0.3.0-beta.2

## In-game summary

- Pick which stock car appears when a custom car is missing.
- Keep the donor separate for assets, saves and shared ghosts.
- Set the fallback per authored car in origin.txt.

## Full release notes

- Add a **Missing-car fallback** section to the editor for every authored car.
  Choose any stock car, or **Same as donor** to omit the setting and keep the
  donor.
- Write the optional `[fallbacks]` section to `origin.txt` beside the existing
  `[origins]` donor mappings. Each entry is **authored car ID = stock fallback**.
- Keep the fallback independent of the donor: the donor still supplies copied
  assets, configuration, save identity and shared-ghost identity.
- Preserve imported fallback metadata byte-for-byte until it is changed, like
  other imported metadata. Exports, reimports, renames and metadata-only edits
  retain the declared value.
- Refuse unknown, malformed, unmapped, runtime and custom-car fallbacks instead
  of guessing. Erasing a fallback restores donor behaviour.
- Read the setting with ReRiff 1.4.1-beta.7 or newer. Older clients ignore the
  section and keep using the donor; the original game sees no change.

Validation: automated origin fallback set/reset, export/reimport, metadata
preservation and editor selector checks, plus the existing geometry, texture,
HUD, snapping, model import and unlock checks. Windows and Linux portable
packages are built and smoke-tested.

Limitations: the fallback must be a stock car and only applies when the custom
car's slot is unavailable; it does not install missing assets or bypass a
disabled mod. Authoring remains save-safe and does not modify game saves.

---

# RiffRacer Customs 0.3.0-beta.1

## In-game summary

- Add imported cars as new cars instead of replacing a stock donor.
- Migrate older custom-car mods by choosing each donor once.
- Set car unlocks and shop prices, including artist and title rules.

## Full release notes

- Choose **Replace donor car** or **Add as a new car** for imported models and
  packages. Add mode needs a ReRiff client with automatic car-slot support.
- Write `origin.txt` beside `assets/` with a stable package identity and one
  stock donor per authored car. The identity survives edits, repeated exports,
  reimports and folder renames, so ReRiff can recognise one installed package.
- Import legacy mod folders and ZIPs without `origin.txt`. Known stock resource
  IDs map automatically; unknown custom IDs are assigned with **Legacy car
  migration** by donor name. Imports open before mapping completes, and export
  stays disabled until every donor is chosen.
- Keep imported metadata byte-for-byte, including comments and line endings,
  until its policy or mappings change. Ownership receipts record the exported
  metadata hash.
- Open **Unlock and prices** to choose the car unlock type: keep the existing
  rule, available from start, play a track, shop purchase, artist, or
  artist – title. Edit the car price for shop purchases and the shared
  paint/livery price. Zero makes a paint free.
- Patch native unlock and paint configuration in place, preserving unrelated
  fields, records and Lua initialization. Exports include paired CFG/Lua files
  and a hash manifest for the customs-unlock client extension.
- Refuse runtime car slots and non-authored IDs in metadata, with a clear rename
  message instead of guessing a donor from geometry or a car name.

Validation: automated origin metadata, stable identity, legacy migration,
ownership, unlock, price, config roundtrip and Lua patch checks, plus the
existing geometry, texture, HUD, snapping and editor export/reimport tests.
Windows and Linux portable packages are built and smoke-tested.

Limitations: new-car mode requires a ReRiff client with automatic car-slot
support and an available custom slot. Install only one copy of a package.
Custom music unlock rules require the matching Customs-unlock client extension;
the original game ignores artist and title rules beyond Police and Monaco.
Existing saved ownership is not revoked by changing rules or prices. Export
edits before closing.

---

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
