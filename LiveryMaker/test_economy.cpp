#include "economy.hpp"
#include "mod_appearance.hpp"
#include <iostream>
using namespace livery;
int main(int argc,char**argv){try{
    require(argc==3,"Pass game folder and new output folder");Archive stock((fs::path(argv[1])/"vfs.zip").u8string());ImportedMod mod({},&stock);auto labels=displayNames(stock);ModAppearance paints(mod,stock,labels);UnlockConfig unlocks(mod,stock);
    require(unlocks.files().empty(),"Unedited unlocks exported");require(UnlockConfig::mode(unlocks.get("pl1"))=="artist","Police mode");require(UnlockConfig::mode(unlocks.get("f1r"))=="artist-title","Monaco mode");
    unlocks.set("alf","artist-title",1000,"Custom Artist","Custom Title");unlocks.set("pl1","shop",4321,"ignored","ignored");require(!unlocks.get("pl1").contains("artistUnlock"),"Conflicting music rule retained");
    unlocks.set("bui","artist",1,"Any Artist","");require(!unlocks.get("bui").contains("purchaseUnlock"),"Artist car has purchase price");unlocks.reset("bui");require(UnlockConfig::mode(unlocks.get("bui"))=="shop","Restore imported rule");
    auto first=paints.selection("alf",0),second=paints.selection("alf",1);auto color=paints.colors->get(second,"col0",3);paints.colors->setPrice(first,0);paints.colors->setPrice(second,1234);paints.colors->set(second,"col0",{12,34,56});paints.reset(second);require(paints.colors->price(second)==1234,"Appearance reset changed price");require(paints.colors->get(second,"col0",3)==color,"Appearance reset failed");
    auto unchanged=unlocks.files();bool rejected=false;try{unlocks.set("alf","artist-title",1,"","");}catch(const std::exception&){rejected=true;}require(rejected&&unlocks.files()==unchanged,"Invalid rule changed state");
    auto files=paints.files();for(auto&e:unlocks.files())files[e.first]=e.second;mod.exportCopy(argv[2],files);ImportedMod imported(argv[2],&stock);UnlockConfig again(imported,stock);ModAppearance againPaint(imported,stock,labels);
    require(again.get("alf").at("songUnlock")=="Custom Title"&&again.get("pl1").at("purchaseUnlock")==4321,"Unlock export/import");require(againPaint.colors->price(second)==1234&&againPaint.colors->price(first)==0,"Paint price export/import");require(again.get("f1r")==unlocks.get("f1r"),"Unrelated unlock changed");
    auto p=fs::path(argv[2]);auto original=mod.assetBytes("scripts/config/unlocks_pc.cfg",stock);write(p/"original-unlocks.cfg",original);write(p/"original-unlocks.luac",mod.assetBytes("scripts/config/unlocks_pc.luac",stock));write(p/"original-paint.luac",mod.assetBytes("scripts/config/paint_pc.luac",stock));
    std::cout<<"PASS prices, artist/combo/shop modes, reset, invalid input, export/reimport and unrelated data preservation\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
