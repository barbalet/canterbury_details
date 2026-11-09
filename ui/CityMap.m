#import "CityMap.h"
@implementation CdCenteredClipView
- (NSRect)constrainBoundsRect:(NSRect)proposed {
  NSRect r = [super constrainBoundsRect:proposed];
  NSSize d = self.documentView.frame.size;
  if (d.width < r.size.width)
    r.origin.x = (d.width - r.size.width) / 2;
  if (d.height < r.size.height)
    r.origin.y = (d.height - r.size.height) / 2;
  return r;
}
@end
@interface CdMapView ()
@property(nonatomic) NSPoint anchorWindow, anchorScroll;
@property(nonatomic) BOOL dragged;
@end
static void centerViewport(NSScrollView *scroll, NSPoint point) {
  NSClipView *clip = scroll.contentView;
  NSSize visible = clip.bounds.size;
  [clip scrollToPoint:NSMakePoint(point.x - visible.width / 2,
                                  point.y - visible.height / 2)];
  [scroll reflectScrolledClipView:clip];
}
@implementation CdMapView
- (void)setBuildings:(NSArray *)buildings {
  _buildings = [buildings copy];
  NSMutableArray *paths = [NSMutableArray new];
  for (NSDictionary *building in buildings) {
    NSBezierPath *path = [NSBezierPath bezierPath];
    path.windingRule = NSWindingRuleEvenOdd;
    for (NSArray *ring in building[@"geometry"][@"rings"]) {
      BOOL first = YES;
      for (NSArray *xy in ring) {
        NSPoint p = NSMakePoint([xy[0] doubleValue], [xy[1] doubleValue]);
        if (first) {
          [path moveToPoint:p];
          first = NO;
        } else
          [path lineToPoint:p];
      }
      [path closePath];
    }
    [paths addObject:path];
  }
  self.footprints = paths;
  self.needsDisplay = YES;
}
- (BOOL)isFlipped {
  return YES;
}
- (BOOL)acceptsFirstResponder {
  return YES;
}
- (void)resetCursorRects {
  [self addCursorRect:self.bounds cursor:NSCursor.openHandCursor];
}
- (BOOL)isAccessibilityElement {
  return YES;
}
- (NSString *)accessibilityRole {
  return NSAccessibilityImageRole;
}
- (NSString *)accessibilityLabel {
  return @"Canterbury assembled city map, rendered by json2png2";
}
- (NSString *)accessibilityHelp {
  return @"Drag or scroll to pan. Pinch or Option-scroll to zoom. Plus/minus "
         @"zoom; arrows pan; F fits map; Space runs or pauses. Click an "
         @"institution or building to inspect it.";
}
- (NSString *)accessibilityValue {
  return
      [NSString stringWithFormat:@"Zoom %.0f percent. %@",
                                 self.enclosingScrollView.magnification * 100,
                                 self.selected ? self.selected[@"name"]
                                                     ?: self.selected[@"id"]
                                               : @"No selection"];
}
- (void)drawRect:(NSRect)dirty {
  (void)dirty;
  [[NSColor colorWithCalibratedRed:.94 green:.90 blue:.80 alpha:1] setFill];
  NSRectFill(self.bounds);
  [NSGraphicsContext currentContext].imageInterpolation =
      NSImageInterpolationHigh;
  [NSGraphicsContext currentContext].shouldAntialias = YES;
  [self.basemap drawInRect:self.bounds
                  fromRect:NSZeroRect
                 operation:NSCompositingOperationSourceOver
                  fraction:1
            respectFlipped:YES
                     hints:nil];
  // Paths are cached when content changes, never reconstructed during zoom.
  [[NSColor colorWithCalibratedWhite:68.0 / 255 alpha:1] setFill];
  for (NSBezierPath *path in self.footprints)
    if (NSIntersectsRect(path.bounds, self.visibleRect))
      [path fill];
  CGFloat z = MAX(.1, self.enclosingScrollView.magnification);
  if (self.showFootprints) {
    [[NSColor colorWithCalibratedRed:.08 green:.52 blue:.52
                               alpha:.5] setStroke];
    for (NSBezierPath *p in self.footprints) {
      if (!NSIntersectsRect(p.bounds, self.visibleRect))
        continue;
      p.lineWidth = 1 / z;
      [p stroke];
    }
  }
  if (self.selected[@"geometry"]) {
    NSUInteger i = [self.buildings indexOfObjectIdenticalTo:self.selected];
    if (i != NSNotFound) {
      NSBezierPath *p = self.footprints[i];
      [[NSColor colorWithCalibratedRed:.06 green:.65 blue:.65
                                 alpha:.35] setFill];
      [p fill];
      [[NSColor colorWithCalibratedRed:0 green:.35 blue:.37 alpha:1] setStroke];
      p.lineWidth = 3 / z;
      [p stroke];
    }
  }
  if (self.selected[@"ref"]) {
    uint64_t selected = [self.selected[@"id"] longLongValue];
    const CdAgentGlyph *glyph = self.agentGlyphs.bytes;
    for (size_t i = 0; i < self.agentGlyphs.length / sizeof *glyph; i++)
      if (glyph[i].located && glyph[i].ref.person_id == selected) {
        [NSColor.systemBlueColor setStroke];
        NSBezierPath *halo = [NSBezierPath
            bezierPathWithOvalInRect:NSMakeRect(glyph[i].position.x - 7 / z,
                                                glyph[i].position.y - 7 / z,
                                                14 / z, 14 / z)];
        halo.lineWidth = 2 / z;
        [halo stroke];
        break;
      }
  }
  if (self.showAgents) {
    NSBezierPath *markers = [NSBezierPath bezierPath];
    const CdAgentGlyph *agents = self.agentGlyphs.bytes;
    size_t count = self.agentGlyphs.length / sizeof *agents;
    CGFloat radius = 3 / z;
    NSRect visible = NSInsetRect(self.visibleRect, -radius, -radius);
    for (size_t i = 0; i < count; i++) {
      if (!agents[i].located)
        continue;
      NSPoint p = NSMakePoint(agents[i].position.x, agents[i].position.y);
      if (!NSPointInRect(p, visible))
        continue;
      [markers moveToPoint:NSMakePoint(p.x - radius, p.y)];
      [markers lineToPoint:NSMakePoint(p.x + radius, p.y)];
      [markers moveToPoint:NSMakePoint(p.x, p.y - radius)];
      [markers lineToPoint:NSMakePoint(p.x, p.y + radius)];
    }
    [[NSColor colorWithCalibratedRed:0 green:.42 blue:.40 alpha:1] setStroke];
    markers.lineWidth = 1 / z;
    [markers stroke];
  }
  if (self.showSites)
    for (NSDictionary *s in self.sites) {
      NSArray *xy = s[@"position_pixels"];
      if (![xy isKindOfClass:NSArray.class])
        continue;
      BOOL selected = [s isEqual:self.selected];
      CGFloat radius = (selected ? 7 : 4.5) / z;
      NSPoint p = NSMakePoint([xy[0] doubleValue], [xy[1] doubleValue]);
      NSBezierPath *dot = [NSBezierPath
          bezierPathWithOvalInRect:NSMakeRect(p.x - radius, p.y - radius,
                                              radius * 2, radius * 2)];
      [(selected ? [NSColor colorWithCalibratedRed:0 green:.43 blue:.47 alpha:1]
                 : [NSColor colorWithCalibratedRed:.83
                                             green:.35
                                              blue:.14
                                             alpha:1]) setFill];
      [dot fill];
      [NSColor.whiteColor setStroke];
      dot.lineWidth = 1.5 / z;
      [dot stroke];
    }
}
- (void)zoomBy:(CGFloat)factor {
  NSScrollView *s = self.enclosingScrollView;
  CGFloat z = MIN(s.maxMagnification,
                  MAX(s.minMagnification, s.magnification * factor));
  [s setMagnification:z
      centeredAtPoint:NSMakePoint(NSMidX(self.visibleRect),
                                  NSMidY(self.visibleRect))];
  self.needsDisplay = YES;
}
- (void)fitMap {
  NSScrollView *s = self.enclosingScrollView;
  CGFloat z = MIN(s.contentSize.width / 1600, s.contentSize.height / 1600);
  [s setMagnification:MAX(s.minMagnification, z)];
  centerViewport(s, NSMakePoint(800, 800));
  self.needsDisplay = YES;
}
- (void)centerOn:(NSPoint)p {
  NSScrollView *scroll = self.enclosingScrollView;
  [scroll setMagnification:MAX(1.4, scroll.magnification)];
  /* setMagnification:centeredAtPoint: does not recenter an unchanged zoom.
     Always scroll explicitly in source-map coordinates after sizing. */
  centerViewport(scroll, p);
  self.needsDisplay = YES;
}
- (void)mouseDown:(NSEvent *)e {
  [self.window makeFirstResponder:self];
  self.anchorWindow = e.locationInWindow;
  self.anchorScroll = self.enclosingScrollView.contentView.bounds.origin;
  self.dragged = NO;
  [[NSCursor closedHandCursor] push];
}
- (void)mouseDragged:(NSEvent *)e {
  CGFloat z = self.enclosingScrollView.magnification;
  CGFloat dx = e.locationInWindow.x - self.anchorWindow.x,
          dy = e.locationInWindow.y - self.anchorWindow.y;
  if (hypot(dx, dy) > 3)
    self.dragged = YES;
  NSClipView *clip = self.enclosingScrollView.contentView;
  [clip scrollToPoint:NSMakePoint(self.anchorScroll.x - dx / z,
                                  self.anchorScroll.y + dy / z)];
  [self.enclosingScrollView reflectScrolledClipView:clip];
}
- (void)mouseUp:(NSEvent *)e {
  [NSCursor pop];
  if (self.dragged)
    return;
  NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
  NSDictionary *pick = nil;
  CGFloat distance = 10 / MAX(.1, self.enclosingScrollView.magnification);
  if (self.showSites)
    for (NSDictionary *site in self.sites) {
      NSArray *xy = site[@"position_pixels"];
      if (![xy isKindOfClass:NSArray.class])
        continue;
      CGFloat d = hypot(p.x - [xy[0] doubleValue], p.y - [xy[1] doubleValue]);
      if (d < distance) {
        distance = d;
        pick = site;
      }
    }
  NSString *kind = @"institution";
  if (self.showAgents) {
    const CdAgentGlyph *agents = self.agentGlyphs.bytes;
    size_t count = self.agentGlyphs.length / sizeof *agents;
    const CdAgentGlyph *nearest = NULL;
    double best = 8 / MAX(.1, self.enclosingScrollView.magnification);
    for (size_t i = 0; i < count; i++)
      if (agents[i].located) {
        double d =
            hypot(p.x - agents[i].position.x, p.y - agents[i].position.y);
        if (d < best || (d == best && nearest &&
                         agents[i].ref.person_id < nearest->ref.person_id)) {
          best = d;
          nearest = &agents[i];
        }
      }
    if (nearest) {
      CdPersonRef ref = nearest->ref;
      pick = @{
        @"id" : [NSString
            stringWithFormat:@"%llu", (unsigned long long)ref.person_id],
        @"name" : [NSString stringWithFormat:@"Resident #%llu",
                                             (unsigned long long)ref.person_id],
        @"ref" : [NSValue valueWithBytes:&ref objCType:@encode(CdPersonRef)],
        @"position_pixels" : @[ @(nearest->position.x), @(nearest->position.y) ]
      };
      kind = @"person";
    }
  }
  if (!pick) {
    kind = @"building";
    for (NSUInteger i = 0; i < self.footprints.count; i++)
      if ([self.footprints[i] containsPoint:p]) {
        pick = self.buildings[i];
        break;
      }
  }
  self.selected = pick;
  self.needsDisplay = YES;
  if (self.selection)
    self.selection(pick, kind);
}
- (void)scrollWheel:(NSEvent *)e {
  if (e.modifierFlags & NSEventModifierFlagOption)
    [self zoomBy:exp(-e.scrollingDeltaY * .025)];
  else
    [super scrollWheel:e];
}
- (void)magnifyWithEvent:(NSEvent *)e {
  [self zoomBy:1 + e.magnification];
}
- (void)keyDown:(NSEvent *)e {
  NSString *key = e.charactersIgnoringModifiers.lowercaseString;
  if ([key isEqual:@"+"] || [key isEqual:@"="])
    [self zoomBy:1.25];
  else if ([key isEqual:@"-"])
    [self zoomBy:.8];
  else if ([key isEqual:@"f"])
    [self fitMap];
  else if ([key isEqual:@" "]) {
    if (self.toggleRun)
      self.toggleRun();
  } else {
    unichar c = key.length ? [key characterAtIndex:0] : 0;
    NSPoint p = self.enclosingScrollView.contentView.bounds.origin;
    CGFloat d = 60 / self.enclosingScrollView.magnification;
    if (c == NSLeftArrowFunctionKey)
      p.x -= d;
    else if (c == NSRightArrowFunctionKey)
      p.x += d;
    else if (c == NSUpArrowFunctionKey)
      p.y -= d;
    else if (c == NSDownArrowFunctionKey)
      p.y += d;
    else {
      [super keyDown:e];
      return;
    }
    [self.enclosingScrollView.contentView scrollToPoint:p];
    [self.enclosingScrollView
        reflectScrolledClipView:self.enclosingScrollView.contentView];
  }
}
@end

