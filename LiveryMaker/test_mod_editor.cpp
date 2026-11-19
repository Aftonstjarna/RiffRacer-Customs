#include "mod_editor.hpp"
#include <iostream>
using namespace livery;
template<class F> void refuses(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid operation accepted");}
int main(int argc,char **argv){try{
 require(argc==4,"Usage: test-mod-editor MOD STOCK_VFS NEW_TEST_DIRECTORY");
 ImportedMod mod(argv[1]);Archive stock(argv[2]);fs::path root(argv[3]);require(!fs::exists(root),"Test directory exists");fs::create_directories(root);
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
 mod.exportCopy(root/"adjusted");ImportedMod output(root/"adjusted");
 for(const auto &f:mod.files)if(f.first.find("/nodes.bdae")==std::string::npos)require(f.second==output.files.at(f.first),"Unrelated file differs");
 require(ImportedMod(argv[1]).cars.begin()->second->bytes()==original,"Source modified");
 refuses([&]{mod.exportCopy(root/"adjusted");});refuses([&]{mod.exportCopy(mod.source/"nested-export");});
 output.exportCopy(root/"roundtrip");require(ImportedMod(root/"roundtrip").files==output.files,"Reimport differs");
 std::map<std::string,Bytes> zipFiles;for(const auto &f:mod.files)zipFiles[lower(f.first)]=f.second;auto pair=archivePair(zipFiles);write(root/"mod.zip",pair.second);ImportedMod zipped(root/"mod.zip");require(zipped.files==zipFiles,"ZIP import differs");
 auto bad=archivePair(std::map<std::string,Bytes>{{"xx/assets/evil",{1}}});for(size_t i=0;i+3<bad.second.size();i++)if(bad.second[i]=='x'&&bad.second[i+1]=='x'&&bad.second[i+2]=='/'){bad.second[i]='.';bad.second[i+1]='.';}write(root/"bad.zip",bad.second);refuses([&]{ImportedMod unsafe(root/"bad.zip");});
 fs::create_directory_symlink(mod.source,root/"link");refuses([&]{ImportedMod unsafe(root/"link");});
 std::cout<<"PASS "<<checked<<" node transforms, native byte preservation, reset, folder/ZIP import, copy export, reimport and unsafe paths\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
