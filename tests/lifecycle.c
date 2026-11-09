#include "cd_bootstrap.h"
#include "bronze/actors/bronze_actors.h"
#include "entity/entity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#error Lifecycle qualification requires assertions
#endif
static void date(uint64_t elapsed,unsigned y,unsigned m,unsigned d,unsigned h,unsigned min){
    CdCivilTime t;CdDiagnostic e;assert(cd_smoke_civil_time(elapsed,&t,&e)==CD_OK);
    assert(t.year==(int)y&&t.month==m&&t.day==d&&t.minute_of_day==h*60+min);
}
static uint64_t raw_a,raw_b;
static void raw_callback(simulated_group *g,void *context){
    assert(g->num==1);(*(uint64_t *)context)++;
}
static void raw_session(uint64_t *counter){
    BronzeActorRegistry *r=bronze_actors_create();assert(r);bronze_actors_activate(r);
    assert(sim_canterbury_configure(1,raw_callback,counter));
    assert(sim_init(KIND_START_UP,1940,MAP_AREA,0));sim_cycle();sim_cycle();
    assert(*counter==2);assert(!sim_canterbury_configure(1,raw_callback,&raw_a));
    sim_close();sim_cycle();assert(*counter==2);bronze_actors_destroy(r);assert(!bronze_actors_current());
    /* Close must erase the handler: an unconfigured restart cannot succeed. */
    assert(!sim_init(KIND_START_UP,1940,MAP_AREA,0));
}
int main(void){
    CdDiagnostic e;CdCivilTime t;CdSmokeReport initial,after;CdSmokeSession *s=NULL;
    date(0,1940,1,1,0,0);date(59,1940,1,1,0,59);date(60,1940,1,1,1,0);
    date(1439,1940,1,1,23,59);date(1440,1940,1,2,0,0);
    date(59*UINT64_C(1440),1940,2,29,0,0);date(60*UINT64_C(1440),1940,3,1,0,0);
    date(366*UINT64_C(1440),1941,1,1,0,0);
    /* 2000 is leap, 2100 is not; 1940..1999 contains 15 leap days. */
    date((60*UINT64_C(365)+15+59)*1440,2000,2,29,0,0);
    date((160*UINT64_C(365)+40+59)*1440,2100,3,1,0,0);
    assert(cd_smoke_civil_time(UINT64_MAX,&t,&e)==CD_INVALID_ARGUMENT);
    assert(cd_smoke_civil_time(0,NULL,&e)==CD_INVALID_ARGUMENT);
    assert(cd_smoke_time(NULL,&t,&e)==CD_INVALID_STATE);
    assert(cd_smoke_open(1,1940,&s,&e)==CD_OK);
    assert(cd_smoke_read(s,&initial,&e)==CD_OK&&initial.native_minutes==1&&initial.callbacks==1);
    assert(cd_smoke_time(s,&t,&e)==CD_OK&&t.minute_of_day==1);
    simulated_being *body=&sim_group()->beings[0];
    n_int x=being_location_x(body),y=being_location_y(body),energy=being_energy(body);
    n_byte2 rng[2];memcpy(rng,body->delta.random_seed,sizeof rng);
    for(unsigned i=0;i<20;i++){
        assert(cd_smoke_read(s,&after,&e)==CD_OK&&after.digest==initial.digest);
        assert(cd_smoke_time(s,&t,&e)==CD_OK&&t.minute_of_day==1);
    }
    assert(cd_smoke_step(s,0,&e)==CD_OK);
    assert(cd_smoke_step(s,10001,&e)==CD_INVALID_ARGUMENT);
    assert(cd_smoke_read(s,&after,&e)==CD_OK&&after.digest==initial.digest);
    assert(cd_smoke_step(s,1439,&e)==CD_OK);
    assert(cd_smoke_time(s,&t,&e)==CD_OK&&t.day==2&&t.minute_of_day==0);
    assert(cd_smoke_read(s,&after,&e)==CD_OK&&after.native_minutes==1440&&after.callbacks==1440);
    assert(x==being_location_x(body)&&y==being_location_y(body)&&energy==being_energy(body));
    assert(!memcmp(rng,body->delta.random_seed,sizeof rng));
    cd_smoke_close(s);s=NULL;
    assert(cd_smoke_step(NULL,1,&e)==CD_INVALID_STATE);cd_smoke_close(NULL);
    /* Alternate population and seed; each fresh session restarts native elapsed. */
    assert(cd_smoke_open(32,7,&s,&e)==CD_OK);
    assert(cd_smoke_step(s,17,&e)==CD_OK);cd_smoke_close(s);s=NULL;
    assert(cd_smoke_open(1,1940,&s,&e)==CD_OK);
    assert(cd_smoke_read(s,&after,&e)==CD_OK&&after.digest==initial.digest&&after.callbacks==1);
    assert(cd_smoke_time(s,&t,&e)==CD_OK&&t.day==1&&t.minute_of_day==1);
    cd_smoke_close(s);s=NULL;
    raw_session(&raw_a);raw_session(&raw_b);assert(raw_a==2&&raw_b==2);
    assert(cd_smoke_open(1,1940,&s,&e)==CD_OK);
    assert(cd_smoke_step(s,1,&e)==CD_OK);assert(raw_a==2&&raw_b==2);cd_smoke_close(s);
    puts("{\"native_minutes_checked\":1440,\"native_sessions\":6,\"pause_pure\":true,\"calendar_boundaries\":true,\"native_autonomy_bypassed\":true,\"stale_callback_rejected\":true,\"ok\":true}");
    return 0;
}
