#include "cd_city_internal.h"
static int less(const CdWorld *w, uint32_t a, uint32_t b) {
  return w->distance[a] < w->distance[b] ||
         (w->distance[a] == w->distance[b] &&
          w->content->nodes[a].id < w->content->nodes[b].id);
}
static void swap(CdWorld *w, size_t a, size_t b) {
  uint32_t t = w->heap[a];
  w->heap[a] = w->heap[b];
  w->heap[b] = t;
  w->heap_pos[w->heap[a]] = (uint32_t)a;
  w->heap_pos[w->heap[b]] = (uint32_t)b;
}
static void push(CdWorld *w, uint32_t n) {
  size_t i = w->heap_pos[n];
  if (i == CD_NONE) {
    i = w->heap_count++;
    w->heap[i] = n;
    w->heap_pos[n] = (uint32_t)i;
  }
  while (i && less(w, w->heap[i], w->heap[(i - 1) / 2])) {
    swap(w, i, (i - 1) / 2);
    i = (i - 1) / 2;
  }
}
static uint32_t pop(CdWorld *w) {
  uint32_t n = w->heap[0];
  w->heap_pos[n] = CD_NONE;
  w->heap_count--;
  if (w->heap_count) {
    w->heap[0] = w->heap[w->heap_count];
    w->heap_pos[w->heap[0]] = 0;
    size_t i = 0;
    for (;;) {
      size_t left = i * 2 + 1, right = left + 1, best = i;
      if (left < w->heap_count && less(w, w->heap[left], w->heap[best]))
        best = left;
      if (right < w->heap_count && less(w, w->heap[right], w->heap[best]))
        best = right;
      if (best == i)
        break;
      swap(w, i, best);
      i = best;
    }
  }
  return n;
}
const char *cd_route_reason(CdRouteReason r) {
  static const char *s[] = {"route available",
                            "unknown source node",
                            "links remain unqualified",
                            "disconnected source components",
                            "closed link blocks route",
                            "crowd capacity blocks route",
                            "geometry/quantization blocks movement",
                            "bounded route limit exceeded",
                            "resident position is unplaced",
                            "ordinary entrance access denied"};
  return (unsigned)r < sizeof s / sizeof *s ? s[r]
                                            : "unknown routing diagnostic";
}
CdStatus cd_route_indices(CdWorld *w, uint32_t start, uint32_t goal,
                          CdRoute *out) {
  CdContent *c = w->content;
  CdRoute result = {0};
  result.reason = CD_ROUTE_UNKNOWN_NODE;
  if (start >= c->nn || goal >= c->nn) {
    *out = result;
    return CD_UNKNOWN_REFERENCE;
  }
  if (start == goal) {
    result.reason = CD_ROUTE_OK;
    *out = result;
    return CD_OK;
  }
  if (c->components[start] != c->components[goal]) {
    int s = 0, g = 0;
    for (uint32_t a = c->adj_offset[start]; a < c->adj_offset[start + 1]; a++)
      s |= c->edges[c->adj_edges[a]].qualified;
    for (uint32_t a = c->adj_offset[goal]; a < c->adj_offset[goal + 1]; a++)
      g |= c->edges[c->adj_edges[a]].qualified;
    result.reason = s && g ? CD_ROUTE_DISCONNECTED : CD_ROUTE_UNQUALIFIED;
    *out = result;
    return CD_INVALID_STATE;
  }
  for (uint32_t i = 0; i < c->nn; i++) {
    w->distance[i] = INFINITY;
    w->previous[i] = CD_NONE;
    w->heap_pos[i] = CD_NONE;
  }
  w->heap_count = 0;
  w->distance[start] = 0;
  push(w, start);
  int closed = 0, crowded = 0;
  while (w->heap_count) {
    uint32_t node = pop(w);
    if (node == goal)
      break;
    for (uint32_t i = c->adj_offset[node]; i < c->adj_offset[node + 1]; i++) {
      uint32_t ei = c->adj_edges[i];
      CdEdge *e = &c->edges[ei];
      if (!e->qualified)
        continue;
      if (cd_edge_closed(w, ei)) {
        closed = 1;
        continue;
      }
      size_t load = (size_t)w->crowd[ei] + w->traffic[ei];
      if (load >= e->capacity) {
        crowded = 1;
        continue;
      }
      uint32_t next = e->from == node ? e->to : e->from;
      double cost =
          w->distance[node] + e->length * (1 + (double)load / e->capacity);
      if (cost < w->distance[next]) {
        w->distance[next] = cost;
        w->previous[next] = ei;
        push(w, next);
      }
    }
  }
  if (!isfinite(w->distance[goal])) {
    result.reason = closed    ? CD_ROUTE_CLOSED
                    : crowded ? CD_ROUTE_CROWDED
                              : CD_ROUTE_DISCONNECTED;
    *out = result;
    return CD_INVALID_STATE;
  }
  uint32_t node = goal;
  CdEntityId reverse[CD_ROUTE_EDGE_LIMIT];
  size_t n = 0;
  while (node != start) {
    if (n == CD_ROUTE_EDGE_LIMIT) {
      result.reason = CD_ROUTE_LIMIT;
      *out = result;
      return CD_CAPACITY;
    }
    uint32_t ei = w->previous[node];
    if (ei == CD_NONE) {
      result.reason = CD_ROUTE_DISCONNECTED;
      *out = result;
      return CD_INVALID_STATE;
    }
    CdEdge *e = &c->edges[ei];
    reverse[n++] = e->id;
    node = e->from == node ? e->to : e->from;
  }
  result.count = n;
  result.cost = w->distance[goal];
  result.reason = CD_ROUTE_OK;
  for (size_t i = 0; i < n; i++)
    result.edges[i] = reverse[n - i - 1];
  *out = result;
  return CD_OK;
}
CdStatus cd_world_plan(CdWorld *w, CdEntityId from, CdEntityId to, CdRoute *out,
                       CdDiagnostic *d) {
  if (!w || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing route world/output");
  CdStatus s = cd_route_indices(w, cd_node_index(w->content, from),
                                cd_node_index(w->content, to), out);
  return cd_diag(d, s, cd_route_reason(out->reason));
}
CdStatus cd_world_close_edge(CdWorld *w, CdEntityId id, int closed,
                             CdDiagnostic *d) {
  uint32_t e = w ? cd_edge_index(w->content, id) : CD_NONE;
  if (e == CD_NONE || (closed != 0 && closed != 1))
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown closure target");
  w->closed[e] = (unsigned char)closed;
  return cd_diag(
      d, CD_OK,
      closed ? "Source link closed; affected trips replan at nodes or wait"
             : "Closure removed; unqualified links remain disabled");
}
CdStatus cd_world_set_crowd(CdWorld *w, CdEntityId id, size_t load,
                            CdDiagnostic *d) {
  uint32_t e = w ? cd_edge_index(w->content, id) : CD_NONE;
  if (e == CD_NONE || load > cd_smoke_capacity())
    return cd_diag(d, CD_INVALID_ARGUMENT,
                   "Unknown crowd target or load outside capacity bounds");
  w->crowd[e] = (uint32_t)load;
  return cd_diag(d, CD_OK, "Explicit technical background crowd load set");
}
