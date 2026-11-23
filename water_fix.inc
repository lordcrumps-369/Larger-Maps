// Compact only the verified neighborhood-water position channel. Other channels,
// triangle indices and materials remain as copied by the native deep clone.
static constexpr unsigned char array_signature[]={0x8b,0x44,0x24,0x4,0x53,0x56,0x8d,0x34,0x40,0x8b,0x44,0x24,0x10,0x57,0x8b,0x79,0x4,0xc1,0xe6,0x4,0x3,0xf7,0x8d,0x7e,0x18,0x89,0x46,0x4,0x8b,0x47,0x4,0x3b,0xc0,0x8b,0xf,0x75,0x4,0x8b,0xc1,0xeb};
static void* validated_array_method=nullptr;
using Point3=std::array<float,3>;
using Point2=std::array<float,2>;
struct WaterCopyOwner {
 void* copy=nullptr;
 ~WaterCopyOwner(){if(copy)release_native(copy);}
};
static bool read_water_span(const uint32_t bounds[3],unsigned stride,unsigned count,void* out){
 size_t bytes=size_t(stride)*count;
 return bytes&&bytes<=8*1024*1024&&bounds[0]&&bounds[1]>=bounds[0]&&bounds[2]>=bounds[1]&&bounds[1]-bounds[0]==bytes&&safe_read(reinterpret_cast<void*>(bounds[0]),out,bytes);
}
static bool water_uv_identity(void* geometry,unsigned count){
 uint32_t streams[3]{};if(!safe_read(static_cast<unsigned char*>(geometry)+0x10,streams,sizeof(streams))||!streams[0]||streams[1]<streams[0]||streams[2]<streams[1]||streams[1]-streams[0]!=60)return false;
 uint32_t uv[3]{};if(!safe_read(reinterpret_cast<void*>(streams[0]+0x30),uv,sizeof(uv)))return false;
 if(uv[0]==uv[1])return true; // Native implicit identity is valid below 65536.
 std::vector<uint16_t> map(count);if(!read_water_span(uv,2,count,map.data()))return false;
 for(unsigned i=0;i<count;i++)if(map[i]!=i)return false;
 return true;
}
static bool read_water_arrays(void* geometry,unsigned count,std::vector<Point3>& raw,std::vector<Point2>& uv){
 uint32_t bounds[3]{};if(!safe_read(static_cast<unsigned char*>(geometry)+4,bounds,sizeof(bounds))||!bounds[0]||bounds[1]<bounds[0]||bounds[2]<bounds[1]||bounds[1]-bounds[0]<3*48||bounds[1]-bounds[0]>32*48)return false;
 uint32_t pos[12]{},tex[12]{};
 if(!safe_read(reinterpret_cast<void*>(bounds[0]),pos,sizeof(pos))||!safe_read(reinterpret_cast<void*>(bounds[0]+96),tex,sizeof(tex)))return false;
 if(pos[0]!=0||pos[1]!=66049||pos[2]!=0x5b830781||pos[3]!=0||pos[4]!=2||pos[5]!=0)return false;
 if(tex[0]!=2||tex[1]!=count||tex[2]!=0xbb8307ab||tex[3]!=0||tex[4]!=1||tex[5]!=2)return false;
 raw.resize(66049);uv.resize(count);
 return read_water_span(pos+6,12,66049,raw.data())&&read_water_span(tex+6,8,count,uv.data())&&water_uv_identity(geometry,count);
}
static bool water_repair_positions(const std::vector<Point3>& raw,const std::vector<Point2>& uv,std::vector<Point3>& expected_old,std::vector<Point3>& corrected,unsigned& changed){
 changed=0;if(raw.size()!=66049||uv.empty()||uv.size()>65536||!std::isfinite(raw[0][2]))return false;
 for(unsigned i=0;i<raw.size();i++)if(raw[i]!=Point3{float((i%257)*10),float((i/257)*10),raw[0][2]})return false;
 expected_old.resize(uv.size());corrected.resize(uv.size());
 for(unsigned i=0;i<uv.size();i++){
  float x=uv[i][0],y=uv[i][1];if(!std::isfinite(x)||!std::isfinite(y)||x<0||x>256||y<0||y>256||x!=std::floor(x)||y!=std::floor(y))return false;
  unsigned index=unsigned(x)+257*unsigned(y);expected_old[i]=raw[index&65535];corrected[i]=raw[index];changed+=expected_old[i]!=corrected[i];
 }
 return true;
}
static bool match_water_positions(void* geometry,const std::vector<Point3>& expected){
 IteratorTuple it;position_factory(it.pointers,geometry,0,0);unsigned count=0;
 while(!it.ended()&&count<expected.size()){
  Point3 value{};auto fn=it.method(5);if(!fn)return false;
  reinterpret_cast<void*(__thiscall*)(void*,float*)>(fn)(it.pointers[0],value.data());if(value!=expected[count])return false;
  ++count;it.next();
 }
 return count==expected.size()&&it.ended();
}
static bool is_neighborhood_water(void* geometry){
 if(get_native_count(geometry,0xb0/4)!=1)return false;
 auto info=reinterpret_cast<TupleGetter>(native_method(geometry,0xb4/4));if(!info)return false;
 uint32_t desc[16]{};info(geometry,desc,0);constexpr char wanted[]="subset-NHoodWater";char name[sizeof(wanted)]{};
 return desc[1]==2&&desc[3]==0&&desc[5]&&safe_read(reinterpret_cast<void*>(desc[5]),name,sizeof(name))&&!std::memcmp(name,wanted,sizeof(name));
}
static void* try_water_fix(void* geometry) try {
 if(!position_factory||!validated_array_method||!validated_map_method||!validated_clone_method||!is_neighborhood_water(geometry))return nullptr;
 unsigned count=source_point_count(geometry);if(!count||count>65536){return nullptr;}
 std::vector<Point3> raw,old_positions,corrected;std::vector<Point2> uv;unsigned changed=0;
 if(!read_water_arrays(geometry,count,raw,uv))return nullptr;
 if(!water_repair_positions(raw,uv,old_positions,corrected,changed)||!changed||!match_water_positions(geometry,old_positions)){return nullptr;}
 if(native_method(geometry,0xe0/4)!=validated_clone_method)return nullptr;
 WaterCopyOwner owned;bool cloned=reinterpret_cast<CloneMesh>(validated_clone_method)(geometry,0x5bafb147,&owned.copy);
 if(!cloned||!owned.copy||native_method(owned.copy,0x30/4)!=validated_array_method||native_method(owned.copy,0x44/4)!=validated_map_method)return nullptr;
 using SetArray=void(__thiscall*)(void*,unsigned,unsigned,const void*,unsigned,const void*,unsigned);
 reinterpret_cast<SetArray>(validated_array_method)(owned.copy,0,count,corrected.data(),count*12,nullptr,0);
 std::vector<uint16_t> identity(count);for(unsigned i=0;i<count;i++)identity[i]=uint16_t(i);
 reinterpret_cast<void(__thiscall*)(void*,unsigned,unsigned,const uint16_t*,unsigned)>(validated_map_method)(owned.copy,0,0,identity.data(),count);
 if(source_point_count(owned.copy)!=count||!water_uv_identity(owned.copy,count)||!match_water_positions(owned.copy,corrected)){return nullptr;}
 
 void* result=owned.copy;owned.copy=nullptr;return result;
}catch(...){return nullptr;}
static void install_water_fix(){
 if(convert_original)validated_array_method=resolve_signature(array_signature,sizeof(array_signature),0xb5,24,false);
 
}
