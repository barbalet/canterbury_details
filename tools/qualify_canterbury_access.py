#!/usr/bin/env python3
"""Derive a labelled pedestrian trial from the audited Canterbury geometry."""
import argparse
from collections import defaultdict
import hashlib
import itertools
import json
import math
import re

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
WORK_USES = {'workshop', 'industrial', 'commercial', 'agricultural_support',
             'agricultural', 'education', 'education_support', 'healthcare',
             'care_support', 'civic', 'civic_support', 'religious'}
RESIDENTIAL_FLOORS = 2


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


def build():
    bundle_raw = BUNDLE.read_bytes()
    estimate_raw = ESTIMATES.read_bytes()
    bundle = json.loads(bundle_raw)
    empty = dict(format='canterbury-details-spatial-review', version=1,
                 generation_id=bundle['generation_id'], edges=[], doors=[], dwellings=[])
    model = compile_model(bundle, empty, json.loads(estimate_raw))
    polygons = model['polygons']
    scale = 65535 / max(model['width'], model['height'])
    sdk_header = (ROOT / 'apesdk/universe/universe.h').read_text()
    walk_units = int(re.search(r'^#define FATIGUE_SPEED_THRESHOLD (\d+)$', sdk_header, re.M)[1])
    geometry = Geometry(polygons)
    node_lookup = {node['id']: node['point'] for node in model['nodes']}
    sources = {edge['id']: edge for edge in bundle['data']['modern_roads']['network']['edges']}
    adjacency = defaultdict(set)
    for edge in model['edges']:
        namespace, source = edge['source'].split(':', 1)
        if namespace != 'modern_roads' or not road_allowed(sources[source]):
            continue
        if not all(geometry.clear(a, b) for a, b in segments(edge['points'])):
            continue
        adjacency[edge['from']].add(edge['to'])
        adjacency[edge['to']].add(edge['from'])
        edge.update(qualified=True, capacity=24, junction=False,
                    basis='technical trial: surface walking on clear mapped street')
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
        if polygon['kind'] != 1 or (use != 'residential' and use not in WORK_USES):
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
            if use == 'residential':
                capacity = min(128, 4 * max(1, int(area * RESIDENTIAL_FLOORS / 6)))
                basis = 'technical trial: two model floors and four places per six area units'
            else:
                capacity = min(64, max(1, int(area / 4)))
                basis = 'technical trial: one work place per four footprint area units'
            polygon.update(capacity_known=True, capacity=capacity, opening_known=False,
                           policy_basis=basis)
            door_id = uid('technical-access', polygon['source'])
            model['doors'].append(dict(id=door_id, building=polygon['id'], node=node,
                                       reviewed=True, basis='technical trial clear single-wall connector',
                                       points=[start, point]))
            admitted.append(dict(source=polygon['source'], building=polygon['id'], door=door_id,
                                 node=node, use=use, capacity=capacity, component=component[node]))
            break
    model['basis'] = 'Canterbury technical access trial from audited map and provisional uses'
    model['generation'] = hashlib.sha256(canonical({
        'source': model['generation'], 'edges': [edge['id'] for edge in model['edges'] if edge['qualified']],
        'doors': model['doors'], 'premises': admitted,
    })).hexdigest()
    packed = encode(model)
    work_components = {b['component'] for b in admitted if b['use'] in WORK_USES}
    report = {
        'source_generation': bundle['generation_id'],
        'generation': model['generation'],
        'pack_sha256': hashlib.sha256(packed).hexdigest(),
        'source_bundle_sha256': hashlib.sha256(bundle_raw).hexdigest(),
        'building_estimates_sha256': hashlib.sha256(estimate_raw).hexdigest(),
        'dry_native_walk_units': walk_units,
        'scope': 'Technical model assumptions for access and capacity on the actual Canterbury map',
        'qualified_edges': sum(edge['qualified'] for edge in model['edges']),
        'qualified_nodes': len(component),
        'components': len(groups),
        'largest_component_nodes': len(groups[0]),
        'residential_model_floors': RESIDENTIAL_FLOORS,
        'candidate_work_capacity': sum(b['capacity'] for b in admitted if b['use'] in WORK_USES),
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
                    hashlib.sha256(ESTIMATES.read_bytes()).hexdigest() != report['building_estimates_sha256']):
                raise ValueError('Access trial inputs changed before publication')
        atomic_write(PACK, packed, fresh)
        atomic_write(REPORT, raw, fresh)
    print(json.dumps({key: value for key, value in report.items() if key != 'premises'}, sort_keys=True))


if __name__ == '__main__':
    main()
