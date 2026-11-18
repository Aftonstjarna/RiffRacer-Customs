#!/usr/bin/env python3
"""Bundle native Windows GTK dependencies; never include game assets."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--prefix', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
version = (root / 'VERSION').read_text().strip()
name = f'RiffRacer-Customs-{version}-windows-x64'
stage = root / 'build' / name
if stage.exists():
    shutil.rmtree(stage)
stage.mkdir(parents=True)
prefix = args.prefix.resolve()
shutil.copy2(root/'build/windows/RiffRacer-Customs.exe', stage/'RiffRacer-Customs.exe')
shutil.copy2(prefix/'bin/gdk-pixbuf-query-loaders.exe', stage/'gdk-pixbuf-query-loaders.exe')
for relative in ['lib/gdk-pixbuf-2.0/2.10.0/loaders', 'share/glib-2.0/schemas',
                 'share/icons/Adwaita', 'share/icons/hicolor', 'share/licenses']:
    source = prefix/relative
    if not source.exists():
        raise RuntimeError(f'Missing runtime data: {source}')
    shutil.copytree(source, stage/relative)
for relative in ['etc/fonts', 'share/fontconfig']:
    if (prefix/relative).exists():
        shutil.copytree(prefix/relative, stage/relative)
pending = list(stage.rglob('*.exe')) + list(stage.rglob('*.dll'))
seen = set()
while pending:
    binary = pending.pop()
    key = binary.name.lower()
    if key in seen:
        continue
    seen.add(key)
    output = subprocess.check_output([str(prefix/'bin/objdump.exe'), '-p', str(binary)], text=True)
    for dll in re.findall(r'DLL Name:\s*(\S+)', output):
        source = prefix/'bin'/dll
        if source.is_file():
            dest = stage/dll
            if not dest.exists():
                shutil.copy2(source, dest)
                pending.append(dest)
        elif not dll.lower().startswith(('api-ms-', 'ext-ms-')) and not (Path(os.environ['SystemRoot'])/'System32'/dll).exists():
            raise RuntimeError(f'Unresolved import {dll} in {binary}')
shutil.copy2(root/'LICENSE', stage/'LICENSE.txt')
shutil.copy2(root/'NativeLinux/third_party/nlohmann/LICENSE.MIT', stage/'JSON-LICENSE.txt')
shutil.copy2(root/'RELEASE-NOTES.md', stage/'RELEASE-NOTES.md')
(stage/'README.txt').write_text('RiffRacer Customs '+version+'\n\nExtract the entire ZIP, then double-click RiffRacer-Customs.exe.\nKeep the DLLs, lib and share folders alongside the program.\nOpen your Riff Racer folder containing vfs.zip, then select a car and paint.\nExport into a named mod folder, copy it into the game mods folder, and restart.\n\nPNG artwork must match the selected original texture size.\nShared decals affect all listed paints. Paint mods do not merge with other\nmods replacing the paint configuration/script. No game assets are included.\n\nGTK and its dependencies are dynamically linked. Licenses are under\nshare/licenses; replacement compatible DLLs can be used alongside the program.\nBuild dependencies and their source-package locations are recorded in\nRUNTIME-PACKAGES.txt.\n', encoding='utf-8')
# Pacman records exact runtime/build package versions for reproducibility.
packages = subprocess.check_output([str(prefix.parent/'usr/bin/pacman.exe'), '-Q'], text=True)
(stage/'RUNTIME-PACKAGES.txt').write_text('Built with MSYS2 UCRT64 packages:\n'+packages+'\nPackage sources and build recipes:\nhttps://repo.msys2.org/mingw/sources/\nhttps://github.com/msys2/MINGW-packages\n', encoding='utf-8')
manifest = '\n'.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(stage).as_posix() for p in sorted(stage.rglob('*')) if p.is_file())+'\n'
(stage/'FILES.sha256').write_text(manifest, encoding='utf-8')
dist = root/'dist'
dist.mkdir(exist_ok=True)
archive = dist/(name+'.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as output:
    for path in sorted(stage.rglob('*')):
        if path.is_file():
            output.write(path, name+'/'+path.relative_to(stage).as_posix())
(dist/'SHA256SUMS').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n', encoding='utf-8')
print(stage)
