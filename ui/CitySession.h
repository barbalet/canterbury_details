#include "cd_city.h"
#include "cd_civic.h"
#import <Foundation/Foundation.h>
/* Only the serial worker touches the native singleton. Completion blocks run on
 * main. */
typedef struct {
  CdPersonRef ref;
  CdPoint position;
  int located;
} CdAgentGlyph;
@interface CdCityState : NSObject
@property(nonatomic) CdClockView clock;
@property(nonatomic) CdCitySummary summary;
@property(nonatomic) CdCivicSummary civic;
@property(nonatomic, copy) NSArray<NSDictionary *> *people;
@property(nonatomic, copy) NSData *agentGlyphs;
@end
@interface CdCitySession : NSObject
- (void)open:(NSString *)path
    completion:(void (^)(CdCityState *, NSString *))completion;
- (void)step:(unsigned)minutes
    completion:(void (^)(CdCityState *, NSString *))completion;
- (void)restart:(void (^)(CdCityState *, NSString *))completion;
- (void)resident:(CdPersonRef)ref
      completion:(void (^)(CdResidentView, CdStatus))completion;
- (void)institution:(NSString *)sourceID
         completion:(void (^)(NSDictionary *))completion;
- (void)close:(void (^)(void))completion;
@end
