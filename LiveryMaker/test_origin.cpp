#include "core.hpp"
#include <iostream>
using namespace livery;
template<class F> void refuses(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid metadata operation accepted");}
Bytes bytes(const std::string &s){return Bytes(s.begin(),s.end());}
int main(){fs::path root;try{
 for(unsigned i=0;i<100;i++){auto candidate=fs::temp_directory_path()/newPackageId();if(fs::create_directory(candidate)){root=candidate;break;}}
 require(!root.empty(),"Cannot create origin test directory");
 const std::map<std::string,Bytes> files{{"graphics/vehicles/alf/nodes.bdae",{1}},{"graphics/vehicles/decals/bui_01/batchtextures.brres",{2}},{"graphics/be/hud/car/cor/textures.bin",{3}}};
 auto origins=originText(files,"customs-test");std::string expected="[mod]\nid=customs-test\noverwrite=1\n\n[origins]\nalf=alf\nbui=bui\ncor=cor\n";
 require(origins==bytes(expected),"Policy, package ID or native donor mapping differs");
 auto out=root/"mod";publishMod(files,Json::object(),out);auto generated=read((out/"origin.txt").u8string());Origin first(generated);
 require(first.packageId().rfind("customs-",0)==0&&first.overwrite(),"New export lacks replacement policy or ID");
 auto changed=files;changed.begin()->second={5};publishMod(changed,Json::object(),out);require(read((out/"origin.txt").u8string())==generated,"Asset update changed package identity");
 fs::rename(out,root/"renamed");out=root/"renamed";publishMod(files,Json::object(),out);require(read((out/"origin.txt").u8string())==generated,"Folder rename changed package identity");
 publishMod(files,Json::object(),root/"separate");require(Origin(read((root/"separate/origin.txt").u8string())).packageId()!=first.packageId(),"Separate packages share identity");
 first.setOverwrite(false);auto receipt=publishMod(files,Json::object(),out,first);verifyOwnedMod(out);auto add=read((out/"origin.txt").u8string());
 require(!Origin(add).overwrite()&&Origin(add).packageId()==first.packageId(),"Add mode changed identity or policy");require(receipt.at("origin_sha256")==sha(add),"Edited origin receipt is stale");
 first.setOverwrite(true);publishMod(files,Json::object(),out,&first.bytes());verifyOwnedMod(out);require(Origin(read((out/"origin.txt").u8string())).overwrite(),"Replace mode not restored");
 std::string declaration="; custom policy\r\n[mod]\r\nid=owners-car\r\noverwrite=0\r\n\r\n[origins]\r\nalf=bui\r\nbui=bui\r\ncor=cor\r\n";
 Origin custom(bytes(declaration));custom.complete(files);require(custom.bytes()==bytes(declaration),"Complete declaration changed");custom.setOverwrite(true);
 require(custom.bytes()==bytes(std::string(declaration).replace(declaration.find("overwrite=0"),11,"overwrite=1")),"Policy edit damaged comments or mappings");
 publishMod(files,Json::object(),out,custom);require(Origin(read((out/"origin.txt").u8string())).mappings().at("alf")=="bui","Explicit stock donor replaced");
 std::string fallbackText="[mod]\nid=fallback-car\noverwrite=0\n\n[origins]\ndod=alf\n\n[fallbacks]\ndod=fab\n";
 Origin fallback(bytes(fallbackText));require(fallback.mappings().at("dod")=="alf"&&fallback.fallbacks().at("dod")=="fab","Independent fallback or donor differs");
 fallback.eraseFallback("dod");require(fallback.fallbacks().empty()&&Origin(fallback.bytes()).mappings().at("dod")=="alf","Fallback removal changed donor");
 fallback.setFallback("dod","cor");require(Origin(fallback.bytes()).fallbacks().at("dod")=="cor","Fallback setter lost value");
 refuses([&]{Origin invalid(bytes("[origins]\ndod=alf\n[fallbacks]\ndod=bogus\n"));});
 refuses([&]{Origin invalid(bytes("[origins]\ndod=alf\n[fallbacks]\nr00=fab\n"));});
 refuses([&]{fallback.setFallback("dod","bogus");});
 refuses([&]{fallback.setFallback("xyz","fab");});
 Origin orphan(bytes("[mod]\nid=orphan\noverwrite=0\n[origins]\ndod=alf\n[fallbacks]\nabc=fab\n"));refuses([&]{orphan.complete({{"graphics/vehicles/dod/nodes.bdae",{}}});});
 auto mixed=files;mixed["graphics/vehicles/decals/alf_01/batchTextures.brres"]={9};publishMod(mixed,Json::object(),root/"mixed-case");
 auto mixedDir=root/"mixed-case/assets/graphics/vehicles/decals/alf_01";bool lowerName=false,mixedName=false;for(const auto &entry:fs::directory_iterator(mixedDir)){auto name=entry.path().filename().u8string();lowerName=lowerName||name=="batchtextures.brres";mixedName=mixedName||name=="batchTextures.brres";}require(lowerName&&!mixedName,"Exported asset name was not normalized");
 Origin persistent(originText(files,"customs-fallback"));persistent.setFallback("alf","fab");publishMod(files,Json::object(),root/"fallback",persistent);
 auto storedFallback=Origin(read((root/"fallback/origin.txt").u8string()));require(storedFallback.fallbacks().at("alf")=="fab"&&storedFallback.mappings().at("alf")=="alf","Exported fallback did not survive");
 write(out/"origin.txt",bytes("external change"));refuses([&]{publishMod(files,Json::object(),out,custom);});require(read((out/"origin.txt").u8string())==bytes("external change"),"External metadata overwritten");
 auto other=files;other["graphics/vehicles/xyz/nodes.bdae"]={4};refuses([&]{originText(other);});Origin mapped;mapped.setMapping("xyz","alf");mapped.complete(other);require(mapped.mappings().at("xyz")=="alf","Custom donor lost");
 Origin incomplete({},"legacy-needs-donor");incomplete.complete(other,false);
 require(incomplete.packageId()=="legacy-needs-donor"&&incomplete.overwrite()&&incomplete.mappings().size()==3&&!incomplete.mappings().count("xyz"),"Incomplete completion guessed a custom donor or missed native IDs");
 auto incompleteBytes=incomplete.bytes();incomplete.complete(other,false);require(incomplete.bytes()==incompleteBytes,"Incomplete metadata completion changed identity or bytes");
 refuses([&]{incomplete.complete(other);});refuses([&]{publishMod(other,Json::object(),root/"incomplete",incomplete);});require(!fs::exists(root/"incomplete"),"Unresolved export created a package");
 incomplete.setMapping("xyz","alf");incomplete.complete(other);require(incomplete.packageId()=="legacy-needs-donor"&&incomplete.mappings().at("xyz")=="alf","Donor assignment changed package identity");
 for(const auto &bad:{std::string("[mod]\noverwrite=2\n"),std::string("[origins]\nr00=alf\n"),std::string("[origins]\nxyz=xyz\n"),std::string("[mod]\nid=x\nid=y\n")})refuses([&]{Origin invalid(bytes(bad));});
 Origin legacy(bytes("; keep\n[origins]\nxyz=alf\n"),"stable-legacy");legacy.complete(other);require(legacy.packageId()=="stable-legacy"&&legacy.overwrite()&&legacy.mappings().at("xyz")=="alf","Legacy completion lost donor or default policy");
 // Legacy packages using a runtime slot (or any non-authored spelling) can be
 // opened and migrated, but such an ID cannot be declared in origin.txt.
 const std::map<std::string,Bytes> runtimeSlot{{"graphics/vehicles/r00/nodes.bdae",{1}},{"graphics/vehicles/alf/nodes.bdae",{2}}};
 require(discoveredCarIds(runtimeSlot).size()==2,"Legacy car discovery missed a resource ID");
 refuses([&]{originCarIds(runtimeSlot);});refuses([&]{originText(runtimeSlot);});
 Origin tolerant;tolerant.complete(runtimeSlot,false);require(tolerant.mappings().size()==1&&tolerant.mappings().at("alf")=="alf","Tolerant completion guessed a runtime slot donor");
 refuses([&]{tolerant.complete(runtimeSlot);});require(tolerant.mappings().size()==1,"Refused strict completion changed mappings");
 Origin header(bytes("[mod]"),"header-test");header.complete({});require(Origin(header.bytes()).packageId()=="header-test","Unterminated section completion failed");
 Origin spaced(bytes("id=My Car identity\noverwrite=0\n[origins]\nalf=bui\n"));spaced.complete({});require(spaced.packageId()=="My Car identity"&&!spaced.overwrite(),"Implicit mod section or safe identity differs from runtime");
 Origin unusual(bytes("[origins]\nid=identity elsewhere\noverwrite=0\nalf=alf\n"));unusual.setOverwrite(true);unusual.complete(files);require(unusual.overwrite()&&unusual.packageId()=="identity elsewhere","Global identity/policy handling differs from runtime");
 for(const auto &bad:{std::string("[mod]\nid=.\n"),std::string("[mod]\nid=\n"),std::string("[origins]\nalf=ALF\n"),std::string("id=x\n[origins]\nid=y\n"),std::string(65537,';')})refuses([&]{Origin invalid(bytes(bad));});
 Origin uppercase(bytes("[Origins]\nalf=bui\n"));uppercase.complete({{"graphics/vehicles/alf/nodes.bdae",{1}}});require(uppercase.mappings().at("alf")=="alf","Case-sensitive origins parsed differently from runtime");
 Origin explicitId({},"explicit-uncompleted");explicitId.setOverwrite(false);explicitId.setMapping("alf","bui");publishMod(files,Json::object(),root/"explicit-id",explicitId);require(Origin(read((root/"explicit-id/origin.txt").u8string())).packageId()=="explicit-uncompleted","Origin object overload lost generated identity");
 Origin cfgOnly;cfgOnly.complete({{"scripts/config/paint_pc.cfg",{1}}});require(cfgOnly.mappings().empty(),"Shared configuration inferred unedited cars");
 require(!fs::exists(root/"separate/vfs.dat")&&!fs::exists(root/"separate/vfs.zip")&&fs::is_directory(root/"separate/assets"),"Export is not loose assets");
 fs::remove_all(root);std::cout<<"PASS origin policy editing, stable identity, explicit donors, legacy completion, receipt protection and loose export\n";return 0;
}catch(const std::exception &e){if(!root.empty())fs::remove_all(root);std::cerr<<e.what()<<'\n';return 1;}}
