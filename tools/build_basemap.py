#!/usr/bin/env python3
"""Render the native basemap using the exact upstream json2png.c core."""
import argparse,hashlib,json,os,shlex,shutil,subprocess,tempfile
from assembled_map import INPUTS,model,reference_matches
from building_placements import corrected_objects
from pathlib import Path
from native_profile import preflight
ROOT=Path(__file__).resolve().parents[1]
OUTPUT=ROOT/'build/map/CanterburyMap.png'
STAMP=ROOT/'build/map/render.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args):return subprocess.check_output(list(map(str,args)),cwd=ROOT,text=True).strip()
def build(check=False):
    preflight()
    pkg=shutil.which('pkg-config');compiler=shlex.split(os.environ.get('CC','cc'))
    if not pkg:raise ValueError('json2png requires pkg-config, libpng and FreeType; see docs/XCODE.md')
    flags=shlex.split(run([pkg,'--cflags','--libs','libpng','freetype2']))
    paths=['canterbury/newcanterbury/json2png.c','canterbury/newcanterbury/map.h','tools/json2png2.c','tools/assembled_map.py','tools/build_basemap.py','tools/native_profile.py','content/source-manifest.json']
    paths+=INPUTS+['canterbury/simulation-map/derived/buildings-enriched.json']
    identity={'sources':{p:sha(ROOT/p) for p in paths},'map':'final assembled overview with cropped OSM roads and source-locked building clearance','labels':False,'compiler':run([*compiler,'--version']).splitlines()[0],'libraries':run([pkg,'--modversion','libpng','freetype2']),'flags':flags}
    fingerprint=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()
    if OUTPUT.is_file() and STAMP.is_file():
        old=json.loads(STAMP.read_text())
        if old.get('fingerprint')==fingerprint and old.get('png_sha256')==sha(OUTPUT) and old.get('reference_pixel_match') and all((OUTPUT.parent/name).is_file() and sha(OUTPUT.parent/name)==old.get(key) for name,key in [('json2png2','executable_sha256'),('AssembledMap.json','assembled_json_sha256'),('buildings-enriched.json','corrected_buildings_sha256'),('CanterburyMapBackground.png','background_sha256')]):return {'cached':True,'png_sha256':sha(OUTPUT),'renderer':'tools/json2png2.c with upstream json2png.c core','reference_pixel_match':True}
    if check:raise ValueError('json2png basemap is missing/stale; run python3 tools/build_basemap.py --write')
    OUTPUT.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=OUTPUT.parent,prefix='render-') as folder:
        work=Path(folder);exe=work/'json2png2';png=work/'CanterburyMap.png';render_input=work/'assembled.json';render_input.write_text(json.dumps(model(),separators=(',',':'))+'\n')
        enriched=json.loads((ROOT/'canterbury/simulation-map/derived/buildings-enriched.json').read_text());enriched['objects']=corrected_objects(enriched['objects']);enriched['placement_basis']='Source-locked schematic street-clearance corrections; historical positions unverified'
        building_input=work/'buildings-enriched.json';building_input.write_text(json.dumps(enriched,separators=(',',':'))+'\n')
        subprocess.run([*compiler,'-std=c11','-O2','tools/json2png2.c',*flags,'-lm','-o',str(exe)],cwd=ROOT,check=True)
        subprocess.run([str(exe),str(render_input),str(png)],cwd=ROOT,check=True)
        background_input=work/'background.json';background_input.write_text(json.dumps(model(False),separators=(',',':'))+'\n')
        background=work/'CanterburyMapBackground.png'
        subprocess.run([str(exe),str(background_input),str(background)],cwd=ROOT,check=True)
        for p,expected in identity['sources'].items():
            if sha(ROOT/p)!=expected:raise ValueError('Renderer input changed while rendering: '+p)
        if not reference_matches(png):raise ValueError('json2png2 pixels differ from the independently corrected assembled reference')
        background.replace(OUTPUT.parent/'CanterburyMapBackground.png')
        exe.replace(OUTPUT.parent/'json2png2');render_input.replace(OUTPUT.parent/'AssembledMap.json');png.replace(OUTPUT);building_input.replace(OUTPUT.parent/'buildings-enriched.json')
    STAMP.write_text(json.dumps({'fingerprint':fingerprint,'png_sha256':sha(OUTPUT),'inputs':identity,'reference_pixel_match':True,'executable_sha256':sha(OUTPUT.parent/'json2png2'),'assembled_json_sha256':sha(OUTPUT.parent/'AssembledMap.json'),'background_sha256':sha(OUTPUT.parent/'CanterburyMapBackground.png'),'corrected_buildings_sha256':sha(OUTPUT.parent/'buildings-enriched.json')},indent=2)+'\n')
    return {'cached':False,'png_sha256':sha(OUTPUT),'renderer':'tools/json2png2.c with upstream json2png.c core','reference_pixel_match':True}
def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--write',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
    try:print(json.dumps(build(a.check),sort_keys=True))
    except (ValueError,OSError,subprocess.CalledProcessError) as e:p.exit(2,'Basemap error: '+str(e)+'\n')
if __name__=='__main__':main()
