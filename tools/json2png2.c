/* Adapt the final assembled JSON layers to the unmodified upstream raster core.
 * Stroke sampling matches simulation-map/complete_roads.py::render_png. */
#define main cd_original_json2png_main
#include "../canterbury/newcanterbury/json2png.c"
#undef main
static void stroke(Json *j, int points, int color, int radius) {
  if (points < 0 || j->t[points].type != '[')
    fail("Missing line coordinates");
  int previous = -1;
  for (int p = points + 1; p < j->t[points].next; p = j->t[p].next) {
    if (j->t[p].type != '[' || j->t[p].size != 2)
      fail("Invalid line point");
    if (previous >= 0) {
      double ax = number(j, previous + 1),
             ay = number(j, j->t[previous + 1].next);
      double bx = number(j, p + 1), by = number(j, j->t[p + 1].next);
      if (!isfinite(ax) || !isfinite(ay) || !isfinite(bx) || !isfinite(by) ||
          fabs(ax) > 8192 || fabs(ay) > 8192 || fabs(bx) > 8192 ||
          fabs(by) > 8192)
        fail("Invalid bounded line coordinate");
      int steps = (int)ceil(hypot(bx - ax, by - ay) * 2);
      if (steps < 1)
        steps = 1;
      for (int i = 0; i <= steps; i++) {
        int x = (int)floor(ax + (bx - ax) * i / steps),
            y = (int)floor(ay + (by - ay) * i / steps);
        for (int dx = -radius; dx <= radius; dx++)
          for (int dy = -radius; dy <= radius; dy++)
            pixel(x + dx, y + dy, color, 1);
      }
    }
    previous = p;
  }
}
int main(int argc, char **argv) {
  if (argc != 3)
    fail("Usage: json2png2 assembled-render.json output.png");
  Json j = json_read(argv[1]);
  if (!eq(&j, field(&j, 0, "format"), "canterbury-assembled-render") ||
      numfield(&j, 0, "version") != 1)
    fail("Invalid assembled render format");
  double width = numfield(&j, 0, "width"), height = numfield(&j, 0, "height");
  if (width != floor(width) || height != floor(height) || width < 1 ||
      height < 1 || width > 4096 || height > 4096)
    fail("Invalid canvas");
  W = width;
  H = height;
  int palette = field(&j, 0, "palette");
  if (palette < 0 || j.t[palette].type != '[')
    fail("Missing palette");
  for (int a = palette + 1; a < j.t[palette].next; a = j.t[a].next) {
    if (np >= 256 || j.t[a].type != '[' || j.t[a].size != 3)
      fail("Invalid palette");
    int b = a + 1;
    for (int c = 0; c < 3; c++) {
      double v = number(&j, b);
      if (v < 0 || v > 255 || v != floor(v))
        fail("Invalid color");
      pal[np][c] = v;
      b = j.t[b].next;
    }
    np++;
  }
  if (np < 2)
    fail("Missing colors");
  image = alloc((size_t)W * H * 3);
  for (int p = 0; p < W * H; p++)
    for (int c = 0; c < 3; c++)
      image[3 * p + c] = pal[0][c];
  int features = field(&j, 0, "features");
  if (features < 0 || j.t[features].type != '[')
    fail("Missing layers");
  int polygons = 0, lines = 0;
  for (int a = features + 1; a < j.t[features].next; a = j.t[a].next) {
    double color = numfield(&j, a, "color");
    if (color < 0 || color >= np || color != floor(color))
      fail("Invalid layer color");
    if (field(&j, a, "rings") >= 0) {
      polygon(&j, field(&j, a, "rings"), color);
      polygons++;
    } else {
      double radius = numfield(&j, a, "radius");
      if (radius < 0 || radius > 1 || radius != floor(radius))
        fail("Invalid stroke radius");
      stroke(&j, field(&j, a, "coordinates"), color, radius);
      lines++;
    }
  }
  write_png(argv[2], image, W, H);
  printf("Rendered final assembled map: %d footprints, %d connected/source "
         "lines -> %s\n",
         polygons, lines, argv[2]);
  free(image);
  json_free(&j);
  return 0;
}
