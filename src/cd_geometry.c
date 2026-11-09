#include "cd_city_internal.h"
static double cross(CdPoint a, CdPoint b, CdPoint p) {
  return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}
static int on(CdPoint a, CdPoint b, CdPoint p) {
  return fabs(cross(a, b, p)) <= CD_EPS && p.x >= fmin(a.x, b.x) - CD_EPS &&
         p.x <= fmax(a.x, b.x) + CD_EPS && p.y >= fmin(a.y, b.y) - CD_EPS &&
         p.y <= fmax(a.y, b.y) + CD_EPS;
}
static int intersects(CdPoint a, CdPoint b, CdPoint c, CdPoint d) {
  double x = cross(a, b, c), y = cross(a, b, d), u = cross(c, d, a),
         v = cross(c, d, b);
  return (x * y < 0 && u * v < 0) || on(a, b, c) || on(a, b, d) ||
         on(c, d, a) || on(c, d, b);
}
int cd_ring_contains(const CdContent *c, const CdRing *r, CdPoint p,
                     int *boundary) {
  int inside = 0;
  *boundary = 0;
  for (uint32_t i = 1; i < r->count; i++) {
    CdPoint a = c->points[r->offset + i - 1], b = c->points[r->offset + i];
    if (on(a, b, p)) {
      *boundary = 1;
      return 1;
    }
    if ((a.y > p.y) != (b.y > p.y) &&
        p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
      inside = !inside;
  }
  return inside;
}
int cd_polygon_contains(const CdContent *c, uint32_t index, CdPoint p,
                        int boundary) {
  const CdPolygon *poly = &c->polygons[index];
  if (p.x < poly->minx || p.x > poly->maxx || p.y < poly->miny ||
      p.y > poly->maxy)
    return 0;
  int inside = 0;
  for (uint32_t i = 0; i < poly->count; i++) {
    int edge = 0;
    int hit = cd_ring_contains(c, &c->rings[poly->ring + i], p, &edge);
    if (edge)
      return boundary;
    inside ^= hit;
  }
  return inside;
}
int cd_segment_blocked(const CdContent *c, CdPoint a, CdPoint b, int bridge,
                       int rail, uint32_t ignore) {
  for (uint32_t i = 0; i < c->np; i++) {
    const CdPolygon *p = &c->polygons[i];
    if (i == ignore || (p->kind == CD_WATER && bridge) ||
        (p->kind == CD_RAIL && rail))
      continue;
    if (fmax(a.x, b.x) < p->minx || fmin(a.x, b.x) > p->maxx ||
        fmax(a.y, b.y) < p->miny || fmin(a.y, b.y) > p->maxy)
      continue;
    if (cd_polygon_contains(c, i, a, 1) || cd_polygon_contains(c, i, b, 1))
      return 1;
    for (uint32_t j = 0; j < p->count; j++) {
      CdRing r = c->rings[p->ring + j];
      for (uint32_t k = 1; k < r.count; k++)
        if (intersects(a, b, c->points[r.offset + k - 1],
                       c->points[r.offset + k]))
          return 1;
    }
  }
  return 0;
}
unsigned cd_polygon_crossings(const CdContent *c, uint32_t index, CdPoint a,
                              CdPoint b) {
  const CdPolygon *p = &c->polygons[index];
  unsigned hits = 0;
  CdPoint last = {0, 0};
  for (uint32_t j = 0; j < p->count; j++) {
    CdRing ring = c->rings[p->ring + j];
    for (uint32_t k = 1; k < ring.count; k++) {
      CdPoint u = c->points[ring.offset + k - 1],
              v = c->points[ring.offset + k];
      if (!intersects(a, b, u, v))
        continue;
      double den = (b.x - a.x) * (v.y - u.y) - (b.y - a.y) * (v.x - u.x);
      if (fabs(den) < CD_EPS)
        return UINT32_MAX;
      double t = ((u.x - a.x) * (v.y - u.y) - (u.y - a.y) * (v.x - u.x)) / den;
      CdPoint h = {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
      if (!hits || hypot(h.x - last.x, h.y - last.y) > CD_EPS) {
        hits++;
        last = h;
      }
    }
  }
  return hits;
}
int cd_door_valid(const CdContent *c, const CdDoor *d) {
  const CdPolygon *p = &c->polygons[d->building];
  CdPoint first = c->points[d->point],
          last = c->points[d->point + d->count - 1], node = c->nodes[d->node].p;
  if (hypot(first.x - node.x, first.y - node.y) > 1e-5 ||
      !cd_polygon_contains(c, d->building, last, 0))
    return 0;
  for (uint32_t j = 0; j < p->count; j++) {
    int boundary = 0;
    if (cd_ring_contains(c, &c->rings[p->ring + j], first, &boundary))
      return 0;
  }
  unsigned hits = 0;
  CdPoint hit = {0, 0};
  for (uint32_t i = 1; i < d->count; i++) {
    CdPoint a = c->points[d->point + i - 1], b = c->points[d->point + i];
    if (hypot(a.x - b.x, a.y - b.y) <= CD_EPS ||
        cd_segment_blocked(c, a, b, 0, 0, d->building))
      return 0;
    for (uint32_t j = 0; j < p->count; j++) {
      CdRing ring = c->rings[p->ring + j];
      for (uint32_t k = 1; k < ring.count; k++) {
        CdPoint u = c->points[ring.offset + k - 1],
                v = c->points[ring.offset + k];
        if (!intersects(a, b, u, v))
          continue;
        double den = (b.x - a.x) * (v.y - u.y) - (b.y - a.y) * (v.x - u.x);
        if (fabs(den) < CD_EPS)
          return 0;
        double t =
            ((u.x - a.x) * (v.y - u.y) - (u.y - a.y) * (v.x - u.x)) / den;
        CdPoint h = {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
        if (!hits || hypot(hit.x - h.x, hit.y - h.y) > CD_EPS) {
          hits++;
          hit = h;
        }
      }
    }
  }
  return hits == 1;
}
CdStatus cd_content_pick(const CdContent *c, CdPoint p, CdEntityId *out,
                         CdDiagnostic *d) {
  if (!c || !out || !isfinite(p.x) || !isfinite(p.y))
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid map pick");
  for (uint32_t i = 0; i < c->np; i++)
    if (c->polygons[i].kind == CD_BUILDING && cd_polygon_contains(c, i, p, 1)) {
      *out = c->polygons[i].id;
      return cd_diag(d, CD_OK,
                     "Candidate footprint selected; confidence retained");
    }
  return cd_diag(d, CD_UNKNOWN_REFERENCE, "No building at source point");
}
CdStatus cd_content_segment(const CdContent *c, CdPoint a, CdPoint b,
                            int bridge, int rail, CdDiagnostic *d) {
  if (!c || !isfinite(a.x) || !isfinite(a.y) || !isfinite(b.x) ||
      !isfinite(b.y) || a.x < 0 || a.y < 0 || b.x < 0 || b.y < 0 ||
      a.x > c->width || b.x > c->width || a.y > c->height || b.y > c->height)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Invalid segment");
  return cd_diag(d,
                 cd_segment_blocked(c, a, b, bridge, rail, CD_NONE)
                     ? CD_INVALID_STATE
                     : CD_OK,
                 "Ordinary wall/water/rail clearance check");
}
CdStatus cd_access_polygon(const CdPolygon *p, int locked,
                           const CdAccess *access) {
  if (access->emergency_authorized)
    return CD_OK;
  if (locked && !access->has_key)
    return CD_INVALID_STATE;
  if (access->resident || access->employee)
    return CD_OK;
  if (!p->opening_known)
    return CD_UNSUPPORTED;
  unsigned t = access->minute_of_day;
  int open = p->opens == p->closes ||
             (p->opens < p->closes ? (t >= p->opens && t < p->closes)
                                   : (t >= p->opens || t < p->closes));
  return open ? CD_OK : CD_INVALID_STATE;
}
CdStatus cd_content_access(const CdContent *c, CdEntityId building,
                           const CdAccess *access, CdDiagnostic *d) {
  uint32_t i = cd_polygon_index(c, building);
  if (!c || !access || access->minute_of_day >= 1440 || i == CD_NONE ||
      c->polygons[i].kind != CD_BUILDING)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Unknown building/access context");
  CdStatus status =
      cd_access_polygon(&c->polygons[i], c->polygons[i].locked, access);
  return cd_diag(d, status,
                 status == CD_OK ? "Ordinary access permitted"
                 : status == CD_UNSUPPORTED
                     ? "Opening/access policy unknown"
                     : "Locked or outside ordinary opening interval");
}
CdStatus cd_content_connector(const CdContent *c, CdEntityId id, int *reviewed,
                              CdDiagnostic *d) {
  uint32_t i = cd_door_index(c, id);
  if (!c || !reviewed || i == CD_NONE)
    return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown entrance connector");
  *reviewed = (int)c->doors[i].reviewed;
  return cd_diag(d, CD_OK,
                 *reviewed
                     ? "Reviewed entrance connector"
                     : "Technical candidate; access disabled pending review");
}
