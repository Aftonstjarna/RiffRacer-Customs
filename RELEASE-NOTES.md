# RiffRacer Customs 0.3.4-beta.6

## In-game summary

- Model import no longer asks where to save the car.
- The car opens as an unsaved working copy in the editor.
- Choose a folder only when you press Export mod.

## Full release notes

- **Import 3D model** now confirms with **Import and edit** instead of opening a
  save dialog. The converted car opens directly in the paint, livery and HUD
  editor as a working copy.
- The working copy lives in the system temporary folder. **Export mod** is the
  first save prompt and writes the final mod folder you install.
- All editor changes, templates, paint/livery exports and HUD adjustments work
  on the working copy as before. Source files and the game installation are
  still never modified.
- The importer smoke now accepts the dialog and verifies that the returned
  working copy reimports the authored car.

Validation: the model-import smoke creates and reimports the working copy, and
the unified, mod-attachment and unit tests pass. Windows, Linux and AnyLinux
packages are built and smoke-tested.

Limitations: closing the editor without exporting discards the working copy.
The temporary folder is not cleaned automatically.

---

# RiffRacer Customs 0.3.4-beta.5

## In-game summary

- Linked boosters now snap to the car together, keeping their spacing.
- The selected outlet still aligns its rotation to the body surface.
- Ctrl+Z undoes the whole booster snap as one action.

## Full release notes

- Snap to car and Ctrl+Space now move every linked boost outlet by the same
  offset as the selected outlet. The group keeps its spacing instead of
  leaving the other outlets behind.
- The selected outlet is still aligned and rotated to the local surface; the
  linked outlets keep their own rotation and scale.
- This applies to both **All outlets** and **Selected outlet + partner**
  linking. HUD markers are never linked, and snapping without linking is
  unchanged.
- The status message reports when linked outlets moved with the snap, and
  Ctrl+Z restores the whole group.
- Add a smoke regression that snaps a linked four-outlet booster group and
  asserts every outlet moved by the same offset and undo restores them.

Validation: the mod-attachment smoke covers the linked snap and undo, and the
unified smoke, unit tests and Windows/Linux/AnyLinux builds pass.

Limitations: linked snapping translates the group as a whole; it does not fit
each outlet to its own local curvature. Use Ctrl+Space on each outlet when each
one needs its own surface alignment.

---

# RiffRacer Customs 0.3.4-beta.4

## In-game summary

- Chrome wheel-arch trim no longer spins with imported wheels.
- Wheel and tire parts stay wheels; chrome stays on the body.
- Re-import or re-export the car to correct existing wheels.

## Full release notes

- Classify imported parts by their source material before falling back to their
  names. Parts using a wheel or tire material become rolling wheels; parts
  using chrome or metal materials stay on the body, even when their names
  contain "wheel".
- This stops `SideChromewheel`, `BacksideChromewheel` and similar wheel-arch
  moldings from being split across the four wheel groups and rotated with the
  tires. Those pieces previously appeared as floating or misaligned chrome
  around the wheels in game.
- Glass and window parts keep priority, including glass that uses a chrome
  material name.
- Add a regression using the Impala source: the tire and bolt parts keep their
  wheel role, the chrome wheel-arch trims become body metal, and no wheel node
  carries a chrome material.
- Rebuilding a template saved before this version also corrects wheel-arch
  trim roles, and saved material settings are matched by name so they still
  apply when role changes reorder the material plan. Packages already
  installed still need to be rebuilt or re-exported.
- Includes the paint-mask and native no-livery fixes from 0.3.4-beta.3.

Validation: the owner Impala export converts nine donors, reimports the car and
checks the new wheel roles and paint-mask slots. The unified, mod-attachment and
importer-dialog smokes pass. Windows, Linux and AnyLinux packages are built and
smoke-tested.

Limitations: role suggestions still rely on names when a model has no wheel or
chrome material names. Older templates are corrected only for chrome names
that contain "chromewheel" or "wheelarch"; review other wheel trim roles in
Advanced. Imported wheels use the donor wheel pivot positions; if a source
model has unusual wheel origins, correct the roles and check the alignment in
game.

---

# RiffRacer Customs 0.3.4-beta.3

## In-game summary

- Plain custom paints no longer show donor paint-mask UV patches.
- Clearing a livery now writes native zero, so plain packs reset.
- Re-export an imported car to apply both fixes to its package.

## Full release notes

