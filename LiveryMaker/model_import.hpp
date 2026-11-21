#pragma once
#include "mod_editor.hpp"
#include "mod_names.hpp"
#include "texture_encode.hpp"
#include "decimate.hpp"
#include "config_author.hpp"
#include <optional>
#include <functional>
namespace livery::importer {
// Static glTF 2.0 input. No executable helpers, external URIs or asset downloads.
using P=std::array<double,3>;
using M=std::array<double,16>;
inline M identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
inline M multiply(const M&a,const M&b){M r{};for(int c=0;c<4;c++)for(int y=0;y<4;y++)for(int k=0;k<4;k++)r[c*4+y]+=a[k*4+y]*b[c*4+k];return r;}
inline P point(const M&m,P p){return {m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]};}
inline P minus(P a,P b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline P crossP(P a,P b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline double dotP(P a,P b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline P unit(P p){double l=std::sqrt(dotP(p,p));return l>1e-15?P{p[0]/l,p[1]/l,p[2]/l}:P{0,1,0};}
struct Bounds {P lo{1e30,1e30,1e30},hi{-1e30,-1e30,-1e30};size_t count=0;void add(P p){for(int k=0;k<3;k++){require(std::isfinite(p[k])&&std::abs(p[k])<1e9,"Invalid or excessive model coordinate");lo[k]=std::min(lo[k],p[k]);hi[k]=std::max(hi[k],p[k]);}count++;}P size()const{require(count>0,"Empty geometry");return minus(hi,lo);}P center()const{return {(lo[0]+hi[0])/2,(lo[1]+hi[1])/2,(lo[2]+hi[2])/2};}};
enum Role {Body,Glass,Metal,Interior,Wheels,Ignore};
inline const char* roleName(int r){static const char* names[]={"Painted body","Glass","Metal / chrome","Interior / plastic","Wheels (split into four)","Ignore"};return names[r];}
struct Part {std::string name,material;int role=Body,materialIndex=-1;std::vector<P> positions,normals;std::vector<std::array<float,2>> uv;std::vector<uint32_t> indices;};
struct SourceMaterial {std::string name;int image=-1;bool alpha=false;std::array<double,4> baseColor{1,1,1,1};};
struct Source {std::string title,author,license,sourceUrl;std::vector<Part> parts;std::vector<SourceMaterial> materials;std::vector<Bytes> images;std::string hash;size_t triangles=0,textures=0;Bounds bounds;};
inline Source readGLB(const Bytes&bytes){
 require(bytes.size()>=20&&bytes.size()<=256*1024*1024&&u32(bytes,0)==0x46546c67&&u32(bytes,4)==2&&u32(bytes,8)==bytes.size(),"Expected a complete GLB 2.0 file (maximum 256 MiB)");
 Json doc;Bytes bin;size_t at=12;bool first=true,haveBin=false;
 while(at<bytes.size()){require(bytes.size()-at>=8,"Truncated GLB chunk");size_t n=u32(bytes,at);auto kind=u32(bytes,at+4);at+=8;require(n%4==0&&n<=bytes.size()-at,"Invalid GLB chunk length");if(first){require(kind==0x4e4f534a,"GLB JSON must be first");doc=Json::parse(bytes.begin()+at,bytes.begin()+at+n);first=false;}else if(kind==0x004e4942){require(!haveBin,"Duplicate BIN chunk");bin.assign(bytes.begin()+at,bytes.begin()+at+n);haveBin=true;}at+=n;}
 require(doc.at("asset").at("version")=="2.0"&&doc.value("extensionsRequired",Json::array()).empty(),"Only core glTF 2.0 is supported; decompress required extensions before import");
 std::string title,author,license,sourceUrl;if(doc.at("asset").contains("extras")&&doc["asset"]["extras"].is_object()){auto &extras=doc["asset"]["extras"];auto read=[&](const char *key,std::string &out){if(extras.contains(key)&&extras[key].is_string())out=extras[key].get<std::string>();};read("title",title);read("author",author);read("license",license);read("source",sourceUrl);}
 require(doc.value("skins",Json::array()).empty()&&doc.value("animations",Json::array()).empty(),"Export a static model without skins or animations");
 require(doc.at("buffers").size()==1&&!doc.at("buffers")[0].contains("uri"),"Use a GLB with an embedded buffer");size_t declared=doc.at("buffers")[0].at("byteLength");require(declared<=bin.size()&&bin.size()-declared<=3,"Invalid GLB buffer length");bin.resize(declared);
 Source source;
 std::vector<int> textureImages;
 for(const auto &texture:doc.value("textures",Json::array())){require(texture.is_object()&&texture.contains("source")&&texture["source"].is_number_unsigned()&&!texture.contains("extensions"),"Unsupported glTF texture");textureImages.push_back(texture["source"].get<int>());}
 for(const auto &image:doc.value("images",Json::array())){require(image.is_object()&&image.contains("bufferView")&&!image.contains("uri"),"Only embedded glTF images are supported");require(image.value("mimeType",std::string())=="image/png","Only PNG glTF textures are supported");auto view=doc.at("bufferViews").at(image.at("bufferView").get<size_t>());size_t start=view.value("byteOffset",size_t(0)),length=view.at("byteLength");require(start<=bin.size()&&length<=bin.size()-start,"Image exceeds buffer");source.images.emplace_back(bin.begin()+start,bin.begin()+start+length);require(source.images.back().size()>8&&source.images.back()[0]==0x89&&source.images.back()[1]=='P'&&source.images.back()[2]=='N'&&source.images.back()[3]=='G',"Invalid embedded PNG");}
 for(size_t i=0;i<doc.value("materials",Json::array()).size();i++){const auto &material=doc.at("materials").at(i);require(material.is_object(),"Invalid glTF material");SourceMaterial parsed;parsed.name=material.value("name",std::string("Material ")+std::to_string(i));parsed.alpha=material.value("alphaMode",std::string())=="BLEND"||material.value("alphaMode",std::string())=="MASK";if(material.contains("pbrMetallicRoughness")){const auto &pbr=material["pbrMetallicRoughness"];if(pbr.contains("baseColorFactor")&&pbr["baseColorFactor"].is_array())for(size_t c=0;c<4&&c<pbr["baseColorFactor"].size();c++)parsed.baseColor[c]=pbr["baseColorFactor"][c].get<double>();if(pbr.contains("baseColorTexture")){auto index=pbr["baseColorTexture"].value("index",size_t(-1));require(index<textureImages.size()&&textureImages[index]>=0&&size_t(textureImages[index])<source.images.size(),"Invalid base color texture");parsed.image=textureImages[index];}}source.materials.push_back(std::move(parsed));}
 auto values=[&](size_t index,int width){const auto&a=doc.at("accessors").at(index);require(!a.contains("sparse")&&!a.value("normalized",false),"Sparse or normalized attributes require baking before import");std::string type=a.at("type");require(type==(width==1?"SCALAR":width==2?"VEC2":"VEC3"),"Unexpected accessor type");int ct=a.at("componentType");size_t component=ct==5126||ct==5125?4:ct==5123?2:ct==5121?1:0;require(component>0&&(width==1||ct==5126),"Unsupported accessor component type");const auto&v=doc.at("bufferViews").at(a.at("bufferView").get<size_t>());require(v.value("buffer",0)==0,"Invalid buffer reference");size_t count=a.at("count"),start=v.value("byteOffset",size_t(0)),length=v.at("byteLength"),offset=a.value("byteOffset",size_t(0)),stride=v.value("byteStride",component*width);require(count>0&&count<=2000000&&stride>=component*width&&stride<=252&&stride%component==0&&start<=bin.size()&&length<=bin.size()-start&&offset<=length&&component*width<=length-offset&&(count-1)<=(length-offset-component*width)/stride,"Accessor exceeds buffer bounds or conversion limit");std::vector<std::vector<double>> result(count,std::vector<double>(width));for(size_t i=0;i<count;i++)for(int k=0;k<width;k++){size_t p=start+offset+i*stride+k*component;double x=ct==5126?f32(bin,p):ct==5125?u32(bin,p):ct==5123?u16(bin,p):bin[p];require(std::isfinite(x),"Nonfinite vertex attribute");result[i][k]=x;}return result;};
 source.title=title;source.author=author;source.license=license;source.sourceUrl=sourceUrl;source.hash=sha(bytes);source.textures=doc.value("textures",Json::array()).size();std::set<size_t> visited;size_t total=0;
 std::function<void(size_t,M,int)> visit=[&](size_t id,M parent,int depth){require(depth<128&&visited.insert(id).second,"Cyclic or multiply referenced scene node");const auto&node=doc.at("nodes").at(id);M local=identity();if(node.contains("matrix")){require(!node.contains("translation")&&!node.contains("rotation")&&!node.contains("scale"),"Node mixes matrix and TRS");local=node.at("matrix").get<M>();}else{auto t=node.value("translation",P{0,0,0}),s=node.value("scale",P{1,1,1});auto q=node.value("rotation",std::array<double,4>{0,0,0,1});double x=q[0],y=q[1],z=q[2],w=q[3];require(std::abs(x*x+y*y+z*z+w*w-1)<.001,"Invalid rotation quaternion");local={ (1-2*y*y-2*z*z)*s[0],(2*x*y+2*w*z)*s[0],(2*x*z-2*w*y)*s[0],0,(2*x*y-2*w*z)*s[1],(1-2*x*x-2*z*z)*s[1],(2*y*z+2*w*x)*s[1],0,(2*x*z+2*w*y)*s[2],(2*y*z-2*w*x)*s[2],(1-2*x*x-2*y*y)*s[2],0,t[0],t[1],t[2],1};}
 for(double x:local){require(std::isfinite(x),"Nonfinite node matrix");}require(std::abs(local[3])+std::abs(local[7])+std::abs(local[11])+std::abs(local[15]-1)<1e-7,"Non-affine node transform");M world=multiply(parent,local);double det=dotP({world[0],world[1],world[2]},crossP({world[4],world[5],world[6]},{world[8],world[9],world[10]}));require(std::abs(det)>1e-12,"Singular node transform");
 if(node.contains("mesh")){const auto&mesh=doc.at("meshes").at(node.at("mesh").get<size_t>());for(const auto&p:mesh.at("primitives")){require(p.value("mode",4)==4&&!p.contains("targets")&&!p.contains("extensions"),"Only static uncompressed triangle primitives are supported");Part part;part.name=node.value("name",mesh.value("name",std::string("Part ")+std::to_string(id)));if(p.contains("material")){part.materialIndex=p.at("material").get<int>();require(part.materialIndex>=0&&size_t(part.materialIndex)<source.materials.size(),"Invalid primitive material");part.material=source.materials[part.materialIndex].name;}std::string hint=lower(part.name+" "+part.material);if(hint.find("wheel")!=std::string::npos||hint.find("tyre")!=std::string::npos||hint.find("tire")!=std::string::npos)part.role=Wheels;else if(hint.find("glass")!=std::string::npos||hint.find("window")!=std::string::npos)part.role=Glass;else if(hint.find("chrome")!=std::string::npos||hint.find("metal")!=std::string::npos)part.role=Metal;else if(hint.find("interior")!=std::string::npos||hint.find("plastic")!=std::string::npos||hint.find("indoor")!=std::string::npos||hint.find("seat")!=std::string::npos)part.role=Interior;
 auto pos=values(p.at("attributes").at("POSITION"),3);total+=pos.size();require(total<=1000000,"Model exceeds one million input vertices; simplify before import");for(auto v:pos){auto value=point(world,{v[0],v[1],v[2]});part.positions.push_back(value);source.bounds.add(value);}if(p.at("attributes").contains("NORMAL")){auto ns=values(p.at("attributes").at("NORMAL"),3);require(ns.size()==pos.size(),"Normal count differs from positions");auto a=crossP({world[4],world[5],world[6]},{world[8],world[9],world[10]}),b=crossP({world[8],world[9],world[10]},{world[0],world[1],world[2]}),c=crossP({world[0],world[1],world[2]},{world[4],world[5],world[6]});for(auto n:ns)part.normals.push_back(unit({(a[0]*n[0]+b[0]*n[1]+c[0]*n[2])/det,(a[1]*n[0]+b[1]*n[1]+c[1]*n[2])/det,(a[2]*n[0]+b[2]*n[1]+c[2]*n[2])/det}));}if(p.at("attributes").contains("TEXCOORD_0")){auto uv=values(p.at("attributes").at("TEXCOORD_0"),2);require(uv.size()==pos.size(),"UV count differs from positions");for(auto v:uv){require(std::abs(v[0])<=65504&&std::abs(v[1])<=65504,"UV exceeds native half-float range");part.uv.push_back({float(v[0]),float(v[1])});}}else part.uv.resize(pos.size(),{0,0});
 if(p.contains("indices")){int ct=doc.at("accessors").at(p.at("indices").get<size_t>()).at("componentType");require(ct==5121||ct==5123||ct==5125,"Indices must be unsigned integers");for(auto v:values(p.at("indices"),1)){require(v[0]<pos.size(),"Triangle index outside vertices");part.indices.push_back(uint32_t(v[0]));}}else for(size_t i=0;i<pos.size();i++)part.indices.push_back(i);require(part.indices.size()%3==0&&!part.indices.empty(),"Invalid triangle list");if(det<0)for(size_t i=0;i<part.indices.size();i+=3)std::swap(part.indices[i+1],part.indices[i+2]);source.triangles+=part.indices.size()/3;require(source.triangles<=500000,"Simplify model below 500,000 triangles");source.parts.push_back(std::move(part));}}
 for(auto child:node.value("children",Json::array()))visit(child.get<size_t>(),world,depth+1);
 };for(auto node:doc.at("scenes").at(doc.value("scene",0)).at("nodes"))visit(node.get<size_t>(),identity(),0);require(!source.parts.empty(),"Scene contains no geometry");return source;
}
inline Source readGLBFile(const fs::path&path){require(fs::is_regular_file(path)&&fs::file_size(path)<=256*1024*1024,"Select a GLB file no larger than 256 MiB");return readGLB(read(path.u8string()));}
inline Bytes stockAsset(const Archive&a,const std::string&name){for(const auto&e:a.entries)if(lower(e.name)==lower(name))return a.load(e);throw std::runtime_error("Missing stock asset: "+name);}
inline bool supportedDonor(const Model&m){size_t bodies=0,wheels=0;bool paint=false;for(auto&n:m.nodes){bodies+=n.name=="body";wheels+=n.name.rfind("wheel_",0)==0;}for(auto&mesh:m.meshes)if((mesh.stride==24||mesh.stride==36)&&lower(m.materials.at(mesh.material).at("name").get<std::string>()).find("carpaint")!=std::string::npos)paint=true;return bodies==1&&wheels==4&&paint;}
inline P inversePoint(const Matrix&m,P p){P a{m[0],m[1],m[2]},b{m[3],m[4],m[5]},c{m[6],m[7],m[8]};auto bc=crossP(b,c),ca=crossP(c,a),ab=crossP(a,b);double det=dotP(a,bc);require(std::abs(det)>1e-10,"Singular native node basis");p=minus(p,{m[9],m[10],m[11]});return {dotP(p,bc)/det,dotP(p,ca)/det,dotP(p,ab)/det};}
inline P dimensions(const Model&m){Bounds b;for(auto&n:m.nodes)if(n.name=="body"||n.name.rfind("wheel_",0)==0)for(int i:n.meshes)for(auto p:m.meshes.at(i).positions){auto v=transform(n.matrix,p,true);b.add({v.x,v.y,v.z});}return b.size();}
struct Donor {std::string id,name;P collision{},visual{};double widthError=0,overall=0;};
inline std::vector<Donor> donors(const Archive&a,P source){auto names=displayNamesFrom([&](const std::string&p){return stockAsset(a,p);});auto paints=paintSelections(a,names);std::set<std::string> seen;std::vector<Donor> out;for(const auto&p:paints)if(seen.insert(p.car).second){try{auto native=decodeModel(stockAsset(a,"graphics/vehicles/"+p.car+"/lod0_livery.bdae"));if(!supportedDonor(native))continue;auto b=stockAsset(a,"graphics/vehicles/"+p.car+"/collision.hzc");require(b.size()>=20,"Truncated collision");size_t nv=u16(b,2),nt=u32(b,4);require(nv&&nt&&20+nv*12<=b.size()&&nt<=(b.size()-20-nv*12)/6,"Invalid collision mesh");Bounds box;for(size_t i=0;i<nt*3;i++){size_t v=u16(b,20+nv*12+i*2);require(v<nv,"Invalid collision index");box.add({f32(b,20+v*12),f32(b,24+v*12),f32(b,28+v*12)});}Donor d{p.car,p.carName,box.size(),dimensions(native),0,0};for(int k=0;k<3;k++){require(source[k]>0,"Model must have nonzero width, height and length");double e=std::abs(d.collision[k]-source[k])/source[k];if(k==0)d.widthError=e;d.overall+=e*e;}out.push_back(d);}catch(const std::exception&){/* Unused paints or unknown donor formats are not candidates. */}}std::sort(out.begin(),out.end(),[](const Donor&a,const Donor&b){return std::tie(a.widthError,a.overall,a.id)<std::tie(b.widthError,b.overall,b.id);});require(!out.empty(),"No supported stock donors found");return out;}
inline void floatAt(Bytes&b,size_t at,double x){require(std::isfinite(x)&&std::abs(x)<1e9,"Invalid native float");float f=float(x);uint32_t bits;std::memcpy(&bits,&f,4);set32(b,at,bits);}
inline void putFloat(Bytes&b,double x){size_t p=b.size();b.resize(p+4);floatAt(b,p,x);}
inline uint16_t toHalf(double x){require(std::isfinite(x)&&std::abs(x)<=65504,"Native half-float overflow");bool negative=x<0;x=std::abs(x);unsigned sign=negative?0x8000:0;if(x<std::ldexp(1.,-14))return uint16_t(sign|unsigned(std::lround(std::ldexp(x,24))));int e;double f=std::frexp(x,&e);unsigned mant=unsigned(std::lround((f*2-1)*1024));int exp=e+14;if(mant==1024){mant=0;exp++;}require(exp<31,"Native half-float overflow");return uint16_t(sign|(exp<<10)|mant);}
struct Options {enum class Quality {Light,Vanilla,High,Full};double scale=1;int yaw=0;bool zUp=false,fit=true,decimate=true,fitCollision=true,buildEffects=true,neutralMask=true,addPaint=true;std::string paintName="Black";bool allLocaleNames=true,fitShadow=true;Quality quality=Quality::Vanilla;};
inline unsigned qualityTextureSize(unsigned base,const std::string &category,Options::Quality quality){
 if(quality==Options::Quality::Light)return std::min(base,512u);
 if(quality==Options::Quality::High)return std::max(base,1024u);
 if(quality==Options::Quality::Full){bool small=category=="wheel"||category=="grille"||category=="headlight";return std::max(base,small?1024u:2048u);}
 return base;
}
inline P orient(P p,const Options&o){if(o.zUp)p={p[0],p[2],-p[1]};int q=(o.yaw%4+4)%4;while(q--)p={p[2],p[1],-p[0]};for(auto&v:p)v*=o.scale;return p;}
inline P orientedSize(const Source&s,const Options&o){Bounds b;for(auto&p:s.parts)if(p.role!=Ignore)for(auto v:p.positions)b.add(orient(v,o));return b.size();}
struct Result {Bytes model;Json report;};
struct MaterialBinding {std::string name,templateMaterial,texture,category,finish;int image=-1;unsigned size=1024,flags=2;bool importTexture=true;};
struct Plan {std::vector<MaterialBinding> materials;std::vector<int> partMaterial;};

inline Plan defaultPlan(const Source &s,const std::string &car){
 require(!car.empty()&&car.size()<=8,"Invalid authored car ID");Plan plan;plan.partMaterial.assign(s.parts.size(),-1);std::map<std::string,int> shared;
 auto binding=[&](const std::string &category,const std::string &templateName,unsigned size,unsigned flags,int image)->int{
  auto it=shared.find(category);if(it!=shared.end())return it->second;MaterialBinding b;b.name=car+"_"+category;b.category=category;b.templateMaterial=templateName;b.texture=b.name;b.size=size;b.flags=flags;b.image=image;plan.materials.push_back(std::move(b));int index=int(plan.materials.size()-1);shared[category]=index;return index;};
 for(size_t i=0;i<s.parts.size();i++){
  const auto &part=s.parts[i];if(part.role==Ignore||part.role==Body||part.role==Glass)continue;
  int sm=part.materialIndex;if(sm<0||size_t(sm)>=s.materials.size())continue;
  const auto &source=s.materials[sm];if(source.image<0)continue;
  std::string material=lower(source.name),partName=lower(part.name);
  if(part.role==Wheels||material.find("wheel")!=std::string::npos){
   if(partName.find("cwheel")!=std::string::npos||partName.find("tyre")!=std::string::npos||partName.find("tire")!=std::string::npos)continue;
   plan.partMaterial[i]=binding("wheel","wheel|metal|diffuse",512,0,source.image);continue;}
  if(part.role==Interior||material.find("indoor")!=std::string::npos||material.find("interior")!=std::string::npos){plan.partMaterial[i]=binding("interior","blackplastic|undercarriage|diffuse",1024,2,source.image);continue;}
  if(partName.find("grille")!=std::string::npos||partName.find("mask")!=std::string::npos||partName.find("tooth")!=std::string::npos||partName.find("vent")!=std::string::npos){plan.partMaterial[i]=binding("grille","grille|diffuse",512,0,source.image);continue;}
  if(partName.find("lamp")!=std::string::npos||partName.find("light")!=std::string::npos||partName.find("headlight")!=std::string::npos||partName.find("bulb")!=std::string::npos){plan.partMaterial[i]=binding("headlight","headlight|lights1|lights|diffuse",512,2,source.image);continue;}
  if(material.find("front")!=std::string::npos&&material.find("chrome")!=std::string::npos){plan.partMaterial[i]=binding("front","bumper_mat|blackplastic|headlight|lights|diffuse",1024,2,source.image);continue;}
  if(material.find("chrome")!=std::string::npos||material.find("metal")!=std::string::npos){plan.partMaterial[i]=binding("chrome","bumper_mat|blackplastic|headlight|lights|diffuse",1024,2,source.image);continue;}
 }
 return plan;
}

inline std::vector<std::string> donorMaterialNames(const Archive &archive,const std::string &donor){
 auto model=decodeModel(stockAsset(archive,"graphics/vehicles/"+donor+"/lod0_livery.bdae"));std::vector<std::string> names;for(auto &material:model.materials)names.push_back(material.value("name",std::string()));return names;
}

inline void applyFinish(Bytes &block,const std::string &finish){
 struct Preset {double power,scale,color[3],reflection,ambient;};Preset preset;
 if(finish=="matte")preset={8,.05,{1,1,1},.10,.30};
 else if(finish=="satin")preset={24,.35,{1,1,1},.40,.20};
 else if(finish=="glossy")preset={48,.80,{1,1,1},.75,.10};
 else if(finish=="chrome")preset={64,1.,{.90,.95,1.},.90,.05};
 else return;
 require(block.size()%128==0,"Invalid material parameter block");
 for(size_t at=0;at<block.size();at+=128){auto name=text(block,at,64);
  if(name=="g_specularPower")floatAt(block,at+112,preset.power);
  else if(name=="g_specularScale")floatAt(block,at+112,preset.scale);
  else if(name=="g_specularColor"){floatAt(block,at+112,preset.color[0]);floatAt(block,at+116,preset.color[1]);floatAt(block,at+120,preset.color[2]);}
  else if(name=="g_reflectionStrength")floatAt(block,at+112,preset.reflection);
  else if(name=="g_reflectionAmbient")floatAt(block,at+112,preset.ambient);
 }
}

inline Result convert(const Source&s,const Bytes&original,const Options&o,const Plan &plan={}){
 require(std::isfinite(o.scale)&&o.scale>0&&o.scale<=10000,"Invalid import scale");auto native=decodeModel(original);require(u32(original,28)==0,"Donor has unsupported ancillary EADB records");uint32_t sa=u32(original,84),va=u32(original,92);require(u32(original,80)==sa,"Donor has an unsupported extra section");
 const uint32_t mc0=u32(original,12);const size_t ma0=u32(original,68),pa0=u32(original,72),na0=u32(original,76);
 int body=-1;std::vector<int>wheels;for(size_t i=0;i<native.nodes.size();i++){auto&n=native.nodes[i];if(n.name=="body")body=i;if(n.name.rfind("wheel_",0)==0)wheels.push_back(i);}require(body>=0&&wheels.size()==4,"Donor must contain one body and four named wheel nodes");
 // Native animation pivots are retained. Imported wheel groups are fitted to them.
 for(auto&n:native.nodes)(void)inversePoint(n.matrix,{0,0,0});
 std::vector<Part> parts=s.parts;Bounds source,stock;for(auto&p:parts)if(p.role!=Ignore)for(auto&v:p.positions){v=orient(v,o);source.add(v);}for(auto&n:native.nodes)if(n.name=="body"||n.name.rfind("wheel_",0)==0)for(int i:n.meshes)for(auto v:native.meshes[i].positions){auto p=transform(n.matrix,v,true);stock.add({p.x,p.y,p.z});}auto dims=source.size();require(*std::min_element(dims.begin(),dims.end())>1e-8,"Model has zero extent");P scale{1,1,1},shift{};auto center=source.center();std::array<Bounds,4>groups;bool importedWheels=false;for(auto&p:parts)if(p.role==Wheels){importedWheels=true;for(auto v:p.positions)groups[(v[0]>center[0]?1:0)+(v[2]>center[2]?2:0)].add(v);}
 std::array<int,4>wheelNodes{};std::set<int>quadrants;auto nativeCenter=stock.center();for(int i:wheels){auto&m=native.nodes[i].matrix;int q=(m[9]>nativeCenter[0]?1:0)+(m[11]>nativeCenter[2]?2:0);require(quadrants.insert(q).second,"Ambiguous donor wheel layout");wheelNodes[q]=i;}
 if(o.decimate&&o.quality!=Options::Quality::Full){
  auto simplifyPart=[&](Part &p,size_t target){if(target>=p.indices.size()/3)return;decimate::Mesh mesh;mesh.positions.assign(p.positions.begin(),p.positions.end());mesh.normals.assign(p.normals.begin(),p.normals.end());mesh.uv.resize(p.uv.size());for(size_t i=0;i<p.uv.size();i++)mesh.uv[i]={p.uv[i][0],p.uv[i][1]};mesh.indices=p.indices;decimate::simplify(mesh,target);p.positions.assign(mesh.positions.begin(),mesh.positions.end());if(!p.normals.empty())p.normals.assign(mesh.normals.begin(),mesh.normals.end());p.uv.resize(mesh.uv.size());for(size_t i=0;i<mesh.uv.size();i++)p.uv[i]={float(mesh.uv[i][0]),float(mesh.uv[i][1])};p.indices=std::move(mesh.indices);};
  size_t donorTarget=0,bodyTriangles=0,detailTriangles=0;auto addNode=[&](int node){for(int index:native.nodes[node].meshes)donorTarget+=native.meshes[index].indices.size()/3;};addNode(body);if(importedWheels)for(int wheel:wheels)addNode(wheel);
  if(o.quality==Options::Quality::Light){
   for(auto &p:parts){if(p.role==Ignore||p.indices.size()<18)continue;size_t triangles=p.indices.size()/3,target=std::max<size_t>(8,size_t(std::llround(triangles*.2)));simplifyPart(p,target);}
  }else{
   for(auto &p:parts)if(p.role!=Ignore){size_t triangles=p.indices.size()/3;if(p.role==Body)bodyTriangles+=triangles;else detailTriangles+=triangles;}
   double factor=o.quality==Options::Quality::High?1.8:1.;if(detailTriangles>0&&donorTarget*factor>bodyTriangles){double ratio=std::max(.02,std::min(1.,(donorTarget*factor-bodyTriangles)/detailTriangles));for(auto &p:parts){if(p.role==Ignore||p.role==Body||p.indices.size()<18)continue;size_t triangles=p.indices.size()/3;simplifyPart(p,std::max<size_t>(8,size_t(std::llround(triangles*ratio))));}}
  }
 }
 if(o.fit){double factor=stock.size()[2]/dims[2];scale={factor,factor,factor};if(importedWheels){for(auto&g:groups)require(g.count>0,"Wheel assignment must cover all four corners; correct roles or orientation");auto a=groups[0].center(),b=groups[3].center();auto&ma=native.nodes[wheelNodes[0]].matrix;auto&mb=native.nodes[wheelNodes[3]].matrix;require(b[0]-a[0]>1e-6&&b[2]-a[2]>1e-6,"Degenerate wheelbase or track");scale[0]=(mb[9]-ma[9])/(b[0]-a[0]);scale[1]=scale[2]=(mb[11]-ma[11])/(b[2]-a[2]);for(int k=0;k<3;k++)shift[k]=ma[9+k]-a[k]*scale[k];}else{for(int k=0;k<3;k++)shift[k]=nativeCenter[k]-center[k]*scale[k];shift[1]=stock.lo[1]-source.lo[1]*scale[1];}}
 else {shift={-center[0],stock.lo[1]-source.lo[1],-center[2]};}
 auto roleMaterial=[&](int role){std::vector<std::string> words=role==Body?std::vector<std::string>{"carpaint"}:role==Glass?std::vector<std::string>{"glass_wind","glass"}:role==Metal?std::vector<std::string>{"metal","chrome"}:role==Wheels?std::vector<std::string>{"tyre","tire","wheel"}:std::vector<std::string>{"blackplastic","plastic","interior"};for(auto&word:words)for(size_t i=0;i<native.materials.size();i++)if(lower(native.materials[i].at("name").get<std::string>()).find(word)!=std::string::npos)return int(i);throw std::runtime_error(std::string("Donor lacks native material for ")+roleName(role));};
 auto slot0=[&](size_t i){auto slots=native.materials[i].value("textures",Json::array());return slots.is_array()&&!slots.empty()&&slots[0].is_string()&&!slots[0].get<std::string>().empty();};
 auto donorMaterial=[&](const std::string &list){size_t start=0;while(start<=list.size()){auto end=list.find('|',start);auto want=lower(list.substr(start,end==std::string::npos?std::string::npos:end-start));start=end==std::string::npos?list.size()+1:end+1;if(want.empty())continue;
  if(want=="diffuse"){for(size_t i=0;i<native.materials.size();i++)if(native.materials[i].value("shader_id",0)==11&&slot0(i))return int(i);continue;}
  if(want=="wheel"){for(size_t i=0;i<native.materials.size();i++){auto candidate=lower(native.materials[i].value("name",std::string()));if(candidate.find("wheel")!=std::string::npos&&slot0(i))return int(i);}for(size_t i=0;i<native.materials.size();i++)if(native.materials[i].value("shader_id",0)==46&&slot0(i))return int(i);continue;}
  for(size_t i=0;i<native.materials.size();i++){auto candidate=lower(native.materials[i].value("name",std::string()));if(candidate==want||candidate.rfind(want+"_",0)==0||candidate.rfind(want,0)==0)return int(i);}}
  return -1;};
 require(plan.partMaterial.empty()||plan.partMaterial.size()==parts.size(),"Material plan does not match source parts");
 Bytes materials(original.begin()+ma0,original.begin()+ma0+size_t(mc0)*464),params(original.begin()+pa0,original.begin()+na0);std::vector<int> bindingMaterial(plan.materials.size(),-1);
 for(size_t i=0;i<plan.materials.size();i++){const auto &spec=plan.materials[i];int templateMaterial=donorMaterial(spec.templateMaterial);require(templateMaterial>=0&&templateMaterial<int(native.materials.size()),"Donor lacks material template "+spec.templateMaterial);Bytes record(original.begin()+ma0+size_t(templateMaterial)*464,original.begin()+ma0+size_t(templateMaterial+1)*464);
  std::string templateName=text(record,0,64);auto suffixAt=templateName.find('_');std::string suffix=suffixAt==std::string::npos?"":templateName.substr(suffixAt);std::string name=spec.name;if(name.size()+suffix.size()<64)name+=suffix;
  std::fill(record.begin(),record.begin()+64,0);std::memcpy(record.data(),name.data(),name.size());
  std::fill(record.begin()+64,record.begin()+64+8*32,0);
  if(!spec.texture.empty()){std::string slot=spec.texture+".tga";require(slot.size()<32,"Texture name is too long for a native material slot");std::memcpy(record.data()+64,slot.data(),slot.size());}
  if(!spec.finish.empty()&&spec.finish!="default"){
   uint32_t first=u32(record,440),count=u32(record,436);require(first<=params.size()/128&&count<=(params.size()/128-first),"Invalid template parameters");Bytes block(params.begin()+size_t(first)*128,params.begin()+size_t(first+count)*128);applyFinish(block,spec.finish);uint32_t newFirst=uint32_t(params.size()/128);params.insert(params.end(),block.begin(),block.end());set32(record,440,newFirst);
  }
  materials.insert(materials.end(),record.begin(),record.end());bindingMaterial[i]=int(mc0+i);
 }
 uint32_t materialCount=mc0+uint32_t(plan.materials.size());
 struct Mesh {int node;uint32_t stride;Bytes record,vertices;std::vector<uint32_t>indices;};std::vector<Mesh> output;
 auto originalMesh=[&](int i,int node){size_t p=sa+i*48;uint32_t stride=u32(original,p+4),fv=u32(original,p+8),vc=u32(original,p+12);Mesh m{node,stride,Bytes(original.begin()+p,original.begin()+p+48),Bytes(original.begin()+va+size_t(fv)*stride,original.begin()+va+size_t(fv+vc)*stride),native.meshes[i].indices};return m;};
 auto layoutFor=[&](int material){for(size_t i=0;i<native.meshes.size();i++)if(int(native.meshes[i].material)==material&&(native.meshes[i].stride==24||native.meshes[i].stride==36))return int(i);for(size_t i=0;i<native.meshes.size();i++)if(native.meshes[i].stride==24||native.meshes[i].stride==36)return int(i);return -1;};
 struct EffectTarget {int node,mesh;};std::vector<EffectTarget> effects;std::set<int> effectNodes;bool haveBody=std::any_of(parts.begin(),parts.end(),[](const Part &p){return p.role==Body;});
 if(o.buildEffects&&haveBody)for(size_t n=0;n<native.nodes.size();n++)if(native.nodes[n].name=="zone_front"||native.nodes[n].name=="electric"||native.nodes[n].name=="zone_back")for(int mesh:native.nodes[n].meshes){effects.push_back({int(n),mesh});effectNodes.insert(int(n));}
 for(size_t n=0;n<native.nodes.size();n++)if(int(n)!=body&&(!importedWheels||std::find(wheels.begin(),wheels.end(),int(n))==wheels.end())&&!effectNodes.count(int(n)))for(int i:native.nodes[n].meshes)output.push_back(originalMesh(i,n));
 size_t importedTriangles=0;
 struct Group {int node=0;uint32_t material=0,stride=0;Bytes record;std::map<uint64_t,uint32_t> remap;std::vector<P> pos,normal,tangent,bitangent;std::vector<std::array<float,2>> uv;std::vector<uint32_t> indices;};std::map<std::array<uint32_t,3>,Group> mergedGroups;
 for(size_t pi=0;pi<parts.size();pi++){auto&p=parts[pi];if(p.role==Ignore)continue;int binding=pi<plan.partMaterial.size()?plan.partMaterial[pi]:-1;int material=binding>=0?bindingMaterial[binding]:roleMaterial(p.role);int layoutMaterial=binding>=0?donorMaterial(plan.materials[binding].templateMaterial):material;int layoutIndex=layoutFor(layoutMaterial);require(layoutIndex>=0,"Donor material has no supported vertex layout");std::map<int,std::vector<uint32_t>>triangles;for(size_t i=0;i<p.indices.size();i+=3){int node=body;if(p.role==Wheels){P c{};for(int j=0;j<3;j++)for(int k=0;k<3;k++)c[k]+=p.positions[p.indices[i+j]][k]/3;node=wheelNodes[(c[0]>center[0]?1:0)+(c[2]>center[2]?2:0)];}for(int j=0;j<3;j++)triangles[node].push_back(p.indices[i+j]);}
   for(auto &entry:triangles)for(size_t t=0;t+2<entry.second.size();t+=3){
   std::array<uint32_t,3> key{uint32_t(entry.first),uint32_t(material),uint32_t(native.meshes[layoutIndex].stride)};
   auto found=mergedGroups.find(key);
   if(found==mergedGroups.end()){Mesh proto=originalMesh(layoutIndex,entry.first);Group group;group.node=entry.first;group.material=material;group.stride=proto.stride;group.record=std::move(proto.record);found=mergedGroups.emplace(key,std::move(group)).first;}
   Group &group=found->second;auto pivot=native.nodes[entry.first].matrix;uint32_t mapped[3]{};
   for(int k=0;k<3;k++){
    uint64_t source=(uint64_t(pi)<<32)|entry.second[t+k];auto it=group.remap.find(source);
    if(it==group.remap.end()){
     uint32_t index=uint32_t(group.pos.size());group.remap[source]=index;P v=p.positions[entry.second[t+k]];for(int c=0;c<3;c++)v[c]=v[c]*scale[c]+shift[c];v=inversePoint(pivot,v);group.pos.push_back(v);
     P n{};if(!p.normals.empty()){n=orient(p.normals[entry.second[t+k]],o);for(int c=0;c<3;c++)n[c]/=scale[c];n=unit({pivot[0]*n[0]+pivot[1]*n[1]+pivot[2]*n[2],pivot[3]*n[0]+pivot[4]*n[1]+pivot[5]*n[2],pivot[6]*n[0]+pivot[7]*n[1]+pivot[8]*n[2]});}
     group.normal.push_back(n);group.tangent.push_back({0,0,0});group.bitangent.push_back({0,0,0});group.uv.push_back(p.uv[entry.second[t+k]]);mapped[k]=index;
    }else mapped[k]=it->second;
   }
   for(int k=0;k<3;k++)group.indices.push_back(mapped[k]);
   auto e1=minus(group.pos[mapped[1]],group.pos[mapped[0]]),e2=minus(group.pos[mapped[2]],group.pos[mapped[0]]),face=crossP(e1,e2);
   double du=group.uv[mapped[1]][0]-group.uv[mapped[0]][0],dv=group.uv[mapped[1]][1]-group.uv[mapped[0]][1],eu=group.uv[mapped[2]][0]-group.uv[mapped[0]][0],ev=group.uv[mapped[2]][1]-group.uv[mapped[0]][1],d=du*ev-eu*dv;
   P tangent=std::abs(d)>1e-12?P{(e1[0]*ev-e2[0]*dv)/d,(e1[1]*ev-e2[1]*dv)/d,(e1[2]*ev-e2[2]*dv)/d}:P{1,0,0};
   P bitangent=std::abs(d)>1e-12?P{(e2[0]*du-e1[0]*eu)/d,(e2[1]*du-e1[1]*eu)/d,(e2[2]*du-e1[2]*eu)/d}:P{0,0,1};
   for(int k=0;k<3;k++){uint32_t v=mapped[k];if(p.normals.empty())for(int c=0;c<3;c++)group.normal[v][c]+=face[c];for(int c=0;c<3;c++){group.tangent[v][c]+=tangent[c];group.bitangent[v][c]+=bitangent[c];}}
   importedTriangles++;
  }
 }
 for(auto &entry:mergedGroups){Group &group=entry.second;Mesh m;m.node=group.node;m.stride=group.stride;m.record=std::move(group.record);set32(m.record,0,(u32(m.record,0)&~0xfffu)|group.material);m.indices=std::move(group.indices);
  for(size_t i=0;i<group.pos.size();i++){for(auto v:group.pos[i])putFloat(m.vertices,v);auto n=unit(group.normal[i]),t=group.tangent[i];double projection=dotP(n,t);for(int k=0;k<3;k++)t[k]-=n[k]*projection;t=unit(t);double handedness=dotP(crossP(n,t),group.bitangent[i])<0?-1:1;for(auto v:n)put16(m.vertices,toHalf(v));put16(m.vertices,toHalf(1));for(auto v:group.uv[i])put16(m.vertices,toHalf(v));if(m.stride==36){for(auto v:group.uv[i])put16(m.vertices,toHalf(v));for(auto v:t)put16(m.vertices,toHalf(v));put16(m.vertices,toHalf(handedness));}}
  output.push_back(std::move(m));}
 if(!effects.empty()){
  decimate::Mesh bodyMesh;auto bodyMatrix=native.nodes[body].matrix;
  auto rotateNormal=[&](P n){return unit({bodyMatrix[0]*n[0]+bodyMatrix[1]*n[1]+bodyMatrix[2]*n[2],bodyMatrix[3]*n[0]+bodyMatrix[4]*n[1]+bodyMatrix[5]*n[2],bodyMatrix[6]*n[0]+bodyMatrix[7]*n[1]+bodyMatrix[8]*n[2]});};
  for(auto &p:parts)if(p.role!=Ignore&&p.role!=Wheels&&!p.indices.empty()){uint32_t base=uint32_t(bodyMesh.positions.size());for(size_t i=0;i<p.positions.size();i++){P v=p.positions[i];for(int c=0;c<3;c++)v[c]=v[c]*scale[c]+shift[c];bodyMesh.positions.push_back({v[0],v[1],v[2]});bodyMesh.normals.push_back(p.normals.empty()?decimate::Vec{0,0,0}:rotateNormal(orient(p.normals[i],o)));bodyMesh.uv.push_back({p.uv[i][0],p.uv[i][1]});}for(auto index:p.indices)bodyMesh.indices.push_back(base+index);}
  if(!bodyMesh.indices.empty()){
   Bounds bounds;for(auto &position:bodyMesh.positions)bounds.add(position);auto center=bounds.center(),size=bounds.size();
   for(auto &effect:effects){
    std::string name=native.nodes[effect.node].name;decimate::Mesh shell;std::vector<uint32_t> remap(bodyMesh.positions.size(),UINT32_MAX);
    for(size_t i=0;i<bodyMesh.indices.size();i+=3){auto a=bodyMesh.indices[i],b=bodyMesh.indices[i+1],c=bodyMesh.indices[i+2];double z=(bodyMesh.positions[a][2]+bodyMesh.positions[b][2]+bodyMesh.positions[c][2])/3.;if(name=="zone_front"&&z<0)continue;if(name=="zone_back"&&z>=0)continue;for(uint32_t old:{a,b,c}){if(remap[old]==UINT32_MAX){remap[old]=uint32_t(shell.positions.size());shell.positions.push_back(bodyMesh.positions[old]);shell.normals.push_back(bodyMesh.normals[old]);shell.uv.push_back(bodyMesh.uv[old]);}shell.indices.push_back(remap[old]);}}
    if(shell.indices.size()<12){output.push_back(originalMesh(effect.mesh,effect.node));continue;}
    size_t target=std::max<size_t>(16,native.meshes[effect.mesh].indices.size()/3);decimate::simplify(shell,target);
    Mesh m=originalMesh(effect.mesh,effect.node);m.vertices.clear();m.indices=shell.indices;auto effectMatrix=native.nodes[effect.node].matrix;
    for(size_t i=0;i<shell.positions.size();i++){decimate::Vec p=shell.positions[i],n=unit(shell.normals[i]);for(int c=0;c<3;c++)p[c]+=n[c]*0.012;P local=inversePoint(effectMatrix,{p[0],p[1],p[2]});double u=0.5+std::atan2(p[0]-center[0],p[2]-center[2])/(2*3.14159265358979323846);double v=(p[1]-bounds.lo[1])/std::max(1e-6,size[1]);for(double value:local)putFloat(m.vertices,value);for(double value:n)put16(m.vertices,toHalf(value));put16(m.vertices,0x3c00);for(double value:{u,v})put16(m.vertices,toHalf(value));m.vertices.insert(m.vertices.end(),{0xff,0xff,0xff,0xff});}
    output.push_back(std::move(m));
   }
  }
 }
 require(importedTriangles>0&&output.size()<32767,"No imported geometry or too many meshes");std::stable_sort(output.begin(),output.end(),[](const Mesh&a,const Mesh&b){return a.stride<b.stride;});Bytes vertices,indices,records,links,nodes(original.begin()+na0,original.begin()+na0+native.nodes.size()*156);Bounds global,vehicle,wheelsBounds;std::set<int> wheelSet(wheels.begin(),wheels.end());size_t triangles=0;
 for(auto&m:output){vertices.resize((vertices.size()+m.stride-1)/m.stride*m.stride);size_t first=vertices.size()/m.stride,count=m.vertices.size()/m.stride;require(first+count<=65536,"Native 16-bit vertex limit exceeded. Simplify the model before import");set32(m.record,8,first);set32(m.record,12,count);set32(m.record,16,indices.size()/2);set32(m.record,20,m.indices.size());records.insert(records.end(),m.record.begin(),m.record.end());vertices.insert(vertices.end(),m.vertices.begin(),m.vertices.end());for(auto v:m.indices){require(v<count,"Invalid generated index");put16(indices,first+v);}triangles+=m.indices.size()/3;}
 for(size_t n=0;n<native.nodes.size();n++){set32(nodes,n*156+120,links.size()/2);Bounds b;for(size_t i=0;i<output.size();i++)if(output[i].node==int(n)){put16(links,i);auto&m=output[i];for(size_t j=0;j<m.vertices.size();j+=m.stride){P p{f32(m.vertices,j),f32(m.vertices,j+4),f32(m.vertices,j+8)};b.add(p);auto world=transform(native.nodes[n].matrix,{F(p[0]),F(p[1]),F(p[2])},true);global.add({world.x,world.y,world.z});if(int(n)==body||wheelSet.count(int(n)))vehicle.add({world.x,world.y,world.z});if(wheelSet.count(int(n)))wheelsBounds.add({world.x,world.y,world.z});}}put16(links,65535);if(b.count){auto c=b.center(),d=b.size();for(int k=0;k<3;k++){floatAt(nodes,n*156+80+k*4,b.lo[k]);floatAt(nodes,n*156+92+k*4,b.hi[k]);floatAt(nodes,n*156+104+k*4,c[k]);}floatAt(nodes,n*156+116,std::sqrt(dotP(d,d))/2);}}
 Bytes out(original.begin(),original.begin()+ma0);set32(out,12,materialCount);out.insert(out.end(),materials.begin(),materials.end());out.resize((out.size()+31)/32*32);set32(out,72,out.size());set32(out,16,uint32_t(params.size()/128));out.insert(out.end(),params.begin(),params.end());set32(out,76,out.size());out.insert(out.end(),nodes.begin(),nodes.end());auto section=[&](int field,const Bytes&data){out.resize((out.size()+31)/32*32);set32(out,field,out.size());out.insert(out.end(),data.begin(),data.end());};section(84,records);set32(out,80,u32(out,84));section(88,links);section(92,vertices);section(96,indices);out.resize((out.size()+31)/32*32);set32(out,24,output.size());set32(out,32,vertices.size()/4);set32(out,64,indices.size()/2);auto d=global.size(),c=global.center();for(int k=0;k<3;k++){floatAt(out,100+k*4,d[k]);floatAt(out,112+k*4,c[k]);}floatAt(out,124,std::sqrt(dotP(d,d))/2);auto checked=decodeModel(out);require(checked.nodes.size()==native.nodes.size()&&checked.meshes.size()==output.size(),"Native model read-back failed");
 if(plan.materials.empty())require(std::equal(original.begin()+ma0,original.begin()+ma0+size_t(mc0)*464,out.begin()+ma0)&&std::equal(original.begin()+pa0,original.begin()+na0,out.begin()+u32(out,72)),"Native material bytes changed unexpectedly");
 Bounds bodyWorld;for(auto &p:parts)if(p.role==Body)for(auto v:p.positions){auto w=orient(v,o);for(int c=0;c<3;c++)w[c]=w[c]*scale[c]+shift[c];bodyWorld.add(w);}
 return {out,{{"source_sha256",s.hash},{"donor_sha256",sha(original)},{"output_sha256",sha(out)},{"input_triangles",s.triangles},{"imported_triangles",importedTriangles},{"output_triangles",triangles},{"scale_xyz",scale},{"translation",shift},{"visual_dimensions",dimensions(checked)},{"native_wheels_retained",!importedWheels},{"body_lo",Json::array({bodyWorld.lo[0],bodyWorld.lo[1],bodyWorld.lo[2]})},{"body_hi",Json::array({bodyWorld.hi[0],bodyWorld.hi[1],bodyWorld.hi[2]})},{"vehicle_lo",Json::array({vehicle.lo[0],vehicle.lo[1],vehicle.lo[2]})},{"vehicle_hi",Json::array({vehicle.hi[0],vehicle.hi[1],vehicle.hi[2]})},{"wheel_lo",Json::array({wheelsBounds.lo[0],wheelsBounds.lo[1],wheelsBounds.lo[2]})},{"wheel_hi",Json::array({wheelsBounds.hi[0],wheelsBounds.hi[1],wheelsBounds.hi[2]})}}};
}
inline std::pair<std::string,std::string> newPaintToken(const Archive &archive,const std::string &car){
 auto names=displayNames(archive);auto tree=cfgValue(Json::parse(Config(asset(archive,"scripts/config/paint_PC.cfg")).json()));std::set<std::string> used;
 if(tree.contains("Paints"))for(auto it=tree["Paints"].begin();it!=tree["Paints"].end();++it){if(!it.value().is_array())continue;for(auto &pack:it.value())used.insert(lower(pack.value("packName",std::string())));}
 auto prefix=lower(car)+"pack";std::vector<std::pair<std::string,std::string>> candidates;
 for(auto &entry:names){if(entry.first.find("pack")==std::string::npos||used.count(entry.first)||entry.second.empty()||entry.second==entry.first)continue;candidates.push_back({entry.first,entry.second});}
 require(!candidates.empty(),"No unused paint name token is available");std::stable_sort(candidates.begin(),candidates.end(),[&](const auto &a,const auto &b){bool pa=a.first.rfind(prefix,0)==0,pb=b.first.rfind(prefix,0)==0;return pa!=pb?pa:std::tie(a.first,a.second)<std::tie(b.first,b.second);});return candidates.front();
}

inline Json configValue(const Json &node){auto type=node.at("type").get<unsigned>();if(type==0||type==1)return node.at("value");if(type==2){Json out=Json::array();for(auto &child:node.at("value"))out.push_back(configValue(child));return out;}throw std::runtime_error("Unsupported paint config field");}
inline Json paintPackObject(const Json &pack){Json out=Json::object();for(auto &field:pack.at("value"))out[field.at("name").get<std::string>()]=configValue(field);return out;}
struct PaintPackResult {Bytes cfg;size_t index;Json pack;};
inline std::optional<PaintPackResult> addPaintPack(const Bytes &cfg,const std::string &car,const std::string &token){
 auto roots=Json::parse(Config(cfg).json());auto &paints=configNamed(roots,"Paints");auto &carNode=configNamed(paints["value"],car);auto &carTable=carNode["value"];require(carTable.is_array()&&!carTable.empty(),"Missing car paint table");if(carTable.size()>=9)return std::nullopt;
 Json pack;double darkest=1e300;for(auto &candidate:carTable){auto &fields=candidate["value"];double sum=0;for(const char *name:{"col0","col1","col2"}){auto &field=configNamed(fields,name);for(auto &channel:field["value"])sum+=channel["value"].get<double>();}if(sum<darkest){darkest=sum;pack=candidate;}}
 require(!pack.is_null(),"Missing paint pack");auto &fields=pack["value"];configSet(fields,"packName",token);configSet(fields,"livery",-1.0);configSet(fields,"packPrice",1.0);for(const char *name:{"col0","col1","col2"}){auto &field=configNamed(fields,name);for(auto &channel:field["value"])channel["value"]=2.0;}
 size_t index=carTable.size();carTable.push_back(pack);return PaintPackResult{configEncode(roots),index,paintPackObject(pack)};
}

inline Texture neutralPaintMask(){
 Image image{512,512,Bytes(size_t(512)*512*4)};for(size_t i=0;i<image.pixels.size();i+=4){image.pixels[i]=255;image.pixels[i+1]=255;image.pixels[i+2]=0;image.pixels[i+3]=255;}auto dds=textures::buildDDS(image,"DXT1");Texture texture;texture.name="carpaint_occspec";texture.fourcc="DXT1";texture.width=512;texture.height=512;texture.flags=0;texture.mips=u32(dds,28);texture.caps=u32(dds,112);texture.dds=std::move(dds);return texture;
}

inline Bytes fitCollision(const Bytes &original,double targetXL,double targetXH,double targetZL,double targetZH,double targetYTop){
 require(original.size()>20&&u16(original,0)==279,"Unsupported collision hull");uint16_t vertices=u16(original,2);uint32_t triangles=u32(original,4);require(vertices>3&&triangles>0,"Invalid collision hull");size_t body=20+size_t(vertices)*12+size_t(triangles)*6;require(body<=original.size(),"Truncated collision hull");
 P lo{1e30,1e30,1e30},hi{-1e30,-1e30,-1e30};for(uint16_t i=0;i<vertices;i++){P p{f32(original,20+size_t(i)*12),f32(original,24+size_t(i)*12),f32(original,28+size_t(i)*12)};for(int k=0;k<3;k++){lo[k]=std::min(lo[k],p[k]);hi[k]=std::max(hi[k],p[k]);}}
 require(hi[0]>lo[0]&&hi[1]>lo[1]&&hi[2]>lo[2]&&targetXH>targetXL&&targetZH>targetZL&&targetYTop>lo[1],"Degenerate collision fit");
 double sx=(targetXH-targetXL)/(hi[0]-lo[0]),sz=(targetZH-targetZL)/(hi[2]-lo[2]),sy=(targetYTop-lo[1])/(hi[1]-lo[1]);
 auto map=[&](P p){return P{targetXL+(p[0]-lo[0])*sx,lo[1]+(p[1]-lo[1])*sy,targetZL+(p[2]-lo[2])*sz};};
 Bytes out=original;for(uint16_t i=0;i<vertices;i++){P p=map({f32(original,20+size_t(i)*12),f32(original,24+size_t(i)*12),f32(original,28+size_t(i)*12)});for(int k=0;k<3;k++)floatAt(out,20+size_t(i)*12+k*4,p[k]);}
 // The tail stores the broadphase AABB as six floats (minimum and size, or minimum and maximum).
 size_t box=0;bool sizeForm=true;double best=1e30;
 for(size_t at=0;at+24<=original.size()-body;at+=4){float f[6];std::memcpy(f,original.data()+body+at,24);bool finite=true;for(float v:f)if(!std::isfinite(v)){finite=false;break;}if(!finite)continue;
  double a=0,b=0;for(int k=0;k<3;k++){a+=std::abs(double(f[k])-lo[k])+std::abs(double(f[k+3])-(hi[k]-lo[k]));b+=std::abs(double(f[k])-lo[k])+std::abs(double(f[k+3])-hi[k]);}
  if(a<best){best=a;box=at;sizeForm=true;}if(b<best){best=b;box=at;sizeForm=false;}
 }
 if(best<1.0){P initial{f32(original,body+box),f32(original,body+box+4),f32(original,body+box+8)};P mappedMin=map(initial);for(int k=0;k<3;k++)floatAt(out,body+box+k*4,mappedMin[k]);
  if(sizeForm){P size{f32(original,body+box+12),f32(original,body+box+16),f32(original,body+box+20)};for(int k=0;k<3;k++)floatAt(out,body+box+12+k*4,size[k]*(k==0?sx:k==1?sy:sz));}
  else{P mappedMax=map({f32(original,body+box+12),f32(original,body+box+16),f32(original,body+box+20)});for(int k=0;k<3;k++)floatAt(out,body+box+12+k*4,mappedMax[k]);}
 }
 return out;
}
inline Bytes clonePaintCar(const Bytes &cfg,const std::string &donor,const std::string &target){
 auto roots=Json::parse(Config(cfg).json());auto &paints=configNamed(roots,"Paints");auto &list=paints["value"];auto &source=configNamed(list,donor);Json clone=source;clone["name"]=target;clone["hash"]=configNameHash(target);for(auto &node:list)if(node["name"]==target){node=clone;return configEncode(roots);}list.push_back(clone);return configEncode(roots);
}

inline Bytes fitShadow(const Bytes &model,double xl,double xh,double zl,double zh){
 auto out=model;uint32_t sa=u32(model,84),va=u32(model,92);P lo{1e30,1e30,1e30},hi{-1e30,-1e30,-1e30};
 for(uint32_t i=0;i<u32(model,24);i++){size_t r=sa+size_t(i)*48;uint32_t stride=u32(model,r+4),fv=u32(model,r+8),vc=u32(model,r+12);for(uint32_t j=0;j<vc;j++){size_t p=va+(size_t(fv)+j)*stride;P v{f32(model,p),f32(model,p+4),f32(model,p+8)};for(int k=0;k<3;k++){lo[k]=std::min(lo[k],v[k]);hi[k]=std::max(hi[k],v[k]);}}}
 if(!(hi[0]>lo[0]&&hi[2]>lo[2]&&xh>xl&&zh>zl))return model;
 double sx=(xh-xl)/(hi[0]-lo[0]),sz=(zh-zl)/(hi[2]-lo[2]);Bounds bounds;
 for(uint32_t i=0;i<u32(model,24);i++){size_t r=sa+size_t(i)*48;uint32_t stride=u32(model,r+4),fv=u32(model,r+8),vc=u32(model,r+12);for(uint32_t j=0;j<vc;j++){size_t p=va+(size_t(fv)+j)*stride;P v{f32(model,p),f32(model,p+4),f32(model,p+8)};v[0]=xl+(v[0]-lo[0])*sx;v[2]=zl+(v[2]-lo[2])*sz;floatAt(out,p,v[0]);floatAt(out,p+8,v[2]);bounds.add(v);}}
 auto size=bounds.size(),center=bounds.center();for(int k=0;k<3;k++){floatAt(out,100+k*4,size[k]);floatAt(out,112+k*4,center[k]);}floatAt(out,124,std::sqrt(dotP(size,size))/2);return out;
}

inline Bytes fitGarageBox(const Bytes &model,double xl,double xh,double yl,double yh,double zl,double zh){
 auto out=model;uint32_t nc=u32(model,20),na=u32(model,76),sa=u32(model,84),va=u32(model,92),la=u32(model,88);
 for(uint32_t i=0;i<nc;i++){size_t n=na+size_t(i)*156;if(text(model,n,32)!="BoundingBox")continue;int first=int32_t(u32(model,n+120));if(first<0)return model;
  std::vector<int> meshes;size_t cursor=la+size_t(first)*2;for(;;){require(cursor+2<=va,"Unterminated garage box links");int link=int16_t(u16(model,cursor));cursor+=2;if(link<0)break;meshes.push_back(link);}
  P lo{1e30,1e30,1e30},hi{-1e30,-1e30,-1e30};for(int mi:meshes){size_t r=sa+size_t(mi)*48;uint32_t stride=u32(model,r+4),fv=u32(model,r+8),vc=u32(model,r+12);for(uint32_t j=0;j<vc;j++){size_t p=va+(size_t(fv)+j)*stride;P v{f32(model,p),f32(model,p+4),f32(model,p+8)};for(int k=0;k<3;k++){lo[k]=std::min(lo[k],v[k]);hi[k]=std::max(hi[k],v[k]);}}}
  if(!(hi[0]>lo[0]&&hi[1]>lo[1]&&hi[2]>lo[2]&&xh>xl&&yh>yl&&zh>zl))return model;
  double s[3]={(xh-xl)/(hi[0]-lo[0]),(yh-yl)/(hi[1]-lo[1]),(zh-zl)/(hi[2]-lo[2])},center[3]={double(xl+xh)/2,double(yl+yh)/2,double(zl+zh)/2},local[3]={double(lo[0]+hi[0])/2,double(lo[1]+hi[1])/2,double(lo[2]+hi[2])/2};
  for(int k=0;k<12;k++)floatAt(out,n+32+k*4,0);
  floatAt(out,n+32,s[0]);floatAt(out,n+48,s[1]);floatAt(out,n+64,s[2]);
  for(int k=0;k<3;k++)floatAt(out,n+68+k*4,center[k]-s[k]*local[k]);
  return out;
 }
 return model;
}

inline void exportModel(const Archive&archive,const Source&s,const Donor&donor,const Options&o,const fs::path&destination,const std::string&name,bool overwrite=true,const std::string&packageId={},const Plan *planOverride=nullptr,const std::string &authoredId={}){
 auto original=stockAsset(archive,"graphics/vehicles/"+donor.id+"/lod0_livery.bdae");std::string authored=authoredId.empty()?donor.id:authoredId;
 require(authored.size()==3&&std::all_of(authored.begin(),authored.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9');}),"Authored car ID must be three lowercase letters or digits");
 if(overwrite)require(authored==donor.id,"Replacing a donor keeps its car ID");
 auto donorPrefix="graphics/vehicles/"+donor.id+"/",prefix="graphics/vehicles/"+authored+"/";
 Plan plan=planOverride?*planOverride:defaultPlan(s,donor.id);if(!planOverride)for(auto &binding:plan.materials)binding.size=qualityTextureSize(binding.size,binding.category,o.quality);std::vector<Texture> atlasTextures;
 for(auto &binding:plan.materials){if(binding.image<0||!binding.importTexture)continue;auto image=textures::decodePNG(s.images.at(binding.image));auto width=textures::powerOfTwo(image.width,64,binding.size),height=textures::powerOfTwo(image.height,64,binding.size);image=textures::resize(image,width,height);const char *fourcc=textures::hasAlpha(image)?"DXT5":"DXT1";auto dds=textures::buildDDS(image,fourcc);Texture texture;texture.name=binding.texture;texture.fourcc=fourcc;texture.width=width;texture.height=height;texture.flags=binding.flags;texture.mips=u32(dds,28);texture.caps=u32(dds,112);texture.dds=std::move(dds);atlasTextures.push_back(std::move(texture));}
 auto result=convert(s,original,o,plan);ImportedMod mod({},&archive);Origin origin({},packageId.empty()?newPackageId():packageId);origin.setOverwrite(overwrite);origin.setMapping(authored,donor.id);mod.setOriginMetadata(origin);std::map<std::string,Bytes> assets;
 for(auto&e:archive.entries){auto path=lower(e.name);if(path.rfind(donorPrefix,0)==0)assets["assets/"+prefix+path.substr(donorPrefix.size())]=archive.load(e);}assets["assets/"+prefix+"lod0_livery.bdae"]=result.model;
 if(!atlasTextures.empty()||o.neutralMask){auto atlas=stockAsset(archive,donorPrefix+"textures/batchTextures.brres");for(auto &texture:atlasTextures)atlas=textures::withTexture(atlas,texture);if(o.neutralMask)atlas=textures::withTexture(atlas,neutralPaintMask());assets["assets/"+prefix+"textures/batchtextures.brres"]=std::move(atlas);}
 std::map<std::string,std::string> renames{{"car_"+authored,name}};std::string paintToken;std::optional<PaintPackResult> paintAdded;if(o.addPaint)paintToken=newPaintToken(archive,authored).first;
 if(o.addPaint){for(const char *path:{"scripts/config/paint_PC.cfg","scripts/config/paint.cfg"}){try{auto cfg=stockAsset(archive,path);if(authored!=donor.id)cfg=clonePaintCar(cfg,donor.id,authored);auto added=addPaintPack(cfg,authored,paintToken);if(!added)continue;assets["assets/"+lower(path)]=added->cfg;if(std::string(path)=="scripts/config/paint_PC.cfg")paintAdded=added;}catch(const std::exception &){if(std::string(path)=="scripts/config/paint_PC.cfg")throw;}}
  if(paintAdded){renames[paintToken]=o.paintName.empty()?"Black":o.paintName;bool luaPatched=false;for(const char *path:{"scripts/config/paint_PC.luac","scripts/config/paint.luac"}){try{auto lua=stockAsset(archive,path);auto patched=configLuaPatch(lua,{{{"Paints",authored,int(paintAdded->index+1)},paintAdded->pack}});assets["assets/"+lower(path)]=patched;luaPatched=true;}catch(const std::exception &){}}result.report["paint"]={{"token",paintToken},{"name",o.paintName.empty()?"Black":o.paintName},{"index",paintAdded->index},{"lua_patched",luaPatched}};}
  else result.report["paint"]={{"skipped","donor already uses the nine-pack limit"}};}
 if(o.fitCollision){auto &r=result.report;require(r.contains("wheel_lo")&&r.contains("wheel_hi")&&r.contains("vehicle_lo")&&r.contains("vehicle_hi"),"Missing collision fit bounds");auto wl=r["wheel_lo"],wh=r["wheel_hi"],vl=r["vehicle_lo"],vh=r["vehicle_hi"];auto hull=stockAsset(archive,donorPrefix+"collision.hzc");double xl=wl[0].get<double>(),xh=wh[0].get<double>();if(!(xh>xl)){xl=vl[0].get<double>();xh=vh[0].get<double>();}assets["assets/"+prefix+"collision.hzc"]=fitCollision(hull,xl,xh,vl[2].get<double>(),vh[2].get<double>(),vh[1].get<double>());r["collision_fit"]={{"target_x",Json::array({wl[0],wh[0]})},{"target_z",Json::array({vl[2],vh[2]})},{"target_top",vh[1]}};}
 if(o.fitShadow){auto &r=result.report;auto vl=r["vehicle_lo"],vh=r["vehicle_hi"];double xl=vl[0].get<double>(),xh=vh[0].get<double>(),yl=vl[1].get<double>(),yh=vh[1].get<double>(),zl=vl[2].get<double>(),zh=vh[2].get<double>();assets["assets/"+prefix+"shadow.bdae"]=fitShadow(stockAsset(archive,donorPrefix+"shadow.bdae"),xl,xh,zl,zh);assets["assets/"+prefix+"nodes.bdae"]=fitGarageBox(stockAsset(archive,donorPrefix+"nodes.bdae"),xl,xh,yl,yh,zl,zh);r["shadow_box_fit"]=true;}
 for(auto&f:renamedStrings([&](const std::string&p){return stockAsset(archive,p);},renames,o.allLocaleNames,true))assets[f.first]=f.second;
 result.report["options"]={{"source_scale",o.scale},{"quarter_turns",o.yaw},{"z_up",o.zUp},{"fit_to_donor",o.fit}};result.report["donor_id"]=donor.id;result.report["donor_name"]=donor.name;result.report["operation"]=overwrite?"Replace donor car":"Add as a new car";result.report["overwrite"]=overwrite?1:0;result.report["package_id"]=origin.packageId();result.report["authored_id"]=authored;
 Json materialReport=Json::array();for(auto &binding:plan.materials)materialReport.push_back({{"name",binding.name},{"template",binding.templateMaterial},{"texture",binding.texture},{"size",binding.size},{"image",binding.image},{"flags",binding.flags}});result.report["materials"]=materialReport;result.report["imported_textures"]=atlasTextures.size();
 result.report["limitations"]={"Collision hull and effect shells are rebuilt from the imported exterior; a neutral body mask is added. An extra paint is added only when the donor is below the client's nine-pack limit, with CFG and 32-bit Lua updates. Donor handling and animation retained; the shadow and garage box are fitted.","Imported base-color textures are converted to native DXT1/DXT5 atlases with mip chains. Metallic/roughness, normal and occlusion maps are not converted.","Body paint, glass and tyres keep native materials; use the editor to adjust paint, livery and HUD / boosters before final export."};Json roles=Json::array();for(auto&p:s.parts)roles.push_back({{"part",p.name},{"material",p.material},{"role",roleName(p.role)}});result.report["parts"]=roles;auto report=result.report.dump(2)+"\n";mod.files["model-import.json"]=Bytes(report.begin(),report.end());
 if(!s.author.empty()||!s.license.empty()||!s.sourceUrl.empty()){std::string text=(s.title.empty()?name:s.title)+"\n\n";if(!s.author.empty())text+="Author: "+s.author+"\n";if(!s.license.empty())text+="License: "+s.license+"\n";if(!s.sourceUrl.empty())text+="Source: "+s.sourceUrl+"\n";text+="\nModified: geometry transformed, regrouped and converted to native assets for Riff Racer.\n";mod.files["ATTRIBUTION.txt"]=Bytes(text.begin(),text.end());}
 mod.exportCopy(destination,assets);
}
}
