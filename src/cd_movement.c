#include "cd_city_internal.h"
#include "cd_native.h"
#include <stdint.h>
double cd_city_scale(const CdWorld *w) {
  return 65535.0 / fmax(w->content->width, w->content->height);
}
CdPoint cd_city_position(const CdWorld *w, const CdCitizen *p) {
  uint16_t x = 0, y = 0;
  cd_native_position(w->native, p->civil.ref, &x, &y);
  double scale = cd_city_scale(w);
  return (CdPoint){x / scale, y / scale};
}
static double distance(CdPoint a, CdPoint b) {
  return hypot(a.x - b.x, a.y - b.y);
}
static int at_node(CdWorld *w, CdCitizen *p) {
  return p->anchor < w->content->nn &&
         distance(cd_city_position(w, p), w->content->nodes[p->anchor].p) <=
             1.0 / cd_city_scale(w);
}
static void release_edge(CdWorld *w, CdCitizen *p) {
  if (p->held_edge != CD_NONE) {
    if (w->traffic[p->held_edge])
      w->traffic[p->held_edge]--;
    p->held_edge = CD_NONE;
  }
}
CdStatus cd_world_place(CdWorld *w, CdPersonRef ref, CdEntityId node,
                        CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  uint32_t n = w ? cd_node_index(w->content, node) : CD_NONE;
  if (!p || n == CD_NONE)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown person/source node");
  if (p->civil.position_known)
    return cd_diag(d, CD_INVALID_STATE,
                   "Initial placement cannot teleport a placed resident");
  int qualified = 0;
  for (uint32_t i = w->content->adj_offset[n];
       i < w->content->adj_offset[n + 1]; i++)
    qualified |= w->content->edges[w->content->adj_edges[i]].qualified;
  if (!qualified)
    return cd_diag(d, CD_UNSUPPORTED,
                   "Source node has no explicitly qualified walk link");
  double scale = cd_city_scale(w);
  CdPoint target = w->content->nodes[n].p;
  uint16_t x = (uint16_t)llround(target.x * scale),
           y = (uint16_t)llround(target.y * scale);
  CdPoint quantized = {x / scale, y / scale};
  if (cd_segment_blocked(w->content, quantized, quantized, 0, 0, CD_NONE))
    return cd_diag(d, CD_INVALID_DATA, "Quantized placement is obstructed");
  cd_native_write(w->native, ref, x, y);
  p->civil.position_known = 1;
  p->anchor = n;
  return cd_diag(d, CD_OK,
                 "Initial spatial admission on qualified source node; no "
                 "travel tick consumed");
}
CdStatus cd_world_travel(CdWorld *w, CdPersonRef ref, CdEntityId destination,
                         CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown/foreign travel actor");
  if (p->vehicle)
    return cd_diag(d, CD_INVALID_STATE,
                   "Embarked passenger is in vehicle custody");
  if (!p->civil.position_known)
    return cd_diag(d, CD_INVALID_STATE, "Resident position remains unplaced");
  uint32_t goal = cd_node_index(w->content, destination);
  if (goal == CD_NONE)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown destination node");
  if (!at_node(w, p)) {
    if (p->door == CD_NONE && p->destination == destination &&
        p->route_index < p->route.count) {
      p->travel = CD_ROUTE_TRAVELLING;
      p->reason = CD_ROUTE_OK;
      return cd_diag(d, CD_OK, "Interrupted trip resumed from native position");
    }
    return cd_diag(d, CD_INVALID_STATE,
                   "New destination requires a source-node boundary; no "
                   "teleport/rejoin shortcut");
  }
  CdRoute route;
  CdStatus status = cd_route_indices(w, p->anchor, goal, &route);
  if (status != CD_OK)
    return cd_diag(d, status, cd_route_reason(route.reason));
  release_edge(w, p);
  p->route = route;
  p->route_index = 0;
  p->vertex = CD_NONE;
  p->door = CD_NONE;
  p->destination = destination;
  p->travel = route.count ? CD_ROUTE_TRAVELLING : CD_ROUTE_ARRIVED;
  p->reason = CD_ROUTE_OK;
  return cd_diag(d, CD_OK,
                 "Bounded source-polyline travel intention published");
}
CdStatus cd_world_stop(CdWorld *w, CdPersonRef ref, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  if (!p)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown travel actor");
  p->travel = CD_ROUTE_CANCELLED;
  return cd_diag(d, CD_OK,
                 "Travel interrupted in place; existing edge occupancy "
                 "retained until departure");
}
CdStatus cd_world_travel_view(const CdWorld *w, CdPersonRef ref,
                              CdTravelView *out, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen((CdWorld *)w, ref);
  if (!p || !out)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown travel view");
  CdEntityId edge = 0;
  if (p->door != CD_NONE)
    edge = w->content->doors[p->door].id;
  else if (p->route_index < p->route.count)
    edge = p->route.edges[p->route_index];
  *out = (CdTravelView){p->travel,
                        p->reason,
                        p->destination,
                        edge,
                        p->civil.position_known,
                        p->civil.position_known ? cd_city_position(w, p)
                                                : (CdPoint){0, 0}};
  return cd_diag(d, CD_OK, cd_route_reason(p->reason));
}
static CdStatus access_now(CdWorld *w, CdCitizen *p, CdDoor *door) {
  if (p->entering && !p->door_emergency && !cd_usable(w, door->building))
    return CD_INVALID_STATE;
  CdCivilTime time;
  cd_smoke_civil_time(cd_native_minutes(w->native) + w->origin_offset, &time,
                      NULL);
  CdAccess a = {p->civil.dwelling == w->content->polygons[door->building].id,
                cd_employee(w, p, door->building), p->door_key,
                p->door_emergency, time.minute_of_day};
  return cd_access_polygon(&w->content->polygons[door->building],
                           w->locks[door->building], &a);
}
static CdStatus door_intent(CdWorld *w, CdPersonRef ref, CdEntityId id, int key,
                            int emergency, int entering, CdDiagnostic *d) {
  CdCitizen *p = cd_citizen(w, ref);
  uint32_t i = w ? cd_door_index(w->content, id) : CD_NONE;
  if (!p || i == CD_NONE)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown person/door");
  if (p->vehicle)
    return cd_diag(d, CD_INVALID_STATE,
                   "Embarked passenger is in vehicle custody");
  CdDoor *door = &w->content->doors[i];
  if (!door->reviewed)
    return cd_diag(d, CD_UNSUPPORTED, "Entrance remains a technical candidate");
  int resume = p->door == i && p->entering == entering;
  CdPoint endpoint =
      w->content->points[door->point + (entering ? 0 : door->count - 1)];
  if (!resume &&
      (!p->civil.position_known ||
       distance(cd_city_position(w, p), endpoint) > 1.0 / cd_city_scale(w) ||
       (entering ? (p->anchor != door->node || p->inside != CD_NONE)
                 : p->inside != door->building)))
    return cd_diag(
        d, CD_INVALID_STATE,
        "Resident must physically reach the connector endpoint first");
  CdCivilTime time;
  cd_smoke_civil_time(cd_native_minutes(w->native) + w->origin_offset, &time,
                      NULL);
  CdAccess access = {
      p->civil.dwelling == w->content->polygons[door->building].id,
      cd_employee(w, p, door->building), key, emergency, time.minute_of_day};
  CdStatus status = cd_access_polygon(&w->content->polygons[door->building],
                                      w->locks[door->building], &access);
  if (status != CD_OK)
    return cd_diag(d, status,
                   "Ordinary key/opening/access policy denies entrance");
  if (entering && !emergency && !cd_usable(w, door->building))
    return cd_diag(d, CD_INVALID_STATE, "Premises require repair");
  if (entering && !resume) {
    CdPolygon *building = &w->content->polygons[door->building];
    if (!building->capacity_known)
      return cd_diag(d, CD_UNSUPPORTED, "Entrance capacity remains unknown");
    size_t load = cd_housing_load(w, door->building) +
                  cd_vehicle_extra(w, p, door->building);
    int reserved =
        p->civil.dwelling == building->id || p->presence == building->id;
    if (load > cd_capacity(w, door->building) ||
        (!reserved && load >= cd_capacity(w, door->building)) ||
        (!cd_capacity(w, door->building)))
      return cd_diag(d, CD_CAPACITY,
                     "No dwelling/visitor/physical entrance capacity");
  }
  if (resume) {
    p->door_key = key;
    p->door_emergency = emergency;
    p->travel = CD_ROUTE_TRAVELLING;
    p->reason = CD_ROUTE_OK;
    return cd_diag(d, CD_OK,
                   "Interrupted connector resumed from native position");
  }
  release_edge(w, p);
  p->door = i;
  p->entering = entering;
  p->door_key = key;
  p->door_emergency = emergency;
  p->door_vertex = entering ? 1 : door->count - 2;
  p->travel = CD_ROUTE_TRAVELLING;
  p->reason = CD_ROUTE_OK;
  p->destination = id;
  return cd_diag(d, CD_OK,
                 "Reviewed entrance movement queued; position unchanged until "
                 "native minute");
}
CdStatus cd_world_enter(CdWorld *w, CdPersonRef ref, CdEntityId id, int key,
                        int emergency, CdDiagnostic *d) {
  return door_intent(w, ref, id, key, emergency, 1, d);
}
CdStatus cd_world_exit(CdWorld *w, CdPersonRef ref, CdEntityId id, int key,
                       int emergency, CdDiagnostic *d) {
  return door_intent(w, ref, id, key, emergency, 0, d);
}
CdStatus cd_world_lock(CdWorld *w, CdEntityId id, int locked, CdDiagnostic *d) {
  uint32_t b = w ? cd_polygon_index(w->content, id) : CD_NONE;
  if (b == CD_NONE || w->content->polygons[b].kind != CD_BUILDING ||
      (locked != 0 && locked != 1))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Unknown lock target/state");
  w->locks[b] = (unsigned char)locked;
  return cd_diag(d, CD_OK, "Ordinary lock state changed");
}
/* Compute every displacement from the native body, never a cached map position.
   Rounding is toward the start so the native-unit budget cannot be exceeded. */
