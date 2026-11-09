#ifndef CD_NATIVE_H
#define CD_NATIVE_H
#include "cd_bootstrap.h"
typedef void (*CdNativeMinute)(CdSmokeSession *, void *);
CdStatus cd_native_client(CdSmokeSession *, CdNativeMinute, void *);
CdStatus cd_native_position(const CdSmokeSession *, CdPersonRef, uint16_t *,
                            uint16_t *);
CdStatus cd_native_write(CdSmokeSession *, CdPersonRef, uint16_t, uint16_t);
int cd_native_can_work(const CdSmokeSession *, CdPersonRef);
int cd_native_claim(CdSmokeSession *, CdPersonRef);
CdStatus cd_native_name(const CdSmokeSession *, CdPersonRef, char *, size_t);
CdStatus cd_native_city_body(CdSmokeSession *, CdPersonRef);
void cd_native_fixture_name(uint64_t, uint64_t, char *, size_t);
CdStatus cd_native_activity_memories(const CdSmokeSession *, CdPersonRef,
                                     char *, size_t);
void cd_native_remember_food(CdSmokeSession *, CdPersonRef, int);
int cd_native_chat(CdSmokeSession *, CdPersonRef, CdPersonRef);
void cd_native_remember_birth(CdSmokeSession *, CdPersonRef, CdPersonRef);
void cd_native_remember_training(CdSmokeSession *, CdPersonRef, CdPersonRef);
void cd_native_remember_work(CdSmokeSession *, CdPersonRef);
void cd_native_remember_service(CdSmokeSession *, CdPersonRef, CdPersonRef);
unsigned cd_native_walk_units(void);
uint64_t cd_native_minutes(const CdSmokeSession *);
int cd_native_closed(const CdSmokeSession *);
int cd_native_energy(const CdSmokeSession *, CdPersonRef);
void cd_native_energy_set(CdSmokeSession *, CdPersonRef, int);
void cd_native_awake(CdSmokeSession *, CdPersonRef, int);
void cd_native_block(CdSmokeSession *, CdPersonRef, int sleeping);
unsigned cd_native_hungry(void);
unsigned cd_native_full(void);
typedef int (*CdCheckpointIo)(void *, void *, size_t);
CdStatus cd_native_checkpoint(const CdSmokeSession *, CdCheckpointIo, void *,
                              CdDiagnostic *);
CdStatus cd_native_checkpoint_stage(CdCheckpointIo, void *, CdSmokeStage **,
                                    CdDiagnostic *);
int cd_native_checkpoint_owner(const CdSmokeSession *);
void cd_native_checkpoint_publish(CdSmokeStage **, CdSmokeSession **);
uint64_t cd_native_stage_digest(const CdSmokeStage *);
uint64_t cd_native_stage_minutes(const CdSmokeStage *);
size_t cd_native_stage_count(const CdSmokeStage *);
int cd_native_stage_has(const CdSmokeStage *, CdPersonId);
uint64_t cd_native_checkpoint_abi(void);
uint64_t cd_native_stage_token(const CdSmokeStage *);
uint64_t cd_native_full_digest(const CdSmokeSession *);
#endif
