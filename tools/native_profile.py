"""Shared, read-only native SDK/profile admission for all build entry points."""
import json,os,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PROFILE=['-DCANTERBURY_SIM=1','-DBRONZE_SIM=1','-DHIDE_THUNDER=1','-DCOMMAND_LINE_EXPLICIT']
REQUIRED={'apesdk':['universe/universe.h','universe/sim.c','bronze/actors/bronze_actors.c','bronze/work/bronze_work.c','bronze/hybrid/bronze_hybrid.c','bronze/tasks/bronze_tasks.c'],'canterbury':['newcanterbury/canterbury.json','simulation-map/religious-sites-1940.json']}
def preflight():
    for module,paths in REQUIRED.items():
        for name in paths:
            if not (ROOT/module/name).is_file():raise ValueError(f'Missing {module} submodule input: {module}/{name}. Run git submodule update --init --recursive.')
    manifest=json.loads((ROOT/'content/source-manifest.json').read_text())
    for module in manifest['submodules']:
        p=subprocess.run(['git','-C',str(ROOT/module['path']),'rev-parse','HEAD'],text=True,capture_output=True)
        if p.returncode or p.stdout.strip()!=module['revision']:raise ValueError('Submodule revision differs from audited source: '+module['path'])
    from audit_sources import inventory
    if inventory()!=manifest:raise ValueError('Source inventory differs; review SDK working-tree changes before building')
    flags=shlex.split(os.environ.get('CPPFLAGS',''))
    # Any attempt to contradict the build-wide city profile is rejected before
    # compiling, including -U and the split -D / -U spellings.
    compact=[];i=0
    while i<len(flags):
        token=flags[i]
        if token in ('-D','-U') and i+1<len(flags):token+=flags[i+1];i+=1
        compact.append(token);i+=1
    for flag in compact:
        for name in ('CANTERBURY_SIM','BRONZE_SIM'):
            if flag.startswith('-U'+name) or (flag.startswith('-D'+name) and flag not in ('-D'+name,'-D'+name+'=1')):raise ValueError('Inconsistent profile: '+name+' must be 1 in every translation unit')
    return flags

