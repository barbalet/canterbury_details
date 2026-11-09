#import "CitySession.h"
#import <Cocoa/Cocoa.h>
@interface CdManagementController : NSWindowController
@property(nonatomic, strong) CdCitySession *session;
@property(nonatomic, copy) BOOL (^beginMutation)(void);
@property(nonatomic, copy) void (^publish)(CdCityState *, NSString *);
@property(nonatomic, copy) void (^cityAction)(NSString *);
- (void)refresh;
@end
