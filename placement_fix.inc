// Native GMDC collision objects are independent COM geometry resources, not
// indices into the replacement render streams. Native code retains references
// through +0x68 (global) and +0x6c (per-group). The builder clone precedes later
// native processing, so refresh these objects immediately before conversion.
#include "placement_signatures.inc"
using GlobalCollision=void*(__thiscall*)(void*);
using GroupCollision=void*(__thiscall*)(void*,unsigned);
using SetGlobalCollision=void(__thiscall*)(void*,void*);
using SetGroupCollision=void(__thiscall*)(void*,unsigned,void*);
static void* placement_set_global=nullptr;
static void* placement_set_group=nullptr;
static void* placement_get_group=nullptr;
static bool placement_ready=false;
static bool valid_global_getter(void* object){
 void* getter=native_method(object,0xd4/4);unsigned char code[4]{};
 constexpr unsigned char wanted[]={0x8b,0x41,0x4c,0xc3};
 return getter&&safe_read(getter,code,sizeof(code))&&!std::memcmp(code,wanted,sizeof(code));
}
struct PlacementBinding {unsigned target;void* collision;};
static bool synchronize_placement(void* source,void* copy,unsigned phase) try {
 if(!placement_ready||!valid_global_getter(source)||!valid_global_getter(copy)||
    native_method(source,0xd8/4)!=placement_get_group||native_method(copy,0xd8/4)!=placement_get_group||
    native_method(copy,0x68/4)!=placement_set_global||native_method(copy,0x6c/4)!=placement_set_group)return false;
 auto get_global=reinterpret_cast<GlobalCollision>(native_method(source,0xd4/4));
 auto copy_global=reinterpret_cast<GlobalCollision>(native_method(copy,0xd4/4));
 void* desired=get_global(source);
 auto get_group=reinterpret_cast<GroupCollision>(placement_get_group);
 auto source_info=reinterpret_cast<TupleGetter>(native_method(source,0xb4/4));
 auto copy_info=reinterpret_cast<TupleGetter>(native_method(copy,0xb4/4));
 unsigned sources=get_native_count(source,0xb0/4),targets=get_native_count(copy,0xb0/4);
 if(!source_info||!copy_info||!sources||sources>16384||targets<sources||targets>sources*2)return false;
 std::unordered_map<std::string,void*> by_name;
 for(unsigned g=0;g<sources;g++){
  uint32_t desc[16]{};std::string name;source_info(source,desc,g);
  if(!strip_name(reinterpret_cast<void*>(desc[5]),name))return false;
  void* collision=get_group(source,g);
  auto found=by_name.find(name);if(found!=by_name.end()&&found->second!=collision)return false;
  by_name[name]=collision;
 }
 std::vector<PlacementBinding> bindings;bindings.reserve(targets);
 for(unsigned g=0;g<targets;g++){
  uint32_t desc[16]{};std::string name;copy_info(copy,desc,g);
  if(!strip_name(reinterpret_cast<void*>(desc[5]),name))return false;
  auto found=by_name.find(name);if(found==by_name.end())return false;
  bindings.push_back({g,found->second});
 }
 // All source/target names validated before altering the private copy.
 reinterpret_cast<SetGlobalCollision>(placement_set_global)(copy,desired);
 for(auto binding:bindings)reinterpret_cast<SetGroupCollision>(placement_set_group)(copy,binding.target,binding.collision);
 bool ok=copy_global(copy)==desired;unsigned populated=0;
 for(auto binding:bindings){ok=ok&&get_group(copy,binding.target)==binding.collision;populated+=binding.collision!=nullptr;}
 return ok;
}catch(...){return false;}
static void install_placement_fix(){
 placement_set_global=resolve_signature(global_set_signature,sizeof(global_set_signature),0x2c,4,false);
 placement_set_group=resolve_signature(group_set_signature,sizeof(group_set_signature),0x4c,8,false);
 placement_get_group=resolve_signature(group_get_signature,sizeof(group_get_signature),0x4a,4,false);
 placement_ready=placement_set_global&&placement_set_group&&placement_get_group;
}
