#!/usr/bin/env python3
"""Independently check a C++-generated one-resource VFS pair (stdlib only)."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile

folder = Path(sys.argv[1])
raw = (folder / 'vfs.dat').read_bytes()
metadata = json.loads((folder / 'livery-maker.json').read_text())
magic, nodes, count, strings, checksum = struct.unpack_from('<5I', raw)
assert magic == 0x0300ffee and 0 < count < 0x3fff
assert len(raw) == 28 + nodes * 8 + count * 28 + strings
pool = raw[28 + nodes * 8 + count * 28:]
seen, files = set(), {}


def walk(index, prefix):
    while index != 0xffff:
        assert index < nodes and index not in seen
        seen.add(index)
        name, sibling, child, flags = struct.unpack_from('<4H', raw, 28 + index * 8)
        part = pool[name:pool.index(b'\0', name)].decode('ascii')
        assert part and part not in ('.', '..') and part == part.lower()
        path = prefix + part
        if flags == 0x3fff:
            walk(child, path + '/')
        else:
            assert flags < count and child == 0xffff
            files[flags] = path
        index = sibling


walk(struct.unpack_from('<H', raw, 24)[0], '')
assert len(seen) == nodes
with zipfile.ZipFile(folder / 'vfs.zip') as archive:
    assert archive.testzip() is None
    entries = archive.infolist()
    assert len(entries) == count
    assert checksum == sum(i.CRC for i in entries) & 0xffffffff
    central_offset = 0
    for index, info in enumerate(entries):
        assert info.filename == files[index]
        assert struct.unpack_from('<7I', raw, 28 + nodes * 8 + index * 28) == (
            2, info.compress_type, info.compress_size, info.file_size,
            len(info.filename), info.header_offset, central_offset)
        central_offset += 46 + len(info.filename) + len(info.extra) + len(info.comment)
        batch = archive.read(info)
        expected = next((r['sha256'] for r in metadata.get('resources', []) if r['path'] == info.filename), metadata.get('batch_sha256'))
        assert hashlib.sha256(batch).hexdigest() == expected
        if info.filename.endswith('.luac'):
            assert batch[:12] == bytes.fromhex("1b4c75615100010404040400")
            continue
        if info.filename.endswith('.cfg'):
            magic, version, pool, records, roots = struct.unpack_from('<5I', batch)
            assert magic == 0x46434647 and version == 3 and roots <= records
            assert len(batch) == 20 + records * 32 + pool
            continue
        assert batch[:4] == b'TABT'
        payload, textures, names, length = struct.unpack_from('<4I', batch, 12)
        assert textures == 1 and payload == 28 + 24 + names
        assert payload + length == len(batch)
        _, offset, size, width, height, flags = struct.unpack_from('<6I', batch, 28)
        dds = batch[payload + offset:payload + offset + size]
        assert dds[:4] == b'DDS ' and flags == 2
        assert struct.unpack_from('<II', dds, 12) == (height, width)
        assert dds[84:88] in (b'DXT3', b'DXT5')
        levels = max(1, struct.unpack_from('<I', dds, 28)[0])
        assert size == 128 + sum(max(1, (max(1, width >> i) + 3) // 4) *
                                 max(1, (max(1, height >> i) + 3) // 4) * 16
                                 for i in range(levels))
for name, key in [('vfs.dat', 'dat_sha256'), ('vfs.zip', 'zip_sha256')]:
    assert hashlib.sha256((folder / name).read_bytes()).hexdigest() == metadata[key]
print('PASS independent native index, ZIP CRCs/offsets, TABT, DDS/mips and ownership hashes')
