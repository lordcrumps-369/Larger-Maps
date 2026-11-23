static std::vector<Point3> lot_raw,lot_old,lot_fixed;
static std::vector<Point2> lot_uv;
static std::vector<uint16_t> lot_map,lot_faces;
static void test_lot_model(){
 const char* root="fixtures/lot/";
 auto path=[&](const char* file){return std::string(root)+file;};
 lot_raw=load_water_test<Point3>(path("conversion01-raw-element00.bin").c_str());
 lot_uv=load_water_test<Point2>(path("conversion01-raw-element02.bin").c_str());
 lot_map=load_water_test<uint16_t>(path("conversion01-stream00-map0.bin").c_str());
 lot_old=load_water_test<Point3>(path("conversion01-source-positions.bin").c_str());
 auto groups=load_water_test<unsigned char>(path("conversion01-source-groups.bin").c_str());
 uint32_t n=0;std::memcpy(&n,groups.data()+24,4);assert(n==121476&&groups.size()==92+n*2);lot_faces.resize(n);std::memcpy(lot_faces.data(),groups.data()+92,n*2);
 std::vector<Point3> expected;unsigned changed=0;assert(lot_repair_positions(lot_raw,lot_map,expected,lot_fixed,changed));assert(expected==lot_old&&changed==425);
 unsigned long_before=0,long_after=0;float max_after=0;
 for(unsigned i=0;i<n;i+=3){float before=0,after=0;for(unsigned j=0;j<3;j++){unsigned a=lot_faces[i+j],b=lot_faces[i+(j+1)%3];assert(a<lot_old.size()&&b<lot_old.size());float sum1=0,sum2=0;for(unsigned k=0;k<3;k++){float d=lot_old[a][k]-lot_old[b][k];sum1+=d*d;d=lot_fixed[a][k]-lot_fixed[b][k];sum2+=d*d;}before=std::max(before,std::sqrt(sum1));after=std::max(after,std::sqrt(sum2));}long_before+=before>100;long_after+=after>100;max_after=std::max(max_after,after);}
 assert(long_before==426&&long_after==0&&max_after<14.15f);
 auto map=lot_map;map[1]=map[0];assert(!lot_repair_positions(lot_raw,map,expected,lot_fixed,changed));
 map=lot_map;map.back()=600;assert(!lot_repair_positions(lot_raw,map,expected,lot_fixed,changed));
 map=lot_map;map.back()=0;assert(!lot_repair_positions(lot_raw,map,expected,lot_fixed,changed));
 map={1,2,3};assert(!lot_repair_positions(lot_raw,map,expected,lot_fixed,changed));
 auto raw=lot_raw;raw[0][2]+=1;assert(!lot_repair_positions(raw,lot_map,expected,lot_fixed,changed));
 assert(lot_repair_positions(lot_raw,lot_map,expected,lot_fixed,changed));
 puts("PASS real lot capture: 425 repaired positions, 426 stretched triangles removed, max edge 14.142; clipped repeated grid rows preserved; invalid map order, second wrap, out-of-range references, unwrapped map and nonflat grid rejected");
}
static void* lot_index_vt[7]{};
static FakeIterator lot_begin{lot_index_vt},lot_owner{lot_index_vt},lot_end{lot_index_vt};
static bool __fastcall lot_ended(void* self,void*,void*){return static_cast<FakeIterator*>(self)->at>=lot_faces.size();}
static uint16_t __fastcall lot_index(void* self,void*){return lot_faces[static_cast<FakeIterator*>(self)->at];}
static void* __fastcall lot_indices(void*,void*,void* out,unsigned index){assert(index==0);lot_begin.at=0;auto tuple=static_cast<void**>(out);tuple[0]=&lot_begin;tuple[1]=&lot_owner;tuple[2]=&lot_end;return out;}
static void* __fastcall lot_group(void* self,void*,void* out,unsigned index){assert(index==0);uint32_t info[16]={0,2,unsigned(lot_faces.size()),0,0,reinterpret_cast<uint32_t>(static_cast<MockWater*>(self)->name)};std::memcpy(out,info,sizeof(info));return out;}
static void test_lot_integration(){
 auto hood_uv=captured_uv;captured_uv=lot_uv;
 water_mock_vt[0xb4/4]=reinterpret_cast<void*>(&lot_group);water_mock_vt[0xb8/4]=reinterpret_cast<void*>(&lot_indices);
 lot_index_vt[2]=reinterpret_cast<void*>(&iter_release);lot_index_vt[4]=reinterpret_cast<void*>(&lot_ended);lot_index_vt[5]=reinterpret_cast<void*>(&lot_index);lot_index_vt[6]=reinterpret_cast<void*>(&iter_next);
 MockWater source;source.positions=lot_raw;source.uv=lot_uv;source.count=lot_map.size();source.posmap=lot_map;source.uvmap.resize(source.count);for(unsigned i=0;i<source.count;i++)source.uvmap[i]=uint16_t(i);source.name="subset-LotSkirtWater";source.bind(water_mock_vt);
 unsigned clones=water_mock_clones;
 {WaterCopyOwner fixed{try_lot_water_fix(&source)};assert(fixed.copy==&water_mock_copy&&water_mock_copy.positions==lot_fixed&&water_mock_copy.uv==lot_uv);assert(source.positions==lot_raw&&source.posmap==lot_map&&source.releases==0);}
 assert(water_mock_copy.releases==1&&water_mock_clones==clones+1);
 {WaterCopyOwner fixed{try_lot_water_fix(&source)};assert(fixed.copy);}assert(water_mock_clones==clones+2&&water_mock_copy.releases==1);
 source.name="unrelated";assert(!try_lot_water_fix(&source)&&water_mock_clones==clones+2);source.name="subset-LotSkirtWater";
 unsigned saved=lot_faces[0];lot_faces[0]=65535;assert(!try_lot_water_fix(&source)&&water_mock_clones==clones+2);lot_faces[0]=uint16_t(saved);
 corrupt_water_copy=true;assert(!try_lot_water_fix(&source)&&water_mock_clones==clones+3&&water_mock_copy.releases==1);corrupt_water_copy=false;
 assert(source.positions==lot_raw&&source.posmap==lot_map&&source.uv==lot_uv);
 captured_uv=hood_uv;
 puts("PASS lot native-API integration: position-only clone repair, UV and original maps unchanged, repeat conversion, clone release, full triangle validation, wrong subset and bad copy rejection");
}
