#!/usr/bin/env python3
"""Run actual-map household, workplace, and school access checks."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess

from compile_content import ROOT, atomic_write, canonical
from qualify_canterbury_access import (
    PACK, REPORT, WORKFORCE, workforce_bound, workforce_records,
)


def joint_placement_bound(premises, sectors, shared_uses, sessions):
    """Relax household size, age mix, routes, and schedules to test feasibility."""
    housing = Counter()
    school = Counter()
    for site in premises:
        component = site['component']
        if site['use'] == 'residential':
            housing[component] += site['capacity']
        school[component] += site['school_capacity']
    jobs = workforce_bound(premises, sectors, shared_uses, housing)
    return {
        'jobs_by_sector': jobs,
        'jobs': sum(row['capacity_matched'] for row in jobs.values()),
        'pupils': sum(min(places, sessions * school[group])
                      for group, places in housing.items()),
        'scope': 'Independent upper bounds before household grouping, route length or schedules',
    }


def main():
    qualification = json.loads(REPORT.read_text())
    fingerprint = hashlib.sha256(PACK.read_bytes()).hexdigest()
    if fingerprint != qualification['pack_sha256']:
        raise ValueError('Access trial differs from its qualification report')
    command = json.loads((ROOT / 'build/cli-build.json').read_text())['inputs']['command']
    command = ['tests/access_trial.c' if part == 'src/main.c' else part for part in command]
    binary = ROOT / 'build/access-trial-test'
    subprocess.run(command + ['-UNDEBUG', '-o', str(binary)], cwd=ROOT, check=True)
    capacity = qualification['workforce_trial_capacity_bound']
    if sum(row['target'] for row in capacity.values()) != 12000 or any(
            row['capacity_matched'] != row['target'] for row in capacity.values()):
        raise ValueError('Trial model cannot accommodate the draft job counts by premises use')
    draft = WORKFORCE.read_bytes()
    if hashlib.sha256(draft).hexdigest() != qualification['workforce_draft_sha256']:
        raise ValueError('Access trial uses a different workforce draft')
    sectors = workforce_records(draft)
    premises = qualification['premises']
    shared = qualification['workforce_trial_shared_uses']
    sessions = qualification['school_sessions']
    bound = joint_placement_bound(premises, sectors, shared, sessions)
    evidence = {'pack_sha256': fingerprint, 'workforce_capacity_bound': capacity,
                'joint_placement_upper_bounds': bound}
    for name, flags in [('population', ['--population']), ('district', []), ('school', ['--school'])]:
        output = subprocess.check_output([binary, *flags], cwd=ROOT, text=True)
        evidence[name] = json.loads(output.splitlines()[-1])
        if not evidence[name]['ok']:
            raise ValueError(f'Failed access trial: {name}')
        demands = evidence[name].pop('demands', None)
        if demands is not None:
            component = {str(site['building']): site['component'] for site in premises}
            adults, pupils, seats = Counter(), Counter(), Counter()
            for building, workers, children in demands:
                adults[component[building]] += workers
                pupils[component[building]] += children
            if (sum(adults.values()), sum(pupils.values())) != (16470, 4050):
                raise ValueError('Placed household cohort counts differ from the scenario')
            for site in premises:
                seats[site['component']] += site['school_capacity']
            jobs = workforce_bound(premises, sectors, shared, adults)
            evidence['placed_household_bounds'] = {
                'jobs_by_sector': jobs,
                'jobs': sum(row['capacity_matched'] for row in jobs.values()),
                'pupils': sum(min(count, sessions * seats[group])
                              for group, count in pupils.items()),
                'pupil_shortages_by_component': [
                    {'component': group, 'pupils': count, 'session_places': sessions * seats[group]}
                    for group, count in sorted(pupils.items()) if count > sessions * seats[group]
                ],
            }
        print(json.dumps(evidence[name], sort_keys=True), flush=True)
    atomic_write(ROOT / 'build/access-trial-validation.json', canonical(evidence))
    print(f"Any household placement can cover at most {bound['jobs']} jobs "
          f"and {bound['pupils']} pupil places under independent relaxed bounds.", flush=True)


if __name__ == '__main__':
    main()
