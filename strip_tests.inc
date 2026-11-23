static std::vector<uint32_t> strip_test_keys;
static std::vector<uint16_t> strip_test_words;
static StripPlan strip_test_plan;
static void test_strip_plan(){
 for(unsigned key=0;key<131072;key++){std::array<unsigned,3> ids{};assert(strip_corners(key,ids));strip_test_keys.push_back(key);for(unsigned id:ids)strip_test_words.push_back(uint16_t(id));}
 StripGroup group;std::vector<int> remap(66049,-1);
 assert(split_strip_group(strip_test_keys,strip_test_words,group,strip_test_plan,remap));
 assert(strip_test_plan.triangles==131072&&strip_test_plan.edge_triangles==1024&&strip_test_plan.edge_points.size()==771&&group.main.size()==390144&&group.edge.size()==3072);
 unsigned ordinal=0;
 for(unsigned i=0;i<group.main.size();i+=3){std::array<unsigned,3> full{};strip_corners(ordinal++,full);for(unsigned j=0;j<3;j++)assert(group.main[i+j]==full[j]);}
 for(unsigned i=0;i<group.edge.size();i+=3){std::array<unsigned,3> full{};strip_corners(ordinal++,full);for(unsigned j=0;j<3;j++)assert(strip_test_plan.edge_points[group.edge[i+j]]==full[j]);}
 assert(ordinal==131072);strip_test_plan.groups.push_back(group);
 auto corrupt=strip_test_words;corrupt.back()^=1;StripPlan bad;StripGroup other;std::vector<int> badmap(66049,-1);assert(!split_strip_group(strip_test_keys,corrupt,other,bad,badmap));
 std::array<unsigned,3> dummy{};assert(!strip_corners(131072,dummy));
 puts("PASS all 131072 grid triangles preserved exactly; 1024 boundary triangles use 771 local vertices, all indices uint16; invalid keys and native-index mismatches rejected");
 void* a=reinterpret_cast<void*>(0x1111);void* b=reinterpret_cast<void*>(0x2222);void* c=reinterpret_cast<void*>(0x3333);void* d=reinterpret_cast<void*>(0x4444);
 save_preserved({a,c,65536,66049});save_preserved({b,d,65536,66049});assert(has_preserved(a)&&has_preserved(b));assert(take_preserved(b).copy==d&&take_preserved(a).copy==c&&!has_preserved(a)&&!has_preserved(b));
 puts("PASS independent pending copies for two geometry sources, consumed by matching identity");
}
struct SAttr {uint32_t semantic=0;unsigned format=0,role=0,count=0;std::vector<unsigned char> bytes;};
struct SStream {unsigned count=0;std::vector<unsigned> attributes;std::array<std::vector<uint16_t>,3> maps;};
struct SGroup {unsigned type=2,stream=0;uint32_t material=0xffffffff;std::string name;std::vector<uint16_t> indices;};
struct SGeometry {void** vt=nullptr;std::vector<SAttr> attributes;std::vector<SStream> streams;std::vector<SGroup> groups;};
static void* s_vt[64]{};static void* s_pos_vt[7]{};static void* s_index_vt[7]{};
static FakeIterator s_pos_begin{s_pos_vt},s_pos_owner{s_pos_vt},s_pos_end{s_pos_vt};
static FakeIterator s_idx_begin{s_index_vt},s_idx_owner{s_index_vt},s_idx_end{s_index_vt};
static SGeometry* s_current=nullptr;static unsigned s_stream=0;static const std::vector<uint16_t>* s_indices=nullptr;
static unsigned __fastcall s_attributes(void* p,void*){return static_cast<SGeometry*>(p)->attributes.size();}
static unsigned __fastcall s_streams(void* p,void*){return static_cast<SGeometry*>(p)->streams.size();}
static unsigned __fastcall s_groups(void* p,void*){return static_cast<SGeometry*>(p)->groups.size();}
static void* __fastcall s_stream_info(void* p,void*,void* out,unsigned index){auto& s=static_cast<SGeometry*>(p)->streams.at(index);uint32_t desc[16]={index,unsigned(s.attributes.size()),s.count};std::memcpy(out,desc,sizeof(desc));return out;}
static void* __fastcall s_group_info(void* p,void*,void* out,unsigned index){auto& g=static_cast<SGeometry*>(p)->groups.at(index);uint32_t desc[16]={index,g.type,unsigned(g.indices.size()),g.stream,g.material,reinterpret_cast<uint32_t>(g.name.c_str())};std::memcpy(out,desc,sizeof(desc));return out;}
static void __fastcall s_set_count(void* p,void*,unsigned index,unsigned count){auto& s=static_cast<SGeometry*>(p)->streams.at(index);s.count=count;for(auto& map:s.maps)map.clear();}
static void __fastcall s_set_map(void* p,void*,unsigned stream,unsigned component,const uint16_t* data,unsigned count){auto& map=static_cast<SGeometry*>(p)->streams.at(stream).maps.at(component);map.assign(data,data+count);}
static void __fastcall s_set_array(void* p,void*,unsigned element,unsigned count,const void* data,unsigned bytes,const void* map,unsigned maps){assert(!map&&!maps);auto& a=static_cast<SGeometry*>(p)->attributes.at(element);a.count=count;auto begin=static_cast<const unsigned char*>(data);a.bytes.assign(begin,begin+bytes);}
static unsigned __fastcall s_add_element(void* p,void*,uint32_t semantic,unsigned channel,unsigned format,unsigned role){assert(channel==0);auto g=static_cast<SGeometry*>(p);unsigned id=g->attributes.size();g->attributes.push_back({semantic,format,role});return id;}
static void __fastcall s_add_stream(void* p,void*){static_cast<SGeometry*>(p)->streams.emplace_back();}
static void __fastcall s_assign(void* p,void*,unsigned stream,unsigned element){static_cast<SGeometry*>(p)->streams.at(stream).attributes.push_back(element);}
static void __fastcall s_add_primitive(void* p,void*,unsigned type,const char* name,unsigned stream,uint32_t material){static_cast<SGeometry*>(p)->groups.push_back({type,stream,material,name,{}});}
static void __fastcall s_set_indices(void* p,void*,unsigned group,const uint16_t* data,unsigned count){auto& v=static_cast<SGeometry*>(p)->groups.at(group).indices;v.clear();if(count)v.assign(data,data+count);}
static bool __fastcall s_pos_ended(void* p,void*,void*){return static_cast<FakeIterator*>(p)->at>=s_current->streams.at(s_stream).count;}
static void* __fastcall s_position(void* p,void*,float* out){unsigned i=static_cast<FakeIterator*>(p)->at;auto& s=s_current->streams.at(s_stream);unsigned id=s.maps[0].empty()?i:s.maps[0].at(i);auto& a=s_current->attributes.at(s.attributes.at(0));assert((id+1)*12<=a.bytes.size());std::memcpy(out,a.bytes.data()+id*12,12);return out;}
static void* __cdecl s_factory(void* out,void* geometry,unsigned stream,unsigned component){assert(component==0);s_current=static_cast<SGeometry*>(geometry);s_stream=stream;s_pos_begin.at=0;auto tuple=static_cast<void**>(out);tuple[0]=&s_pos_begin;tuple[1]=&s_pos_owner;tuple[2]=&s_pos_end;return out;}
static bool __fastcall s_idx_ended(void* p,void*,void*){return static_cast<FakeIterator*>(p)->at>=s_indices->size();}
static uint16_t __fastcall s_index(void* p,void*){return s_indices->at(static_cast<FakeIterator*>(p)->at);}
static void* __fastcall s_group_indices(void* p,void*,void* out,unsigned group){s_indices=&static_cast<SGeometry*>(p)->groups.at(group).indices;s_idx_begin.at=0;auto tuple=static_cast<void**>(out);tuple[0]=&s_idx_begin;tuple[1]=&s_idx_owner;tuple[2]=&s_idx_end;return out;}
static unsigned s_emitted=0;
static uintptr_t __fastcall s_emit(void*,void*,void*,const int*){++s_emitted;return 77;}
static void test_strip_integration(){
 s_vt[0x7c/4]=reinterpret_cast<void*>(&s_attributes);s_vt[0x9c/4]=reinterpret_cast<void*>(&s_streams);s_vt[0xb0/4]=reinterpret_cast<void*>(&s_groups);s_vt[0xa0/4]=reinterpret_cast<void*>(&s_stream_info);s_vt[0xb4/4]=reinterpret_cast<void*>(&s_group_info);s_vt[0xb8/4]=reinterpret_cast<void*>(&s_group_indices);
 s_vt[0x40/4]=validated_count_method=reinterpret_cast<void*>(&s_set_count);s_vt[0x44/4]=validated_map_method=reinterpret_cast<void*>(&s_set_map);s_vt[0x30/4]=validated_array_method=reinterpret_cast<void*>(&s_set_array);
 s_vt[0x24/4]=strip_element=reinterpret_cast<void*>(&s_add_element);s_vt[0x34/4]=strip_stream=reinterpret_cast<void*>(&s_add_stream);s_vt[0x38/4]=strip_assign=reinterpret_cast<void*>(&s_assign);s_vt[0x50/4]=strip_primitive=reinterpret_cast<void*>(&s_add_primitive);s_vt[0x54/4]=strip_indices=reinterpret_cast<void*>(&s_set_indices);
 for(auto vt:{s_pos_vt,s_index_vt}){vt[2]=reinterpret_cast<void*>(&iter_release);vt[6]=reinterpret_cast<void*>(&iter_next);}s_pos_vt[4]=reinterpret_cast<void*>(&s_pos_ended);s_pos_vt[5]=reinterpret_cast<void*>(&s_position);s_index_vt[4]=reinterpret_cast<void*>(&s_idx_ended);s_index_vt[5]=reinterpret_cast<void*>(&s_index);position_factory=&s_factory;
 std::vector<Point3> points(66049),normals(66049,Point3{0,0,1});std::vector<Point2> uv(66049);
 for(unsigned i=0;i<66049;i++){points[i]={float(i%257*10),float(i/257*10),float(i%100)};uv[i]={float(i%257),float(i/257)};}
 uint32_t renderer[64]{};renderer[9]=renderer[10]=257;uint32_t offsets[6]={0,258,257,0,1,258};std::memcpy(renderer+0x44/4,offsets,sizeof(offsets));
 auto vector=[&](unsigned offset,const void* begin,unsigned bytes){renderer[offset/4]=reinterpret_cast<uint32_t>(begin);renderer[offset/4+1]=renderer[offset/4+2]=renderer[offset/4]+bytes;};
 vector(0x7c,points.data(),points.size()*12);vector(0x88,normals.data(),normals.size()*12);vector(0x94,uv.data(),uv.size()*8);
 uint32_t material[18]{};material[3]=reinterpret_cast<uint32_t>("strip-test");material[12]=reinterpret_cast<uint32_t>(strip_test_keys.data());material[13]=material[14]=material[12]+strip_test_keys.size()*4;material[15]=reinterpret_cast<uint32_t>(strip_test_words.data());material[16]=material[17]=material[15]+strip_test_words.size()*2;uint32_t slot=reinterpret_cast<uint32_t>(material);vector(0xa0,&slot,4);
 SGeometry source;source.vt=s_vt;source.attributes.resize(3);source.streams.push_back({66049,{0,1,2},{}});source.groups.push_back({2,0,0xffffffff,"strip-test",strip_test_words});
 s_set_array(&source,nullptr,0,66049,points.data(),points.size()*12,nullptr,0);s_set_array(&source,nullptr,1,66049,normals.data(),normals.size()*12,nullptr,0);s_set_array(&source,nullptr,2,66049,uv.data(),uv.size()*8,nullptr,0);
 SGeometry copy=source;
 source.groups[0].indices.back()^=1;assert(!prepare_split_terrain(&source,&copy,renderer,3));assert(copy.streams.size()==1&&copy.streams[0].count==66049);source.groups[0].indices.back()^=1;
 assert(prepare_split_terrain(&source,&copy,renderer,3));assert(copy.streams.size()==2&&copy.streams[0].count==65536&&copy.streams[1].count==771&&copy.attributes.size()==6&&copy.groups.size()==2);
 assert(copy.groups[0].indices==strip_test_plan.groups[0].main&&copy.groups[1].indices==strip_test_plan.groups[0].edge&&copy.groups[1].stream==1&&copy.groups[1].name==copy.groups[0].name&&copy.groups[1].material==copy.groups[0].material);
 assert(valid_prepared_terrain(&copy,renderer));assert(source.streams.size()==1&&source.streams[0].count==66049&&source.groups[0].indices==strip_test_words&&source.attributes.size()==3);
 for(unsigned i=0;i<771;i++){unsigned id=strip_test_plan.edge_points[i];assert(!std::memcmp(copy.attributes[3].bytes.data()+i*12,points[id].data(),12));assert(!std::memcmp(copy.attributes[4].bytes.data()+i*12,normals[id].data(),12));assert(!std::memcmp(copy.attributes[5].bytes.data()+i*8,uv[id].data(),8));}
 strip_emit_original=reinterpret_cast<TerrainEmit>(&s_emit);int edge[3]={0,254,0},ordinary[3]={0,0,0};strip_ready=false;assert(strip_emit_hook(renderer,nullptr,nullptr,edge)==0&&s_emitted==0);assert(strip_emit_hook(renderer,nullptr,nullptr,ordinary)==77&&s_emitted==1);strip_ready=true;assert(strip_emit_hook(renderer,nullptr,nullptr,edge)==77&&s_emitted==2);strip_ready=false;
 puts("PASS native split integration: two streams, original arrays/groups untouched, all 771 edge positions/normals/UVs exact, material names retained, mismatched source rejected, all groups verified; emitter fallback excludes overflow and ready mode forwards it");
}