@interface CdPopulationView ()
@property(nonatomic) CdPersonId selectedPerson;
@end
@implementation CdPopulationView
- (BOOL)acceptsFirstResponder {
  return YES;
}
- (BOOL)isFlipped {
  return YES;
}
- (BOOL)isAccessibilityElement {
  return YES;
}
- (NSString *)accessibilityRole {
  return NSAccessibilityImageRole;
}
- (NSString *)accessibilityLabel {
  return @"All agents roster; plus means located, cross means unplaced; not "
         @"geographic locations. Arrow keys select, Return inspects.";
}
- (NSString *)accessibilityValue {
  const CdAgentGlyph *p = self.agentGlyphs.bytes;
  size_t n = self.agentGlyphs.length / sizeof *p, located = 0;
  for (size_t i = 0; i < n; i++)
    located += p[i].located != 0;
  NSString *selection = @"none";
  for (size_t i = 0; i < n; i++)
    if (p[i].ref.person_id == self.selectedPerson)
      selection =
          [NSString stringWithFormat:@"resident %llu, %@",
                                     (unsigned long long)p[i].ref.person_id,
                                     p[i].located ? @"located" : @"unplaced"];
  return [NSString
      stringWithFormat:@"%zu agents, %zu located, %zu unplaced. Selected %@", n,
                       located, n - located, selection];
}
- (void)setAgentGlyphs:(NSData *)data {
  if ([_agentGlyphs isEqualToData:data])
    return;
  _agentGlyphs = [data copy];
  size_t count = data.length / sizeof(CdAgentGlyph);
  self.frame = NSMakeRect(0, 0, 924, MAX(100, 24 + ceil(count / 200.0) * 4.5));
  self.needsDisplay = YES;
}
- (void)drawRect:(NSRect)dirty {
  [[NSColor colorWithCalibratedRed:.97 green:.96 blue:.92 alpha:1] setFill];
  NSRectFill(dirty);
  const CdAgentGlyph *agents = self.agentGlyphs.bytes;
  size_t count = self.agentGlyphs.length / sizeof *agents;
  NSBezierPath *paths[2] = {[NSBezierPath bezierPath],
                            [NSBezierPath bezierPath]};
  NSInteger first = MAX(0, (NSInteger)floor((NSMinY(dirty) - 12) / 4.5)),
            last = MAX(first, (NSInteger)ceil((NSMaxY(dirty) - 12) / 4.5));
  size_t end = MIN(count, (size_t)(last + 1) * 200);
  for (size_t i = (size_t)first * 200; i < end; i++) {
    CGFloat x = 12 + (i % 200 + .5) * 4.5, y = 12 + (i / 200 + .5) * 4.5;
    if (x < NSMinX(dirty) - 2 || x > NSMaxX(dirty) + 2)
      continue;
    NSBezierPath *p = paths[agents[i].located ? 1 : 0];
    if (agents[i].located) {
      [p moveToPoint:NSMakePoint(x - 1.3, y)];
      [p lineToPoint:NSMakePoint(x + 1.3, y)];
      [p moveToPoint:NSMakePoint(x, y - 1.3)];
      [p lineToPoint:NSMakePoint(x, y + 1.3)];
    } else {
      [p moveToPoint:NSMakePoint(x - 1.2, y - 1.2)];
      [p lineToPoint:NSMakePoint(x + 1.2, y + 1.2)];
      [p moveToPoint:NSMakePoint(x + 1.2, y - 1.2)];
      [p lineToPoint:NSMakePoint(x - 1.2, y + 1.2)];
    }
    if (agents[i].ref.person_id == self.selectedPerson) {
      [NSColor.blackColor setStroke];
      NSBezierPath *box = [NSBezierPath
          bezierPathWithRect:NSMakeRect(x - 2.2, y - 2.2, 4.4, 4.4)];
      box.lineWidth = 1;
      [box stroke];
    }
  }
  for (unsigned i = 0; i < 2; i++) {
    [(i ? [NSColor colorWithCalibratedRed:0 green:.42 blue:.40 alpha:1]
        : [NSColor colorWithCalibratedRed:.67 green:.36 blue:.12
                                    alpha:1]) setStroke];
    paths[i].lineWidth = .65;
    [paths[i] stroke];
  }
}
- (void)mouseUp:(NSEvent *)event {
  NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
  if (p.x < 12 || p.x >= 912 || p.y < 12)
    return;
  size_t column = (size_t)((p.x - 12) / 4.5), row = (size_t)((p.y - 12) / 4.5),
         i = row * 200 + column;
  const CdAgentGlyph *agents = self.agentGlyphs.bytes;
  if (i < self.agentGlyphs.length / sizeof *agents) {
    [self.window makeFirstResponder:self];
    self.selectedPerson = agents[i].ref.person_id;
    self.needsDisplay = YES;
    if (self.inspectAgent)
      self.inspectAgent(agents[i].ref);
  }
}
- (void)keyDown:(NSEvent *)event {
  const CdAgentGlyph *agents = self.agentGlyphs.bytes;
  size_t count = self.agentGlyphs.length / sizeof *agents;
  if (!count)
    return;
  size_t current = 0;
  for (size_t i = 0; i < count; i++)
    if (agents[i].ref.person_id == self.selectedPerson) {
      current = i;
      break;
    }
  unichar key = event.charactersIgnoringModifiers.length
                    ? [event.charactersIgnoringModifiers characterAtIndex:0]
                    : 0;
  CdRosterDirection direction;
  if (key == '\r' || key == ' ') {
    self.selectedPerson = agents[current].ref.person_id;
    if (self.inspectAgent)
      self.inspectAgent(agents[current].ref);
    self.needsDisplay = YES;
    return;
  }
  switch (key) {
  case NSLeftArrowFunctionKey:
    direction = CD_ROSTER_LEFT;
    break;
  case NSRightArrowFunctionKey:
    direction = CD_ROSTER_RIGHT;
    break;
  case NSUpArrowFunctionKey:
    direction = CD_ROSTER_UP;
    break;
  case NSDownArrowFunctionKey:
    direction = CD_ROSTER_DOWN;
    break;
  case NSHomeFunctionKey:
    direction = CD_ROSTER_FIRST;
    break;
  case NSEndFunctionKey:
    direction = CD_ROSTER_LAST;
    break;
  default:
    [super keyDown:event];
    return;
  }
  size_t next;
  if (cd_roster_navigate(count, current, direction, &next) != CD_OK)
    return;
  self.selectedPerson = agents[next].ref.person_id;
  [self scrollRectToVisible:NSMakeRect(12 + (next % 200) * 4.5,
                                       12 + (next / 200) * 4.5, 4.5, 4.5)];
  self.needsDisplay = YES;
}

@end
