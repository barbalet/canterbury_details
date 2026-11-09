"""Build the normally launched native presentation measurement application."""
import hashlib,json,plistlib,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def build():
 inputs={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'tests/presentation.m',ROOT/'ui/CityMap.m',ROOT/'ui/CityMap.h',ROOT/'ui/CitySession.h',*sorted((ROOT/'src').glob('*.h')),ROOT/'build/map/CanterburyMapBackground.png',ROOT/'build/map/buildings-enriched.json']}
 source_id=hashlib.sha256(json.dumps(inputs,sort_keys=True).encode()).hexdigest()
 app=ROOT/'build/PresentationBenchmark.app';contents=app/'Contents';(contents/'MacOS').mkdir(parents=True,exist_ok=True);(contents/'Resources').mkdir(exist_ok=True)
 subprocess.run(['cc','-std=c11','-O2','-fobjc-arc','-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-Isrc','-Iui','tests/presentation.m','ui/CityMap.m','-framework','Cocoa','-o',str(contents/'MacOS/PresentationBenchmark')],cwd=ROOT,check=True)
 (contents/'Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable':'PresentationBenchmark','CFBundleIdentifier':'org.canterbury.presentation-benchmark','CFBundleName':'Canterbury presentation benchmark','CFBundlePackageType':'APPL','NSHighResolutionCapable':True,'LSMinimumSystemVersion':'14.0','NSPrincipalClass':'NSApplication','CdBenchmarkSourceSHA':source_id}));shutil.copy2(ROOT/'build/map/CanterburyMapBackground.png',contents/'Resources/CanterburyMapBackground.png');shutil.copy2(ROOT/'build/map/buildings-enriched.json',contents/'Resources/buildings-enriched.json')
 (ROOT/'build/presentation-build.json').write_text(json.dumps({'source_sha256':source_id,'inputs':inputs},indent=2)+'\n')
 print(app)
if __name__=='__main__':build()
