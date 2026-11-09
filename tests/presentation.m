#import <Cocoa/Cocoa.h>
#import "CityMap.h"
@interface CdPresentationBenchmark : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSWindow *window;
@end
@implementation CdPresentationBenchmark
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;
  self.window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 820, 850)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  self.window.title = @"Technical presentation benchmark · copied glyphs only";
  self.window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
  NSScrollView *scroll =
      [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 820, 800)];
  scroll.allowsMagnification = YES;
  scroll.minMagnification = .1;
  scroll.maxMagnification = 8;
  CdMapView *map =
      [[CdMapView alloc] initWithFrame:NSMakeRect(0, 0, 1600, 1600)];
  map.basemap = [[NSImage alloc]
      initWithContentsOfURL:[NSBundle.mainBundle
                                URLForResource:@"CanterburyMapBackground"
                                 withExtension:@"png"]];
  NSDictionary *content = [NSJSONSerialization
      JSONObjectWithData:
          [NSData dataWithContentsOfURL:[NSBundle.mainBundle
                                            URLForResource:@"buildings-enriched"
                                             withExtension:@"json"]]
                 options:0
                   error:nil];
  map.buildings = content[@"objects"];
  map.showAgents = YES;
  scroll.documentView = map;
  [self.window.contentView addSubview:scroll];
  [self.window makeKeyAndOrderFront:nil];
  [self.window center];
  [scroll setMagnification:.48];
  NSTextField *status =
      [NSTextField labelWithString:@"Measuring 30,000 synthetic copied marks; "
                                   @"no simulation is running."];
  status.frame = NSMakeRect(12, 810, 795, 30);
  [self.window.contentView addSubview:status];
  dispatch_async(dispatch_get_main_queue(), ^{
    NSMutableData *data =
        [NSMutableData dataWithLength:30000 * sizeof(CdAgentGlyph)];
    CdAgentGlyph *glyph = data.mutableBytes;
    for (size_t i = 0; i < 30000; i++)
      glyph[i] = (CdAgentGlyph){
          {1, i + 1}, {8 + (i % 200) * 7.9, 8 + (i / 200) * 10.5}, 1};
    NSBitmapImageRep *bitmap =
        [map bitmapImageRepForCachingDisplayInRect:map.bounds];
    NSMutableArray *durations = [NSMutableArray new];
    for (unsigned phase = 0; phase < 2; phase++) {
      map.agentGlyphs = phase ? data : nil;
      for (unsigned frame = 0; frame < 31; frame++) {
        NSTimeInterval start = NSProcessInfo.processInfo.systemUptime;
        [map cacheDisplayInRect:map.bounds toBitmapImageRep:bitmap];
        double ms = (NSProcessInfo.processInfo.systemUptime - start) * 1000;
        if (phase && frame)
          [durations addObject:@(ms)];
      }
    }
    NSArray *sorted = [durations sortedArrayUsingSelector:@selector(compare:)];
    double sum = 0;
    for (NSNumber *n in durations)
      sum += n.doubleValue;
    NSDictionary *result = @{
      @"format" : @"canterbury-presentation-benchmark",
      @"source_sha256" :
          NSBundle.mainBundle.infoDictionary[@"CdBenchmarkSourceSHA"],
      @"frames" : @30,
      @"glyphs" : @30000,
      @"mean_ms" : @(sum / 30),
      @"p95_ms" : sorted[28],
      @"max_ms" : sorted.lastObject,
      @"scope" : @"Offscreen AppKit full-map raster capture; copied technical "
                 @"glyphs, no engine or physical-device FPS claim",
      @"reduce_motion_requested" : @(
          [NSWorkspace sharedWorkspace].accessibilityDisplayShouldReduceMotion),
      @"ok" : @YES
    };
    NSData *json =
        [NSJSONSerialization dataWithJSONObject:result
                                        options:NSJSONWritingPrettyPrinted
                                          error:NULL];
    NSURL *output = [NSURL
        fileURLWithPath:[NSBundle.mainBundle
                                .bundlePath stringByDeletingLastPathComponent]];
    output = [output URLByAppendingPathComponent:@"presentation-metrics.json"];
    [json writeToURL:output options:NSDataWritingAtomic error:NULL];
    status.stringValue = [NSString
        stringWithFormat:
            @"30,000 marks · mean %.2f ms · p95 %@ ms · no simulation clock",
            sum / 30, sorted[28]];
  });
}
@end
int main(void) {
  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];
    CdPresentationBenchmark *delegate = [CdPresentationBenchmark new];
    app.delegate = delegate;
    [app run];
  }
  return 0;
}
