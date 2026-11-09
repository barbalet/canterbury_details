#!/usr/bin/env python3
"""Render the native basemap using the exact upstream json2png.c core."""
import argparse,hashlib,json,os,shlex,shutil,subprocess,tempfile
from pathlib import Path
from native_profile import preflight
ROOT=Path(__file__).resolve().parents[1]
OUTPUT=ROOT/'build/map/CanterburyMap.png'
STAMP=ROOT/'build/map/render.json'
FONT=Path('/System/Library/Fonts/Supplemental/Times New Roman Italic.ttf')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args):return subprocess.check_output(list(map(str,args)),cwd=ROOT,text=True).strip()
def build(check=False):
    preflight()
    pkg=shutil.which('pkg-config');compiler=shlex.split(os.environ.get('CC','cc'))
    if not pkg:raise ValueError('json2png requires pkg-config, libpng and FreeType; see docs/XCODE.md')
    if not FONT.is_file():raise ValueError('json2png label font is missing: '+str(FONT))
    flags=shlex.split(run([pkg,'--cflags','--libs','libpng','freetype2']))
    paths=['canterbury/newcanterbury/json2png.c','canterbury/newcanterbury/map.h','canterbury/newcanterbury/canterbury.json','tools/build_basemap.py','tools/native_profile.py','content/source-manifest.json']
    identity={'sources':{p:sha(ROOT/p) for p in paths},'font_sha256':sha(FONT),'font':str(FONT),'labels':True,'compiler':run([*compiler,'--version']).splitlines()[0],'libraries':run([pkg,'--modversion','libpng','freetype2']),'flags':flags}
    fingerprint=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()
    if OUTPUT.is_file() and STAMP.is_file():
        old=json.loads(STAMP.read_text())
        if old.get('fingerprint')==fingerprint and old.get('png_sha256')==sha(OUTPUT):return {'cached':True,'png_sha256':sha(OUTPUT),'renderer':'canterbury/newcanterbury/json2png.c'}
    if check:raise ValueError('json2png basemap is missing/stale; run python3 tools/build_basemap.py --write')
    OUTPUT.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=OUTPUT.parent,prefix='render-') as folder:
        work=Path(folder);exe=work/'json2png';png=work/'CanterburyMap.png'
        subprocess.run([*compiler,'-std=c11','-O2','canterbury/newcanterbury/json2png.c',*flags,'-lm','-o',str(exe)],cwd=ROOT,check=True)
        subprocess.run([str(exe),'canterbury/newcanterbury/canterbury.json',str(png),'--text','--font',str(FONT)],cwd=ROOT,check=True)
        for p,expected in identity['sources'].items():
            if sha(ROOT/p)!=expected:raise ValueError('Renderer input changed while rendering: '+p)
        png.replace(OUTPUT)
    STAMP.write_text(json.dumps({'fingerprint':fingerprint,'png_sha256':sha(OUTPUT),'inputs':identity},indent=2)+'\n')
    return {'cached':False,'png_sha256':sha(OUTPUT),'renderer':'canterbury/newcanterbury/json2png.c'}
def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--write',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    try:print(json.dumps(build(a.check),sort_keys=True))
    except (ValueError,OSError,subprocess.CalledProcessError) as e:p.exit(2,'Basemap error: '+str(e)+'\n')
if __name__=='__main__':main()
