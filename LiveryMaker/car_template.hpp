#pragma once
#include "model_import.hpp"
namespace livery {
struct CarTemplate {Json manifest;std::map<std::string,Bytes> assets;};
inline int roleFromName(const std::string &name){
    for(int i=0;i<=importer::Ignore;i++)if(importer::roleName(i)==name)return i;
    throw std::runtime_error("Unknown template part role: "+name);
}
inline CarTemplate loadCarTemplate(const fs::path &folder){
    CarTemplate result;result.manifest=loadJson(folder/"car-template.json");
    require(result.manifest.value("format","")=="reriff-car-template-v1","Unsupported car template");
    for(const auto &entry:result.manifest.value("resources",Json::array())){
        auto path=entry.at("path").get<std::string>();resourcePath(path);
        auto file=folder/"assets"/fs::u8path(path);require(fs::is_regular_file(file),"Template resource is missing: "+path);
        auto data=read(file.u8string());require(sha(data)==entry.at("sha256").get<std::string>(),"Template resource changed: "+path);
        result.assets[path]=std::move(data);
    }
    return result;
}
inline void writeCarTemplate(const fs::path &folder,Json manifest,std::map<std::string,Bytes> assets){
    auto dest=safePath(fs::absolute(folder));require(!fs::exists(dest),"Choose a new folder for the car template");
    normalizeAssetKeys(assets,false);manifest["format"]="reriff-car-template-v1";manifest["resources"]=Json::array();
    fs::create_directories(dest/"assets");
    for(auto &entry:assets){resourcePath(entry.first);auto path=dest/"assets"/fs::u8path(entry.first);fs::create_directories(path.parent_path());write(path,entry.second);manifest["resources"].push_back({{"path",entry.first},{"sha256",sha(entry.second)}});}
    save(dest/"car-template.json",manifest);
}
inline void rebuildFromTemplate(const Archive &stock,const CarTemplate &tpl,const fs::path &glb,const fs::path &destination,importer::Options::Quality quality){
    require(tpl.manifest.contains("import_recipe")&&tpl.manifest.at("import_recipe").is_object(),"This car template has no import recipe");
    const auto &recipe=tpl.manifest.at("import_recipe");
    auto source=importer::readGLBFile(glb);
    auto expected=tpl.manifest.value("source_sha256",recipe.value("source_sha256",std::string()));
    require(expected.empty()||expected==source.hash,"Source GLB does not match the template. Expected "+expected+", got "+source.hash);
    auto donorId=recipe.value("donor_id",std::string());require(!donorId.empty(),"Template has no donor car");
    auto candidates=importer::donors(stock,source.bounds.size());auto donor=std::find_if(candidates.begin(),candidates.end(),[&](const importer::Donor &d){return d.id==donorId;});require(donor!=candidates.end(),"Donor car is not available: "+donorId);
    if(recipe.contains("parts")&&recipe["parts"].is_array())for(const auto &part:recipe["parts"]){
        auto index=part.value("index",size_t(-1));if(index>=source.parts.size()){for(size_t i=0;i<source.parts.size();i++)if(source.parts[i].name==part.value("part","")&&source.parts[i].material==part.value("material","")){index=i;break;}}
        if(index<source.parts.size()&&part.contains("role"))source.parts[index].role=roleFromName(part.at("role").get<std::string>());
    }
    // Templates saved before material-first classification kept wheel-arch
    // chrome as rolling wheels. Demote that trim so it stays on the body.
    for(auto &part:source.parts){auto name=lower(part.name),material=lower(part.material);bool trim=name.find("chromewheel")!=std::string::npos||name.find("wheelarch")!=std::string::npos;bool chrome=material.find("chrome")!=std::string::npos||material.find("metal")!=std::string::npos;if(part.role==importer::Wheels&&trim&&chrome)part.role=importer::Metal;}
    importer::Options options;auto values=recipe.value("options",Json::object());
    options.scale=values.value("source_scale",1.0);options.yaw=values.value("quarter_turns",0);options.zUp=values.value("z_up",false);options.fit=values.value("fit_to_donor",true);
    options.decimate=values.value("decimate",true);options.fitCollision=values.value("fit_collision",true);options.buildEffects=values.value("build_effects",true);options.neutralMask=values.value("neutral_mask",true);
    options.addPaint=values.value("add_paint",true);options.paintName=values.value("paint_name",std::string("Black"));options.allLocaleNames=values.value("all_locale_names",true);options.fitShadow=values.value("fit_shadow",true);
    options.quality=quality;options.keepPartColors=values.value("keep_part_colors",false);options.bodyHeightScale=values.value("body_height_scale",1.0);options.bodyHeightOffset=values.value("body_height_offset",0.0);
    auto plan=importer::defaultPlan(source,donor->id,options.keepPartColors);
    if(recipe.contains("materials")&&recipe["materials"].is_array())for(const auto &sourceMaterial:recipe["materials"]){
        auto savedName=sourceMaterial.value("name",std::string());auto found=std::find_if(plan.materials.begin(),plan.materials.end(),[&](const importer::MaterialBinding &binding){return binding.name==savedName;});if(found==plan.materials.end())continue;auto &binding=*found;
        binding.category=sourceMaterial.value("category",binding.category);binding.templateMaterial=sourceMaterial.value("template",binding.templateMaterial);binding.texture=sourceMaterial.value("texture",binding.texture);
        binding.finish=sourceMaterial.value("finish",binding.finish);binding.importTexture=sourceMaterial.value("import_texture",binding.importTexture);binding.image=sourceMaterial.value("image",binding.image);binding.flags=sourceMaterial.value("flags",binding.flags);binding.solid=sourceMaterial.value("solid",binding.solid);auto color=sourceMaterial.value("color",Json::array());if(color.is_array()&&color.size()==4)for(int c=0;c<4;c++)binding.color[c]=color[c].get<double>();
        auto base=sourceMaterial.value("base_size",sourceMaterial.value("size",binding.size));binding.size=importer::qualityTextureSize(base,binding.category,quality);
    }
    auto authored=tpl.manifest.value("authored_id",recipe.value("authored_id",donor->id));auto name=tpl.manifest.value("name",recipe.value("name",std::string("Imported car")));
    bool overwrite=recipe.value("overwrite",0)!=0;auto packageId=recipe.value("package_id",std::string());
    importer::exportModel(stock,source,*donor,options,destination,name,overwrite,packageId,&plan,authored);
    for(const auto &entry:tpl.assets){auto path=destination/"assets"/fs::u8path(entry.first);fs::create_directories(path.parent_path());write(path,entry.second);}
    if(tpl.manifest.contains("origin")&&tpl.manifest.at("origin").is_string()){auto originData=tpl.manifest.at("origin").get<std::string>();write(destination/"origin.txt",Bytes(originData.begin(),originData.end()));}
}
}
