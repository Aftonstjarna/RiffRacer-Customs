import pathlib,struct,sys
p=pathlib.Path(sys.argv[1])/'assets/scripts/config'
b=(p/'paint_pc.luac').read_bytes();u=lambda off:struct.unpack_from('<I',b,off)[0]
assert b[:12]==b'\x1bLua\x51\0\1\4\4\4\4\0'
n=u(28);code=[u(32+i*4) for i in range(n)];at=32+4*n;count=u(at);at+=4;constants=[]
for _ in range(count):
 t=b[at];at+=1
 if t==0:v=None
 elif t==1:v=bool(b[at]);at+=1
 elif t==3:v=struct.unpack_from('<f',b,at)[0];at+=4
 elif t==4:length=u(at);at+=4;v=b[at:at+length-1].decode();assert b[at+length-1]==0;at+=length
 else:raise ValueError(t)
 constants.append(v)
assert b[at:]==bytes(16)
r={};g={};rk=lambda x:constants[x-256] if x>=256 else r[x]
for inst in code:
 op=inst&63;a=(inst>>6)&255;c=(inst>>14)&511;v=(inst>>23)&511;bx=inst>>14
 if op==1:r[a]=constants[bx]
 elif op==5:r[a]=g[constants[bx]]
 elif op==6:r[a]=r[v][rk(c)]
 elif op==7:g[constants[bx]]=r[a]
 elif op==9:r[a][rk(v)]=rk(c)
 elif op==10:r[a]={}
 elif op==34:
  assert c>0
  for j in range(1,v+1):r[a][(c-1)*50+j]=r[a+j]
 elif op==30:break
 else:raise ValueError(('opcode',op))
b=(p/'paint_pc.cfg').read_bytes();u=lambda off:struct.unpack_from('<I',b,off)[0];pool=20+u(12)*32
text=lambda off:b[pool+off:b.index(0,pool+off)].decode();checks=0
def compare(start,count,table):
 global checks
 for j,i in enumerate(range(start,start+count)):
  off=20+i*32;key=text(u(off)) or j+1;kind=u(off+20)
  if kind==2:compare(u(off+8),u(off+24),table[key])
  else:
   value=struct.unpack_from('<f',b,off+8)[0] if kind==1 else text(u(off+8))
   assert table[key]==value,(key,value,table[key]);checks+=1
compare(0,u(16),g);print('PASS independent Lua 5.1 instruction evaluation:',checks,'CFG values match',sys.argv[1])
