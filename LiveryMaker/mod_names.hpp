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

// Replace localized text only: retain native token IDs, order and other strings.
template<class Load> std::map<std::string,Bytes> renamedStrings(Load load,const std::map<std::string,std::string> &changes){
    if(changes.empty())return {};
    auto tokens=load("strings/tokens.dat");require(u32(tokens,0)==3&&u32(tokens,4)==2,"Unsupported tokens");auto count=u32(tokens,8);require(count>0&&count<=32,"Invalid groups");size_t at=12;
    std::vector<std::string> groups;std::vector<unsigned> counts,used(count,0);std::map<unsigned,std::map<unsigned,std::string>> edits;std::set<std::string> found;
    for(unsigned i=0;i<count;i++){auto length=u32(tokens,at),entries=u32(tokens,at+4);at+=12;require(length<=tokens.size()-at,"Invalid group name");groups.emplace_back(tokens.begin()+at,tokens.begin()+at+length);counts.push_back(entries);at+=length;}
    at+=1024;require(at<=tokens.size(),"Missing token table");while(at<tokens.size()){auto group=tokens[at++];require(group<count&&used[group]<counts[group],"Invalid token index");auto token=lower(text(tokens,at,tokens.size()-at));at+=token.size()+1;auto change=changes.find(token);if(change!=changes.end()){edits[group][used[group]]=change->second;found.insert(token);}++used[group];}
    require(found.size()==changes.size(),"Name token missing from imported localization");std::map<std::string,Bytes> result;
    for(const auto &group:edits){auto path="strings/"+lower(groups[group.first])+"/us.str";auto original=load(path);require(u32(original,0)==3&&u32(original,4)==2,"Unsupported string bank");Bytes out(original.begin(),original.begin()+8);at=8;
        for(unsigned i=0;i<counts[group.first];i++){auto units=u32(original,at);require(at+4<=original.size()&&uint64_t(units)*2<=original.size()-at-4,"Truncated localized string");auto edit=group.second.find(i);
            if(edit==group.second.end())out.insert(out.end(),original.begin()+at,original.begin()+at+4+units*2);
            else {require(!edit->second.empty()&&edit->second.size()<=240,"Name must contain 1–240 UTF-8 bytes");auto utf16=nameUTF16(edit->second);require(utf16.find(u'\0')==std::u16string::npos,"Name contains NUL");put32(out,uint32_t(utf16.size()));for(auto c:utf16)put16(out,c);}
            at+=4+units*2;
        }
        require(at==original.size(),"Unexpected localized data");result["assets/"+path]=std::move(out);
    }
    return result;
}
}
