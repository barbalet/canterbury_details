#!/usr/bin/env python3
"""Derive a labelled pedestrian trial from the audited Canterbury geometry."""
import argparse
from collections import defaultdict, deque
import hashlib
import itertools
import json
import math

from canterbury_dsl_parser import parse
from compile_content import ROOT, atomic_write, canonical
from compile_runtime import build as check_runtime, compile_model, encode, uid

BUNDLE = ROOT / 'build/content/canterbury.json'
ESTIMATES = ROOT / 'content/building-use-estimates.json'
PACK = ROOT / 'build/content/CanterburyAccessTrial.bin'
REPORT = ROOT / 'build/content/CanterburyAccessTrial.json'
CELL = 16
EPS = 1e-8
MARGIN = 0.04
WALKABLE = {
    'residential', 'living_street', 'unclassified', 'tertiary', 'secondary',
    'primary', 'pedestrian', 'footway', 'path', 'steps', 'service', 'track',
}
WORKFORCE = ROOT / 'content/scenarios/canterbury1940-draft.canterbury'
RESIDENTIAL_FLOORS = 2
WORK_FLOORS = 2
# Trial upper-floor offices and hospitality space in these mapped premises.
EXTRA_WORK_FLOOR_USES = {'commercial', 'civic', 'leisure'}
# Technical capacity per floor, in map area units per person.
WORK_AREA_PER_PERSON = 1
SCHOOL_USES = {'education', 'education_support'}
SCHOOL_SESSIONS = 2
# Concurrent walkers on a qualified link in the citywide trial.
WALK_LINK_CAPACITY = 128
# Provisional premises sharing for light bench work and building trades offices.
# These are activity permissions, with no change to the mapped building use.
TRIAL_SHARED_USES = {'sector_2': {'commercial'}, 'sector_3': {'commercial'}}


def workforce_records(raw):
    records = parse(raw.decode(), str(WORKFORCE))
    return {name: record for (kind, name), record in records.items()
            if kind == 'workforce'}


def workforce_bound(premises, sectors, shared_uses=None, adults=None, allocations=None):
    """Maximum jobs under use constraints, optionally bounded by local adults."""
    by_use = defaultdict(int)
    by_component_use = defaultdict(int)
    for site in premises:
        if site['work_capacity']:
            by_use[site['use']] += site['work_capacity']
            by_component_use[site['component'], site['use']] += site['work_capacity']
    residual = defaultdict(dict)

    def connect(a, b, capacity):
        residual[a][b] = capacity
        residual[b][a] = 0

    source, sink = ('source', ''), ('sink', '')
    if adults is None:
        for use, capacity in sorted(by_use.items()):
            connect(source, ('use', use), capacity)
    else:
        for component, count in sorted(adults.items()):
            connect(source, ('component', component), count)
        for (component, use), capacity in sorted(by_component_use.items()):
            connect(('component', component), ('component_use', component, use), capacity)
    for name, record in sectors.items():
        target = record['target_people']
        connect(('sector', name), sink, target)
        uses = set(record['premises_types'])
        uses.update((shared_uses or {}).get(name, ()))
        for use in sorted(uses):
            if adults is None and by_use[use]:
                connect(('use', use), ('sector', name), target)
            elif adults is not None:
                for component, candidate in sorted(by_component_use):
                    if candidate == use and adults.get(component, 0):
                        connect(('component_use', component, use), ('sector', name), target)
    while True:
        parents = {source: None}
        queue = deque([source])
        while queue and sink not in parents:
            node = queue.popleft()
            for neighbor, capacity in residual[node].items():
                if capacity and neighbor not in parents:
                    parents[neighbor] = node
                    queue.append(neighbor)
        if sink not in parents:
            break
        path = []
        node = sink
        while parents[node] is not None:
            parent = parents[node]
            path.append((parent, node))
            node = parent
        amount = min(residual[a][b] for a, b in path)
        for a, b in path:
            residual[a][b] -= amount
            residual[b][a] += amount
    if allocations is not None:
        for name in sectors:
            for node, count in residual['sector', name].items():
                if node[0] == 'component_use' and count:
                    _, component, use = node
                    allocations.append((component, use, name, count))
    return {
        name: {
            'sector': record['sector'],
            'target': record['target_people'],
            'capacity_matched': residual[sink][('sector', name)],
        }
        for name, record in sectors.items()
    }



def segments(points):
    return zip(points, points[1:])


