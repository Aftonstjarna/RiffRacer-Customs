#pragma once
#include "mod_editor.hpp"
namespace livery {
// Swap one original native HUD atlas texture, without recompressing either image.
inline std::string hudTexturePath(const std::string&car){require(car.size()==3&&std::all_of(car.begin(),car.end(),[](unsigned char c){return std::isalnum(c);}),"Invalid car ID");return "graphics/be/hud/car/"+car+"/batchTextures.brres";}
inline Bytes hudStyleTexture(const Bytes&batch,const std::string&name){auto textures=decodeTextures(batch);for(auto&t:textures)if(t.name==name)return t.dds;throw std::runtime_error("HUD batch has no "+name+" texture");}
inline Bytes multiplierTexture(const Bytes&batch){return hudStyleTexture(batch,"multiplier");}
inline Bytes meterTexture(const Bytes&batch){return hudStyleTexture(batch,"car_meter");}
inline Bytes hudStyleBatch(const Bytes&original,const Bytes&donor,const std::string&name){
 auto textures=decodeTextures(original),replacement=decodeTextures(donor);auto selected=std::find_if(replacement.begin(),replacement.end(),[&](const Texture&t){return t.name==name;});require(selected!=replacement.end(),"Style source has no "+name+" texture");auto target=std::find_if(textures.begin(),textures.end(),[&](const Texture&t){return t.name==name;});require(target!=textures.end(),"This car has no supported "+name+" atlas");require(target->width==selected->width&&target->height==selected->height&&target->fourcc==selected->fourcc&&target->mips==selected->mips&&target->flags==selected->flags&&target->caps==selected->caps,"Incompatible "+name+" atlas layout");
 // Native variants have the same DDS layout. Refuse unknown layouts rather than
 // rewriting unrelated offsets or data in an imported/custom HUD batch.
 size_t index=std::distance(textures.begin(),target),record=28+index*24,begin=u32(original,12)+u32(original,record+4),size=u32(original,record+8);require(size==selected->dds.size(),"Incompatible "+name+" payload size");Bytes out=original;std::copy(selected->dds.begin(),selected->dds.end(),out.begin()+begin);auto checked=decodeTextures(out);for(size_t i=0;i<textures.size();i++)require(checked[i].dds==(i==index?selected->dds:textures[i].dds),"Unexpected HUD texture change");return out;
}
inline Bytes multiplierStyleBatch(const Bytes&original,const Bytes&donor){return hudStyleBatch(original,donor,"multiplier");}
inline Bytes meterStyleBatch(const Bytes&original,const Bytes&donor){return hudStyleBatch(original,donor,"car_meter");}
inline Bytes stockHudStyle(const Archive&stock,int style){require(style==0||style==1,"Unknown multiplier style");auto path=hudTexturePath(style==0?"mot":"alf");for(auto&e:stock.entries)if(lower(e.name)==lower(path))return stock.load(e);throw std::runtime_error("Original multiplier style resource is missing");}
inline int identifyMultiplierStyle(const Archive&stock,const Bytes&batch){auto dds=multiplierTexture(batch);for(int style=0;style<2;style++)if(dds==multiplierTexture(stockHudStyle(stock,style)))return style;return -1;}
// Each representative has a distinct native car_meter payload. Cars that share
// an identical meter are represented by the first one in the list.
inline const std::vector<std::string>& stockMeterStyles(){static const std::vector<std::string> cars={"alf","bui","cad","cor","f1r","fab","hon","mer","pha","pl1","rol","rs1","vau"};return cars;}
inline Bytes stockMeterStyle(const Archive&stock,int style){auto &cars=stockMeterStyles();require(style>=0&&style<int(cars.size()),"Unknown boost meter style");auto path=hudTexturePath(cars.at(style));for(auto&e:stock.entries)if(lower(e.name)==lower(path))return stock.load(e);throw std::runtime_error("Original boost meter style resource is missing");}
inline int identifyMeterStyle(const Archive&stock,const Bytes&batch){auto dds=meterTexture(batch);auto &cars=stockMeterStyles();for(int style=0;style<int(cars.size());style++)if(dds==meterTexture(stockMeterStyle(stock,style)))return style;return -1;}
}
