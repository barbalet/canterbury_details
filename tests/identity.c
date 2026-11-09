#include "cd_bootstrap.h"
#include "bronze/actors/bronze_actors.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef NDEBUG
#error Identity qualification requires assertions
#endif
static void unchanged(CdSmokeSession *s,uint64_t digest){CdSmokeReport r;CdDiagnostic e;assert(cd_smoke_read(s,&r,&e)==CD_OK&&r.digest==digest);}
int main(int argc,char **argv){
    assert(argc==2);size_t count=strtoul(argv[1],NULL,10);assert(count==32||count==27000||count==30000);
    CdPersonRef *refs=calloc(count,sizeof *refs),*order=calloc(count,sizeof *order);assert(refs&&order);
    CdSmokeSession *s=NULL;CdDiagnostic e;CdSmokeReport report;size_t slot;
    assert(cd_smoke_open(count,1940,&s,&e)==CD_OK);
    for(size_t i=0;i<count;i++){
        assert(cd_smoke_person(s,i,&refs[i],&e)==CD_OK);
        assert(cd_smoke_resolve(s,refs[i],&slot,&e)==CD_OK&&slot==i);
        sim_group()->beings[i].delta.random_seed[0]=(n_byte2)i;
        order[i]=refs[i];
    }
    assert(cd_smoke_read(s,&report,&e)==CD_OK);uint64_t digest=report.digest;
    order[1]=order[0];assert(cd_smoke_reorder(s,order,count,&e)==CD_INVALID_DATA);unchanged(s,digest);order[1]=refs[1];
    assert(cd_smoke_reorder(s,order,count-1,&e)==CD_INVALID_ARGUMENT);unchanged(s,digest);
    order[1].world_token++;assert(cd_smoke_reorder(s,order,count,&e)==CD_UNKNOWN_REFERENCE);unchanged(s,digest);order[1]=refs[1];
    order[1].person_id=UINT64_MAX;assert(cd_smoke_reorder(s,order,count,&e)==CD_UNKNOWN_REFERENCE);unchanged(s,digest);order[1]=refs[1];
    for(size_t i=0;i<count;i++)order[i]=refs[(i+1)%count];
    sim_group()->select=&sim_group()->beings[0];
    assert(cd_smoke_reorder(s,order,count,&e)==CD_OK);
    for(size_t i=0;i<count;i++){
        assert(cd_smoke_resolve(s,refs[i],&slot,&e)==CD_OK&&slot==(i+count-1)%count);
        assert(sim_group()->beings[slot].delta.random_seed[0]==(n_byte2)i);
    }
    assert(sim_group()->select==&sim_group()->beings[count-1]);
    assert(cd_smoke_read(s,&report,&e)==CD_OK);digest=report.digest;
    sim_group()->num--;assert(cd_smoke_rebind(s,&e)==CD_INVALID_DATA);sim_group()->num++;
    unchanged(s,digest);
    /* Reconstruct SDK bindings without changing person records or body order. */
    BronzeActorRegistry *r=bronze_actors_current();bronze_actors_activate(NULL);bronze_actors_activate(r);
    assert(cd_smoke_rebind(s,&e)==CD_OK);
    assert(cd_smoke_step(s,1,&e)==CD_OK);
    CdPersonRef retired=order[1],survivor=order[count-1];
    assert(cd_smoke_retire(s,retired,&e)==CD_OK);
    assert(cd_smoke_resolve(s,retired,&slot,&e)==CD_UNKNOWN_REFERENCE);
    assert(cd_smoke_resolve(s,survivor,&slot,&e)==CD_OK&&slot==1);
    assert(sim_group()->beings[slot].delta.random_seed[0]==0);
    assert(cd_smoke_read(s,&report,&e)==CD_OK);digest=report.digest;
    assert(cd_smoke_retire(s,retired,&e)==CD_UNKNOWN_REFERENCE);unchanged(s,digest);
    CdPersonRef fresh;assert(cd_smoke_admit(s,7,&fresh,&e)==CD_OK);
    assert(fresh.person_id>refs[count-1].person_id&&fresh.world_token==refs[0].world_token);
    assert(cd_smoke_resolve(s,retired,&slot,&e)==CD_UNKNOWN_REFERENCE);
    if(count==cd_smoke_capacity()){assert(cd_smoke_read(s,&report,&e)==CD_OK);digest=report.digest;assert(cd_smoke_admit(s,8,&fresh,&e)==CD_CAPACITY);unchanged(s,digest);}
    assert(cd_smoke_rebind(s,&e)==CD_OK);assert(cd_smoke_step(s,1,&e)==CD_OK);
    if(count==32){
        while(sim_group()->num){CdPersonRef p;assert(cd_smoke_person(s,0,&p,&e)==CD_OK);assert(cd_smoke_retire(s,p,&e)==CD_OK);}
        assert(cd_smoke_reorder(s,NULL,0,&e)==CD_OK&&cd_smoke_rebind(s,&e)==CD_OK);
        assert(cd_smoke_step(s,1,&e)==CD_OK);assert(cd_smoke_read(s,&report,&e)==CD_OK&&report.native_people==0);
        assert(cd_smoke_admit(s,9,&fresh,&e)==CD_OK&&fresh.person_id>refs[count-1].person_id);
    }
    CdPersonRef foreign=refs[0];cd_smoke_close(s);s=NULL;
    assert(cd_smoke_open(1,1940,&s,&e)==CD_OK);
    assert(cd_smoke_person(s,0,&fresh,&e)==CD_OK&&fresh.person_id==foreign.person_id&&fresh.world_token!=foreign.world_token);
    assert(cd_smoke_read(s,&report,&e)==CD_OK);digest=report.digest;
    assert(cd_smoke_resolve(s,foreign,&slot,&e)==CD_UNKNOWN_REFERENCE);
    assert(cd_smoke_retire(s,foreign,&e)==CD_UNKNOWN_REFERENCE);
    assert(cd_smoke_reorder(s,&foreign,1,&e)==CD_UNKNOWN_REFERENCE);unchanged(s,digest);
    assert(cd_smoke_resolve(s,fresh,&slot,&e)==CD_OK&&slot==0);
    cd_smoke_close(s);free(refs);free(order);
    printf("{\"people\":%zu,\"reorder_body_markers_preserved\":true,\"compaction_preserved\":true,\"full_rebind\":true,\"retired_ids_rejected\":true,\"foreign_world_rejected\":true,\"invalid_requests_atomic\":true,\"ok\":true}\n",count);
    return 0;
}
