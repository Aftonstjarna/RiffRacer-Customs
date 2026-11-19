#pragma once
#include "mod_editor.hpp"
namespace livery {
// Swap one original native HUD atlas, without recompressing either texture.
inline std::string hudTexturePath(const std::string&car){require(car.size()==3&&std::all_of(car.begin(),car.end(),[](unsigned char c){return std::isalnum(c);}),"Invalid car ID");return "graphics/be/hud/car/"+car+"/batchTextures.brres";}
inline Bytes multiplierTexture(const Bytes&batch){auto textures=decodeTextures(batch);for(auto&t:textures)if(t.name=="multiplier")return t.dds;throw std::runtime_error("HUD batch has no multiplier texture");}
inline Bytes multiplierStyleBatch(const Bytes&original,const Bytes&donor){
 auto textures=decodeTextures(original),replacement=decodeTextures(donor);auto selected=std::find_if(replacement.begin(),replacement.end(),[](const Texture&t){return t.name=="multiplier";});require(selected!=replacement.end(),"Style source has no multiplier atlas");auto target=std::find_if(textures.begin(),textures.end(),[](const Texture&t){return t.name=="multiplier";});require(target!=textures.end(),"This car has no supported multiplier atlas");require(target->width==selected->width&&target->height==selected->height&&target->fourcc==selected->fourcc&&target->mips==selected->mips&&target->flags==selected->flags&&target->caps==selected->caps,"Incompatible multiplier atlas layout");
 // Native variants have the same DDS layout. Refuse unknown layouts rather than
 // rewriting unrelated offsets or data in an imported/custom HUD batch.
 size_t index=std::distance(textures.begin(),target),record=28+index*24,begin=u32(original,12)+u32(original,record+4),size=u32(original,record+8);require(size==selected->dds.size(),"Incompatible multiplier payload size");Bytes out=original;std::copy(selected->dds.begin(),selected->dds.end(),out.begin()+begin);auto checked=decodeTextures(out);for(size_t i=0;i<textures.size();i++)require(checked[i].dds==(i==index?selected->dds:textures[i].dds),"Unexpected HUD texture change");return out;
}
inline Bytes stockHudStyle(const Archive&stock,int style){require(style==0||style==1,"Unknown multiplier style");auto path=hudTexturePath(style==0?"mot":"alf");for(auto&e:stock.entries)if(lower(e.name)==lower(path))return stock.load(e);throw std::runtime_error("Original multiplier style resource is missing");}
inline int identifyMultiplierStyle(const Archive&stock,const Bytes&batch){auto dds=multiplierTexture(batch);for(int style=0;style<2;style++)if(dds==multiplierTexture(stockHudStyle(stock,style)))return style;return -1;}
}