def cross(a, b, p):
    return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])


def point_segment(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = dx * dx + dy * dy
    fraction = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / length)) if length else 0
    return math.hypot(p[0] - a[0] - fraction * dx, p[1] - a[1] - fraction * dy)


def ring_contains(ring, p, boundary=True):
    inside = False
    x, y = p
    for a, b in segments(ring):
        if (abs(cross(a, b, p)) <= EPS and
                min(a[0], b[0]) - EPS <= x <= max(a[0], b[0]) + EPS and
                min(a[1], b[1]) - EPS <= y <= max(a[1], b[1]) + EPS):
            return boundary
        if ((a[1] > y) != (b[1] > y)) and x < a[0] + (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]):
            inside = not inside
    return inside


def inside(poly, point):
    return sum(ring_contains(ring, point) for ring in poly['rings']) % 2 == 1


def cells(bounds):
    x1, y1, x2, y2 = bounds
    return itertools.product(range(math.floor(x1 / CELL), math.floor(x2 / CELL) + 1),
                             range(math.floor(y1 / CELL), math.floor(y2 / CELL) + 1))


class Geometry:
    def __init__(self, polygons):
        self.polygons = polygons
        self.index = defaultdict(list)
        for i, polygon in enumerate(polygons):
            points = list(itertools.chain.from_iterable(polygon['rings']))
            xs, ys = zip(*points)
            bounds = min(xs), min(ys), max(xs), max(ys)
            for cell in cells(bounds):
                self.index[cell].append(i)

    def clear(self, a, b, ignore=None):
        bounds = (min(a[0], b[0]) - MARGIN, min(a[1], b[1]) - MARGIN,
                  max(a[0], b[0]) + MARGIN, max(a[1], b[1]) + MARGIN)
        candidates = set(itertools.chain.from_iterable(self.index.get(c, ()) for c in cells(bounds)))
        for i in candidates:
            if i == ignore:
                continue
            polygon = self.polygons[i]
            if inside(polygon, a) or inside(polygon, b):
                return False
            for ring in polygon['rings']:
                for c, d in segments(ring):
                    if (max(c[0], d[0]) < bounds[0] or min(c[0], d[0]) > bounds[2] or
                            max(c[1], d[1]) < bounds[1] or min(c[1], d[1]) > bounds[3]):
                        continue
                    if (cross(a, b, c) * cross(a, b, d) < 0 and
                            cross(c, d, a) * cross(c, d, b) < 0):
                        return False
                    if min(point_segment(a, c, d), point_segment(b, c, d),
                           point_segment(c, a, b), point_segment(d, a, b)) <= MARGIN:
                        return False
        return True


def road_allowed(source):
    tags = source.get('tags', {})
    restricted = {'no', 'private', 'customers', 'permit', 'delivery'}
    return (source.get('highway') in WALKABLE and source.get('layer', '0') == '0' and
            source.get('bridge', 'no') == source.get('tunnel', 'no') == 'no' and
            tags.get('foot') not in restricted and tags.get('access') not in restricted and
            not tags.get('foot:conditional') and not tags.get('access:conditional'))


def motor_road(source):
    tags = source.get('tags', {})
    restricted = {'no', 'private', 'customers', 'permit', 'delivery', 'destination'}
    return (source['highway'] in {'residential', 'living_street', 'unclassified',
                                  'tertiary', 'secondary', 'primary', 'service'}
            and source.get('vehicle_direction') == 'both'
            and not any(tags.get(key) in restricted or tags.get(key + ':conditional')
                        for key in ('access', 'vehicle', 'motor_vehicle', 'motorcar',
                                    'motor_vehicle:forward', 'motor_vehicle:backward', 'bus', 'hgv'))
            and tags.get('junction') not in {'roundabout', 'circular'}
            and not tags.get('restriction'))


def crossings(poly, a, b):
    hits = 0
    bounds = min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])
    for ring in poly['rings']:
        for c, d in segments(ring):
            if (max(c[0], d[0]) < bounds[0] or min(c[0], d[0]) > bounds[2] or
                    max(c[1], d[1]) < bounds[1] or min(c[1], d[1]) > bounds[3]):
                continue
            ab_c, ab_d = cross(a, b, c), cross(a, b, d)
            cd_a, cd_b = cross(c, d, a), cross(c, d, b)
            if ab_c * ab_d < 0 and cd_a * cd_b < 0:
                hits += 1
            elif min(point_segment(c, a, b), point_segment(d, a, b),
                     point_segment(a, c, d), point_segment(b, c, d)) <= EPS:
                return -1
    return hits


