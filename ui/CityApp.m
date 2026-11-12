#import "CityMap.h"
#import "CitySession.h"
#import "CityManagement.h"
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
    *zoomLabel, *inspectorTitle, *agentLocation;
@property(nonatomic, strong) NSTextView *inspector;
@property(nonatomic, strong) NSButton *runButton;
@property(nonatomic, strong) NSPopUpButton *speed, *mode;
@property(nonatomic, strong) NSSearchField *search;
@property(nonatomic, strong) NSTableView *table;
@property(nonatomic, copy) NSArray<NSDictionary *> *sites, *buildings, *people,
    *rows, *occupations;
@property(nonatomic, strong) NSTimer *timer;
@property(nonatomic, strong) CdCitySession *citySession;
@property(nonatomic, strong) CdManagementController *management;
@property(nonatomic) CGFloat textScale;
@property(nonatomic, strong) CdCityState *cityState;
@property(nonatomic) BOOL busy, terminating;
@property(nonatomic, strong) NSDictionary *inspected;
@property(nonatomic, copy) NSString *inspectedKind;
@property(nonatomic) BOOL running;
@end
@implementation CdApplication
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;
  self.busy = YES;
  self.textScale = 1;
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
  NSURL *mapURL = [NSBundle.mainBundle URLForResource:@"CanterburyMapBackground"
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
  self.window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
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
  self.mapScroll.documentView = self.map;
  __weak CdApplication *weakSelf = self;
  self.map.selection = ^(NSDictionary *item, NSString *kind) {
    [weakSelf selectRecord:item kind:kind];
  };
  self.map.toggleRun = ^{
    [weakSelf toggle:nil];
  };
  NSTextField *legend =
      wrap(@"Final assembled map · connected OSM roads · blue water · dark "
           @"buildings · reference rail\nDrag/scroll to pan · pinch or "
           @"Option-scroll to zoom · click to inspect",
           11);
  legend.textColor = NSColor.secondaryLabelColor;
  self.inspectorTitle = label(@"Explore Canterbury", 17, NSFontWeightSemibold);
  self.agentLocation = label(@"", 12, NSFontWeightMedium);
  self.agentLocation.lineBreakMode = NSLineBreakByWordWrapping;
  self.agentLocation.maximumNumberOfLines = 2;
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
      @[
        mapToolbar, self.mapScroll, legend, self.inspectorTitle,
        self.agentLocation, detail
      ],
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
  NSMenuItem *edit = [NSMenuItem new];
  edit.title = @"Edit";
  NSMenu *editing = [[NSMenu alloc] initWithTitle:@"Edit"];
  edit.submenu = editing;
  [main addItem:edit];
  for (NSArray *entry in @[
         @[ @"Undo", @"undo:", @"z" ], @[ @"Cut", @"cut:", @"x" ],
         @[ @"Copy", @"copy:", @"c" ], @[ @"Paste", @"paste:", @"v" ],
         @[ @"Select All", @"selectAll:", @"a" ]
       ])
    [editing addItemWithTitle:entry[0]
                       action:NSSelectorFromString(entry[1])
                keyEquivalent:entry[2]];
  NSMenuItem *view = [NSMenuItem new];
  view.title = @"View";
  NSMenu *menu = [[NSMenu alloc] initWithTitle:@"View"];
  view.submenu = menu;
  [main addItem:view];
  for (NSArray *entry in @[
         @[ @"Fit map", @"fit:", @"0" ], @[ @"City centre", @"city:", @"1" ],
         @[ @"Zoom in", @"zoomIn:", @"+" ], @[ @"Zoom out", @"zoomOut:", @"-" ],
         @[ @"Larger text", @"largerText:", @"]" ],
         @[ @"Smaller text", @"smallerText:", @"[" ]
       ]) {
    NSMenuItem *item = [menu addItemWithTitle:entry[0]
                                       action:NSSelectorFromString(entry[1])
                                keyEquivalent:entry[2]];
    item.target = self;
  }
  NSMenuItem *simulation = [NSMenuItem new];
  simulation.title = @"Simulation";
  NSMenu *controls = [[NSMenu alloc] initWithTitle:@"Simulation"];
  simulation.submenu = controls;
  [main addItem:simulation];
  for (NSArray *entry in @[
         @[ @"Save checkpoint…", @"saveCheckpoint:", @"s" ],
         @[ @"Load checkpoint…", @"loadCheckpoint:", @"o" ],
         @[ @"Open scenario startup…", @"openScenarioStartup:", @"" ],
         @[ @"City management…", @"showManagement:", @"m" ],
         @[ @"Scenario operation…", @"scenarioOperation:", @"i" ],
         @[ @"Export causal records…", @"exportRecords:", @"e" ]
       ]) {
    NSMenuItem *item = [controls addItemWithTitle:entry[0]
                                           action:NSSelectorFromString(entry[1])
                                    keyEquivalent:entry[2]];
    item.target = self;
  }
  NSApp.mainMenu = main;
}
- (void)largerText:(id)sender {
  (void)sender;
  self.textScale = MIN(1.5, self.textScale + .1);
  [self applyTextScale];
}
- (void)smallerText:(id)sender {
  (void)sender;
  self.textScale = MAX(1, self.textScale - .1);
  [self applyTextScale];
}
- (void)applyTextScale {
  self.inspector.font = [NSFont systemFontOfSize:12 * self.textScale];
  self.agentLocation.font = [NSFont systemFontOfSize:12 * self.textScale];
  self.table.rowHeight = 32 * self.textScale;
  [self.table reloadData];
}
- (void)showManagement:(id)sender {
  (void)sender;
  if (!self.cityState)
    return;
  if (!self.management) {
    self.management = [CdManagementController new];
    self.management.session = self.citySession;
    __weak CdApplication *weakSelf = self;
    self.management.beginMutation = ^BOOL {
      CdApplication *owner = weakSelf;
      if (!owner || owner.busy)
        return NO;
      owner.running = NO;
      owner.busy = YES;
      [owner refresh];
      return YES;
    };
    self.management.publish = ^(CdCityState *state, NSString *error) {
      CdApplication *owner = weakSelf;
      owner.busy = NO;
      if (state)
        owner.cityState = state;
      else if (error)
        [owner failure:error];
      [owner refresh];
    };
    self.management.cityAction = ^(NSString *action) {
      CdApplication *owner = weakSelf;
      if ([action isEqual:@"run"])
        [owner toggle:nil];
      else if ([action isEqual:@"step"])
        [owner stepOne:nil];
      else if ([action isEqual:@"save"])
        [owner saveCheckpoint:nil];
      else if ([action isEqual:@"load"])
        [owner loadCheckpoint:nil];
    };
  }
  [self.management showWindow:nil];
  [self.management.window makeKeyAndOrderFront:nil];
}
- (void)exportRecords:(id)sender {
  (void)sender;
  if (self.busy || !self.cityState)
    return;
  self.running = NO;
  NSSavePanel *panel = [NSSavePanel savePanel];
  panel.nameFieldStringValue = @"Canterbury-records.json";
  NSButton *scope = [NSButton
      checkboxWithTitle:@"Diagnostic scope: include private/household records"
                 target:nil
                 action:nil];
  panel.accessoryView = scope;
  [panel beginSheetModalForWindow:self.window
                completionHandler:^(NSModalResponse response) {
                  if (response != NSModalResponseOK)
                    return;
                  self.busy = YES;
                  [self.citySession
                      exportRecords:panel.URL.path
                         diagnostic:scope.state == NSControlStateValueOn
                         completion:^(NSString *message) {
                           self.busy = NO;
                           [self refresh];
                           [self failure:message];
                         }];
                }];
}
- (void)saveCheckpoint:(id)sender {
  (void)sender;
  [self checkpoint:NO];
}
- (void)loadCheckpoint:(id)sender {
  (void)sender;
  [self checkpoint:YES];
}
- (void)checkpoint:(BOOL)load {
  if (self.busy || !self.cityState)
    return;
  self.running = NO;
  NSSavePanel *panel =
      load ? (NSSavePanel *)[NSOpenPanel openPanel] : [NSSavePanel savePanel];
  if (!load)
    panel.nameFieldStringValue = @"Canterbury.city";
  [panel beginSheetModalForWindow:self.window
                completionHandler:^(NSModalResponse response) {
                  if (response != NSModalResponseOK)
                    return;
                  self.busy = YES;
                  [self.citySession
                      checkpoint:panel.URL.path
                            load:load
                      completion:^(CdCityState *state, NSString *error) {
                        self.busy = NO;
                        if (state) {
                          self.cityState = state;
                          self.people = state.people;
                          [self listChanged:nil];
                        } else
                          [self failure:error];
                        [self refresh];
                      }];
                }];
}
- (void)scenarioOperation:(id)sender {
  (void)sender;
  if (self.busy || !self.cityState)
    return;
  self.running = NO;
  NSAlert *alert = [NSAlert new];
  alert.messageText = @"Ordinary scenario operation";
  alert.informativeText =
      @"Observer by default. Interventions change city state. Use the "
      @"documented 15-field pipe command; Query/Cancel take an operation ID. "
      @"Deadlines and eligibility are rechecked at execution. See "
      @"docs/OPERATIONS.md.";
  NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 650, 90)];
  NSTextField *line =
      [[NSTextField alloc] initWithFrame:NSMakeRect(0, 55, 650, 26)];
  line.placeholderString =
      @"key|verb|actorID|target|secondary|units|value|starts|duration|earliest|"
      @"deadline|flags|auxiliary|label|basis";
  NSButton *permission =
      [NSButton checkboxWithTitle:@"Enable explicit scenario interventions"
                           target:nil
                           action:nil];
  permission.frame = NSMakeRect(0, 15, 650, 24);
  [view addSubview:line];
  [view addSubview:permission];
  alert.accessoryView = view;
  for (NSString *title in
       @[ @"Preview", @"Queue", @"Flush", @"Query", @"Cancel", @"Close" ])
    [alert addButtonWithTitle:title];
  [alert beginSheetModalForWindow:self.window
                completionHandler:^(NSModalResponse response) {
                  NSInteger action = response - NSAlertFirstButtonReturn;
                  if (action < 0 || action >= 5)
                    return;
                  self.busy = YES;
                  [self.citySession
                       operation:line.stringValue
                          action:@[
                            @"Preview", @"Queue", @"Flush", @"Query", @"Cancel"
                          ][action]
                       intervene:permission.state == NSControlStateValueOn
                      completion:^(CdCityState *state, NSString *message) {
                        self.busy = NO;
                        if (state)
                          self.cityState = state;
                        [self refresh];
                        [self failure:message];
                      }];
                }];
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
  NSString *identity = self.cityState.scenarioIdentity;
  self.window.subtitle = identity.length ? [@"Scenario " stringByAppendingString:identity] : @"";
  self.runButton.title = self.running ? @"Pause" : @"Run";
  if (!self.cityState) {
    self.status.stringValue = @"Loading city on simulation worker…";
    return;
  }
  if (self.management.window.isVisible)
    [self.management refresh];
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
  self.people = self.cityState.people;
  if (self.inspected && self.inspectedKind) {
    NSDictionary *selected = self.inspected;
    if ([self.inspectedKind isEqual:@"person"]) {
      NSString *identifier = selected[@"id"];
      NSDictionary *current = nil;
      for (NSDictionary *person in self.people)
        if ([person[@"id"] isEqual:identifier]) {
          current = person;
          break;
        }
      if (current)
        selected = current;
      else {
        self.agentLocation.stringValue =
            @"Selected agent is no longer active in this city; no current "
            @"location is available.";
        self.inspectorTitle.stringValue = @"Resident no longer active";
        self.inspector.string =
            @"The selected stable ID is retained for inspection. This person "
            @"cannot act in the current city.";
      }
      if (current) {
        self.map.selected = current;
        [self inspect:current kind:@"person"];
      }
    } else
      [self inspect:selected kind:self.inspectedKind];
  }
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
- (void)openScenarioStartup:(id)sender {
  (void)sender;
  if (self.busy || !self.cityState)
    return;
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  panel.title = @"Open compiled Canterbury startup (.cdstart)";
  if ([panel runModal] != NSModalResponseOK)
    return;
  self.running = NO;
  self.busy = YES;
  [self.citySession openScenario:panel.URL.path
                      completion:^(CdCityState *state, NSString *error) {
    self.busy = NO;
    if (!state) {
      [self failure:error];
    } else {
      self.cityState = state;
      self.people = state.people;
      self.map.selected = nil;
      [self listChanged:nil];
      [self inspect:nil kind:nil];
    }
    [self refresh];
  }];
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
        label(@"Every agent, one mark each", 21, NSFontWeightSemibold);
    self.agentsSummary = label(@"", 13, NSFontWeightMedium);
    NSTextField *legend = wrap(
        @"Roster order, NOT city locations. ×: unplaced. +: "
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
        @"id" : [NSString
            stringWithFormat:@"%llu", (unsigned long long)ref.person_id],
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
  view.font = [NSFont systemFontOfSize:12 * self.textScale];
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
- (void)locatePerson:(NSDictionary *)item center:(BOOL)center {
  CdPersonRef ref = {0};
  [item[@"ref"] getValue:&ref size:sizeof ref];
  CdPoint point;
  CdStatus status = cd_glyph_location(self.cityState.agentGlyphs.bytes,
                                      self.cityState.agentGlyphs.length /
                                          sizeof(CdAgentGlyph),
                                      ref.person_id, &point, NULL);
  if (status == CD_OK) {
    self.agentLocation.stringValue = [NSString
        stringWithFormat:
            @"Agent #%llu · map position %.1f, %.1f · highlighted on map",
            (unsigned long long)ref.person_id, point.x, point.y];
    if (center)
      [self.map centerOn:NSMakePoint(point.x, point.y)];
  } else
    self.agentLocation.stringValue =
        @"No map location assigned to this agent. Its + in All agents is a "
        @"roster cell, not a geographic position.";
}
- (void)selectRecord:(NSDictionary *)item kind:(NSString *)kind {
  [self inspect:item kind:kind];
  if ([kind isEqual:@"person"])
    [self locatePerson:item center:YES];
  self.map.selected = [kind isEqual:@"occupation"] ? nil : item;
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
  self.inspectedKind = kind;
  uint64_t revision = self.cityState.revision;
  if ([kind isEqual:@"person"])
    [self locatePerson:item center:NO];
  else
    self.agentLocation.stringValue = @"";
  if (!item) {
    self.inspectorTitle.stringValue = @"Explore Canterbury";
    self.inspector.string =
        @"The final assembled Canterbury map is rendered by json2png2.c using "
        @"the upstream json2png.c core, with cropped OpenStreetMap road "
        @"connections and the assembled overview palette.\nSelect a marker, "
        @"footprint or record to inspect it. "
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
           if (self.inspected != item || self.cityState.revision != revision ||
               !details)
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
           self.inspector.string = [self.inspector.string
               stringByAppendingFormat:@"\nRuntime institution ID %@",
                                       details[@"runtimeID"]];
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
          if (self.inspected != item || self.cityState.revision != revision)
            return;
          if (status == CD_OK) {
            self.inspectorTitle.stringValue =
                [NSString stringWithUTF8String:resident.name];
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
            self.inspector.string=[self.inspector.string stringByAppendingFormat:@"\nNative energy %d · fatigue %u · %@ · meals carried %u · health severity %u",resident.energy,resident.fatigue,resident.sleeping?@"sleeping":@[@"idle",@"travelling",@"working",@"eating",@"resting",@"care",@"waiting",@"retired",@"chatting"][(unsigned)resident.activity],resident.carried_meals,resident.health_severity];
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
            if (item[@"structure_refinement"][@"osm_ref"])
              self.inspector.string = [self.inspector.string
                  stringByAppendingFormat:
                      @"\nReviewed surviving structure: %@. OSM outline only; "
                      @"surrounding modern buildings excluded. Historical "
                      @"coordinates remain schematic.",
                      item[@"structure_refinement"][@"osm_ref"]];
            NSString *base = [self.inspector.string copy];
            [self.citySession context:resident.ref
                           completion:^(NSString *context) {
                             if (self.inspected == item &&
                                 self.cityState.revision == revision)
                               self.inspector.string =
                                   [base stringByAppendingString:context];
                           }];

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
    if (item[@"placement_correction"])
      self.inspector.string = [self.inspector.string
          stringByAppendingString:
              @"\nMap footprint corrected for street clearance. "
              @"Schematic placement; historical location remains unverified."];
    if (item[@"structure_refinement"][@"osm_ref"])
      self.inspector.string = [self.inspector.string
          stringByAppendingFormat:
              @"\nReviewed surviving structure: %@. OSM outline only; "
              @"surrounding modern buildings excluded. Historical coordinates "
              @"remain schematic.",
              item[@"structure_refinement"][@"osm_ref"]];
    NSString *base = [self.inspector.string copy];
    [self.citySession
          building:item[@"id"]
        completion:^(NSString *details) {
          if (self.inspected == item && self.cityState.revision == revision)
            self.inspector.string =
                [base stringByAppendingFormat:@"\n%@", details];
        }];
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