- Fix the neutral body paint mask for donors whose paint shader does not use
  `carpaint_occspec`. The importer now reads the body material's actual paint
  mask slot, which is `occ_spec` on the Malibu, Bolt and some other donors, and
  replaces that texture at the donor's size, format and mip count. Imported
  source UVs therefore sample a solid yellow mask instead of the donor's
  occlusion and livery islands, removing the cyan and black patches on plain
  paints.
- Keep the existing behavior for donors that use `carpaint_occspec`; the
  replacement is still a neutral yellow texture.
- Write native `0` instead of `-1` when an added paint pack has no decal, when
  a cloned donor pack has no copied decal, and when clearing a livery in the
  Paint tab. Switching from a livery pack to a plain pack now clears the decal
  in packages exported by this version.
- Add regressions for the Malibu paint-mask slot and size, for non-negative
  livery values across a cloned Impala paint table, and for clearing a livery.
- Includes the car templates and livery-mod merge from 0.3.4-beta.1 and
  0.3.4-beta.2.

Validation: the owner Impala export converts nine donors, exports and reimports
the car, and now also exports a Malibu-donor body whose `occ_spec` mask is a
1024² neutral texture with 11 mips. The editor smokes cover imported paint
editing, clearing a livery, the importer dialog, packaging and the merge action.
Windows, Linux and AnyLinux packages are built and smoke-tested.

Limitations: existing packages keep their donor mask and any negative livery
values until they are re-exported; the ReRiff loader 1.5.0-beta.2 normalizes
negative livery values at load time, but it cannot repair an old body mask.

---

# RiffRacer Customs 0.3.4-beta.2

## In-game summary

- Merge livery mods that paint different packs of the same car.
- One combined paint config keeps both decals and paint names.
- Install only the merged package and remove the original mods.

## Full release notes

- Add **Merge livery mods** in the main window. Livery-only packages each
  contain a complete paint configuration, so installing two of them makes the
  later mod replace the earlier paint assignment. Choose two or more
  livery-only mod folders and a new output folder; the merge keeps every
  mod's selected paint pack and decal in one conflict-free package.
- The merge rebuilds one paint configuration and Lua file from the stock
  tables, copies the decal and name resources, and writes
  `assets/` plus `livery-maker.json` with a `merged_from` record of the input
  folders.
- The merge refuses two mods that target the same car paint, and refuses a
  resource whose hash does not match its `livery-maker.json` entry.
- Disable or remove the original livery mods and install only the merged
  folder, otherwise their full configurations still conflict.
- Includes the complete car templates from 0.3.4-beta.1: save a car's import
  recipe and appearance, then rebuild it at another quality by choosing the
  source GLB, which the editor verifies against the saved hash.

Validation: the merge was verified with two Pulse livery mods, producing one
package with the Arctic decal on pack 1 and the Kazuho decal on pack 2. Package
reimport resolves both paint assignments, names and decals. Windows, Linux and
AnyLinux packages are built and smoke-tested.

Limitations: merging expects livery-only packages created by Riff Racer
Customs that select different car paints. It does not merge arbitrary config
mods, and it does not modify the original mod folders.

---

# RiffRacer Customs 0.3.4-beta.1

## In-game summary

- Save a complete car template and rebuild it at another quality.
- Templates store no GLB or path; choose it when rebuilding.
- Reapply paint, livery, unlocks, names and HUD automatically.

## Full release notes

- Add **Save car template…** in the Paint tab. It stores the import recipe
  (source hash/title, donor, authored ID, mode, options, part roles and
  material plan) plus the appearance deltas for paint, livery, unlocks, names,
  HUD and attachment transforms.
- Add **Rebuild car from template** in the main window. Choose the template,
  the source GLB, a target detail level and an output folder. The GLB hash is
  verified against the template; a mismatch is refused and both hashes are
  shown.
- Add **Apply appearance template…** in the Paint tab to reapply paint, livery,
  unlocks, names, HUD and attachment changes to the same authored car without
  the source model.
- Templates store no source GLB path and no embedded GLB. The player supplies
  the path for each rebuild; it is never written into the template or package.
- Rebuilding at another quality reuses the saved roles and material plan,
  applies the requested texture sizes and reapplies the saved appearance.
- Templates are same-car-ID only.

Validation: template save/apply is covered by the editor smoke; a headless
High-quality rebuild, custom-car unlock retention, material/part recipe reuse
and package reimport are covered by the owner test. Windows, Linux and AnyLinux
packages are built and smoke-tested.

