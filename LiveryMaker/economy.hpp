#pragma once
#include "mod_editor.hpp"
#include "config_author.hpp"
namespace livery {
class UnlockConfig {
    Bytes original_,lua_;Json originalTree_,tree_;std::map<std::string,Json> edits_;
    size_t index(const std::string&car)const{auto tree=tree_;auto names=configNamed(tree,"carNames").at("value");for(size_t i=0;i<names.size();i++)if(names[i].at("value")==car)return i;throw std::runtime_error("Car has no native unlock slot");}
public:
    UnlockConfig(const ImportedMod&mod,const Archive&stock):original_(mod.assetBytes("scripts/config/unlocks_pc.cfg",stock)),lua_(mod.assetBytes("scripts/config/unlocks_pc.luac",stock)),originalTree_(Json::parse(Config(original_).json())),tree_(originalTree_){}
    bool supports(const std::string&car)const{try{(void)index(car);return true;}catch(const std::exception&){return false;}}
    Json get(const std::string&car)const{auto t=tree_;auto items=configNamed(t,"carUnlocks").at("value");auto i=index(car);if(i>=items.size()||items[i].at("type")!=2)return Json::object();return cfgValue(items[i].at("value"));}
    static std::string mode(const Json&v){if(v.value("unlockedAtStart",0.)!=0)return "available";if(v.contains("artistUnlock"))return v.contains("songUnlock")?"artist-title":"artist";if(v.value("purchaseUnlock",0.)>0)return "shop";if(v.value("playsUnlock",0.)>0)return "play";return "original";}
    void reset(const std::string&car){auto i=index(car);configNamed(tree_,"carUnlocks")["value"][i]=configNamed(originalTree_,"carUnlocks")["value"][i];edits_.erase(car);}
    void set(const std::string&car,const std::string&mode,int price,const std::string&artist,const std::string&title){
        auto i=index(car);require(mode=="shop"||mode=="artist"||mode=="artist-title"||mode=="available"||mode=="play","Unsupported unlock type");require(price>=1&&price<=10000000,"Car price must be 1–10,000,000");Json value={{"unlockedAtStart",mode=="available"}};
        if(mode=="shop")value["purchaseUnlock"]=price;
        else if(mode=="play")value["playsUnlock"]=1;
        else if(mode=="artist"||mode=="artist-title"){
            auto valid=[](const std::string&s){require(!s.empty()&&s.size()<=200&&std::any_of(s.begin(),s.end(),[](unsigned char c){return c>' ';}),"Enter an artist/title (1–200 bytes)");for(unsigned char c:s)require(c>=32&&c!=127,"Control character in artist/title");};valid(artist);value["artistUnlock"]=artist;if(mode=="artist-title"){valid(title);value["songUnlock"]=title;}
        }
        Json fields=Json::array();for(auto it=value.begin();it!=value.end();++it)fields.push_back(configScalar(it.key(),it.value().is_boolean()?Json(it.value().get<bool>()?1:0):it.value()));
        auto&items=configNamed(tree_,"carUnlocks")["value"];require(i<items.size(),"Missing native unlock entry");items[i]["type"]=2;items[i]["value"]=fields;edits_[car]=value;
    }
    std::map<std::string,Bytes> files()const{if(edits_.empty())return {};std::vector<ConfigAssignment> patches;for(auto&e:edits_)patches.push_back({{"carUnlocks",index(e.first)},e.second});auto cfg=configEncode(tree_),lua=configLuaPatch(lua_,patches);auto marker=std::string("reriff-customs-unlocks-v1\n")+sha(cfg)+"\n"+sha(lua)+"\n";return {{"assets/scripts/config/unlocks_pc.cfg",cfg},{"assets/scripts/config/unlocks_pc.luac",lua},{"assets/scripts/config/customs_unlocks.txt",Bytes(marker.begin(),marker.end())}};}
};
}
