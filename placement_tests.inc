struct ColliderMock {void** vt;unsigned refs=1;};
static unsigned __fastcall collider_add(void* p,void*){return ++static_cast<ColliderMock*>(p)->refs;}
static unsigned __fastcall collider_release(void* p,void*){auto c=static_cast<ColliderMock*>(p);assert(c->refs);return --c->refs;}
struct PickingMock {
 void** vt;unsigned char padding[0x4c-sizeof(void*)]{};void* global=nullptr;
 std::vector<std::string> names;std::vector<void*> collision;
};
static void retain_collider(void* p){if(p)reinterpret_cast<unsigned(__thiscall*)(void*)>(native_method(p,1))(p);}
static void drop_collider(void* p){if(p)release_native(p);}
static unsigned __fastcall picking_groups(void* p,void*){return static_cast<PickingMock*>(p)->names.size();}
static void* __fastcall picking_info(void* p,void*,void* out,unsigned group){auto g=static_cast<PickingMock*>(p);uint32_t desc[16]{};desc[1]=2;desc[3]=group<2?0:1;desc[5]=reinterpret_cast<uint32_t>(g->names[group].c_str());std::memcpy(out,desc,sizeof(desc));return out;}
static void* __fastcall picking_get_group(void* p,void*,unsigned group){auto g=static_cast<PickingMock*>(p);return group<g->collision.size()?g->collision[group]:nullptr;}
static void __fastcall picking_set_global(void* p,void*,void* target){auto g=static_cast<PickingMock*>(p);if(g->global==target)return;retain_collider(target);drop_collider(g->global);g->global=target;}
static void __fastcall picking_set_group(void* p,void*,unsigned group,void* target){auto g=static_cast<PickingMock*>(p);if(g->collision.size()<=group)g->collision.resize(group+1);if(g->collision[group]==target)return;retain_collider(target);drop_collider(g->collision[group]);g->collision[group]=target;}
static void test_placement_sync(){
 static_assert(offsetof(PickingMock,global)==0x4c,"match checked native getter ABI");
 void* vt[64]{};void* cvt[3]={nullptr,reinterpret_cast<void*>(&collider_add),reinterpret_cast<void*>(&collider_release)};
 auto getter=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));assert(getter);
 const unsigned char code[]={0x8b,0x41,0x4c,0xc3};std::memcpy(getter,code,4);FlushInstructionCache(GetCurrentProcess(),getter,4);
 vt[0xd4/4]=getter;vt[0xd8/4]=reinterpret_cast<void*>(&picking_get_group);vt[0xb0/4]=reinterpret_cast<void*>(&picking_groups);vt[0xb4/4]=reinterpret_cast<void*>(&picking_info);vt[0x68/4]=reinterpret_cast<void*>(&picking_set_global);vt[0x6c/4]=reinterpret_cast<void*>(&picking_set_group);
 placement_set_global=vt[0x68/4];placement_set_group=vt[0x6c/4];placement_get_group=vt[0xd8/4];placement_ready=true;
 ColliderMock old{cvt},global{cvt},land{cvt},beach{cvt};
 PickingMock original{vt},copy{vt};original.names={"land","beach"};original.collision={&land,&beach};original.global=&global;copy.names={"land","beach","land"};copy.collision={&old,nullptr};copy.global=&old;old.refs=2;
 assert(synchronize_placement(&original,&copy,0));assert(copy.global==&global&&copy.collision==std::vector<void*>({&land,&beach,&land}));
 assert(old.refs==0&&global.refs==2&&land.refs==3&&beach.refs==2);assert(original.global==&global&&original.collision==std::vector<void*>({&land,&beach}));
 assert(synchronize_placement(&original,&copy,0));assert(global.refs==2&&land.refs==3&&beach.refs==2);
 copy.names.back()="unknown";assert(!synchronize_placement(&original,&copy,1));assert(global.refs==2&&land.refs==3&&beach.refs==2);copy.names.back()="land";
 original.global=nullptr;original.collision={nullptr,nullptr};assert(synchronize_placement(&original,&copy,2));assert(!copy.global&&std::all_of(copy.collision.begin(),copy.collision.end(),[](void* p){return !p;}));assert(global.refs==1&&land.refs==1&&beach.refs==1);
 auto save=vt[0x68/4];vt[0x68/4]=nullptr;assert(!synchronize_placement(&original,&copy,2));vt[0x68/4]=save;
 placement_ready=false;VirtualFree(getter,0,MEM_RELEASE);
 puts("PASS late picking bindings: original untouched, global and per-group collision refreshed, boundary group matches material name, native retain/release balanced, repeated use idempotent, missing names/methods rejected before mutation, stale collisions cleared");
}
