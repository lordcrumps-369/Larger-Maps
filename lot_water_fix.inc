static bool is_lot_skirt_water(void* geometry){
 if(get_native_count(geometry,0xb0/4)!=1)return false;
 auto info=reinterpret_cast<TupleGetter>(native_method(geometry,0xb4/4));if(!info)return false;
 uint32_t desc[16]{};info(geometry,desc,0);constexpr char wanted[]="subset-LotSkirtWater";char name[sizeof(wanted)]{};
 return desc[1]==2&&desc[3]==0&&desc[5]&&safe_read(reinterpret_cast<void*>(desc[5]),name,sizeof(name))&&!std::memcmp(name,wanted,sizeof(name));
}
static bool read_lot_position_map(void* geometry,unsigned count,std::vector<uint16_t>& map){
 uint32_t streams[3]{};if(!safe_read(static_cast<unsigned char*>(geometry)+0x10,streams,sizeof(streams))||!streams[0]||streams[1]<streams[0]||streams[2]<streams[1]||streams[1]-streams[0]!=60)return false;
 uint32_t bounds[3]{};if(!safe_read(reinterpret_cast<void*>(streams[0]+0x18),bounds,sizeof(bounds)))return false;
 map.resize(count);return read_water_span(bounds,2,count,map.data());
}
static bool lot_repair_positions(const std::vector<Point3>& raw,const std::vector<uint16_t>& map,std::vector<Point3>& expected_old,std::vector<Point3>& corrected,unsigned& changed){
 changed=0;if(raw.size()!=66049||map.empty()||map.size()>65536||!std::isfinite(raw[0][2]))return false;
 // Lot water is translated/rotated and clipped to the skirt bounds. Its flat
 // position grid may therefore contain repeated rows or columns legitimately.
 for(auto p:raw)if(!std::isfinite(p[0])||!std::isfinite(p[1])||p[2]!=raw[0][2]||std::fabs(p[0])>1000000||std::fabs(p[1])>1000000)return false;
 expected_old.resize(map.size());corrected.resize(map.size());unsigned base=0,previous=0;
 for(unsigned i=0;i<map.size();i++){
  unsigned low=map[i];
  if(i&&low<=previous){if(base||previous<65000||low>=513||low==previous)return false;base=65536;}
  unsigned full=base+low;if(full>=raw.size())return false;
  expected_old[i]=raw[low];corrected[i]=raw[full];changed+=expected_old[i]!=corrected[i];previous=low;
 }
 return base==65536&&changed>0;
}
static bool validate_lot_triangles(void* geometry,const std::vector<Point3>& positions){
 auto info=reinterpret_cast<TupleGetter>(native_method(geometry,0xb4/4));auto indices=reinterpret_cast<TupleGetter>(native_method(geometry,0xb8/4));if(!info||!indices)return false;
 uint32_t desc[16]{};info(geometry,desc,0);unsigned count=desc[2];if(!count||count%3||count>2097152)return false;
 IteratorTuple it;indices(geometry,it.pointers,0);Point3 triangle[3]{};
 for(unsigned i=0;i<count;i++){
  if(it.ended())return false;auto fn=it.method(5);if(!fn)return false;
  unsigned index=reinterpret_cast<uint16_t(__thiscall*)(void*)>(fn)(it.pointers[0]);if(index>=positions.size())return false;
  triangle[i%3]=positions[index];it.next();
  if(i%3==2)for(unsigned j=0;j<3;j++){
   float squared=0;for(unsigned k=0;k<3;k++){float d=triangle[j][k]-triangle[(j+1)%3][k];squared+=d*d;}
   if(!std::isfinite(squared)||squared>200.1f)return false;
  }
 }
 return it.ended();
}
static void* try_lot_water_fix(void* geometry) try {
 if(!position_factory||!validated_array_method||!validated_map_method||!validated_clone_method||!is_lot_skirt_water(geometry))return nullptr;
 unsigned count=source_point_count(geometry);if(!count||count>65536)return nullptr;
 std::vector<Point3> raw,old_positions,corrected;std::vector<Point2> uv;std::vector<uint16_t> map;unsigned changed=0;
 if(!read_water_arrays(geometry,count,raw,uv)||!read_lot_position_map(geometry,count,map))return nullptr;
 if(!lot_repair_positions(raw,map,old_positions,corrected,changed)||!match_water_positions(geometry,old_positions)||!validate_lot_triangles(geometry,corrected)){return nullptr;}
 if(native_method(geometry,0xe0/4)!=validated_clone_method)return nullptr;
 WaterCopyOwner owned;bool cloned=reinterpret_cast<CloneMesh>(validated_clone_method)(geometry,0x5bafb147,&owned.copy);
 if(!cloned||!owned.copy||native_method(owned.copy,0x30/4)!=validated_array_method||native_method(owned.copy,0x44/4)!=validated_map_method)return nullptr;
 using SetArray=void(__thiscall*)(void*,unsigned,unsigned,const void*,unsigned,const void*,unsigned);
 reinterpret_cast<SetArray>(validated_array_method)(owned.copy,0,count,corrected.data(),count*12,nullptr,0);
 std::vector<uint16_t> identity(count);for(unsigned i=0;i<count;i++)identity[i]=uint16_t(i);
 reinterpret_cast<void(__thiscall*)(void*,unsigned,unsigned,const uint16_t*,unsigned)>(validated_map_method)(owned.copy,0,0,identity.data(),count);
 if(source_point_count(owned.copy)!=count||!water_uv_identity(owned.copy,count)||!match_water_positions(owned.copy,corrected)||!validate_lot_triangles(owned.copy,corrected)){return nullptr;}
 
 void* result=owned.copy;owned.copy=nullptr;return result;
}catch(...){return nullptr;}