def one_crossing(poly, a, b):
    return (not any(ring_contains(ring, a) for ring in poly['rings']) and
            inside(poly, b) and crossings(poly, a, b) == 1)


def native_connector(geometry, index, start, end, scale, walk_units):
    """Screen the rounded dry entry and return path before admitting a door."""
    polygon = geometry.polygons[index]

    def quantize(point):
        return tuple(math.floor(value * scale + .5) for value in point)

    def point(units):
        return tuple(value / scale for value in units)

    def walk(current, destination):
        target = quantize(destination)
        for _ in range(160):
            delta = tuple(target[i] - current[i] for i in range(2))
            length = math.hypot(*delta)
            if length < .5:
                return current
            a = point(current)
            candidate = None
            for budget in range(walk_units, 0, -1):
                factor = min(1, budget / length)
                candidate = tuple(current[i] + int(math.copysign(
                    math.floor(abs(delta[i]) * factor), delta[i])) for i in range(2))
                b = point(candidate)
                boundary = any(ring_contains(ring, b) != ring_contains(ring, b, False)
                               for ring in polygon['rings'])
                if not boundary or budget == 1:
                    break
            if candidate == current or boundary:
                return None
            if crossings(polygon, a, b) != int(inside(polygon, a) != inside(polygon, b)):
                return None
            # The ideal connector already has MARGIN clearance. Check the
            # actual segment again whenever rounding departs from that margin.
            if (max(point_segment(a, start, end), point_segment(b, start, end)) > MARGIN / 2
                    and not geometry.clear(a, b, index)):
                return None
            current = candidate
            if math.dist(b, destination) <= 1 / scale:
                return current
        return None

    entered = walk(quantize(start), end)
    return (entered is not None and inside(polygon, point(entered)) and
            walk(entered, start) is not None)


def entrances(poly):
    ring = poly['rings'][0]
    for a, b in sorted(segments(ring), key=lambda pair: -math.dist(*pair))[:24]:
        length = math.dist(a, b)
        if length < 0.2:
            continue
        center = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        normal = ((b[1] - a[1]) / length, (a[0] - b[0]) / length)
        for direction in (1, -1):
            point = tuple(center[k] + direction * normal[k] * 0.28 for k in range(2))
            if inside(poly, point):
                yield point


def components(adjacency):
    seen = set()
    groups = []
    for start in sorted(adjacency):
        if start in seen:
            continue
        group = {start}
        seen.add(start)
        stack = [start]
        while stack:
            for node in adjacency[stack.pop()]:
                if node not in seen:
                    seen.add(node)
                    group.add(node)
                    stack.append(node)
        groups.append(group)
    return sorted(groups, key=lambda group: (-len(group), min(group)))


def apply_rail_footprint_review(model, bundle):
    review_path = ROOT / 'content/canterbury-rail-footprint-review.json'
    if not review_path.exists():
        return None
    raw = review_path.read_bytes()
    review = json.loads(raw)
    rail_source = ROOT / 'content/canterbury-west-sturry-rail-candidates.json'
    if (review['map_generation'] != bundle['generation_id'] or
            review['rail_source_sha256'] != hashlib.sha256(rail_source.read_bytes()).hexdigest()):
        raise ValueError('Rail footprint review belongs to different map or track inputs')
    buildings = {p['source']: p for p in model['polygons'] if p['kind'] == 1}
    seen = set()
    for move in review['moves']:
        name = move['source']
        dx, dy = move['offset_pixels']
        if name in seen or name not in buildings or not 0 < math.hypot(dx, dy) <= 12:
            raise ValueError('Invalid rail footprint translation')
        seen.add(name)
        polygon = buildings[name]
        rings = [[[x + dx, y + dy] for x, y in ring] for ring in polygon['rings']]
        if not all(0 <= x <= model['width'] and 0 <= y <= model['height']
                   for ring in rings for x, y in ring):
            raise ValueError('Rail footprint translation leaves the map')
        polygon['rings'] = rings
    for trim in review.get('trims', []):
        name, rings = trim['source'], trim['rings']
        if (name in seen or name not in buildings or not rings or
                not all(len(ring) >= 4 and ring[0] == ring[-1] and
                        all(0 <= x <= model['width'] and 0 <= y <= model['height']
                            for x, y in ring) for ring in rings)):
            raise ValueError('Invalid rail footprint trim')
        seen.add(name)
        buildings[name]['rings'] = rings
    return hashlib.sha256(raw).hexdigest()


