#import "CityManagement.h"
#include "cd_management.h"
@interface CdManagementController ()
@property(nonatomic, strong) NSPopUpButton *section, *verb;
@property(nonatomic, strong) NSGridView *formGrid;
@property(nonatomic, strong) NSTextView *report;
@property(nonatomic, strong) NSTextField *help, *result, *operationID;
@property(nonatomic, strong) NSButton *intervene;
@property(nonatomic, strong) NSArray<NSTextField *> *fields;
@property(nonatomic, strong) NSArray<NSDictionary *> *catalogue;
@property(nonatomic) size_t offset, total;
@property(nonatomic) BOOL reading, working;
@end
static NSButton *actionButton(NSString *title, id target, SEL action) {
  NSButton *b = [NSButton buttonWithTitle:title target:target action:action];
  b.accessibilityLabel = title;
  NSDictionary *keys = @{
    @"Preview" : @"p",
    @"Queue" : @"k",
    @"Apply due" : @"f",
    @"Query ID" : @"j",
    @"Cancel ID" : @"b",
    @"Refresh status" : @"r",
    @"Next page" : @"]",
    @"Previous page" : @"["
  };
  if (keys[title]) {
    b.keyEquivalent = keys[title];
    b.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    b.toolTip =
        [NSString stringWithFormat:@"%@ (Command-%@)", title, keys[title]];
  }
  return b;
}
static NSStackView *row(NSArray *views) {
  NSStackView *s = [NSStackView stackViewWithViews:views];
  s.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  s.spacing = 8;
  return s;
}
@implementation CdManagementController
- (instancetype)init {
  NSWindow *window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 1000, 820)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  if ((self = [super initWithWindow:window])) {
    window.title = @"Canterbury 1940 · City management";
    window.minSize = NSMakeSize(820, 740);
    window.releasedWhenClosed = NO;
    window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    NSURL *url = [NSBundle.mainBundle URLForResource:@"operation-controls"
                                       withExtension:@"json"];
    NSDictionary *data =
        url ? [NSJSONSerialization
                  JSONObjectWithData:[NSData dataWithContentsOfURL:url]
                             options:0
                               error:NULL]
            : nil;
    self.catalogue = data[@"commands"] ?: @[];
    NSStackView *layout = [NSStackView new];
    layout.orientation = NSUserInterfaceLayoutOrientationVertical;
    layout.alignment = NSLayoutAttributeLeading;
    layout.spacing = 10;
    layout.translatesAutoresizingMaskIntoConstraints = NO;
    [window.contentView addSubview:layout];
    [NSLayoutConstraint activateConstraints:@[
      [layout.leadingAnchor
          constraintEqualToAnchor:window.contentView.leadingAnchor
                         constant:16],
      [layout.trailingAnchor
          constraintEqualToAnchor:window.contentView.trailingAnchor
                         constant:-16],
      [layout.topAnchor constraintEqualToAnchor:window.contentView.topAnchor
                                       constant:16],
      [layout.bottomAnchor
          constraintEqualToAnchor:window.contentView.bottomAnchor
                         constant:-16]
    ]];
    NSTextField *scenario =
        [NSTextField wrappingLabelWithString:
                         @"Canterbury 1940 · synthetic ordinary population · "
                         @"final assembled map. Source staffing, housing and "
                         @"unassigned positions remain unknown."];
    scenario.font = [NSFont boldSystemFontOfSize:14];
    [layout addArrangedSubview:scenario];
    [layout addArrangedSubview:row(@[
              actionButton(@"Run / pause", self, @selector(run:)),
              actionButton(@"Step 1 minute", self, @selector(step:)),
              actionButton(@"Save…", self, @selector(save:)),
              actionButton(@"Load…", self, @selector(load:)),
              actionButton(@"Larger text", self, @selector(larger:)),
              actionButton(@"Smaller text", self, @selector(smaller:))
            ])];
    self.section = [NSPopUpButton new];
    for (unsigned i = 0; i < CD_MANAGE_KINDS; i++)
      [self.section
          addItemWithTitle:[NSString
                               stringWithUTF8String:cd_management_name(i)]];
    self.section.target = self;
    self.section.action = @selector(sectionChanged:);
    self.section.accessibilityLabel = @"Management status category";
    [layout addArrangedSubview:row(@[
              self.section,
              actionButton(@"Previous page", self, @selector(previous:)),
              actionButton(@"Next page", self, @selector(next:)),
              actionButton(@"Refresh status", self, @selector(refreshAction:))
            ])];
    self.report = [NSTextView new];
    self.report.editable = NO;
    self.report.font = [NSFont monospacedSystemFontOfSize:12
                                                   weight:NSFontWeightRegular];
    self.report.accessibilityLabel = @"Current city management records";
    NSScrollView *scroll = [NSScrollView new];
    scroll.documentView = self.report;
    scroll.hasVerticalScroller = YES;
    scroll.borderType = NSBezelBorder;
    [layout addArrangedSubview:scroll];
    [scroll.heightAnchor constraintGreaterThanOrEqualToConstant:140].active =
        YES;
    self.verb = [NSPopUpButton new];
    for (unsigned i = 0; i < CD_CMD_KINDS; i++)
      [self.verb
          addItemWithTitle:[NSString stringWithUTF8String:cd_command_name(i)]];
    self.verb.target = self;
    self.verb.action = @selector(verbChanged:);
    self.verb.accessibilityLabel = @"Ordinary operation";
    self.intervene =
        [NSButton checkboxWithTitle:@"Enable scenario interventions"
                             target:nil
                             action:nil];
    self.intervene.accessibilityHelp =
        @"Off means observer mode; requests that change the city are rejected.";
    [layout addArrangedSubview:row(@[ self.verb, self.intervene ])];
    self.help = [NSTextField wrappingLabelWithString:@""];
    self.help.font = [NSFont systemFontOfSize:12];
    [layout addArrangedSubview:self.help];
    NSArray *names = @[
      @"Request key", @"Person ID", @"Target ID", @"Secondary ID", @"Units",
      @"Value", @"Starts (civil minute)", @"Duration / shift end",
      @"Earliest (native minute)", @"Deadline (native minute)",
      @"Options / flags", @"Auxiliary ID", @"Label", @"Explicit basis"
    ];
    NSMutableArray *fields = [NSMutableArray new],
                   *gridRows = [NSMutableArray new];
    for (NSUInteger i = 0; i < names.count; i += 2) {
      NSMutableArray *cells = [NSMutableArray new];
      for (NSUInteger k = i; k < MIN(i + 2, names.count); k++) {
        NSTextField *label = [NSTextField labelWithString:names[k]],
                    *field = [NSTextField textFieldWithString:k == 0    ? @"1"
                                                              : k >= 12 ? @""
                                                                        : @"0"];
        field.accessibilityLabel = names[k];
        field.placeholderString =
            k == 13 ? @"Required scenario provenance" : @"";
        [fields addObject:field];
        [cells addObjectsFromArray:@[ label, field ]];
      }
      [gridRows addObject:cells];
    }
    self.fields = fields;
    NSGridView *grid = [NSGridView gridViewWithViews:gridRows];
    self.formGrid = grid;
    grid.columnSpacing = 8;
    grid.rowSpacing = 6;
    NSScrollView *formScroll = [NSScrollView new];
    formScroll.hasVerticalScroller = YES;
    formScroll.hasHorizontalScroller = YES;
    formScroll.documentView = grid;
    grid.frame =
        NSMakeRect(0, 0, grid.fittingSize.width, grid.fittingSize.height);
    [layout addArrangedSubview:formScroll];
    [formScroll.heightAnchor constraintGreaterThanOrEqualToConstant:110]
        .active = YES;
    NSLayoutConstraint *formHeight =
        [formScroll.heightAnchor constraintEqualToConstant:210];
    formHeight.priority = NSLayoutPriorityDefaultLow;
    formHeight.active = YES;
    for (NSUInteger i = 0; i < grid.numberOfColumns; i++)
      if (i % 2)
        [grid columnAtIndex:i].width = 170;
    grid.frame =
        NSMakeRect(0, 0, grid.fittingSize.width, grid.fittingSize.height);
    self.operationID = [NSTextField textFieldWithString:@""];
    self.operationID.placeholderString = @"Operation ID";
    self.operationID.accessibilityLabel = @"Operation ID to query or cancel";
    [self.operationID.widthAnchor constraintEqualToConstant:110].active = YES;
    [layout
        addArrangedSubview:row(@[
          actionButton(@"Preview", self, @selector(preview:)),
          actionButton(@"Queue", self, @selector(queue:)),
          actionButton(@"Apply due", self, @selector(flush:)), self.operationID,
          actionButton(@"Query ID", self, @selector(query:)),
          actionButton(@"Cancel ID", self, @selector(cancel:))
        ])];
    self.result = [NSTextField
        wrappingLabelWithString:
            @"Observer mode. Status/query are read-only. Enable interventions "
            @"for "
            @"preview, queue, apply or cancel. Apply due advances no time."];
    self.result.accessibilityLabel = @"Operation result";
    [layout addArrangedSubview:self.result];
    for (NSView *v in @[ scenario, scroll, self.help, formScroll, self.result ])
      [v.widthAnchor constraintEqualToAnchor:layout.widthAnchor].active = YES;
    for (NSUInteger i = 0; i + 1 < fields.count; i++)
      [fields[i] setNextKeyView:fields[i + 1]];
    window.initialFirstResponder = self.section;
    [self verbChanged:nil];
    [window center];
  }
  return self;
}
- (void)showWindow:(id)sender {
  [super showWindow:sender];
  [self refresh];
}
- (void)verbChanged:(id)sender {
  (void)sender;
  NSUInteger i = self.verb.indexOfSelectedItem;
  self.help.stringValue = i < self.catalogue.count
                              ? self.catalogue[i][@"help"]
                              : @"See OPERATIONS.md for parameters. Zero "
                                @"unused numeric fields; basis is required.";
}
- (void)refresh {
  if (self.reading || !self.session)
    return;
  self.reading = YES;
  NSInteger section = self.section.indexOfSelectedItem;
  size_t offset = self.offset;
  [self.session management:(CdManagementKind)section
                    offset:offset
                completion:^(NSString *text, size_t total) {
                  self.reading = NO;
                  if (section != self.section.indexOfSelectedItem ||
                      offset != self.offset) {
                    [self refresh];
                    return;
                  }
                  self.total = total;
                  self.report.string = text;
                }];
}
- (void)sectionChanged:(id)sender {
  (void)sender;
  self.offset = 0;
  [self refresh];
}
- (void)refreshAction:(id)sender {
  (void)sender;
  [self refresh];
}
- (void)previous:(id)sender {
  (void)sender;
  self.offset = self.offset >= 100 ? self.offset - 100 : 0;
  [self refresh];
}
- (void)next:(id)sender {
  (void)sender;
  if (self.offset + 100 < self.total)
    self.offset += 100;
  [self refresh];
}
- (void)send:(NSString *)action {
  if (self.working)
    return;
  if (self.beginMutation && !self.beginMutation()) {
    self.result.stringValue = @"A native minute batch is still running. Pause "
                              @"and retry after it finishes.";
    return;
  }
  self.working = YES;
  NSMutableArray *parts = [NSMutableArray new];
  [parts addObject:self.fields[0].stringValue];
  [parts addObject:self.verb.titleOfSelectedItem];
  for (NSUInteger i = 1; i < self.fields.count; i++)
    [parts addObject:self.fields[i].stringValue];
  NSString *line = ([action isEqual:@"Query"] || [action isEqual:@"Cancel"])
                       ? self.operationID.stringValue
                       : [parts componentsJoinedByString:@"|"];
  [self.session operation:line
                   action:action
                intervene:self.intervene.state == NSControlStateValueOn
               completion:^(CdCityState *state, NSString *message) {
                 self.working = NO;
                 self.result.stringValue = message;
                 if (self.publish)
                   self.publish(state, nil);
                 [self refresh];
               }];
}
- (void)preview:(id)sender {
  (void)sender;
  [self send:@"Preview"];
}
- (void)queue:(id)sender {
  (void)sender;
  [self send:@"Queue"];
}
- (void)flush:(id)sender {
  (void)sender;
  [self send:@"Flush"];
}
- (void)query:(id)sender {
  (void)sender;
  [self send:@"Query"];
}
- (void)cancel:(id)sender {
  (void)sender;
  [self send:@"Cancel"];
}
- (void)run:(id)sender {
  (void)sender;
  if (self.cityAction)
    self.cityAction(@"run");
}
- (void)step:(id)sender {
  (void)sender;
  if (self.cityAction)
    self.cityAction(@"step");
}
- (void)save:(id)sender {
  (void)sender;
  if (self.cityAction)
    self.cityAction(@"save");
}
- (void)load:(id)sender {
  (void)sender;
  if (self.cityAction)
    self.cityAction(@"load");
}
- (void)scaleForm:(CGFloat)delta {
  for (NSInteger r = 0; r < self.formGrid.numberOfRows; r++)
    for (NSInteger c = 0; c < self.formGrid.numberOfColumns; c++) {
      NSView *view = [self.formGrid cellAtColumnIndex:c rowIndex:r].contentView;
      if ([view isKindOfClass:NSTextField.class]) {
        NSTextField *field = (NSTextField *)view;
        field.font = [NSFont
            systemFontOfSize:MIN(24, MAX(10, field.font.pointSize + delta))];
      }
    }
  self.formGrid.frame = NSMakeRect(0, 0, self.formGrid.fittingSize.width,
                                   self.formGrid.fittingSize.height);
  self.help.font = [NSFont
      systemFontOfSize:MIN(24, MAX(10, self.help.font.pointSize + delta))];
  self.result.font = [NSFont
      systemFontOfSize:MIN(24, MAX(10, self.result.font.pointSize + delta))];
}
- (void)larger:(id)sender {
  (void)sender;
  self.report.font =
      [NSFont monospacedSystemFontOfSize:MIN(26, self.report.font.pointSize + 1)
                                  weight:NSFontWeightRegular];
}
- (void)smaller:(id)sender {
  (void)sender;
  self.report.font =
      [NSFont monospacedSystemFontOfSize:MAX(10, self.report.font.pointSize - 1)
                                  weight:NSFontWeightRegular];
}
@end
