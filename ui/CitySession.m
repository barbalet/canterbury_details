#import "CitySession.h"
#include "cd_work.h"
@implementation CdCityState
@end
@interface CdCitySession () {
  CdContent *_content;
  CdWorld *_world;
  dispatch_queue_t _queue;
  NSArray<NSDictionary *> *_people;
  uint64_t _admissions, _retirements;
}
@end
@implementation CdCitySession
- (instancetype)init {
  if ((self = [super init]))
    _queue = dispatch_queue_create("org.canterbury.details.native",
                                   DISPATCH_QUEUE_SERIAL);
  return self;
}
- (CdCityState *)state:(CdDiagnostic *)error rebuild:(BOOL)rebuild {
  CdCityState *state = [CdCityState new];
  CdClockView clock;
  CdCitySummary summary;
  if (cd_world_clock(_world, &clock, error) != CD_OK ||
      cd_world_summary(_world, &summary, error) != CD_OK)
    return nil;
  state.clock = clock;
  state.summary = summary;
  CdCivicSummary civic;
  if (cd_world_civic_summary(_world, &civic, error) != CD_OK)
    return nil;
  state.civic = civic;
  rebuild = rebuild || _people.count != summary.people ||
            _admissions != summary.admissions ||
            _retirements != summary.deaths + summary.departures;
  _admissions = summary.admissions;
  _retirements = summary.deaths + summary.departures;
  NSMutableData *data =
      [NSMutableData dataWithLength:summary.people * sizeof(CdAgentGlyph)];
  CdAgentGlyph *glyphs = data.mutableBytes;
  NSMutableArray *people = rebuild ? [NSMutableArray new] : nil;
  for (size_t i = 0; i < summary.people; i++) {
    CdResidentView resident;
    if (cd_world_resident(_world, i, &resident, error) != CD_OK)
      return nil;
    glyphs[i] = (CdAgentGlyph){resident.ref, resident.position,
                               resident.position_known};
    if (rebuild) {
      CdPersonRef ref = resident.ref;
      [people addObject:@{
        @"id" : [NSString
            stringWithFormat:@"%llu", (unsigned long long)ref.person_id],
        @"name" : [NSString stringWithUTF8String:resident.name],
        @"ref" : [NSValue valueWithBytes:&ref objCType:@encode(CdPersonRef)]
      }];
    }
  }
  if (rebuild)
    _people = [people copy];
  state.agentGlyphs = data;
  state.people = _people;
  return state;
}
- (void)publish:(CdCityState *)state
          error:(CdDiagnostic)error
     completion:(void (^)(CdCityState *, NSString *))completion {
  NSString *message =
      state ? nil : [NSString stringWithUTF8String:error.message];
  dispatch_async(dispatch_get_main_queue(), ^{
    completion(state, message);
  });
}
- (void)open:(NSString *)path
    completion:(void (^)(CdCityState *, NSString *))completion {
  dispatch_async(_queue, ^{
    @autoreleasepool {
      CdDiagnostic error = {0};
      CdScenario scenario = {
          27000, 1940, {1940, 1, 1, 0}, "synthetic-ordinary-v1"};
      CdCityState *state = nil;
      if (cd_content_open(path.fileSystemRepresentation, &self->_content,
                          &error) == CD_OK &&
          cd_world_open(self->_content, &scenario, &self->_world, &error) ==
              CD_OK)
        state = [self state:&error rebuild:YES];
      [self publish:state error:error completion:completion];
    }
  });
}
- (void)step:(unsigned)minutes
    completion:(void (^)(CdCityState *, NSString *))completion {
  dispatch_async(_queue, ^{
    @autoreleasepool {
      CdDiagnostic error = {0};
      CdCityState *state = cd_world_step(self->_world, minutes, &error) == CD_OK
                               ? [self state:&error rebuild:NO]
                               : nil;
      [self publish:state error:error completion:completion];
    }
  });
}
- (void)restart:(void (^)(CdCityState *, NSString *))completion {
  dispatch_async(_queue, ^{
    @autoreleasepool {
      CdDiagnostic error = {0};
      CdScenario scenario = {
          27000, 1940, {1940, 1, 1, 0}, "synthetic-ordinary-v1"};
      CdCityState *state = cd_world_open(self->_content, &scenario,
                                         &self->_world, &error) == CD_OK
                               ? [self state:&error rebuild:YES]
                               : nil;
      [self publish:state error:error completion:completion];
    }
  });
}
- (void)resident:(CdPersonRef)ref
      completion:(void (^)(CdResidentView, CdStatus))completion {
  dispatch_async(_queue, ^{
    CdResidentView view = {0};
    CdStatus status = cd_world_resolve(self->_world, ref, &view, NULL);
    dispatch_async(dispatch_get_main_queue(), ^{
      completion(view, status);
    });
  });
}
- (void)institution:(NSString *)sourceID
         completion:(void (^)(NSDictionary *))completion {
  dispatch_async(_queue, ^{
    NSDictionary *result = nil;
    for (size_t i = 0; i < cd_world_institution_count(self->_world); i++) {
      CdInstitutionDetail view;
      if (cd_world_institution(self->_world, i, &view, NULL) != CD_OK)
        continue;
      if (![sourceID
              isEqualToString:[NSString stringWithUTF8String:view.source_id]])
        continue;
      result = @{
        @"operation" : view.operation_known
            ? (view.operating ? @"configured active" : @"configured closed")
            : @"unknown; no staffing inferred",
        @"premises" : @(view.premises_count),
        @"office" : [NSString stringWithUTF8String:view.office],
        @"basis" : [NSString stringWithUTF8String:view.operation_basis],
        @"source" : [NSString stringWithUTF8String:view.source_record]
      };
      break;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      completion(result);
    });
  });
}
- (void)close:(void (^)(void))completion {
  dispatch_async(_queue, ^{
    cd_world_close(self->_world);
    self->_world = NULL;
    cd_content_close(self->_content);
    self->_content = NULL;
    self->_people = nil;
    dispatch_async(dispatch_get_main_queue(), completion);
  });
}
@end