static double advance(CdWorld *w, CdCitizen *p, CdPoint target, double budget,
                      int bridge, int rail, uint32_t door_building) {
  uint16_t x, y;
  cd_native_position(w->native, p->civil.ref, &x, &y);
  double scale = cd_city_scale(w);
  long tx = lround(target.x * scale), ty = lround(target.y * scale);
  double dx = tx - x, dy = ty - y, len = hypot(dx, dy);
  if (len < .5)
    return 0;
  double factor = fmin(1, budget / len);
  long nx = x + (long)(copysign(floor(fabs(dx) * factor), dx)),
       ny = y + (long)(copysign(floor(fabs(dy) * factor), dy));
  CdPoint a = {x / scale, y / scale}, b = {nx / scale, ny / scale};
  if (door_building != CD_NONE) {
    int edge1 = cd_polygon_contains(w->content, door_building, b, 1),
        edge0 = cd_polygon_contains(w->content, door_building, b, 0);
    if (edge1 != edge0 && budget > 1)
      return advance(w, p, target, budget - 1, bridge, rail, door_building);
  }
  double actual = hypot(nx - x, ny - y);
  if (actual < .5)
    return 0;
  if (cd_segment_blocked(w->content, a, b, bridge, rail, door_building)) {
    p->travel = CD_ROUTE_WAITING;
    p->reason = CD_ROUTE_GEOMETRY;
    return -1;
  }
  if (door_building != CD_NONE) {
    int from = cd_polygon_contains(w->content, door_building, a, 0),
        to = cd_polygon_contains(w->content, door_building, b, 0);
    unsigned crossings = cd_polygon_crossings(w->content, door_building, a, b);
    if (crossings != (unsigned)(from != to)) {
      p->travel = CD_ROUTE_WAITING;
      p->reason = CD_ROUTE_GEOMETRY;
      return -1;
    }
  }
  cd_native_write(w->native, p->civil.ref, (uint16_t)nx, (uint16_t)ny);
  return actual;
}
void cd_movement_tick(CdWorld *w) {
  CdContent *c = w->content;
  for (size_t i = 0; i < w->count; i++) {
    CdCitizen *p = &w->people[i];
    if (p->activity != CD_ACTIVITY_IDLE)
      continue;
    if (p->travel != CD_ROUTE_TRAVELLING && p->travel != CD_ROUTE_WAITING)
      continue;
    if (p->door != CD_NONE) {
      CdDoor *door = &c->doors[p->door];
      if (access_now(w, p, door) != CD_OK) {
        p->travel = CD_ROUTE_WAITING;
        p->reason = CD_ROUTE_ACCESS;
        continue;
      }
    } else if (at_node(w, p) && p->route_index < p->route.count) {
      uint32_t ei = cd_edge_index(c, p->route.edges[p->route_index]);
      CdEdge *e = &c->edges[ei];
      size_t load = w->crowd[ei] + w->traffic[ei] - (p->held_edge == ei);
      if (cd_edge_closed(w, ei) || load >= e->capacity) {
        CdRoute replacement;
        CdStatus status = cd_route_indices(
            w, p->anchor, cd_node_index(c, p->destination), &replacement);
        if (status != CD_OK) {
          p->travel = CD_ROUTE_WAITING;
          p->reason = replacement.reason;
          continue;
        }
        release_edge(w, p);
        p->route = replacement;
        p->route_index = 0;
        p->vertex = CD_NONE;
      }
    }
    if (!cd_native_claim(w->native, p->civil.ref)) {
      p->travel = CD_ROUTE_WAITING;
      p->reason = CD_ROUTE_ACCESS;
      continue;
    }
    p->activity = CD_ACTIVITY_TRAVEL;
    p->travel = CD_ROUTE_TRAVELLING;
    p->reason = CD_ROUTE_OK;
    double budget = cd_native_walk_units() / (1 + w->rain);
    unsigned iterations = 0;
    while (budget >= 1 && iterations++ < 2048) {
      if (p->door != CD_NONE) {
        CdDoor *door = &c->doors[p->door];
        CdPoint target = c->points[door->point + p->door_vertex];
        double moved = advance(w, p, target, budget, 0, 0, door->building);
        if (moved < 0)
          break;
        budget -= moved;
        if (distance(cd_city_position(w, p), target) > 1.0 / cd_city_scale(w))
          break;
        if ((p->entering && p->door_vertex + 1 == door->count) ||
            (!p->entering && !p->door_vertex)) {
          p->inside = p->entering ? door->building : CD_NONE;
          p->anchor = p->entering ? CD_NONE : door->node;
          p->door = CD_NONE;
          p->travel = CD_ROUTE_ARRIVED;
          break;
        }
        if (p->entering)
          p->door_vertex++;
        else
          p->door_vertex--;
        continue;
      }
      if (p->route_index >= p->route.count) {
        release_edge(w, p);
        p->travel = CD_ROUTE_ARRIVED;
        break;
      }
      uint32_t ei = cd_edge_index(c, p->route.edges[p->route_index]);
      CdEdge *e = &c->edges[ei];
      if (cd_edge_closed(w, ei)) {
        p->travel = CD_ROUTE_WAITING;
        p->reason = CD_ROUTE_CLOSED;
        break;
      }
      if (p->held_edge != ei) {
        if ((size_t)w->crowd[ei] + w->traffic[ei] >= e->capacity) {
          p->travel = CD_ROUTE_WAITING;
          p->reason = CD_ROUTE_CROWDED;
          break;
        }
        w->traffic[ei]++;
        p->held_edge = ei;
      }
      int forward = e->from == p->anchor;
      if (p->vertex == CD_NONE)
        p->vertex = forward ? 1 : e->count - 2;
      CdPoint target = c->points[e->point + p->vertex];
      double crowd = 1 + (double)w->crowd[ei] / e->capacity;
      double moved = advance(w, p, target, budget / crowd, e->bridge,
                             e->rail_crossing, CD_NONE);
      if (moved < 0)
        break;
      budget -= moved * crowd;
      if (distance(cd_city_position(w, p), target) > 1.0 / cd_city_scale(w))
        break;
      if ((forward && p->vertex + 1 == e->count) || (!forward && !p->vertex)) {
        p->anchor = forward ? e->to : e->from;
        p->route_index++;
        p->vertex = CD_NONE;
        release_edge(w, p);
      } else if (forward)
        p->vertex++;
      else
        p->vertex--;
    }
    if (p->door == CD_NONE && p->route_index == p->route.count &&
        p->travel == CD_ROUTE_TRAVELLING) {
      release_edge(w, p);
      p->travel = CD_ROUTE_ARRIVED;
    }
  }
}