def admit_rail(model, bundle, footprint_hash, scale, walk_units):
    raw = (ROOT / 'content/canterbury-rail-operation-review.json').read_bytes()
    review = json.loads(raw)
    source = ROOT / 'content/canterbury-west-sturry-rail-candidates.json'
    if (review['map_generation'] != bundle['generation_id'] or
            review['footprint_review_sha256'] != footprint_hash or
            review['source_subset_sha256'] != hashlib.sha256(source.read_bytes()).hexdigest()):
        raise ValueError('Stale operating rail review')
    polygons = model['polygons']
    west = next(p for p in polygons if p['source'] == 'reviewed_rail:osm_way_151293646')
    dry = Geometry([p for p in polygons if p['kind'] != 2])
    water = Geometry([p for p in polygons if p['kind'] == 2])
    nodes = {}
    bridges = 0
    last = None
    for segment in review['segments']:
        a, b = segment['nodes']
        start, end = segment['points']
        if last is not None and a != last:
            raise ValueError('Disconnected operating rail review')
        last = b
        bridge = segment['conflicts'] == {'water': ['waterways-86030fec4f522c40687b']}
        if (not dry.clear(start, end) or water.clear(start, end) == bridge or
                (bridge and (segment['way'], a, b) != (196268643, 26985469, 26985472))):
            raise ValueError(f'Operating rail geometry conflict at {a}–{b}')
        bridges += bridge
        for node, point in ((a, start), (b, end)):
            if node not in nodes:
                nodes[node] = dict(id=uid('reviewed-rail-node', str(node)),
                                   source=f'reviewed_rail:{node}', point=point)
        model['edges'].append(dict(id=uid('reviewed-rail-edge', f'{a}:{b}'),
                                   source=f'reviewed_rail:{segment["way"]}:{a}:{b}',
                                   basis='technical trial: explicitly reviewed operating rail',
                                   qualified=True, bridge=bridge, capacity=1,
                                   points=[start, end], **{'from': nodes[a]['id'], 'to': nodes[b]['id']}))
    if bridges != 1:
        raise ValueError('Expected one reviewed Sturry water crossing')
    model['nodes'].extend(nodes.values())
    geometry = Geometry(polygons)
    for source_id, track, point in (
            (west['source'], 3453612248, (728.6, 669.4)),
            ('buildings-ae21ffc2ad9ed726de63', 26985474, (1506.72, 160.0))):
        i = next(i for i, p in enumerate(polygons) if p['source'] == source_id)
        polygon = polygons[i]
        start = nodes[track]['point']
        if (not one_crossing(polygon, start, point) or not geometry.clear(start, point, i) or
                not native_connector(geometry, i, start, point, scale, walk_units)):
            raise ValueError(f'Unqualified station connection for {source_id}')
        if not polygon.get('capacity_known'):
            polygon.update(capacity_known=True, capacity=12,
                           policy_basis='Technical rail station capacity')
        polygon.update(opening_known=True, opens=360, closes=1320)
        model['doors'].append( dict(id=uid('reviewed-rail-door', source_id),
                                      building=polygon['id'], node=nodes[track]['id'], reviewed=True,
                                      basis='Technical reviewed rail station connector',
                                      points=[start, point]))
    return hashlib.sha256(raw).hexdigest()


