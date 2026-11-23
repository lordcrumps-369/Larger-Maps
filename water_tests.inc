template<class T>static std::vector<T> load_water_test(const char* name){
 FILE* f=std::fopen(name,"rb");assert(f);std::fseek(f,0,SEEK_END);long size=std::ftell(f);std::rewind(f);assert(size>=0&&size%sizeof(T)==0);std::vector<T> out(size/sizeof(T));assert(std::fread(out.data(),1,size,f)==unsigned(size));std::fclose(f);return out;
}
static std::vector<Point3> captured_raw,captured_old,captured_fixed;
static std::vector<Point2> captured_uv;
static void test_water_model(){
 captured_raw=load_water_test<Point3>("fixtures/hood/conversion00-raw-element00.bin");
 captured_uv=load_water_test<Point2>("fixtures/hood/conversion00-raw-element02.bin");
 captured_old=load_water_test<Point3>("fixtures/hood/conversion00-source-positions.bin");
 std::vector<Point3> old;unsigned changed=0;
 assert(water_repair_positions(captured_raw,captured_uv,old,captured_fixed,changed));assert(old==captured_old&&changed==439);
 auto groups=load_water_test<unsigned char>("fixtures/hood/conversion00-source-groups.bin");
 uint32_t n=0;std::memcpy(&n,groups.data()+24,4);assert(groups.size()==92+n*2&&n==158178);
 unsigned long_before=0,long_after=0;float max_after=0;
 for(unsigned i=0;i<n;i+=3){uint16_t ids[3];std::memcpy(ids,groups.data()+92+i*2,6);float before=0,after=0;
  for(unsigned j=0;j<3;j++){assert(ids[j]<captured_old.size());float a=0,b=0;for(unsigned k=0;k<3;k++){float d=captured_old[ids[j]][k]-captured_old[ids[(j+1)%3]][k];a+=d*d;d=captured_fixed[ids[j]][k]-captured_fixed[ids[(j+1)%3]][k];b+=d*d;}before=std::max(before,std::sqrt(a));after=std::max(after,std::sqrt(b));}
  long_before+=before>100;long_after+=after>100;max_after=std::max(max_after,after);
 }
 assert(long_before==438&&long_after==0&&max_after<14.15f);
 auto uv=captured_uv;uv[0][0]=0.5f;assert(!water_repair_positions(captured_raw,uv,old,captured_fixed,changed));
 uv=captured_uv;uv[0][0]=257;assert(!water_repair_positions(captured_raw,uv,old,captured_fixed,changed));
 uv=captured_uv;uv[0][0]=NAN;assert(!water_repair_positions(captured_raw,uv,old,captured_fixed,changed));
 auto raw=captured_raw;raw[65536][2]+=1;assert(!water_repair_positions(raw,captured_uv,old,captured_fixed,changed));
 assert(water_repair_positions(captured_raw,captured_uv,old,captured_fixed,changed));
 puts("PASS real water capture: 439 corrected positions, 438 stretched triangles removed, maximum repaired edge 14.142; fractional/out-of-range/nonfinite UV and nonflat grid rejected");
}
struct MockWater {
 uint32_t header[7]{};uint32_t elements[3][12]{};uint32_t stream[15]{};
 std::vector<Point3> positions;std::vector<Point2> uv;std::vector<uint16_t> posmap,uvmap;
 unsigned count=0,releases=0;const char* name="subset-NHoodWater";
 void bind(void** vt){
  header[0]=reinterpret_cast<uint32_t>(vt);header[1]=reinterpret_cast<uint32_t>(elements);header[2]=header[3]=header[1]+sizeof(elements);
  header[4]=reinterpret_cast<uint32_t>(stream);header[5]=header[6]=header[4]+sizeof(stream);
  uint32_t pos[12]={0,unsigned(positions.size()),0x5b830781,0,2,0,reinterpret_cast<uint32_t>(positions.data()),reinterpret_cast<uint32_t>(positions.data())+unsigned(positions.size()*12),reinterpret_cast<uint32_t>(positions.data())+unsigned(positions.size()*12)};
  uint32_t tex[12]={2,unsigned(uv.size()),0xbb8307ab,0,1,2,reinterpret_cast<uint32_t>(uv.data()),reinterpret_cast<uint32_t>(uv.data())+unsigned(uv.size()*8),reinterpret_cast<uint32_t>(uv.data())+unsigned(uv.size()*8)};
  std::memcpy(elements[0],pos,sizeof(pos));std::memcpy(elements[2],tex,sizeof(tex));
  stream[6]=reinterpret_cast<uint32_t>(posmap.data());stream[7]=stream[8]=stream[6]+posmap.size()*2;
  stream[12]=reinterpret_cast<uint32_t>(uvmap.data());stream[13]=stream[14]=stream[12]+uvmap.size()*2;
 }
};
static void* water_mock_vt[64]{};static MockWater water_mock_copy;static MockWater* iter_water=nullptr;
static unsigned water_mock_clones=0;static bool corrupt_water_copy=false;
static void* water_iter_vt[7]{};static FakeIterator water_begin{water_iter_vt},water_owner{water_iter_vt},water_end{water_iter_vt};
static bool __fastcall water_ended(void* self,void*,void*){return static_cast<FakeIterator*>(self)->at>=iter_water->count;}
static void* __fastcall water_point(void* self,void*,float* out){unsigned i=static_cast<FakeIterator*>(self)->at;assert(i<iter_water->posmap.size());auto p=iter_water->positions[iter_water->posmap[i]];std::memcpy(out,p.data(),12);return out;}
static void* __cdecl water_factory(void* out,void* geometry,unsigned stream,unsigned component){assert(stream==0&&component==0);iter_water=static_cast<MockWater*>(geometry);water_begin.at=0;auto tuple=static_cast<void**>(out);tuple[0]=&water_begin;tuple[1]=&water_owner;tuple[2]=&water_end;return out;}
static void* __fastcall water_info(void* self,void*,void* out,unsigned index){assert(index==0);uint32_t info[3]={0,3,static_cast<MockWater*>(self)->count};std::memcpy(out,info,sizeof(info));return out;}
static void* __fastcall water_group(void* self,void*,void* out,unsigned index){assert(index==0);uint32_t info[16]={0,2,3,0,0,reinterpret_cast<uint32_t>(static_cast<MockWater*>(self)->name)};std::memcpy(out,info,sizeof(info));return out;}
static unsigned __fastcall water_release(void* self,void*){return ++static_cast<MockWater*>(self)->releases;}
static bool __fastcall water_clone(void* self,void*,uint32_t iid,void** out){assert(iid==0x5bafb147);water_mock_copy=*static_cast<MockWater*>(self);water_mock_copy.bind(water_mock_vt);*out=&water_mock_copy;++water_mock_clones;return true;}
static void __fastcall water_set_array(void* self,void*,unsigned element,unsigned count,const void* data,unsigned bytes,const void* map,unsigned maps){assert(element==0&&count==captured_uv.size()&&bytes==count*12&&!map&&!maps);auto w=static_cast<MockWater*>(self);auto begin=static_cast<const Point3*>(data);w->positions.assign(begin,begin+count);if(corrupt_water_copy)w->positions[0][2]+=1;w->bind(water_mock_vt);}
static void __fastcall water_set_map(void* self,void*,unsigned stream,unsigned component,const uint16_t* ids,unsigned count){assert(stream==0&&component==0&&count==captured_uv.size());auto w=static_cast<MockWater*>(self);w->posmap.assign(ids,ids+count);w->bind(water_mock_vt);}
static void test_water_integration(){
 water_mock_vt[2]=reinterpret_cast<void*>(&water_release);water_mock_vt[0x9c/4]=water_mock_vt[0xb0/4]=reinterpret_cast<void*>(&fake_one);water_mock_vt[0xa0/4]=reinterpret_cast<void*>(&water_info);water_mock_vt[0xb4/4]=reinterpret_cast<void*>(&water_group);
 water_mock_vt[0xe0/4]=reinterpret_cast<void*>(&water_clone);water_mock_vt[0x30/4]=reinterpret_cast<void*>(&water_set_array);water_mock_vt[0x44/4]=reinterpret_cast<void*>(&water_set_map);
 water_iter_vt[2]=reinterpret_cast<void*>(&iter_release);water_iter_vt[4]=reinterpret_cast<void*>(&water_ended);water_iter_vt[5]=reinterpret_cast<void*>(&water_point);water_iter_vt[6]=reinterpret_cast<void*>(&iter_next);
 position_factory=&water_factory;validated_array_method=water_mock_vt[0x30/4];validated_map_method=water_mock_vt[0x44/4];validated_clone_method=water_mock_vt[0xe0/4];
 MockWater source;source.positions=captured_raw;source.uv=captured_uv;source.count=source.uv.size();source.posmap.resize(source.count);source.uvmap.resize(source.count);
 for(unsigned i=0;i<source.count;i++){source.posmap[i]=uint16_t(unsigned(source.uv[i][0])+257*unsigned(source.uv[i][1]));source.uvmap[i]=uint16_t(i);}source.bind(water_mock_vt);
 auto original_map=source.posmap;auto original_uv=source.uv;
 {WaterCopyOwner result{try_water_fix(&source)};assert(result.copy==&water_mock_copy);assert(water_mock_copy.positions==captured_fixed&&water_mock_copy.uv==original_uv);assert(source.positions==captured_raw&&source.posmap==original_map&&source.releases==0);}
 assert(water_mock_copy.releases==1&&water_mock_clones==1);
 {WaterCopyOwner result{try_water_fix(&source)};assert(result.copy);}assert(water_mock_clones==2&&water_mock_copy.releases==1);
 source.name="unrelated";assert(!try_water_fix(&source)&&water_mock_clones==2);source.name="subset-NHoodWater";
 source.uvmap[0]=1;assert(!try_water_fix(&source)&&water_mock_clones==2);source.uvmap[0]=0;
 source.posmap[0]=1;assert(!try_water_fix(&source)&&water_mock_clones==2);source.posmap[0]=0;
 corrupt_water_copy=true;assert(!try_water_fix(&source)&&water_mock_clones==3&&water_mock_copy.releases==1);corrupt_water_copy=false;
 assert(source.positions==captured_raw&&source.posmap==original_map&&source.uv==original_uv);
 puts("PASS water native-API integration: deep-clone position-only repair, original arrays/maps untouched, UV unchanged, repeated use, clone release and wrong subset/mapping/position/copy rejection");
}

