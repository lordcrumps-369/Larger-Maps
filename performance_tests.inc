static void test_full_validation_cost(){
 void* vt[64]{};vt[0xa0/4]=reinterpret_cast<void*>(&fake_stream_info);vt[0x9c/4]=reinterpret_cast<void*>(&fake_one);
 FakeGeometry geometry{vt,65536,7};position_factory=&fake_position_factory;
 std::vector<Point3> expected(65536);for(unsigned i=0;i<expected.size();i++)expected[i]={float((i%257)*10),float((i/257)*10),float(i%100)};
 uint32_t renderer[128]{};renderer[0x7c/4]=reinterpret_cast<uint32_t>(expected.data());
 memory_read_calls=0;assert(validate_position_sequence(&geometry,renderer));unsigned complete_reads=memory_read_calls;
 assert(complete_reads<32);
 // Corruption at the final point must still be rejected: no quota, early exit
 // or shortened geometry validation was used to obtain the lower lookup cost.
 expected.back()[2]+=1;memory_read_calls=0;assert(!validate_position_sequence(&geometry,renderer));assert(memory_read_calls<32);
 IteratorTuple invalid;assert(invalid.ended()&&!invalid.method(5));invalid.next();
 std::printf("PASS full 65536-point validation uses %u checked memory reads; last-point corruption still rejected, iterator lookup local, invalid iterator safe\n",complete_reads);
}
