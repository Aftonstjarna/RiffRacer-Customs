#pragma once
#include "mod_editor.hpp"
namespace livery {
class ModAppearance {
    const ImportedMod &mod_;const Archive &stock_;
    std::map<std::string,Target> decals_;
    std::set<std::string> used_;
    static std::string key(const PaintSelection &p){return p.car+"/"+std::to_string(p.packIndex);}
    Bytes load(const std::string &name) const{return mod_.assetBytes(name,stock_);}
public:
    std::unique_ptr<PaintConfig> colors;
    std::vector<PaintSelection> paints;
    ModAppearance(const ImportedMod &mod,const Archive &stock,const std::map<std::string,std::string> &labels):mod_(mod),stock_(stock){
        auto cfg=load("scripts/config/paint_pc.cfg");auto table=cfgValue(Json::parse(Config(cfg).json())).at("Paints");
        for(auto car=table.begin();car!=table.end();++car)for(size_t i=0;i<car.value().size();i++){
            const auto &pack=car.value().at(i);int id=pack.value("livery",-1);std::string resource;
            if(id>0){resource=car.key()+"_"+(id<10?"0":"")+std::to_string(id);used_.insert(resource);}
            paints.push_back({car.key(),localized(labels,"car_"+car.key()),localized(labels,pack.value("packName",std::string())),resource,i});
        }
        for(auto &name:targets(stock))used_.insert(lower(name));
        for(auto &file:mod.files){auto name=lower(file.first);const std::string prefix="assets/graphics/vehicles/decals/";if(name.rfind(prefix,0)==0)used_.insert(name.substr(prefix.size(),6));}
        colors=std::make_unique<PaintConfig>(cfg,load("scripts/config/paint_pc.luac"),paints);
    }
    PaintSelection selection(const std::string &car,size_t index) const {for(const auto &p:paints)if(p.car==car&&p.packIndex==index)return p;throw std::runtime_error("Unknown imported paint");}
    std::unique_ptr<Target> decal(const PaintSelection &p) const {
        auto edited=decals_.find(key(p));if(edited!=decals_.end())return std::make_unique<Target>(edited->second);
        auto id=colors->get(p,"livery",1);if(id.empty()||id[0]<1)return {};
        auto name=p.car+"_"+(id[0]<10?"0":"")+std::to_string(int(id[0]));return std::make_unique<Target>(targetFrom([&](const std::string &path){return load(path);},name));
    }
    Target imageTemplate(const PaintSelection &p) const {auto current=decal(p);return current?*current:target(stock_,"alf_01");}
    void importPNG(const PaintSelection &p,const Image &image){importImage(p,imageTemplate(p),image);}
    void importImage(const PaintSelection &p,Target result,const Image &image){
        auto existing=decals_.find(key(p));std::string id;
        if(existing!=decals_.end())id=existing->second.name;
        else for(int n=1;n<=99;n++){auto candidate=p.car+"_"+(n<10?"0":"")+std::to_string(n);if(!used_.count(candidate)){id=candidate;break;}}
        require(!id.empty(),"No unused decal ID for this car");require(u32(result.batch,12)>=59&&text(result.batch,52,7)==result.name,"Unsupported decal name pool");
        std::copy(id.begin(),id.end(),result.batch.begin()+52);result.car=p.car;result.name=id;result.member="graphics/vehicles/decals/"+id+"/batchTextures.brres";result.texture=decodeTextures(result.batch).at(0);
        result.batch=editedBatch(result,&image);result.texture=decodeTextures(result.batch).at(0);result.packs=Json::array({{{"index",p.packIndex},{"name",p.paintName}}});
        auto undecorated=p;undecorated.resource.clear();colors->setLivery(undecorated,unsigned(std::stoi(id.substr(4))));decals_[key(p)]=std::move(result);used_.insert(id);
    }
    void makeDefault(const PaintSelection &p){
        if(p.packIndex==0)return;
        auto source=decal(p);auto dest=selection(p.car,0);auto before=*colors;auto oldDecals=decals_;auto oldUsed=used_;
        try{colors->copyAppearance(p,dest);if(source)importImage(dest,*source,decodeDDS(source->texture));else remove(dest);}
        catch(...){*colors=std::move(before);decals_=std::move(oldDecals);used_=std::move(oldUsed);throw;}
    }
    void remove(const PaintSelection &p){colors->clearLivery(p);decals_.erase(key(p));}
    void reset(const PaintSelection &p){colors->reset(p);decals_.erase(key(p));}
    std::map<std::string,Bytes> files()const{
        std::map<std::string,Bytes> result;if(colors->changed())for(const auto &f:colors->files())result["assets/"+f.first]=f.second;
        for(const auto &entry:decals_)result["assets/"+lower(entry.second.member)]=entry.second.batch;
        return result;
    }
};
}
