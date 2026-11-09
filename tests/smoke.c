#include "cd_bootstrap.h"
#include "universe/universe.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
static void unused_callback(simulated_group *group,void *context){(void)group;(void)context;assert(0);}
int main(void){
    assert(!sim_canterbury_configure(30001,unused_callback,NULL));
    assert(!sim_canterbury_configure(27000,NULL,NULL));
    assert(sim_canterbury_configure(27000,unused_callback,NULL));
    assert(cd_smoke_default_people()==27000&&cd_smoke_capacity()==30000);
    CdSmokeSession *session=NULL,*other=NULL;CdDiagnostic error;CdSmokeReport first,next;
    assert(cd_smoke_open(0,1940,&session,&error)==CD_INVALID_ARGUMENT);
    assert(cd_smoke_open(30001,1940,&session,&error)==CD_INVALID_ARGUMENT);
    assert(cd_smoke_open(32,1940,&session,&error)==CD_OK);
    assert(cd_smoke_open(1,1940,&other,&error)==CD_INVALID_STATE);
    assert(!sim_canterbury_configure(27000,unused_callback,NULL));
    assert(cd_smoke_read(session,&first,&error)==CD_OK&&first.people==32&&first.native_people==32&&first.actor_records==32&&first.bindings_valid&&first.callbacks==1);
    for(unsigned i=0;i<10;i++){assert(cd_smoke_read(session,&next,&error)==CD_OK);assert(next.digest==first.digest&&next.native_minutes==first.native_minutes);}
    assert(cd_smoke_step(session,0,&error)==CD_OK);assert(cd_smoke_read(session,&next,&error)==CD_OK&&next.digest==first.digest);
    assert(cd_smoke_step(session,5,&error)==CD_OK);assert(cd_smoke_read(session,&next,&error)==CD_OK&&next.native_minutes==first.native_minutes+5&&next.callbacks==6);
    cd_smoke_close(session);session=NULL;
    assert(cd_smoke_open(1,1940,&session,&error)==CD_OK);assert(cd_smoke_read(session,&next,&error)==CD_OK&&next.people==1&&next.bindings_valid);cd_smoke_close(session);
    puts("PASS native bootstrap identities, sole clock, read-only inspection, zero-step pause, single-session rejection and reopen");return 0;
}
