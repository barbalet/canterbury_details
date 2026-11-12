#!/usr/bin/env python3
"""Run actual-map household, workplace, and school access checks."""
from collections import Counter, defaultdict, deque
import hashlib
import json
import math
from pathlib import Path
import subprocess

from compile_content import ROOT, atomic_write, canonical
from trace_canterbury_roles import FUNDED_SERVICE_TESTS
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


def allocate_households(binary, premises, sessions, sectors, shared):
    """Place intact startup households against local school and employment capacity."""
    homes, school = defaultdict(deque), defaultdict(Counter)
    employment = Counter()
    jobs_at = defaultdict(Counter)
    for site in sorted(premises, key=lambda site: site['building']):
        group = site['component']
        if site['use'] == 'residential':
            homes[group].extend([site['building']] * (site['capacity'] // 4))
        for session in range(sessions):
            if site['school_capacity']:
                school[group][site['building'], session] = site['school_capacity']
        employment[group] += site['work_capacity']
        if site['work_capacity']:
            jobs_at[group, site['use']][site['building']] = site['work_capacity']

    adults, placements, enrollment = defaultdict(deque), [], []
    households, points = [], {}
    output = subprocess.check_output([binary, '--households'], cwd=ROOT, text=True)
    for line in output.splitlines():
        fields = line.split()
        if fields[0] == 'door':
            points[int(fields[1])] = tuple(map(float, fields[2:]))
            continue
        household, *members = map(int, fields)
        workers = [person for person, age in zip(members[::2], members[1::2]) if 18 <= age < 65]
        pupils = [person for person, age in zip(members[::2], members[1::2]) if 5 <= age < 15]
        households.append((household, workers, pupils))

    def nearest(remaining, dwelling, building_id):
        site = min(remaining, key=lambda key: (math.dist(points[dwelling], points[building_id(key)]), key))
        remaining[site] -= 1
        if not remaining[site]:
            del remaining[site]
        return site

    seats = Counter({group: sum(counts.values()) for group, counts in school.items()})
    for household, workers, pupils in sorted(households,
                                             key=lambda h: (-len(h[2]), -len(h[1]), h[0])):
        choices = [group for group, slots in homes.items()
                   if slots and seats[group] >= len(pupils)]
        if not choices:
            raise ValueError(f'No local home and school capacity for household {household}')
        if pupils:
            group = max(choices, key=lambda g: (seats[g], employment[g] - len(adults[g]), -g))
        elif workers:
            group = max(choices, key=lambda g: (min(len(workers), employment[g] - len(adults[g])),
                                                employment[g] - len(adults[g]), len(homes[g]), -g))
        else:
            group = max(choices, key=lambda g: (len(homes[g]), -g))
        dwelling = homes[group].popleft()
        placements.append({'household': household, 'home': dwelling})
        adults[group].extend((person, dwelling) for person in workers)
        for pupil in pupils:
            building, session = nearest(school[group], dwelling, lambda site: site[0])
            seats[group] -= 1
            enrollment.append({'person': pupil, 'home': dwelling,
                               'school': building, 'session': session})

    allocations = []
    jobs = workforce_bound(premises, sectors, shared,
                           {group: len(people) for group, people in adults.items()}, allocations)
    workers = []
    for group, use, sector, count in allocations:
        for _ in range(count):
            person, dwelling = adults[group].popleft()
            workers.append({'person': person, 'home': dwelling,
                            'workplace': nearest(jobs_at[group, use], dwelling, int), 'sector': sector})
    return {'homes': placements, 'school_enrollment': enrollment,
            'workers': workers, 'jobs_by_sector': jobs}



def measure_routes(binary, journeys, walk_units):
    pairs = sorted(set(journeys) | {(end, start) for start, end in journeys})
    routes = subprocess.run(
        [binary, '--routes'], cwd=ROOT, text=True, capture_output=True, check=True,
        input=''.join(f'{start} {end}\n' for start, end in pairs),
    ).stdout.splitlines()
    if len(routes) != len(pairs):
        raise ValueError('Native router did not return every requested route')
    travel = {}
    for pair, route in zip(pairs, routes):
        found, _, distance = route.split()
        if int(found):
            travel[pair] = math.ceil(float(distance) / walk_units)

    return travel, len(pairs)


def check_routes(binary, placement, walk_units, premises):
    workers, pupils = placement['workers'], placement['school_enrollment']
    component = {site['building']: site['component'] for site in premises}
    journeys = [(row['home'], row['workplace']) for row in workers]
    journeys.extend((row['home'], row['school']) for row in pupils)
    travel, checked = measure_routes(binary, journeys, walk_units)

    def round_trip(start, end):
        return travel.get((start, end), math.inf) + travel.get((end, start), math.inf)

    groups = defaultdict(list)
    for worker in workers:
        groups[component[worker['home']], worker['sector']].append(worker)
    missing = [w for w in workers if not math.isfinite(round_trip(w['home'], w['workplace']))]
    candidates = set()
    for worker in missing:
        for other in groups[component[worker['home']], worker['sector']]:
            candidates.add((worker['home'], other['workplace']))
            candidates.add((other['home'], worker['workplace']))
    candidates = {pair for pair in candidates if pair not in travel or pair[::-1] not in travel}
    if candidates:
        extra, count = measure_routes(binary, candidates, walk_units)
        travel.update(extra)
        checked += count
    repaired = 0
    for worker in missing:
        start, end = worker['home'], worker['workplace']
        if math.isfinite(round_trip(start, end)):
            continue
        choices = []
        for other in groups[component[start], worker['sector']]:
            if other is worker:
                continue
            outward = round_trip(start, other['workplace'])
            inward = round_trip(other['home'], end)
            if max(outward, inward) <= 420:
                choices.append((outward + inward, other['person'], other))
        if choices:
            _, _, other = min(choices)
            worker['workplace'], other['workplace'] = other['workplace'], end
            repaired += 1

    def assess(rows, destination, duty_minutes):
        complete = []
        scheduled = 0
        for row in rows:
            pair = row['home'], row[destination]
            if pair not in travel or pair[::-1] not in travel:
                continue
            outbound, inbound = travel[pair], travel[pair[::-1]]
            complete.append(outbound + inbound)
            # Split worker shifts into four start waves. Pupils keep their
            # building's two non-overlapping sessions.
            start = (570 + 240 * row['session'] if 'session' in row
                     else 540 + 20 * (row['person'] % 4))
            end = start + duty_minutes
            leave = max(360, start - outbound - 105 - row['person'] % 15)
            expected_home = end + inbound + 60
            if leave >= 360 and expected_home <= 1320:
                row['draft_schedule'] = {
                    'leave_home': leave, 'attendance_start': start,
                    'attendance_end': end, 'expected_home': expected_home,
                }
                scheduled += 1
        return {
            'assigned': len(rows),
            'with_draft_schedule': scheduled,
            'with_routes_both_ways': len(complete),
            'longest_dry_street_round_trip_minutes': max(complete, default=0),
            'duty_minutes': duty_minutes,
            'over_day_with_duty_and_8_hours_rest': sum(
                duration + duty_minutes + 480 > 1440 for duration in complete),
        }

    return {
        'route_queries': checked,
        'worker_route_swaps': repaired,
        'workers': assess(placement['workers'], 'workplace', 480),
        'pupils': assess(placement['school_enrollment'], 'school', 180),
    }


def startup_rows(placement, groups):
    homes = sorted(placement['homes'], key=lambda row: row['household'])
    lines = [str(row['home']) for row in homes]
    for group, destination in groups:
        for row in placement[group]:
            schedule = row['draft_schedule']
            lines.append(f"{row['person']} {row[destination]} {schedule['leave_home']} "
                         f"{schedule['attendance_start']} {schedule['attendance_end']}")
    return lines


def run_day(binary):
    output = subprocess.check_output([binary, '--day'], cwd=ROOT, text=True)
    return json.loads(output.splitlines()[-1])


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
    placement = allocate_households(binary, premises, sessions, sectors, shared)
    placement['pack_sha256'] = fingerprint
    evidence['joint_household_plan'] = {
        'households': len(placement['homes']),
        'pupils': len(placement['school_enrollment']),
        'jobs_capacity_matched': len(placement['workers']),
    }
    print(json.dumps(evidence['joint_household_plan'], sort_keys=True), flush=True)
    evidence['joint_household_routes'] = check_routes(binary, placement, qualification['dry_native_walk_units'], premises)
    school_rows = startup_rows(placement, [('school_enrollment', 'school')])
    atomic_write(ROOT / 'build/content/access-school-startup.txt',
                 ('\n'.join(['CDSCHOOL1 ' + qualification['generation'], *school_rows]) + '\n').encode())
    worker_rows = startup_rows(placement, [('workers', 'workplace')])[len(placement['homes']):]
    duty_rows = [f"{line} {int(row['sector'].split('_')[-1])}"
                 for line, row in zip(worker_rows, placement['workers'], strict=True)]
    atomic_write(ROOT / 'build/content/access-duty-startup.txt',
                 ('\n'.join(['CDDUTIES1 ' + qualification['generation'], *school_rows, *duty_rows])
                  + '\n').encode())
    subprocess.run(['python3', 'tools/compile_canterbury_dsl.py',
                    'content/scenarios/population-27000.canterbury',
                    '--city', str(PACK), '--startup', 'build/content/access-27000.cdstart'],
                   cwd=ROOT, check=True)
    atomic_write(ROOT / 'build/access-trial-placement.json', canonical(placement))
    print(json.dumps(evidence['joint_household_routes'], sort_keys=True), flush=True)
    subprocess.run(['python3', 'tools/city_fixtures.py'], cwd=ROOT, check=True)
    checks = {
        'transitions': ['--transitions'],
        'recipes': ['--recipes'],
        'retail': ['--retail'],
        'transport_hospitality': ['--transport'],
        'transport_stages': ['--transport-stages'],
        'public_administration': ['--public-administration'],
        'police_response': ['--police-response'],
        'library_access': ['--library-access'],
        'water_supply': ['--water-supply'],
        'clinical_care': ['--clinical-care'],
        'community_services': ['--community-services'],
        'maternal_and_domestic': ['--maternal-and-domestic'],
        'repair_and_worship': ['--repair-and-worship'],
        'senior_public_roles': ['--senior-public-roles'],
        'institutional_support_roles': ['--institutional-support-roles'],
        'population': ['--population'],
        'district': [],
        'school': ['--school'],
    }
    for name, flags in checks.items():
        output = subprocess.check_output([binary, *flags], cwd=ROOT, text=True)
        lines = output.splitlines()
        evidence[name] = json.loads(lines[-1])
        if name == 'senior_public_roles':
            evidence[name]['roles'] = [
                role for line in lines[:-1] for role, passed in json.loads(line).items()
                if role != 'ok' and passed == 1
            ]
            if len(set(evidence[name]['roles'])) != 38:
                raise ValueError('Expected evidence for 38 distinct public roles')
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
    print('Running 1,440 native minutes with all scheduled residents', flush=True)
    evidence['scheduled_native_day'] = run_day(binary)
    print(json.dumps(evidence['scheduled_native_day'], sort_keys=True), flush=True)
    evidence['dsl_service_coverage'] = {
        service: {'check': check, 'passed': evidence[check]['ok']}
        for service, check in FUNDED_SERVICE_TESTS.items()
    }
    day = evidence['scheduled_native_day']
    evidence['cycle_16'] = {
        'scheduled_services': day['scheduled_services'],
        'ledgers_balanced': day['ledgers_balanced'],
        'inspection_pure': day['inspection_pure'],
        'work_training_contention': evidence['district']['work_training_contention'],
    }
    if day['scheduled_services'] != 3 or not all(evidence['cycle_16'].values()):
        raise ValueError('Cycle 16 native integration did not meet the funded-day requirements')
    atomic_write(ROOT / 'build/access-trial-validation.json', canonical(evidence))
    if not evidence['scheduled_native_day']['ok']:
        raise SystemExit('Cycle 8 native day did not meet every attendance and return requirement')


if __name__ == '__main__':
    main()
