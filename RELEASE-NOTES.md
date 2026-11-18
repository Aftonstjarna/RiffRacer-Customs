# RiffRacer Customs 0.1.0

- Portable Windows x64 editor, with the GTK runtime included.
- Edit body, highlight and wheel paint colors using in-game car/paint names.
- Import exact-size PNG artwork and export native livery mods.
- Preview textures, export UV templates and retain the last game-folder path.
- Export through ordinary symlinked parent folders with overwrite safeguards.
- Include the native Lua paint resource required for in-game paint changes.

Extract the complete Windows ZIP and launch `RiffRacer-Customs.exe`.
Linux users can build from source; see the README.

Paint edits are retained during the session but must be exported before closing.
Shared decals affect every paint listed by the editor. Separate paint mods do
not merge. The preview does not simulate the game's shaders or lighting.
The editor replaces existing paints/liveries; it does not add new garage slots.
No original game files are included, and authoring does not modify game saves.
