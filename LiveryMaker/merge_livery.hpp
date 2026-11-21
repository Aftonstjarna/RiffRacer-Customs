#pragma once
#include "mod_editor.hpp"
namespace livery {
struct MergedLivery {Json metadata;std::map<std::string,Bytes> files;};
inline MergedLivery mergeLiveryMods(const Archive &stock,const std::vector<fs::path> &folders){
    require(folders.size()>=2,"Choose at least two livery mods");
    auto stockCfg=asset(stock,"scripts/config/paint_PC.cfg"),stockLua=asset(stock,"scripts/config/paint_PC.luac");
    auto roots=Json::parse(Config(stockCfg).json());std::vector<ConfigAssignment> luaPatches;Json merged=Json::array();std::map<std::string,Bytes> files;
    std::set<std::pair<std::string,int>> targets;
    std::string firstCar;int firstPack=0;
    for(const auto &folder:folders){
        auto owner=loadJson(folder/"livery-maker.json");require(owner.value("tool","").rfind("reriff-livery-maker-",0)==0,"Not a livery mod: "+folder.u8string());
        auto car=owner.at("selected_car").get<std::string>();int pack=owner.at("selected_pack_index").get<int>();require(pack>=0,"Invalid selected paint");
        require(targets.insert({car,pack}).second,"Two selected livery mods target the same car paint: "+car+" pack "+std::to_string(pack+1));
        if(firstCar.empty()){firstCar=car;firstPack=pack;}
        auto cfgPath=folder/"assets/scripts/config/paint_pc.cfg";require(fs::is_regular_file(cfgPath),"Paint config missing from "+folder.u8string());
        auto cfg=read(cfgPath.u8string());auto modRoots=Json::parse(Config(cfg).json());auto &modPaints=configNamed(modRoots,"Paints");auto &modCar=configNamed(modPaints["value"],car);auto &modPacks=modCar["value"];
        require(pack<int(modPacks.size()),"Paint index outside "+folder.u8string());
        Json packNode=modPacks[pack];require(packNode.at("type")==2,"Invalid paint pack");
        auto &basePaints=configNamed(roots,"Paints");auto &baseCar=configNamed(basePaints["value"],car);auto &basePacks=baseCar["value"];require(pack<int(basePacks.size()),"Paint index outside stock config");basePacks[pack]=packNode;
        luaPatches.push_back({{"Paints",car,pack+1},cfgValue(packNode.at("value"))});
        merged.push_back({{"folder",folder.filename().u8string()},{"car",car},{"pack_index",pack}});
        for(const auto &resource:owner.value("resources",Json::array())){
            auto path=resource.at("path").get<std::string>();resourcePath(path);if(path.rfind("scripts/config/",0)==0)continue;
            auto source=folder/"assets"/fs::u8path(path);require(fs::is_regular_file(source),"Livery resource missing: "+path);
            auto data=read(source.u8string());require(sha(data)==resource.at("sha256").get<std::string>(),"Livery resource changed: "+path);
            auto found=files.find(path);if(found!=files.end()){require(found->second==data,"Two livery mods provide different "+path);continue;}files[path]=std::move(data);
        }
    }
    files["scripts/config/paint_pc.cfg"]=configEncode(roots);files["scripts/config/paint_pc.luac"]=configLuaPatch(stockLua,luaPatches);
    MergedLivery result;result.files=std::move(files);
    result.metadata={{"paint_source_sha256",sha(stockCfg)},{"paint_configuration_modified",true},{"selected_car",firstCar},{"selected_pack_index",firstPack},{"merged_from",merged}};
    return result;
}
}
