#!/usr/bin/env python3
"""Run actual-map household entry and staffed district acceptance."""
import hashlib
import json
from pathlib import Path
import subprocess

from compile_content import ROOT, atomic_write, canonical
from qualify_canterbury_access import PACK, REPORT


def main():
    qualification = json.loads(REPORT.read_text())
    fingerprint = hashlib.sha256(PACK.read_bytes()).hexdigest()
    if fingerprint != qualification['pack_sha256']:
        raise ValueError('Access trial differs from its qualification report')
    command = json.loads((ROOT / 'build/cli-build.json').read_text())['inputs']['command']
    command = ['tests/access_trial.c' if part == 'src/main.c' else part for part in command]
    binary = ROOT / 'build/access-trial-test'
    subprocess.run(command + ['-UNDEBUG', '-o', str(binary)], cwd=ROOT, check=True)
    evidence = {'pack_sha256': fingerprint}
    for name, flags in [('population', ['--population']), ('district', [])]:
        output = subprocess.check_output([binary, *flags], cwd=ROOT, text=True)
        evidence[name] = json.loads(output.splitlines()[-1])
        if not evidence[name]['ok']:
            raise ValueError(f'Failed access trial: {name}')
        print(output.strip(), flush=True)
    atomic_write(ROOT / 'build/access-trial-validation.json', canonical(evidence))


if __name__ == '__main__':
    main()