Limitations: the source GLB must match the template hash. Template recipes from
older imports may lack newer option fields and fall back to defaults. Templates
do not remap to a different authored car ID yet.

---

# RiffRacer Customs 0.3.3

## In-game summary

- Exported assets now use lowercase names for current cache builders.
- Mixed-case duplicate assets are refused safely.
- Full mods and paint/livery packages use the normalized paths.

## Full release notes

- Normalize exported asset resource names to lowercase, matching the native VFS
  index while preserving the original stock archive.
- Apply normalization to full editor exports and standalone paint/livery
  packages, including their embedded resource metadata.
- Refuse case-only duplicate assets with different bytes instead of silently
  choosing one.
- Keep the 0.3.2 custom-car unlock slots and Riff Racer Customs display name.

Validation: the origin/export test now writes a mixed-case decal path, verifies
the lowercase output path and checks the absence of the mixed-case file. Full
unit and import tests pass; Windows, Linux and AnyLinux packages are built and
smoke-tested.

Limitations: cache generation remains the game's responsibility; verify the
installed mod and its resource names in game.

---

# RiffRacer Customs 0.3.2

## In-game summary

- Custom imported cars now expose unlock and car-price options.
- Custom cars inherit their donor's unlock rule by default.
- The program and docs now use the Riff Racer Customs name.

## Full release notes

- Add a native unlock slot for a custom authored car when the imported unlock
  table has no entry. The donor's unlock rule is copied when available;
  otherwise the car defaults to available from start.
- Patch both the unlock CFG and Lua with the new car name and unlock entry, so
  **Car unlock**, **Car price**, **Artist contains** and **Title contains**
  remain editable and survive export/reimport.
- Use **Riff Racer Customs** as the program, window, update-dialog, documentation
  and desktop-entry display name. The GitHub repository, release URLs and
  artifact filenames remain `RiffRacer-Customs`.
- Keep the 0.3.1 standalone paint/livery mod export and include-paint checkbox.

Validation: the owner import test creates a custom car, adds a shop unlock and
price, exports/reimports it, and checks both unlock CFG and Lua changes. Unit
and import tests pass; Windows, Linux and AnyLinux packages are built and
smoke-tested.

Limitations: unlock order and car availability still depend on the ReRiff
composer registering the custom car slot. Verify the car and its price in game.

---

# RiffRacer Customs 0.3.1

## In-game summary

- Export the selected livery as its own standalone mod package.
- Choose whether to include paint colors, price and names.
- The package uses only assets and metadata, with no origin file.

## Full release notes

- Add **Export paint/livery mod** to the Livery tab. It writes a standalone
  package for the current paint instead of the whole imported car.
- Add an **Include paint configuration** checkbox to the export dialog. When
  checked, the package includes paint colors, price, names and the paint
  CFG/Lua. When unchecked, it contains only the livery texture.
- Use the runtime-compatible package layout: an `assets/` folder plus
  `livery-maker.json`, with no `origin.txt` and no generated cache. The game
  rebuilds the cache at startup.
- Default to livery-only when the paint already has a livery and no paint
  changes; default to including paint when colors or names changed.
- Safely replace a previous owned paint package, removing stale caches, while
  refusing unrelated folders and full mod packages.
- Keep the 0.3.0 importer, editor tabs, HUD styles, placement shortcuts, decal
  cloning and car/paint-based PNG filenames.

Validation: the unified editor smoke exports both package variants and checks
the file layout, metadata, livery-only contents and safe replacement. Windows,
Linux and AnyLinux packages are built and smoke-tested.

Limitations: including paint configuration writes the full native paint table,
which can conflict with other paint-config mods. Livery-only packages overwrite
the selected decal resource, so verify the installed result in game.

---

# RiffRacer Customs 0.3.0

## In-game summary

- Import GLB cars with paints, liveries, HUD and decals.
- Custom car IDs, imported-body paint and native HUD styles.
- Simple importer, sidebar tabs and a resizable preview.

## Full release notes

### Model import

- Import static GLB 2.0 cars into native Riff Racer assets: geometry, base-color
  textures, native materials, collision, effect shells, shadow, garage box,
  paint slots and names.
- Use the **Simple** importer by default: in-game name, auto-selected donor,
  Add/Replace mode, generated car ID and detail level. **Add as a new car** is
  the safe default. Advanced options expose scale, orientation, donor ranking,
  authored ID, fitting, materials and the full part-role review.
- Choose **Light**, **Vanilla-matched**, **High** or **Full source** detail.
  Quadric simplification preserves UV seams and boundaries; imported meshes are
  merged by node and material.
