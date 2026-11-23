namespace importer {
// Local executable signatures, with only relative call displacements masked.
// These guards identify code/layout, not a filename or whole executable digest.
static constexpr unsigned char validator_signature[]={
 0x56,0x8b,0xf1,0x8b,0x06,0x8d,0x4e,0x10,0x51,0x50,0x8b,0xce,0xe8,0x1f,0xfd,0xff,0xff,0x84,0xc0,0x74,0x1c,0x8b,0x4e,0x12,0xb8,0x41,0x00,0x00,0x00,0x3b,0xc8,0x75,0x0c,0x39,0x46,0x16,0x75,0x07,0xb8,0x01,0x00,0x00,0x00,0x5e,0xc3,0x33,0xc0,0x5e,0xc3,0x32,0xc0,0x5e,0xc3};
static constexpr unsigned char import_signature[]={
 0x56,0x8b,0xf1,0x8b,0x06,0x8d,0x4e,0x10,0x51,0x50,0x8b,0xce,0xe8,0x3f,0xf5,0xff,0xff,0x84,0xc0,0x74,0x2a,0x8b,0x46,0x08,0x85,0xc0,0x74,0x23,0x8b,0x56,0x16,0x8b,0x4e,0x12,0x8d,0x44,0x12,0xfe,0x8d,0x54,0x09,0xfe,0x50,0x52,0x8b,0xce,0xe8,0x2d,0xf8,0xff,0xff,0x8b,0x06,0x50,0x8b,0xce,0xe8,0x93,0xfa,0xff,0xff,0x5e,0xc3,0x32,0xc0,0x5e,0xc3};
static constexpr unsigned char reader_signature[]={
 0x8b,0x4c,0x24,0x04,0x83,0xec,0x10,0x85,0xc9,0x75,0x08,0x32,0xc0,0x83,0xc4,0x10,0xc2,0x08,0x00,0x8b,0x44,0x24,0x18,0x56,0x33,0xd2,0x8b,0xf0,0x89,0x16,0x89,0x56};
template<size_t N> static std::array<unsigned char,N> mask_for(bool importer=false){
    std::array<unsigned char,N> mask{};mask.fill(255);
    for(size_t i=13;i<=16&&i<N;i++)mask[i]=0;
    if(importer)for(size_t begin:{size_t(47),size_t(57)})for(size_t i=begin;i<begin+4&&i<N;i++)mask[i]=0;
    return mask;
}
static bool matches(const unsigned char* data,const unsigned char* pattern,const unsigned char* mask,size_t length){
    for(size_t i=0;i<length;i++)if(mask[i]&&data[i]!=pattern[i])return false;return true;
}
static size_t unique_pattern(const unsigned char* data,size_t size,const unsigned char* pattern,const unsigned char* mask,size_t length,size_t* count=nullptr){
    size_t found=SIZE_MAX,n=0;if(size>=length&&length)for(size_t i=0;i<=size-length;i++)if(matches(data+i,pattern,mask,length)){found=i;++n;}
    if(count)*count=n;return n==1?found:SIZE_MAX;
}
struct Resolved {unsigned char* validator=nullptr;unsigned char* importer=nullptr;unsigned char* reader=nullptr;};
static bool readable_executable(const unsigned char* p,size_t length){
    MEMORY_BASIC_INFORMATION m{};
    if(!VirtualQuery(p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    if(!(m.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
    return reinterpret_cast<uintptr_t>(p)+length<=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;
}
static unsigned char* relative_target(unsigned char* function,size_t call_offset){
    int32_t displacement=0;std::memcpy(&displacement,function+call_offset+1,4);
    return function+call_offset+5+displacement;
}
static Resolved resolve_native(){
    Resolved result{};wchar_t path[MAX_PATH]{};if(!GetModuleFileNameW(nullptr,path,MAX_PATH))return {};
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)return {};
    LARGE_INTEGER size{};if(!GetFileSizeEx(file,&size)||size.QuadPart<256||size.QuadPart>512*1024*1024){CloseHandle(file);return {};}
    std::vector<unsigned char> bytes(static_cast<size_t>(size.QuadPart));DWORD got=0;
    bool ok=ReadFile(file,bytes.data(),DWORD(bytes.size()),&got,nullptr)&&got==bytes.size();CloseHandle(file);if(!ok)return {};
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||size_t(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS32)>bytes.size())return {};
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data()+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC)return {};
    size_t section_offset=size_t(dos->e_lfanew)+24+nt->FileHeader.SizeOfOptionalHeader;
    if(section_offset+size_t(nt->FileHeader.NumberOfSections)*sizeof(IMAGE_SECTION_HEADER)>bytes.size())return {};
    const auto* sections=reinterpret_cast<const IMAGE_SECTION_HEADER*>(bytes.data()+section_offset);
    auto* image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto vm=mask_for<sizeof(validator_signature)>();
    const auto im=mask_for<sizeof(import_signature)>(true);
    unsigned validator_count=0,import_count=0;
    for(unsigned s=0;s<nt->FileHeader.NumberOfSections;s++){
        const auto& section=sections[s];if(!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
        if(size_t(section.PointerToRawData)+section.SizeOfRawData>bytes.size())return {};
        auto find=[&](const unsigned char* pattern,const unsigned char* mask,size_t length,unsigned& total)->unsigned char*{
            size_t count=0,at=unique_pattern(bytes.data()+section.PointerToRawData,section.SizeOfRawData,pattern,mask,length,&count);
            total+=unsigned(count);if(count!=1)return nullptr;
            if(at+length>section.Misc.VirtualSize||size_t(section.VirtualAddress)+at+length>nt->OptionalHeader.SizeOfImage)return nullptr;
            auto* mapped=image+section.VirtualAddress+at;
            if(!readable_executable(mapped,length)||!matches(mapped,pattern,mask,length))return nullptr;
            // All mapped bytes, including the selected call displacements, must agree with the file.
            if(std::memcmp(mapped,bytes.data()+section.PointerToRawData+at,length))return nullptr;
            return mapped;
        };
        if(auto* p=find(validator_signature,vm.data(),vm.size(),validator_count))result.validator=p;
        if(auto* p=find(import_signature,im.data(),im.size(),import_count))result.importer=p;
    }
    if(validator_count!=1||import_count!=1||!result.validator||!result.importer)return {};
    auto* reader=relative_target(result.validator,12);
    if(reinterpret_cast<uintptr_t>(reader)<reinterpret_cast<uintptr_t>(image)||reinterpret_cast<uintptr_t>(reader)+0x77>reinterpret_cast<uintptr_t>(image)+nt->OptionalHeader.SizeOfImage)return {};
    if(reader!=relative_target(result.importer,12)||!readable_executable(reader,0x77)||std::memcmp(reader,reader_signature,sizeof(reader_signature)))return {};
    const unsigned char ret8[]={0xc2,8,0};if(std::memcmp(reader+0x74,ret8,3))return {};
    // Establish that the reader body really reads the expected 26-byte SC4 resource.
    const unsigned char header_key[]={0xc7,0x44,0x24,0x10,0xe3,0xe7,0x91,0x8a};
    if(std::memcmp(reader+0x46,header_key,sizeof(header_key)))return {};
    result.reader=reader;return result;
}

using Validate=bool(__thiscall*)(void*);
using HeaderRead=bool(__stdcall*)(void*,void*);
static Validate validate_original=nullptr;
static HeaderRead read_header=nullptr;
static uint32_t word32(const unsigned char* data){uint32_t result;std::memcpy(&result,data,4);return result;}
static bool medium_header(const unsigned char* header){return word32(header+2)==129&&word32(header+6)==129;}
static bool __fastcall validate_hook(void* self,void*){
    // Preserve the complete original small-map path. Failed native validation can
    // be extended only by a second full successful read of the same medium header.
    bool native=validate_original(self);auto* data=static_cast<unsigned char*>(self);
    bool medium=false;
    if(!native){
        unsigned char header[26]{};void* file=nullptr;std::memcpy(&file,data,sizeof(file));
        medium=read_header(file,header)&&medium_header(header)&&!std::memcmp(header,data+0x10,sizeof(header));
    }
    return native||medium;
}
static bool install_importer(){
 auto code=resolve_native();if(!code.validator||!code.importer||!code.reader)return false;
 read_header=reinterpret_cast<HeaderRead>(code.reader);
 if(MH_CreateHook(code.validator,reinterpret_cast<void*>(&validate_hook),reinterpret_cast<void**>(&validate_original))!=MH_OK)return false;
 if(MH_EnableHook(code.validator)!=MH_OK){MH_RemoveHook(code.validator);validate_original=nullptr;return false;}
 return true;
}
}
