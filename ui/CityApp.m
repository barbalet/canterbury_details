#import "CityMap.h"
#import "CitySession.h"
#include "cd_bootstrap.h"
#include "cd_city.h"
#include "cd_human.h"
#import <Cocoa/Cocoa.h>
#include <inttypes.h>

static NSString *recordID(uint64_t value) {
  return value ? [NSString stringWithFormat:@"%llu", (unsigned long long)value]
               : @"none";
}
static NSTextField *label(NSString *s, CGFloat size, NSFontWeight weight) {
  NSTextField *v = [NSTextField labelWithString:s];
  v.font = [NSFont systemFontOfSize:size weight:weight];
  return v;
}
static NSTextField *wrap(NSString *s, CGFloat size) {
  NSTextField *v = [NSTextField wrappingLabelWithString:s];
  v.font = [NSFont systemFontOfSize:size];
  return v;
}
static NSButton *button(NSString *s, id target, SEL action) {
  NSButton *v = [NSButton buttonWithTitle:s target:target action:action];
  v.bezelStyle = NSBezelStyleRounded;
  return v;
}
static NSStackView *stack(NSArray *views,
                          NSUserInterfaceLayoutOrientation orientation,
                          CGFloat gap) {
  NSStackView *v = [NSStackView stackViewWithViews:views];
  v.orientation = orientation;
  v.spacing = gap;
  v.alignment = orientation == NSUserInterfaceLayoutOrientationVertical
                    ? NSLayoutAttributeLeading
                    : NSLayoutAttributeCenterY;
  return v;
}
static NSView *spacer(void) {
  NSView *v = [NSView new];
  [v setContentHuggingPriority:1
                forOrientation:NSLayoutConstraintOrientationHorizontal];
  return v;
}
static void pin(NSView *child, NSView *parent, CGFloat inset) {
  child.translatesAutoresizingMaskIntoConstraints = NO;
  [parent addSubview:child];
  [NSLayoutConstraint activateConstraints:@[
    [child.leadingAnchor constraintEqualToAnchor:parent.leadingAnchor
                                        constant:inset],
    [child.trailingAnchor constraintEqualToAnchor:parent.trailingAnchor
                                         constant:-inset],
    [child.topAnchor constraintEqualToAnchor:parent.topAnchor constant:inset],
    [child.bottomAnchor constraintEqualToAnchor:parent.bottomAnchor
                                       constant:-inset]
  ]];
}
static NSDictionary *resource(NSString *name) {
  NSURL *url = [NSBundle.mainBundle URLForResource:name withExtension:@"json"];
  if (!url)
    return nil;
  NSData *data = [NSData dataWithContentsOfURL:url];
  id value = data ? [NSJSONSerialization JSONObjectWithData:data
                                                    options:0
                                                      error:NULL]
                  : nil;
  return [value isKindOfClass:NSDictionary.class] ? value : nil;
}
@interface CdApplication
    : NSObject <NSApplicationDelegate, NSTableViewDataSource,
                NSTableViewDelegate, NSSearchFieldDelegate>
@property(nonatomic, strong) NSWindow *window, *agentsWindow;
@property(nonatomic, strong) CdPopulationView *agentsView;
@property(nonatomic, strong) NSTextField *agentsSummary;
@property(nonatomic, strong) NSScrollView *agentsScroll;
@property(nonatomic, strong) CdMapView *map;
@property(nonatomic, strong) NSScrollView *mapScroll;
@property(nonatomic, strong) NSTextField *clockLabel, *population, *status,
    *zoomLabel, *inspectorTitle;
@property(nonatomic, strong) NSTextView *inspector;
@property(nonatomic, strong) NSButton *runButton;
@property(nonatomic, strong) NSPopUpButton *speed, *mode;
@property(nonatomic, strong) NSSearchField *search;
@property(nonatomic, strong) NSTableView *table;
@property(nonatomic, copy) NSArray<NSDictionary *> *sites, *buildings, *people,
    *rows, *occupations;
