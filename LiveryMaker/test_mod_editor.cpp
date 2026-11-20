#include "mod_editor.hpp"
#include <iostream>
using namespace livery;
template<class F> void refuses(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid operation accepted");}
int main(int argc,char **argv){try{
 require(argc==4,"Usage: test-mod-editor MOD STOCK_VFS NEW_TEST_DIRECTORY");
 ImportedMod mod(argv[1]);Archive stock(argv[2]);fs::path root(argv[3]);require(!fs::exists(root),"Test directory exists");fs::create_directories(root);
 // Legacy packages may be inspected and edited before donors are assigned.
 std::map<std::string,Bytes> migrationAssets;const auto sample=mod.cars.begin()->first;
 for(const auto &id:{std::string("dod"),std::string("xyz")})for(const auto &name:{std::string("nodes.bdae"),std::string("lod0_livery.bdae")})migrationAssets["graphics/vehicles/"+id+"/"+name]=mod.assetBytes("graphics/vehicles/"+sample+"/"+name,stock);
 migrationAssets["graphics/be/hud/car/alf/legacy-test.bin"]={1};
 Origin fixtureOrigin;fixtureOrigin.setMapping("dod","alf");fixtureOrigin.setMapping("xyz","alf");publishMod(migrationAssets,Json::object(),root/"legacy-source",fixtureOrigin);fs::remove(root/"legacy-source/origin.txt");
 ImportedMod legacyFolder(root/"legacy-source");const auto untouchedLegacy=legacyFolder.files;
 auto migrationZip=archivePair(untouchedLegacy);write(root/"legacy.zip",migrationZip.second);const auto untouchedZip=read((root/"legacy.zip").u8string());
 auto migrate=[&](const fs::path &source,const std::string &label){
     ImportedMod legacy(source);auto metadata=legacy.originMetadata();const auto identity=metadata.packageId();
     require(legacy.unresolvedOrigins()==std::vector<std::string>({"dod","xyz"}),"Legacy custom donors were guessed or unavailable");
     require(metadata.mappings().at("alf")=="alf"&&!metadata.mappings().count("dod")&&!metadata.mappings().count("xyz"),"Legacy stock or custom origin differs");
     require(legacy.originMetadata().packageId()==identity,"Reading incomplete metadata changed identity");
     auto matrix=legacy.cars.at("dod")->get(1);matrix[10]+=.05;legacy.cars.at("dod")->set(1,matrix);
     refuses([&]{legacy.exportCopy(root/(label+"-unresolved"));});require(!fs::exists(root/(label+"-unresolved")),"Unresolved export created output");
     metadata.setMapping("dod","alf");legacy.setOriginMetadata(metadata);
     require(legacy.unresolvedOrigins()==std::vector<std::string>({"xyz"}),"Stepwise assignment did not retain unresolved car");
     require(legacy.originMetadata().packageId()==identity&&legacy.originMetadata().mappings().at("dod")=="alf","Partial donor assignment lost identity or mapping");
     refuses([&]{legacy.exportCopy(root/(label+"-partial"));});
     metadata=legacy.originMetadata();metadata.setMapping("xyz","alf");legacy.setOriginMetadata(metadata);require(legacy.unresolvedOrigins().empty(),"Completed assignments remain unresolved");
     for(bool overwrite:{false,true}){
         metadata=legacy.originMetadata();metadata.setOverwrite(overwrite);legacy.setOriginMetadata(metadata);auto destination=root/(label+(overwrite?"-replace":"-add"));legacy.exportCopy(destination);verifyOwnedMod(destination);
         ImportedMod migrated(destination);auto persisted=migrated.originMetadata();require(persisted.packageId()==identity&&persisted.overwrite()==overwrite&&persisted.mappings().at("dod")=="alf"&&persisted.mappings().at("xyz")=="alf"&&migrated.unresolvedOrigins().empty(),"Migrated metadata did not survive export/reimport");
         require(migrated.cars.at("dod")->bytes()==legacy.cars.at("dod")->bytes(),"Legacy model edit lost during migration");
         auto receipt=loadJson(destination/"livery-maker.json");require(receipt.at("origin_sha256")==sha(read((destination/"origin.txt").u8string())),"Migrated origin receipt is stale");
     }
 };
 migrate(root/"legacy-source","folder-migrated");migrate(root/"legacy.zip","zip-migrated");
 require(ImportedMod(root/"legacy-source").files==untouchedLegacy&&!fs::exists(root/"legacy-source/origin.txt")&&read((root/"legacy.zip").u8string())==untouchedZip,"Migration modified the legacy source");
 unsigned checked=0;
 auto check=[&](const Bytes &bytes){NodeTransforms writer(bytes);require(writer.bytes()==bytes,"No-op differs");for(size_t n=0;n<writer.originalModel().nodes.size();n++){
 auto m=writer.get(n);std::array<double,9> values{m[9],m[10],m[11],0,0,0,1,1,1};require(adjustedMatrix(m,values)==m,"Identity changes basis");
 auto invalid=m;invalid[0]=std::numeric_limits<float>::quiet_NaN();refuses([&]{writer.set(n,invalid);});
 invalid=m;invalid[0]=invalid[1]=invalid[2]=0;refuses([&]{writer.set(n,invalid);});
 values[0]+=.05;values[3]=5;values[4]=15;values[5]=-10;values[6]=1.1;auto adjusted=adjustedMatrix(m,values);writer.set(n,adjusted);auto output=writer.bytes();auto decoded=decodeModel(output);require(decoded.nodes.at(n).matrix==adjusted,"Transform round trip failed");
 auto start=u32(bytes,76)+n*156+32;for(size_t i=0;i<bytes.size();i++)require((i>=100&&i<128)||(i>=start&&i<start+48)||bytes[i]==output[i],"Unrelated byte changed");
 writer.reset(n);require(writer.bytes()==bytes,"Reset differs");++checked;
 }};
 for(auto &c:mod.cars)check(c.second->bytes());
 for(const auto &e:stock.entries)if(e.name.find("/vehicles/")!=std::string::npos && e.name.size()>10 && e.name.substr(e.name.size()-10)=="nodes.bdae")check(stock.load(e));
 auto &writer=*mod.cars.begin()->second;auto original=writer.bytes();auto matrix=writer.get(1);matrix[10]+=.05;writer.set(1,matrix);
 mod.files.erase("origin.txt");
 // Unknown authored IDs need an explicit donor; native IDs remain automatic.
 Origin seed;for(const auto &id:originCarIds(mod.files))if(!stockDonors().count(id))seed.setMapping(id,"alf");
 mod.setOriginMetadata(seed);mod.exportCopy(root/"generated-origin");mod.exportCopy(root/"generated-origin-again");
 auto generated=read((root/"generated-origin/origin.txt").u8string());require(generated==read((root/"generated-origin-again/origin.txt").u8string()),"Package identity changed within the editing session");
 require(Origin(generated).packageId()==seed.packageId(),"Generated package identity missing or changed by setter");
 ImportedMod generatedMod(root/"generated-origin");generatedMod.exportCopy(root/"generated-origin-roundtrip");require(generated==read((root/"generated-origin-roundtrip/origin.txt").u8string()),"Generated package identity changed on reimport");
 std::string declared="; keep my policy\n[mod]\nid=owner-custom-car\noverwrite=0\n\n[origins]\ndod=alf\n";mod.files["origin.txt"]=Bytes(declared.begin(),declared.end());
 auto explicitMetadata=mod.originMetadata();require(!explicitMetadata.overwrite()&&explicitMetadata.packageId()=="owner-custom-car"&&explicitMetadata.mappings().at("dod")=="alf","Imported policy, ID or donor differs");
 mod.setOriginMetadata(explicitMetadata);mod.exportCopy(root/"adjusted");require(read((root/"adjusted/origin.txt").u8string())==explicitMetadata.bytes(),"Donor metadata was not preserved");ImportedMod output(root/"adjusted");
 auto metadataOnly=output.originMetadata();metadataOnly.setOverwrite(true);output.setOriginMetadata(metadataOnly);output.exportCopy(root/"metadata-only");ImportedMod policy(root/"metadata-only");require(policy.originMetadata().overwrite()&&policy.originMetadata().packageId()=="owner-custom-car"&&policy.originMetadata().mappings()==explicitMetadata.mappings(),"Metadata-only export changed identity or donors");
 fs::rename(root/"metadata-only",root/"renamed-package");ImportedMod renamed(root/"renamed-package");renamed.exportCopy(root/"renamed-reexport");require(ImportedMod(root/"renamed-reexport").originMetadata().packageId()=="owner-custom-car","Renaming changed imported package ID");
 for(const auto &f:mod.files)if(f.first!="origin.txt"&&f.first.find("/nodes.bdae")==std::string::npos)require(f.second==output.files.at(f.first),"Unrelated file differs");
 require(ImportedMod(argv[1]).cars.begin()->second->bytes()==original,"Source modified");
 refuses([&]{mod.exportCopy(root/"adjusted");});refuses([&]{mod.exportCopy(mod.source/"nested-export");});
 output.exportCopy(root/"roundtrip");require(ImportedMod(root/"roundtrip").files==output.files,"Reimport differs");
 std::string legacy="[origins]\ndod=alf\n";mod.files["origin.txt"]=Bytes(legacy.begin(),legacy.end());mod.exportCopy(root/"legacy-origin");require(Origin(read((root/"legacy-origin/origin.txt").u8string())).mappings().at("dod")=="alf","Legacy donor metadata lost");
 std::map<std::string,Bytes> ownedAssets;for(const auto &f:mod.files)if(lower(f.first).rfind("assets/",0)==0)ownedAssets[lower(f.first).substr(7)]=f.second;
 auto ownedOrigin=mod.originMetadata();publishMod(ownedAssets,Json::object(),root/"owned",ownedOrigin);ImportedMod owned(root/"owned");auto ownedPolicy=owned.originMetadata();ownedPolicy.setOverwrite(false);owned.setOriginMetadata(ownedPolicy);owned.exportCopy(root/"owned-policy");verifyOwnedMod(root/"owned-policy");
 auto ownedReceipt=loadJson(root/"owned-policy/livery-maker.json");require(ownedReceipt.at("origin_sha256")==sha(read((root/"owned-policy/origin.txt").u8string())),"Imported metadata edit left stale ownership");
 std::map<std::string,Bytes> zipFiles;for(const auto &f:mod.files)zipFiles[lower(f.first)]=f.second;auto pair=archivePair(zipFiles);write(root/"mod.zip",pair.second);ImportedMod zipped(root/"mod.zip");require(zipped.files==zipFiles,"ZIP import differs");
 auto bad=archivePair(std::map<std::string,Bytes>{{"xx/assets/evil",{1}}});for(size_t i=0;i+3<bad.second.size();i++)if(bad.second[i]=='x'&&bad.second[i+1]=='x'&&bad.second[i+2]=='/'){bad.second[i]='.';bad.second[i+1]='.';}write(root/"bad.zip",bad.second);refuses([&]{ImportedMod unsafe(root/"bad.zip");});
 fs::create_directory_symlink(mod.source,root/"link");refuses([&]{ImportedMod unsafe(root/"link");});
 std::cout<<"PASS "<<checked<<" node transforms, native byte preservation, reset, folder/ZIP import, copy export, reimport and unsafe paths\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
