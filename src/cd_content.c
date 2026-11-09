#include "cd_city_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <zlib.h>
CdStatus cd_diag(CdDiagnostic *d, CdStatus s, const char *m) {
  if (d) {
    d->status = s;
    snprintf(d->message, sizeof d->message, "%s", m);
  }
  return s;
}
typedef struct {
  unsigned char *p, *end;
  int bad;
  CdContent *c;
  uint32_t point, ring;
} Reader;
static uint32_t u32(Reader *r) {
  if (r->bad || r->end - r->p < 4) {
    r->bad = 1;
    return 0;
  }
  uint32_t n = 0;
  for (unsigned i = 0; i < 4; i++)
    n |= (uint32_t)*r->p++ << (i * 8);
  return n;
}
static uint64_t u64(Reader *r) {
  uint64_t lo = u32(r), hi = u32(r);
  return lo | (hi << 32);
}
static double real(Reader *r) {
  uint64_t bits = u64(r);
  double d;
  memcpy(&d, &bits, 8);
  if (!isfinite(d))
    r->bad = 1;
  return d;
}
static int utf8(const unsigned char *s, size_t n) {
  for (size_t i = 0; i < n;) {
    unsigned x = s[i++], extra = 0, min = 0;
    if (!x)
      return 0;
    if (x < 128)
      continue;
    if (x >= 0xc2 && x <= 0xdf) {
      extra = 1;
      x &= 31;
      min = 128;
    } else if (x >= 0xe0 && x <= 0xef) {
      extra = 2;
      x &= 15;
      min = 2048;
    } else if (x >= 0xf0 && x <= 0xf4) {
      extra = 3;
      x &= 7;
      min = 65536;
    } else
      return 0;
    if (extra > n - i)
      return 0;
    while (extra--) {
      unsigned y = s[i++];
      if ((y & 192) != 128)
        return 0;
      x = (x << 6) | (y & 63);
    }
    if (x < min || x > 0x10ffff || (x >= 0xd800 && x <= 0xdfff))
      return 0;
  }
  return 1;
}
static char *text(Reader *r) {
  uint32_t n = u32(r);
  if (r->bad || n > 4096 || (size_t)(r->end - r->p) < (size_t)n + 1 ||
      r->p[n] || !utf8(r->p, n)) {
    r->bad = 1;
    return NULL;
  }
  char *s = (char *)r->p;
  r->p += n + 1;
  return s;
}
static CdPoint point(Reader *r) {
  CdPoint p = {real(r), real(r)};
  if (p.x < 0 || p.y < 0 || p.x > r->c->width || p.y > r->c->height)
    r->bad = 1;
  return p;
}
static uint32_t points(Reader *r, uint32_t n) {
  uint32_t offset = r->point;
  if (n > r->c->nv - r->point) {
    r->bad = 1;
    return 0;
  }
  for (uint32_t i = 0; i < n; i++)
    r->c->points[r->point++] = point(r);
  return offset;
}
static int ordered_id(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return (x > y) - (x < y);
}
#define INDEX_FN(name, field, count, type)                                     \
  uint32_t name(const CdContent *c, CdEntityId id) {                           \
    if (!c || !id)                                                             \
      return CD_NONE;                                                          \
    uint32_t lo = 0, hi = c->count;                                            \
    while (lo < hi) {                                                          \
      uint32_t mid = lo + (hi - lo) / 2;                                       \
      if (c->field[mid].id < id)                                               \
        lo = mid + 1;                                                          \
      else                                                                     \
        hi = mid;                                                              \
    }                                                                          \
    return lo < c->count && c->field[lo].id == id ? lo : CD_NONE;              \
  }
