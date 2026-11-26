#pragma once
#include "mod_editor.hpp"
namespace livery {
inline std::u16string nameUTF16(const std::string &text){
    std::u16string out;for(size_t i=0;i<text.size();){uint32_t c=uint8_t(text[i++]),minimum=0;unsigned extra=0;
        if(c>=0xf0){require(c<=0xf4,"Invalid UTF-8");c&=7;extra=3;minimum=0x10000;}
        else if(c>=0xe0){c&=15;extra=2;minimum=0x800;}
        else if(c>=0xc2){c&=31;extra=1;minimum=0x80;}
        else require(c<0x80,"Invalid UTF-8");
        require(extra<=text.size()-i,"Truncated UTF-8");for(unsigned j=0;j<extra;j++){auto tail=uint8_t(text[i++]);require((tail&0xc0)==0x80,"Invalid UTF-8 continuation");c=(c<<6)|(tail&63);}
        require(c>=minimum&&c<=0x10ffff&&!(c>=0xd800&&c<=0xdfff),"Invalid Unicode name");
        if(c<0x10000)out.push_back(char16_t(c));else{c-=0x10000;out.push_back(char16_t(0xd800+(c>>10)));out.push_back(char16_t(0xdc00+(c&1023)));}
    }return out;
}
inline void addCarNameAliases(std::map<std::string,std::string> &names,const std::map<std::string,std::string> &mappings){
    for(const auto &mapping:mappings){auto custom=lower("car_"+mapping.first),donor=lower("car_"+mapping.second);if(custom==donor||names.count(custom))continue;auto found=names.find(donor);if(found!=names.end())names[custom]=found->second;}
}
inline int stringBucket(const std::string &name){
    uint32_t hash=5381;for(unsigned char c:name){require(c<128,"Name token must be ASCII");hash=hash*33+uint32_t(c&0xdf);}return int(hash&255);
}
// Replace localized text only: retain native token IDs, order and other strings.
// New car_* tokens can be added for authored IDs; paint tokens must already exist.
template<class Load> std::map<std::string,Bytes> renamedStrings(Load load,const std::map<std::string,std::string> &changes,bool allLocales=false,bool addCarTokens=false){
    if(changes.empty())return {};
    auto tokens=load("strings/tokens.dat");require(u32(tokens,0)==3&&u32(tokens,4)==2,"Unsupported tokens");auto count=u32(tokens,8);require(count>0&&count<=32,"Invalid groups");size_t at=12;
    std::vector<std::string> groups;std::vector<uint32_t> counts,caps,used(count,0);std::map<uint32_t,std::map<uint32_t,std::string>> edits;std::set<std::string> found;
    struct Token{uint32_t group,oldIndex,newIndex;std::string name;};std::vector<Token> list;std::map<std::string,std::pair<uint32_t,uint32_t>> positions;
    for(uint32_t i=0;i<count;i++){auto length=u32(tokens,at),entries=u32(tokens,at+4),capacity=u32(tokens,at+8);at+=12;require(length>0&&length<100&&entries<100000&&at+length<=tokens.size(),"Invalid group descriptor");groups.emplace_back(tokens.begin()+at,tokens.begin()+at+length);counts.push_back(entries);caps.push_back(capacity);at+=length;}
    at+=1024;require(at<=tokens.size(),"Missing token table");
    while(at<tokens.size()){auto group=tokens[at++];require(group<count&&used[group]<counts[group],"Invalid token index");size_t zero=at;while(zero<tokens.size()&&tokens[zero])zero++;require(zero<tokens.size(),"Truncated token name");auto token=lower(std::string(tokens.begin()+at,tokens.begin()+zero));at=zero+1;list.push_back({group,used[group],0,token});positions[token]={group,used[group]};++used[group];}
    for(uint32_t i=0;i<count;i++)require(used[i]==counts[i],"Token count mismatch");
    std::vector<std::pair<std::string,std::string>> additions;
    for(const auto &change:changes){auto foundToken=positions.find(change.first);if(foundToken!=positions.end()){edits[foundToken->second.first][foundToken->second.second]=change.second;}else if(addCarTokens&&change.first.rfind("car_",0)==0){additions.push_back(change);}else require(false,"Name token missing from imported localization");}
    static const char *english[]={"us","gb"},*every[]={"us","gb","de","fr","es","it","br","ru"};const char **locales=allLocales?every:english;unsigned localeCount=allLocales?8:2;
    if(!additions.empty()){locales=every;localeCount=8;}
    std::map<std::string,Bytes> result;
    if(additions.empty()){
        for(const auto &group:edits)for(unsigned localeIndex=0;localeIndex<localeCount;localeIndex++){const char *locale=locales[localeIndex];auto path="strings/"+lower(groups[group.first])+"/"+locale+".str";auto original=load(path);require(u32(original,0)==3&&u32(original,4)==2,"Unsupported string bank");Bytes out(original.begin(),original.begin()+8);at=8;
            for(uint32_t i=0;i<counts[group.first];i++){auto units=u32(original,at);require(at+4<=original.size()&&uint64_t(units)*2<=original.size()-at-4,"Truncated localized string");auto edit=group.second.find(i);
                if(edit==group.second.end())out.insert(out.end(),original.begin()+at,original.begin()+at+4+units*2);
                else {require(!edit->second.empty()&&edit->second.size()<=240,"Name must contain 1–240 UTF-8 bytes");auto utf16=nameUTF16(edit->second);require(utf16.find(u'\0')==std::u16string::npos,"Name contains NUL");put32(out,uint32_t(utf16.size()));for(auto c:utf16)put16(out,c);}
                at+=4+units*2;
            }
            require(at==original.size(),"Unexpected localized data");result["assets/"+path]=std::move(out);
        }
        return result;
    }
    int common=-1;for(uint32_t i=0;i<count;i++)if(lower(groups[i])=="common"){common=int(i);break;}require(common>=0,"Localization bank has no Common group");
    for(const auto &addition:additions)list.push_back({uint32_t(common),UINT32_MAX,0,lower(addition.first)});
    std::stable_sort(list.begin(),list.end(),[](const Token&a,const Token&b){return stringBucket(a.name)<stringBucket(b.name);});
    std::vector<uint32_t> newCounts(count,0),newUsed(count,0);for(auto &token:list)token.newIndex=newUsed[token.group]++;
    for(uint32_t i=0;i<count;i++)newCounts[i]=newUsed[i];
    std::set<uint32_t> affected;for(const auto &change:changes){auto foundToken=positions.find(change.first);affected.insert(foundToken==positions.end()?uint32_t(common):foundToken->second.first);}
    for(uint32_t group:affected){auto groupName=lower(groups[group]);for(unsigned localeIndex=0;localeIndex<localeCount;localeIndex++){const char *locale=locales[localeIndex];auto path="strings/"+groupName+"/"+locale+".str";auto original=load(path);require(u32(original,0)==3&&u32(original,4)==2,"Unsupported string bank");std::vector<Bytes> strings;at=8;
        for(uint32_t i=0;i<counts[group];i++){auto units=u32(original,at);require(at+4<=original.size()&&uint64_t(units)*2<=original.size()-at-4,"Truncated localized string");strings.emplace_back(original.begin()+at+4,original.begin()+at+4+size_t(units)*2);at+=4+size_t(units)*2;}
        require(at==original.size(),"Unexpected localized data");Bytes out(original.begin(),original.begin()+8);
        for(const auto &token:list)if(token.group==group){auto edit=changes.find(token.name);if(edit!=changes.end()){require(!edit->second.empty()&&edit->second.size()<=240,"Name must contain 1–240 UTF-8 bytes");auto utf16=nameUTF16(edit->second);require(utf16.find(u'\0')==std::u16string::npos,"Name contains NUL");put32(out,uint32_t(utf16.size()));for(auto c:utf16)put16(out,c);}else{require(token.oldIndex!=UINT32_MAX,"Missing original token text");auto &text=strings.at(token.oldIndex);put32(out,uint32_t(text.size()/2));out.insert(out.end(),text.begin(),text.end());}}
        caps[group]=std::max(caps[group],uint32_t(out.size()-8));result["assets/"+path]=std::move(out);
    }}
    Bytes output;put32(output,3);put32(output,2);put32(output,uint32_t(groups.size()));for(uint32_t i=0;i<count;i++){auto &name=groups[i];put32(output,uint32_t(name.size()));put32(output,newCounts[i]);put32(output,caps[i]);output.insert(output.end(),name.begin(),name.end());}
    for(int bucket=0;bucket<256;bucket++){uint32_t size=0;for(const auto &token:list)if(stringBucket(token.name)==bucket)size+=uint32_t(token.name.size())+2;put32(output,size);}for(const auto &token:list){output.push_back(uint8_t(token.group));output.insert(output.end(),token.name.begin(),token.name.end());output.push_back(0);}
    result["assets/strings/tokens.dat"]=std::move(output);return result;
}
}
