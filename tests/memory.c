#include "cd_bootstrap.h"
#include "cd_memory.h"
#include "cd_random.h"
#include "bronze/actors/bronze_actors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef CD_TESTING
#error Memory qualification requires fault injection
#endif
#ifdef NDEBUG
#error Memory qualification requires assertions
#endif
static void same_memory(CdMemoryStats a,CdMemoryStats b){assert(a.blocks==b.blocks&&a.total==b.total);for(unsigned i=0;i<CD_MEM_CATEGORIES;i++)assert(a.bytes[i]==b.bytes[i]);}
static void empty(void){CdMemoryStats m=cd_memory_stats();assert(!m.blocks&&!m.total&&!bronze_actors_current()&&!sim_group());}
int main(void){
    CdDiagnostic e;CdSmokeSession *s=NULL;CdSmokeStage *stage=NULL;CdSmokeReport before,after;
    empty();assert(!cd_memory_calloc(SIZE_MAX,2,CD_MEM_CITY));assert(!cd_memory_calloc(1,SIZE_MAX,CD_MEM_CITY));
    assert(!cd_memory_calloc(1,(size_t)CD_MEMORY_LIMIT+1,CD_MEM_CITY));assert(!cd_memory_calloc(1,4,(CdMemoryCategory)-1));empty();
    size_t open_failures=0,stage_failures=0;
    for(size_t n=1;n<32;n++){
        cd_memory_fail_at(n);CdStatus result=cd_smoke_open(1,1940,&s,&e);cd_memory_fail_at(0);
        if(result==CD_OK){cd_smoke_close(s);s=NULL;empty();break;}
        assert(result==CD_CAPACITY&&!s);empty();open_failures++;
    }
    assert(open_failures>8&&open_failures<31);
    assert(cd_smoke_open(1,1940,&s,&e)==CD_OK);
    CdSmokeSession *saved=s;assert(cd_smoke_open(1,1940,&s,&e)==CD_INVALID_STATE&&s==saved);
    assert(cd_smoke_open(0,1940,&s,&e)==CD_INVALID_ARGUMENT&&s==saved);
    CdMemoryStats live=cd_memory_stats();assert(live.bytes[CD_MEM_NATIVE]==sizeof(simulated_group));assert(live.bytes[CD_MEM_ACTORS]==bronze_actors_canterbury_storage_bytes());
    assert(cd_smoke_read(s,&before,&e)==CD_OK);CdPersonRef old;assert(cd_smoke_person(s,0,&old,&e)==CD_OK);
    n_byte2 native_rng[2];memcpy(native_rng,sim_group()->beings[0].delta.random_seed,sizeof native_rng);
    for(size_t n=1;n<32;n++){
        cd_memory_fail_at(n);CdStatus result=cd_smoke_prepare(32,7,&stage,&e);cd_memory_fail_at(0);
        if(result==CD_OK){cd_smoke_discard(stage);stage=NULL;same_memory(live,cd_memory_stats());break;}
        assert(result==CD_CAPACITY&&!stage);stage_failures++;
        same_memory(live,cd_memory_stats());assert(cd_smoke_read(s,&after,&e)==CD_OK&&after.digest==before.digest);
        size_t slot;assert(cd_smoke_resolve(s,old,&slot,&e)==CD_OK&&slot==0);
        assert(!memcmp(native_rng,sim_group()->beings[0].delta.random_seed,sizeof native_rng));
    }
    assert(stage_failures==open_failures);
    assert(cd_smoke_prepare(32,7,&stage,&e)==CD_OK);CdMemoryStats staged=cd_memory_stats();
    CdSmokeStage *saved_stage=stage;assert(cd_smoke_prepare(1,1,&stage,&e)==CD_INVALID_ARGUMENT&&stage==saved_stage);same_memory(staged,cd_memory_stats());
    CdSmokeSession *wrong=NULL;assert(cd_smoke_publish(&stage,&wrong,&e)==CD_INVALID_STATE&&stage);same_memory(staged,cd_memory_stats());
    assert(cd_smoke_read(s,&after,&e)==CD_OK&&after.digest==before.digest);
    uint64_t preserved_rng;
    assert(cd_smoke_random(s,CD_RNG_SERVICES,&preserved_rng,&e)==CD_OK&&preserved_rng==UINT64_C(0x69f4606cbb110662));
    /* Any attempt to allocate during publication would now fail. */
    size_t attempts=cd_memory_stats().attempts;cd_memory_fail_at(1);
    assert(cd_smoke_publish(&stage,&s,&e)==CD_OK&&!stage);cd_memory_fail_at(0);
    assert(cd_memory_stats().attempts==attempts);
    assert(cd_smoke_read(s,&after,&e)==CD_OK&&after.people==32&&after.native_minutes==1);
    size_t slot;assert(cd_smoke_resolve(s,old,&slot,&e)==CD_UNKNOWN_REFERENCE);cd_smoke_close(s);s=NULL;empty();
    const uint64_t golden[]={UINT64_C(0x18a4325abf118bc0),UINT64_C(0x2ba53866c2eb6ea5),UINT64_C(0x69f4606cbb110662)};
    for(unsigned repeat=0;repeat<3;repeat++){
        assert(cd_smoke_open(1,1940,&s,&e)==CD_OK);uint64_t value=99;
        assert(cd_smoke_random(s,(CdRandomStream)99,&value,&e)==CD_INVALID_ARGUMENT&&value==99);
        for(unsigned i=0;i<3;i++)assert(cd_smoke_random(s,(CdRandomStream)i,&value,&e)==CD_OK&&value==golden[i]);
        CdRandom expected;cd_random_seed(&expected,1940);cd_random_next(&expected,CD_RNG_ADMISSION);
        for(unsigned i=0;i<10;i++)assert(cd_smoke_random(s,CD_RNG_ROUTINES,&value,&e)==CD_OK);
        assert(cd_smoke_random(s,CD_RNG_ADMISSION,&value,&e)==CD_OK&&value==cd_random_next(&expected,CD_RNG_ADMISSION));
        CdPersonRef admitted;
        assert(cd_smoke_admit_next(s,&admitted,&e)==CD_OK);cd_random_next(&expected,CD_RNG_ADMISSION);
        assert(cd_smoke_admit_next(s,NULL,&e)==CD_INVALID_ARGUMENT);
        assert(cd_smoke_random(s,CD_RNG_ADMISSION,&value,&e)==CD_OK&&value==cd_random_next(&expected,CD_RNG_ADMISSION));
        cd_smoke_close(s);s=NULL;empty();
    }
    assert(cd_smoke_open(30000,1940,&s,&e)==CD_OK);CdPersonRef fresh;
    assert(cd_smoke_admit_next(s,&fresh,&e)==CD_CAPACITY);uint64_t value;
    assert(cd_smoke_random(s,CD_RNG_ADMISSION,&value,&e)==CD_OK&&value==golden[0]);
    cd_smoke_close(s);s=NULL;empty();
    CdMemoryStats m=cd_memory_stats();
    printf("{\"open_allocation_failures\":%zu,\"replacement_allocation_failures\":%zu,\"publish_allocations\":0,\"live_world_preserved\":true,\"rng_golden_and_isolation\":true,\"closed_blocks\":%zu,\"closed_bytes\":%zu,\"peak_payload_bytes\":%zu,\"live_native_bytes\":%zu,\"live_actor_bytes\":%zu,\"live_city_bytes\":%zu,\"live_temporary_bytes\":%zu,\"live_sdk_buffer_bytes\":%zu,\"ok\":true}\n",open_failures,stage_failures,m.blocks,m.total,m.peak_total,live.bytes[CD_MEM_NATIVE],live.bytes[CD_MEM_ACTORS],live.bytes[CD_MEM_CITY],live.bytes[CD_MEM_TEMP],live.bytes[CD_MEM_SDK_BUFFER]);
}
