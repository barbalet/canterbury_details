#import "CitySession.h"
#include "cd_work.h"
@implementation CdCityState
@end
@interface CdCitySession () {
  CdContent *_content;
  CdWorld *_world;
  dispatch_queue_t _queue;
  NSArray<NSDictionary *> *_people;
  uint64_t _admissions, _retirements, _revision;
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
  state.revision = ++_revision;
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
    CdPersonRef current;
    CdStatus status =
        cd_world_reference(self->_world, ref.person_id, &current, NULL);
    if (status == CD_OK)
      status = cd_world_resolve(self->_world, current, &view, NULL);
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
        @"runtimeID" : @(view.id),
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
- (void)checkpoint:(NSString *)path
              load:(BOOL)load
        completion:(void (^)(CdCityState *, NSString *))completion {
  dispatch_async(_queue, ^{
    @autoreleasepool {
      CdDiagnostic d = {0};
      CdStatus status =
          load ? cd_world_load(&self->_world, self->_content,
                               path.fileSystemRepresentation, &d)
               : cd_world_save(self->_world, path.fileSystemRepresentation, &d);
      CdCityState *state = status == CD_OK ? [self state:&d rebuild:load] : nil;
      [self publish:state error:d completion:completion];
    }
  });
}
- (void)operation:(NSString *)line
           action:(NSString *)action
        intervene:(BOOL)intervene
       completion:(void (^)(CdCityState *, NSString *))completion {
  dispatch_async(_queue, ^{
    @autoreleasepool {
      CdDiagnostic d = {0};
      CdCommand command = {0};
      CdStatus status = CD_INVALID_ARGUMENT;
      uint64_t id = 0;
      CdPermission permission =
          intervene ? CD_PERMISSION_INTERVENTION : CD_PERMISSION_OBSERVER;
      if ([action isEqual:@"Flush"])
        status = cd_world_operations_flush(self->_world, permission, &d);
      else if ([action isEqual:@"Cancel"] || [action isEqual:@"Query"]) {
        NSScanner *scanner = [NSScanner scannerWithString:line];
        unsigned long long value = 0;
        if ([scanner scanUnsignedLongLong:&value] && scanner.isAtEnd && value) {
          if ([action isEqual:@"Cancel"])
            status =
                cd_world_operation_cancel(self->_world, value, permission, &d);
          else {
            CdOperationView view = {0};
            status = cd_world_operation(self->_world, value, &view, &d);
            if (status == CD_OK)
              snprintf(
                  d.message, sizeof d.message,
                  "Operation %llu: state %u, result %u, resulting ID %llu: %s",
                  value, view.state, view.result,
                  (unsigned long long)view.result_id, view.reason);
          }
        } else
          snprintf(d.message, sizeof d.message,
                   "Enter one positive operation ID to query/cancel");
      } else if ((status = cd_command_parse(self->_world, line.UTF8String,
                                            &command, &d)) == CD_OK) {
        if ([action isEqual:@"Preview"]) {
          CdPreview preview;
          status = cd_world_operation_preview(self->_world, &command,
                                              permission, &preview, &d);
        } else {
          status = cd_world_operation_queue(self->_world, &command, permission,
                                            &id, &d);
          if (status == CD_OK)
            snprintf(
                d.message, sizeof d.message,
                "Queued operation %llu. Query its ID for execution result.",
                (unsigned long long)id);
        }
      }
      NSString *message =
          [NSString stringWithFormat:@"%s · status %u", d.message, status];
      CdCityState *state = [self state:NULL rebuild:NO];
      dispatch_async(dispatch_get_main_queue(), ^{
        completion(state, message);
      });
    }
  });
}
- (void)context:(CdPersonRef)ref completion:(void (^)(NSString *))completion {
  dispatch_async(_queue, ^{
    CdPersonRef current;
    CdResidentView view;
    NSMutableString *text = [NSMutableString new];
    if (cd_world_reference(self->_world, ref.person_id, &current, NULL) ==
            CD_OK &&
        cd_world_resolve(self->_world, current, &view, NULL) == CD_OK) {
      for (unsigned i = 0; i < 2; i++)
        if (view.jobs[i]) {
          CdJobView job;
          CdOccupationView occupation;
          if (cd_world_job(self->_world, view.jobs[i], &job, NULL) == CD_OK &&
              cd_occupation(job.occupation - 1, &occupation, NULL) == CD_OK)
            [text appendFormat:@"\nOccupation: %s · job basis: %s",
                               occupation.name, job.basis];
        }
      CdWorkView work;
      if (cd_world_work(self->_world, current, &work, NULL) == CD_OK && work.id)
        [text appendFormat:@"\nWork status: %s", work.reason];
      CdServiceView service;
      if (view.service_request &&
          cd_world_service(self->_world, view.service_request, &service,
                           NULL) == CD_OK)
        [text appendFormat:@"\nService state %u: %s · basis %s", service.state,
                           service.reason, service.basis];
      CdGatheringView gathering;
      if (view.gathering &&
          cd_world_gathering_view(self->_world, view.gathering, &gathering,
                                  NULL) == CD_OK)
        [text appendFormat:@"\nGathering state %u: %s", gathering.state,
                           gathering.reason];
      for (unsigned k = 0; k < 4; k++)
        if (view.institution_memberships[k])
          for (size_t i = 0; i < cd_world_institution_count(self->_world);
               i++) {
            CdInstitutionDetail site;
            if (cd_world_institution(self->_world, i, &site, NULL) == CD_OK &&
                site.id == view.institution_memberships[k])
              [text appendFormat:
                        @"\nExplicit institutional membership: %s (%llu)",
                        site.name, (unsigned long long)site.id];
          }
      char memories[4096];
      if (cd_world_activity_memories(
              self->_world, current,
              (CdRecordQuery){current, CD_RECORD_PERSONAL}, memories,
              sizeof memories, NULL) == CD_OK)
        [text appendFormat:@"\n\nNative activity memories (bounded)\n%@",
                           [NSString stringWithUTF8String:memories] ?: @""];
      CdRecordSummary history = {0};
      if (cd_world_record_summary(self->_world, &history, NULL) == CD_OK) {
        [text appendString:@"\n\nRecent named conversations (newest first)\n"];
        unsigned shown = 0;
        for (uint64_t id = history.next; id > history.first && shown < 12;) {
          CdRecord event;
          if (cd_world_record(self->_world, --id,
                              (CdRecordQuery){current, CD_RECORD_PERSONAL},
                              &event, NULL) != CD_OK ||
              event.actor.person_id != current.person_id ||
              event.kind != CD_EVENT_CHAT)
            continue;
          [text appendFormat:@"\nMinute %llu · %s",
                             (unsigned long long)event.minute, event.message];
          shown++;
        }
        if (!shown)
          [text appendString:@"No named conversation records retained yet."];
      }
      CdSocialView social;
      if (cd_world_social(self->_world, current, &social, NULL) == CD_OK &&
          social.bereavement_event)
        [text appendFormat:@"\nHousehold bereavement record %llu · ordinary "
                           @"support context %@",
                           (unsigned long long)social.bereavement_event,
                           social.support_context_active ? @"active"
                                                         : @"elapsed"];
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      completion(text);
    });
  });
}
- (void)building:(NSString *)sourceID
      completion:(void (^)(NSString *))completion {
  dispatch_async(_queue, ^{
    NSString *text = @"No runtime premises match this source record.";
    for (size_t i = 0; i < cd_content_building_count(self->_content); i++) {
      CdBuildingView building;
      CdPremisesView premises;
      if (cd_content_building(self->_content, i, &building, NULL) != CD_OK ||
          ![sourceID
              isEqualToString:[NSString
                                  stringWithUTF8String:building.source_id]])
        continue;
      if (cd_world_premises(self->_world, building.id, &premises, NULL) ==
          CD_OK)
        text = [NSString
            stringWithFormat:
                @"Runtime ID %llu · capacity %@ · condition %u\nUse: %s · "
                @"employer: %s · %@\nSource confidence %.3f · policy: %s",
                (unsigned long long)building.id,
                premises.capacity_known ? [@(premises.capacity) description]
                                        : @"unknown",
                premises.condition, premises.use, premises.employer,
                premises.scenario_override ? @"explicit scenario override"
                                           : @"source candidate",
                building.confidence, building.policy_basis];
      break;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      completion(text);
    });
  });
}
- (void)exportRecords:(NSString *)path
           diagnostic:(BOOL)diagnostic
           completion:(void (^)(NSString *))completion {
  dispatch_async(_queue, ^{
    CdDiagnostic d = {0};
    CdStatus status = cd_world_export_records(
        self->_world,
        (CdRecordQuery){{0},
                        diagnostic ? CD_RECORD_DIAGNOSTIC : CD_RECORD_PUBLIC},
        path.fileSystemRepresentation, &d);
    NSString *message =
        [NSString stringWithFormat:@"%s · status %u", d.message, status];
    dispatch_async(dispatch_get_main_queue(), ^{
      completion(message);
    });
  });
}
- (void)management:(CdManagementKind)kind
            offset:(size_t)offset
        completion:(void (^)(NSString *, size_t))completion {
  dispatch_async(_queue, ^{
    @autoreleasepool {
      CdManagementRow rows[100];
      size_t total = 0, written = 0;
      CdDiagnostic d = {0};
      CdClockView clock = {0};
      cd_world_clock(self->_world, &clock, NULL);
      CdStatus status = cd_world_management(self->_world, kind, offset, rows,
                                            100, &total, &written, &d);
      NSMutableString *text = [NSMutableString
          stringWithFormat:
              @"%s · native minute %llu · %zu records · page %zu\nRead-only "
              @"diagnostics; source unknowns remain unknown.\n",
              cd_management_name(kind),
              (unsigned long long)clock.native_minutes, total,
              offset / 100 + 1];
      if (status != CD_OK)
        [text appendFormat:@"%s", d.message];
      for (size_t i = 0; i < written; i++) {
        CdManagementRow *r = &rows[i];
        [text appendFormat:@"\nID %llu · %s · state %u%@\n%s\n",
                           (unsigned long long)r->id, r->name, r->state,
                           r->known ? @"" : @" · source capacity UNKNOWN",
                           r->detail];
      }
      if (!total)
        [text
            appendString:@"\nNo configured records in this category. No "
                         @"staffing, jobs or scenario events are inferred.\n"];
      dispatch_async(dispatch_get_main_queue(), ^{
        completion(text, total);
      });
    }
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
