from pathlib import Path
import re,struct,json,hashlib
p=Path(__file__).parent
exe=Path('C:/Program Files/EA Games/The Sims 2 Ultimate Collection/Fun with Pets/SP9/TSBin/TheSims2.exe')
b=exe.read_bytes();pe=struct.unpack_from('<I',b,60)[0];base=struct.unpack_from('<I',b,pe+52)[0]
start=pe+24+struct.unpack_from('<H',b,pe+20)[0];sections=[]
for i in range(struct.unpack_from('<H',b,pe+6)[0]):
 a=start+i*40;vs,rva,n,off=struct.unpack_from('<4I',b,a+8)
 if struct.unpack_from('<I',b,a+36)[0]&0x20000000:sections.append((base+rva,b[off:off+n]))
s='\n'.join((p/n).read_text() for n in ['large_maps.cpp','water_fix.inc','lot_water_fix.inc','terrain_strip.inc','strip_signatures.inc','importer.inc','placement_fix.inc','placement_signatures.inc'])
patterns={n:bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',v)) for n,v in re.findall(r'unsigned char (\w+)_signature\[\]=\{(.*?)\};',s,re.S)}
hits={}
for n,pat in patterns.items():
 masked=set(range(13,17)) if n in ('validator','import') else set()
 if n=='import':masked|=set(range(47,51))|set(range(57,61))
 found=[]
 for va,data in sections:
  cursor=0;prefix=pat[:13] if masked else pat
  while True:
   at=data.find(prefix,cursor)
   if at<0:break
   if at+len(pat)<=len(data) and all(j in masked or data[at+j]==x for j,x in enumerate(pat)):found.append(va+at)
   cursor=at+1
 assert len(found)==1,(n,found)
 hits[n]=hex(found[0])
def read(va,n):
 for base,data in sections:
  if base<=va and va+n<=base+len(data):return data[va-base:va-base+n]
 raise AssertionError(hex(va))
for sig,off,pop in re.findall(r'resolve_signature\((\w+)_signature,sizeof\(\w+_signature\),(0x[0-9a-f]+),(\d+),false\)',s):
 pop=int(pop);expected=bytes([0xc2,pop,0]) if pop else b'\xc3'
 assert read(int(hits[sig],16)+int(off,16),len(expected))==expected
assert int(hits['position_factory'],16)==0xe810f0
for forbidden in ['log_line','write_blob','capture_source','Direct3DCreate9','DrawIndexedPrimitive','_wfopen','CreateDirectoryW']:
 assert forbidden not in s,forbidden
out={'result':'PASS','executable_sha256':hashlib.sha256(b).hexdigest(),'unique_signatures':hits,'return_instructions':'verified','diagnostic_facilities':'absent from production source','asi_sha256':hashlib.sha256((p/'TS2-LargeMaps.asi').read_bytes()).hexdigest()}
(p/'signature-validation.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
