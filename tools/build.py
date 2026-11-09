#!/usr/bin/env python3
"""Standalone Cycle 03 builds. All compiler sources/headers are local to this checkout."""
import argparse,hashlib,json,os,platform,shlex,shutil,subprocess,sys
from pathlib import Path
import native_sources
ROOT=Path(__file__).resolve().parents[1]
from native_profile import PROFILE,preflight
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def build(target):
    extra=preflight()
    if target=='check':print('PASS local submodule inputs, pinned revisions and build-wide profile flags');return
    if target=='app' and platform.system()!='Darwin':raise ValueError('The native application target requires macOS and the Apple SDK; use cli on other platforms')
    subprocess.run([sys.executable,str(ROOT/'tools/compile_occupations.py'),'--write'],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'tools/compile_controls.py')],cwd=ROOT,check=True)
    from checkpoint_abi import build as checkpoint_abi
    checkpoint_abi()
    compiler=shlex.split(os.environ.get('CC','cc'))
    if not compiler or not shutil.which(compiler[0]):raise ValueError('C compiler unavailable; install the platform C toolchain or set CC')
    if target=='app':
        subprocess.run([sys.executable,str(ROOT/'tools/build_basemap.py'),'--write'],cwd=ROOT,check=True)
        subprocess.run([sys.executable,str(ROOT/'tools/compile_runtime.py'),'--write'],cwd=ROOT,check=True)
    sources=[ROOT/p for p in native_sources.sources(target)]
    command=[*compiler,*PROFILE,*extra,'-Iapesdk','-Isrc','-include','src/cd_api.h','-std=c11',*shlex.split(os.environ.get('CFLAGS','-O2 -g'))]
    if target=='app':command+=['-fobjc-arc']
    command+=[str(p.relative_to(ROOT)) for p in sources]
    if target=='app':command+=['-framework','Cocoa']
    command+=['-lz','-lm','-lpthread']
    output=ROOT/({'cli':'build/CanterburyDetails','smoke':'build/smoke-test','app':'build/CanterburyDetails.app/Contents/MacOS/CanterburyDetails'}[target])
    inputs=set(sources)|set((ROOT/'src').glob('*.h'))|set((ROOT/'apesdk').rglob('*.h'))|set((ROOT/'apesdk').rglob('*.inc'))|{ROOT/'tools/build.py',ROOT/'tools/native_sources.py',ROOT/'tools/native_profile.py',ROOT/'content/source-manifest.json',ROOT/'tools/compile_occupations.py',ROOT/'content/occupation-policy.json',ROOT/'build/content/OccupationCatalogue.inc',ROOT/'build/content/CheckpointABI.inc',ROOT/'tools/checkpoint_abi.py'}
    if target=='app':inputs.update([ROOT/'ui/Info.plist',ROOT/'ui/CityMap.h',ROOT/'ui/CitySession.h',ROOT/'ui/CityManagement.h',*(ROOT/p for p in native_sources.RESOURCES)])
    identity={'command':command,'compiler':subprocess.check_output([*compiler,'--version'],text=True).splitlines()[0],'sources':{str(p.relative_to(ROOT)):sha(p) for p in sorted(inputs)}}
    fingerprint=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest();stamp=ROOT/f'build/{target}-build.json'
    if output.is_file() and stamp.is_file():
        old=json.loads(stamp.read_text())
        metadata_ok=target!='app' or ((output.parents[1]/'Info.plist').is_file() and sha(output.parents[1]/'Info.plist')==sha(ROOT/'ui/Info.plist') and all((output.parents[1]/'Resources'/Path(p).name).is_file() and sha(output.parents[1]/'Resources'/Path(p).name)==sha(ROOT/p) for p in native_sources.RESOURCES))
        if metadata_ok and old.get('source_fingerprint')==fingerprint and old.get('output_sha256')==sha(output):print(json.dumps({'target':target,'output':str(output.relative_to(ROOT)),'cached':True}));return
    output.parent.mkdir(parents=True,exist_ok=True);temporary=output.with_name(output.name+'.tmp')
    try:
        subprocess.run([*command,'-o',str(temporary.relative_to(ROOT))],cwd=ROOT,check=True)
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    if target=='app':
        shutil.copyfile(ROOT/'ui/Info.plist',output.parents[1]/'Info.plist')
        resources=output.parents[1]/'Resources';resources.mkdir(exist_ok=True)
        for p in native_sources.RESOURCES:shutil.copyfile(ROOT/p,resources/Path(p).name)
    stamp.write_text(json.dumps({'target':target,'source_fingerprint':fingerprint,'output_sha256':sha(output),'profile':PROFILE,'inputs':identity},indent=2)+'\n')
    print(json.dumps({'target':target,'output':str(output.relative_to(ROOT)),'cached':False}))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('target',choices=['cli','app','smoke','check']);a=p.parse_args()
    try:build(a.target)
    except (ValueError,OSError,subprocess.CalledProcessError) as error:print('Build error: '+str(error),file=sys.stderr);raise SystemExit(2)
if __name__=='__main__':main()
