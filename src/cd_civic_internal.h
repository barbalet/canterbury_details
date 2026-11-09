#ifndef CD_CIVIC_INTERNAL_H
#define CD_CIVIC_INTERNAL_H
#include "cd_civic.h"
typedef struct {
  CdOfficeView view;
} CdOffice;
typedef struct {
  CdRosterView view;
} CdRoster;
typedef struct {
  CdServiceView view;
  uint64_t deadline, rate, care_start;
  int care_owned;
} CdService;
typedef struct {
  CdGatheringView view;
  CdPersonRef attendees[CD_ATTENDANCE_LIMIT];
  uint64_t credited[CD_ATTENDANCE_LIMIT];
  size_t count;
  uint64_t rate;
} CdGathering;
typedef struct {
  CdVehicleView view;
  uint32_t destination_door;
  CdPersonRef requested_passenger;
  CdEntityId unload_building;
  CdVehiclePhase resume_phase;
  int aborted;
} CdVehicle;
typedef struct {
  CdPressureView view;
} CdPressure;
#endif
