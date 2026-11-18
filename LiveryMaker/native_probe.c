#ifndef RERIFF_ENGINE_SOURCE
#error "Define RERIFF_ENGINE_SOURCE to the ReRiff ExperimentalMenu/engine_menu.c path"
#endif
#include RERIFF_ENGINE_SOURCE
#define CHECK(c) do{if(!(c)){printf("FAIL %d: %s\n",__LINE__,#c);return 1;}}while(0)
typedef struct {union{WCHAR small[8];const WCHAR *text;};unsigned length,capacity;} NativeString;
typedef struct {BYTE *begin,*end,*capacity;} NativeVector;

static WCHAR fixture_base[MAX_PATH];
static void __fastcall fixture_scan(const ModsNativeString *unused,void *entries){
 (void)unused;mods_scan_path(fixture_base,entries);
}
static int __thiscall fixture_cancelled(void *self){(void)self;return 0;}
static int verify_bytes(BYTE *base,const char *path,const void *expected,unsigned length){
 BYTE stream[64]={0};BYTE *data=calloc(length+1,1);CHECK(data);
 ((void *(__thiscall *)(void *,const char *,BOOL))(base+0x197940))(stream,path,FALSE);
 CHECK(stream[0xc]);
 int count=((int(__thiscall *)(void *,void *,unsigned))(base+0x197c00))(stream,data,length+1);
 CHECK(count==(int)length);CHECK(!memcmp(data,expected,length));free(data);
 ((void *(__thiscall *)(void *,unsigned))(base+0x197b60))(stream,0);
 return 0;
}
int main(int argc,char **argv){
 CHECK(argc==4||argc==5||argc==6);WCHAR exe[MAX_PATH],folder[MAX_PATH];CHECK(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,exe,MAX_PATH));CHECK(supported_file(exe));CHECK(MultiByteToWideChar(CP_UTF8,0,argv[2],-1,folder,MAX_PATH));
    BYTE *base=(BYTE *)LoadLibraryExW(exe,NULL,DONT_RESOLVE_DLL_REFERENCES);CHECK(base);
    IMAGE_NT_HEADERS32 *nt=(void *)(base+((IMAGE_DOS_HEADER *)base)->e_lfanew);
    /* Wine maps an EXE without relocating its native code in this mode.
       Relocate this private test mapping only; never change the source file. */
    DWORD delta=(DWORD)(uintptr_t)base-nt->OptionalHeader.ImageBase;
    if(delta && *(DWORD *)(base+0x198433)==0x0092d39c){
        IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        CHECK(reloc.VirtualAddress && reloc.Size);
        DWORD cursor=0;
        while(cursor<reloc.Size){
            IMAGE_BASE_RELOCATION *block=(void *)(base+reloc.VirtualAddress+cursor);
            CHECK(block->SizeOfBlock>=8 && block->SizeOfBlock<=reloc.Size-cursor);
            WORD *entries=(void *)(block+1);
            for(DWORD i=0;i<(block->SizeOfBlock-8)/2;i++){
                unsigned type=entries[i]>>12;if(type==IMAGE_REL_BASED_ABSOLUTE)continue;
                CHECK(type==IMAGE_REL_BASED_HIGHLOW);
                DWORD *target=(void *)(base+block->VirtualAddress+(entries[i]&0xfff));
                DWORD old,ignored;CHECK(VirtualProtect(target,4,PAGE_READWRITE,&old));
                *target+=delta;CHECK(VirtualProtect(target,4,old,&ignored));
            }
            cursor+=block->SizeOfBlock;
        }
        FlushInstructionCache(GetCurrentProcess(),base,nt->OptionalHeader.SizeOfImage);
    }
    IMAGE_IMPORT_DESCRIPTOR *imports=(void *)(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(unsigned i=0;imports[i].Name;i++){
        const char *name=(char *)(base+imports[i].Name);
        if(_stricmp(name,"kernel32.dll")&&_stricmp(name,"msvcr120.dll")&&_stricmp(name,"msvcp120.dll"))continue;
        HMODULE library=LoadLibraryA(name);CHECK(library);
        IMAGE_THUNK_DATA32 *names=(void *)(base+imports[i].OriginalFirstThunk),*slots=(void *)(base+imports[i].FirstThunk);
        for(unsigned j=0;names[j].u1.AddressOfData;j++){
            const char *symbol=IMAGE_SNAP_BY_ORDINAL32(names[j].u1.Ordinal)?(const char *)(uintptr_t)IMAGE_ORDINAL32(names[j].u1.Ordinal):(char *)((IMAGE_IMPORT_BY_NAME *)(base+names[j].u1.AddressOfData))->Name;
            FARPROC function=GetProcAddress(library,symbol);CHECK(function);DWORD old,ignored;CHECK(VirtualProtect(slots+j,4,PAGE_READWRITE,&old));
            slots[j].u1.Function=(DWORD)(uintptr_t)function;VirtualProtect(slots+j,4,old,&ignored);
        }
    }

 WCHAR fake_exe[MAX_PATH];wcscpy(fake_exe,folder);size_t len=wcslen(fake_exe);CHECK(len>5);fake_exe[len-5]=0;wcscat(fake_exe,L"Game.exe");
 BYTE manager[0x100]={0};*(unsigned *)(manager+0x44)=7;
 BYTE *original_record=((void *(__cdecl *)(size_t))(void *)GetProcAddress(GetModuleHandleA("msvcr120.dll"),"malloc"))(20);CHECK(original_record);*(BYTE **)(manager+0x48)=original_record;*(BYTE **)(manager+0x4c)=original_record;*(BYTE **)(manager+0x50)=original_record;
 /* A non-null empty native vector models the initialized manager and exercises
    the late-start path without opening any installed game archive. */
 *(BYTE **)(base+0x7fcd2c)=manager;


 mods_scan_original=(ModsScanFn)(base+0x15e130);
 wcscpy(fixture_base,exe);WCHAR *last=wcsrchr(fixture_base,L'\\');CHECK(last);last[1]=0;
 BYTE *site=base+0x15ede7;DWORD old,ignored;CHECK(VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old));
 BYTE saved[5];memcpy(saved,site,5);*(int32_t *)(site+1)=(int32_t)((BYTE *)(uintptr_t)fixture_scan-(site+5));
 ((void(__thiscall *)(void *))(base+0x15ed80))(manager);
 memcpy(site,saved,5);VirtualProtect(site,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),site,5);
 CHECK(*(BYTE **)(manager+0x4c)-*(BYTE **)(manager+0x48)==20);
 void *vtable[4]={NULL,NULL,NULL,fixture_cancelled};*(void **)manager=vtable;
 CHECK(install_mods_vfs(base,fake_exe));CHECK(mods_scanned);
 CHECK(*(BYTE **)(manager+0x4c)-*(BYTE **)(manager+0x48)==40);
 FILE *f=fopen(argv[3],"rb");CHECK(f);CHECK(!fseek(f,0,SEEK_END));long size=ftell(f);CHECK(size>0&&size<2000000);rewind(f);
 BYTE *expected=malloc(size);CHECK(expected);CHECK(fread(expected,1,size,f)==(size_t)size);fclose(f);
 CHECK(!verify_bytes(base,argc>=5?argv[4]:"graphics\\vehicles\\decals\\alf_01\\batchtextures.brres",expected,(unsigned)size));
 if(argc==6){
   BYTE context[36]={0};
   *(void **)(context+8)=((void *(__cdecl *)(void))(base+0x853f0))();CHECK(*(void **)(context+8));
   typedef unsigned char (__thiscall *RunScript)(void *,const char *,const void *,unsigned,int);
   RunScript run=(RunScript)(base+0x41750);
   CHECK(run(context,"paint-export",expected,(unsigned)size,0));
   FILE *check=fopen(argv[5],"rb");CHECK(check);CHECK(!fseek(check,0,SEEK_END));long check_size=ftell(check);CHECK(check_size>0&&check_size<2000000);rewind(check);
   char *source=malloc(check_size);CHECK(source);CHECK(fread(source,1,check_size,check)==(size_t)check_size);fclose(check);
   CHECK(run(context,"paint-value-check",source,(unsigned)check_size,0));free(source);
   ((void(__thiscall *)(void *))(base+0x85550))(*(void **)(context+8));
   puts("PASS original Lua runtime executed exported paint and verified table values");
 }
 free(expected);
 puts("PASS actual generated resource read through original VFS over stock archive");return 0;
}