INDEX_FN(cd_polygon_index, polygons, np, CdPolygon)
INDEX_FN(cd_node_index, nodes, nn, CdNode)
INDEX_FN(cd_edge_index, edges, ne, CdEdge)
INDEX_FN(cd_door_index, doors, nd, CdDoor)
static uint32_t root(uint32_t *parents, uint32_t x) {
  while (parents[x] != x) {
    parents[x] = parents[parents[x]];
    x = parents[x];
  }
  return x;
}
static int graph_build(CdContent *c) {
  c->adj_offset =
      cd_memory_calloc((size_t)c->nn + 1, sizeof *c->adj_offset, CD_MEM_CITY);
  if (c->ne)
    c->adj_edges =
        cd_memory_calloc((size_t)c->ne * 2, sizeof *c->adj_edges, CD_MEM_CITY);
  if (c->nn)
    c->components = cd_memory_calloc(c->nn, sizeof *c->components, CD_MEM_CITY);
  uint32_t *cursor =
      c->nn ? cd_memory_calloc(c->nn, sizeof *cursor, CD_MEM_TEMP) : NULL;
  if (!c->adj_offset || (c->ne && !c->adj_edges) ||
      (c->nn && (!c->components || !cursor))) {
    cd_memory_free(cursor);
    return 0;
  }
  for (uint32_t n = 0; n < c->nn; n++)
    c->components[n] = n;
  for (uint32_t e = 0; e < c->ne; e++) {
    CdEdge *p = &c->edges[e];
    c->adj_offset[p->from + 1]++;
    c->adj_offset[p->to + 1]++;
    if (p->qualified)
      c->components[root(c->components, p->from)] = root(c->components, p->to);
  }
  for (uint32_t n = 0; n < c->nn; n++) {
    c->adj_offset[n + 1] += c->adj_offset[n];
    cursor[n] = c->adj_offset[n];
  }
  for (uint32_t e = 0; e < c->ne; e++) {
    CdEdge *p = &c->edges[e];
    c->adj_edges[cursor[p->from]++] = e;
    c->adj_edges[cursor[p->to]++] = e;
  }
  for (uint32_t n = 0; n < c->nn; n++)
    c->components[n] = root(c->components, n);
  cd_memory_free(cursor);
  return 1;
}
void cd_content_retain(CdContent *c) {
  if (c)
    c->refs++;
}
void cd_content_close(CdContent *c) {
  if (!c || --c->refs)
    return;
  cd_memory_free(c->polygons);
  cd_memory_free(c->rings);
  cd_memory_free(c->points);
  cd_memory_free(c->nodes);
  cd_memory_free(c->edges);
  cd_memory_free(c->doors);
  cd_memory_free(c->sites);
  cd_memory_free(c->adj_offset);
  cd_memory_free(c->adj_edges);
  cd_memory_free(c->components);
  cd_memory_free(c->file);
  cd_memory_free(c);
}
CdStatus cd_content_open(const char *path, CdContent **out, CdDiagnostic *d) {
  if (!path || !out || *out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Content output must be empty");
  FILE *f = fopen(path, "rb");
  if (!f)
    return cd_diag(d, CD_IO_ERROR, "Cannot open runtime content pack");
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return cd_diag(d, CD_IO_ERROR, "Cannot size runtime content");
  }
  long length = ftell(f);
  if (length < 20 || length > 256 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) {
    fclose(f);
    return cd_diag(d, CD_INVALID_DATA, "Runtime content size outside bounds");
  }
  CdContent *c = cd_memory_calloc(1, sizeof *c, CD_MEM_CITY);
  if (!c) {
    fclose(f);
    return cd_diag(d, CD_CAPACITY, "Cannot allocate content owner");
  }
  c->refs = 1;
  c->file_bytes=(size_t)length;
  c->file = cd_memory_calloc((size_t)length, 1, CD_MEM_CITY);
  if (!c->file) {
    fclose(f);
    cd_content_close(c);
    return cd_diag(d, CD_CAPACITY, "Cannot allocate content bytes");
  }
  int read = fread(c->file, 1, (size_t)length, f) == (size_t)length;
  fclose(f);
  Reader r = {c->file + 8, c->file + length, 0, c, 0, 0};
  uint64_t payload = u64(&r);
  uint32_t crc = u32(&r);
  if (!read || memcmp(c->file, "CDCT0002", 8) ||
      payload != (uint64_t)length - 20 ||
      crc != (uint32_t)crc32(0, c->file + 20, (uInt)payload))
    goto invalid;
  c->width = u32(&r);
  c->height = u32(&r);
  char *generation = text(&r);
  c->basis = text(&r);
  if (!generation || strlen(generation) != 64 || !c->basis || !c->width ||
      !c->height || c->width > 4096 || c->height > 4096)
    goto invalid;
  for (unsigned i = 0; i < 64; i++)
    if (!((generation[i] >= '0' && generation[i] <= '9') ||
          (generation[i] >= 'a' && generation[i] <= 'f')))
      goto invalid;
  memcpy(c->generation, generation, 65);
  c->np = u32(&r);
  c->nr = u32(&r);
  c->nv = u32(&r);
  c->nn = u32(&r);
  c->ne = u32(&r);
  c->nd = u32(&r);
  c->ns = u32(&r);
  if (r.bad || c->np > 20000 || c->nr > 100000 || c->nv > 4000000 ||
      c->nn > 50000 || c->ne > 100000 || c->nd > 20000 || c->ns > 10000)
    goto invalid;
#define ARRAY(field, count)                                                    \
  if (c->count) {                                                              \
    c->field = cd_memory_calloc(c->count, sizeof *c->field, CD_MEM_CITY);      \
    if (!c->field)                                                             \
      goto capacity;                                                           \
  }
  ARRAY(polygons, np)
  ARRAY(rings, nr)
  ARRAY(points, nv)
  ARRAY(nodes, nn) ARRAY(edges, ne) ARRAY(doors, nd) ARRAY(sites, ns)
#undef ARRAY
      for (uint32_t i = 0; i < c->np; i++) {
    CdPolygon *p = &c->polygons[i];
    p->id = u64(&r);
    p->kind = u32(&r);
    p->capacity_known = u32(&r);
    p->capacity = u32(&r);
    p->opening_known = u32(&r);
    p->opens = u32(&r);
    p->closes = u32(&r);
    p->locked = u32(&r);
    p->count = u32(&r);
    p->ring = r.ring;
    p->source = text(&r);
    p->name = text(&r);
    p->use = text(&r);
    p->policy_basis = text(&r);
    p->confidence = real(&r);
    p->minx = p->miny = INFINITY;
    p->maxx = p->maxy = -INFINITY;
    if (r.bad || !p->id || p->kind < CD_BUILDING || p->kind > CD_RAIL ||
        p->capacity_known > 1 || p->opening_known > 1 || p->locked > 1 ||
        p->capacity > 128 || (!p->capacity_known && p->capacity) ||
        p->opens >= 1440 || p->closes >= 1440 || p->confidence < 0 ||
        p->confidence > 1 || !p->count || p->count > c->nr - r.ring)
      goto invalid;
    for (uint32_t j = 0; j < p->count; j++) {
      CdRing *ring = &c->rings[r.ring++];
      ring->count = u32(&r);
      if (ring->count < 4 || ring->count > c->nv - r.point)
        goto invalid;
      ring->offset = points(&r, ring->count);
      CdPoint first = c->points[ring->offset],
              last = c->points[ring->offset + ring->count - 1];
      if (first.x != last.x || first.y != last.y)
        goto invalid;
      for (uint32_t k = 0; k < ring->count; k++) {
        CdPoint a = c->points[ring->offset + k];
        p->minx = fmin(p->minx, a.x);
        p->miny = fmin(p->miny, a.y);
        p->maxx = fmax(p->maxx, a.x);
        p->maxy = fmax(p->maxy, a.y);
      }
    }
  }
  if (c->np > 1)
    qsort(c->polygons, c->np, sizeof *c->polygons, ordered_id);
  for (uint32_t i = 0; i < c->nn; i++) {
    CdNode *n = &c->nodes[i];
    n->id = u64(&r);
    n->source = text(&r);
    n->p = point(&r);
    if (!n->id || r.bad)
      goto invalid;
  }
  if (c->nn > 1)
    qsort(c->nodes, c->nn, sizeof *c->nodes, ordered_id);
  for (uint32_t i = 0; i < c->ne; i++) {
    CdEdge *e = &c->edges[i];
    e->id = u64(&r);
    e->from = cd_node_index(c, u64(&r));
    e->to = cd_node_index(c, u64(&r));
    e->qualified = u32(&r);
    e->bridge = u32(&r);
    e->rail_crossing = u32(&r);
    e->capacity = u32(&r);
    e->grade = (int32_t)u32(&r);
    e->junction = u32(&r);
    e->source = text(&r);
    e->basis = text(&r);
    e->count = u32(&r);
    if (r.bad || !e->id || e->from == CD_NONE || e->to == CD_NONE ||
        e->count < 2 || e->count > c->nv - r.point || e->qualified > 1 ||
        e->bridge > 1 || e->rail_crossing > 1 || e->junction > 1 ||
        e->capacity > cd_smoke_capacity() || e->grade < -16 || e->grade > 16 ||
        (e->qualified && (!e->capacity || !e->basis[0])) ||
        (!e->qualified && (e->bridge || e->rail_crossing)))
      goto invalid;
    e->point = points(&r, e->count);
    CdPoint start = c->points[e->point],
            end = c->points[e->point + e->count - 1], a = c->nodes[e->from].p,
            b = c->nodes[e->to].p;
    if (hypot(start.x - a.x, start.y - a.y) > 1e-5 ||
        hypot(end.x - b.x, end.y - b.y) > 1e-5)
      goto invalid;
    for (uint32_t j = 1; j < e->count; j++) {
      a = c->points[e->point + j - 1];
      b = c->points[e->point + j];
      double distance = hypot(a.x - b.x, a.y - b.y);
      if (distance <= 0)
        goto invalid;
      e->length += distance;
      if (e->qualified &&
          cd_segment_blocked(c, a, b, e->bridge, e->rail_crossing, CD_NONE))
        goto invalid;
    }
  }
  if (c->ne > 1)
    qsort(c->edges, c->ne, sizeof *c->edges, ordered_id);
  for (uint32_t i = 0; i < c->nd; i++) {
    CdDoor *door = &c->doors[i];
    door->id = u64(&r);
    door->building = cd_polygon_index(c, u64(&r));
    door->node = cd_node_index(c, u64(&r));
    door->reviewed = u32(&r);
    door->basis = text(&r);
    door->count = u32(&r);
    if (r.bad || !door->id || door->building == CD_NONE ||
        c->polygons[door->building].kind != CD_BUILDING ||
        door->node == CD_NONE || door->reviewed > 1 || door->count < 2 ||
        door->count > c->nv - r.point)
      goto invalid;
    door->point = points(&r, door->count);
    if (!cd_door_valid(c, door))
      goto invalid;
  }
  if (c->nd > 1)
    qsort(c->doors, c->nd, sizeof *c->doors, ordered_id);
  for (uint32_t i = 0; i < c->ns; i++) {
    CdSite *s = &c->sites[i];
    s->id = u64(&r);
    s->building = u64(&r);
    s->located = u32(&r);
    s->p = point(&r);
    s->source = text(&r);
    s->name = text(&r);
    s->religion = text(&r);
    s->denomination = text(&r);
    s->status = text(&r);
    s->notes = text(&r);
    s->site_type = text(&r);
    s->source_record = text(&r);
    s->location_basis = text(&r);
    s->setting_year = u32(&r);
    s->identity_verified = u32(&r);
    s->review_required = u32(&r);
    s->confidence_known = u32(&r);
    s->confidence = real(&r);
    if (r.bad || s->identity_verified > 1 || s->review_required > 1 ||
        s->confidence_known > 1 || s->confidence < 0 || s->confidence > 1 ||
        (!s->confidence_known && s->confidence) ||
        (s->setting_year && s->setting_year != 1940))
      goto invalid;
    if (r.bad || !s->id || s->located > 1 ||
        (s->building && cd_polygon_index(c, s->building) == CD_NONE) ||
        (!s->located && (s->p.x || s->p.y)))
      goto invalid;
  }
  if (r.bad || r.p != r.end || r.point != c->nv || r.ring != c->nr)
    goto invalid;
  size_t total = (size_t)c->np + c->nn + c->ne + c->nd + c->ns;
  uint64_t *ids =
      total ? cd_memory_calloc(total, sizeof *ids, CD_MEM_TEMP) : NULL;
  if (total && !ids)
    goto capacity;
  size_t at = 0;
  for (uint32_t i = 0; i < c->np; i++)
    ids[at++] = c->polygons[i].id;
  for (uint32_t i = 0; i < c->nn; i++)
    ids[at++] = c->nodes[i].id;
  for (uint32_t i = 0; i < c->ne; i++)
    ids[at++] = c->edges[i].id;
  for (uint32_t i = 0; i < c->nd; i++)
    ids[at++] = c->doors[i].id;
  for (uint32_t i = 0; i < c->ns; i++)
    ids[at++] = c->sites[i].id;
  if (total > 1)
    qsort(ids, total, sizeof *ids, ordered_id);
  int duplicates = 0;
  for (size_t i = 1; i < total; i++)
    if (ids[i] == ids[i - 1])
      duplicates = 1;
  cd_memory_free(ids);
  if (duplicates)
    goto invalid;
  if (!graph_build(c))
    goto capacity;
  /* Shared source nodes may join grades only with an explicit reviewed ramp. */
  for (uint32_t n = 0; n < c->nn; n++)
    for (uint32_t i = c->adj_offset[n]; i < c->adj_offset[n + 1]; i++)
      for (uint32_t j = i + 1; j < c->adj_offset[n + 1]; j++) {
        CdEdge *a = &c->edges[c->adj_edges[i]], *b = &c->edges[c->adj_edges[j]];
        if (a->qualified && b->qualified && a->grade != b->grade &&
            !a->junction && !b->junction)
          goto invalid;
      }
  *out = c;
  return cd_diag(d, CD_OK, "Immutable runtime content admitted");
capacity:
  cd_content_close(c);
  return cd_diag(d, CD_CAPACITY, "Runtime content allocation/budget failed");
invalid:
  cd_content_close(c);
  return cd_diag(
      d, CD_INVALID_DATA,
      "Invalid runtime geometry, topology, review flags or checksum");
}
size_t cd_content_building_count(const CdContent *c) {
  size_t n = 0;
  if (c)
    for (uint32_t i = 0; i < c->np; i++)
      n += c->polygons[i].kind == CD_BUILDING;
  return n;
}
CdStatus cd_content_building(const CdContent *c, size_t index,
                             CdBuildingView *out, CdDiagnostic *d) {
  if (!c || !out)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Missing content/building output");
  for (uint32_t i = 0; i < c->np; i++)
    if (c->polygons[i].kind == CD_BUILDING) {
      if (index-- == 0) {
        CdPolygon *p = &c->polygons[i];
        *out = (CdBuildingView){p->id,
                                p->kind,
                                p->capacity,
                                p->source,
                                p->name,
                                p->use,
                                p->policy_basis,
                                p->confidence,
                                (int)p->capacity_known,
                                (int)p->opening_known,
                                (int)p->locked,
                                p->opens,
                                p->closes};
        return cd_diag(d, CD_OK, "Candidate building view");
      }
    }
  return cd_diag(d, CD_UNKNOWN_REFERENCE, "Unknown building index");
}
size_t cd_content_node_count(const CdContent *c) { return c ? c->nn : 0; }
CdStatus cd_content_node(const CdContent *c, size_t i, CdEntityId *id,
                         CdPoint *p, CdDiagnostic *d) {
  if (!c || !id || !p || i >= c->nn)
    return cd_diag(d, CD_INVALID_ARGUMENT, "Unknown node index");
  *id = c->nodes[i].id;
  *p = c->nodes[i].p;
  return cd_diag(d, CD_OK, "Source node view");
}
