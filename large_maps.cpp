#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <MinHook.h>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <atomic>
#include <mutex>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

static LONG scheduled=0;
static size_t unique_pattern(const unsigned char* data,size_t size,const unsigned char* pattern,size_t length,size_t* matches=nullptr) {
    size_t found=SIZE_MAX,count=0;if(matches)*matches=0;
    if(!length||size<length)return SIZE_MAX;
    for(size_t i=0;i<=size-length;i++)if(!std::memcmp(data+i,pattern,length)){++count;found=i;}
    if(matches)*matches=count;return count==1?found:SIZE_MAX;
}
static void* resolve_signature(const unsigned char* pattern,size_t pattern_length,size_t ret_offset,unsigned ret_pop,bool emitter) {
    wchar_t path[MAX_PATH];if(!GetModuleFileNameW(nullptr,path,MAX_PATH))return nullptr;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(f==INVALID_HANDLE_VALUE)return nullptr;
    LARGE_INTEGER size{};if(!GetFileSizeEx(f,&size)||size.QuadPart<256||size.QuadPart>512*1024*1024){CloseHandle(f);return nullptr;}
    std::vector<unsigned char> data(static_cast<size_t>(size.QuadPart));DWORD got=0;
    bool read=ReadFile(f,data.data(),DWORD(data.size()),&got,nullptr)&&got==data.size();CloseHandle(f);if(!read)return nullptr;
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(data.data());
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||size_t(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS32)>data.size())return nullptr;
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(data.data()+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC)return nullptr;
    size_t section_offset=size_t(dos->e_lfanew)+24+nt->FileHeader.SizeOfOptionalHeader;
    if(section_offset+size_t(nt->FileHeader.NumberOfSections)*sizeof(IMAGE_SECTION_HEADER)>data.size())return nullptr;
    auto sections=reinterpret_cast<const IMAGE_SECTION_HEADER*>(data.data()+section_offset);
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));void* result=nullptr;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++){
        const auto& s=sections[i];if(!(s.Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
        if(size_t(s.PointerToRawData)+s.SizeOfRawData>data.size())return nullptr;
        size_t matches=0,at=unique_pattern(data.data()+s.PointerToRawData,s.SizeOfRawData,pattern,pattern_length,&matches);
        if(matches>1)return nullptr;
        if(at==SIZE_MAX)continue;
        if(result)return nullptr;
        size_t rva=size_t(s.VirtualAddress)+at;
        if(at+pattern_length>s.Misc.VirtualSize||rva+pattern_length>nt->OptionalHeader.SizeOfImage)return nullptr;
        auto candidate=base+rva;MEMORY_BASIC_INFORMATION mbi{};
        if(!VirtualQuery(candidate,&mbi,sizeof(mbi))||mbi.State!=MEM_COMMIT||(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!mbi.Protect)return nullptr;
        if(reinterpret_cast<uintptr_t>(candidate)+pattern_length>reinterpret_cast<uintptr_t>(mbi.BaseAddress)+mbi.RegionSize)return nullptr;
        if(std::memcmp(candidate,pattern,pattern_length))return nullptr;
        if(at+ret_offset+3>s.SizeOfRawData||at+ret_offset+3>s.Misc.VirtualSize)return nullptr;
        if(reinterpret_cast<uintptr_t>(candidate)+ret_offset+3>reinterpret_cast<uintptr_t>(mbi.BaseAddress)+mbi.RegionSize)return nullptr;
        const unsigned char* code=data.data()+s.PointerToRawData+at;
        if(ret_pop?(code[ret_offset]!=0xc2||code[ret_offset+1]!=ret_pop||code[ret_offset+2]!=0):(code[ret_offset]!=0xc3))return nullptr;
        if(std::memcmp(candidate+ret_offset,code+ret_offset,ret_pop?3:1))return nullptr;
        result=candidate;
    }
    return result;
}


static constexpr unsigned char hood_signature[]={0x6a,0xff,0x64,0xa1,0x0,0x0,0x0,0x0,0x68,0x38,0xfc,0x18,0x1,0x50,0x64,0x89,0x25,0x0,0x0,0x0,0x0,0x83,0xec,0x14,0x53,0x55,0x56,0x8b,0x74,0x24,0x30,0x8b,0x6,0x57,0x6a,0x0,0x6a,0x2,0x6a,0x0};



static constexpr unsigned char convert_signature[]={0x55,0x8b,0xec,0x6a,0xff,0x68,0xa3,0xeb,0x1a,0x1,0x64,0xa1,0x0,0x0,0x0,0x0,0x50,0x64,0x89,0x25,0x0,0x0,0x0,0x0,0x81,0xec,0xc,0x2,0x0,0x0,0x53,0x56,0x57,0x8b,0xf9,0x89,0x65,0xf0,0x89,0x7d};

static constexpr unsigned char count_signature[]={0x8b,0x44,0x24,0x8,0x56,0x8b,0x74,0x24,0x8,0x6b,0xf6,0x3c,0x57,0x3,0x71,0x10,0x89,0x46,0x8,0x8b,0x46,0x1c,0x3b,0xc0,0x8b,0x4e,0x18,0x75,0x4,0x8b,0xc1,0xeb,0x11,0x8b,0xf8,0x2b,0xf8,0x57,0x50,0x51};
static constexpr unsigned char map_signature[]={0x8b,0x44,0x24,0x8,0x83,0xe8,0x0,0x53,0x56,0x57,0x8b,0xf1,0xf,0x84,0x8c,0x0,0x0,0x0,0x48,0x74,0x4a,0x48,0xf,0x85,0xbb,0x0,0x0,0x0,0x8b,0x7c,0x24,0x10,0x8b,0x46,0x10,0x6b,0xff,0x3c,0x8b,0x5c};
static constexpr unsigned char clone_signature[]={0x6a,0xff,0x68,0x4b,0x3e,0x1b,0x1,0x64,0xa1,0x0,0x0,0x0,0x0,0x50,0x64,0x89,0x25,0x0,0x0,0x0,0x0,0x51,0x56,0x57,0x68,0x84,0x0,0x0,0x0,0x8b,0xf9,0xe8,0x87,0xa0,0xec,0xff,0x83,0xc4,0x4,0x89};
static constexpr unsigned char position_factory_signature[]={0x6a,0xff,0x68,0xaf,0x8b,0x1a,0x1,0x64,0xa1,0x0,0x0,0x0,0x0,0x50,0x64,0x89,0x25,0x0,0x0,0x0,0x0,0x83,0xec,0x4c,0x8b,0x44,0x24,0x68,0x55,0x56,0x57,0x8b,0x7c,0x24,0x70,0x50,0xc7,0x44,0x24,0x10};
struct TerrainTag {void* geometry=nullptr;void* renderer=nullptr;unsigned phase=0;};
static std::mutex tags_mutex;static std::array<TerrainTag,64> terrain_tags{};
static unsigned tag_cursor=0;
#ifdef TS2_UNIT_TESTS
static unsigned memory_read_calls=0;
#endif
static bool safe_read(const void* p,void* out,size_t n){
#ifdef TS2_UNIT_TESTS
 ++memory_read_calls;
#endif
 SIZE_T got=0;return n&&ReadProcessMemory(GetCurrentProcess(),p,out,n,&got)&&got==n;}
static bool terrain_owner(void* self,void*& renderer,unsigned& phase){
 uint32_t owner=0;phase=99;
 if(!safe_read(static_cast<unsigned char*>(self)-8,&owner,4)||!safe_read(static_cast<unsigned char*>(self)-12,&phase,4)||phase>3||!owner)return false;
 uint32_t dims[2]{};if(!safe_read(reinterpret_cast<void*>(owner+0x24),dims,8)||dims[0]!=257||dims[1]!=257)return false;
 renderer=reinterpret_cast<void*>(owner);return true;
}
static TerrainTag get_tag(void* geometry){std::lock_guard<std::mutex> lock(tags_mutex);for(auto t:terrain_tags)if(t.geometry==geometry)return t;return {};}
static void set_tag(void* geometry,void* renderer,unsigned phase,bool valid){
 std::lock_guard<std::mutex> lock(tags_mutex);
 for(auto& t:terrain_tags)if(t.geometry==geometry){t=valid?TerrainTag{geometry,renderer,phase}:TerrainTag{};return;}
 if(valid)terrain_tags[tag_cursor++%terrain_tags.size()]={geometry,renderer,phase};
}
using NativePass=void(__thiscall*)(void*,void*);
static NativePass hood_original=nullptr,convert_original=nullptr;
static void __fastcall hood_hook(void* self,void*,void* geometry);
using NativeCount=unsigned(__thiscall*)(void*);
static void* native_method(void* object,unsigned slot){void** vt=nullptr;void* fn=nullptr;if(!object||!safe_read(object,&vt,sizeof(vt))||!vt||!safe_read(vt+slot,&fn,sizeof(fn)))return nullptr;return fn;}
static unsigned get_native_count(void* object,unsigned slot){auto fn=native_method(object,slot);return fn?reinterpret_cast<NativeCount>(fn)(object):0;}
using TupleGetter=void*(__thiscall*)(void*,void*,unsigned);
using PositionFactory=void*(__cdecl*)(void*,void*,unsigned,unsigned);
static PositionFactory position_factory=nullptr;
// A native iterator retains its own COM object until this tuple is destroyed.
// Resolve methods locally once; never cache iterator objects across builds.
struct IteratorTuple {
 void* pointers[4]{};
 void* resolved_object=nullptr;
 void* methods[3]{};
 void* method(unsigned slot){
  if(slot<4||slot>6||!pointers[0])return nullptr;
  if(resolved_object!=pointers[0]){
   for(unsigned i=0;i<3;i++)methods[i]=native_method(pointers[0],i+4);
   resolved_object=pointers[0];
  }
  return methods[slot-4];
 }
 ~IteratorTuple(){for(int i=2;i>=0;--i)if(pointers[i]){auto fn=native_method(pointers[i],2);if(fn)reinterpret_cast<unsigned(__thiscall*)(void*)>(fn)(pointers[i]);}}
 bool ended(){auto fn=method(4);return !fn||reinterpret_cast<bool(__thiscall*)(void*,void*)>(fn)(pointers[0],pointers[2]);}
 void next(){auto fn=method(6);if(fn)reinterpret_cast<void(__thiscall*)(void*)>(fn)(pointers[0]);}
};

using CloneMesh=bool(__thiscall*)(void*,uint32_t,void**);
static void* validated_clone_method=nullptr;static void* validated_count_method=nullptr;static void* validated_map_method=nullptr;
struct PreservedMesh {void* geometry=nullptr;void* copy=nullptr;unsigned expected=0;unsigned built=0;};
static std::array<PreservedMesh,64> preserved_meshes{};static unsigned preserved_cursor=0;static std::mutex preserved_mutex;
static void release_native(void* p){auto fn=native_method(p,2);if(fn)reinterpret_cast<unsigned(__thiscall*)(void*)>(fn)(p);}
static void save_preserved(PreservedMesh entry){
 PreservedMesh displaced{};
 {std::lock_guard<std::mutex> lock(preserved_mutex);PreservedMesh* slot=nullptr;
  for(auto& item:preserved_meshes)if(item.geometry==entry.geometry){slot=&item;break;}
  if(!slot)for(auto& item:preserved_meshes)if(!item.geometry){slot=&item;break;}
  if(!slot)slot=&preserved_meshes[preserved_cursor++%preserved_meshes.size()];
  displaced=*slot;*slot=entry;
 }
 if(displaced.copy){release_native(displaced.copy);}
}
static PreservedMesh take_preserved(void* geometry){
 std::lock_guard<std::mutex> lock(preserved_mutex);for(auto& item:preserved_meshes)if(item.geometry==geometry){auto out=item;item={};return out;}return {};
}
static bool has_preserved(void* geometry){std::lock_guard<std::mutex> lock(preserved_mutex);for(auto& item:preserved_meshes)if(item.geometry==geometry)return true;return false;}
static unsigned source_point_count(void* geometry){
 if(get_native_count(geometry,0x9c/4)!=1)return 0;auto fn=native_method(geometry,0xa0/4);if(!fn)return 0;uint32_t info[16]{};reinterpret_cast<TupleGetter>(fn)(geometry,info,0);return info[2];
}

static bool validate_position_sequence(void* geometry,void* renderer) try {
 uint32_t positions_ptr=0;if(!safe_read(static_cast<unsigned char*>(renderer)+0x7c,&positions_ptr,4)||!positions_ptr)return false;
 std::vector<std::array<float,3>> expected(65536);
 if(!safe_read(reinterpret_cast<void*>(positions_ptr),expected.data(),expected.size()*sizeof(expected[0])))return false;
 IteratorTuple it;position_factory(it.pointers,geometry,0,0);unsigned count=0;
 while(!it.ended()&&count<65536){std::array<float,3> point{};auto fn=it.method(5);if(!fn)return false;reinterpret_cast<void*(__thiscall*)(void*,float*)>(fn)(it.pointers[0],point.data());if(std::memcmp(point.data(),expected[count].data(),12))return false;++count;it.next();}
 bool ok=count==65536&&it.ended();return ok;
} catch(...){return false;}
static bool prepare_explicit_mesh(void* copy,void* renderer) try {
 if(!validated_count_method||!validated_map_method||native_method(copy,0x40/4)!=validated_count_method||native_method(copy,0x44/4)!=validated_map_method)return false;
 std::vector<uint16_t> indices(65536);for(unsigned i=0;i<indices.size();i++)indices[i]=static_cast<uint16_t>(i);
 reinterpret_cast<void(__thiscall*)(void*,unsigned,unsigned)>(validated_count_method)(copy,0,65536);
 for(unsigned component=0;component<3;component++)reinterpret_cast<void(__thiscall*)(void*,unsigned,unsigned,const uint16_t*,unsigned)>(validated_map_method)(copy,0,component,indices.data(),65536);
 return source_point_count(copy)==65536&&validate_position_sequence(copy,renderer);
} catch(...){return false;}
#include "water_fix.inc"
#include "lot_water_fix.inc"
#include "terrain_strip.inc"
#include "placement_fix.inc"

static void __fastcall hood_hook(void* self,void*,void* geometry){
 void* renderer=nullptr;unsigned phase=99;bool valid=geometry&&terrain_owner(self,renderer,phase);
 set_tag(geometry,renderer,phase,valid);
 hood_original(self,geometry);
 if(!valid||(!strip_ready&&phase!=3))return;

 unsigned count=source_point_count(geometry);

 if(count!=66049||native_method(geometry,0xe0/4)!=validated_clone_method||!validated_clone_method){return;}
 void* clone=nullptr;bool ok=reinterpret_cast<CloneMesh>(validated_clone_method)(geometry,0x5bafb147,&clone);
 if(!ok||!clone||source_point_count(clone)!=count||!(strip_ready?prepare_split_terrain(geometry,clone,renderer,phase):prepare_explicit_mesh(clone,renderer))){if(clone)release_native(clone);return;}
 save_preserved({geometry,clone,65536,count});
 
}


static void __fastcall convert_hook(void* self,void*,void* geometry){
 auto tag=get_tag(geometry);set_tag(geometry,nullptr,0,false);
 PreservedMesh saved=take_preserved(geometry);
 void* used=geometry;
 if(tag.geometry&&saved.copy&&saved.built==66049&&valid_prepared_terrain(saved.copy,tag.renderer)){
  // Preserve late native picking data in the render replacement as well.
  if(!placement_ready||synchronize_placement(geometry,saved.copy,tag.phase))used=saved.copy;
 }
 WaterCopyOwner water{!tag.geometry?try_water_fix(geometry):nullptr};
 if(!water.copy&&!tag.geometry)water.copy=try_lot_water_fix(geometry);
 if(water.copy)used=water.copy;
 convert_original(self,used);
 if(saved.copy)release_native(saved.copy);
}
static void install_native_guard(){
 void* hood=resolve_signature(hood_signature,sizeof(hood_signature),0x3cf,4,false);
 void* convert=resolve_signature(convert_signature,sizeof(convert_signature),0x25b5,4,false);
 void* clone=resolve_signature(clone_signature,sizeof(clone_signature),0xe1,8,false);
 void* setcount=resolve_signature(count_signature,sizeof(count_signature),0x66,8,false);
 void* setmap=resolve_signature(map_signature,sizeof(map_signature),0xda,16,false);
 auto factory=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+(0xe810f0-0x400000);unsigned char actual[40]{};
 if(!hood||!convert||!clone||!setcount||!setmap||!safe_read(factory,actual,40)||std::memcmp(actual,position_factory_signature,40)){return;}
 position_factory=reinterpret_cast<PositionFactory>(factory);validated_clone_method=clone;validated_count_method=setcount;validated_map_method=setmap;
 void* targets[]={hood,convert};void* hooks[]={reinterpret_cast<void*>(&hood_hook),reinterpret_cast<void*>(&convert_hook)};
 void** originals[]={reinterpret_cast<void**>(&hood_original),reinterpret_cast<void**>(&convert_original)};
 unsigned created=0;bool ok=true;
 for(unsigned i=0;i<2;i++){MH_STATUS status=MH_CreateHook(targets[i],hooks[i],originals[i]);if(status!=MH_OK){ok=false;break;}++created;}
 if(ok)for(unsigned i=0;i<2;i++){MH_STATUS status=MH_EnableHook(targets[i]);if(status!=MH_OK){ok=false;break;}}
 if(!ok){for(unsigned i=0;i<created;i++){MH_DisableHook(targets[i]);MH_RemoveHook(targets[i]);}hood_original=convert_original=nullptr;position_factory=nullptr;}
}

#include "importer.inc"
static DWORD WINAPI worker(void*) {
 try {
  MH_STATUS status=MH_Initialize();if(status!=MH_OK)return 0;
  install_placement_fix();
  install_native_guard();install_water_fix();install_terrain_strip();
  importer::install_importer();
 }catch(...){/* Unsupported host or allocation failure: install no further hooks. */}
 return 0;
}
static void schedule(){if(InterlockedCompareExchange(&scheduled,1,0))return;HANDLE t=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(t)CloseHandle(t);}
extern "C" __declspec(dllexport) void __cdecl InitializeASI(){schedule();}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)schedule();return TRUE;}

