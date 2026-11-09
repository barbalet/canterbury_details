#!/usr/bin/env python3
"""Generate a strict local checkpoint ABI identity and exhaustive world pointer list."""
import hashlib,re,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def build():
 paths=sorted([*ROOT.glob('src/*.h'),*ROOT.glob('apesdk/**/*.h'),ROOT/'content/source-manifest.json',Path(__file__)])
 digest=hashlib.sha256(b''.join(str(p.relative_to(ROOT)).encode()+b'\0'+p.read_bytes() for p in paths)).hexdigest()
 text=(ROOT/'src/cd_city_internal.h').read_text().split('struct CdWorld {',1)[1].split('\n};',1)[0]
 pointers=re.findall(r'\*\s*(\w+)',text)
 owned=['people','homes','housing','visitors','crowd','traffic','owners','closed','locks','premises','home_meals','home_raw','life','jobs','projects','institutions','home_wallet','home_goods','offices','rosters','services','gatherings','vehicles','pressures','records','record_cursors','operations']
 assert set(pointers)==set(owned+['content','native','distance','previous','heap','heap_pos']),pointers
 output='/* Generated; same ABI/content only, no implicit migrations. */\nstatic const char cd_checkpoint_abi[]="'+digest+'";\n#define CD_WORLD_POINTERS(X) '+' '.join('X('+p+')' for p in pointers)+'\n'
 path=ROOT/'build/content/CheckpointABI.inc';path.parent.mkdir(parents=True,exist_ok=True)
 if not path.exists() or path.read_text()!=output:path.write_text(output)
 return digest
if __name__=='__main__':print(json.dumps({'checkpoint_abi':build()}))