- Import base-color PNG textures with power-of-two resize, mip chains and
  DXT1/DXT5 encoding. Set native templates, sizes and finishes per material.
- Fit the donor collision hull, ground shadow and garage selection box to the
  imported footprint, and rebuild the front, electric and rear effect shells.
- Add a neutral body paint mask. **Imported-body paint** is on by default and
  creates a decal-free imported-body paint; when a donor already has nine paint
  packs, it replaces the default pack instead of overflowing the client limit.
- Use any three-letter lowercase car ID for a new car, such as `spn`. The
  exporter creates its name token for all locales, clones both paint tables and
  the 32-bit paint Lua, and copies/remaps referenced donor decals under the new
  ID. Stock archives and stock paint-name tokens remain untouched.
- Generate `ATTRIBUTION.txt` from GLB author, licence, title and source metadata.

### Editor

- Sidebar tabs for **Paint**, **Livery** and **HUD**; the full-height sidebar
  scrolls cleanly and the status line sits under the preview.
- Resize the preview by dragging the wide sidebar divider.
- Placement shortcuts: **Space** snap, **Ctrl+Space** snap and follow local body
  curvature, and **Ctrl+Shift+Z** undoable reset to the imported transform.
- Multiplier bracket styles plus a **Boost meter shape** selector with the 13
  distinct native stock meter artworks and a live preview.
- **Add livery template** creates a transparent decal with cyan UV island
  outlines for use as a guide layer. PNG saves use car and paint names, such as
  `pulse-carmine-livery.png`, `pulse-template.png`, `pulse-test.png` and
  `pulse-uv.png`.
- Error dialogs and status text are selectable so failures can be copied.

Validation: GLB parsing, conversion, decimation, texture encoding, material
mapping, collision/shadow fitting, localization tokens, paint cloning, decal
copying, HUD composition, placement snapping and update rules are covered by
automated tests. Import and editor UI smokes cover Simple/Advanced modes,
custom-ID export/reimport, decorated paints, tabs, placement and exported image
names. Windows, Linux and AnyLinux packages are built and smoke-tested.

Limitations: imported cars still require an in-game check of textures, glow,
collision, paint and decals. Metallic/roughness, normal, occlusion and emissive
maps, vertex colours and base-color factors are not converted. Curvature
following is a local normal average, and effect UVs/fades remain experimental.
Add mode requires ReRiff automatic car-slot support. Livery templates are guides
only; their cyan lines render in game if not removed before reimport.

---

# RiffRacer Customs 0.3.0-beta.10

## In-game summary

- Decorated donor paints now work on custom car IDs.
- Donor decals are copied and renamed for the new car automatically.
- Missing decals clear the livery instead of failing the import.

## Full release notes

- Copy every donor decal referenced by a cloned paint table into the exported
  package under the authored car ID, for example `hon_02` → `spn_02`.
- Rename each decal's internal texture to the new ID and remap the cloned paint
  table's livery IDs in both paint CFGs and the 32-bit paint Lua.
- If a referenced donor decal cannot be copied, clear that paint's livery
  reference instead of leaving a missing asset that breaks paint selection.
- Keep the beta.9 imported-body paint, nine-pack fallback, custom paint-table
  cloning, selectable errors and Simple/Advanced importer work.

Validation: the model-import smoke creates a custom `spn` car from a decorated
donor, verifies the copied decals and remapped paint table, and opens every
decorated paint through the editor appearance model. Unit, import and packaging
tests pass; Windows, Linux and AnyLinux packages are built and smoke-tested.

Limitations: copied donor decals use the imported model's UV layout, so their
placement may differ from the donor and should be checked in game. Persisted
import preferences and field-level validation remain follow-ups.

---

# RiffRacer Customs 0.3.0-beta.9

## In-game summary

- Imported-body paint is on by default, with a safe full-table fallback.
- Custom car IDs keep cloned paint tables even when no slot is free.
- Error text is selectable and the sidebar reaches the window bottom.

## Full release notes

- Enable **Add imported-body paint** by default and place it in the Simple
  import view with an explanation. The paint name stays in Advanced and follows
  the checkbox state.
- When the donor already has nine paint packs, the imported-body paint now
  replaces the donor's default paint pack instead of being skipped. Both paint
  CFGs and the 32-bit paint Lua are updated.
