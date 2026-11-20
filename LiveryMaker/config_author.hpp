#pragma once
#include "core.hpp"
namespace livery {
// Validated native GFCF v3 writer. Keep record names/hashes and values;
// rebuilding is necessary when a previously absent field is inserted.
inline uint32_t configNameHash(const std::string &s){uint32_t h=5387;for(unsigned char c:s)h=h*33+c;return h;}
inline Json configScalar(const std::string &name,const Json &value){return {{"name",name},{"hash",configNameHash(name)},{"type",value.is_string()?0:1},{"value",value}};}
inline Json &configNamed(Json &nodes,const char *name){for(auto &n:nodes)if(n.at("name")==name)return n;throw std::runtime_error(std::string("Missing config table: ")+name);}
inline Json &configNamed(Json &nodes,const std::string &name){return configNamed(nodes,name.c_str());}
inline void configSet(Json &nodes,const std::string &name,const Json &value){for(auto &n:nodes)if(n.at("name")==name){n["type"]=value.is_string()?0:1;n["value"]=value;return;}nodes.push_back(configScalar(name,value));}
inline Bytes configEncode(const Json &roots){
    require(roots.is_array()&&!roots.empty(),"Empty config");std::vector<Bytes> records(roots.size());Bytes pool;std::map<std::string,uint32_t> strings;
    auto string=[&](const std::string&s){require(s.find('\0')==std::string::npos,"NUL in config string");auto it=strings.find(s);if(it!=strings.end())return it->second;require(pool.size()+s.size()+1<UINT32_MAX,"Config strings too large");auto p=uint32_t(pool.size());append(pool,s);pool.push_back(0);strings[s]=p;return p;};
    std::function<void(size_t,const Json&,unsigned)> fill=[&](size_t i,const Json&n,unsigned depth){
        require(depth<128&&records.size()<1000000,"Config too deep/large");auto name=n.at("name").get<std::string>();auto type=n.at("type").get<unsigned>();uint32_t value=0,count=0;
        require(n.at("hash").get<uint32_t>()==configNameHash(name),"Unexpected native config name hash");
        if(type==2){const auto&children=n.at("value");require(children.is_array(),"Invalid table");value=uint32_t(records.size());count=uint32_t(children.size());records.resize(records.size()+count);for(size_t j=0;j<count;j++)fill(value+j,children[j],depth+1);}
        else if(type==0)value=string(n.at("value").get<std::string>());
        else {require(type==1,"Unknown config type");float f=n.at("value").get<float>();require(std::isfinite(f),"Nonfinite config number");std::memcpy(&value,&f,4);}
        Bytes record;for(uint32_t v:{string(name),0u,value,0u,configNameHash(name),type,count,0u})put32(record,v);records[i]=std::move(record);
    };
    for(size_t i=0;i<roots.size();i++)fill(i,roots[i],0);
    Bytes out;for(uint32_t v:{0x46434647u,3u,uint32_t(pool.size()),uint32_t(records.size()),uint32_t(roots.size())})put32(out,v);for(auto&r:records)out.insert(out.end(),r.begin(),r.end());out.insert(out.end(),pool.begin(),pool.end());
    require(Json::parse(Config(out).json())==roots,"Config roundtrip mismatch");return out;
}
struct ConfigAssignment {std::vector<Json> path;Json value;};
// Append assignments to an existing native chunk, retaining original dofile
// calls, closures, constants, untouched tables and nil array holes.
inline Bytes configLuaPatch(const Bytes &original,const std::vector<ConfigAssignment>&edits){
    if(edits.empty())return original;
    const Bytes header{0x1b,'L','u','a',0x51,0,1,4,4,4,4,0};require(original.size()>48&&std::equal(header.begin(),header.end(),original.begin())&&u32(original,12)==0,"Unsupported native Lua prototype");
    unsigned oldCode=u32(original,28);size_t ca=32+size_t(oldCode)*4;require(oldCode&&ca+4<=original.size()&&u32(original,ca-4)==0x0080001e,"Unsupported final Lua return");unsigned count=u32(original,ca);size_t end=ca+4;
    for(unsigned i=0;i<count;i++){require(end<original.size(),"Truncated Lua constants");auto t=original[end++];if(t==3)end+=4;else if(t==1)end++;else if(t==4){auto n=u32(original,end);end+=4;require(n&&end<=original.size()&&n<=original.size()-end,"Bad Lua string");end+=n;}else require(t==0,"Unsupported Lua constant");require(end<=original.size(),"Truncated Lua constant");}
    Bytes code,constants;unsigned maxReg=original[27];
    auto constant=[&](const Json&v){unsigned id=count++;require(id<262144,"Too many Lua constants");if(v.is_string()){constants.push_back(4);auto s=v.get<std::string>();put32(constants,uint32_t(s.size()+1));append(constants,s);constants.push_back(0);}else{constants.push_back(3);float f=v.get<float>();require(std::isfinite(f),"Nonfinite Lua constant");uint32_t bits;std::memcpy(&bits,&f,4);put32(constants,bits);}return id;};
    auto abc=[&](unsigned op,unsigned a,unsigned b,unsigned c){put32(code,op|(a<<6)|(c<<14)|(b<<23));};auto abx=[&](unsigned op,unsigned a,unsigned bx){put32(code,op|(a<<6)|(bx<<14));};
    std::function<void(const Json&,unsigned)> load=[&](const Json&v,unsigned reg){maxReg=std::max(maxReg,reg+3);require(maxReg<250,"Lua nesting too deep");if(v.is_null())abc(3,reg,reg,0);else if(v.is_boolean())abc(2,reg,v.get<bool>(),0);else if(v.is_object()){abc(10,reg,0,0);for(auto it=v.begin();it!=v.end();++it){abx(1,reg+1,constant(it.key()));load(it.value(),reg+2);abc(9,reg,reg+1,reg+2);}}else if(v.is_array()){abc(10,reg,0,0);for(size_t i=0;i<v.size();i++){abx(1,reg+1,constant(Json(i+1)));load(v[i],reg+2);abc(9,reg,reg+1,reg+2);}}else abx(1,reg,constant(v));};
    for(const auto&e:edits){require(e.path.size()>=2&&e.path[0].is_string(),"Invalid config assignment path");abx(5,0,constant(e.path[0]));for(size_t i=1;i+1<e.path.size();i++){abx(1,1,constant(e.path[i]));abc(6,0,0,1);}abx(1,1,constant(e.path.back()));load(e.value,2);abc(9,0,1,2);}
    Bytes out(original.begin(),original.begin()+28);out[27]=uint8_t(maxReg);put32(out,oldCode+uint32_t(code.size()/4));out.insert(out.end(),original.begin()+32,original.begin()+ca-4);out.insert(out.end(),code.begin(),code.end());put32(out,0x0080001e);put32(out,count);out.insert(out.end(),original.begin()+ca+4,original.begin()+end);out.insert(out.end(),constants.begin(),constants.end());out.insert(out.end(),original.begin()+end,original.end());return out;
}

}
