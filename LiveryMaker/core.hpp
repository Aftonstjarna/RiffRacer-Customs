#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
// Intentional authoring tool; reuses ReRiff's original-format decoders.
#include "../NativeLinux/native_assets.hpp"
#include <map>
#include <numeric>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace livery {
using namespace rr;
inline int processId() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}
inline void put16(Bytes &b, uint32_t x) { b.push_back(x & 255); b.push_back((x >> 8) & 255); }
inline void put32(Bytes &b, uint32_t x) { put16(b,x); put16(b,x>>16); }
inline void append(Bytes &b, const std::string &s) { b.insert(b.end(),s.begin(),s.end()); }
inline void set32(Bytes &b, size_t p, uint32_t x) { for(int i=0;i<4;i++) b.at(p+i)=uint8_t(x>>(8*i)); }
inline bool isOutputLink(const fs::path &path) {
#ifdef _WIN32
    DWORD attributes=GetFileAttributesW(path.c_str());
    return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0;
#else
    return fs::is_symlink(fs::symlink_status(path));
#endif
}
inline fs::path resolveOutputParent(const fs::path &path) {
#ifdef _WIN32
    auto existing=path;std::vector<fs::path> missing;
    while(!fs::exists(existing)) {
        require(!isOutputLink(existing),"Output parent is a dangling link");
        require(existing.has_relative_path(),"Output parent is unavailable");
        missing.push_back(existing.filename());existing=existing.parent_path();
    }
    HANDLE handle=CreateFileW(existing.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    require(handle!=INVALID_HANDLE_VALUE,"Cannot resolve output directory");
    DWORD length=GetFinalPathNameByHandleW(handle,nullptr,0,FILE_NAME_NORMALIZED);
    if(!length){CloseHandle(handle);throw std::runtime_error("Cannot resolve output directory name");}
    std::wstring name(length,L'\0');DWORD copied=GetFinalPathNameByHandleW(handle,name.data(),length,FILE_NAME_NORMALIZED);CloseHandle(handle);
    require(copied>0 && copied<length,"Cannot read resolved output directory");name.resize(copied);
    if(name.rfind(L"\\\\?\\UNC\\",0)==0)name=L"\\\\"+name.substr(8);
    else if(name.rfind(L"\\\\?\\",0)==0)name=name.substr(4);
    fs::path result(name);for(auto it=missing.rbegin();it!=missing.rend();++it)result/=*it;
    return result;
#else
    return fs::weakly_canonical(path);
#endif
}
inline fs::path safePath(const fs::path &p) {
    require(p.is_absolute(), "Use an absolute output path");
    for (const auto &part : p) {
        require(part != ".." && part != ".", "Output path contains traversal");
    }
    require(!p.filename().empty(), "Choose a named output folder");
    // Resolve ordinary directory aliases such as /home -> /var/home once.
    // Keep the output leaf separate so an existing mod-folder symlink cannot
    // redirect replacement, and perform all writes using the resolved path.
    auto resolved=resolveOutputParent(p.parent_path())/p.filename();
    fs::path current=resolved.root_path();
    for (const auto &part : resolved.relative_path()) {
        current /= part;
        require(!isOutputLink(current), "Output folder or unresolved parent is a symlink");
    }
    return resolved;
}
inline Image loadPNG(const fs::path &path, unsigned w, unsigned h) {
    require(fs::file_size(path)<=64*1024*1024, "PNG exceeds 64 MiB");
    png_image png{}; png.version=PNG_IMAGE_VERSION;
    auto encoded=read(path.u8string());
    require(png_image_begin_read_from_memory(&png,encoded.data(),encoded.size())!=0,"Cannot read PNG header");
    if(png.width!=w || png.height!=h) { png_image_free(&png); throw std::runtime_error("PNG must be exactly "+std::to_string(w)+" x "+std::to_string(h)); }
    png.format=PNG_FORMAT_RGBA;
    Image out{w,h,Bytes(size_t(w)*h*4)};
    bool ok=png_image_finish_read(&png,nullptr,out.pixels.data(),0,nullptr)!=0;
    png_image_free(&png); require(ok,"Invalid or truncated PNG"); return out;
}
inline Image downsample(const Image &im) {
    Image out{std::max(1u,im.width/2),std::max(1u,im.height/2),{}};
    out.pixels.resize(size_t(out.width)*out.height*4);
    for(unsigned y=0;y<out.height;y++) for(unsigned x=0;x<out.width;x++) for(unsigned c=0;c<4;c++) {
        unsigned sum=0;
        for(unsigned dy=0;dy<2;dy++) for(unsigned dx=0;dx<2;dx++)
            sum+=im.pixels[(size_t(std::min(im.height-1,y*2+dy))*im.width+std::min(im.width-1,x*2+dx))*4+c];
        out.pixels[(size_t(y)*out.width+x)*4+c]=uint8_t((sum+2)/4);
    }
    return out;
}
using Pixel=std::array<int,4>;
inline uint16_t rgb565(const Pixel &p) { return uint16_t(((p[0]*31+127)/255)<<11 | ((p[1]*63+127)/255)<<5 | ((p[2]*31+127)/255)); }
inline Pixel rgb(uint16_t p) { return {int((p>>11)*255/31),int(((p>>5)&63)*255/63),int((p&31)*255/31),255}; }
// Small deterministic BC2/BC3 encoder. Bounding-box endpoints prioritize a
// dependency-free first authoring milestone; photographic quality is limited.
inline Bytes encodeLevel(const Image &im, bool bc2) {
    Bytes out;
    for(unsigned by=0;by<im.height;by+=4) for(unsigned bx=0;bx<im.width;bx+=4) {
        std::array<Pixel,16> p{}; Pixel lo{255,255,255,255},hi{0,0,0,0};
        for(unsigned y=0;y<4;y++) for(unsigned x=0;x<4;x++) {
            size_t off=(size_t(std::min(by+y,im.height-1))*im.width+std::min(bx+x,im.width-1))*4;
            for(int c=0;c<4;c++) { int v=im.pixels[off+c]; p[y*4+x][c]=v; lo[c]=std::min(lo[c],v); hi[c]=std::max(hi[c],v); }
        }
        uint64_t bits=0;
        if(bc2) { for(int i=0;i<16;i++) bits|=uint64_t((p[i][3]*15+127)/255)<<(i*4); for(int i=0;i<8;i++) out.push_back(uint8_t(bits>>(8*i))); }
        else {
            int a=hi[3],b=lo[3]; if(a==b) { if(a<255) ++a; else --b; }
            out.push_back(uint8_t(a)); out.push_back(uint8_t(b));
            std::array<int,8> palette{a,b,0,0,0,0,0,0};
            for(int i=2;i<8;i++) palette[i]=((8-i)*a+(i-1)*b)/7;
            for(int i=0;i<16;i++) { int best=0; for(int j=1;j<8;j++) if(std::abs(p[i][3]-palette[j])<std::abs(p[i][3]-palette[best])) best=j; bits|=uint64_t(best)<<(i*3); }
            for(int i=0;i<6;i++) out.push_back(uint8_t(bits>>(8*i)));
        }
        uint16_t a=rgb565(hi),b=rgb565(lo); if(a<b) std::swap(a,b);
        if(a==b) { if(a<65535) ++a; else --b; }
        put16(out,a);put16(out,b); std::array<Pixel,4> palette{rgb(a),rgb(b),{}, {}};
        for(int c=0;c<3;c++) { palette[2][c]=(2*palette[0][c]+palette[1][c])/3; palette[3][c]=(palette[0][c]+2*palette[1][c])/3; }
        uint32_t indices=0;
        for(int i=0;i<16;i++) { int best=0,dist=INT32_MAX; for(int j=0;j<4;j++) { int d=0;for(int c=0;c<3;c++) d+=(p[i][c]-palette[j][c])*(p[i][c]-palette[j][c]); if(d<dist) {dist=d;best=j;} } indices|=uint32_t(best)<<(2*i); }
        put32(out,indices);
    }
    return out;
}
inline Bytes encodeDDS(const Texture &t, Image image) {
    require(image.width==t.width && image.height==t.height && image.pixels.size()==size_t(t.width)*t.height*4,"Image dimensions mismatch");
    require((t.fourcc=="DXT5" || t.fourcc=="DXT3") && !t.caps,"Only original 2D DXT3/DXT5 targets are supported");
    require(t.mips<=13,"Invalid mip count");
    Bytes out(t.dds.begin(),t.dds.begin()+128);
    for(unsigned level=0;level<std::max(1u,t.mips);level++) { auto block=encodeLevel(image,t.fourcc=="DXT3"); out.insert(out.end(),block.begin(),block.end()); image=downsample(image); }
    require(out.size()==t.dds.size(),"Encoded mip layout differs from original"); return out;
}
inline Image diagnostic(const Texture &t) {
    Image im=decodeDDS(t);
    // Retain original alpha. Four asymmetric colored corner markers and a
    // diagonal stripe make UV orientation and visible coverage easy to report.
    for(unsigned y=0;y<im.height;y++) for(unsigned x=0;x<im.width;x++) {
        bool corner=(x<im.width/4 || x>=im.width*3/4) && (y<im.height/4 || y>=im.height*3/4);
        bool stripe=std::abs(int(x)-int(y))<int(im.width/32);
        if(!corner && !stripe) continue;
        Pixel col=stripe?Pixel{255,0,255,255}:(y<im.height/2?(x<im.width/2?Pixel{255,32,32,255}:Pixel{32,255,32,255}):(x<im.width/2?Pixel{32,96,255,255}:Pixel{255,224,32,255}));
        size_t off=(size_t(y)*im.width+x)*4;for(int c=0;c<3;c++) im.pixels[off+c]=uint8_t(col[c]);
    }
    return im;
}
struct Target { std::string car,name,member; Bytes batch; Texture texture; Json packs; };
// Original localization: group descriptors, 256-word lookup table, then
// group-byte/NUL-terminated token records. Strings follow each group's order.
inline std::map<std::string,std::string> displayNames(const Archive &archive) {
    const auto tokens=asset(archive,"strings/tokens.dat");
    require(u32(tokens,0)==3 && u32(tokens,4)==2,"Unsupported localization tokens");
    const auto count=u32(tokens,8);require(count>0 && count<=32,"Invalid localization groups");
    std::vector<std::vector<std::string>> values(count);std::vector<size_t> used(count,0);
    size_t cursor=12;
    for(unsigned group=0;group<count;group++) {
        auto length=u32(tokens,cursor), entries=u32(tokens,cursor+4);(void)u32(tokens,cursor+8);cursor+=12;
        require(length && length<=64 && cursor<=tokens.size() && length<=tokens.size()-cursor && entries<=100000,"Invalid localization descriptor");
        std::string name(tokens.begin()+cursor,tokens.begin()+cursor+length);cursor+=length;
        require(name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")==std::string::npos,"Invalid localization group name");
        if(!entries)continue;
        auto strings=asset(archive,"strings/"+lower(name)+"/us.str");
        require(u32(strings,0)==3 && u32(strings,4)==2,"Unsupported localized strings");size_t offset=8;
        for(unsigned i=0;i<entries;i++) {
            auto units=u32(strings,offset);offset+=4;require(units<=100000 && offset<=strings.size() && uint64_t(units)*2<=strings.size()-offset,"Truncated localized string");
            std::string value;
            for(unsigned j=0;j<units;j++) {
                uint32_t cp=u16(strings,offset+j*2);
                if(cp>=0xd800 && cp<=0xdbff) {
                    require(j+1<units,"Truncated UTF-16 surrogate");auto tail=u16(strings,offset+(++j)*2);
                    require(tail>=0xdc00 && tail<=0xdfff,"Invalid UTF-16 surrogate");cp=0x10000+((cp-0xd800)<<10)+(tail-0xdc00);
                } else require(cp<0xdc00 || cp>0xdfff,"Unexpected UTF-16 low surrogate");
                if(cp<0x80)value.push_back(char(cp));
                else if(cp<0x800){value.push_back(char(0xc0|(cp>>6)));value.push_back(char(0x80|(cp&63)));}
                else if(cp<0x10000){value.push_back(char(0xe0|(cp>>12)));value.push_back(char(0x80|((cp>>6)&63)));value.push_back(char(0x80|(cp&63)));}
                else {value.push_back(char(0xf0|(cp>>18)));value.push_back(char(0x80|((cp>>12)&63)));value.push_back(char(0x80|((cp>>6)&63)));value.push_back(char(0x80|(cp&63)));}
            }
            offset+=units*2;values[group].push_back(std::move(value));
        }
        require(offset==strings.size(),"Localized string count mismatch");
    }
    require(cursor<=tokens.size() && tokens.size()-cursor>=1024,"Truncated token lookup table");cursor+=1024;
    std::map<std::string,std::string> result;
    while(cursor<tokens.size()) {
        auto group=tokens[cursor++];require(group<count && used[group]<values[group].size(),"Invalid token group/index");
        auto name=text(tokens,cursor,tokens.size()-cursor);cursor+=name.size()+1;
        require(!name.empty() && result.emplace(lower(name),values[group][used[group]++]).second,"Duplicate localization token");
    }
    for(unsigned group=0;group<count;group++)require(used[group]==values[group].size(),"Missing localization tokens");
    return result;
}
inline std::string localized(const std::map<std::string,std::string> &names,const std::string &key) {
    auto it=names.find(lower(key));return it==names.end() || it->second.empty()?key:it->second;
}
inline std::string paintNames(const Target &t,const std::map<std::string,std::string> &names) {
    std::string result;
    for(const auto &pack:t.packs) {
        if(!result.empty())result+=" / ";
        auto key=pack.at("name").get<std::string>();auto it=names.find(lower(key));
        result+=it==names.end()?"Unnamed paint ("+key+")":it->second;
    }
    return result.empty()?t.name:result;
}
inline std::string displayLabel(const Target &t,const std::map<std::string,std::string> &names) {
    return localized(names,"car_"+t.car)+" — "+paintNames(t,names);
}
inline Target target(const Archive &archive,const std::string &name) {
    require(name.size()==6 && name[3]=='_' && name[4]>='0' && name[4]<='9' && name[5]>='0' && name[5]<='9' && name.substr(4)!="00","Invalid livery ID");
    for(int i=0;i<3;i++) require(std::isalnum(static_cast<unsigned char>(name[i])),"Invalid car ID");
    std::string member="graphics/vehicles/decals/"+name+"/batchTextures.brres";
    Bytes batch=asset(archive,member); auto textures=decodeTextures(batch);
    require(textures.size()==1 && textures[0].name==name,"Unsupported decal batch");
    Target out{name.substr(0,3),name,member,batch,textures[0],Json::array()};
    auto cfg=asset(archive,"scripts/config/paint_PC.cfg");auto paints=cfgValue(Json::parse(Config(cfg).json()));
    auto packs=paints.at("Paints").at(out.car);
    for(size_t i=0;i<packs.size();i++) if(packs[i].value("livery",-1.0)==std::stoi(name.substr(4))) out.packs.push_back({{"index",i},{"name",packs[i].value("packName",std::string())}});
    return out;
}
inline std::vector<std::string> targets(const Archive &a) {
    std::vector<std::string> names;
    const std::string prefix="graphics/vehicles/decals/";
    for(const auto &e:a.entries) if(e.name.rfind(prefix,0)==0 && e.name.find("/batchTextures.brres",prefix.size())!=std::string::npos) names.push_back(e.name.substr(prefix.size(),6));
    std::sort(names.begin(),names.end()); return names;
}
struct PaintSelection {
    std::string car, carName, paintName, resource;
    size_t packIndex;
};
inline std::vector<PaintSelection> paintSelections(const Archive &archive,const std::map<std::string,std::string> &names) {
    auto carBytes=asset(archive,"scripts/config/vehicleSelect.cfg");
    auto cars=cfgValue(Json::parse(Config(carBytes).json())).at("carNames");
    auto paintBytes=asset(archive,"scripts/config/paint_PC.cfg");
    auto paints=cfgValue(Json::parse(Config(paintBytes).json())).at("Paints");
    auto resources=targets(archive);std::set<std::string> available(resources.begin(),resources.end());
    std::vector<PaintSelection> result;
    for(const auto &value:cars) {
        auto car=value.get<std::string>();if(car.empty())continue;
        auto carName=names.find("car_"+car);require(carName!=names.end(),"Missing selectable car name");
        const auto &packs=paints.at(car);
        for(size_t index=0;index<packs.size();index++) {
            const auto &pack=packs[index];auto token=lower(pack.value("packName",std::string()));
            auto translated=names.find(token);
            std::string label=translated==names.end()?"Paint "+std::to_string(index+1)+" (name unavailable)":translated->second;
            int livery=pack.value("livery",-1);std::ostringstream id;id<<car<<'_'<<std::setw(2)<<std::setfill('0')<<livery;
            auto resource=available.count(id.str())?id.str():std::string();
            result.push_back({car,carName->second,label,resource,index});
        }
    }
    return result;
}
// Prefer a same-car texture container; otherwise use the stock Pulse container.
// UVs always come from the selected car, never from the container donor. Reserve an ID
// absent from both the archive and every configured paint; never overwrite a stock decal.
inline Target newDecal(const Archive &archive,const PaintSelection &paint,const std::set<std::string> &reserved={}) {
    require(paint.resource.empty(),"This paint already has a decal");
    auto all=targets(archive);std::string source;
    std::set<std::string> used(all.begin(),all.end());used.insert(reserved.begin(),reserved.end());
    auto config=cfgValue(Json::parse(Config(asset(archive,"scripts/config/paint_PC.cfg")).json()));
    for(const auto &pack:config.at("Paints").at(paint.car)) {
        std::ostringstream name;name<<paint.car<<'_'<<std::setw(2)<<std::setfill('0')<<pack.value("livery",-1);used.insert(name.str());
    }
    for(const auto &id:all)if(id.substr(0,3)==paint.car){source=id;break;}
    if(source.empty())source="alf_01"; // Standard 1024x1024 DXT5 container; no donor geometry.
    std::string id;
    for(unsigned n=1;n<=99;n++){auto candidate=paint.car+"_"+(n<10?"0":"")+std::to_string(n);if(!used.count(candidate)){id=candidate;break;}}
    require(!id.empty(),"No unused decal ID available for this car");
    auto result=target(archive,source);
    // Single-texture TABT's string pool starts immediately after its entry.
    require(u32(result.batch,12)>=59 && text(result.batch,52,7)==source,"Unsupported decal name pool");
    std::copy(id.begin(),id.end(),result.batch.begin()+52);
    result.car=paint.car;result.name=id;result.member="graphics/vehicles/decals/"+id+"/batchTextures.brres";
    result.texture=decodeTextures(result.batch).at(0);
    require(result.texture.name==id,"New decal name mismatch");
    result.packs=Json::array({{{"index",paint.packIndex},{"name",paint.paintName}}});return result;
}
// Port of packaging/native_archive.py's verified tree and file-record layout.
inline std::pair<Bytes,Bytes> archivePair(const std::map<std::string,Bytes> &files) {
    require(!files.empty() && files.size()<0x3fff,"Invalid resource count");
    struct File {std::string name;uint32_t crc,size,local,central;};
    struct Node {uint16_t name,next=0xffff,child=0xffff,file=0x3fff;};
    std::vector<File> records;std::vector<Node> nodes;Bytes zip,pool;
    std::map<std::string,uint16_t> paths;std::map<std::string,std::vector<uint16_t>> children;
    uint32_t crcSum=0,centralOffset=0;
    for(const auto &item:files) {
        const auto &name=item.first;const auto &data=item.second;
        require(!name.empty() && name==lower(name) && name.size()<512 && name.front()!='/' && name.back()!='/' && name.find('\\')==std::string::npos,"Invalid resource path");
        for(unsigned char c:name)require(c>=32 && c<127,"Resource names must be ASCII");
        File f{name,uint32_t(crc32(0,data.data(),data.size())),uint32_t(data.size()),uint32_t(zip.size()),centralOffset};
        require(data.size()<256*1024*1024,"Resource too large");crcSum+=f.crc;centralOffset+=46+name.size();
        put32(zip,0x04034b50);put16(zip,20);put16(zip,0);put16(zip,0);put16(zip,0);put16(zip,33);
        put32(zip,f.crc);put32(zip,f.size);put32(zip,f.size);put16(zip,name.size());put16(zip,0);append(zip,name);zip.insert(zip.end(),data.begin(),data.end());
        std::stringstream stream(name);std::string part,parent;
        while(std::getline(stream,part,'/')) {
            require(!part.empty() && part!="." && part!="..","Invalid path component");
            auto path=parent.empty()?part:parent+"/"+part;bool leaf=path==name;
            if(!paths.count(path)) {
                require(pool.size()+part.size()+1<65536 && nodes.size()<65535,"VFS index too large");
                auto index=uint16_t(nodes.size());paths[path]=index;children[parent].push_back(index);
                nodes.push_back({uint16_t(pool.size()),0xffff,0xffff,uint16_t(leaf?records.size():0x3fff)});append(pool,part);pool.push_back(0);
            } else require(nodes[paths.at(path)].file==0x3fff && !leaf,"File/directory conflict");
            parent=path;
        }
        records.push_back(f);
    }
    for(const auto &entry:children) {
        const auto &list=entry.second;if(!entry.first.empty())nodes[paths.at(entry.first)].child=list.front();
        for(size_t i=1;i<list.size();i++)nodes[list[i-1]].next=list[i];
    }
    uint32_t central=zip.size();
    for(const auto &f:records) {
        put32(zip,0x02014b50);put16(zip,20);put16(zip,20);put16(zip,0);put16(zip,0);put16(zip,0);put16(zip,33);put32(zip,f.crc);put32(zip,f.size);put32(zip,f.size);put16(zip,f.name.size());put16(zip,0);put16(zip,0);put16(zip,0);put16(zip,0);put32(zip,0);put32(zip,f.local);append(zip,f.name);
    }
    uint32_t centralSize=zip.size()-central;put32(zip,0x06054b50);put16(zip,0);put16(zip,0);put16(zip,records.size());put16(zip,records.size());put32(zip,centralSize);put32(zip,central);put16(zip,0);
    Bytes dat;put32(dat,0x0300ffee);put32(dat,nodes.size());put32(dat,records.size());put32(dat,pool.size());put32(dat,crcSum);put16(dat,0);put16(dat,0xffff);put16(dat,children.at("").front());put16(dat,0xffff);
    for(auto &n:nodes){put16(dat,n.name);put16(dat,n.next);put16(dat,n.child);put16(dat,n.file);}
    for(auto &f:records)for(uint32_t n:{2u,0u,f.size,f.size,uint32_t(f.name.size()),f.local,f.central})put32(dat,n);
    dat.insert(dat.end(),pool.begin(),pool.end());return {dat,zip};
}
inline std::pair<Bytes,Bytes> archivePair(const std::string &name,const Bytes &data) {return archivePair(std::map<std::string,Bytes>{{name,data}});}
inline Json conflicts(const fs::path &parent,const fs::path &own,const std::string &resource) {
    Json found=Json::array();if(!fs::exists(parent))return found;
    for(const auto &entry:fs::directory_iterator(parent)) {
        if(isOutputLink(entry.path()))continue;
        auto path=entry.path();if(path==own)continue;
        if(entry.is_directory())path/="vfs.zip";
        else if(path.extension()!=".zip" || path.stem().u8string().rfind("vfs",0)!=0)continue;
        if(!fs::is_regular_file(path) || isOutputLink(path))continue;
        try { Archive a(path.u8string());for(const auto &e:a.entries)if(lower(e.name)==resource)found.push_back(path.u8string()); }
        catch(const std::exception &e) {found.push_back("Could not inspect "+path.u8string()+": "+e.what());}
    }
    return found;
}
inline Json publishMod(const std::map<std::string,Bytes> &files,Json metadata,const fs::path &destination) {
    auto dest=safePath(fs::absolute(destination));
    if(fs::exists(dest)) {
        require(fs::is_directory(dest),"Output is not a directory");
        require(fs::exists(dest/"livery-maker.json"),"Refusing an unowned output folder");
        auto owner=loadJson(dest/"livery-maker.json");require(owner.value("tool","")=="reriff-livery-maker-v1","Unknown ownership record");
        for(const auto &entry:fs::directory_iterator(dest)) require(!isOutputLink(entry.path()) && entry.is_regular_file() && (entry.path().filename()=="vfs.dat" || entry.path().filename()=="vfs.zip" || entry.path().filename()=="livery-maker.json"),"Output contains unrelated files");
        require(sha(read((dest/"vfs.dat").u8string()))==owner.at("dat_sha256") && sha(read((dest/"vfs.zip").u8string()))==owner.at("zip_sha256"),"Owned mod has been modified externally");
    }
    auto pair=archivePair(files);
    metadata["tool"]="reriff-livery-maker-v1";metadata["dat_sha256"]=sha(pair.first);metadata["zip_sha256"]=sha(pair.second);
    metadata["resources"]=Json::array();metadata["sibling_resource_conflicts"]=Json::array();
    for(const auto &entry:files) {
        metadata["resources"].push_back({{"path",entry.first},{"sha256",sha(entry.second)}});
        for(const auto &conflict:conflicts(dest.parent_path(),dest,entry.first))metadata["sibling_resource_conflicts"].push_back(conflict);
    }
    fs::create_directories(dest.parent_path());auto stage=dest;stage+=".staging-"+std::to_string(processId());auto backup=dest;backup+=".previous-"+std::to_string(processId());
    require(!fs::exists(stage) && !fs::exists(backup),"Staging path already exists");require(fs::create_directory(stage),"Cannot create staging folder");
    bool moved=false;
    try {
        write(stage/"vfs.dat",pair.first);write(stage/"vfs.zip",pair.second);save(stage/"livery-maker.json",metadata);
        Archive check((stage/"vfs.zip").u8string());require(check.entries.size()==files.size(),"ZIP file count mismatch");
        for(const auto &entry:check.entries)require(check.load(entry)==files.at(entry.name),"ZIP round-trip failed");
        if(fs::exists(dest)) {fs::rename(dest,backup);moved=true;}
        fs::rename(stage,dest);if(moved)fs::remove_all(backup);
    } catch(...) { if(moved && !fs::exists(dest))fs::rename(backup,dest);fs::remove_all(stage);throw; }
    return metadata;
}
inline Bytes editedBatch(const Target &t,const Image *image) {
    Bytes batch=t.batch;
    if(image){auto dds=encodeDDS(t.texture,*image);size_t offset=u32(batch,12)+u32(batch,32);require(dds.size()==u32(batch,36),"DDS size changed");std::copy(dds.begin(),dds.end(),batch.begin()+offset);}
    require(decodeTextures(batch).size()==1,"Batch validation failed");return batch;
}
inline Json exportMod(const Target &t,const Image *image,const fs::path &destination) {
    auto batch=editedBatch(t,image);
    Json metadata={{"target",t.name},{"affected_packs",t.packs},{"resource",lower(t.member)},{"source_batch_sha256",sha(t.batch)},{"batch_sha256",sha(batch)},{"preview","Texture only; shader/UV behavior requires in-game verification"}};
    return publishMod({{lower(t.member),batch}},metadata,destination);
}
} // namespace livery