- Always clone the donor paint table under a custom authored car ID, even when
  the optional extra paint is disabled or cannot open a new slot. This fixes the
  editor error `key '<id>' not found` after importing a nine-pack donor.
- Make error dialogs and the import status text selectable so messages can be
  copied. Move the editor status line under the preview so the sidebar scroll
  area reaches the bottom of the window.
- Keep the beta.8 custom-ID tokens, Simple/Advanced importer, tabs and release
  validation work.

Validation: unit and import-dialog tests cover nine-pack default replacement,
custom paint-table and Lua cloning, default-on state, custom IDs, Simple/Advanced
switching and error-path startup. Windows, Linux and AnyLinux packages are built
and smoke-tested.

Limitations: persisted import preferences and field-level inline validation are
not implemented yet. Imported-body paint replaces the donor default rather than
creating a tenth pack because the composer supports at most nine.

---

# RiffRacer Customs 0.3.0-beta.8

## In-game summary

- Use any custom three-letter car ID when adding a model.
- Import models in a simple view with advanced options on demand.
- Import controls wrap cleanly without a horizontal scrollbar.

## Full release notes

- Allow **Add as a new car** to use any three-letter authored ID, such as `spn`.
  The importer writes a matching `car_<id>` token for all eight locales into the
  exported package. The stock archive and its `tokens.dat` stay untouched, and
  paint names still reuse existing stock paint tokens.
- Add a **Simple** import view with car name, auto-selected donor, export mode,
  generated car ID and detail level. **Add as a new car** is the default and
  leaves the stock car unchanged. The car name is prefilled from GLB metadata.
- Move scale, orientation, Z-up, donor ranking, authored ID, fitting, collision,
  effects, paint mask/slot, shadow, materials and part roles behind **Show
  advanced options**. Part roles are collapsed into **Review part roles**.
- **Create mod and edit** now previews automatically when settings changed, so a
  separate Preview step is not required. The import controls also use wrapped
  labels and no horizontal scrollbar.
- Keep the beta.7 livery template, sidebar tabs, placement shortcuts, meter
  selector and release validation work.

Validation: automated tests cover custom car IDs and locale tokens, generated
IDs, Simple/Advanced switching, advanced visibility, preview invalidation and
automatic export preparation. Import dialogs and advanced options are captured
for review. Windows, Linux and AnyLinux packages are built and smoke-tested.

Limitations: persisted import preferences and per-field inline validation are
not implemented yet. The livery template remains a guide only, and curvature
following remains a local normal average.

---

# RiffRacer Customs 0.3.0-beta.7

## In-game summary

- Add a transparent livery template with UV guide outlines.
- Paint, Livery and HUD tools now live in sidebar tabs.
- Drag the sidebar divider to make the preview larger or smaller.

## Full release notes

- Add **Add livery template** in the Livery tab. It creates a transparent
  DXT5 livery containing cyan UV island outlines for the paint mesh only.
  Export it with **Save PNG / blank template**, use it as a guide layer in an
  image editor, delete the guide layer, then import the finished livery.
- Replace the Paint/Livery expander and **Adjust HUD / boosters** button with
  native **Paint**, **Livery** and **HUD** tabs. Paint holds names, prices,
  colors, defaults and reset; Livery holds PNG import/save, templates and
  test/UV artwork; HUD holds multiplier brackets, meter shape and attachment
  placement. Switching away from HUD applies the active adjustment and hides
  placement guides.
- Make the sidebar/preview divider a wide draggable handle for resizing the
  preview viewport. Long sidebar labels wrap, and the horizontal scrollbar is
  removed while vertical scrolling remains for overflow.
- Keep the beta.6 placement shortcuts, meter selector, importer and release
  validation work.

Validation: automated tests cover the transparent UV guide, tab startup and
switching, HUD placement activation, and viewport resizing. UI screenshots cover
the Paint, Livery and HUD tabs. Windows, Linux and AnyLinux packages are built
and smoke-tested.

Limitations: the livery template is a guide only; its cyan lines will render in
game if the template is not replaced. Curvature following remains a local normal
average, and HUD style controls require a native HUD atlas for the current car.

---

# RiffRacer Customs 0.3.0-beta.6

## In-game summary

- Snap HUD parts with Space and follow body curves with Ctrl+Space.
- Reset the selected attachment instantly with Ctrl+Shift+Z.
- Choose from 13 native boost meter shapes with a live preview.

## Full release notes

