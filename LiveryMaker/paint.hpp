#pragma once
#include "core.hpp"
#include "config_author.hpp"
namespace livery {
// Patch appearance payloads in place; rebuild configuration only for explicit
// price edits. Preserve unrelated native fields and Lua initialization logic.
class PaintConfig {
    Bytes original_, current_, lua_;
    std::map<std::pair<std::string,size_t>,int> prices_;
    std::vector<PaintSelection> selections_;
    uint32_t nodes_,strings_;
    size_t record(uint32_t index) const {require(index<nodes_,"Invalid paint record");return 20+size_t(index)*32;}
    std::string name(uint32_t index) const {auto p=record(index);return text(current_,strings_+u32(current_,p),current_.size()-strings_-u32(current_,p));}
    uint32_t child(uint32_t parent,const std::string &key) const {
        size_t p=record(parent);require(u32(current_,p+20)==2,"Paint record is not a table");
        auto first=u32(current_,p+8),count=u32(current_,p+24);
        for(uint32_t i=0;i<count;i++)if(name(first+i)==key)return first+i;
        return UINT32_MAX;
    }
    uint32_t pack(const PaintSelection &selection) const {
        uint32_t root=UINT32_MAX;
        for(unsigned i=0;i<u32(current_,16);i++)if(name(i)=="Paints")root=i;
        require(root!=UINT32_MAX,"Missing Paints table");auto car=child(root,selection.car);require(car!=UINT32_MAX,"Missing car paint table");
        auto p=record(car);require(selection.packIndex<u32(current_,p+24),"Paint index out of range");return u32(current_,p+8)+selection.packIndex;
    }
public:
    static constexpr const char *resource="scripts/config/paint_pc.cfg";
    explicit PaintConfig(Bytes cfg):original_(std::move(cfg)),current_(original_) {(void)Config(original_).json();nodes_=u32(current_,12);strings_=20+nodes_*32;}
    PaintConfig(Bytes cfg,Bytes lua,std::vector<PaintSelection> selections):PaintConfig(std::move(cfg)){lua_=std::move(lua);selections_=std::move(selections);}
    void clearLivery(const PaintSelection &selection){auto offsets=values(selection,"livery",1);require(offsets.size()==1,"Missing native livery field");float value=-1;uint32_t bits;std::memcpy(&bits,&value,4);set32(current_,offsets[0],bits);}
    explicit PaintConfig(const Archive &archive):original_(asset(archive,"scripts/config/paint_PC.cfg")),current_(original_) {
        lua_=asset(archive,"scripts/config/paint_PC.luac");
        selections_=paintSelections(archive,displayNames(archive));
        (void)Config(original_).json();nodes_=u32(current_,12);strings_=20+nodes_*32;
    }
    std::vector<size_t> values(const PaintSelection &selection,const std::string &key,unsigned count) const {
        auto field=child(pack(selection),key);if(field==UINT32_MAX)return {};
        std::vector<size_t> offsets;auto p=record(field);
        if(count==1) {require(u32(current_,p+20)==1,"Paint scalar is not numeric");offsets.push_back(p+8);}
        else {
            require(u32(current_,p+20)==2 && u32(current_,p+24)>=count,"Invalid paint color");
            for(unsigned i=0;i<count;i++){auto item=record(u32(current_,p+8)+i);require(u32(current_,item+20)==1,"Non-numeric color channel");offsets.push_back(item+8);}
        }
        return offsets;
    }
    std::vector<float> get(const PaintSelection &selection,const std::string &key,unsigned count) const {
        std::vector<float> result;for(auto p:values(selection,key,count))result.push_back(f32(current_,p));return result;
    }
    void set(const PaintSelection &selection,const std::string &key,const std::vector<float> &value) {
        static const std::set<std::string> colors{"col0","col1","col2","specCol","alloysCol0","alloysCol1"};
        require(colors.count(key) && value.size()==3,"Unsupported paint field");
        auto offsets=values(selection,key,3);require(offsets.size()==3,"Color is absent in original paint");
        for(float v:value)require(std::isfinite(v) && v>=0 && v<=255,"Paint color must be in 0–255 range");
        for(size_t i=0;i<3;i++){uint32_t bits;std::memcpy(&bits,&value[i],4);set32(current_,offsets[i],bits);}
    }
    void setLivery(const PaintSelection &selection,unsigned id) {
        require(selection.resource.empty() && id>=1 && id<=99,"New decals require an undecorated paint and ID 1–99");
        auto offsets=values(selection,"livery",1);require(offsets.size()==1,"Missing native livery field");
        float value=float(id);uint32_t bits;std::memcpy(&bits,&value,4);set32(current_,offsets[0],bits);
    }
    static const std::vector<std::string> &appearanceFields(){static const std::vector<std::string> fields{"col0","col1","col2","colLerp0","colLerp1","reflAmbient","reflFalloff","reflStrength","specPower","specScale","specCol","alloysCol0","alloysCol1","alloysColLerp","livery"};return fields;}
    static unsigned fieldCount(const std::string &field){return field=="col0"||field=="col1"||field=="col2"||field=="specCol"||field=="alloysCol0"||field=="alloysCol1"?3:1;}
    void copyAppearance(const PaintSelection &from,const PaintSelection &to){
        require(from.car==to.car,"Default paint must belong to the same car");Bytes next=current_;
        for(const auto &field:appearanceFields()){auto source=values(from,field,fieldCount(field)),dest=values(to,field,fieldCount(field));require(source.size()==dest.size(),"Incompatible paint appearance fields");for(size_t i=0;i<source.size();i++)std::copy_n(current_.begin()+source[i],4,next.begin()+dest[i]);}
        current_=std::move(next);
    }
    void reset(const PaintSelection &selection) {
        for(const auto &field:appearanceFields())for(auto p:values(selection,field,fieldCount(field)))std::copy_n(original_.begin()+p,4,current_.begin()+p);
    }
    // Extend the stock Lua 5.1 chunk just before its final RETURN. Preserve
    // all stock instructions/constants: shared constants must never be patched.
    Bytes luaBytes() const {
        if(!changed())return lua_;
        const Bytes header{0x1b,'L','u','a',0x51,0,1,4,4,4,4,0};
        require(lua_.size()>48 && std::equal(header.begin(),header.end(),lua_.begin()),"Unsupported native paint Lua format");
        require(u32(lua_,12)==0 && lua_[24]==0 && lua_[25]==0 && lua_[27]>=3,"Unsupported paint Lua prototype");
        auto codeCount=u32(lua_,28);size_t constantsAt=32+size_t(codeCount)*4;
        require(codeCount>0 && constantsAt+4<=lua_.size(),"Invalid paint Lua code");
        require(u32(lua_,constantsAt-4)==0x0080001e,"Missing final paint Lua RETURN");
        // This stock chunk is a straight-line table initializer, no branches,
        // functions or dynamic stack instructions. Refuse unfamiliar scripts.
        for(unsigned i=0;i+1<codeCount;i++) {
            auto instruction=u32(lua_,32+i*4),op=instruction&63;
            require(op==1 || op==5 || op==6 || op==7 || op==9 || op==10 || (op==34 && ((instruction>>14)&511)!=0),"Unsupported paint Lua instruction");
        }
        auto constantCount=u32(lua_,constantsAt);size_t end=constantsAt+4;
        for(unsigned i=0;i<constantCount;i++) {
            require(end<lua_.size(),"Truncated Lua constant");auto type=lua_[end++];
            if(type==3)end+=4;
            else if(type==4){auto length=u32(lua_,end);end+=4;require(length>0 && length<=lua_.size()-end,"Invalid Lua string");end+=length;require(lua_[end-1]==0,"Unterminated Lua string");}
            else if(type==1)end++;
            else require(type==0,"Unsupported Lua constant");
            require(end<=lua_.size(),"Truncated Lua constant payload");
        }
        require(lua_.size()-end==16 && std::all_of(lua_.begin()+end,lua_.end(),[](uint8_t b){return b==0;}),"Unsupported paint Lua debug/functions");
        Bytes extraCode,extraConstants;
        auto stringConstant=[&](const std::string &value){auto index=constantCount++;extraConstants.push_back(4);put32(extraConstants,uint32_t(value.size()+1));append(extraConstants,value);extraConstants.push_back(0);return index;};
        auto numberConstant=[&](float value){auto index=constantCount++;uint32_t bits;std::memcpy(&bits,&value,4);extraConstants.push_back(3);put32(extraConstants,bits);return index;};
        auto abx=[&](unsigned op,unsigned a,unsigned bx){require(bx<262144,"Too many Lua constants");put32(extraCode,op|(a<<6)|(bx<<14));};
        auto abc=[&](unsigned op,unsigned a,unsigned b,unsigned c){put32(extraCode,op|(a<<6)|(c<<14)|(b<<23));};
        for(const auto &selection:selections_)for(const auto &field:appearanceFields()) {
            bool scalar=fieldCount(field)==1;
            auto offsets=values(selection,field,scalar?1:3);bool edited=false;
            for(auto offset:offsets)if(u32(original_,offset)!=u32(current_,offset))edited=true;
            if(!edited)continue;
            abx(5,0,stringConstant("Paints"));
            abx(1,1,stringConstant(selection.car));abc(6,0,0,1);
            abx(1,1,numberConstant(float(selection.packIndex+1)));abc(6,0,0,1);
            abx(1,1,stringConstant(field));
            if(scalar) {
                abx(1,2,numberConstant(f32(current_,offsets[0])));abc(9,0,1,2);continue;
            }
            abc(6,0,0,1);
            for(unsigned channel=0;channel<3;channel++) {
                abx(1,1,numberConstant(float(channel+1)));
                abx(1,2,numberConstant(f32(current_,offsets[channel])));abc(9,0,1,2);
            }
        }
        Bytes out(lua_.begin(),lua_.begin()+28);put32(out,codeCount+uint32_t(extraCode.size()/4));
        out.insert(out.end(),lua_.begin()+32,lua_.begin()+constantsAt-4);
        out.insert(out.end(),extraCode.begin(),extraCode.end());put32(out,0x0080001e);
        put32(out,constantCount);out.insert(out.end(),lua_.begin()+constantsAt+4,lua_.begin()+end);
        out.insert(out.end(),extraConstants.begin(),extraConstants.end());out.insert(out.end(),lua_.begin()+end,lua_.end());return out;
    }
    int price(const PaintSelection &p)const{auto found=prices_.find({p.car,p.packIndex});if(found!=prices_.end())return found->second;auto v=get(p,"packPrice",1);return v.empty()?0:int(v[0]);}
    void setPrice(const PaintSelection&p,int value){require(value>=0&&value<=10000000,"Paint price must be 0–10,000,000");prices_[{p.car,p.packIndex}]=value;}
    std::map<std::string,Bytes> files() const {
        auto cfg=current_,lua=luaBytes();if(!prices_.empty()){
            auto tree=Json::parse(Config(cfg).json());auto&paints=configNamed(tree,"Paints")["value"];std::vector<ConfigAssignment> patches;
            for(auto&e:prices_){auto&packs=configNamed(paints,e.first.first)["value"];configSet(packs.at(e.first.second)["value"],"packPrice",e.second);patches.push_back({{"Paints",e.first.first,e.first.second+1,"packPrice"},e.second});}
            cfg=configEncode(tree);lua=configLuaPatch(lua,patches);
        }
        return {{resource,cfg},{"scripts/config/paint_pc.luac",lua}};
    }
    bool changed() const {return current_!=original_||!prices_.empty();}
    const Bytes &bytes() const {return current_;}
    const Bytes &original() const {return original_;}
};
}
