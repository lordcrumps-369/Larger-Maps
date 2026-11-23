#define TS2_UNIT_TESTS 1
#include "large_maps.cpp"
#include <cassert>
struct FakeIterator {void** vt;unsigned at=0,releases=0;bool position=false;};
static void* position_vt[7]{};static void* index_vt[7]{};
static FakeIterator pos_begin{position_vt,0,0,true},pos_owner{position_vt},pos_end{position_vt};
static FakeIterator idx_begin{index_vt},idx_owner{index_vt},idx_end{index_vt};
static const float source_points[3][3]={{0,0,1},{10,0,2},{10,10,3}};
static const uint16_t source_indices[3]={0,1,2};
static unsigned __fastcall iter_release(void* p,void*){return ++static_cast<FakeIterator*>(p)->releases;}
static unsigned position_limit=3;
static bool __fastcall iter_ended(void* p,void*,void*){return static_cast<FakeIterator*>(p)->at>=(static_cast<FakeIterator*>(p)->position?position_limit:3);}
static void __fastcall iter_next(void* p,void*){++static_cast<FakeIterator*>(p)->at;}
static void* __fastcall iter_position(void* p,void*,float* out){unsigned i=static_cast<FakeIterator*>(p)->at;if(position_limit==3)std::memcpy(out,source_points[i],12);else{out[0]=float((i%257)*10);out[1]=float((i/257)*10);out[2]=float(i%100);}return out;}
static uint16_t __fastcall iter_index(void* p,void*){return source_indices[static_cast<FakeIterator*>(p)->at];}
struct FakeGeometry{void** vt;unsigned count=3;unsigned maps=0;};
static void* __cdecl fake_position_factory(void* out,void* geometry,unsigned stream,unsigned channel){assert(stream==0&&channel==0);auto g=static_cast<FakeGeometry*>(geometry);position_limit=g->maps==7?g->count:(g->count==66049?513:3);pos_begin.at=0;auto p=static_cast<void**>(out);p[0]=&pos_begin;p[1]=&pos_owner;p[2]=&pos_end;return out;}
static unsigned __fastcall fake_one(void*,void*){return 1;}
static void* __fastcall fake_stream_info(void* self,void*,void* out,unsigned index){assert(index==0);uint32_t data[4]={0,3,static_cast<FakeGeometry*>(self)->count,0};std::memcpy(out,data,16);return out;}
static void* __fastcall fake_group_info(void*,void*,void* out,unsigned index){assert(index==0);uint32_t data[8]={0,2,3,0,100,0,0,0};std::memcpy(out,data,32);return out;}
static void* __fastcall fake_group_indices(void*,void*,void* out,unsigned index){assert(index==0);idx_begin.at=0;auto p=static_cast<void**>(out);p[0]=&idx_begin;p[1]=&idx_owner;p[2]=&idx_end;return out;}
static unsigned native_forwarded=0;static void* last_native_geometry=nullptr;
static void __fastcall fake_native_pass(void*,void*,void* geometry){last_native_geometry=geometry;++native_forwarded;}
static void setup_iterators(){
 for(void** vt:{position_vt,index_vt}){vt[2]=reinterpret_cast<void*>(&iter_release);vt[4]=reinterpret_cast<void*>(&iter_ended);vt[6]=reinterpret_cast<void*>(&iter_next);}
 position_vt[5]=reinterpret_cast<void*>(&iter_position);index_vt[5]=reinterpret_cast<void*>(&iter_index);
 hood_original=convert_original=reinterpret_cast<NativePass>(&fake_native_pass);
}
static FakeGeometry cloned_geometry{};static unsigned clone_releases=0,clone_calls=0;
static unsigned __fastcall clone_release(void*,void*){return ++clone_releases;}
static bool __fastcall fake_clone(void* self,void*,uint32_t iid,void** out){assert(iid==0x5bafb147);cloned_geometry=*static_cast<FakeGeometry*>(self);*out=&cloned_geometry;++clone_calls;return true;}

