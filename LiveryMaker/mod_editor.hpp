#pragma once
#include "paint.hpp"
#include <memory>
namespace livery {
// A narrow, validated EADB writer: edit node transforms while preserving all
// geometry, links, material records and unknown data. Not a general mesh writer.
class NodeTransforms {
    Bytes original_;
    Model originalModel_;
    std::map<size_t,Matrix> edits_;
public:
    explicit NodeTransforms(Bytes bytes):original_(std::move(bytes)),originalModel_(decodeModel(original_)) {}
    const Model &originalModel() const{return originalModel_;}
    Matrix get(size_t i) const {auto found=edits_.find(i);return found==edits_.end()?originalModel_.nodes.at(i).matrix:found->second;}
    void reset(size_t i){(void)originalModel_.nodes.at(i);edits_.erase(i);}
    void set(size_t i,const Matrix &m) {
        (void)originalModel_.nodes.at(i);
        for(float v:m)require(std::isfinite(v) && std::abs(v)<10000,"Invalid attachment transform");
        double det=m[0]*(m[4]*m[8]-m[7]*m[5])-m[3]*(m[1]*m[8]-m[7]*m[2])+m[6]*(m[1]*m[5]-m[4]*m[2]);
        require(std::abs(det)>1e-10,"Attachment scale cannot be zero");
        if(m==originalModel_.nodes[i].matrix)edits_.erase(i);else edits_[i]=m;
    }
    bool changed() const{return !edits_.empty();}
    Bytes bytes() const {
        if(edits_.empty())return original_;
        Bytes result=original_;std::set<size_t> allowed;
        auto putFloat=[&](size_t p,float v){require(std::isfinite(v),"Invalid model bounds");uint32_t bits;std::memcpy(&bits,&v,4);set32(result,p,bits);for(int j=0;j<4;j++)allowed.insert(p+j);};
        for(const auto &e:edits_){auto at=u32(result,76)+e.first*156+32;for(int j=0;j<12;j++)putFloat(at+j*4,e.second[j]);}
        auto decoded=decodeModel(result);bool first=true;V lo{},hi{};
        for(const auto &node:decoded.nodes)for(int index:node.meshes)for(const auto &p:decoded.meshes.at(index).positions){
            auto v=transform(node.matrix,p,true);
            if(first){lo=hi=v;first=false;}else{lo={std::min(lo.x,v.x),std::min(lo.y,v.y),std::min(lo.z,v.z)};hi={std::max(hi.x,v.x),std::max(hi.y,v.y),std::max(hi.z,v.z)};}
        }
        require(!first,"Node model has no bounds");
        float bounds[]={hi.x-lo.x,hi.y-lo.y,hi.z-lo.z,(hi.x+lo.x)/2,(hi.y+lo.y)/2,(hi.z+lo.z)/2,F(length(V{hi.x-lo.x,hi.y-lo.y,hi.z-lo.z})/2)};
        for(int j=0;j<7;j++)putFloat(100+j*4,bounds[j]);
        for(size_t i=0;i<result.size();i++)require(allowed.count(i)||result[i]==original_[i],"Unexpected native model change");
        (void)decodeModel(result);return result;
    }
};
inline std::string attachmentLabel(const std::string &name) {
    if(name=="node_window")return "Multiplier";
    if(name=="node_backpanel")return "Boost meter";
    if(name.rfind("nos",0)==0)return "Boost outlet "+name.substr(3);
    return name;
}
inline bool editableAttachment(const std::string &name){return name=="node_window"||name=="node_backpanel"||name.rfind("nos",0)==0;}
inline Matrix adjustedMatrix(const Matrix &base,const std::array<double,9> &values) {
    // Absolute position; world-axis rotation deltas and local scale relative to
    // the imported transform. This retains original shear/orientation at zero.
    Matrix out=base;
    double pitch=values[3]*3.141592653589793/180,yaw=values[4]*3.141592653589793/180,roll=values[5]*3.141592653589793/180;
    for(int col=0;col<3;col++){
        require(values[6+col]>0 && values[6+col]<=20,"Scale must be positive");
        double x=base[col*3]*values[6+col],y=base[col*3+1]*values[6+col],z=base[col*3+2]*values[6+col];
        double yy=std::cos(pitch)*y-std::sin(pitch)*z,zz=std::sin(pitch)*y+std::cos(pitch)*z;y=yy;z=zz;
        double xx=std::cos(yaw)*x+std::sin(yaw)*z;zz=-std::sin(yaw)*x+std::cos(yaw)*z;x=xx;z=zz;
        xx=std::cos(roll)*x-std::sin(roll)*y;yy=std::sin(roll)*x+std::cos(roll)*y;
        out[col*3]=F(xx);out[col*3+1]=F(yy);out[col*3+2]=F(z);
    }
    for(int i=0;i<3;i++)out[9+i]=F(values[i]);
    return out;
}
class ImportedMod {
    std::string packageId_=newPackageId();
    static void validPath(const std::string &s) {
        require(!s.empty() && s.front()!='/' && s.find('\\')==std::string::npos && s.find(':')==std::string::npos && s.find('\0')==std::string::npos,"Unsafe mod path");
        for(const auto &part:fs::u8path(s))require(part!=".." && part!=".","Unsafe mod path component");
    }
    static bool cache(const std::string &s){auto n=fs::u8path(s).filename().u8string();return n.rfind(".reriff-cache-",0)==0||n.rfind(".reriff-merged-",0)==0;}
public:
    fs::path source;
    std::map<std::string,Bytes> files;
    std::map<std::string,std::unique_ptr<NodeTransforms>> cars;
    explicit ImportedMod(const fs::path &path,const Archive *stock=nullptr):source(path.empty()?fs::path{}:fs::weakly_canonical(path)) {
        if(path.empty()){
            require(stock,"A game archive is required");auto selections=paintSelections(*stock,displayNames(*stock));for(const auto &paint:selections)if(!cars.count(paint.car))cars[paint.car]=std::make_unique<NodeTransforms>(assetBytes("graphics/vehicles/"+paint.car+"/nodes.bdae",*stock));return;
        }
        uint64_t total=0;std::set<std::string> seen;
        auto add=[&](std::string name,Bytes data){validPath(name);if(cache(name))return;require(seen.insert(lower(name)).second,"Case-colliding mod files");total+=data.size();require(total<=512ull*1024*1024 && files.size()<16382,"Mod exceeds import limits");files.emplace(std::move(name),std::move(data));};
        require(!isOutputLink(path),"Select a real mod folder or ZIP");
        if(fs::is_directory(path)) {
            require(fs::is_directory(path/"assets"),"Select the mod folder containing assets/");
            for(const auto &entry:fs::recursive_directory_iterator(path)){
                require(!isOutputLink(entry.path()),"Mod contains a symbolic link");
                if(entry.is_regular_file()){auto relative=entry.path().lexically_relative(path).generic_u8string();if(cache(relative))continue;require(entry.file_size()<=256ull*1024*1024,"Mod file exceeds size limit");add(relative,read(entry.path().u8string()));}
                else require(entry.is_directory(),"Unsupported mod file type");
            }
        } else {
            require(fs::file_size(path)<=512ull*1024*1024,"Mod ZIP exceeds size limit");Archive archive(path.u8string());std::set<std::string> roots;
            for(const auto &e:archive.entries){validPath(e.name);auto pos=e.name.find("assets/");if(pos==0 || (pos!=std::string::npos && e.name[pos-1]=='/'))roots.insert(e.name.substr(0,pos));}
            require(roots.size()==1,"ZIP must contain exactly one loose mod with assets/");auto root=*roots.begin();
            for(const auto &e:archive.entries){require(e.name.rfind(root,0)==0,"ZIP has files outside its mod folder");if(e.name.back()=='/')continue;auto name=e.name.substr(root.size());if(cache(name))continue;require(total+e.size<=512ull*1024*1024,"Mod exceeds import limits");add(name,archive.load(e));}
        }
        for(const auto &entry:files){
            auto name=lower(entry.first);const std::string prefix="assets/graphics/vehicles/",suffix="/nodes.bdae";
            if(name.rfind(prefix,0)!=0 || name.size()<=prefix.size()+suffix.size() || name.substr(name.size()-suffix.size())!=suffix)continue;
            auto id=name.substr(prefix.size(),name.size()-prefix.size()-suffix.size());require(id.find('/')==std::string::npos,"Invalid car path");
            if(seen.count(prefix+id+"/lod0_livery.bdae")){auto writer=std::make_unique<NodeTransforms>(entry.second);bool supported=false;for(const auto &node:writer->originalModel().nodes)supported|=editableAttachment(node.name);if(supported)cars[id]=std::move(writer);}
        }
        if(stock){
            std::set<std::string> candidates;
            for(const auto &file:files){auto name=lower(file.first);const std::string prefix="assets/graphics/vehicles/";
                if(name.rfind(prefix,0)==0){auto rest=name.substr(prefix.size());if(rest.rfind("decals/",0)==0&&rest.size()>=13)candidates.insert(rest.substr(7,3));else candidates.insert(rest.substr(0,rest.find('/')));}
                const std::string hud="assets/graphics/be/hud/car/";if(name.rfind(hud,0)==0){auto rest=name.substr(hud.size());candidates.insert(rest.substr(0,rest.find('/')));}
                if(name=="assets/scripts/config/unlocks_pc.cfg"){auto config=cfgValue(Json::parse(Config(file.second).json()));for(const auto&id:config.at("carNames"))if(id.is_string()&&!id.get<std::string>().empty())candidates.insert(id.get<std::string>());}
                if(name=="assets/scripts/config/paint_pc.cfg"){auto paints=cfgValue(Json::parse(Config(file.second).json())).at("Paints");for(auto car=paints.begin();car!=paints.end();++car)candidates.insert(car.key());}
            }
            for(const auto &id:candidates)if(!cars.count(id)&&id.size()==3){
                try{(void)decodeModel(assetBytes("graphics/vehicles/"+id+"/lod0_livery.bdae",*stock));cars[id]=std::make_unique<NodeTransforms>(assetBytes("graphics/vehicles/"+id+"/nodes.bdae",*stock));}catch(const std::exception&){/* Unused native paint entries may have no model. */}
            }
        }
        require(!cars.empty(),"No supported car paint, livery or model found in this mod");
    }
    Origin originMetadata() const {
        auto found=std::find_if(files.begin(),files.end(),[](const auto &entry){return lower(entry.first)=="origin.txt";});
        Origin result(found==files.end()?Bytes{}:found->second,packageId_);result.complete(files,false);return result;
    }
    std::vector<std::string> unresolvedOrigins() const {
        const auto declared=originMetadata().mappings();std::vector<std::string> unresolved;
        for(const auto &id:discoveredCarIds(files))if(authoredCarId(id)&&!declared.count(id))unresolved.push_back(id);
        return unresolved;
    }
    // Legacy runtime-slot or non-authored car IDs cannot be declared in origin.txt.
    std::vector<std::string> incompatibleOrigins() const {
        std::vector<std::string> incompatible;
        for(const auto &id:discoveredCarIds(files))if(!authoredCarId(id))incompatible.push_back(id);
        return incompatible;
    }
    void setOriginMetadata(const Origin &origin) {
        auto completed=origin;completed.complete(files,false);
        auto found=std::find_if(files.begin(),files.end(),[](const auto &entry){return lower(entry.first)=="origin.txt";});
        if(found==files.end())files["origin.txt"]=completed.bytes();else found->second=completed.bytes();
    }
    Bytes assetBytes(const std::string &name,const Archive &stock) const {for(const auto &f:files)if(lower(f.first)=="assets/"+lower(name))return f.second;for(const auto &entry:stock.entries)if(lower(entry.name)==lower(name))return stock.load(entry);return asset(stock,name);}
    void exportCopy(const fs::path &destination,const std::map<std::string,Bytes> &overrides={}) const {
        auto out=safePath(fs::absolute(destination));require(!fs::exists(out),"Choose a new folder; existing imports/exports are never overwritten");
        auto relative=out.lexically_relative(source);require(relative.empty()||*relative.begin()=="..","Export outside the imported folder");
        auto exported=files;
        for(const auto &file:overrides){validPath(file.first);require(file.first.rfind("assets/",0)==0,"Override must be a game asset");auto found=std::find_if(exported.begin(),exported.end(),[&](const auto &entry){return lower(entry.first)==lower(file.first);});if(found==exported.end())exported[file.first]=file.second;else found->second=file.second;}
        Json changed=Json::array();
        for(const auto &car:cars)if(car.second->changed()){auto name="assets/graphics/vehicles/"+car.first+"/nodes.bdae";bool found=false;for(auto &f:exported)if(lower(f.first)==name){f.second=car.second->bytes();found=true;}if(!found)exported[name]=car.second->bytes();changed.push_back(name);}
        normalizeAssetKeys(exported,true);
        // Preserve declared custom donors across re-export. Stock replacements
        // can derive their origin directly from native resource IDs.
        auto origin=std::find_if(exported.begin(),exported.end(),[](const auto &f){return lower(f.first)=="origin.txt";});
        Origin declaration(origin==exported.end()?Bytes{}:origin->second,packageId_);declaration.complete(exported);
        if(origin==exported.end())exported["origin.txt"]=declaration.bytes();else origin->second=declaration.bytes();
        // Refresh known editor ownership hashes rather than keeping stale claims.
        auto owner=exported.find("livery-maker.json");
        if(owner!=exported.end()){
            auto j=Json::parse(owner->second);require(j.value("tool","")=="reriff-livery-maker-v2","Unsupported ownership metadata");
            for(const auto &f:exported)if(lower(f.first)=="origin.txt")j["origin_sha256"]=sha(f.second);
            j["resources"]=Json::array();for(const auto &f:exported)if(f.first.rfind("assets/",0)==0)j["resources"].push_back({{"path",f.first.substr(7)},{"sha256",sha(f.second)}});
            auto s=j.dump(2)+"\n";owner->second=Bytes(s.begin(),s.end());
        }
        auto stage=out;stage+=".staging-"+std::to_string(processId());require(!fs::exists(stage) && !isOutputLink(stage),"Staging folder exists");
        require(fs::create_directory(stage),"Cannot create export staging folder");
        try{for(const auto &f:exported)write(stage/fs::u8path(f.first),f.second);fs::rename(stage,out);}catch(...){fs::remove_all(stage);throw;}
    }
};
// Imported cars use their donor's HUD atlas until the editor saves a style.
inline Bytes hudAtlasFor(const ImportedMod &mod,const Archive &stock,const std::string &car){
    try{return mod.assetBytes("graphics/be/hud/car/"+car+"/batchTextures.brres",stock);}catch(...){}
    std::string donor;
    try{auto mappings=mod.originMetadata().mappings();auto found=mappings.find(car);if(found!=mappings.end())donor=found->second;}catch(...){}
    if(donor.empty()){auto recipe=std::find_if(mod.files.begin(),mod.files.end(),[](const auto &entry){return lower(entry.first)=="model-import.json";});if(recipe!=mod.files.end())try{auto parsed=Json::parse(std::string(recipe->second.begin(),recipe->second.end()));donor=parsed.value("donor_id",std::string());}catch(...){}}
    require(donor.size()==3,"No HUD artwork found for "+car+" or its donor");
    return mod.assetBytes("graphics/be/hud/car/"+donor+"/batchTextures.brres",stock);
}
}
