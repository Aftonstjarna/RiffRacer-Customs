#include "paint.hpp"
#include <random>
using namespace livery;
static void directoryLink(const fs::path &target,const fs::path &link) {
#ifdef _WIN32
    require(CreateSymbolicLinkW(link.c_str(),target.c_str(),SYMBOLIC_LINK_FLAG_DIRECTORY|SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)!=0,"Native directory symlink creation failed: "+std::to_string(GetLastError()));
#else
    fs::create_directory_symlink(target,link);
#endif
}
template<class F> void rejects(F fn) { bool failed=false;try{fn();}catch(const std::exception &){failed=true;}require(failed,"Expected refusal"); }
int main(int argc,char **argv) {
 try {
    fs::path root;std::random_device random;
    for(unsigned attempt=0;attempt<100;attempt++) {
        auto candidate=fs::temp_directory_path()/fs::u8path("customs-テスト-"+std::to_string(random()));
        if(fs::create_directory(candidate)){root=candidate;break;}
    }
    require(!root.empty(),"Cannot create test directory");
    Image image{8,8,Bytes(8*8*4)};for(size_t i=0;i<64;i++){image.pixels[i*4]=255;image.pixels[i*4+1]=32;image.pixels[i*4+2]=64;image.pixels[i*4+3]=uint8_t((i%4)*85);}
    for(auto format:{"DXT3","DXT5"}) {
        Texture t{"test",format,8,8,2,4,0,Bytes(128)};std::memcpy(t.dds.data(),"DDS ",4);set32(t.dds,4,124);set32(t.dds,12,8);set32(t.dds,16,8);set32(t.dds,28,4);set32(t.dds,76,32);set32(t.dds,80,4);std::memcpy(t.dds.data()+84,format,4);t.dds.resize(128+64+16+16+16);
        t.dds=encodeDDS(t,image);auto decoded=decodeDDS(t);for(size_t i=0;i<64;i++){require(std::abs(int(decoded.pixels[i*4])-255)<=8,"RGB round trip");require(std::abs(int(decoded.pixels[i*4+3])-image.pixels[i*4+3])<=19,"Alpha round trip");}
        require(encodeDDS(t,image)==t.dds,"Nondeterministic DDS");
        Target target{"alf","alf_01","graphics/vehicles/decals/alf_01/batchTextures.brres",{},t,Json::array()};
        auto &b=target.batch;for(uint32_t n:{0x54424154u,0xffeeu,4u,59u,1u,7u,uint32_t(t.dds.size()),0u,0u,uint32_t(t.dds.size()),8u,8u,2u})put32(b,n);append(b,"alf_01");b.push_back(0);b.insert(b.end(),t.dds.begin(),t.dds.end());
        auto dir=root/format;auto result=exportMod(target,nullptr,dir);require(!fs::exists(dir/"vfs.zip") && !fs::exists(dir/"vfs.dat"),"Nested archives exported");require(read((dir/"assets"/lower(target.member)).u8string())==b,"No-op changed original bytes");
        // Loader caches are disposable only when complete and hash-verified.
        auto cache=archivePair(lower(target.member),b);
        write(dir/".reriff-cache-test.dat",cache.first);write(dir/".reriff-cache-test.zip",cache.second);
        auto receipt=std::string("reriff-loose-cache-v1\n")+sha(cache.first)+"\n"+sha(cache.second);
        write(dir/".reriff-cache-test.receipt",Bytes(receipt.begin(),receipt.end()));
        image.pixels[0]=64;exportMod(target,&image,dir);
        require(!fs::exists(dir/".reriff-cache-test.zip"),"Stale loader cache retained on changed export");
        auto owned=dir/"assets"/lower(target.member);auto saved=read(owned.u8string());
        write(owned,Bytes{1});rejects([&]{exportMod(target,nullptr,dir);});write(owned,saved);
        auto legacy=root/(std::string("legacy-")+format);fs::create_directory(legacy);
        write(legacy/"vfs.dat",cache.first);write(legacy/"vfs.zip",cache.second);
        save(legacy/"livery-maker.json",{{"tool","reriff-livery-maker-v1"},{"dat_sha256",sha(cache.first)},{"zip_sha256",sha(cache.second)}});
        exportMod(target,nullptr,legacy);require(!fs::exists(legacy/"vfs.zip") && fs::exists(legacy/"assets"/lower(target.member)),"Legacy migration failed");
        auto duplicate=conflicts(root,dir,lower(target.member));require(!duplicate.empty(),"Loose conflict missed");
        auto hidden=root/".disabled";fs::create_directories(hidden);write(hidden/"vfs.zip",cache.second);
        auto withoutHidden=conflicts(root,dir,lower(target.member));require(withoutHidden==duplicate,"Disabled/cache archive entered conflicts");

        auto payload=read((dir/"assets"/lower(target.member)).u8string());require(std::equal(b.begin(),b.begin()+59+128,payload.begin()),"Metadata changed");
        write(dir/"unrelated.txt",Bytes{1});rejects([&]{exportMod(target,nullptr,dir);});
        fs::create_directory(root/"unowned");rejects([&]{exportMod(target,nullptr,root/"unowned");});
        directoryLink(root/"unowned",root/"alias");rejects([&]{exportMod(target,nullptr,root/"alias");});fs::remove(root/"alias");
        directoryLink(root/"unowned",root/"alias");
        auto aliasOutput=root/"alias"/format/"mod";
        exportMod(target,nullptr,aliasOutput);exportMod(target,&image,aliasOutput);
        require(fs::is_regular_file(root/"unowned"/format/"mod"/"assets"/lower(target.member)),"Export through parent alias failed");
        require(safePath(aliasOutput)==resolveOutputParent(root/"unowned"/format)/"mod","Output alias was not resolved");
        directoryLink(root/"unowned"/format/"mod",root/"owned-link");
        rejects([&]{exportMod(target,nullptr,root/"owned-link");});fs::remove(root/"owned-link");
        directoryLink(root/"missing",root/"dangling");
        rejects([&]{exportMod(target,nullptr,root/"dangling");});
        rejects([&]{exportMod(target,nullptr,root/"dangling"/"mod");});fs::remove(root/"dangling");
        fs::remove(root/"alias");
        image.pixels[0]=255;
    }
    savePNG(root/"good.png",image);require(loadPNG(root/"good.png",8,8).pixels==image.pixels,"PNG round trip");rejects([&]{loadPNG(root/"good.png",9,8);});
    auto broken=read((root/"good.png").u8string());broken.resize(40);write(root/"bad.png",broken);rejects([&]{loadPNG(root/"bad.png",8,8);});rejects([&]{archivePair("../escape",Bytes{1});});rejects([&]{safePath(root/".."/"oops");});
    if(argc==2) {
        Archive original((fs::u8path(argv[1])/"vfs.zip").u8string());unsigned count=0;
        auto names=displayNames(original);
        PaintConfig paintConfig(original);auto selectionList=paintSelections(original,names);
        auto baseline=paintConfig.original();require(!paintConfig.changed(),"Paint config changed on load");
        auto luaBaseline=asset(original,"scripts/config/paint_PC.luac");
        require(paintConfig.luaBytes()==luaBaseline,"No-op Lua export changed stock chunk");
        auto offsets=paintConfig.values(selectionList[0],"col0",3);
        paintConfig.set(selectionList[0],"col0",{25,230,50});
        require(paintConfig.get(selectionList[0],"col0",3)==std::vector<float>({25,230,50}),"Color change lost");
        std::set<size_t> allowed;for(auto offset:offsets)for(size_t i=0;i<4;i++)allowed.insert(offset+i);
        for(size_t i=0;i<baseline.size();i++)if(!allowed.count(i))require(baseline[i]==paintConfig.bytes()[i],"Unrelated paint byte changed");
        (void)Config(paintConfig.bytes()).json();
        rejects([&]{paintConfig.set(selectionList[0],"packPrice",{1,2,3});});
        rejects([&]{paintConfig.set(selectionList[0],"col0",{0,-1,256});});
        rejects([&]{paintConfig.set(selectionList[0],"col0",{0,std::numeric_limits<float>::quiet_NaN(),0});});
        paintConfig.set(selectionList[6],"alloysCol0",{100,150,200});
        paintConfig.reset(selectionList[0]);require(paintConfig.changed(),"Reset discarded another paint's edit");
        paintConfig.reset(selectionList[6]);require(paintConfig.bytes()==baseline,"Reset did not restore original bytes");
        require(paintConfig.luaBytes()==luaBaseline,"Reset did not restore original Lua");
        auto added=newDecal(original,selectionList[0]);
        require(added.name=="alf_03" && added.texture.name=="alf_03","New decal collides with stock texture");
        require(newDecal(original,selectionList[1],{"alf_03"}).name=="alf_04","New decals collide with each other");
        rejects([&]{newDecal(original,selectionList[6]);});
        require(newDecal(original,selectionList[0],{"alf_03","alf_04","alf_05","alf_06","alf_07","alf_08","alf_09"}).name=="alf_10","Two-digit ID allocation failed");
        std::set<std::string> full;for(unsigned n=1;n<=99;n++)full.insert("alf_"+std::string(n<10?"0":"")+std::to_string(n));
        rejects([&]{newDecal(original,selectionList[0],full);});
        paintConfig.setLivery(selectionList[0],3);
        auto liveryOffset=paintConfig.values(selectionList[0],"livery",1).at(0);
        for(size_t i=0;i<baseline.size();i++)if(i<liveryOffset || i>=liveryOffset+4)
            require(baseline[i]==paintConfig.bytes()[i],"Decal assignment changed unrelated config bytes");
        require(paintConfig.get(selectionList[0],"livery",1)==std::vector<float>{3},"Livery field not assigned");
        require(u32(paintConfig.luaBytes(),28)==u32(luaBaseline,28)+8,"Scalar Lua assignment instruction count");
        paintConfig.reset(selectionList[0]);require(paintConfig.luaBytes()==luaBaseline,"Decal reset changed original Lua");
        unsigned supported=0;std::set<std::string> reserved;
        for(const auto &paint:selectionList)if(paint.resource.empty()) {
            auto created=newDecal(original,paint,reserved);reserved.insert(created.name);
            require(created.car==paint.car && created.name.substr(0,3)==paint.car,"Wrong new decal car");
            require(decodeTextures(created.batch)[0].name==created.name,"Invalid new native texture name");
            auto model=decodeModel(asset(original,"graphics/vehicles/"+paint.car+"/lod0_livery.bdae"));
            require(!model.meshes.empty(),"Missing selected-car UV model");
            paintConfig.setLivery(paint,unsigned(std::stoi(created.name.substr(4))));
            require(paintConfig.get(paint,"livery",1).at(0)==std::stoi(created.name.substr(4)),"New livery assignment failed");
            ++supported;
        }
        require(supported==std::count_if(selectionList.begin(),selectionList.end(),[](const auto &p){return p.resource.empty();}),"Missing undecorated paint coverage");
        (void)paintConfig.luaBytes();
        for(const auto &paint:selectionList)paintConfig.reset(paint);
        require(paintConfig.bytes()==baseline && paintConfig.luaBytes()==luaBaseline,"All-paint reset failed");
        std::cout<<"PASS isolated decal IDs, scalar-only CFG/Lua changes, reset, and "<<supported<<" eligible paints\n";
        paintConfig.set(selectionList[0],"col0",{25,230,50});
        auto decal=target(original,"alf_01");
        auto combined=root/"combined";
        auto files=paintConfig.files();files[lower(decal.member)]=decal.batch;
        auto mixed=publishMod(files,{{"test","combined"}},combined);
        require(loadJson(combined/"livery-maker.json")["resources"].size()==3,"Combined export lost a resource");
        require(read((combined/"assets"/PaintConfig::resource).u8string())==paintConfig.bytes(),"Paint bytes lost in export");
        require(read((combined/"assets"/lower(decal.member)).u8string())==decal.batch,"Combined export changed decal");
        require(read((combined/"assets/scripts/config/paint_pc.luac").u8string())==paintConfig.luaBytes(),"Runtime paint script lost in export");
        auto luaEdited=paintConfig.luaBytes();auto oldCount=u32(luaBaseline,28);
        require(u32(luaEdited,28)==oldCount+16,"Wrong Lua patch instruction count");
        require(std::equal(luaBaseline.begin()+32,luaBaseline.begin()+32+(oldCount-1)*4,luaEdited.begin()+32),"Stock Lua instructions changed");
        auto config=cfgValue(Json::parse(Config(read((combined/"assets"/PaintConfig::resource).u8string())).json()));
        require(config["Paints"]["alf"][0]["col0"][1]==230,"Exported paint parse mismatch");
        std::cout<<"PASS native paint RGB edits, exact unrelated-byte preservation, per-paint reset and combined archive export\n";
        auto selections=paintSelections(original,names);std::set<std::string> selectableCars;
        unsigned pulse=0,rocketShared=0,withoutDecal=0;
        for(const auto &paint:selections) {
            selectableCars.insert(paint.car);
            require(paint.car!="dod","Unselectable Goliath leaked into selector");
            require(paint.paintName.find("name unavailable")==std::string::npos,"Selectable paint lacks localization");
            if(paint.car=="alf")++pulse;
            if(paint.resource=="bui_02")++rocketShared;
            if(paint.resource.empty())++withoutDecal;
        }
        require(selections.size()==110 && selectableCars.size()==15 && pulse==9 && rocketShared==6 && withoutDecal>0,"Native paint selection coverage mismatch");
        require(selections[6].paintName=="Carmine" && selections[6].resource=="alf_01","Carmine selection targets wrong decal");
        require(selections[7].paintName=="Glacier" && selections[8].paintName=="Cobalt" && selections[7].resource==selections[8].resource,"Shared paints not represented independently");
        std::cout<<"PASS all 110 selectable paints across 15 cars, shared mappings and no-decal selections\n";
        require(localized(names,"car_alf")=="Pulse" && localized(names,"alfPack7")=="Carmine","Original display names mismatch");
        require(displayLabel(target(original,"alf_01"),names)=="Pulse — Carmine","Display label mismatch");
        require(paintNames(target(original,"alf_02"),names).find(" / ")!=std::string::npos,"Shared paint names missing");
        for(const auto &id:targets(original)) {
            auto t=target(original,id);auto image=decodeDDS(t.texture);auto dds=encodeDDS(t.texture,image);
            require(names.count("car_"+t.car)==1,"Missing car display name");
            for(const auto &pack:t.packs) {
                auto key=lower(pack.at("name").get<std::string>());
                require(names.count(key)==1 || key=="dodpack2" || key=="dodpack3","Unexpected missing paint display name");
                if(!names.count(key))require(paintNames(t,names).find("Unnamed paint (")!=std::string::npos,"Missing-name fallback absent");
            }
            require(dds.size()==t.texture.dds.size(),"Original mip layout changed");
            require(std::equal(dds.begin(),dds.begin()+128,t.texture.dds.begin()),"Original DDS header changed");
            auto model=decodeModel(asset(original,"graphics/vehicles/"+t.car+"/lod0_livery.bdae"));require(!model.meshes.empty(),"Missing UV model");++count;
        }
        require(count==30,"Expected original 30 decal targets");std::cout<<"PASS all 30 original decal layouts and UV models\n";
    }
    fs::remove_all(root);std::cout<<"PASS BC2/BC3 alpha, RGB, mips, PNG failures, no-op, metadata preservation, ownership and path refusal\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