static void __fastcall fake_set_count(void* p,void*,unsigned stream,unsigned count){assert(stream==0&&count==65536);auto g=static_cast<FakeGeometry*>(p);g->count=count;g->maps=0;}
static void __fastcall fake_set_map(void* p,void*,unsigned stream,unsigned component,const uint16_t* ids,unsigned count){assert(stream==0&&component<3&&count==65536);for(unsigned i=0;i<count;i++)assert(ids[i]==i);static_cast<FakeGeometry*>(p)->maps|=1u<<component;}
static void test_preservation(){
 void* vt[64]{};vt[2]=reinterpret_cast<void*>(&clone_release);vt[0x9c/4]=reinterpret_cast<void*>(&fake_one);vt[0xb0/4]=reinterpret_cast<void*>(&fake_one);vt[0xa0/4]=reinterpret_cast<void*>(&fake_stream_info);vt[0xb4/4]=reinterpret_cast<void*>(&fake_group_info);vt[0xb8/4]=reinterpret_cast<void*>(&fake_group_indices);vt[0xe0/4]=reinterpret_cast<void*>(&fake_clone);
 vt[0x40/4]=reinterpret_cast<void*>(&fake_set_count);vt[0x44/4]=reinterpret_cast<void*>(&fake_set_map);validated_count_method=vt[0x40/4];validated_map_method=vt[0x44/4];validated_clone_method=reinterpret_cast<void*>(&fake_clone);position_factory=&fake_position_factory;
 std::vector<std::array<float,3>> expected(65536);for(unsigned i=0;i<expected.size();i++)expected[i]={float((i%257)*10),float((i/257)*10),float(i%100)};uint32_t renderer[128]{};renderer[9]=renderer[10]=257;renderer[0x7c/4]=reinterpret_cast<uint32_t>(expected.data());uint32_t builder[128]{};builder[5]=3;builder[6]=reinterpret_cast<uint32_t>(renderer);uint32_t converted[128]{};
 auto run=[&](unsigned built,unsigned incoming,bool expected_restore){
  FakeGeometry geom{vt,built};unsigned before_clones=clone_calls,before_releases=clone_releases;
  hood_hook(builder+8,nullptr,&geom);geom.count=incoming;convert_hook(converted,nullptr,&geom);
  assert(last_native_geometry==(expected_restore?static_cast<void*>(&cloned_geometry):static_cast<void*>(&geom)));
  assert(geom.count==incoming);assert(!has_preserved(&geom));
  assert(clone_calls-before_clones==(built==66049?1u:0u));assert(clone_releases-before_releases==(built==66049?1u:0u));
 };
 assert((66049u&0xffffu)==513u);
 
 run(66049,65536,true);run(66049,65536,true);run(66049,65536,true);
 run(66049,66049,true);run(65536,65536,false);
 expected[0][0]=123;run(66049,65536,false);expected[0][0]=0;
 renderer[9]=renderer[10]=129;FakeGeometry normal{vt,66049};unsigned before=clone_calls;
 hood_hook(builder+8,nullptr,&normal);convert_hook(converted,nullptr,&normal);assert(last_native_geometry==&normal&&clone_calls==before);
 puts("PASS repeated hood builds without resetting counters, explicit maps across 65535/65536, 513-point implicit-wrap model, exact 65536-position verification, mismatch rejection, conditional substitution, original data and clone lifetime");
}
#include "water_tests.inc"
#include "lot_water_tests.inc"
#include "strip_tests.inc"
using namespace importer;
static std::array<unsigned char,26> input{};
static unsigned native_calls=0,reader_calls=0,import_calls=0;
static bool native_parse=true,second_parse=true,change_second_header=false,import_result=true;
static void* expected_object=nullptr;
static void put32(unsigned char* p,unsigned n){std::memcpy(p,&n,4);}
static bool __fastcall fake_validate(void* object,void*){
    assert(object==expected_object);++native_calls;
    std::memset(static_cast<unsigned char*>(object)+0x10,0,26);
    if(!native_parse)return false;
    std::memcpy(static_cast<unsigned char*>(object)+0x10,input.data(),26);
    return word32(input.data()+2)==65&&word32(input.data()+6)==65;
}
static bool __stdcall fake_read(void* file,void* output){
    assert(file==reinterpret_cast<void*>(0x1234));++reader_calls;
    std::memcpy(output,input.data(),26);
    if(change_second_header)static_cast<unsigned char*>(output)[25]^=1;
    return second_parse;
}
static void test_importer(){
    using namespace importer;
    std::array<unsigned char,128> object{};void* file=reinterpret_cast<void*>(0x1234);std::memcpy(object.data(),&file,sizeof(file));
    expected_object=object.data();validate_original=reinterpret_cast<Validate>(&fake_validate);read_header=&fake_read;
    for(unsigned w:{0u,64u,65u,66u,128u,129u,130u,257u})for(unsigned h:{0u,65u,129u,257u}){
        input.fill(0);put32(input.data()+2,w);put32(input.data()+6,h);unsigned before=reader_calls;
        bool result=validate_hook(object.data(),nullptr);
        assert(result==((w==65&&h==65)||(w==129&&h==129)));
        if(w==65&&h==65)assert(reader_calls==before);else assert(reader_calls==before+1);
        assert(!std::memcmp(object.data(),&file,sizeof(file)));
        assert(!std::memcmp(object.data()+0x10,input.data(),26));
    }
    // Never accept a partial/failed native resource read or a changed resource.
    input.fill(0);put32(input.data()+2,129);put32(input.data()+6,129);
    second_parse=false;assert(!validate_hook(object.data(),nullptr));second_parse=true;
    native_parse=false;assert(!validate_hook(object.data(),nullptr));native_parse=true;
    change_second_header=true;assert(!validate_hook(object.data(),nullptr));change_second_header=false;
    assert(validate_hook(object.data(),nullptr));
    // Relative calls can move; the actual instructions and field offsets cannot.
    auto mask=mask_for<sizeof(validator_signature)>();std::vector<unsigned char> bytes(300,0);
    std::memcpy(bytes.data()+17,validator_signature,sizeof(validator_signature));bytes[17+13]^=0x72;
    assert(unique_pattern(bytes.data(),bytes.size(),validator_signature,mask.data(),mask.size())==17);
    bytes[17+23]^=1;assert(unique_pattern(bytes.data(),bytes.size(),validator_signature,mask.data(),mask.size())==SIZE_MAX);bytes[17+23]^=1;
    std::memcpy(bytes.data()+150,validator_signature,sizeof(validator_signature));
    assert(unique_pattern(bytes.data(),bytes.size(),validator_signature,mask.data(),mask.size())==SIZE_MAX);
    auto resolved=resolve_native();assert(!resolved.validator&&!resolved.importer&&!resolved.reader);
    std::puts("PASS: original small-map path; medium-only extension; other sizes rejected; full stable header required; x86 calls/returns; masked-call signatures and wrong-code rejection.");
}

#include "placement_tests.inc"
#include "performance_tests.inc"
int main(){
 setup_iterators();test_strip_plan();test_water_model();test_lot_model();test_preservation();
 test_water_integration();test_lot_integration();test_strip_integration();test_importer();test_placement_sync();test_full_validation_cost();
 puts("PASS build23: full importer/terrain/boundary/water/picking suite and bounded iterator lookup cost; production has no runtime logs/captures");
}