- Add placement shortcuts: **Space** snaps the selected HUD element or boost
  outlet to the nearest exposed body surface, and **Ctrl+Space** also averages
  surrounding outward faces so the part follows local body curvature. The
  footprint is lifted over curved panels. **Ctrl+Shift+Z** resets the selected
  attachment to its imported transform as one undoable step. Enabling placement
  focuses the preview so the shortcuts work immediately.
- Rename the former **Add PNG decal** action to **Add clear PNG livery** and
  place it directly below **Import PNG livery**.
- Add a **Boost meter shape** selector beside the multiplier style control. It
  lists the 13 distinct native stock `car_meter` artworks, labelled by source
  car, with a live preview. Copies only the original `car_meter` DDS payload;
  other HUD textures and bytes stay unchanged.
- Compose the boost meter and multiplier choices over the imported HUD atlas, so
  changing one never discards the other. **Keep imported / original** restores
  only the selected artwork. Choices retain while switching cars and are included
  in exported mods.
- Keep the earlier importer, authored-ID, nine-pack paint, texture, collision,
  shadow and locale changes from beta.5.

Validation: automated tests cover the new shortcuts, curved-surface normal
averaging, clear-livery placement, all 13 meter swaps, composed styles,
export/reimport and UI retention. Placement controls and the selector were
captured at the standard window size. Windows, Linux and AnyLinux packages are
built and smoke-tested.

Limitations: curvature following is a local normal average, not a curved decal
projection; check seams, open windows and sharply curved bodywork in game. Meter
and multiplier controls require a native HUD atlas for the current car. The HUD
preview remains static and approximate.

---

# RiffRacer Customs 0.3.0-beta.5

## In-game summary

- Build a custom car from a GLB inside the editor.
- Match vanilla detail or choose higher polygon and texture budgets.
- Generate collision, effects, paint slots and names automatically.

## Full release notes

- Import static GLB 2.0 cars end to end: geometry conversion, donor fitting,
  base-color textures, native materials, collision, effects, paint, names and
  packaging.
- Choose a detail level: **Light**, **Vanilla-matched**, **High** or **Full
  source**. Quadric edge-collapse simplification protects UV seams and
  boundaries; imported meshes are merged by node and material. Normal levels
  keep the painted body intact and fit the rest to the donor triangle budget.
- Import base-color PNG textures: power-of-two resize, full mip chains,
  DXT1/DXT5 encoding and TABT atlas rebuild. **Materials…** sets the native
  template, texture size (256–2048) and finish (matte, satin, glossy or
  chrome) per category, or disables a texture. DXT colour endpoints now use a
  principal-axis and least-squares fit instead of a bounding box.
- Fit the donor collision hull to the imported wheel width and body length and
  roof, preserve ground clearance, and update the hull's broadphase bounds.
  Fit the ground shadow and garage selection box to the same bounds.
- Rebuild the `zone_front`, `electric` and `zone_back` effect shells from the
  imported exterior with the donor effect materials, stride-28 layout and
  projected UVs.
- Replace `carpaint_occspec` with a uniform neutral mask and add one extra
  decal-free paint priced at 1, cloned from the darkest donor pack. Update both
  paint CFGs and the 32-bit Lua, respecting the client's nine-pack limit.
- Write the car and paint names into all eight string banks, and generate
  `ATTRIBUTION.txt` from the GLB's author, licence, title and source metadata.
- **Add as a new car** accepts an authored three-letter ID and namespaces its
  assets, paint table, origin mapping and names. It requires a stock name token
  such as `dod`; the ReRiff composer remaps it to a free native slot.
- Keep the earlier toolbar, scrollbar and in-app updater changes from beta.4.

Validation: GLB parsing, material/category mapping, texture PNG/DXT/atlas,
decimation, paint configuration, collision/shadow fitting, locale names,
attribution and updater rules are covered by automated tests. The Impala GLB
was converted against multiple donors and reimported; the authored-ID export,
nine-pack paint limit, neutral mask, Lua and fitted shadow/box were checked.
Windows, Linux and AnyLinux packages are built and smoke-tested.

Limitations: the imported car still needs an in-game check of textures, glow,
collision and the extra paint. Source metallic/roughness, normal, occlusion and
emissive maps, vertex colours and base-colour factors are not converted.
Collision is an affine fit of the donor hull, effect UVs and fades are
experimental, and the extra paint is skipped when the donor already has nine
packs. Add mode needs a stock name token. The import preview shows geometry and
material colours, not the generated textures. Standalone packages patch the
32-bit paint Lua; the ReRiff composer regenerates both Lua variants.

---

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
