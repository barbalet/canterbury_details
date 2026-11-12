#!/usr/bin/env python3
"""Register source ACR track candidates and report obstructions and station gaps."""
import argparse
from collections import defaultdict
import hashlib
import heapq
import json
import math

from qualify_canterbury_access import BUNDLE, ESTIMATES, Geometry, ROOT, cells, compile_model, MARGIN, apply_rail_footprint_review
from compile_content import atomic_write, canonical

SOURCE = ROOT / 'content/canterbury-west-sturry-rail-candidates.json'
OUTPUT = ROOT / 'build/content/CanterburyRailAudit.json'


def project(coordinate, registration):
    lon, lat = registration['origin_lonlat']
    a, b = registration['coefficients_east_north']
    east = (coordinate[0] - lon) * 111320 * math.cos(math.radians(lat)) - a[0]
    north = (coordinate[1] - lat) * 111320 - b[0]
    determinant = a[1] * b[2] - a[2] * b[1]
    return [(east * b[2] - north * a[2]) * 1000 / determinant,
            (north * a[1] - east * b[1]) * 1000 / determinant]


def shortest_path(graph, start, end, avoid_conflicts=False):
    distances = {start: (0, 0)}
    previous = {}
    queue = [(0, 0, start)]
    while queue:
        conflicts, distance, node = heapq.heappop(queue)
        if (conflicts, distance) != distances[node]:
            continue
        if node == end:
            break
        for adjacent, length, way, blocked in graph[node]:
            candidate = (conflicts + (blocked if avoid_conflicts else 0), distance + length)
            if candidate < distances.get(adjacent, (math.inf, math.inf)):
                distances[adjacent] = candidate
                previous[adjacent] = (node, way)
                heapq.heappush(queue, (*candidate, adjacent))
    if end not in distances:
        raise ValueError('No connected source track between station candidates')
    path = []
    while end != start:
        origin, way = previous[end]
        path.append((origin, end, way))
        end = origin
    return path[::-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corrected', action='store_true')
    args = parser.parse_args()
    raw = SOURCE.read_bytes()
    source = json.loads(raw)
    bundle = json.loads(BUNDLE.read_bytes())
    registration = bundle['data']['modern_roads']['registration']
    points = {int(node): project(point, registration) for node, point in source['nodes'].items()}
    empty = dict(format='canterbury-details-spatial-review', version=1,
                 generation_id=bundle['generation_id'], edges=[], doors=[], dwellings=[])
    model = compile_model(bundle, empty, json.loads(ESTIMATES.read_bytes()))
    review = apply_rail_footprint_review(model, bundle) if args.corrected else None
    geometry = {kind: Geometry([p for p in model['polygons'] if p['kind'] == code])
                for kind, code in [('building', 1), ('water', 2), ('rail_ink', 3)]}
    graph = defaultdict(list)
    ways = {way['id']: way for way in source['tracks']}
    for way in ways.values():
        if way['tags'].get('usage') != 'main' and way['tags'].get('service') != 'crossover':
            continue
        for a, b in zip(way['nodes'], way['nodes'][1:]):
            if a not in points or b not in points or not all(
                    0 <= x <= model['width'] and 0 <= y <= model['height']
                    for x, y in (points[a], points[b])):
                continue
            length = math.dist(points[a], points[b])
            blocked = int(any(not geometry[kind].clear(points[a], points[b])
                              for kind in ('building', 'water')))
            graph[a].append((b, length, way['id'], blocked))
            graph[b].append((a, length, way['id'], blocked))
    stations = []
    for item in source['stations']:
        marker = points[item['id']]
        node = min(graph, key=lambda n: math.dist(points[n], marker))
        distance = math.dist(points[node], marker) * registration['approximate_metres_per_pixel']
        stations.append(dict(**item, marker=marker, candidate_track_node=node,
                             marker_to_candidate_metres=round(distance, 2),
                             operational_connector=False))
    endpoints = [station['candidate_track_node'] for station in stations]
    shortest = shortest_path(graph, *endpoints)
    path = shortest_path(graph, *endpoints, avoid_conflicts=True)
    segments = []
    for a, b, way in path:
        start, end = points[a], points[b]
        bounds = (min(start[0], end[0]) - MARGIN, min(start[1], end[1]) - MARGIN,
                  max(start[0], end[0]) + MARGIN, max(start[1], end[1]) + MARGIN)
        conflicts = {}
        for kind, layer in geometry.items():
            candidates = {index for cell in cells(bounds) for index in layer.index.get(cell, ())}
            sources = [layer.polygons[index]['source'] for index in sorted(candidates)
                       if not Geometry([layer.polygons[index]]).clear(start, end)]
            if sources:
                conflicts[kind] = sources
        segments.append(dict(nodes=[a, b], way=way, points=[points[a], points[b]],
                             bridge_tag=ways[way]['tags'].get('bridge'), conflicts=conflicts))
    result = dict(source_url=source['source_url'], attribution=source['attribution'],
                  source_subset_sha256=hashlib.sha256(raw).hexdigest(),
                  map_generation=bundle['generation_id'], footprint_review_sha256=review, stations=stations,
                  candidate_segments=len(segments), shortest_source_segments=len(shortest),
                  selection='minimize blocked segments, then registered length',
                  operational_segments=0,
                  conflicts={kind: sum(kind in s['conflicts'] for s in segments) for kind in geometry},
                  segments=segments)
    target = OUTPUT.with_name("CanterburyRailCorrectedAudit.json") if args.corrected else OUTPUT
    atomic_write(target, canonical(result))
    print(json.dumps({k: v for k, v in result.items() if k != 'segments'}, sort_keys=True))


if __name__ == '__main__':
    main()