@property(nonatomic, strong) NSTimer *timer;
@property(nonatomic, strong) CdCitySession *citySession;
@property(nonatomic, strong) CdCityState *cityState;
@property(nonatomic) BOOL busy, terminating;
@property(nonatomic, strong) NSDictionary *inspected;
@property(nonatomic) BOOL running;
@end
@implementation CdApplication
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;
  self.busy = YES;
  self.citySession = [CdCitySession new];
  NSMutableArray *roles = [NSMutableArray new];
  for (size_t i = 0; i < cd_occupation_count(); i++) {
    CdOccupationView role;
    if (cd_occupation(i, &role, NULL) == CD_OK)
      [roles addObject:@{
        @"id" :
            [NSString stringWithFormat:@"%llu", (unsigned long long)role.id],
        @"name" : [NSString
            stringWithFormat:@"%@%@", [NSString stringWithUTF8String:role.name],
                             role.enabled ? @"" : @" · disabled"],
        @"basis" : [NSString stringWithUTF8String:role.basis],
        @"source" : [NSString stringWithUTF8String:role.source],
        @"output" : [NSString stringWithUTF8String:role.output],
        @"minimum_age" : @(role.minimum_age),
        @"skills" : @(role.skills),
        @"enabled" : @(role.enabled)
      }];
  }
  self.occupations = roles;
  self.sites = resource(@"religious-sites-1940")[@"sites"];
  self.buildings = resource(@"buildings-enriched")[@"objects"];
  NSURL *mapURL = [NSBundle.mainBundle URLForResource:@"CanterburyMap"
                                        withExtension:@"png"];
  NSImage *image =
      mapURL ? [[NSImage alloc] initWithContentsOfURL:mapURL] : nil;
  if (!image || self.sites.count != 33 || self.buildings.count != 3341) {
    [self
        failure:
            @"The map resources are missing or invalid. Rebuild the "
            @"application to render json2png and bundle the Canterbury data."];
    [NSApp terminate:nil];
    return;
  }
  self.window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 1320, 900)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  self.window.title = @"Canterbury Details · 1940";
  self.window.minSize = NSMakeSize(1060, 720);
  self.window.backgroundColor = [NSColor colorWithCalibratedRed:.96
                                                          green:.95
                                                           blue:.91
                                                          alpha:1];
  NSView *content = self.window.contentView;
  NSTextField *title = label(@"Canterbury Details", 24, NSFontWeightSemibold);
  self.clockLabel = label(@"1 JANUARY 1940 · 00:01", 13, NSFontWeightMedium);
  self.clockLabel.textColor = NSColor.secondaryLabelColor;
  NSStackView *heading = stack(@[ title, self.clockLabel ],
                               NSUserInterfaceLayoutOrientationVertical, 3);
  self.runButton = button(@"Run", self, @selector(toggle:));
  self.runButton.bezelColor = [NSColor colorWithCalibratedRed:.08
                                                        green:.43
                                                         blue:.45
                                                        alpha:1];
  self.speed = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
  [self.speed addItemsWithTitles:@[ @"1×", @"5×", @"20×" ]];
  self.speed.accessibilityLabel = @"Simulation speed";
  NSStackView *header = stack(
      @[
        heading, spacer(), self.runButton,
        button(@"Step 1 minute", self, @selector(stepOne:)),
        button(@"Step 10 minutes", self, @selector(stepTen:)), self.speed,
        button(@"Restart 1940", self, @selector(restart:))
      ],
      NSUserInterfaceLayoutOrientationHorizontal, 10);
  header.translatesAutoresizingMaskIntoConstraints = NO;
  [content addSubview:header];
  NSView *sidebar = [NSView new];
  sidebar.translatesAutoresizingMaskIntoConstraints = NO;
  sidebar.wantsLayer = YES;
  sidebar.layer.backgroundColor =
      [NSColor colorWithCalibratedRed:.07 green:.17 blue:.19 alpha:1].CGColor;
  sidebar.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
  [content addSubview:sidebar];
  NSTextField *eyebrow = label(@"CITY OBSERVATORY", 10, NSFontWeightBold);
  eyebrow.textColor = [NSColor colorWithCalibratedRed:.57
                                                green:.76
                                                 blue:.73
                                                alpha:1];
  self.population = label(@"27,000", 36, NSFontWeightSemibold);
  NSTextField *populationCaption = wrap(@"Ordinary human population", 12);
  populationCaption.textColor = NSColor.secondaryLabelColor;
  self.status = wrap(@"Paused · resident records active", 12);
  self.status.accessibilityLabel = @"Simulation status";
  self.mode = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
  [self.mode addItemsWithTitles:@[
    @"Institutions · 33", @"Building candidates · 3,341", @"Residents · 27,000",
    @"Occupation catalogue"
  ]];
  self.mode.target = self;
  self.mode.action = @selector(listChanged:);
  self.mode.accessibilityLabel = @"Browse city records";
  self.search = [NSSearchField new];
  self.search.placeholderString = @"Search institutions";
  self.search.delegate = self;
  self.search.accessibilityLabel = @"Search city records";
  self.table = [NSTableView new];
  NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"name"];
  column.width = 250;
  [self.table addTableColumn:column];
  self.table.headerView = nil;
  self.table.rowHeight = 32;
  self.table.delegate = self;
  self.table.dataSource = self;
  self.table.target = self;
  self.table.action = @selector(recordClicked:);
  self.table.accessibilityLabel = @"City records";
  NSScrollView *list = [NSScrollView new];
  list.documentView = self.table;
  list.hasVerticalScroller = YES;
  list.borderType = NSNoBorder;
  NSButton *sites = [NSButton checkboxWithTitle:@"Institution markers"
                                         target:self
                                         action:@selector(sitesChanged:)];
  sites.state = NSControlStateValueOn;
  NSButton *footprints =
      [NSButton checkboxWithTitle:@"Building footprints"
                           target:self
                           action:@selector(footprintsChanged:)];
  NSButton *agents = [NSButton checkboxWithTitle:@"Agents (+)"
                                          target:self
                                          action:@selector(agentsChanged:)];
  agents.state = NSControlStateValueOn;
  NSButton *allAgents = button(@"All agents…", self, @selector(showAllAgents:));
  NSTextField *scope =
      wrap(@"Names, ages and households are synthetic fixtures. Housing and "
           @"map positions remain unknown until explicitly admitted. Source "
           @"routes and entrances require review. Lists show up to 200 "
           @"matches; search checks every record.",
           11);
  scope.textColor = NSColor.secondaryLabelColor;
  NSStackView *left = stack(
      @[
        eyebrow, self.population, populationCaption, self.status, self.mode,
        self.search, list, sites, footprints,
        stack(@[ agents, allAgents ],
              NSUserInterfaceLayoutOrientationHorizontal, 8),
        scope
      ],
      NSUserInterfaceLayoutOrientationVertical, 12);
  pin(left, sidebar, 20);
  for (NSView *v in @[ self.mode, self.search, list, scope ]) {
    v.translatesAutoresizingMaskIntoConstraints = NO;
    [v.widthAnchor constraintEqualToAnchor:left.widthAnchor].active = YES;
  }
  [list.heightAnchor constraintGreaterThanOrEqualToConstant:150].active = YES;
  [list setContentHuggingPriority:1
                   forOrientation:NSLayoutConstraintOrientationVertical];
  NSView *main = [NSView new];
  main.translatesAutoresizingMaskIntoConstraints = NO;
  [content addSubview:main];
  NSTextField *mapTitle =
      label(@"CANTERBURY & SURROUNDINGS", 11, NSFontWeightSemibold);
  self.zoomLabel = label(@"100%", 11, NSFontWeightRegular);
  self.zoomLabel.textColor = NSColor.secondaryLabelColor;
  NSButton *minus = button(@"−", self, @selector(zoomOut:));
  minus.accessibilityLabel = @"Zoom out";
  NSButton *plus = button(@"+", self, @selector(zoomIn:));
  plus.accessibilityLabel = @"Zoom in";
  NSStackView *mapToolbar = stack(
      @[
        mapTitle, spacer(), self.zoomLabel, minus, plus,
        button(@"Fit map", self, @selector(fit:)),
        button(@"City centre", self, @selector(city:))
      ],
      NSUserInterfaceLayoutOrientationHorizontal, 8);
  self.mapScroll = [NSScrollView new];
  self.mapScroll.hasHorizontalScroller = YES;
  self.mapScroll.hasVerticalScroller = YES;
  self.mapScroll.allowsMagnification = YES;
  self.mapScroll.minMagnification = .15;
  self.mapScroll.maxMagnification = 6;
  self.mapScroll.backgroundColor = [NSColor colorWithCalibratedRed:.93
                                                             green:.90
                                                              blue:.82
                                                             alpha:1];
  self.mapScroll.contentView = [CdCenteredClipView new];
  self.mapScroll.contentView.backgroundColor = self.mapScroll.backgroundColor;
  self.mapScroll.contentView.drawsBackground = YES;
  self.map = [[CdMapView alloc] initWithFrame:NSMakeRect(0, 0, 1600, 1600)];
  self.map.basemap = image;
  self.map.sites = self.sites;
  self.map.buildings = self.buildings;
  self.map.showSites = YES;
  self.map.showAgents = YES;
  NSMutableArray *paths = [NSMutableArray new];
  for (NSDictionary *building in self.buildings) {
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
  self.map.footprints = paths;
  self.mapScroll.documentView = self.map;
  __weak CdApplication *weakSelf = self;
  self.map.selection = ^(NSDictionary *item, NSString *kind) {
    [weakSelf selectRecord:item kind:kind];
  };
  self.map.toggleRun = ^{
    [weakSelf toggle:nil];
  };
  NSTextField *legend =
      wrap(@"json2png reference · red/orange roads · blue water · green "
           @"woodland · grey orchards\nDrag/scroll to pan · pinch or "
           @"Option-scroll to zoom · click to inspect",
           11);
  legend.textColor = NSColor.secondaryLabelColor;
  self.inspectorTitle = label(@"Explore Canterbury", 17, NSFontWeightSemibold);
  self.inspector = [NSTextView new];
  self.inspector.editable = NO;
  self.inspector.selectable = YES;
  self.inspector.drawsBackground = NO;
  self.inspector.font = [NSFont systemFontOfSize:12];
  self.inspector.textContainerInset = NSMakeSize(0, 4);
  NSScrollView *detail = [NSScrollView new];
  detail.documentView = self.inspector;
  detail.hasVerticalScroller = YES;
  detail.drawsBackground = NO;
  detail.borderType = NSNoBorder;
  NSStackView *right = stack(
      @[ mapToolbar, self.mapScroll, legend, self.inspectorTitle, detail ],
      NSUserInterfaceLayoutOrientationVertical, 10);
  pin(right, main, 18);
  for (NSView *v in @[ mapToolbar, self.mapScroll, legend, detail ]) {
    v.translatesAutoresizingMaskIntoConstraints = NO;
    [v.widthAnchor constraintEqualToAnchor:right.widthAnchor].active = YES;
  }
  [self.mapScroll.heightAnchor constraintGreaterThanOrEqualToConstant:350]
      .active = YES;
  [detail.heightAnchor constraintEqualToConstant:128].active = YES;
  [self.mapScroll
      setContentHuggingPriority:1
                 forOrientation:NSLayoutConstraintOrientationVertical];
  [NSLayoutConstraint activateConstraints:@[
    [header.leadingAnchor constraintEqualToAnchor:content.leadingAnchor
                                         constant:24],
    [header.trailingAnchor constraintEqualToAnchor:content.trailingAnchor
                                          constant:-20],
    [header.topAnchor constraintEqualToAnchor:content.topAnchor constant:14],
    [header.heightAnchor constraintEqualToConstant:56],
    [sidebar.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
    [sidebar.topAnchor constraintEqualToAnchor:header.bottomAnchor constant:14],
    [sidebar.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
    [sidebar.widthAnchor constraintEqualToConstant:310],
    [main.leadingAnchor constraintEqualToAnchor:sidebar.trailingAnchor],
    [main.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
    [main.topAnchor constraintEqualToAnchor:sidebar.topAnchor],
    [main.bottomAnchor constraintEqualToAnchor:content.bottomAnchor]
  ]];
  [self installMenus];
  [self listChanged:nil];
  [self inspect:nil kind:nil];
  [self refresh];
  [self.window center];
  [self.window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
  [content layoutSubtreeIfNeeded];
  [self.map fitMap];
  [self.window makeFirstResponder:self.map];
  NSURL *pack = [NSBundle.mainBundle URLForResource:@"CanterburyCity"
                                      withExtension:@"bin"];
  if (!pack) {
    [self failure:@"Runtime city map is missing."];
    [NSApp terminate:nil];
    return;
  }
  [self.citySession open:pack.path
              completion:^(CdCityState *state, NSString *error) {
                self.busy = NO;
                if (!state) {
                  [self failure:error];
                  [NSApp terminate:nil];
                  return;
                }
                self.cityState = state;
                self.people = state.people;
                [self filter];
                [self refresh];
              }];
  self.timer = [NSTimer timerWithTimeInterval:1
                                       target:self
                                     selector:@selector(tick:)
                                     userInfo:nil
                                      repeats:YES];
  [[NSRunLoop mainRunLoop] addTimer:self.timer forMode:NSRunLoopCommonModes];
}
- (void)installMenus {
  NSMenu *main = [NSMenu new];
  NSMenuItem *application = [NSMenuItem new];
  [main addItem:application];
  NSMenu *app = [NSMenu new];
  application.submenu = app;
  [app addItemWithTitle:@"Quit Canterbury Details"
                 action:@selector(terminate:)
          keyEquivalent:@"q"];
  NSMenuItem *view = [NSMenuItem new];
  view.title = @"View";
  NSMenu *menu = [[NSMenu alloc] initWithTitle:@"View"];
  view.submenu = menu;
  [main addItem:view];
  for (NSArray *entry in @[
         @[ @"Fit map", @"fit:", @"0" ], @[ @"City centre", @"city:", @"1" ],
         @[ @"Zoom in", @"zoomIn:", @"+" ], @[ @"Zoom out", @"zoomOut:", @"-" ]
       ]) {
    NSMenuItem *item = [menu addItemWithTitle:entry[0]
                                       action:NSSelectorFromString(entry[1])
                                keyEquivalent:entry[2]];
    item.target = self;
  }
  NSApp.mainMenu = main;
}
- (void)failure:(NSString *)message {
  NSAlert *a = [NSAlert new];
  a.messageText = @"Canterbury Details";
  a.informativeText = message;
  [a runModal];
}
- (void)advance:(unsigned)minutes {
  if (self.busy || !self.cityState)
    return;
  self.busy = YES;
  [self.citySession step:minutes
              completion:^(CdCityState *state, NSString *error) {
                self.busy = NO;
                if (!state) {
                  self.running = NO;
                  [self failure:error];
                } else
                  self.cityState = state;
                [self refresh];
              }];
}
- (void)toggle:(id)sender {
  (void)sender;
  if (!self.cityState)
    return;
  self.running = !self.running;
  [self refresh];
}
- (void)stepOne:(id)sender {
  (void)sender;
  self.running = NO;
  [self advance:1];
}
- (void)stepTen:(id)sender {
  (void)sender;
  self.running = NO;
  [self advance:10];
}
- (void)tick:(NSTimer *)timer {
  (void)timer;
  self.zoomLabel.stringValue =
      [NSString stringWithFormat:@"%.0f%%", self.mapScroll.magnification * 100];
  if (self.running)
    [self advance:(unsigned[]){1, 5, 20}[self.speed.indexOfSelectedItem]];
}
- (void)refresh {
  self.runButton.title = self.running ? @"Pause" : @"Run";
  if (!self.cityState) {
    self.status.stringValue = @"Loading city on simulation worker…";
    return;
  }
  CdCitySummary r = self.cityState.summary;
  CdCivicSummary civic = self.cityState.civic;
  self.status.toolTip = [NSString
      stringWithFormat:
          @"Configured offices %zu · gatherings %zu · rosters %zu · requests "
          @"%zu · vehicles %zu · pressures %zu\nRain %u · alert %u · blackout "
          @"%u · ration limit %u. No historical staffing or event is inferred.",
          civic.offices, civic.gatherings, civic.rosters, civic.requests,
          civic.vehicles, civic.pressures, civic.rain, civic.alert,
          civic.blackout, civic.ration_limit];
  self.map.agentGlyphs = self.cityState.agentGlyphs;
  self.map.needsDisplay = YES;
  if (self.agentsView) {
    self.agentsView.agentGlyphs = self.cityState.agentGlyphs;
    self.agentsSummary.stringValue = [NSString
        stringWithFormat:
            @"%zu agents · %zu located · %zu unplaced · native minute %llu",
            r.people, r.located, r.people - r.located,
            (unsigned long long)r.native_minutes];
  }
  CdCivilTime t = self.cityState.clock.civil;
  self.clockLabel.stringValue = [NSString
      stringWithFormat:@"%u %@ %04d  ·  %02u:%02u", t.day,
                       @[
                         @"JANUARY", @"FEBRUARY", @"MARCH", @"APRIL", @"MAY",
                         @"JUNE", @"JULY", @"AUGUST", @"SEPTEMBER", @"OCTOBER",
                         @"NOVEMBER", @"DECEMBER"
                       ][t.month - 1],
                       t.year, t.minute_of_day / 60, t.minute_of_day % 60];
  self.population.stringValue = [NSNumberFormatter
      localizedStringFromNumber:@(r.people)
                    numberStyle:NSNumberFormatterDecimalStyle];
  self.status.stringValue = [NSString
      stringWithFormat:@"%@ · minute %llu\n%zu households · capacity "
                       @"%zu\n%zu located · %zu unplaced",
                       self.running ? @"Running" : @"Paused",
                       (unsigned long long)r.native_minutes, r.households,
                       r.capacity, r.located, r.people - r.located];
}
- (void)restart:(id)sender {
  (void)sender;
  if (self.busy || !self.cityState)
    return;
  self.running = NO;
  self.busy = YES;
  [self.citySession restart:^(CdCityState *state, NSString *error) {
    self.busy = NO;
    if (!state)
      [self failure:error];
    else {
      self.cityState = state;
      self.people = state.people;
      self.map.selected = nil;
      [self listChanged:nil];
      [self inspect:nil kind:nil];
    }
    [self refresh];
  }];
}
- (void)zoomIn:(id)sender {
  (void)sender;
  [self.map zoomBy:1.25];
}
- (void)zoomOut:(id)sender {
  (void)sender;
  [self.map zoomBy:.8];
}
- (void)fit:(id)sender {
  (void)sender;
  [self.map fitMap];
}
- (void)city:(id)sender {
  (void)sender;
  [self.map centerOn:NSMakePoint(830, 810)];
}
- (void)agentsChanged:(NSButton *)sender {
  self.map.showAgents = sender.state == NSControlStateValueOn;
  self.map.needsDisplay = YES;
}
- (void)showAllAgents:(id)sender {
  (void)sender;
  if (!self.cityState)
    return;
  if (!self.agentsWindow) {
    self.agentsWindow =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 980, 790)
                                    styleMask:NSWindowStyleMaskTitled |
                                              NSWindowStyleMaskClosable |
                                              NSWindowStyleMaskResizable
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    self.agentsWindow.releasedWhenClosed = NO;
    self.agentsWindow.title = @"All agents · population overview";
    self.agentsWindow.minSize = NSMakeSize(700, 500);
    NSTextField *title =
        label(@"Every agent, one + each", 21, NSFontWeightSemibold);
    self.agentsSummary = label(@"", 13, NSFontWeightMedium);
    NSTextField *legend = wrap(
        @"Roster order, NOT city locations. Amber +: unplaced. Teal +: "
        @"located. Click a + to inspect; use zoom to separate small marks.",
        12);
    self.agentsScroll = [NSScrollView new];
    self.agentsScroll.hasHorizontalScroller = YES;
    self.agentsScroll.hasVerticalScroller = YES;
    self.agentsScroll.allowsMagnification = YES;
    self.agentsScroll.minMagnification = .3;
    self.agentsScroll.maxMagnification = 8;
    self.agentsView =
        [[CdPopulationView alloc] initWithFrame:NSMakeRect(0, 0, 924, 650)];
    self.agentsScroll.documentView = self.agentsView;
    __weak CdApplication *weakSelf = self;
    self.agentsView.inspectAgent = ^(CdPersonRef ref) {
      CdApplication *owner = weakSelf;
      if (!owner)
        return;
      [owner selectRecord:@{
        @"name" : [NSString
            stringWithFormat:@"Agent #%llu", (unsigned long long)ref.person_id],
        @"ref" : [NSValue valueWithBytes:&ref objCType:@encode(CdPersonRef)]
      }
                     kind:@"person"];
      [owner.window makeKeyAndOrderFront:nil];
    };
    NSStackView *toolbar = stack(
        @[
          title, spacer(), button(@"−", self, @selector(agentsZoomOut:)),
          button(@"+", self, @selector(agentsZoomIn:)),
          button(@"Fit all", self, @selector(agentsFit:))
        ],
        NSUserInterfaceLayoutOrientationHorizontal, 8);
    NSStackView *layout =
        stack(@[ toolbar, self.agentsSummary, legend, self.agentsScroll ],
              NSUserInterfaceLayoutOrientationVertical, 10);
    pin(layout, self.agentsWindow.contentView, 18);
    for (NSView *view in @[ toolbar, legend, self.agentsScroll ]) {
      view.translatesAutoresizingMaskIntoConstraints = NO;
      [view.widthAnchor constraintEqualToAnchor:layout.widthAnchor].active =
          YES;
    }
    [self.agentsScroll.heightAnchor constraintGreaterThanOrEqualToConstant:300]
        .active = YES;
    [self.agentsScroll
        setContentHuggingPriority:1
                   forOrientation:NSLayoutConstraintOrientationVertical];
    [self.agentsWindow center];
  }
  [self refresh];
  [self.agentsWindow makeKeyAndOrderFront:nil];
  [self.agentsWindow.contentView layoutSubtreeIfNeeded];
  [self agentsFit:nil];
}
- (void)agentsFit:(id)sender {
  (void)sender;
  NSSize size = self.agentsView.bounds.size,
         visible = self.agentsScroll.contentSize;
  self.agentsScroll.magnification =
      MAX(.3, MIN(visible.width / size.width, visible.height / size.height));
}
- (void)agentsZoomIn:(id)sender {
  (void)sender;
  self.agentsScroll.magnification =
      MIN(8, self.agentsScroll.magnification * 1.5);
}
- (void)agentsZoomOut:(id)sender {
  (void)sender;
  self.agentsScroll.magnification =
      MAX(.3, self.agentsScroll.magnification / 1.5);
}
- (void)sitesChanged:(NSButton *)sender {
  self.map.showSites = sender.state == NSControlStateValueOn;
  self.map.needsDisplay = YES;
}
- (void)footprintsChanged:(NSButton *)sender {
  self.map.showFootprints = sender.state == NSControlStateValueOn;
  self.map.needsDisplay = YES;
}
- (void)listChanged:(id)sender {
  (void)sender;
  self.search.stringValue = @"";
  self.search.placeholderString = @[
    @"Search institutions", @"Search modern candidate labels",
    @"Search fixture name or resident ID", @"Search role definitions"
  ][self.mode.indexOfSelectedItem];
  [self filter];
}
- (void)controlTextDidChange:(NSNotification *)notification {
  (void)notification;
  [self filter];
}
- (void)filter {
  NSArray *source = self.sites;
  if (self.mode.indexOfSelectedItem == 1)
    source = self.buildings;
  else if (self.mode.indexOfSelectedItem == 2) {
    source = self.people;
  } else if (self.mode.indexOfSelectedItem == 3)
    source = self.occupations;
  NSString *query = self.search.stringValue;
  self.rows =
      query.length
          ? [source
                filteredArrayUsingPredicate:
                    [NSPredicate predicateWithBlock:^BOOL(
                                     NSDictionary *item,
                                     NSDictionary *bindings) {
                      (void)bindings;
                      return
                          [item[@"name"] ?: @""
                              localizedCaseInsensitiveContainsString:query] ||
                          [item[@"id"]
                              localizedCaseInsensitiveContainsString:query];
                    }]]
          : source;
  [self.table reloadData];
}
- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
  (void)tableView;
  return MIN((NSUInteger)200, self.rows.count);
}
- (NSView *)tableView:(NSTableView *)tableView
    viewForTableColumn:(NSTableColumn *)tableColumn
                   row:(NSInteger)row {
  (void)tableColumn;
  NSTextField *view = [tableView makeViewWithIdentifier:@"record" owner:self];
  if (!view) {
    view = label(@"", 12, NSFontWeightRegular);
    view.identifier = @"record";
    view.lineBreakMode = NSLineBreakByTruncatingTail;
  }
  NSDictionary *item = self.rows[row];
  NSString *name = item[@"name"];
  view.stringValue =
      name.length && ![name isEqual:@"unknown"]
          ? name
          : [NSString
                stringWithFormat:@"Building %@",
                                 [item[@"id"]
                                     substringFromIndex:MIN((NSUInteger)10,
                                                            [item[@"id"]
                                                                length])]];
  return view;
}
- (void)recordClicked:(id)sender {
  (void)sender;
  NSInteger row = self.table.clickedRow;
  if (row >= 0 && row < (NSInteger)self.rows.count)
    [self
        selectRecord:self.rows[row]
                kind:@[ @"institution", @"building", @"person",
                        @"occupation" ][self.mode.indexOfSelectedItem]];
}
- (void)tableViewSelectionDidChange:(NSNotification *)notification {
  (void)notification;
  NSInteger row = self.table.selectedRow;
  if (row < 0 || row >= (NSInteger)self.rows.count)
    return;
  [self selectRecord:self.rows[row]
                kind:@[ @"institution", @"building", @"person",
                        @"occupation" ][self.mode.indexOfSelectedItem]];
}
- (void)selectRecord:(NSDictionary *)item kind:(NSString *)kind {
  [self inspect:item kind:kind];
  self.map.selected =
      ([kind isEqual:@"person"] || [kind isEqual:@"occupation"]) ? nil : item;
  NSArray *xy = item[@"position_pixels"];
  if ([xy isKindOfClass:NSArray.class]) {
    [self.map centerOn:NSMakePoint([xy[0] doubleValue], [xy[1] doubleValue])];
  } else if (item[@"bounds"]) {
    NSArray *b = item[@"bounds"];
    [self.map
        centerOn:NSMakePoint(([b[0] doubleValue] + [b[2] doubleValue]) / 2,
                             ([b[1] doubleValue] + [b[3] doubleValue]) / 2)];
  }
  self.map.needsDisplay = YES;
}
- (void)inspect:(NSDictionary *)item kind:(NSString *)kind {
  self.inspected = item;
  if (!item) {
    self.inspectorTitle.stringValue = @"Explore Canterbury";
    self.inspector.string =
        @"The map is rendered directly from Canterbury vector JSON by the "
        @"upstream json2png.c core, with source colors, labels and grey "
        @"orchard fields.\nSelect a marker, footprint or record to inspect it. "
        @"Locations and modern use labels are candidates, not verified 1940 "
        @"identities. © OpenStreetMap contributors (ODbL-1.0) applies to "
        @"modern annotations.";
    return;
  }
  NSString *name = item[@"name"];
  self.inspectorTitle.stringValue =
      name.length && ![name isEqual:@"unknown"] ? name : @"Building candidate";
  if ([kind isEqual:@"institution"]) {
    BOOL located = [item[@"position_pixels"] isKindOfClass:NSArray.class];
    self.inspector.string = [NSString
        stringWithFormat:
            @"%@ · %@ · %@\nLocation: %@ · 1940 worship status: %@\n%@",
            item[@"religion"], item[@"denomination"], item[@"site_type"],
            located ? @"approximate; review required"
                    : @"unlocated — no marker assigned",
            item[@"worship_status_1940"],
            item[@"notes"] ?: @"Historical identity remains unverified."];
    NSString *sourceDescription = [self.inspector.string copy];
    [self.citySession
        institution:item[@"id"]
         completion:^(NSDictionary *details) {
           if (self.inspected != item || !details)
             return;
           self.inspector.string = [sourceDescription
               stringByAppendingFormat:
                   @"\nCurrent operations: %@\nLinked premises: %@ · office: "
                   @"%@\nCongregation and precinct boundary: unknown. Property "
                   @"ownership is separate.\nSource record (dates and "
                   @"provenance): %@",
                   details[@"operation"], details[@"premises"],
                   [details[@"office"] length] ? details[@"office"]
                                               : @"unknown",
                   details[@"source"]];
         }];
  } else if ([kind isEqual:@"occupation"]) {
    NSMutableArray *skills = [NSMutableArray new];
    unsigned mask = [item[@"skills"] unsignedIntValue];
    for (unsigned bit = 0; bit < 4; bit++)
      if (mask & (1u << bit))
        [skills addObject:@[ @"food", @"craft", @"care", @"community" ][bit]];
    self.inspector.string = [NSString
        stringWithFormat:
            @"%@ · minimum age %@ · qualifications %@\nSource: %@\nOutput: "
            @"%@\n%@\nRole definition only. No employer, vacancy, workplace or "
            @"religious affiliation assigned.",
            [item[@"enabled"] boolValue] ? @"Enabled technical role"
                                         : @"Disabled upstream specialization",
            item[@"minimum_age"],
            skills.count ? [skills componentsJoinedByString:@", "] : @"none",
            item[@"source"], item[@"output"], item[@"basis"]];
  } else if ([kind isEqual:@"person"]) {
    CdPersonRef ref;
    [item[@"ref"] getValue:&ref size:sizeof ref];
    [self.citySession
          resident:ref
        completion:^(CdResidentView resident, CdStatus status) {
          if (self.inspected != item)
            return;
          if (status == CD_OK) {
            self.inspector.string = [NSString
                stringWithFormat:
                    @"Resident #%llu · synthetic fixture · age %u · born "
                    @"%04d-%02u-%02u\nHousehold %llu · housing %@ · religious "
                    @"affiliation unknown\n%@ · %@",
                    (unsigned long long)resident.ref.person_id, resident.age,
                    resident.birth.year, resident.birth.month,
                    resident.birth.day, (unsigned long long)resident.household,
                    resident.housing_known
                        ? (resident.dwelling ? @"assigned"
                                             : @"explicitly unhoused")
                        : @"unknown",
                    [NSString stringWithUTF8String:resident.admission_basis],
                    resident.position_known ? @"Native map position assigned"
                                            : @"Map position unplaced; no "
                                              @"historical location inferred"];
            self.inspector.string=[self.inspector.string stringByAppendingFormat:@"\nNative energy %d · fatigue %u · %@ · meals carried %u · health severity %u",resident.energy,resident.fatigue,resident.sleeping?@"sleeping":@[@"idle",@"travelling",@"working",@"eating",@"resting",@"care",@"waiting",@"retired"][(unsigned)resident.activity],resident.carried_meals,resident.health_severity];
            self.inspector.string = [self.inspector.string
                stringByAppendingFormat:
                    @"\nPrimary job %llu · secondary job %llu · workplace "
                    @"%llu\nWork %u/%u native minutes · model-token balance "
                    @"%llu",
                    (unsigned long long)resident.jobs[0],
                    (unsigned long long)resident.jobs[1],
                    (unsigned long long)resident.workplace,
                    resident.work_progress, resident.work_duration,
                    (unsigned long long)resident.wallet];
            NSMutableArray *permissions = [NSMutableArray new];
            for (unsigned bit = 0; bit < 6; bit++)
              if (resident.credentials & (1u << bit))
                [permissions addObject:@[
                  @"teaching", @"administration", @"policing", @"fire response",
                  @"carriage", @"burial administration"
                ][bit]];
            self.inspector.string = [self.inspector.string
                stringByAppendingFormat:
                    @"\nService request %@ · gathering %@\nPassenger vehicle "
                    @"%@ · crew vehicle %@ · shelter %@\nExplicit service "
                    @"permissions: %@",
                    recordID(resident.service_request),
                    recordID(resident.gathering), recordID(resident.vehicle),
                    recordID(resident.driving), recordID(resident.refuge),
                    permissions.count
                        ? [permissions componentsJoinedByString:@", "]
                        : @"none"];
            if (resident.position_known)
              [self.map centerOn:NSMakePoint(resident.position.x,
                                             resident.position.y)];
          } else
            self.inspector.string = @"Resident reference is no longer active.";
        }];
  } else {
    self.inspector.string = [NSString
        stringWithFormat:
            @"%@\nModern suggested use: %@ · historical identity "
            @"unverified\nFootprint area: %@ source pixels · %@\nNames and "
            @"uses are approximate modern OSM annotations. Doors, occupancy "
            @"and access have not yet been established.",
            item[@"id"], item[@"use"] ?: @"unknown", item[@"area_pixels"],
            item[@"status"]];
  }
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app {
  (void)app;
  return YES;
}
- (NSApplicationTerminateReply)applicationShouldTerminate:
    (NSApplication *)sender {
  (void)sender;
  if (self.terminating)
    return NSTerminateLater;
  self.terminating = YES;
  self.running = NO;
  [self.timer invalidate];
  [self.citySession close:^{
    [NSApp replyToApplicationShouldTerminate:YES];
  }];
  return NSTerminateLater;
}

@end
int main(void) {
  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];
    CdApplication *delegate = [CdApplication new];
    app.delegate = delegate;
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [app run];
  }
  return 0;
}
