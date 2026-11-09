#!/usr/bin/env python3
"""Build a standalone ordinary C core archive and independent mock frontend."""
import hashlib,json,os,shlex,subprocess,shutil
from pathlib import Path
from native_sources import ROOT,CORE,RESOURCES,sdk_sources
from native_profile import PROFILE,preflight
from checkpoint_abi import build as abi
PUBLIC=['cd_api.h','cd_city.h','cd_human.h','cd_work.h','cd_civic.h','cd_observer.h','cd_management.h','cd_core.h']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build():
 extra=preflight();subprocess.run(['python3','tools/compile_occupations.py','--write'],cwd=ROOT,check=True);abi()
 compiler=shlex.split(os.environ.get('CC','cc'));flags=[*PROFILE,*extra,'-O2','-std=c11','-Iapesdk','-Isrc','-include','src/cd_api.h'];out=ROOT/'build/core';out.mkdir(parents=True,exist_ok=True)
 sources=CORE+sdk_sources();inputs=sorted(set(sources+[str(p.relative_to(ROOT)) for p in (ROOT/'src').glob('*.h')]+[str(p.relative_to(ROOT)) for p in (ROOT/'apesdk').rglob('*.h')]+['content/source-manifest.json','build/content/CheckpointABI.inc','build/content/OccupationCatalogue.inc','tools/build_core.py','tools/native_sources.py']))
 identity={'sources':{p:sha(ROOT/p) for p in inputs},'compiler':subprocess.check_output([*compiler,'--version'],text=True).splitlines()[0],'flags':flags};key=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest();stamp=out/'manifest.json';archive=out/'libCanterburyCore.a'
 old=json.loads(stamp.read_text()) if stamp.exists() else {}
 if not archive.exists() or old.get('fingerprint')!=key or old.get('archive_sha256')!=sha(archive):
  objects=[]
  for source in sources:
   obj=out/'objects'/Path(source).with_suffix('.o');obj.parent.mkdir(parents=True,exist_ok=True);objects.append(str(obj));subprocess.run([*compiler,*flags,'-c',source,'-o',str(obj)],cwd=ROOT,check=True)
  temporary=out/'libCanterburyCore.tmp.a';temporary.unlink(missing_ok=True);subprocess.run(['ar','rcs',str(temporary),*objects],cwd=ROOT,check=True,env={**os.environ,'ZERO_AR_DATE':'1'});temporary.replace(archive)
 headers=out/'include';headers.mkdir(exist_ok=True)
 for name in PUBLIC:(headers/name).write_bytes((ROOT/'src'/name).read_bytes())
 subprocess.run([*compiler,*PROFILE,'-std=c11','-O2','-I'+str(headers),'examples/ordinary_frontend.c',str(archive),'-lz','-lm','-lpthread','-o',str(out/'OrdinaryFrontend')],cwd=ROOT,check=True)
 resources=out/'resources';resources.mkdir(exist_ok=True)
 for path in RESOURCES:shutil.copy2(ROOT/path,resources/Path(path).name)
 shutil.copy2(ROOT/'build/map/render.json',resources/'render.json');shutil.copy2(ROOT/'examples/ordinary_frontend.c',out/'ordinary_frontend.c')
 result={'format':'canterbury-core-package','version':1,'interface':1,'fingerprint':key,'archive_sha256':sha(archive),'headers':{p:sha(headers/p) for p in PUBLIC},'resources':{p.name:sha(p) for p in resources.iterdir() if p.is_file()},'inputs':identity,'frontend_sha256':sha(out/'OrdinaryFrontend'),'excluded':['Cocoa frontend','native app main','CLI main','offline renderer main'],'runtime_dependencies':['system C library','zlib','math','pthread'],'sdk_singleton':True};stamp.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'archive':str(archive.relative_to(ROOT)),'fingerprint':key}))
 return result
if __name__=='__main__':build()
