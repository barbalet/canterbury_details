#!/usr/bin/env python3
"""Generate bounded, shape-preserving footprint moves for registered rail."""
import hashlib
import json
from shapely import LineString, Polygon, STRtree, make_valid, union_all
from shapely.affinity import translate

from audit_canterbury_rail import OUTPUT, SOURCE
from qualify_canterbury_access import BUNDLE, ESTIMATES, ROOT, compile_model
from compile_content import atomic_write, canonical

DESTINATION = ROOT / 'content/canterbury-rail-footprint-review.json'
LIMIT = 12


def shape(record):
    result = Polygon()
    for ring in record['rings']:
        result = result.symmetric_difference(make_valid(Polygon(ring)))
    return result


def main():
    audit = json.loads(OUTPUT.read_bytes())
    bundle = json.loads(BUNDLE.read_bytes())
    source_hash = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    if audit['map_generation'] != bundle['generation_id'] or audit['source_subset_sha256'] != source_hash:
        raise ValueError('Stale rail geometry audit')
    empty = dict(format='canterbury-details-spatial-review', version=1,
                 generation_id=bundle['generation_id'], edges=[], doors=[], dwellings=[])
    model = compile_model(bundle, empty, json.loads(ESTIMATES.read_bytes()))
    buildings = {p['source']: shape(p) for p in model['polygons'] if p['kind'] == 1}
    barriers = union_all([shape(p) for p in model['polygons'] if p['kind'] != 1])
    track = union_all([LineString(s['points']) for s in audit['segments']]).buffer(.1)
    roads = [LineString(e['points']).buffer(.05) for e in model['edges']]
    road_index = STRtree(roads)
    affected = [name for name, polygon in buildings.items() if polygon.intersects(track)]
    offsets = sorted(((x, y) for x in range(-LIMIT, LIMIT + 1)
                      for y in range(-LIMIT, LIMIT + 1) if x or y),
                     key=lambda pair: (pair[0]**2 + pair[1]**2, pair))
    moves, unresolved = [], []
    for name in sorted(affected, key=lambda name: (-buildings[name].area, name)):
        original = buildings[name]
        other = [p for key, p in buildings.items() if key != name]
        index = STRtree(other)
        for dx, dy in offsets:
            if dx * dx + dy * dy > LIMIT * LIMIT:
                continue
            moved = translate(original, dx, dy)
            x0, y0, x1, y1 = moved.bounds
            if not (0 <= x0 < x1 <= model['width'] and 0 <= y0 < y1 <= model['height']):
                continue
            if moved.intersects(track) or moved.intersects(barriers):
                continue
            if any(moved.intersection(other[i]).area > 1e-8 for i in index.query(moved)):
                continue
            if any(moved.intersects(roads[i]) for i in road_index.query(moved)):
                continue
            buildings[name] = moved
            moves.append(dict(source=name, offset_pixels=[dx, dy]))
            break
        else:
            unresolved.append(name)
    trims = []
    for name in unresolved[:]:
        if name != 'buildings-77492e7f5fdb8c3c3193':
            continue
        original = buildings[name]
        trimmed = original.difference(track.buffer(.1))
        if trimmed.is_empty or trimmed.area < original.area * .8:
            continue
        parts = [trimmed] if trimmed.geom_type == 'Polygon' else list(trimmed.geoms)
        rings = [list(ring.coords) for part in parts
                 for ring in (part.exterior, *part.interiors)]
        trims.append(dict(source=name, rings=rings,
                          removed_square_pixels=round(original.area - trimmed.area, 6)))
        unresolved.remove(name)
    result = dict(map_generation=bundle['generation_id'], rail_source_sha256=source_hash,
                  basis='Bounded schematic translation preserving footprint shape and road clearance',
                  maximum_displacement_pixels=LIMIT, moves=moves, trims=trims, unresolved=unresolved)
    atomic_write(DESTINATION, canonical(result))
    print(json.dumps(dict(moves=len(moves), unresolved=unresolved)))


if __name__ == '__main__':
    main()
