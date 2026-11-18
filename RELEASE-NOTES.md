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
