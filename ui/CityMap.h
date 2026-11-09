#import "CitySession.h"
#import <Cocoa/Cocoa.h>
@interface CdMapView : NSView
@property(nonatomic, strong) NSImage *basemap;
@property(nonatomic, copy) NSArray<NSDictionary *> *sites;
@property(nonatomic, copy) NSArray<NSDictionary *> *buildings;
@property(nonatomic, copy) NSArray<NSBezierPath *> *footprints;
@property(nonatomic, strong) NSDictionary *selected;
@property(nonatomic) BOOL showSites, showFootprints, showAgents;
@property(nonatomic, copy) NSData *agentGlyphs;
@property(nonatomic, copy) void (^selection)(NSDictionary *, NSString *);
@property(nonatomic, copy) void (^toggleRun)(void);
- (void)zoomBy:(CGFloat)factor;
- (void)fitMap;
- (void)centerOn:(NSPoint)point;
@end
@interface CdCenteredClipView : NSClipView
@end

/* A roster grid, deliberately separate from geographic map coordinates. */
@interface CdPopulationView : NSView
@property(nonatomic, copy) NSData *agentGlyphs;
@property(nonatomic, copy) void (^inspectAgent)(CdPersonRef);
@end