def build():
    bundle_raw = BUNDLE.read_bytes()
    estimate_raw = ESTIMATES.read_bytes()
    workforce_raw = WORKFORCE.read_bytes()
    sectors = workforce_records(workforce_raw)
    work_uses = {use for record in sectors.values() for use in record['premises_types']}
    # Mixed-use and residential work need separate floor allocations.
    work_uses -= {'mixed_use', 'residential'}
    bundle = json.loads(bundle_raw)
    empty = dict(format='canterbury-details-spatial-review', version=1,
                 generation_id=bundle['generation_id'], edges=[], doors=[], dwellings=[])
    model = compile_model(bundle, empty, json.loads(estimate_raw))
    rail_review = apply_rail_footprint_review(model, bundle)
    review = json.loads((ROOT / 'content/canterbury-rail-operation-review.json').read_bytes())
    west = dict(id=uid('reviewed-rail-station', '151293646'),
                source='reviewed_rail:osm_way_151293646', name='Canterbury West',
                kind=1, use='transport', rings=[review['west_station_ring']],
                capacity_known=True, capacity=12,
                policy_basis='Technical station premises from registered OSM footprint')
    model['polygons'].append(west)
    polygons = model['polygons']
    scale = 65535 / max(model['width'], model['height'])
    metres_per_pixel = bundle['data']['modern_roads']['registration']['approximate_metres_per_pixel']
    # Scenario assumption for dry walking, in metres per simulated minute.
    walking_metres_per_minute = 75
    walk_units = int(walking_metres_per_minute * scale / metres_per_pixel)
    if not 8 <= walk_units <= 4096:
        raise ValueError('Registered walking budget is outside native bounds')
    geometry = Geometry(polygons)
    node_lookup = {node['id']: node['point'] for node in model['nodes']}
    sources = {edge['id']: edge for edge in bundle['data']['modern_roads']['network']['edges']}
    restricted_ways = {member['ref'] for relation in bundle['data']['modern_roads']['source_turn_restrictions']
                       for member in relation['members'] if member['type'] == 'way'}
    adjacency = defaultdict(set)
    for edge in model['edges']:
        namespace, source = edge['source'].split(':', 1)
        if namespace != 'modern_roads' or not road_allowed(sources[source]):
            continue
        if not all(geometry.clear(a, b) for a, b in segments(edge['points'])):
            continue
        adjacency[edge['from']].add(edge['to'])
        adjacency[edge['to']].add(edge['from'])
        edge.update(qualified=True, capacity=WALK_LINK_CAPACITY, junction=False,
                    basis=('technical trial: shared two-way road and walking' if sources[source]['osm_way_id'] not in restricted_ways and motor_road(sources[source])
                           else 'technical trial: surface walking on clear mapped street'))
    groups = components(adjacency)
    if not groups:
        raise ValueError('No clear pedestrian network')
    component = {node: index for index, group in enumerate(groups) for node in group}
    node_grid = defaultdict(list)
    for node in sorted(component):
        point = node_lookup[node]
        node_grid[(math.floor(point[0] / CELL), math.floor(point[1] / CELL))].append(node)
    admitted = []
    for i, polygon in enumerate(polygons):
        use = polygon['use']
        if polygon['kind'] != 1 or (use != 'residential' and use not in work_uses):
            continue
        candidates = []
        for point in entrances(polygon):
            x, y = point
            nearby = itertools.chain.from_iterable(
                node_grid.get(cell, ()) for cell in cells((x - 12, y - 12, x + 12, y + 12)))
            for node in nearby:
                distance = math.dist(node_lookup[node], point)
                if distance <= 12:
                    candidates.append((distance, node, point))
        for _, node, point in sorted(candidates):
            start = node_lookup[node]
            if (not one_crossing(polygon, start, point) or
                    not geometry.clear(start, point, i) or
                    not native_connector(geometry, i, start, point, scale, walk_units)):
                continue
            area = abs(sum(a[0] * b[1] - b[0] * a[1]
                           for ring in polygon['rings'] for a, b in segments(ring))) / 2
            work_capacity = 0
            school_capacity = 0
            if use == 'residential':
                capacity = min(128, 4 * max(1, int(area * RESIDENTIAL_FLOORS / 6)))
                basis = 'technical trial: two model floors and four places per six area units'
            else:
                floors = WORK_FLOORS + int(use in EXTRA_WORK_FLOOR_USES)
                capacity = min(128, max(1, int(area * floors / WORK_AREA_PER_PERSON)))
                school_capacity = capacity * 3 // 4 if use in SCHOOL_USES else 0
                work_capacity = capacity - school_capacity
                basis = f'technical trial: {floors} model floors, one person per floor area unit'
            polygon.update(capacity_known=True, capacity=capacity, opening_known=False,
                           policy_basis=basis)
            if work_capacity or school_capacity:
                polygon.update(opening_known=True, opens=360, closes=1320)
            door_id = uid('technical-access', polygon['source'])
            model['doors'].append(dict(id=door_id, building=polygon['id'], node=node,
                                       reviewed=True, basis='technical trial clear single-wall connector',
                                       points=[start, point]))
            admitted.append(dict(source=polygon['source'], building=polygon['id'], door=door_id,
                                 node=node, use=use, capacity=capacity, component=component[node],
                                 work_capacity=work_capacity, school_capacity=school_capacity))
            break
    operation_review = admit_rail(model, bundle, rail_review, scale, walk_units)
    model['basis'] = f'Canterbury registered access trial; walking units/minute={walk_units}'
    model['generation'] = hashlib.sha256(canonical({
        'source': model['generation'], 'rail_footprint_review': rail_review, 'rail_operation_review': operation_review, 'walking_budget': walk_units, 'walking_link_capacity': WALK_LINK_CAPACITY, 'edges': [(edge['id'], edge['basis']) for edge in model['edges'] if edge['qualified']],
        'doors': model['doors'], 'premises': admitted,
        'trial_destination_opening': [360, 1320],
    })).hexdigest()
    packed = encode(model)
    work_components = {b['component'] for b in admitted if b['work_capacity']}
    inhabited_components = {b['component'] for b in admitted if b['use'] == 'residential'}
    local = [b for b in admitted if b['component'] in inhabited_components]
    report = {
        'rail_footprint_review_sha256': rail_review,
        'rail_operation_review_sha256': operation_review,
        'source_generation': bundle['generation_id'],
        'generation': model['generation'],
        'pack_sha256': hashlib.sha256(packed).hexdigest(),
        'source_bundle_sha256': hashlib.sha256(bundle_raw).hexdigest(),
        'building_estimates_sha256': hashlib.sha256(estimate_raw).hexdigest(),
        'workforce_draft_sha256': hashlib.sha256(workforce_raw).hexdigest(),
        'metres_per_pixel': metres_per_pixel,
        'walking_metres_per_minute': walking_metres_per_minute,
        'walking_link_capacity': WALK_LINK_CAPACITY,
        'dry_native_walk_units': walk_units,
        'scope': 'Technical model assumptions for access and capacity on the actual Canterbury map',
        'qualified_edges': sum(edge['qualified'] for edge in model['edges']),
        'motor_road_edges': sum(edge['qualified'] and edge['basis'] ==
                                'technical trial: shared two-way road and walking'
                                for edge in model['edges']),
        'operating_rail_segments': sum(edge['source'].startswith('reviewed_rail:') for edge in model['edges']),
        'qualified_rail_water_crossings': 1,
        'qualified_rail_station_connections': 2,
        'qualified_nodes': len(component),
        'components': len(groups),
        'largest_component_nodes': len(groups[0]),
        'residential_model_floors': RESIDENTIAL_FLOORS,
        'work_model_floors': WORK_FLOORS,
        'extra_work_floor_uses': sorted(EXTRA_WORK_FLOOR_USES),
        'candidate_work_capacity': sum(b['work_capacity'] for b in admitted),
        'candidate_school_capacity': sum(b['school_capacity'] for b in admitted),
        'school_capacity_in_residential_components': sum(b['school_capacity'] for b in local),
        'school_sessions': SCHOOL_SESSIONS,
        'school_session_places_in_residential_components': SCHOOL_SESSIONS * sum(
            b['school_capacity'] for b in local),
        'workforce_capacity_bound': workforce_bound(local, sectors),
        'workforce_trial_shared_uses': {k: sorted(v) for k, v in TRIAL_SHARED_USES.items()},
        'workforce_trial_capacity_bound': workforce_bound(local, sectors, TRIAL_SHARED_USES),
        'residential_capacity_in_work_components': sum(
            b['capacity'] for b in admitted if b['use'] == 'residential'
            and b['component'] in work_components),
        'reviewed_entrances': len(admitted),
        'residential_capacity': sum(b['capacity'] for b in admitted if b['use'] == 'residential'),
        'premises': admitted,
    }
    return packed, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    check_runtime(True)
    packed, report = build()
    raw = canonical(report)
    if args.check:
        if PACK.read_bytes() != packed or REPORT.read_bytes() != raw:
            parser.exit(2, 'Canterbury access trial is stale\n')
    else:
        def fresh():
            if (hashlib.sha256(BUNDLE.read_bytes()).hexdigest() != report['source_bundle_sha256'] or
                    hashlib.sha256(ESTIMATES.read_bytes()).hexdigest() != report['building_estimates_sha256'] or
                    hashlib.sha256(WORKFORCE.read_bytes()).hexdigest() != report['workforce_draft_sha256']):
                raise ValueError('Access trial inputs changed before publication')
        atomic_write(PACK, packed, fresh)
        atomic_write(REPORT, raw, fresh)
    print(json.dumps({key: value for key, value in report.items() if key != 'premises'}, sort_keys=True))


if __name__ == '__main__':
    main()
