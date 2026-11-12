#include "cd_city.h"
#include "cd_civic.h"
#include "cd_observer.h"
#include "cd_management.h"
#import <Foundation/Foundation.h>
/* Only the serial worker touches the native singleton. Completion blocks run on
 * main. */
@interface CdCityState : NSObject
@property(nonatomic) uint64_t revision;
@property(nonatomic) CdClockView clock;
@property(nonatomic, copy) NSString *scenarioIdentity;
@property(nonatomic) CdCitySummary summary;
@property(nonatomic) CdCivicSummary civic;
@property(nonatomic, copy) NSArray<NSDictionary *> *people;
@property(nonatomic, copy) NSData *agentGlyphs;
@end
@interface CdCitySession : NSObject
- (void)open:(NSString *)path
  checkpoint:(NSString *)checkpoint
    completion:(void (^)(CdCityState *, NSString *))completion;
- (void)openScenario:(NSString *)path
         completion:(void (^)(CdCityState *, NSString *))completion;
- (void)step:(unsigned)minutes
    completion:(void (^)(CdCityState *, NSString *))completion;
- (void)restart:(void (^)(CdCityState *, NSString *))completion;
- (void)resident:(CdPersonRef)ref
      completion:(void (^)(CdResidentView, CdStatus))completion;
- (void)institution:(NSString *)sourceID
         completion:(void (^)(NSDictionary *))completion;
- (void)checkpoint:(NSString *)path
              load:(BOOL)load
        completion:(void (^)(CdCityState *, NSString *))completion;
- (void)operation:(NSString *)line
           action:(NSString *)action
        intervene:(BOOL)intervene
       completion:(void (^)(CdCityState *, NSString *))completion;
- (void)context:(CdPersonRef)ref completion:(void (^)(NSString *))completion;
- (void)building:(NSString *)sourceID
      completion:(void (^)(NSString *))completion;
- (void)exportRecords:(NSString *)path
           diagnostic:(BOOL)diagnostic
           completion:(void (^)(NSString *))completion;
- (void)management:(CdManagementKind)kind
            offset:(size_t)offset
        completion:(void (^)(NSString *, size_t))completion;
- (void)close:(void (^)(void))completion;
@end
