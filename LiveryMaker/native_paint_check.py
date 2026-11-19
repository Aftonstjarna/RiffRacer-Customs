#!/usr/bin/env python3
"""Generate native runtime expectations from a paint export's CFG.

Usage: native_paint_check.py MOD_FOLDER OUTPUT_FOLDER
Pass expected.bin and check.lua to native-probe.exe with the luac resource path.
Only the exported CFG supplies expectations; the Lua chunk is executed by the
original game runtime, independently of the authoring implementation.
"""
import zipfile, struct, pathlib, json, sys
src=pathlib.Path(sys.argv[1]); output=pathlib.Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)
if (src/'assets').is_dir():
 b=(src/'assets/scripts/config/paint_pc.cfg').read_bytes();(output/'expected.bin').write_bytes((src/'assets/scripts/config/paint_pc.luac').read_bytes())
else:
 with zipfile.ZipFile(src/'vfs.zip') as z:
  b=z.read('scripts/config/paint_pc.cfg');(output/'expected.bin').write_bytes(z.read('scripts/config/paint_pc.luac'))
u=lambda o:struct.unpack_from('<I',b,o)[0];s=20+u(12)*32;checks=[]
def walk(start,count,path=''):
 for j,i in enumerate(range(start,start+count)):
  o=20+i*32;n=s+u(o);name=b[n:b.index(0,n)].decode();p=(path+'['+(json.dumps(name) if name else str(j+1))+']') if path else name;t=u(o+20)
  if t==2:walk(u(o+8),u(o+24),p)
  else:
   if t==1:v=repr(struct.unpack_from('<f',b,o+8)[0])
   else:n=s+u(o+8);v=json.dumps(b[n:b.index(0,n)].decode())
   checks.append('if '+p+' ~= '+v+' then paint_check_failed() end')
walk(0,u(16));(output/'check.lua').write_text('\n'.join(checks));print(len(checks),'native value assertions')
