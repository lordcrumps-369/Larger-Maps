#include "strip_signatures.inc"
static bool strip_ready=false;
static void* strip_element=nullptr;static void* strip_stream=nullptr;static void* strip_assign=nullptr;
static void* strip_primitive=nullptr;static void* strip_indices=nullptr;
using TerrainEmit=uintptr_t(__thiscall*)(void*,void*,const int*);
static TerrainEmit strip_emit_original=nullptr;
struct StripGroup {unsigned original=0;uint32_t type=2,material=0xffffffff;std::string name;std::vector<uint16_t> main,edge;};
struct StripPlan {std::vector<StripGroup> groups;std::vector<unsigned> edge_points;unsigned triangles=0,edge_triangles=0;};
struct StripMaterial {uint32_t words[18]{};};
static bool strip_name(const void* address,std::string& name){
 name.clear();if(!address)return false;
 for(unsigned i=0;i<256;i++){char c=0;if(!safe_read(static_cast<const unsigned char*>(address)+i,&c,1))return false;if(!c)return !name.empty();name+=c;}
 return false;
}
static bool strip_vector(const uint32_t bounds[3],unsigned stride,unsigned maxbytes){
 return stride&&bounds[1]>=bounds[0]&&bounds[2]>=bounds[1]&&(bounds[1]-bounds[0])%stride==0&&bounds[1]-bounds[0]<=maxbytes&&(bounds[0]||bounds[0]==bounds[1]);
}
static bool strip_corners(unsigned key,std::array<unsigned,3>& ids){
 if(key>=256*256*2)return false;unsigned cell=key/2,x=cell%256,y=cell/256,base=y*257+x;
 ids=key%2?std::array<unsigned,3>{base,base+1,base+258}:std::array<unsigned,3>{base,base+258,base+257};return true;
}
static bool split_strip_group(const std::vector<uint32_t>& keys,const std::vector<uint16_t>& original,StripGroup& group,StripPlan& plan,std::vector<int>& remap){
 if(keys.size()*3!=original.size())return false;
 group.main.clear();group.edge.clear();
 for(unsigned i=0;i<keys.size();i++){
  std::array<unsigned,3> ids{};if(!strip_corners(keys[i],ids))return false;
  for(unsigned j=0;j<3;j++)if(original[i*3+j]!=uint16_t(ids[j]))return false;
  bool edge=std::any_of(ids.begin(),ids.end(),[](unsigned id){return id>65535;});
  if(edge){
   for(unsigned id:ids){if(remap[id]<0){if(plan.edge_points.size()>=65536)return false;remap[id]=int(plan.edge_points.size());plan.edge_points.push_back(id);}group.edge.push_back(uint16_t(remap[id]));}
   ++plan.edge_triangles;
  }else for(unsigned id:ids)group.main.push_back(uint16_t(id));
  ++plan.triangles;
 }
 return true;
}
static bool strip_read_indices(void* geometry,unsigned group,unsigned expected,std::vector<uint16_t>& out){
 auto getter=reinterpret_cast<TupleGetter>(native_method(geometry,0xb8/4));if(!getter||expected>2097152)return false;
 IteratorTuple it;getter(geometry,it.pointers,group);out.clear();out.reserve(expected);
 for(unsigned i=0;i<expected;i++){if(it.ended())return false;auto fn=it.method(5);if(!fn)return false;out.push_back(reinterpret_cast<uint16_t(__thiscall*)(void*)>(fn)(it.pointers[0]));it.next();}
 return it.ended();
}
static bool collect_strip_plan(void* geometry,void* renderer,StripPlan& plan){
 uint32_t table[3]{},offsets[6]{};constexpr uint32_t expected_offsets[6]={0,258,257,0,1,258};
 if(!safe_read(static_cast<unsigned char*>(renderer)+0xa0,table,sizeof(table))||!strip_vector(table,4,16384*4)||!safe_read(static_cast<unsigned char*>(renderer)+0x44,offsets,sizeof(offsets))||std::memcmp(offsets,expected_offsets,sizeof(offsets)))return false;
 std::vector<uint32_t> materials((table[1]-table[0])/4);if(materials.empty()||!safe_read(reinterpret_cast<void*>(table[0]),materials.data(),materials.size()*4))return false;
 std::unordered_map<std::string,std::vector<StripMaterial>> named;
 for(auto pointer:materials){if(!pointer)continue;StripMaterial m;std::string name;if(safe_read(reinterpret_cast<void*>(pointer),m.words,sizeof(m.words))&&strip_name(reinterpret_cast<void*>(m.words[3]),name))named[name].push_back(m);}
 unsigned groups=get_native_count(geometry,0xb0/4);auto info=reinterpret_cast<TupleGetter>(native_method(geometry,0xb4/4));if(!groups||groups>16384||!info)return false;
 std::vector<int> remap(66049,-1);unsigned total=0;
 for(unsigned g=0;g<groups;g++){
  uint32_t desc[16]{};info(geometry,desc,g);StripGroup group;group.original=g;group.type=desc[1];group.material=desc[4];
  if(desc[1]!=2||desc[3]!=0||desc[2]%3||desc[2]>2097152-total||!strip_name(reinterpret_cast<void*>(desc[5]),group.name))return false;
  total+=desc[2];std::vector<uint16_t> indices;if(!strip_read_indices(geometry,g,desc[2],indices))return false;
  auto found=named.find(group.name);if(found==named.end())return false;bool matched=false;
  for(const auto& m:found->second){
   const uint32_t* keys=m.words+12;const uint32_t* native_indices=m.words+15;
   if(!strip_vector(keys,4,131072*4)||!strip_vector(native_indices,2,131072*6)||(keys[1]-keys[0])/4*3!=indices.size()||(native_indices[1]-native_indices[0])/2!=indices.size())continue;
   std::vector<uint16_t> native(indices.size());if(!native.empty()&&(!safe_read(reinterpret_cast<void*>(native_indices[0]),native.data(),native.size()*2)||native!=indices))continue;
   std::vector<uint32_t> triangles((keys[1]-keys[0])/4);if(!triangles.empty()&&!safe_read(reinterpret_cast<void*>(keys[0]),triangles.data(),triangles.size()*4))continue;
   // Only a verified native material/triangle-key match can introduce edge faces.
   if(!split_strip_group(triangles,indices,group,plan,remap))return false;
   matched=true;break;
  }
  if(!matched)return false;plan.groups.push_back(std::move(group));
 }
 return true;
}
template<class T>static bool strip_renderer_array(void* renderer,unsigned offset,std::vector<T>& out){
 uint32_t bounds[3]{};if(!safe_read(static_cast<unsigned char*>(renderer)+offset,bounds,sizeof(bounds)))return false;
 out.resize(66049);return read_water_span(bounds,sizeof(T),66049,out.data());
}
static unsigned strip_stream_points(void* geometry,unsigned stream){
 auto info=reinterpret_cast<TupleGetter>(native_method(geometry,0xa0/4));if(!info)return 0;uint32_t desc[16]{};info(geometry,desc,stream);return desc[2];
}
static bool strip_position_match(void* geometry,unsigned stream,const std::vector<Point3>& expected){
 IteratorTuple it;position_factory(it.pointers,geometry,stream,0);unsigned count=0;
 while(!it.ended()&&count<expected.size()){Point3 point{};auto fn=it.method(5);if(!fn)return false;reinterpret_cast<void*(__thiscall*)(void*,float*)>(fn)(it.pointers[0],point.data());if(point!=expected[count])return false;++count;it.next();}
 return count==expected.size()&&it.ended();
}
static bool prepare_split_terrain(void* source,void* copy,void* renderer,unsigned phase) try {
 StripPlan plan;if(!collect_strip_plan(source,renderer,plan)){return false;}
 if(native_method(copy,0x24/4)!=strip_element||native_method(copy,0x34/4)!=strip_stream||native_method(copy,0x38/4)!=strip_assign||native_method(copy,0x50/4)!=strip_primitive||native_method(copy,0x54/4)!=strip_indices||native_method(copy,0x30/4)!=validated_array_method||get_native_count(copy,0x7c/4)!=3)return false;
 std::vector<Point3> positions,normals;std::vector<Point2> uv;
 if(!strip_renderer_array(renderer,0x7c,positions)||!strip_renderer_array(renderer,0x88,normals)||!strip_renderer_array(renderer,0x94,uv)||!prepare_explicit_mesh(copy,renderer))return false;
 if(plan.edge_points.empty()){return true;}
 unsigned edge_count=plan.edge_points.size();std::vector<Point3> edge_positions,edge_normals;std::vector<Point2> edge_uv;edge_positions.reserve(edge_count);edge_normals.reserve(edge_count);edge_uv.reserve(edge_count);
 for(unsigned id:plan.edge_points){edge_positions.push_back(positions[id]);edge_normals.push_back(normals[id]);edge_uv.push_back(uv[id]);}
 reinterpret_cast<void(__thiscall*)(void*)>(strip_stream)(copy);
 if(get_native_count(copy,0x9c/4)!=2)return false;
 reinterpret_cast<void(__thiscall*)(void*,unsigned,unsigned)>(validated_count_method)(copy,1,edge_count);
 const uint32_t semantics[3]={0x5b830781,0x3b83078b,0xbb8307ab};const unsigned formats[3]={2,2,1};const unsigned strides[3]={12,12,8};const void* arrays[3]={edge_positions.data(),edge_normals.data(),edge_uv.data()};
 for(unsigned component=0;component<3;component++){
  unsigned before=get_native_count(copy,0x7c/4);
  unsigned id=reinterpret_cast<unsigned(__thiscall*)(void*,uint32_t,unsigned,unsigned,unsigned)>(strip_element)(copy,semantics[component],0,formats[component],component);
  if(id!=before||get_native_count(copy,0x7c/4)!=before+1)return false;
  reinterpret_cast<void(__thiscall*)(void*,unsigned,unsigned,const void*,unsigned,const void*,unsigned)>(validated_array_method)(copy,id,edge_count,arrays[component],edge_count*strides[component],nullptr,0);
  reinterpret_cast<void(__thiscall*)(void*,unsigned,unsigned)>(strip_assign)(copy,1,id);
 }
 std::vector<uint16_t> identity(edge_count);for(unsigned i=0;i<edge_count;i++)identity[i]=uint16_t(i);
 for(unsigned component=0;component<3;component++)reinterpret_cast<void(__thiscall*)(void*,unsigned,unsigned,const uint16_t*,unsigned)>(validated_map_method)(copy,1,component,identity.data(),edge_count);
 if(strip_stream_points(copy,0)!=65536||strip_stream_points(copy,1)!=edge_count||!validate_position_sequence(copy,renderer)||!strip_position_match(copy,1,edge_positions))return false;
 for(const auto& group:plan.groups){
  reinterpret_cast<void(__thiscall*)(void*,unsigned,const uint16_t*,unsigned)>(strip_indices)(copy,group.original,group.main.data(),group.main.size());
  if(!group.edge.empty()){
   unsigned index=get_native_count(copy,0xb0/4);
   reinterpret_cast<void(__thiscall*)(void*,unsigned,const char*,unsigned,uint32_t)>(strip_primitive)(copy,group.type,group.name.c_str(),1,group.material);
   if(get_native_count(copy,0xb0/4)!=index+1)return false;
   reinterpret_cast<void(__thiscall*)(void*,unsigned,const uint16_t*,unsigned)>(strip_indices)(copy,index,group.edge.data(),group.edge.size());
   std::vector<uint16_t> verify;if(!strip_read_indices(copy,index,group.edge.size(),verify)||verify!=group.edge)return false;
  }
  std::vector<uint16_t> verify;if(!strip_read_indices(copy,group.original,group.main.size(),verify)||verify!=group.main)return false;
 }
 
 return true;
}catch(...){return false;}
static bool valid_prepared_terrain(void* copy,void* renderer){
 unsigned streams=get_native_count(copy,0x9c/4);if(streams<1||streams>2||strip_stream_points(copy,0)!=65536)return false;
 if(streams==2){unsigned count=strip_stream_points(copy,1);if(!count||count>65536)return false;}
 return validate_position_sequence(copy,renderer);
}
static uintptr_t __fastcall strip_emit_hook(void* self,void*,void* material,const int* cell){
 uint32_t dims[2]{};int c[3]{};
 if(!strip_ready&&safe_read(static_cast<unsigned char*>(self)+0x24,dims,sizeof(dims))&&dims[0]==257&&dims[1]==257&&safe_read(cell,c,sizeof(c))&&c[0]>=0&&c[0]<256&&c[1]>=0&&c[1]<256&&c[2]>=0&&c[2]<2){
  std::array<unsigned,3> ids{};strip_corners((unsigned(c[1])*256+c[0])*2+c[2],ids);if(std::any_of(ids.begin(),ids.end(),[](unsigned id){return id>65535;}))return 0;
 }
 return strip_emit_original(self,material,cell);
}
static void install_terrain_strip(){
 strip_element=resolve_signature(strip_element_signature,sizeof(strip_element_signature),0xf8,16,false);
 strip_stream=resolve_signature(strip_stream_signature,sizeof(strip_stream_signature),0xde,0,false);
 strip_assign=resolve_signature(strip_assign_signature,sizeof(strip_assign_signature),0x3c,8,false);
 strip_primitive=resolve_signature(strip_primitive_signature,sizeof(strip_primitive_signature),0xfd,16,false);
 strip_indices=resolve_signature(strip_indices_signature,sizeof(strip_indices_signature),0x45,12,false);
 void* emitter=resolve_signature(strip_emit_signature,sizeof(strip_emit_signature),0xdd,8,false);
 bool ready=strip_element&&strip_stream&&strip_assign&&strip_primitive&&strip_indices&&validated_array_method&&hood_original&&convert_original;
 if(!emitter){return;}
 MH_STATUS status=MH_CreateHook(emitter,reinterpret_cast<void*>(&strip_emit_hook),reinterpret_cast<void**>(&strip_emit_original));
 if(status==MH_OK){strip_ready=ready;status=MH_EnableHook(emitter);if(status!=MH_OK)strip_ready=false;}
 
}
