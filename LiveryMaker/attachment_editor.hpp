#pragma once
#include "mod_appearance.hpp"
#include "economy.hpp"
#include "mod_names.hpp"
#include "error_dialog.hpp"
#include "car_template.hpp"
#include "surface_snap.hpp"
#include "multiplier_style.hpp"
#include "car_preview.hpp"
#include "model_import_ui.hpp"
#include "update_ui.hpp"
#include <gtk/gtk.h>
namespace livery {
class AttachmentEditor {
    const Archive &stock_;ImportedMod mod_;std::map<std::string,std::string> labels_;
    GtkWidget *dialog_{},*car_{},*node_{},*paint_{},*canvas_{},*status_{};
    std::array<GtkWidget*,9> fields_{};bool updating_=false,unsaved_=false;
    std::string currentCar_;size_t selected_=0,paintIndex_=0;
    GtkWidget *exportMode_{},*exportButton_{},*migrationInfo_{};
    std::unique_ptr<update::UpdatePanel> updates_{};
    std::map<std::string,GtkWidget*> migrationDonors_,fallbackCars_;
    bool legacyOrigin_=false;
    void refreshMigration(){
        auto missing=mod_.unresolvedOrigins();auto incompatible=mod_.incompatibleOrigins();
        if(exportButton_)gtk_widget_set_sensitive(exportButton_,missing.empty()&&incompatible.empty());
        if(!migrationInfo_)return;
        std::string text;
        if(!incompatible.empty())text="Car IDs "+joinIds(incompatible)+" use a runtime slot or a non-authored spelling. Rename those car folders to three lowercase letters or digits, then import the renamed copy to migrate them.";
        else if(!missing.empty())text="Choose a stock donor for each legacy car before exporting. Its resource ID alone does not identify the donor.";
        else text="Donor metadata is ready. Export a copy to save the migration; the imported mod stays unchanged.";
        gtk_label_set_text(GTK_LABEL(migrationInfo_),text.c_str());
    }
    static std::string joinIds(const std::vector<std::string> &ids){std::string text;for(size_t i=0;i<ids.size();i++)text+=(i?", ":"")+ids[i];return text;}
    void buildMigration(GtkWidget *controls){
        auto missing=mod_.unresolvedOrigins();auto incompatible=mod_.incompatibleOrigins();
        if(!legacyOrigin_&&missing.empty()&&incompatible.empty())return;
        auto *expander=gtk_expander_new("Legacy car migration");gtk_expander_set_expanded(GTK_EXPANDER(expander),true);gtk_box_pack_start(GTK_BOX(controls),expander,false,false,0);
        auto *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_container_add(GTK_CONTAINER(expander),box);
        migrationInfo_=gtk_label_new("");gtk_label_set_line_wrap(GTK_LABEL(migrationInfo_),true);gtk_label_set_max_width_chars(GTK_LABEL(migrationInfo_),32);gtk_box_pack_start(GTK_BOX(box),migrationInfo_,false,false,0);
        auto stockLabels=displayNames(stock_);
        for(const auto &id:missing){
            auto title=localized(labels_,"car_"+id)+" ("+id+") — donor";auto *label=gtk_label_new(title.c_str());gtk_label_set_line_wrap(GTK_LABEL(label),true);gtk_label_set_max_width_chars(GTK_LABEL(label),32);gtk_box_pack_start(GTK_BOX(box),label,false,false,0);
            auto *combo=gtk_combo_box_text_new();gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),"","Choose stock donor…");for(const auto &donor:stockDonors()){auto name=localized(stockLabels,"car_"+donor);gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),donor.c_str(),name.c_str());}
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);gtk_box_pack_start(GTK_BOX(box),combo,false,false,0);migrationDonors_[id]=combo;
            g_signal_connect(combo,"changed",G_CALLBACK(+[](GtkComboBox *changed,gpointer data){auto &a=*static_cast<AttachmentEditor*>(data);a.guard([&]{const char *selected=gtk_combo_box_get_active_id(changed);auto origin=a.mod_.originMetadata();if(!selected||!*selected){auto mappings=origin.mappings();for(const auto &item:a.migrationDonors_)if(item.second==GTK_WIDGET(changed)&&mappings.count(item.first))gtk_combo_box_set_active_id(changed,mappings.at(item.first).c_str());return;}for(const auto &item:a.migrationDonors_)if(item.second==GTK_WIDGET(changed))origin.setMapping(item.first,selected);a.mod_.setOriginMetadata(origin);a.unsaved_=true;a.refreshMigration();});}),this);
        }
        refreshMigration();
    }
    void markEdited(){
        if(!currentCar_.empty()){
            auto origin=mod_.originMetadata();
            if(!origin.mappings().count(currentCar_)&&stockDonors().count(currentCar_))origin.setMapping(currentCar_,currentCar_);
            mod_.setOriginMetadata(origin);
        }
        unsaved_=true;
    }
    void buildExportMode(GtkWidget *controls){
        exportMode_=gtk_combo_box_text_new();gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(exportMode_),"replace","Replace donor car");gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(exportMode_),"add","Add as a new car");
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(exportMode_),mod_.originMetadata().overwrite()?"replace":"add");gtk_box_pack_end(GTK_BOX(controls),exportMode_,false,false,0);gtk_widget_set_tooltip_text(exportMode_,"Applies to the whole package. Adding cars needs ReRiff car slots.");
        g_signal_connect(exportMode_,"changed",G_CALLBACK(+[](GtkComboBox *combo,gpointer data){auto &a=*static_cast<AttachmentEditor*>(data);a.guard([&]{auto origin=a.mod_.originMetadata();origin.setOverwrite(std::string(gtk_combo_box_get_active_id(combo))=="replace");a.mod_.setOriginMetadata(origin);a.unsaved_=true;});}),this);
    }

    void buildFallback(GtkWidget *controls){
        auto origin=mod_.originMetadata();auto mappings=origin.mappings();if(mappings.empty())return;
        auto *expander=gtk_expander_new("Missing-car fallback");gtk_box_pack_start(GTK_BOX(controls),expander,false,false,0);
        auto *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_container_add(GTK_CONTAINER(expander),box);
        auto *note=gtk_label_new("Stock car shown if this custom car is unavailable. Requires ReRiff 1.4.1-beta.7 or newer; the donor is used when unset.");gtk_label_set_line_wrap(GTK_LABEL(note),true);gtk_label_set_max_width_chars(GTK_LABEL(note),32);gtk_box_pack_start(GTK_BOX(box),note,false,false,0);
        auto stockLabels=displayNames(stock_);auto declared=origin.fallbacks();
        for(const auto &mapping:mappings){
            auto title=localized(labels_,"car_"+mapping.first)+" ("+mapping.first+") — fallback";auto *label=gtk_label_new(title.c_str());gtk_label_set_line_wrap(GTK_LABEL(label),true);gtk_label_set_max_width_chars(GTK_LABEL(label),32);gtk_box_pack_start(GTK_BOX(box),label,false,false,0);
            auto *combo=gtk_combo_box_text_new();gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),"","Same as donor");for(const auto &donor:stockDonors()){auto name=localized(stockLabels,"car_"+donor);gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),donor.c_str(),name.c_str());}
            if(declared.count(mapping.first))gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo),declared.at(mapping.first).c_str());else gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);
            gtk_box_pack_start(GTK_BOX(box),combo,false,false,0);fallbackCars_[mapping.first]=combo;
            g_signal_connect(combo,"changed",G_CALLBACK(+[](GtkComboBox *changed,gpointer data){auto &a=*static_cast<AttachmentEditor*>(data);a.guard([&]{std::string car;for(const auto &item:a.fallbackCars_)if(item.second==GTK_WIDGET(changed))car=item.first;if(car.empty())return;const char *selected=gtk_combo_box_get_active_id(changed);auto origin=a.mod_.originMetadata();if(!selected||!*selected)origin.eraseFallback(car);else origin.setFallback(car,selected);a.mod_.setOriginMetadata(origin);a.unsaved_=true;});}),this);
        }
    }

    std::unique_ptr<Model> body_;PaintConfig *colors_{};std::unique_ptr<ModAppearance> appearance_;std::unique_ptr<Image> decalPreview_;Model nodes_;
    std::map<std::string,std::array<double,9>> values_;
    double yaw_=3.14,pitch_=.45,zoom_=1.;
    std::vector<std::pair<std::string,GtkWidget*>> paintColors_;bool loadingPaint_=false;GtkWidget *liveryInfo_{};
    std::unique_ptr<UnlockConfig> unlocks_;
    GtkWidget *unlockMode_{},*carPrice_{},*packPrice_{},*unlockArtist_{},*unlockTitle_{},*unlockInfo_{},*unlockApply_{};
    void unlockSensitivity(){
        auto id=gtk_combo_box_get_active_id(GTK_COMBO_BOX(unlockMode_));std::string mode=id?id:"original";bool supported=unlocks_->supports(currentCar_);
        gtk_widget_set_sensitive(carPrice_,supported&&mode=="shop");gtk_widget_set_sensitive(unlockArtist_,supported&&(mode=="artist"||mode=="artist-title"));gtk_widget_set_sensitive(unlockTitle_,supported&&mode=="artist-title");
        gtk_label_set_text(GTK_LABEL(unlockInfo_),mode=="artist"||mode=="artist-title"?"Music unlocks match artist/title text, ignoring case. Custom rules require the updated ReRiff client. Existing ownership is unchanged.":"Prices apply to future purchases. 0 makes a paint free. Paint and livery share one price; ownership already saved is unchanged.");
    }
    void refreshUnlock(){
        if(!unlockMode_||currentCar_.empty())return;
        unlocks_->ensure(currentCar_);bool supported=unlocks_->supports(currentCar_);auto v=supported?unlocks_->get(currentCar_):Json::object();
        gtk_widget_set_sensitive(unlockMode_,supported);gtk_combo_box_set_active_id(GTK_COMBO_BOX(unlockMode_),UnlockConfig::mode(v).c_str());gtk_spin_button_set_value(GTK_SPIN_BUTTON(carPrice_),v.value("purchaseUnlock",1000));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(packPrice_),colors_->price(paintSelection()));gtk_entry_set_text(GTK_ENTRY(unlockArtist_),v.value("artistUnlock",std::string()).c_str());gtk_entry_set_text(GTK_ENTRY(unlockTitle_),v.value("songUnlock",std::string()).c_str());unlockSensitivity();
    }
    void applyUnlock(){
        auto id=gtk_combo_box_get_active_id(GTK_COMBO_BOX(unlockMode_));std::string mode=id?id:"original";unlocks_->ensure(currentCar_);
        if(unlocks_->supports(currentCar_)&&mode!="original")unlocks_->set(currentCar_,mode,gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(carPrice_)),gtk_entry_get_text(GTK_ENTRY(unlockArtist_)),gtk_entry_get_text(GTK_ENTRY(unlockTitle_)));
        colors_->setPrice(paintSelection(),gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(packPrice_)));markEdited();refreshUnlock();message("Unlock and prices updated. Export a copy to save; restart the game after installing it.");
    }
    void buildUnlock(GtkWidget *controls){
        auto*expand=gtk_expander_new("Unlock and prices");gtk_box_pack_start(GTK_BOX(controls),expand,false,false,0);auto*box=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_container_add(GTK_CONTAINER(expand),box);
        auto label=[&](const char*s){auto*w=gtk_label_new(s);gtk_label_set_xalign(GTK_LABEL(w),0);gtk_box_pack_start(GTK_BOX(box),w,false,false,0);};
        label("Car unlock");unlockMode_=gtk_combo_box_text_new();for(auto choice:std::vector<std::pair<const char*,const char*>>{{"original","Keep existing unlock"},{"available","Available from start"},{"play","Play a track"},{"shop","Shop purchase"},{"artist","Artist"},{"artist-title","Artist – Title"}})gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(unlockMode_),choice.first,choice.second);gtk_box_pack_start(GTK_BOX(box),unlockMode_,false,false,0);
        label("Car price (shop purchases only)");carPrice_=gtk_spin_button_new_with_range(1,10000000,100);gtk_box_pack_start(GTK_BOX(box),carPrice_,false,false,0);
        label("Artist contains");unlockArtist_=gtk_entry_new();gtk_entry_set_max_length(GTK_ENTRY(unlockArtist_),200);gtk_box_pack_start(GTK_BOX(box),unlockArtist_,false,false,0);
        label("Title contains");unlockTitle_=gtk_entry_new();gtk_entry_set_max_length(GTK_ENTRY(unlockTitle_),200);gtk_box_pack_start(GTK_BOX(box),unlockTitle_,false,false,0);
        label("Selected paint / livery price");packPrice_=gtk_spin_button_new_with_range(0,10000000,100);gtk_box_pack_start(GTK_BOX(box),packPrice_,false,false,0);
        unlockInfo_=gtk_label_new("");gtk_label_set_line_wrap(GTK_LABEL(unlockInfo_),true);gtk_label_set_max_width_chars(GTK_LABEL(unlockInfo_),32);gtk_box_pack_start(GTK_BOX(box),unlockInfo_,false,false,0);
        unlockApply_=gtk_button_new_with_label("Apply unlock and prices");gtk_box_pack_start(GTK_BOX(box),unlockApply_,false,false,0);g_signal_connect(unlockApply_,"clicked",G_CALLBACK(+[](GtkButton*,gpointer data){auto&a=*static_cast<AttachmentEditor*>(data);a.guard([&]{a.applyUnlock();});}),this);
        auto*reset=gtk_button_new_with_label("Restore imported unlock");gtk_box_pack_start(GTK_BOX(box),reset,false,false,0);g_signal_connect(reset,"clicked",G_CALLBACK(+[](GtkButton*,gpointer data){auto&a=*static_cast<AttachmentEditor*>(data);a.guard([&]{a.unlocks_->ensure(a.currentCar_);if(a.unlocks_->supports(a.currentCar_)){a.unlocks_->reset(a.currentCar_);a.markEdited();a.refreshUnlock();}});}),this);
        g_signal_connect(unlockMode_,"changed",G_CALLBACK(+[](GtkComboBox*,gpointer data){static_cast<AttachmentEditor*>(data)->unlockSensitivity();}),this);
    }

    PaintSelection paintSelection()const{return appearance_->selection(currentCar_,paintIndex_);}
    void refreshPaint(){
        if(currentCar_.empty())return;
        auto p=paintSelection();loadNames();refreshUnlock();loadingPaint_=true;
        for(auto &field:paintColors_){auto rgb=colors_->get(p,field.first,3);gtk_widget_set_sensitive(field.second,rgb.size()==3);if(rgb.size()==3){GdkRGBA color{rgb[0]/255.,rgb[1]/255.,rgb[2]/255.,1};gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(field.second),&color);}}
        loadingPaint_=false;auto decal=appearance_->decal(p);decalPreview_.reset();
        if(decal)decalPreview_=std::make_unique<Image>(decodeDDS(decal->texture));
        auto source=appearance_->imageTemplate(p);auto text=std::to_string(source.texture.width)+" × "+std::to_string(source.texture.height)+" PNG • "+(decal?"Livery present":"No livery")+"\nPNG import affects only this paint.";
        if(addDecalButton_)gtk_widget_set_sensitive(addDecalButton_,true);
        gtk_label_set_text(GTK_LABEL(liveryInfo_),text.c_str());gtk_widget_queue_draw(canvas_);
    }
    static std::string paintKey(const PaintSelection &p){return p.car+"/"+std::to_string(p.packIndex);}
    static std::string exportSlug(const std::string &name,const std::string &fallback){
        std::string base;for(unsigned char c:name){if(std::isalnum(c))base.push_back(char(std::tolower(c)));else if(!base.empty()&&base.back()!='-')base.push_back('-');}
        while(!base.empty()&&base.back()=='-')base.pop_back();
        return base.empty()?fallback:base;
    }
    std::string exportBaseName(const PaintSelection &p) const {return exportSlug(localized(labels_,"car_"+p.car),p.car);}
    std::string exportFileName(const PaintSelection &p,const std::string &kind) const {
        auto base=exportBaseName(p);if(kind=="livery"&&!p.paintName.empty()){auto paint=exportSlug(p.paintName,"");if(!paint.empty())base+="-"+paint;}return base+"-"+kind+".png";
    }
    std::string chooseArtwork(bool save,const std::string &suggested={}){
        auto *d=gtk_file_chooser_dialog_new(save?"Save livery PNG":"Import PNG livery",GTK_WINDOW(dialog_),save?GTK_FILE_CHOOSER_ACTION_SAVE:GTK_FILE_CHOOSER_ACTION_OPEN,"Cancel",GTK_RESPONSE_CANCEL,save?"Save":"Import",GTK_RESPONSE_ACCEPT,nullptr);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),true);if(save)gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d),suggested.empty()?"livery.png":suggested.c_str());
        std::string result;if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){auto *path=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));result=path;g_free(path);}gtk_widget_destroy(d);return result;
    }
    void artwork(bool save){
        auto p=paintSelection();auto source=appearance_->imageTemplate(p);std::string suggested;
        if(save){auto found=exportKinds_.find(paintKey(p));suggested=exportFileName(p,found!=exportKinds_.end()?found->second:(appearance_->decal(p)?"livery":"template"));}
        auto path=chooseArtwork(save,suggested);if(path.empty())return;
        if(save){auto image=decodeDDS(source.texture);if(!appearance_->decal(p))std::fill(image.pixels.begin(),image.pixels.end(),0);savePNG(fs::u8path(path),image);}
        else {appearance_->importPNG(p,loadPNG(fs::u8path(path),source.texture.width,source.texture.height));markEdited();refreshPaint();message("Livery updated for this paint. Export a copy to save.");}
    }
    void paintAction(int action){
        if(action<2){if(action==0)exportKinds_.erase(paintKey(paintSelection()));artwork(action==1);return;}
        if(action==7){saveUV();return;}
        if(action==8){exportPaintMod();return;}
        auto p=paintSelection();
        if(action==5){auto path=choosePNG("Export livery template",true,exportFileName(p,"template").c_str());if(path.empty())return;exportLiveryTemplate(fs::u8path(path));message("Exported livery template to "+path+".");return;}
        if(action==6){auto source=appearance_->imageTemplate(p);exportKinds_[paintKey(p)]="test";appearance_->importPNG(p,diagnostic(source.texture));markEdited();refreshPaint();message("Test livery applied. Save PNG suggests "+exportFileName(p,"test")+".");return;}
        if(action==2||action==3)exportKinds_.erase(paintKey(p));
        if(action==2)appearance_->remove(p);else if(action==3)appearance_->reset(p);else appearance_->makeDefault(p);
        markEdited();refreshPaint();if(action==4)message("Default appearance updated in the first paint slot. Existing saved paint selections are unchanged.");
    }
    GtkWidget *previewHint_{},*carName_{},*paintName_{},*notebook_{},*attachmentControls_{},*paned_{};bool placement_=false;int hudPage_=2;
    std::map<std::string,std::string> nameEdits_;std::map<std::string,std::string> exportKinds_;
    std::string paintToken()const {return lower(cfgValue(Json::parse(Config(colors_->bytes()).json())).at("Paints").at(currentCar_).at(paintIndex_).at("packName").get<std::string>());}
    void loadNames(){gtk_entry_set_text(GTK_ENTRY(carName_),localized(labels_,"car_"+currentCar_).c_str());gtk_entry_set_text(GTK_ENTRY(paintName_),localized(labels_,paintToken()).c_str());}
    void applyNames(){
        auto proposed=nameEdits_;auto carKey="car_"+currentCar_;try{auto mappings=mod_.originMetadata().mappings();auto found=mappings.find(currentCar_);if(found!=mappings.end())carKey="car_"+found->second;}catch(...){}
        proposed[carKey]=gtk_entry_get_text(GTK_ENTRY(carName_));proposed[paintToken()]=gtk_entry_get_text(GTK_ENTRY(paintName_));
        (void)renamedStrings([&](const std::string &path){return mod_.assetBytes(path,stock_);},proposed,allLocaleNames_,false);
        nameEdits_=std::move(proposed);for(const auto &entry:nameEdits_)labels_[entry.first]=entry.second;labels_["car_"+currentCar_]=gtk_entry_get_text(GTK_ENTRY(carName_));
        for(auto *combo:{car_,paint_}){GtkTreeIter iter;auto *model=gtk_combo_box_get_model(GTK_COMBO_BOX(combo));if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo),&iter))gtk_list_store_set(GTK_LIST_STORE(model),&iter,0,combo==car_?localized(labels_,"car_"+currentCar_).c_str():localized(labels_,paintToken()).c_str(),-1);}
        markEdited();message(allLocaleNames_?"In-game names updated for all languages. Export a copy to save. Shared name tokens change together.":"In-game names updated for US and GB. Export a copy to save. Shared name tokens change together.");
    }
    std::map<std::string,Bytes> extraFiles()const {auto files=appearance_->files();bool carPackage=false;try{carPackage=!mod_.originMetadata().mappings().empty();}catch(...){}for(auto &entry:unlocks_->files(!carPackage))files[entry.first]=entry.second;for(auto &entry:renamedStrings([&](const std::string &path){return mod_.assetBytes(path,stock_);},nameEdits_,allLocaleNames_,false))files[entry.first]=std::move(entry.second);for(const auto&entry:hudEdits_)files["assets/"+hudTexturePath(entry.first)]=entry.second;return files;}
    std::map<std::string,Bytes> paintModFiles(const PaintSelection &p,bool includePaint=true) const {
        std::map<std::string,Bytes> files;
        if(includePaint&&colors_->changed())for(const auto &entry:colors_->files())files[entry.first]=entry.second;
        auto decal=appearance_->decal(p);if(decal)files[lower(decal->member)]=decal->batch;
        if(includePaint){std::map<std::string,std::string> names;auto carToken="car_"+p.car;try{auto mappings=mod_.originMetadata().mappings();auto found=mappings.find(p.car);if(found!=mappings.end())carToken="car_"+found->second;}catch(...){}auto paintTokenName=paintToken();auto carEdit=nameEdits_.find(carToken);if(carEdit!=nameEdits_.end())names[carToken]=carEdit->second;auto paintEdit=nameEdits_.find(paintTokenName);if(paintEdit!=nameEdits_.end())names[paintTokenName]=paintEdit->second;for(auto &entry:renamedStrings([&](const std::string &path){return mod_.assetBytes(path,stock_);},names,allLocaleNames_,false)){require(entry.first.rfind("assets/",0)==0,"Unexpected string path");files[entry.first.substr(7)]=std::move(entry.second);}}
        return files;
    }
    void saveCarTemplateTo(const fs::path &path){
        auto p=paintSelection();auto files=extraFiles();
        for(auto &car:mod_.cars)if(car.second->changed())files["assets/graphics/vehicles/"+car.first+"/nodes.bdae"]=car.second->bytes();
        std::map<std::string,Bytes> assets;for(auto &entry:files){require(entry.first.rfind("assets/",0)==0,"Unexpected template resource");assets[entry.first.substr(7)]=std::move(entry.second);}
        auto origin=mod_.originMetadata();auto mappings=origin.mappings();Json manifest;
        manifest["created_with"]=CUSTOMS_VERSION;manifest["authored_id"]=currentCar_;manifest["name"]=localized(labels_,"car_"+currentCar_);
        manifest["donor_id"]=mappings.count(currentCar_)?mappings.at(currentCar_):"";manifest["overwrite"]=origin.overwrite();
        auto recipe=std::find_if(mod_.files.begin(),mod_.files.end(),[](const auto &entry){return lower(entry.first)=="model-import.json";});
        if(recipe!=mod_.files.end()){auto parsed=Json::parse(std::string(recipe->second.begin(),recipe->second.end()));manifest["import_recipe"]=parsed;manifest["source_sha256"]=parsed.value("source_sha256","");manifest["source_title"]=parsed.value("source_title","");}
        else{manifest["import_recipe"]=Json();manifest["source_sha256"]="";manifest["source_title"]="";}
        auto bytes=origin.bytes();manifest["origin"]=std::string(bytes.begin(),bytes.end());
        writeCarTemplate(path,manifest,std::move(assets));
        message("Saved car template: "+path.u8string()+". Rebuild will ask for the source GLB; no path is stored.");
    }
    void applyCarTemplateFrom(const fs::path &path){
        auto tpl=loadCarTemplate(path);auto authored=tpl.manifest.value("authored_id",std::string());require(authored==currentCar_,"Template is for car "+authored+"; select that car first");
        for(auto &entry:tpl.assets){auto asset="assets/"+entry.first;mod_.files[asset]=entry.second;const std::string prefix="graphics/vehicles/",suffix="/nodes.bdae";if(entry.first.rfind(prefix,0)==0&&entry.first.size()==prefix.size()+3+suffix.size()&&entry.first.substr(prefix.size()+3)==suffix){auto car=entry.first.substr(prefix.size(),3);mod_.cars[car]=std::make_unique<NodeTransforms>(entry.second);}}
        labels_=displayNamesFrom([&](const std::string &name){return mod_.assetBytes(name,stock_);});applyCarNameAliases();loadNameLocalePolicy();appearance_=std::make_unique<ModAppearance>(mod_,stock_,labels_);colors_=appearance_->colors.get();unlocks_=std::make_unique<UnlockConfig>(mod_,stock_);refreshPreviewColors();nameEdits_.clear();hudEdits_.clear();
        GtkTreeIter iter;if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(car_),&iter)){auto *model=gtk_combo_box_get_model(GTK_COMBO_BOX(car_));gtk_list_store_set(GTK_LIST_STORE(model),&iter,0,localized(labels_,"car_"+currentCar_).c_str(),-1);}
        selectCar();refreshPaint();refreshMaterials();markEdited();message("Applied appearance template: "+path.u8string()+". Export a copy to save it.");
    }
    void saveCarTemplateDialog(){
        auto p=paintSelection();std::string name=exportBaseName(p)+"-car-template";auto *d=gtk_file_chooser_dialog_new("Save car template",GTK_WINDOW(dialog_),GTK_FILE_CHOOSER_ACTION_SAVE,"Cancel",GTK_RESPONSE_CANCEL,"Save",GTK_RESPONSE_ACCEPT,nullptr);gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),true);gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d),name.c_str());
        std::string path;if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){auto *value=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));path=value;g_free(value);}gtk_widget_destroy(d);if(path.empty())return;saveCarTemplateTo(fs::u8path(path));
    }
    void applyCarTemplateDialog(){
        auto *d=gtk_file_chooser_dialog_new("Open car template",GTK_WINDOW(dialog_),GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,"Cancel",GTK_RESPONSE_CANCEL,"Open",GTK_RESPONSE_ACCEPT,nullptr);
        std::string path;if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){auto *value=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));path=value;g_free(value);}gtk_widget_destroy(d);if(path.empty())return;applyCarTemplateFrom(fs::u8path(path));
    }
    void exportPaintMod(){
        auto p=paintSelection();auto decal=appearance_->decal(p);bool hasNameEdit=nameEdits_.count("car_"+p.car)||nameEdits_.count(paintToken());require(decal||colors_->changed()||hasNameEdit,"No livery or paint changes to export");auto found=exportKinds_.find(paintKey(p));std::string kind=found!=exportKinds_.end()?found->second:(decal?"livery":"template");
        std::string name=exportFileName(p,kind);name.resize(name.size()-4);name+="-mod";
        auto *d=gtk_file_chooser_dialog_new("Export paint/livery mod",GTK_WINDOW(dialog_),GTK_FILE_CHOOSER_ACTION_SAVE,"Cancel",GTK_RESPONSE_CANCEL,"Export",GTK_RESPONSE_ACCEPT,nullptr);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),true);gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d),name.c_str());
        auto *options=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);auto *includePaint=gtk_check_button_new_with_label("Include paint configuration");gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(includePaint),!decal||colors_->changed()||hasNameEdit);if(!decal){gtk_widget_set_sensitive(includePaint,false);gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(includePaint),true);}auto *includeNote=gtk_label_new("Include the full paint configuration (colors, price and names). Uncheck to export only the livery texture.");gtk_label_set_line_wrap(GTK_LABEL(includeNote),true);gtk_label_set_max_width_chars(GTK_LABEL(includeNote),40);gtk_box_pack_start(GTK_BOX(options),includePaint,false,false,0);gtk_box_pack_start(GTK_BOX(options),includeNote,false,false,0);gtk_widget_show_all(options);gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(d),options);
        std::string path;bool withPaint=true;if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){auto *value=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));path=value;g_free(value);withPaint=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(includePaint));}gtk_widget_destroy(d);if(path.empty())return;
        auto files=paintModFiles(p,withPaint);require(!files.empty(),"No livery or paint changes to export");require(!fs::exists(path)||fs::is_directory(path),"Choose a folder for the paint mod");
        Json metadata={{"paint_source_sha256",sha(colors_->original())},{"paint_configuration_modified",withPaint&&colors_->changed()},{"selected_car",p.car},{"selected_pack_index",p.packIndex}};
        auto result=publishPaintMod(files,metadata,fs::u8path(path));
        std::string text="Exported paint/livery mod: "+path+". Install only this folder, then restart the game.";
        if(!result["sibling_resource_conflicts"].empty())text+=" Resource conflicts: "+result["sibling_resource_conflicts"].dump();
        message(text);
    }
    GtkWidget *multiplierStyle_{},*multiplierSample_{},*meterStyle_{},*meterSample_{},*showHud_{};std::unique_ptr<CarHudPreview> hudPreview_;bool showCarHud_=true;bool loadingMultiplier_=false;std::map<std::string,Bytes> hudEdits_;
    void refreshMultiplier(){
        loadingMultiplier_=true;hudPreview_.reset();
        try{
            auto found=hudEdits_.find(currentCar_);auto bytes=found==hudEdits_.end()?hudAtlasFor(mod_,stock_,currentCar_):found->second;
            hudPreview_=std::make_unique<CarHudPreview>(carHudPreview(bytes));
            gtk_combo_box_set_active(GTK_COMBO_BOX(multiplierStyle_),identifyMultiplierStyle(stock_,bytes)+1);gtk_widget_set_sensitive(multiplierStyle_,true);
            auto &multiplier=hudPreview_->multiplier;auto*pix=gdk_pixbuf_new(GDK_COLORSPACE_RGB,true,8,multiplier.width,multiplier.height);for(uint32_t y=0;y<multiplier.height;y++)std::copy_n(multiplier.pixels.data()+size_t(y)*multiplier.width*4,size_t(multiplier.width)*4,gdk_pixbuf_get_pixels(pix)+size_t(y)*gdk_pixbuf_get_rowstride(pix));gtk_image_set_from_pixbuf(GTK_IMAGE(multiplierSample_),pix);g_object_unref(pix);
            gtk_combo_box_set_active(GTK_COMBO_BOX(meterStyle_),identifyMeterStyle(stock_,bytes)+1);gtk_widget_set_sensitive(meterStyle_,true);
            auto &meter=hudPreview_->meter;auto*meterPix=gdk_pixbuf_new(GDK_COLORSPACE_RGB,true,8,meter.width,meter.height);for(uint32_t y=0;y<meter.height;y++)std::copy_n(meter.pixels.data()+size_t(y)*meter.width*4,size_t(meter.width)*4,gdk_pixbuf_get_pixels(meterPix)+size_t(y)*gdk_pixbuf_get_rowstride(meterPix));auto*scaled=gdk_pixbuf_scale_simple(meterPix,192,48,GDK_INTERP_BILINEAR);gtk_image_set_from_pixbuf(GTK_IMAGE(meterSample_),scaled?scaled:meterPix);if(scaled)g_object_unref(scaled);g_object_unref(meterPix);
        }catch(const std::exception&){
            gtk_combo_box_set_active(GTK_COMBO_BOX(multiplierStyle_),0);gtk_widget_set_sensitive(multiplierStyle_,false);gtk_image_clear(GTK_IMAGE(multiplierSample_));
            gtk_combo_box_set_active(GTK_COMBO_BOX(meterStyle_),0);gtk_widget_set_sensitive(meterStyle_,false);gtk_image_clear(GTK_IMAGE(meterSample_));
        }
        loadingMultiplier_=false;if(canvas_)gtk_widget_queue_draw(canvas_);
    }
    // Compose the selected native multiplier and boost meter styles over the
    // imported atlas so changing one selector never discards the other.
    void rebuildHud(){
        auto original=hudAtlasFor(mod_,stock_,currentCar_);Bytes edited=original;
        int multiplier=gtk_combo_box_get_active(GTK_COMBO_BOX(multiplierStyle_))-1,meter=gtk_combo_box_get_active(GTK_COMBO_BOX(meterStyle_))-1;
        if(multiplier>=0)edited=multiplierStyleBatch(edited,stockHudStyle(stock_,multiplier));
        if(meter>=0)edited=meterStyleBatch(edited,stockMeterStyle(stock_,meter));
        if(edited==original)hudEdits_.erase(currentCar_);else hudEdits_[currentCar_]=std::move(edited);
    }
    void changeMultiplier(){if(loadingMultiplier_||currentCar_.empty())return;rebuildHud();markEdited();refreshMultiplier();message("Multiplier brackets updated for this car and all its paints. Export a copy to save.");}
    void changeMeter(){if(loadingMultiplier_||currentCar_.empty())return;rebuildHud();markEdited();refreshMultiplier();message("Boost meter shape updated for this car and all its paints. Export a copy to save.");}
    void setPlacement(bool placement){
        if(placement_==placement)return;
        finish(true);placement_=placement;
        if(previewHint_)gtk_label_set_text(GTK_LABEL(previewHint_),placement_?"Drag marker • G move • R rotate • S scale • X/Y/Z constrain\nSpace: snap • Ctrl+Space: conform • Ctrl+Shift+Z: reset\nMiddle-drag: pan • Wheel: zoom • Right-drag: orbit/elevation\nShift: fine • Enter: apply • ~: cancel/reset view • Ctrl+Z: undo attachment":"Paint + livery preview • Approximate lighting\nMiddle-drag: pan • Wheel: zoom • Right-drag: orbit/elevation\nUse the HUD tab for placement controls.");
        if(placement_){g_idle_add(+[](gpointer w)->gboolean{gtk_widget_grab_focus(GTK_WIDGET(w));return G_SOURCE_REMOVE;},canvas_);}
        if(canvas_){gtk_widget_queue_draw(canvas_);}
    }
    GtkWidget *viewMode_{},*addDecalButton_{};int view_=0;
    void uv(cairo_t *cr,double width,double height,double lineWidth=1,double alpha=.8){
        if(!body_)return;
        cairo_set_source_rgba(cr,.1,.8,1,alpha);cairo_set_line_width(cr,lineWidth);
        for(const auto &mesh:body_->meshes)for(size_t i=0;i+2<mesh.indices.size();i+=3){for(int j=0;j<3;j++){auto v=mesh.uv.at(mesh.indices[i+j]);if(j==0)cairo_move_to(cr,v[0]*width,v[1]*height);else cairo_line_to(cr,v[0]*width,v[1]*height);}cairo_close_path(cr);}
        cairo_stroke(cr);
    }
    void uvBoundaries(cairo_t *cr,double width,double height,double lineWidth=1,double alpha=.8,int onlyMaterial=-1){
        if(!body_)return;
        cairo_set_source_rgba(cr,.1,.8,1,alpha);cairo_set_line_width(cr,lineWidth);
        for(const auto &mesh:body_->meshes){
            if(onlyMaterial>=0&&int(mesh.material)!=onlyMaterial)continue;
            if(onlyMaterial<0){const auto &material=body_->materials.at(mesh.material);bool livery=false;for(const auto &texture:material.value("textures",Json::array()))if(texture.is_string()&&lower(texture.get<std::string>()).find("livery")!=std::string::npos){livery=true;break;}if(!livery&&lower(material.value("name",std::string())).rfind("carpaint",0)!=0)continue;}
            struct Edge{int count=0;std::array<float,4> uv{};};std::map<std::array<long long,4>,Edge> edges;
            auto quantize=[](float value){return (long long)std::llround(double(value)*100000.0);};
            for(size_t i=0;i+2<mesh.indices.size();i+=3){
                size_t index[3]={mesh.indices[i],mesh.indices[i+1],mesh.indices[i+2]};
                for(int side=0;side<3;side++){
                    size_t a=index[side],b=index[(side+1)%3];long long au=quantize(mesh.uv.at(a)[0]),av=quantize(mesh.uv.at(a)[1]),bu=quantize(mesh.uv.at(b)[0]),bv=quantize(mesh.uv.at(b)[1]);if(au==bu&&av==bv)continue;
                    std::array<float,4> uv{mesh.uv.at(a)[0],mesh.uv.at(a)[1],mesh.uv.at(b)[0],mesh.uv.at(b)[1]};
                    if(std::tie(au,av)>std::tie(bu,bv)){std::swap(au,bu);std::swap(av,bv);std::swap(uv[0],uv[2]);std::swap(uv[1],uv[3]);}
                    auto &edge=edges[{au,av,bu,bv}];if(edge.count==0)edge.uv=uv;edge.count++;
                }
            }
            for(const auto &entry:edges){const auto &edge=entry.second;if(edge.count!=1)continue;cairo_move_to(cr,edge.uv[0]*width,edge.uv[1]*height);cairo_line_to(cr,edge.uv[2]*width,edge.uv[3]*height);}
        }
        cairo_stroke(cr);
    }
    // Transparent DXT5-ready guide: UV island edges on an empty livery layer.
    Image uvGuide(const Texture &texture){
        require(bool(body_),"No car model loaded for the UV guide");
        auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,texture.width,texture.height);
        require(cairo_surface_status(surface)==CAIRO_STATUS_SUCCESS,"Could not create the livery guide");
        auto *cr=cairo_create(surface);
        cairo_set_operator(cr,CAIRO_OPERATOR_SOURCE);cairo_set_source_rgba(cr,0,0,0,0);cairo_paint(cr);cairo_set_operator(cr,CAIRO_OPERATOR_OVER);
        uvBoundaries(cr,texture.width,texture.height,std::max(1.0,double(std::min(texture.width,texture.height))/1024),1);
        cairo_destroy(cr);cairo_surface_flush(surface);
        auto *data=cairo_image_surface_get_data(surface);int stride=cairo_image_surface_get_stride(surface);
        Image image{texture.width,texture.height,Bytes(size_t(texture.width)*texture.height*4)};
        for(unsigned y=0;y<texture.height;y++)for(unsigned x=0;x<texture.width;x++){
            auto *src=data+size_t(y)*stride+size_t(x)*4;auto *dst=image.pixels.data()+(size_t(y)*texture.width+x)*4;unsigned a=src[3];dst[3]=uint8_t(a);
            if(a){dst[0]=uint8_t(std::min(255u,unsigned(src[2])*255/a));dst[1]=uint8_t(std::min(255u,unsigned(src[1])*255/a));dst[2]=uint8_t(std::min(255u,unsigned(src[0])*255/a));}
        }
        cairo_surface_destroy(surface);return image;
    }
    void exportLiveryTemplate(const fs::path &path){auto source=appearance_->imageTemplate(paintSelection());auto image=uvGuide(source.texture);size_t ink=0;for(size_t i=3;i<image.pixels.size();i+=4)if(image.pixels[i])ink++;require(ink>0,"No livery or car-paint UVs; use Save material UV template for this car");savePNG(path,image);}
    void saveUV(){auto p=paintSelection();auto source=appearance_->imageTemplate(p);auto path=chooseArtwork(true,exportFileName(p,"uv"));if(path.empty())return;auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,source.texture.width,source.texture.height);auto *cr=cairo_create(surface);uv(cr,source.texture.width,source.texture.height);cairo_destroy(cr);auto status=cairo_surface_write_to_png(surface,path.c_str());cairo_surface_destroy(surface);require(status==CAIRO_STATUS_SUCCESS,"Could not save UV template");}
    void drawPreview(cairo_t *cr,int width,int height){
        cairo_set_source_rgb(cr,.07,.08,.10);cairo_paint(cr);if(!body_||!colors_)return;
        if(view_<2||placement_){drawCar(cr,*body_,*colors_,{currentCar_,"","","",paintIndex_},width,height,yaw_,zoom_,view_==1?nullptr:decalPreview_.get(),placement_?&nodes_:nullptr,int(selected_),pitch_,&projection_,panX_,panY_,&hit_,showCarHud_?hudPreview_.get():nullptr,&nodes_,&previewColors_);return;}
        std::unique_ptr<Image> original;const Image *image=decalPreview_.get();if(view_==3){auto p=paintSelection();if(!p.resource.empty()){auto target=targetFrom([&](const std::string &path){return mod_.assetBytes(path,stock_);},p.resource);original=std::make_unique<Image>(decodeDDS(target.texture));image=original.get();}else image=nullptr;}
        auto dimensions=appearance_->imageTemplate(paintSelection()).texture;double ratio=std::min(double(width)/dimensions.width,double(height)/dimensions.height)*.94,w=dimensions.width*ratio,h=dimensions.height*ratio;cairo_save(cr);cairo_translate(cr,(width-w)/2,(height-h)/2);cairo_rectangle(cr,0,0,w,h);cairo_clip(cr);
        for(int y=0;y<16;y++)for(int x=0;x<16;x++){double c=(x+y)%2?.3:.2;cairo_set_source_rgb(cr,c,c,c);cairo_rectangle(cr,x*w/16,y*h/16,w/16+1,h/16+1);cairo_fill(cr);}
        if(view_==4)uv(cr,w,h);else if(image){auto *pix=gdk_pixbuf_new_from_data(image->pixels.data(),GDK_COLORSPACE_RGB,true,8,image->width,image->height,image->width*4,nullptr,nullptr);cairo_scale(cr,w/image->width,h/image->height);gdk_cairo_set_source_pixbuf(cr,pix,0,0);cairo_paint(cr);g_object_unref(pix);}cairo_restore(cr);
    }
    void importModel(){
        if(unsaved_){auto*q=gtk_message_dialog_new(GTK_WINDOW(dialog_),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,"Discard unexported edits after importing a new model?");int response=gtk_dialog_run(GTK_DIALOG(q));gtk_widget_destroy(q);if(response!=GTK_RESPONSE_YES)return;}
        auto*chooser=gtk_file_chooser_dialog_new("Import model",GTK_WINDOW(dialog_),GTK_FILE_CHOOSER_ACTION_OPEN,"Cancel",GTK_RESPONSE_CANCEL,"Open",GTK_RESPONSE_ACCEPT,nullptr);auto*filter=gtk_file_filter_new();gtk_file_filter_set_name(filter,"glTF binary (*.glb)");gtk_file_filter_add_pattern(filter,"*.glb");gtk_file_filter_add_pattern(filter,"*.GLB");gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser),filter);std::string path;if(gtk_dialog_run(GTK_DIALOG(chooser))==GTK_RESPONSE_ACCEPT){auto*v=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));path=v;g_free(v);}gtk_widget_destroy(chooser);if(path.empty())return;
        ModelImportDialog importer(stock_,fs::u8path(path));auto output=importer.run(GTK_WINDOW(dialog_));if(output.empty())return;
        AttachmentEditor check(stock_,output);requestedAction=1;requestedPath=output.u8string();restartRequested=true;unsaved_=false;gtk_dialog_response(GTK_DIALOG(dialog_),100);
    }
    void openSource(int action){
        std::string path;if(action!=3){auto *chooser=gtk_file_chooser_dialog_new(action==0?"Open game folder":action==1?"Import mod folder":"Import mod ZIP",GTK_WINDOW(dialog_),action==2?GTK_FILE_CHOOSER_ACTION_OPEN:GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,"Cancel",GTK_RESPONSE_CANCEL,"Open",GTK_RESPONSE_ACCEPT,nullptr);if(gtk_dialog_run(GTK_DIALOG(chooser))==GTK_RESPONSE_ACCEPT){auto *value=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));path=value;g_free(value);}gtk_widget_destroy(chooser);if(path.empty())return;}
        // Validate before replacing the working session.
        if(action==0){Archive archive((fs::u8path(path)/"vfs.zip").u8string());AttachmentEditor check(archive,{});}
        else {AttachmentEditor check(stock_,action==3?fs::path{}:fs::u8path(path));}
        if(unsaved_){auto *q=gtk_message_dialog_new(GTK_WINDOW(dialog_),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,"Discard unexported edits and open the new source?");int answer=gtk_dialog_run(GTK_DIALOG(q));gtk_widget_destroy(q);if(answer!=GTK_RESPONSE_YES)return;}
        requestedAction=action;requestedPath=path;restartRequested=true;unsaved_=false;gtk_dialog_response(GTK_DIALOG(dialog_),100);
    }
    PreviewProjection projection_;PreviewHit hit_;V orbitPivot_{};std::array<double,2> orbitAnchor_{};
    std::array<GtkWidget*,3> camera_{};
    guint cameraDrag_=0;double panX_=0,panY_=0,cameraX_=0,cameraY_=0;
    void cameraMotion(double x,double y){
        double dx=x-cameraX_,dy=y-cameraY_;cameraX_=x;cameraY_=y;mouseX_=x;mouseY_=y;
        if(cameraDrag_==2){panX_+=dx;panY_+=dy;projection_.cx+=dx;projection_.cy+=dy;}
        else if(cameraDrag_==3){
            double degrees=std::fmod(yaw_*180/3.141592653589793+dx*.4+360,360);
            gtk_range_set_value(GTK_RANGE(camera_[0]),degrees);
            gtk_range_set_value(GTK_RANGE(camera_[1]),std::clamp(pitch_*180/3.141592653589793+dy*.4,-80.,80.));
            if(body_){auto next=cameraProjection(*body_,gtk_widget_get_allocated_width(canvas_),gtk_widget_get_allocated_height(canvas_),yaw_,pitch_,zoom_,panX_,panY_);auto point=next.project(orbitPivot_);double px=orbitAnchor_[0]-point[0],py=orbitAnchor_[1]-point[1];panX_+=px;panY_+=py;next.cx+=px;next.cy+=py;projection_=next;}
        }
        gtk_widget_queue_draw(canvas_);
    }
    bool scroll(GdkEventScroll *event){
        double delta=0;
        if(event->direction==GDK_SCROLL_UP)delta=-1;
        else if(event->direction==GDK_SCROLL_DOWN)delta=1;
        else if(event->direction==GDK_SCROLL_SMOOTH)delta=event->delta_y;
        else return false;
        double next=std::clamp(zoom_*std::exp(-delta*.12),.5,12.),ratio=next/zoom_;
        // Zoom around the pointer, keeping a grabbed marker under the cursor.
        double x=event->x,y=event->y,w=gtk_widget_get_allocated_width(canvas_),h=gtk_widget_get_allocated_height(canvas_);
        panX_=x-(x-w*.5-panX_)*ratio-w*.5;
        panY_=y-(y-h*.5-panY_)*ratio-h*.5;
        projection_.cx=x+(projection_.cx-x)*ratio;projection_.cy=y+(projection_.cy-y)*ratio;projection_.scale*=ratio;
        if(operation_=='g'){moveBase_=values_.at(key());startX_=mouseX_=x;startY_=mouseY_=y;}
        if(cameraDrag_){cameraX_=x;cameraY_=y;if(cameraDrag_==3)orbitAnchor_=projection_.project(orbitPivot_);}
        gtk_range_set_value(GTK_RANGE(camera_[2]),next);gtk_widget_queue_draw(canvas_);return true;
    }
    char operation_=0;int axis_=-1;bool dragging_=false,wasUnsaved_=false;
    double mouseX_=0,mouseY_=0,startX_=0,startY_=0;
    std::array<double,9> before_{},moveBase_{};
    using NodeValues=std::map<size_t,std::array<double,9>>;
    GtkWidget *linkNos_{},*linkScope_{},*linkPartner_{};NodeValues beforeGroup_;
    std::array<double,9> nodeValues(size_t node)const{
        auto found=values_.find(currentCar_+"/"+std::to_string(node));if(found!=values_.end())return found->second;
        auto m=mod_.cars.at(currentCar_)->originalModel().nodes.at(node).matrix;return {m[9],m[10],m[11],0,0,0,1,1,1};
    }
    NodeValues snapshot()const{NodeValues result;const auto &nodes=mod_.cars.at(currentCar_)->originalModel().nodes;for(size_t i=0;i<nodes.size();i++)if(editableAttachment(nodes[i].name))result[i]=nodeValues(i);return result;}
    void restoreNodes(const NodeValues &state){auto &writer=*mod_.cars.at(currentCar_);for(const auto &entry:state){writer.set(entry.first,adjustedMatrix(writer.originalModel().nodes.at(entry.first).matrix,entry.second));values_[currentCar_+"/"+std::to_string(entry.first)]=entry.second;}updating_=true;auto v=nodeValues(selected_);for(int i=0;i<9;i++)gtk_spin_button_set_value(GTK_SPIN_BUTTON(fields_[i]),v[i]);updating_=false;refreshNodes();}
    std::vector<size_t> linkedNodes()const{
        std::vector<size_t> result;const auto &nodes=mod_.cars.at(currentCar_)->originalModel().nodes;
        if(replaying_||!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(linkNos_))||nodes.at(selected_).name.rfind("nos",0)!=0)return result;
        if(gtk_combo_box_get_active(GTK_COMBO_BOX(linkScope_))==1){auto id=gtk_combo_box_get_active_id(GTK_COMBO_BOX(linkPartner_));if(id&&std::stoul(id)!=selected_)result.push_back(std::stoul(id));}
        else for(size_t i=0;i<nodes.size();i++)if(i!=selected_&&nodes[i].name.rfind("nos",0)==0)result.push_back(i);
        return result;
    }
    struct UndoEntry {std::string car;size_t node;std::array<double,9> values;NodeValues group;};
    std::vector<UndoEntry> undo_;bool replaying_=false;
    void remember(const std::array<double,9> &values){
        if(replaying_)return;
        if(undo_.size()==200)undo_.erase(undo_.begin());
        auto state=snapshot();state[selected_]=values;undo_.push_back({currentCar_,selected_,values,std::move(state)});
    }
    void undo(){
        if(operation_){finish(true);return;}
        if(undo_.empty()){message("Nothing to undo.");return;}
        auto entry=undo_.back();undo_.pop_back();replaying_=true;
        try{
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(car_),entry.car.c_str());
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(node_),std::to_string(entry.node).c_str());
            restoreNodes(entry.group);markEdited();replaying_=false;message("Undid attachment edit. Export a copy to save.");
        }catch(...){replaying_=false;undo_.push_back(entry);throw;}
    }
    void snapToCar(bool conform=false){
        finish(true);require(bool(body_),"No car model loaded");auto &writer=*mod_.cars.at(currentCar_);auto before=nodeValues(selected_);bool hud=writer.originalModel().nodes.at(selected_).name.rfind("nos",0)!=0;auto result=snap::align(*body_,writer.originalModel().nodes.at(selected_).matrix,before,hud||conform,conform);
        remember(before);auto state=snapshot();state[selected_]=result;auto linked=linkedNodes();if(!linked.empty()){double delta[3]={result[0]-before[0],result[1]-before[1],result[2]-before[2]};for(size_t node:linked){auto value=state.count(node)?state.at(node):nodeValues(node);for(int i=0;i<3;i++)value[i]+=delta[i];state[node]=value;}}restoreNodes(state);markEdited();std::string suffix=linked.empty()?"":(linked.size()==1?" The linked outlet moved with it.":" The linked outlets moved with it.");message((conform?"Selected element snapped and conformed to local curvature.":"Selected element snapped to exterior with clearance.")+suffix+" Ctrl+Z to undo; verify in game.");
    }
    void resetAttachment(){
        finish(true);require(bool(mod_.cars.count(currentCar_)),"No car model loaded");auto &writer=*mod_.cars.at(currentCar_);auto before=nodeValues(selected_);
        remember(before);writer.reset(selected_);values_.erase(key());selectNode();refreshNodes();markEdited();message("Attachment reset to its imported transform. Ctrl+Z to undo.");
    }
    void applyValues(const std::array<double,9> &values){
        updating_=true;for(int i=0;i<9;i++)gtk_spin_button_set_value(GTK_SPIN_BUTTON(fields_[i]),values[i]);updating_=false;edit();
    }
    void finish(bool cancel){
        if(!operation_)return;
        if(!cancel&&values_.at(key())!=before_){remember(before_);undo_.back().group=beforeGroup_;}
        // Keep the operation active during rollback so it cannot record an edit.
        if(cancel){restoreNodes(beforeGroup_);unsaved_=wasUnsaved_;}
        operation_=0;dragging_=false;
        message(cancel?"Adjustment cancelled.":"Adjustment applied. Export a copy to save.");
    }
    void begin(char op,bool drag=false){
        if(operation_)finish(false);
        before_=values_.at(key());beforeGroup_=snapshot();moveBase_=before_;wasUnsaved_=unsaved_;operation_=op;axis_=-1;dragging_=drag;startX_=mouseX_;startY_=mouseY_;
        message("Move: G • Rotate: R • Scale: S • Axis: X/Y/Z • Shift: fine • Enter/click: apply • ~: cancel");
    }
    void motion(double x,double y,bool fine){
        if(cameraDrag_){cameraMotion(x,y);return;}
        mouseX_=x;mouseY_=y;if(!operation_||projection_.scale<=0)return;
        double dx=(x-startX_)*(fine?.1:1),dy=(y-startY_)*(fine?.1:1);auto value=operation_=='g'?moveBase_:before_;
        if(operation_=='g'){
            auto v=projection_.screenDelta(dx,dy);double delta[]={v.x,v.y,v.z};
            if(axis_>=0){V unit{};if(axis_==0)unit.x=1;else if(axis_==1)unit.y=1;else unit.z=1;
                auto a=projection_.project({0,0,0}),b=projection_.project(unit);double ax=b[0]-a[0],ay=b[1]-a[1],norm=ax*ax+ay*ay;
                // An axis facing the camera has no screen direction: use vertical drag.
                delta[axis_]=norm>projection_.scale*projection_.scale*.01?(dx*ax+dy*ay)/norm:-dy/projection_.scale;
            }
            for(int i=0;i<3;i++)if(axis_<0||axis_==i)value[i]+=delta[i];
        }else if(operation_=='r'){
            int axis=axis_<0?2:axis_;value[3+axis]=std::remainder(before_[3+axis]+(dx-dy)*.5,360.);
        }else if(operation_=='s'){
            double factor=std::exp(std::clamp((dx-dy)*.01,-6.,6.));
            for(int i=0;i<3;i++)if(axis_<0||axis_==i)value[6+i]=std::clamp(before_[6+i]*factor,.05,20.);
        }
        applyValues(value);
        std::string mode=operation_=='g'?"Move":operation_=='r'?"Rotate":"Scale";
        message(mode+(axis_<0?(operation_=='r'?" Z (roll)":" — view plane / all axes"):" "+std::string(1,"XYZ"[axis_]))+" • Shift: fine • Enter/click: apply • ~: cancel");
    }
    void resetView(){
        cameraDrag_=0;panX_=panY_=0;
        gtk_range_set_value(GTK_RANGE(camera_[0]),180);gtk_range_set_value(GTK_RANGE(camera_[1]),26);gtk_range_set_value(GTK_RANGE(camera_[2]),1);gtk_widget_queue_draw(canvas_);
        message("Camera view reset.");
    }
    bool keyPress(guint key,guint state=0){
        key=gdk_keyval_to_lower(key);
        if(key==GDK_KEY_space){if(!placement_)return false;snapToCar((state&GDK_CONTROL_MASK)!=0);return true;}
        if(key==GDK_KEY_asciitilde||key==GDK_KEY_grave){if(operation_)finish(true);else resetView();return true;}
        if(key==GDK_KEY_Escape){if(operation_){finish(true);return true;}return false;}
        if(key==GDK_KEY_Return||key==GDK_KEY_KP_Enter){if(operation_){finish(false);return true;}return false;}
        if(placement_&&(key==GDK_KEY_g||key==GDK_KEY_r||key==GDK_KEY_s)){begin(char(key));return true;}
        if(operation_&&(key==GDK_KEY_x||key==GDK_KEY_y||key==GDK_KEY_z)){axis_=key==GDK_KEY_x?0:key==GDK_KEY_y?1:2;motion(mouseX_,mouseY_,false);return true;}
        return false;
    }
    bool press(GdkEventButton *event){
        mouseX_=event->x;mouseY_=event->y;gtk_widget_grab_focus(canvas_);
        if(event->button==2||event->button==3){
            if(operation_)return true;
            cameraDrag_=event->button;cameraX_=event->x;cameraY_=event->y;
            if(cameraDrag_==3&&projection_.scale>0){orbitPivot_=hit_.point(projection_,event->x,event->y);orbitAnchor_={event->x,event->y};}return true;
        }
        if(event->button!=1||cameraDrag_)return false;
        if(operation_){finish(false);return true;}
        if(!placement_||projection_.scale<=0)return true;
        double best=18*18;int hit=-1;
        for(size_t i=0;i<nodes_.nodes.size();i++)if(editableAttachment(nodes_.nodes[i].name)){
            auto &m=nodes_.nodes[i].matrix;auto v=projection_.project({m[9],m[10],m[11]});double distance=std::pow(v[0]-event->x,2)+std::pow(v[1]-event->y,2);
            if(distance<best){best=distance;hit=int(i);}
        }
        if(hit>=0){gtk_combo_box_set_active_id(GTK_COMBO_BOX(node_),std::to_string(hit).c_str());begin('g',true);}
        return true;
    }
    std::string key()const{return currentCar_+"/"+std::to_string(selected_);}
    void message(const std::string &s){gtk_label_set_text(GTK_LABEL(status_),s.c_str());}
    template<class F> void guard(F fn){try{fn();}catch(const std::exception &e){showErrorDialog(GTK_WINDOW(dialog_),e.what());}}
    void refreshNodes(){nodes_=decodeModel(mod_.cars.at(currentCar_)->bytes());gtk_widget_queue_draw(canvas_);}
    void selectNode(){
        finish(true);
        auto id=gtk_combo_box_get_active_id(GTK_COMBO_BOX(node_));if(!id)return;selected_=std::stoul(id);
        auto &value=values_[key()];if(value[6]==0){auto m=mod_.cars.at(currentCar_)->originalModel().nodes.at(selected_).matrix;value={m[9],m[10],m[11],0,0,0,1,1,1};}
        updating_=true;for(int i=0;i<9;i++)gtk_spin_button_set_value(GTK_SPIN_BUTTON(fields_[i]),value[i]);updating_=false;
        bool nos=mod_.cars.at(currentCar_)->originalModel().nodes.at(selected_).name.rfind("nos",0)==0;
        gtk_widget_set_sensitive(linkNos_,nos);gtk_widget_set_sensitive(linkScope_,nos);gtk_widget_set_sensitive(linkPartner_,nos);
        auto partnerId=gtk_combo_box_get_active_id(GTK_COMBO_BOX(linkPartner_));std::string previousPartner=partnerId?partnerId:"";
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(linkPartner_));const auto &allNodes=mod_.cars.at(currentCar_)->originalModel().nodes;
        for(size_t i=0;i<allNodes.size();i++)if(i!=selected_&&allNodes[i].name.rfind("nos",0)==0)gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(linkPartner_),std::to_string(i).c_str(),attachmentLabel(allNodes[i].name).c_str());
        if(!gtk_combo_box_set_active_id(GTK_COMBO_BOX(linkPartner_),previousPartner.c_str()))gtk_combo_box_set_active(GTK_COMBO_BOX(linkPartner_),0);
        gtk_widget_queue_draw(canvas_);
    }
    void edit(){if(updating_||currentCar_.empty())return;
        std::array<double,9> value{};for(int i=0;i<9;i++)value[i]=gtk_spin_button_get_value(GTK_SPIN_BUTTON(fields_[i]));
        if(!operation_&&!replaying_&&values_.at(key())!=value)remember(values_.at(key()));
        auto previous=nodeValues(selected_);auto &writer=*mod_.cars.at(currentCar_);
        for(auto node:linkedNodes()){auto next=nodeValues(node);for(int i=0;i<3;i++)next[i]+=value[i]-previous[i];writer.set(node,adjustedMatrix(writer.originalModel().nodes.at(node).matrix,next));values_[currentCar_+"/"+std::to_string(node)]=next;}
        writer.set(selected_,adjustedMatrix(writer.originalModel().nodes.at(selected_).matrix,value));
        values_[key()]=value;markEdited();refreshNodes();message("Unsaved adjustments. Guides show through the car; verify clipping in-game.");
    }
    void selectCar(){
        finish(true);
        auto id=gtk_combo_box_get_active_id(GTK_COMBO_BOX(car_));if(!id)return;currentCar_=id;
        body_=std::make_unique<Model>(decodeModel(mod_.assetBytes("graphics/vehicles/"+currentCar_+"/lod0_livery.bdae",stock_)));
        auto cfg=colors_->bytes();
        auto paints=cfgValue(Json::parse(Config(cfg).json())).at("Paints").at(currentCar_);
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(paint_));
        for(size_t i=0;i<paints.size();i++){auto name=localized(labels_,paints.at(i).at("packName").get<std::string>());gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(paint_),std::to_string(i).c_str(),name.c_str());}
        gtk_combo_box_set_active(GTK_COMBO_BOX(paint_),0);
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(node_));
        const auto &model=mod_.cars.at(currentCar_)->originalModel();
        for(size_t i=0;i<model.nodes.size();i++)if(editableAttachment(model.nodes[i].name)){auto label=attachmentLabel(model.nodes[i].name);gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(node_),std::to_string(i).c_str(),label.c_str());}
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(linkPartner_));for(size_t i=0;i<model.nodes.size();i++)if(model.nodes[i].name.rfind("nos",0)==0)gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(linkPartner_),std::to_string(i).c_str(),attachmentLabel(model.nodes[i].name).c_str());
        gtk_combo_box_set_active(GTK_COMBO_BOX(node_),0);refreshNodes();refreshMultiplier();refreshMaterials();
    }
    void exportCopy(){
        auto incompatible=mod_.incompatibleOrigins();
        require(incompatible.empty(),"Rename car IDs "+joinIds(incompatible)+" to three lowercase letters or digits before exporting; runtime slots and non-authored IDs cannot be declared in origin.txt");
        require(mod_.unresolvedOrigins().empty(),"Choose the missing stock donors in Legacy car migration before exporting");
        auto *d=gtk_file_chooser_dialog_new("Export mod",GTK_WINDOW(dialog_),GTK_FILE_CHOOSER_ACTION_SAVE,"Cancel",GTK_RESPONSE_CANCEL,"Export",GTK_RESPONSE_ACCEPT,nullptr);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d),"customs-adjusted-mod");
        std::string path;if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){auto *p=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));path=p;g_free(p);}gtk_widget_destroy(d);if(path.empty())return;
        auto files=extraFiles();
        if(files.empty()&&std::none_of(mod_.files.begin(),mod_.files.end(),[](const auto&f){return lower(f.first).rfind("assets/",0)==0;})){
            markEdited();files["assets/graphics/vehicles/"+currentCar_+"/nodes.bdae"]=mod_.cars.at(currentCar_)->bytes();
        }
        mod_.exportCopy(fs::u8path(path),files);unsaved_=false;message("Exported "+path+". Disable the original mod before installing this copy, then restart the game.");
    }
    struct MaterialTarget {size_t material;std::string name,texture;unsigned width=0,height=0;};
    std::vector<MaterialTarget> materialTargets_;
    GtkWidget *materialCombo_{},*materialImport_{},*materialSave_{},*materialUV_{};
    std::string materialAtlasPath() const {return "graphics/vehicles/"+currentCar_+"/textures/batchtextures.brres";}
    std::vector<Texture> materialAtlasTextures() const {return decodeTextures(mod_.assetBytes(materialAtlasPath(),stock_));}
    void refreshMaterials(){
        if(!materialCombo_)return;
        materialTargets_.clear();
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(materialCombo_));
        if(body_)try{auto atlas=materialAtlasTextures();std::set<std::string> labels;
            for(size_t i=0;i<body_->materials.size();i++){auto &material=body_->materials[i];std::string slot;
                for(auto &texture:material.value("textures",Json::array())){if(!texture.is_string())continue;auto value=lower(texture.get<std::string>());if(value.size()>4&&(value.substr(value.size()-4)==".tga"||value.substr(value.size()-4)==".dds")){slot=value.substr(0,value.size()-4);break;}}
                if(slot.empty())continue;
                auto found=std::find_if(atlas.begin(),atlas.end(),[&](const Texture &texture){return texture.name==slot;});
                if(found==atlas.end())continue;
                auto name=material.value("name",slot);if(name.empty())name=slot;auto label=name;for(int suffix=2;!labels.insert(label).second;suffix++)label=name+" #"+std::to_string(suffix);
                materialTargets_.push_back({i,label,slot,found->width,found->height});gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(materialCombo_),std::to_string(materialTargets_.size()-1).c_str(),label.c_str());
            }
        }catch(...){}
        gtk_combo_box_set_active(GTK_COMBO_BOX(materialCombo_),0);
        bool any=!materialTargets_.empty();
        for(auto *button:{materialImport_,materialSave_,materialUV_})if(button)gtk_widget_set_sensitive(button,any);
    }
    const MaterialTarget &materialTarget() const {int index=materialCombo_?gtk_combo_box_get_active(GTK_COMBO_BOX(materialCombo_)):-1;require(index>=0&&size_t(index)<materialTargets_.size(),"Select a material texture");return materialTargets_.at(index);}
    std::string choosePNG(const char *title,bool save,const char *suggested=nullptr){
        auto *d=gtk_file_chooser_dialog_new(title,GTK_WINDOW(dialog_),GTK_FILE_CHOOSER_ACTION_SAVE,"Cancel",GTK_RESPONSE_CANCEL,"Save",GTK_RESPONSE_ACCEPT,nullptr);
        if(!save){gtk_widget_destroy(d);d=gtk_file_chooser_dialog_new(title,GTK_WINDOW(dialog_),GTK_FILE_CHOOSER_ACTION_OPEN,"Cancel",GTK_RESPONSE_CANCEL,"Import",GTK_RESPONSE_ACCEPT,nullptr);}
        else{gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),true);if(suggested)gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d),suggested);}
        std::string path;if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){auto *value=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));path=value;g_free(value);}gtk_widget_destroy(d);return path;
    }
    void materialPNG(bool save,const fs::path &chosen={}){
        auto target=materialTarget();auto atlas=materialAtlasTextures();auto found=std::find_if(atlas.begin(),atlas.end(),[&](const Texture &texture){return texture.name==target.texture;});require(found!=atlas.end(),"Material texture is missing from the car atlas");
        auto base=exportBaseName(paintSelection())+"-"+exportSlug(target.name,"material")+"-material.png";
        if(save){auto path=chosen.empty()?fs::u8path(choosePNG("Save material PNG",true,base.c_str())):chosen;if(path.empty())return;savePNG(path,decodeDDS(*found));message("Saved material texture to "+path.u8string()+".");return;}
        auto path=chosen.empty()?fs::u8path(choosePNG("Import material PNG",false)):chosen;if(path.empty())return;auto image=loadPNG(path,found->width,found->height);
        const char *fourcc=textures::hasAlpha(image)?"DXT5":"DXT1";auto dds=textures::buildDDS(image,fourcc);Texture updated=*found;updated.fourcc=fourcc;updated.width=image.width;updated.height=image.height;updated.flags=textures::hasAlpha(image)?2:updated.flags;updated.mips=u32(dds,28);updated.caps=u32(dds,112);updated.dds=std::move(dds);
        mod_.files[lower("assets/"+materialAtlasPath())]=textures::withTexture(mod_.assetBytes(materialAtlasPath(),stock_),updated);
        auto key=lower(target.name);std::array<double,3> average{1,1,1};double total=0;for(size_t i=0;i<image.pixels.size();i+=4){double a=image.pixels[i+3]/255.;average[0]+=image.pixels[i]/255.*a;average[1]+=image.pixels[i+1]/255.*a;average[2]+=image.pixels[i+2]/255.*a;total+=a;}if(total>0)for(auto &c:average)c/=total;for(auto &entry:previewColors_)if(key.rfind(entry.first,0)==0)entry.second=average;
        markEdited();gtk_widget_queue_draw(canvas_);message("Material texture updated for "+target.name+". Export a copy to save.");
    }
    void materialUV(const fs::path &chosen={}){
        auto target=materialTarget();auto path=chosen.empty()?fs::u8path(choosePNG("Save material UV template",true,(exportBaseName(paintSelection())+"-"+exportSlug(target.name,"material")+"-uv.png").c_str())):chosen;if(path.empty())return;
        auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,target.width,target.height);require(cairo_surface_status(surface)==CAIRO_STATUS_SUCCESS,"Could not create the material UV template");auto *cr=cairo_create(surface);
        cairo_set_operator(cr,CAIRO_OPERATOR_SOURCE);cairo_set_source_rgba(cr,0,0,0,0);cairo_paint(cr);cairo_set_operator(cr,CAIRO_OPERATOR_OVER);uvBoundaries(cr,target.width,target.height,std::max(1.0,double(std::min(target.width,target.height))/1024),1,int(target.material));cairo_destroy(cr);cairo_surface_flush(surface);
        auto status=cairo_surface_write_to_png(surface,path.u8string().c_str());cairo_surface_destroy(surface);require(status==CAIRO_STATUS_SUCCESS,"Could not save the material UV template");message("Saved material UV template to "+path.u8string()+".");
    }
    std::map<std::string,std::array<double,3>> previewColors_;
    bool allLocaleNames_=false;
    void applyCarNameAliases(){try{addCarNameAliases(labels_,mod_.originMetadata().mappings());}catch(...){}}
    void loadNameLocalePolicy(){
        allLocaleNames_=false;auto recipe=std::find_if(mod_.files.begin(),mod_.files.end(),[](const auto &entry){return lower(entry.first)=="model-import.json";});if(recipe==mod_.files.end())return;
        try{auto parsed=Json::parse(std::string(recipe->second.begin(),recipe->second.end()));auto options=parsed.value("options",Json::object());if(options.is_object())allLocaleNames_=options.value("all_locale_names",false);}catch(...){}
    }
    void refreshPreviewColors(){
        previewColors_.clear();auto found=std::find_if(mod_.files.begin(),mod_.files.end(),[](const auto &entry){return lower(entry.first)=="model-import.json";});if(found==mod_.files.end())return;
        try{auto parsed=Json::parse(std::string(found->second.begin(),found->second.end()));for(auto &material:parsed.value("materials",Json::array())){auto name=lower(material.value("name",std::string()));auto color=material.value("average_color",Json::array());if(!name.empty()&&color.is_array()&&color.size()==3)previewColors_[name]={color[0].get<double>(),color[1].get<double>(),color[2].get<double>()};}}catch(...){}
    }
public:
    bool restartRequested=false;int requestedAction=-1;std::string requestedPath;int windowWidth=1100,windowHeight=760;bool maximized=false;
    AttachmentEditor(const Archive &stock,const fs::path &path):stock_(stock),mod_(path,&stock){legacyOrigin_=!path.empty()&&std::none_of(mod_.files.begin(),mod_.files.end(),[](const auto&f){return lower(f.first)=="origin.txt";});labels_=displayNamesFrom([&](const std::string &name){return mod_.assetBytes(name,stock_);});applyCarNameAliases();loadNameLocalePolicy();appearance_=std::make_unique<ModAppearance>(mod_,stock_,labels_);colors_=appearance_->colors.get();unlocks_=std::make_unique<UnlockConfig>(mod_,stock_);refreshPreviewColors();}
    bool confirmUpdate(){
        if(!unsaved_)return true;
        auto *question=gtk_message_dialog_new(GTK_WINDOW(dialog_),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,"Installing the update closes Riff Racer Customs. Adjustments that have not been exported will be lost. Continue?");
        int answer=gtk_dialog_run(GTK_DIALOG(question));gtk_widget_destroy(question);
        return answer==GTK_RESPONSE_YES;
    }
    void build(GtkWindow *parent){
        dialog_=gtk_dialog_new_with_buttons("Riff Racer Customs",parent,GTK_DIALOG_MODAL,"Close",GTK_RESPONSE_CLOSE,nullptr);gtk_window_set_default_size(GTK_WINDOW(dialog_),windowWidth,windowHeight);if(maximized)gtk_window_maximize(GTK_WINDOW(dialog_));
        auto *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog_));gtk_container_set_border_width(GTK_CONTAINER(content),12);
        auto *sources=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);gtk_box_pack_start(GTK_BOX(content),sources,false,false,4);
        const char *sourceLabels[]={"Open game folder","Import mod folder","Import mod ZIP","Stock cars / new mod"};for(int i=0;i<4;i++){auto *button=gtk_button_new_with_label(sourceLabels[i]);gtk_box_pack_start(GTK_BOX(sources),button,false,false,0);g_object_set_data(G_OBJECT(button),"source-action",GINT_TO_POINTER(i));g_signal_connect(button,"clicked",G_CALLBACK(+[](GtkButton *b,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.openSource(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b),"source-action")));});}),this);}
        auto*importButton=gtk_button_new_with_label("Import 3D model");gtk_box_pack_start(GTK_BOX(sources),importButton,false,false,0);g_signal_connect(importButton,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto&a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.importModel();});}),this);
        auto *top=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_pack_start(GTK_BOX(content),top,false,false,5);
        car_=gtk_combo_box_text_new();paint_=gtk_combo_box_text_new();
        for(auto *combo:{car_,paint_}){auto *cells=gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(combo));for(auto *cell=cells;cell;cell=cell->next)g_object_set(G_OBJECT(cell->data),"ellipsize",PANGO_ELLIPSIZE_END,"width-chars",16,nullptr);g_list_free(cells);}
        gtk_box_pack_start(GTK_BOX(top),gtk_label_new("Car"),false,false,0);gtk_box_pack_start(GTK_BOX(top),car_,false,false,0);
        gtk_box_pack_start(GTK_BOX(top),gtk_label_new("Paint"),false,false,0);gtk_box_pack_start(GTK_BOX(top),paint_,false,false,0);
        viewMode_=gtk_combo_box_text_new();for(auto *label:{"Painted + Livery","Car preview","Texture preview","Original","UV template"})gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(viewMode_),label);gtk_combo_box_set_active(GTK_COMBO_BOX(viewMode_),0);gtk_box_pack_start(GTK_BOX(top),viewMode_,false,false,0);g_signal_connect(viewMode_,"changed",G_CALLBACK(+[](GtkComboBox *c,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.view_=gtk_combo_box_get_active(c);gtk_widget_queue_draw(a.canvas_);}),this);
        auto *exportButton=gtk_button_new_with_label("Export mod");exportButton_=exportButton;gtk_box_pack_end(GTK_BOX(top),exportButton,false,false,0);buildExportMode(top);
        updates_=std::make_unique<update::UpdatePanel>(CUSTOMS_VERSION,[this]{return confirmUpdate();},[]{std::exit(0);});
        updates_->buildButton(sources);
        paned_=gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);gtk_paned_set_wide_handle(GTK_PANED(paned_),true);gtk_widget_set_tooltip_text(paned_,"Resizes the preview pane.");gtk_box_pack_start(GTK_BOX(content),paned_,true,true,5);
        auto *scroll=gtk_scrolled_window_new(nullptr,nullptr);gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scroll),false);gtk_widget_set_size_request(scroll,299,200);gtk_paned_pack1(GTK_PANED(paned_),scroll,false,false);
        auto *controls=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);gtk_container_set_border_width(GTK_CONTAINER(controls),8);gtk_container_add(GTK_CONTAINER(scroll),controls);
        buildMigration(controls);
        buildFallback(controls);
        notebook_=gtk_notebook_new();gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook_),true);gtk_box_pack_start(GTK_BOX(controls),notebook_,false,false,0);
        auto addPaintAction=[&](GtkWidget *box,const char *label,int id){auto *b=gtk_button_new_with_label(label);gtk_box_pack_start(GTK_BOX(box),b,false,false,0);if(id==5){addDecalButton_=b;gtk_widget_set_tooltip_text(b,"Exports a transparent livery template with UV outlines.");}if(id==8)gtk_widget_set_tooltip_text(b,"Exports the selected paint and livery without the car model.");g_object_set_data(G_OBJECT(b),"paint-action",GINT_TO_POINTER(id));g_signal_connect(b,"clicked",G_CALLBACK(+[](GtkButton *button,gpointer data){auto &a=*static_cast<AttachmentEditor*>(data);a.guard([&]{a.paintAction(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button),"paint-action")));});}),this);};
        auto *paintTab=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);gtk_container_set_border_width(GTK_CONTAINER(paintTab),6);
        gtk_box_pack_start(GTK_BOX(paintTab),gtk_label_new("In-game car name"),false,false,0);carName_=gtk_entry_new();gtk_entry_set_max_length(GTK_ENTRY(carName_),80);gtk_box_pack_start(GTK_BOX(paintTab),carName_,false,false,0);
        gtk_box_pack_start(GTK_BOX(paintTab),gtk_label_new("In-game paint name"),false,false,0);paintName_=gtk_entry_new();gtk_entry_set_max_length(GTK_ENTRY(paintName_),80);gtk_box_pack_start(GTK_BOX(paintTab),paintName_,false,false,0);
        auto *applyNamesButton=gtk_button_new_with_label("Apply names");gtk_box_pack_start(GTK_BOX(paintTab),applyNamesButton,false,false,0);g_signal_connect(applyNamesButton,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.applyNames();});}),this);
        buildUnlock(paintTab);
        const char *paintKeys[]={"col0","col1","col2","specCol","alloysCol0","alloysCol1"};const char *paintLabels[]={"Body color 1","Body color 2","Body color 3","Highlight","Wheel color 1","Wheel color 2"};
        for(int i=0;i<6;i++){auto *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);gtk_box_pack_start(GTK_BOX(paintTab),row,false,false,0);gtk_box_pack_start(GTK_BOX(row),gtk_label_new(paintLabels[i]),true,true,0);auto *button=gtk_color_button_new();gtk_box_pack_end(GTK_BOX(row),button,false,false,0);paintColors_.push_back({paintKeys[i],button});
            g_signal_connect(button,"color-set",G_CALLBACK(+[](GtkColorButton *b,gpointer data){auto &a=*static_cast<AttachmentEditor*>(data);if(a.loadingPaint_)return;a.guard([&]{for(auto &field:a.paintColors_)if(field.second==GTK_WIDGET(b)){GdkRGBA rgb;gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(b),&rgb);a.colors_->set(a.paintSelection(),field.first,{float(rgb.red*255),float(rgb.green*255),float(rgb.blue*255)});a.markEdited();a.refreshPaint();}});}),this);
        }
        addPaintAction(paintTab,"Set as default paint",4);addPaintAction(paintTab,"Reset paint and livery",3);
        auto *saveTemplate=gtk_button_new_with_label("Save car template…");gtk_box_pack_start(GTK_BOX(paintTab),saveTemplate,false,false,0);gtk_widget_set_tooltip_text(saveTemplate,"Saves the import recipe and appearance changes; the GLB path is not stored.");g_signal_connect(saveTemplate,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto&a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.saveCarTemplateDialog();});}),this);
        auto *applyTemplate=gtk_button_new_with_label("Apply appearance template…");gtk_box_pack_start(GTK_BOX(paintTab),applyTemplate,false,false,0);gtk_widget_set_tooltip_text(applyTemplate,"Applies saved paint, livery, unlocks, names, HUD and attachment changes.");g_signal_connect(applyTemplate,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto&a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.applyCarTemplateDialog();});}),this);
        auto *liveryTab=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_container_set_border_width(GTK_CONTAINER(liveryTab),6);
        liveryInfo_=gtk_label_new("");gtk_label_set_line_wrap(GTK_LABEL(liveryInfo_),true);gtk_label_set_max_width_chars(GTK_LABEL(liveryInfo_),32);gtk_box_pack_start(GTK_BOX(liveryTab),liveryInfo_,false,false,0);
        addPaintAction(liveryTab,"Import PNG livery",0);addPaintAction(liveryTab,"Export livery template",5);addPaintAction(liveryTab,"Save PNG / blank template",1);addPaintAction(liveryTab,"Save UV template",7);addPaintAction(liveryTab,"Make test livery",6);addPaintAction(liveryTab,"Export paint/livery mod",8);addPaintAction(liveryTab,"Remove livery",2);
        gtk_box_pack_start(GTK_BOX(liveryTab),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),false,false,6);gtk_box_pack_start(GTK_BOX(liveryTab),gtk_label_new("Material texture"),false,false,0);
        materialCombo_=gtk_combo_box_text_new();gtk_box_pack_start(GTK_BOX(liveryTab),materialCombo_,false,false,0);gtk_widget_set_tooltip_text(materialCombo_,"Material whose texture and UV layout are edited.");
        materialImport_=gtk_button_new_with_label("Import material PNG");gtk_box_pack_start(GTK_BOX(liveryTab),materialImport_,false,false,0);gtk_widget_set_tooltip_text(materialImport_,"Replaces the selected material texture in the car atlas.");g_signal_connect(materialImport_,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.materialPNG(false);});}),this);
        materialSave_=gtk_button_new_with_label("Save material PNG");gtk_box_pack_start(GTK_BOX(liveryTab),materialSave_,false,false,0);gtk_widget_set_tooltip_text(materialSave_,"Exports the selected material texture.");g_signal_connect(materialSave_,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.materialPNG(true);});}),this);
        materialUV_=gtk_button_new_with_label("Save material UV template");gtk_box_pack_start(GTK_BOX(liveryTab),materialUV_,false,false,0);gtk_widget_set_tooltip_text(materialUV_,"Writes UV outlines for the selected material at its texture size.");g_signal_connect(materialUV_,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.materialUV();});}),this);
        attachmentControls_=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);gtk_container_set_border_width(GTK_CONTAINER(attachmentControls_),6);
        auto *placementHint=gtk_label_new("Attachment placement (Ctrl+Z undo • Ctrl+Shift+Z reset)");gtk_label_set_line_wrap(GTK_LABEL(placementHint),true);gtk_label_set_max_width_chars(GTK_LABEL(placementHint),32);gtk_box_pack_start(GTK_BOX(attachmentControls_),placementHint,false,false,6);
        node_=gtk_combo_box_text_new();gtk_box_pack_start(GTK_BOX(attachmentControls_),node_,false,false,0);
        gtk_box_pack_start(GTK_BOX(attachmentControls_),gtk_label_new("Multiplier brackets — all paints"),false,false,0);multiplierStyle_=gtk_combo_box_text_new();for(auto*text:{"Keep imported / original","Four brackets","Two top brackets (Pulse)"})gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(multiplierStyle_),text);gtk_box_pack_start(GTK_BOX(attachmentControls_),multiplierStyle_,false,false,0);multiplierSample_=gtk_image_new();gtk_box_pack_start(GTK_BOX(attachmentControls_),multiplierSample_,false,false,0);g_signal_connect(multiplierStyle_,"changed",G_CALLBACK(+[](GtkComboBox*,gpointer p){auto&a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.changeMultiplier();});}),this);
        gtk_box_pack_start(GTK_BOX(attachmentControls_),gtk_label_new("Boost meter shape — all paints"),false,false,0);meterStyle_=gtk_combo_box_text_new();gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(meterStyle_),"Keep imported / original");for(const auto&id:stockMeterStyles()){auto label=localized(labels_,"car_"+id);gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(meterStyle_),label.c_str());}gtk_box_pack_start(GTK_BOX(attachmentControls_),meterStyle_,false,false,0);gtk_widget_set_tooltip_text(meterStyle_,"Uses another car's native boost meter artwork for this car and all paints.");meterSample_=gtk_image_new();gtk_box_pack_start(GTK_BOX(attachmentControls_),meterSample_,false,false,0);g_signal_connect(meterStyle_,"changed",G_CALLBACK(+[](GtkComboBox*,gpointer p){auto&a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.changeMeter();});}),this);
        linkNos_=gtk_check_button_new_with_label("Link booster / NOS movement");gtk_box_pack_start(GTK_BOX(attachmentControls_),linkNos_,false,false,0);
        linkScope_=gtk_combo_box_text_new();gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(linkScope_),"All outlets");gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(linkScope_),"Selected outlet + partner");gtk_combo_box_set_active(GTK_COMBO_BOX(linkScope_),0);gtk_box_pack_start(GTK_BOX(attachmentControls_),linkScope_,false,false,0);
        linkPartner_=gtk_combo_box_text_new();gtk_box_pack_start(GTK_BOX(attachmentControls_),linkPartner_,false,false,0);gtk_widget_set_tooltip_text(linkPartner_,"Second outlet for pair movement; position only.");
        g_signal_connect(linkNos_,"toggled",G_CALLBACK(+[](GtkToggleButton*,gpointer p){static_cast<AttachmentEditor*>(p)->finish(true);}),this);
        for(auto *w:{linkScope_,linkPartner_})g_signal_connect(w,"changed",G_CALLBACK(+[](GtkComboBox*,gpointer p){static_cast<AttachmentEditor*>(p)->finish(true);}),this);
        const char *names[]={"X — left / right","Y — height","Z — front / rear","Pitch delta (degrees)","Yaw delta (degrees)","Roll delta (degrees)","Width scale","Height scale","Depth scale"};
        for(int i=0;i<9;i++){
            gtk_box_pack_start(GTK_BOX(attachmentControls_),gtk_label_new(names[i]),false,false,0);
            fields_[i]=gtk_spin_button_new_with_range(i<3?-100:(i<6?-180:.05),i<3?100:(i<6?180:20),i<3?.005:(i<6?.5:.01));
            gtk_spin_button_set_digits(GTK_SPIN_BUTTON(fields_[i]),i<3?3:2);gtk_box_pack_start(GTK_BOX(attachmentControls_),fields_[i],false,false,0);
            g_signal_connect(fields_[i],"value-changed",G_CALLBACK(+[](GtkSpinButton*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.edit();});}),this);
        }
        auto *snapButton=gtk_button_new_with_label("Snap to car");gtk_box_pack_start(GTK_BOX(attachmentControls_),snapButton,false,false,0);gtk_widget_set_tooltip_text(snapButton,"Aligns the selected element to the body. Ctrl+Space fits the local curve. Linked outlets move with it.");g_signal_connect(snapButton,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.snapToCar();});}),this);
        auto *reset=gtk_button_new_with_label("Reset attachment");gtk_box_pack_start(GTK_BOX(attachmentControls_),reset,false,false,0);
        auto *paintTabLabel=gtk_label_new("Paint");gtk_widget_set_tooltip_text(paintTabLabel,"Colors, names, prices and defaults");gtk_notebook_append_page(GTK_NOTEBOOK(notebook_),paintTab,paintTabLabel);
        auto *liveryTabLabel=gtk_label_new("Livery");gtk_widget_set_tooltip_text(liveryTabLabel,"Decals, templates and PNG artwork");gtk_notebook_append_page(GTK_NOTEBOOK(notebook_),liveryTab,liveryTabLabel);
        auto *hudTabLabel=gtk_label_new("HUD");gtk_widget_set_tooltip_text(hudTabLabel,"HUD styles and attachment placement");gtk_notebook_append_page(GTK_NOTEBOOK(notebook_),attachmentControls_,hudTabLabel);
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_),0);
        g_signal_connect(notebook_,"switch-page",G_CALLBACK(+[](GtkNotebook*,GtkWidget*,guint page,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.setPlacement(int(page)==a.hudPage_);});}),this);
        auto *right=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_widget_set_margin_start(right,8);gtk_paned_pack2(GTK_PANED(paned_),right,true,false);
        showHud_=gtk_check_button_new_with_label("Show car HUD");gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showHud_),true);gtk_box_pack_start(GTK_BOX(right),showHud_,false,false,0);gtk_widget_set_tooltip_text(showHud_,"Uses native HUD textures and current transforms. Static, depth-tested preview.");g_signal_connect(showHud_,"toggled",G_CALLBACK(+[](GtkToggleButton*b,gpointer p){auto&a=*static_cast<AttachmentEditor*>(p);a.showCarHud_=gtk_toggle_button_get_active(b);gtk_widget_queue_draw(a.canvas_);}),this);
        auto *note=gtk_label_new("Drag a marker to move • G move • R rotate (Z roll) • S scale • X/Y/Z constrain\nSpace: snap • Ctrl+Space: conform • Ctrl+Shift+Z: reset attachment\nMiddle-drag: pan • Wheel: zoom • Right-drag: orbit/elevation\nR then X: pitch • R then Y: yaw • Shift: fine • Enter: apply • ~: cancel / reset view • Ctrl+Z: undo");previewHint_=note;gtk_label_set_text(GTK_LABEL(note),"Paint + livery preview • Approximate lighting\nMiddle-drag: pan • Wheel: zoom • Right-drag: orbit/elevation\nUse the HUD tab for placement controls.");gtk_label_set_line_wrap(GTK_LABEL(note),true);gtk_box_pack_start(GTK_BOX(right),note,false,false,0);
        canvas_=gtk_drawing_area_new();gtk_widget_set_size_request(canvas_,320,260);gtk_box_pack_start(GTK_BOX(right),canvas_,true,true,0);
        gtk_widget_set_can_focus(canvas_,true);
        gtk_widget_add_events(canvas_,GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK|GDK_KEY_PRESS_MASK|GDK_FOCUS_CHANGE_MASK|GDK_SCROLL_MASK|GDK_SMOOTH_SCROLL_MASK);
        gtk_widget_set_tooltip_text(canvas_,"Click a marker to select and drag. Click the preview before using shortcuts.");
        g_signal_connect(canvas_,"button-press-event",G_CALLBACK(+[](GtkWidget*,GdkEventButton *e,gpointer p)->gboolean{auto &a=*static_cast<AttachmentEditor*>(p);bool handled=false;a.guard([&]{handled=a.press(e);});return handled;}),this);
        g_signal_connect(canvas_,"button-release-event",G_CALLBACK(+[](GtkWidget*,GdkEventButton *e,gpointer p)->gboolean{auto &a=*static_cast<AttachmentEditor*>(p);if(e->button==a.cameraDrag_){a.cameraDrag_=0;return TRUE;}if(e->button==1&&a.dragging_){a.finish(false);return TRUE;}return FALSE;}),this);
        g_signal_connect(canvas_,"motion-notify-event",G_CALLBACK(+[](GtkWidget*,GdkEventMotion *e,gpointer p)->gboolean{auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.motion(e->x,e->y,e->state&GDK_SHIFT_MASK);});return TRUE;}),this);
        g_signal_connect(dialog_,"key-press-event",G_CALLBACK(+[](GtkWidget*,GdkEventKey *e,gpointer p)->gboolean{
            if((e->state&GDK_CONTROL_MASK)&&!(e->state&GDK_MOD1_MASK)&&gdk_keyval_to_lower(e->keyval)==GDK_KEY_z){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{if(e->state&GDK_SHIFT_MASK)a.resetAttachment();else a.undo();});return TRUE;}return FALSE;
        }),this);
        g_signal_connect(canvas_,"key-press-event",G_CALLBACK(+[](GtkWidget*,GdkEventKey *e,gpointer p)->gboolean{auto &a=*static_cast<AttachmentEditor*>(p);bool handled=false;a.guard([&]{handled=a.keyPress(e->keyval,e->state);});return handled;}),this);
        g_signal_connect(canvas_,"focus-out-event",G_CALLBACK(+[](GtkWidget*,GdkEventFocus*,gpointer p)->gboolean{auto &a=*static_cast<AttachmentEditor*>(p);a.cameraDrag_=0;a.guard([&]{a.finish(true);});return FALSE;}),this);
        g_signal_connect(canvas_,"scroll-event",G_CALLBACK(+[](GtkWidget*,GdkEventScroll *e,gpointer p)->gboolean{return static_cast<AttachmentEditor*>(p)->scroll(e);}),this);
        auto *resetView=gtk_button_new_with_label("Reset view");gtk_box_pack_start(GTK_BOX(right),resetView,false,false,0);
        g_signal_connect(resetView,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.resetView();}),this);
        const char *cameraLabels[]={"Orbit","Elevation","Zoom"};
        for(int i=0;i<3;i++){
            auto *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);gtk_box_pack_start(GTK_BOX(right),row,false,false,0);gtk_box_pack_start(GTK_BOX(row),gtk_label_new(cameraLabels[i]),false,false,0);
            auto *slider=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,i==0?0:(i==1?-80:.5),i==0?360:(i==1?80:12.),i<2?1:.05);camera_[i]=slider;gtk_range_set_value(GTK_RANGE(slider),i==0?180:(i==1?26:1));gtk_widget_set_hexpand(slider,true);gtk_box_pack_start(GTK_BOX(row),slider,true,true,0);
            g_object_set_data(G_OBJECT(slider),"camera-index",GINT_TO_POINTER(i));g_signal_connect(slider,"value-changed",G_CALLBACK(+[](GtkRange *r,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);int i=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(r),"camera-index"));double v=gtk_range_get_value(r);if(i==0)a.yaw_=v*3.141592653589793/180;else if(i==1)a.pitch_=v*3.141592653589793/180;else a.zoom_=v;gtk_widget_queue_draw(a.canvas_);}),this);
        }
        status_=gtk_label_new("Edit paints, PNG liveries and names, then export a new mod folder. The import remains untouched.");gtk_label_set_line_wrap(GTK_LABEL(status_),true);gtk_label_set_selectable(GTK_LABEL(status_),true);gtk_box_pack_start(GTK_BOX(right),status_,false,false,6);
        g_signal_connect(car_,"changed",G_CALLBACK(+[](GtkComboBox*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.selectCar();});}),this);
        g_signal_connect(node_,"changed",G_CALLBACK(+[](GtkComboBox*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.selectNode();});}),this);
        g_signal_connect(paint_,"changed",G_CALLBACK(+[](GtkComboBox *w,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);auto id=gtk_combo_box_get_active_id(w);if(id)a.guard([&]{a.paintIndex_=std::stoul(id);a.refreshPaint();});}),this);
        g_signal_connect(reset,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.resetAttachment();});}),this);
        g_signal_connect(exportButton,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.exportCopy();});}),this);
        g_signal_connect(canvas_,"draw",G_CALLBACK(+[](GtkWidget *w,cairo_t *cr,gpointer p)->gboolean{auto &a=*static_cast<AttachmentEditor*>(p);a.guard([&]{a.drawPreview(cr,gtk_widget_get_allocated_width(w),gtk_widget_get_allocated_height(w));});return FALSE;}),this);
        for(const auto &entry:mod_.cars){auto label=localized(labels_,"car_"+entry.first);gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(car_),entry.first.c_str(),label.c_str());}
        gtk_combo_box_set_active(GTK_COMBO_BOX(car_),0);
        const std::string prefix="assets/graphics/vehicles/";for(const auto &file:mod_.files){auto path=lower(file.first);if(path.rfind(prefix,0)==0&&path.size()==prefix.size()+3+17&&path.substr(path.size()-17)=="/lod0_livery.bdae"){gtk_combo_box_set_active_id(GTK_COMBO_BOX(car_),path.substr(prefix.size(),3).c_str());break;}}
        gtk_widget_show_all(dialog_);
    }
    void run(GtkWindow *parent){build(parent);if(updates_)updates_->autoCheck();for(;;){gtk_dialog_run(GTK_DIALOG(dialog_));if(!unsaved_)break;auto *q=gtk_message_dialog_new(GTK_WINDOW(dialog_),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,"Discard adjustments that have not been exported?");int answer=gtk_dialog_run(GTK_DIALOG(q));gtk_widget_destroy(q);if(answer==GTK_RESPONSE_YES)break;}gtk_window_get_size(GTK_WINDOW(dialog_),&windowWidth,&windowHeight);maximized=gtk_window_is_maximized(GTK_WINDOW(dialog_));gtk_widget_destroy(dialog_);dialog_=nullptr;}
    void smokeMigration(const fs::path &out){
        require(legacyOrigin_,"Migration fixture already has origin metadata");auto missing=mod_.unresolvedOrigins();auto incompatible=mod_.incompatibleOrigins();
        require(!missing.empty()||!incompatible.empty(),"Migration fixture needs an unknown or incompatible car ID");
        build(nullptr);require(!gtk_widget_get_sensitive(exportButton_),"Incomplete legacy metadata allowed export");
        fs::create_directories(out);
        auto identity=mod_.originMetadata().packageId();for(const auto &id:missing)gtk_combo_box_set_active_id(GTK_COMBO_BOX(migrationDonors_.at(id)),"alf");
        require(mod_.unresolvedOrigins().empty(),"Choosing donors left an authored car unresolved");
        if(!incompatible.empty()){
            require(!gtk_widget_get_sensitive(exportButton_),"Incompatible car ID allowed export");
            bool rejected=false;try{mod_.exportCopy(out/"incompatible");}catch(const std::exception&){rejected=true;}
            require(rejected&&!fs::exists(out/"incompatible"),"Incompatible car ID exported without validation");
            require(std::string(gtk_label_get_text(GTK_LABEL(migrationInfo_))).find(incompatible.front())!=std::string::npos,"Migration notice omitted the incompatible car ID");
        } else {
            require(gtk_widget_get_sensitive(exportButton_),"Choosing donors did not enable export");
            mod_.exportCopy(out/"replace");gtk_combo_box_set_active_id(GTK_COMBO_BOX(exportMode_),"add");mod_.exportCopy(out/"add");
            for(const auto &mode:{"replace","add"}){ImportedMod check(out/mode,&stock_);auto origin=check.originMetadata();require(origin.packageId()==identity&&origin.overwrite()==(std::string(mode)=="replace"),"Migration identity or policy lost");for(const auto &id:missing)require(origin.mappings().at(id)=="alf","Migration donor lost");}
        }
        require(std::none_of(fs::directory_iterator(mod_.source),fs::directory_iterator(),[](const auto &e){return lower(e.path().filename().u8string())=="origin.txt";}),"Migration modified input");
        for(int wait=0;wait<30;wait++){while(gtk_events_pending())gtk_main_iteration();g_usleep(1000);}auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gtk_widget_get_allocated_width(dialog_),gtk_widget_get_allocated_height(dialog_));auto *cr=cairo_create(surface);gtk_widget_draw(dialog_,cr);cairo_destroy(cr);cairo_surface_write_to_png(surface,(out/"migration-editor.png").u8string().c_str());cairo_surface_destroy(surface);gtk_widget_destroy(dialog_);dialog_=nullptr;
        std::cout<<(incompatible.empty()?"PASS legacy import, explicit GUI donor selection, export gating, both modes, stable identity and source preservation":"PASS legacy runtime-slot import, actionable gating and unchanged source")<<"\n";
    }
    void smokeUnified(const fs::path &out){
        require(mod_.files.empty(),"Unified stock session includes imported files");build(nullptr);require(!placement_&&gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_))==3&&gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_))==0,"Sidebar tabs or startup page are incorrect");fs::create_directories(out);
        for(int wait=0;wait<30;wait++){while(gtk_events_pending())gtk_main_iteration();g_usleep(1000);}int widePreview=gtk_widget_get_allocated_width(canvas_);gtk_paned_set_position(GTK_PANED(paned_),380);for(int wait=0;wait<30;wait++){while(gtk_events_pending())gtk_main_iteration();g_usleep(1000);}int narrowPreview=gtk_widget_get_allocated_width(canvas_);require(narrowPreview<widePreview,"Preview divider did not resize the viewport");gtk_paned_set_position(GTK_PANED(paned_),299);for(int wait=0;wait<30;wait++){while(gtk_events_pending())gtk_main_iteration();g_usleep(1000);}require(gtk_widget_get_allocated_width(canvas_)>narrowPreview,"Preview divider did not restore the viewport");
        require(gtk_combo_box_get_active(GTK_COMBO_BOX(multiplierStyle_))==2,"Pulse bracket style not detected");gtk_combo_box_set_active(GTK_COMBO_BOX(multiplierStyle_),1);require(hudEdits_.count("alf")&&identifyMultiplierStyle(stock_,hudEdits_.at("alf"))==0,"Four-bracket selection failed");mod_.exportCopy(out/"brackets-only",extraFiles());ImportedMod bracketRoundtrip(out/"brackets-only",&stock_);require(bracketRoundtrip.cars.count("alf")&&identifyMultiplierStyle(stock_,bracketRoundtrip.assetBytes(hudTexturePath("alf"),stock_))==0,"HUD-only export/reimport failed");gtk_combo_box_set_active(GTK_COMBO_BOX(car_),1);gtk_combo_box_set_active_id(GTK_COMBO_BOX(car_),"alf");require(gtk_combo_box_get_active(GTK_COMBO_BOX(multiplierStyle_))==1,"Switching cars lost bracket choice");gtk_combo_box_set_active(GTK_COMBO_BOX(multiplierStyle_),0);require(!hudEdits_.count("alf"),"Keep original failed to restore source atlas");gtk_combo_box_set_active(GTK_COMBO_BOX(multiplierStyle_),1);
        auto &meterStyles=stockMeterStyles();int bolt=int(std::distance(meterStyles.begin(),std::find(meterStyles.begin(),meterStyles.end(),"hon")));require(bolt>0,"Bolt meter style missing");require(gtk_combo_box_get_active(GTK_COMBO_BOX(meterStyle_))==1,"Boost meter shape not detected");gtk_combo_box_set_active(GTK_COMBO_BOX(meterStyle_),bolt+1);require(hudEdits_.count("alf")&&identifyMeterStyle(stock_,hudEdits_.at("alf"))==bolt&&identifyMultiplierStyle(stock_,hudEdits_.at("alf"))==0,"Boost meter selection or bracket retention failed");mod_.exportCopy(out/"meter-hud",extraFiles());ImportedMod meterRoundtrip(out/"meter-hud",&stock_);require(meterRoundtrip.cars.count("alf")&&identifyMeterStyle(stock_,meterRoundtrip.assetBytes(hudTexturePath("alf"),stock_))==bolt&&identifyMultiplierStyle(stock_,meterRoundtrip.assetBytes(hudTexturePath("alf"),stock_))==0,"Meter HUD export/reimport failed");gtk_combo_box_set_active_id(GTK_COMBO_BOX(car_),"bui");gtk_combo_box_set_active_id(GTK_COMBO_BOX(car_),"alf");require(gtk_combo_box_get_active(GTK_COMBO_BOX(meterStyle_))==bolt+1,"Switching cars lost meter shape");gtk_combo_box_set_active(GTK_COMBO_BOX(meterStyle_),0);require(hudEdits_.count("alf")&&identifyMeterStyle(stock_,hudEdits_.at("alf"))==0&&identifyMultiplierStyle(stock_,hudEdits_.at("alf"))==0,"Keep original meter lost bracket choice");gtk_combo_box_set_active(GTK_COMBO_BOX(multiplierStyle_),0);require(!hudEdits_.count("alf"),"Keep original did not clear the composed HUD");std::cout<<"PASS boost meter shape selection, preview, export and retention with multiplier choice\n";
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(unlockMode_),"artist-title");require(!gtk_widget_get_sensitive(carPrice_)&&gtk_widget_get_sensitive(unlockTitle_),"Music unlock field sensitivity");
        gtk_entry_set_text(GTK_ENTRY(unlockArtist_),"GUI Artist");gtk_entry_set_text(GTK_ENTRY(unlockTitle_),"GUI Title");gtk_spin_button_set_value(GTK_SPIN_BUTTON(packPrice_),321);gtk_button_clicked(GTK_BUTTON(unlockApply_));require(unlocks_->get("alf").value("songUnlock",std::string())=="GUI Title"&&colors_->price(paintSelection())==321,"Unlock/price button did not apply");
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(car_),"bui");require(gtk_widget_get_sensitive(carPrice_)&&!gtk_widget_get_sensitive(unlockTitle_),"Shop price field sensitivity");gtk_combo_box_set_active_id(GTK_COMBO_BOX(car_),"alf");require(std::string(gtk_entry_get_text(GTK_ENTRY(unlockTitle_)))=="GUI Title"&&gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(packPrice_))==321,"Switching cars lost unlock/price edits");
        auto p=paintSelection();require(p.car=="alf","Stock car selection failed");require(exportFileName(p,"uv")=="pulse-uv.png"&&exportFileName(p,"template")=="pulse-template.png"&&exportFileName(p,"test")=="pulse-test.png"&&exportFileName(p,"livery")=="pulse-arctic-livery.png","Car-name export filenames failed");auto source=appearance_->imageTemplate(p);
        {require(gtk_widget_get_sensitive(addDecalButton_),"Export livery template disabled");require(std::string(gtk_button_get_label(GTK_BUTTON(addDecalButton_)))=="Export livery template","Livery template label changed");auto guidePath=out/"livery-template.png";exportLiveryTemplate(guidePath);require(!appearance_->decal(p),"Exporting a livery template changed the livery");auto guide=loadPNG(fs::u8path(guidePath.u8string()),source.texture.width,source.texture.height);size_t clearPixels=0,inkPixels=0;bool cyan=false;for(size_t i=0;i<guide.pixels.size();i+=4){if(guide.pixels[i+3]==0){clearPixels++;continue;}inkPixels++;if(guide.pixels[i+2]>guide.pixels[i]+40&&guide.pixels[i+1]>guide.pixels[i]+40)cyan=true;}require(clearPixels>inkPixels&&inkPixels>0&&cyan,"Livery template is missing its transparent layer or UV outlines");}
        {require(!materialTargets_.empty(),"No material textures exposed");auto target=materialTarget();auto materialPng=out/"material.png";materialPNG(true,materialPng);require(fs::is_regular_file(materialPng),"Material PNG export failed");materialPNG(false,materialPng);auto materialUv=out/"material-uv.png";materialUV(materialUv);auto guide=loadPNG(materialUv,target.width,target.height);size_t ink=0;for(size_t i=0;i<guide.pixels.size();i+=4)if(guide.pixels[i+3])ink++;require(ink>0,"Material UV template is empty");}
        appearance_->importPNG(p,diagnostic(source.texture));colors_->set(p,"col0",{25,50,75});refreshPaint();
        auto templatePath=out/"car-template-test";saveCarTemplateTo(templatePath);auto templateCheck=loadCarTemplate(templatePath);require(templateCheck.manifest.value("authored_id","")==p.car&&templateCheck.assets.count("scripts/config/paint_pc.cfg"),"Car template save failed");auto savedColor=colors_->get(p,"col0",3);colors_->set(p,"col0",{1,2,3});refreshPaint();applyCarTemplateFrom(templatePath);require(colors_->get(p,"col0",3)==savedColor,"Car template appearance was not reapplied");
        auto paintFiles=paintModFiles(p);require(!paintFiles.empty(),"Paint-only export has no files");for(const auto &entry:paintFiles)require(entry.first.rfind("graphics/vehicles/"+p.car+"/",0)!=0,"Paint-only export included the car model");auto liveryOnly=paintModFiles(p,false);require(!liveryOnly.empty()&&liveryOnly.count("scripts/config/paint_pc.cfg")==0,"Livery-only export included paint configuration");auto paintMeta=publishPaintMod(paintFiles,{{"test","paint-only"}},out/"paint-only-mod");require(!fs::exists(out/"paint-only-mod/origin.txt")&&fs::exists(out/"paint-only-mod/livery-maker.json")&&fs::is_directory(out/"paint-only-mod/assets"),"Paint-only package layout failed");require(paintMeta["tool"]=="reriff-livery-maker-v1"&&!paintMeta.contains("origin_sha256")&&!paintMeta.contains("dat_sha256"),"Paint-only package metadata failed");auto liveryMeta=publishPaintMod(liveryOnly,{{"paint_configuration_modified",false}},out/"livery-only-mod");require(!fs::exists(out/"livery-only-mod/assets/scripts/config/paint_pc.cfg")&&fs::exists(out/"livery-only-mod/livery-maker.json"),"Livery-only package included paint configuration");require(liveryMeta["paint_configuration_modified"]==false,"Livery-only metadata changed");publishPaintMod(liveryOnly,{{"paint_configuration_modified",false}},out/"livery-only-mod");
        gtk_combo_box_set_active(GTK_COMBO_BOX(car_),1);gtk_combo_box_set_active_id(GTK_COMBO_BOX(car_),p.car.c_str());require(colors_->get(p,"col0",3)==std::vector<float>({25,50,75})&&appearance_->decal(p),"Switching stock cars lost edits");
        for(int mode=0;mode<5;mode++){gtk_combo_box_set_active(GTK_COMBO_BOX(viewMode_),mode);auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,900,600);auto *cr=cairo_create(surface);drawPreview(cr,900,600);cairo_destroy(cr);require(cairo_surface_write_to_png(surface,(out/("view-"+std::to_string(mode)+".png")).u8string().c_str())==CAIRO_STATUS_SUCCESS,"Preview export failed");cairo_surface_destroy(surface);}
        gtk_combo_box_set_active(GTK_COMBO_BOX(viewMode_),0);auto *templateSurface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,source.texture.width,source.texture.height);auto *templateCr=cairo_create(templateSurface);uv(templateCr,source.texture.width,source.texture.height);cairo_destroy(templateCr);require(cairo_surface_write_to_png(templateSurface,(out/"uv-template.png").u8string().c_str())==CAIRO_STATUS_SUCCESS,"UV export failed");cairo_surface_destroy(templateSurface);
        mod_.exportCopy(out/"stock-mod",extraFiles());ImportedMod roundtrip(out/"stock-mod",&stock_);ModAppearance appearance(roundtrip,stock_,labels_);require(appearance.colors->get(p,"col0",3)==std::vector<float>({25,50,75})&&appearance.decal(p),"Stock export/reimport failed");
        require(roundtrip.originMetadata().overwrite(),"Stock editor did not default to replacement");
        auto identity=roundtrip.originMetadata().packageId();gtk_combo_box_set_active_id(GTK_COMBO_BOX(exportMode_),"add");
        mod_.exportCopy(out/"new-car-mod",extraFiles());ImportedMod newCar(out/"new-car-mod",&stock_);
        require(!newCar.originMetadata().overwrite()&&newCar.originMetadata().packageId()==identity,"GUI export choice or package identity lost");
        require(newCar.originMetadata().mappings().size()==1&&newCar.originMetadata().mappings().at("alf")=="alf","GUI registered untouched cars from shared config");
        AttachmentEditor reopened(stock_,out/"new-car-mod");reopened.build(nullptr);require(std::string(gtk_combo_box_get_active_id(GTK_COMBO_BOX(reopened.exportMode_)))=="add","Imported export choice not shown");
        require(reopened.fallbackCars_.count("alf"),"Fallback selector missing for an authored car");auto *fallbackCombo=GTK_WIDGET(reopened.fallbackCars_.at("alf"));
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(fallbackCombo),"fab");require(reopened.mod_.originMetadata().fallbacks().at("alf")=="fab","Fallback selector did not update metadata");
        reopened.mod_.exportCopy(out/"fallback-mod",reopened.extraFiles());ImportedMod fallbackCheck(out/"fallback-mod",&stock_);
        require(fallbackCheck.originMetadata().fallbacks().at("alf")=="fab"&&fallbackCheck.originMetadata().mappings().at("alf")=="alf","Fallback export changed donor or lost fallback");
        gtk_combo_box_set_active(GTK_COMBO_BOX(fallbackCombo),0);require(reopened.mod_.originMetadata().fallbacks().empty(),"Fallback reset did not restore donor fallback");
        gtk_widget_destroy(reopened.dialog_);reopened.dialog_=nullptr;
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(exportMode_),"replace");
        UnlockConfig importedUnlocks(roundtrip,stock_);require(importedUnlocks.get("alf").value("songUnlock",std::string())=="GUI Title"&&appearance.colors->price(p)==321,"GUI unlock/price export failed");gtk_expander_set_expanded(GTK_EXPANDER(gtk_widget_get_parent(gtk_widget_get_parent(unlockMode_))),true);
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_),hudPage_);
        for(int wait=0;wait<30;wait++){while(gtk_events_pending())gtk_main_iteration();g_usleep(1000);}auto *placement=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gtk_widget_get_allocated_width(attachmentControls_),gtk_widget_get_allocated_height(attachmentControls_));auto *placementCr=cairo_create(placement);gtk_widget_draw(attachmentControls_,placementCr);cairo_destroy(placementCr);require(cairo_surface_write_to_png(placement,(out/"placement-controls.png").u8string().c_str())==CAIRO_STATUS_SUCCESS,"Placement screenshot failed");cairo_surface_destroy(placement);gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_),0);
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_),1);
        for(int wait=0;wait<30;wait++){while(gtk_events_pending())gtk_main_iteration();g_usleep(1000);}auto *liverySurface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gtk_widget_get_allocated_width(dialog_),gtk_widget_get_allocated_height(dialog_));auto *liveryCr=cairo_create(liverySurface);gtk_widget_draw(dialog_,liveryCr);cairo_destroy(liveryCr);require(cairo_surface_write_to_png(liverySurface,(out/"livery-tab.png").u8string().c_str())==CAIRO_STATUS_SUCCESS,"Livery tab screenshot failed");cairo_surface_destroy(liverySurface);gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_),0);
        for(int wait=0;wait<30;wait++){while(gtk_events_pending())gtk_main_iteration();g_usleep(1000);}auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gtk_widget_get_allocated_width(dialog_),gtk_widget_get_allocated_height(dialog_));auto *cr=cairo_create(surface);gtk_widget_draw(dialog_,cr);cairo_destroy(cr);cairo_surface_write_to_png(surface,(out/"unified-editor.png").u8string().c_str());cairo_surface_destroy(surface);gtk_widget_destroy(dialog_);dialog_=nullptr;
        std::cout<<"PASS unified stock startup, source controls, five preview modes, test livery, UV template, car switching and export/reimport\n";
    }
    // Actual widget signal path, used by the packaged GUI smoke check.
    void smokeImportedNames(const fs::path &out){
        require(allLocaleNames_,"Imported car did not retain its all-language name policy");build(nullptr);std::string car=currentCar_;gtk_entry_set_text(GTK_ENTRY(carName_),"Imported Locale Test");applyNames();
        fs::create_directories(out);mod_.exportCopy(out/"exported",extraFiles());ImportedMod exported(out/"exported",&stock_);
        for(const char *locale:{"us","de","ru"}){auto names=displayNamesFrom([&](const std::string &path){return exported.assetBytes(path,stock_);},locale);addCarNameAliases(names,exported.originMetadata().mappings());require(localized(names,"car_"+car)=="Imported Locale Test",std::string("Name edit missed locale ")+locale);}
        gtk_widget_destroy(dialog_);dialog_=nullptr;std::cout<<"PASS imported car name edit updates every exported locale\n";
    }
    void smoke(GtkWindow *parent,const fs::path &output){build(parent);require(!placement_&&gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_))==0,"HUD tab opened automatically");
        {GList *items=gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(addDecalButton_))),*node=items;int index=0,clearIndex=-1,importIndex=-1;for(;node;node=node->next,++index){auto *child=GTK_WIDGET(node->data);if(child==addDecalButton_)clearIndex=index;if(GTK_IS_BUTTON(child)){const char *label=gtk_button_get_label(GTK_BUTTON(child));if(label&&std::string(label)=="Import PNG livery")importIndex=index;}}g_list_free(items);require(std::string(gtk_button_get_label(GTK_BUTTON(addDecalButton_)))=="Export livery template","Livery template label changed");require(importIndex>=0&&clearIndex==importIndex+1,"Livery template is not directly below import livery");}gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_),hudPage_);require(placement_,"HUD tab did not enable placement");if(mod_.cars.count("dod"))gtk_combo_box_set_active_id(GTK_COMBO_BOX(car_),"dod");
        for(int wait=0;wait<30;wait++){while(gtk_events_pending())gtk_main_iteration();g_usleep(1000);}
        auto *initial=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gtk_widget_get_allocated_width(canvas_),gtk_widget_get_allocated_height(canvas_));auto *initialCr=cairo_create(initial);gtk_widget_draw(canvas_,initialCr);cairo_destroy(initialCr);cairo_surface_destroy(initial);
        // Drive the connected GTK handlers, not a separate transform implementation.
        auto key=[&](guint k,guint state=0){GdkEventKey event{};event.type=GDK_KEY_PRESS;event.keyval=k;event.state=state;gboolean handled=false;g_signal_emit_by_name(canvas_,"key-press-event",&event,&handled);require(handled,"Shortcut not handled");};
        auto move=[&](double x,double y,guint state=0){GdkEventMotion event{};event.type=GDK_MOTION_NOTIFY;event.x=x;event.y=y;event.state=state;gboolean handled=false;g_signal_emit_by_name(canvas_,"motion-notify-event",&event,&handled);};
        auto click=[&](double x,double y,bool release,guint button=1){GdkEventButton event{};event.type=release?GDK_BUTTON_RELEASE:GDK_BUTTON_PRESS;event.button=button;event.x=x;event.y=y;gboolean handled=false;g_signal_emit_by_name(canvas_,release?"button-release-event":"button-press-event",&event,&handled);};
        auto original=values_.at(this->key());auto m=nodes_.nodes.at(selected_).matrix;auto point=projection_.project({m[9],m[10],m[11]});
        click(point[0],point[1],false);require(dragging_,"Marker picking failed");move(point[0]+30,point[1]-20);click(point[0]+30,point[1]-20,true);
        auto moved=values_.at(this->key());require(moved!=original&&!operation_,"Drag failed: operation="+std::to_string(int(operation_))+" original="+std::to_string(original[0])+","+std::to_string(original[1])+" moved="+std::to_string(moved[0])+","+std::to_string(moved[1])+" projection="+std::to_string(projection_.scale));auto end=projection_.project({F(moved[0]),F(moved[1]),F(moved[2])});require(std::abs(end[0]-point[0]-30)<.1&&std::abs(end[1]-point[1]+20)<.1,"Dragging does not follow pointer");
        key(GDK_KEY_g);key(GDK_KEY_x);move(mouseX_+40,mouseY_+10);auto constrained=values_.at(this->key());require(constrained[1]==moved[1]&&constrained[2]==moved[2]&&constrained[0]!=moved[0],"Axis constraint failed");key(GDK_KEY_Escape);require(values_.at(this->key())==moved,"Cancel failed");
        for(int axis=0;axis<3;axis++){key(GDK_KEY_r);key(axis==0?GDK_KEY_x:axis==1?GDK_KEY_y:GDK_KEY_z);move(mouseX_+20,mouseY_);require(values_.at(this->key())[3+axis]!=moved[3+axis],"Rotation failed");key(GDK_KEY_Escape);}
        key(GDK_KEY_s);key(GDK_KEY_x);move(mouseX_+20,mouseY_);require(values_.at(this->key())[6]>moved[6]&&values_.at(this->key())[7]==moved[7],"Scale constraint failed");key(GDK_KEY_Escape);
        key(GDK_KEY_r);move(mouseX_+20,mouseY_,GDK_SHIFT_MASK);require(std::abs(values_.at(this->key())[5]-moved[5]-1)<.001,"Fine adjustment failed");key(GDK_KEY_Return);require(!operation_,"Confirm failed");
        auto unchanged=values_.at(this->key());double oldYaw=yaw_,oldPitch=pitch_;
        click(200,200,false,2);move(240,225);click(240,225,true,2);require(panX_==40&&panY_==25&&!cameraDrag_,"Pan failed");
        click(200,200,false,3);move(230,220);click(230,220,true,3);require(yaw_!=oldYaw&&pitch_!=oldPitch&&!cameraDrag_,"Orbit/elevation failed");auto anchored=projection_.project(orbitPivot_);require(std::abs(anchored[0]-200)<.01&&std::abs(anchored[1]-200)<.01,"Mouse orbit pivot drifted");
        GdkEventScroll wheel{};wheel.type=GDK_SCROLL;wheel.direction=GDK_SCROLL_UP;gboolean handled=false;double oldZoom=zoom_;g_signal_emit_by_name(canvas_,"scroll-event",&wheel,&handled);require(handled&&zoom_>oldZoom,"Wheel zoom failed");
        wheel.direction=GDK_SCROLL_SMOOTH;wheel.delta_y=2;g_signal_emit_by_name(canvas_,"scroll-event",&wheel,&handled);require(zoom_<oldZoom,"Smooth scroll zoom failed");require(values_.at(this->key())==unchanged,"Camera changed attachment");
        key(GDK_KEY_g);move(mouseX_+20,mouseY_);key(GDK_KEY_asciitilde);require(!operation_&&!cameraDrag_&&values_.at(this->key())==unchanged,"Tilde cancel failed");key(GDK_KEY_grave);require(panX_==0&&panY_==0&&zoom_==1&&std::abs(yaw_-3.141592653589793)<.001,"Tilde view reset failed");
        wheel.direction=GDK_SCROLL_UP;
        for(guint button:{2u,3u}){click(210,210,false,button);oldZoom=zoom_;wheel.x=210;wheel.y=210;g_signal_emit_by_name(canvas_,"scroll-event",&wheel,&handled);require(zoom_>oldZoom&&cameraDrag_==button,"Zoom blocked camera drag");move(220,215);click(220,215,true,button);}
        key(GDK_KEY_g);move(mouseX_+20,mouseY_+10);auto grabbed=values_.at(this->key());oldZoom=zoom_;wheel.x=mouseX_;wheel.y=mouseY_;g_signal_emit_by_name(canvas_,"scroll-event",&wheel,&handled);require(zoom_>oldZoom&&operation_=='g',"Zoom blocked attachment move");move(mouseX_,mouseY_);require(values_.at(this->key())==grabbed,"Zoom made attachment jump");move(mouseX_+10,mouseY_);require(values_.at(this->key())!=grabbed,"Drag stopped after zoom");key(GDK_KEY_asciitilde);require(values_.at(this->key())==unchanged,"Zoom lost cancel baseline");
        auto undoKey=[&]{GdkEventKey event{};event.type=GDK_KEY_PRESS;event.keyval=GDK_KEY_z;event.state=GDK_CONTROL_MASK;gboolean used=false;g_signal_emit_by_name(dialog_,"key-press-event",&event,&used);require(used,"Ctrl+Z not handled");};
        auto undoBaseline=values_.at(this->key());size_t count=undo_.size();key(GDK_KEY_g);move(mouseX_+10,mouseY_);move(mouseX_+20,mouseY_);key(GDK_KEY_Return);require(undo_.size()==count+1,"Drag not grouped into one undo step");undoKey();require(values_.at(this->key())==undoBaseline&&undo_.size()==count,"Ctrl+Z failed to undo complete drag");
        key(GDK_KEY_r);move(mouseX_+20,mouseY_);undoKey();require(values_.at(this->key())==undoBaseline&&undo_.size()==count&&!operation_,"Ctrl+Z active adjustment cancellation failed");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(fields_[1]),undoBaseline[1]+.1);undoKey();require(values_.at(this->key())==undoBaseline,"Numeric edit undo failed");
        std::cout<<"PASS Ctrl+Z grouped drag, numeric edit and active adjustment cancellation\n";
        std::cout<<"PASS simultaneous zoom with pan, orbit and attachment movement; no jump and full cancellation\n";
        std::cout<<"PASS middle pan, right orbit/elevation, wheel/smooth zoom and camera/attachment isolation\n";
        std::cout<<"PASS marker drag, projected movement, axis locks, pitch/yaw/roll, scale, fine adjustment, cancel and confirm\n";
        auto originalNode=selected_;std::vector<size_t> outlets;for(size_t i=0;i<nodes_.nodes.size();i++)if(nodes_.nodes[i].name.rfind("nos",0)==0)outlets.push_back(i);
        require(outlets.size()>=4,"Fixture needs four outlets");gtk_combo_box_set_active_id(GTK_COMBO_BOX(node_),std::to_string(outlets[0]).c_str());gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(linkNos_),true);auto linkedBefore=snapshot();
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(fields_[0]),nodeValues(selected_)[0]+.1);for(auto node:outlets)require(std::abs(nodeValues(node)[0]-linkedBefore.at(node)[0]-.1)<1e-6,"Linked all movement failed");undoKey();require(snapshot()==linkedBefore,"Group undo failed");
        gtk_combo_box_set_active(GTK_COMBO_BOX(linkScope_),1);gtk_combo_box_set_active_id(GTK_COMBO_BOX(linkPartner_),std::to_string(outlets[1]).c_str());key(GDK_KEY_g);key(GDK_KEY_y);move(mouseX_,mouseY_-20);require(nodeValues(outlets[0])!=linkedBefore.at(outlets[0])&&nodeValues(outlets[1])!=linkedBefore.at(outlets[1])&&nodeValues(outlets[2])==linkedBefore.at(outlets[2]),"Pair movement failed");key(GDK_KEY_asciitilde);require(snapshot()==linkedBefore,"Pair cancellation failed");
        key(GDK_KEY_g);move(mouseX_+15,mouseY_);key(GDK_KEY_Return);undoKey();require(snapshot()==linkedBefore,"Pair drag undo failed");gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(linkNos_),false);gtk_combo_box_set_active_id(GTK_COMBO_BOX(node_),std::to_string(originalNode).c_str());
        std::cout<<"PASS linked all outlets, explicit pair, preserved spacing, group undo and cancel\n";
        auto beforeSnap=snapshot();snapToCar();auto snapped=nodeValues(selected_);require(snapped!=beforeSnap.at(selected_),"Snap did not change placement");undoKey();require(snapshot()==beforeSnap,"Snap undo failed");std::cout<<"PASS exterior snap and undo\n";
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(linkNos_),true);gtk_combo_box_set_active(GTK_COMBO_BOX(linkScope_),0);gtk_combo_box_set_active_id(GTK_COMBO_BOX(node_),std::to_string(outlets[0]).c_str());auto linkedSnapBefore=snapshot();auto linkedSelectedBefore=nodeValues(selected_);snapToCar();auto linkedSelectedAfter=nodeValues(selected_);require(linkedSelectedAfter!=linkedSelectedBefore,"Linked snap did not move the selected outlet");double snapDX=linkedSelectedAfter[0]-linkedSelectedBefore[0],snapDY=linkedSelectedAfter[1]-linkedSelectedBefore[1],snapDZ=linkedSelectedAfter[2]-linkedSelectedBefore[2];require(std::abs(snapDX)+std::abs(snapDY)+std::abs(snapDZ)>1e-9,"Linked snap produced no translation");for(auto node:outlets){auto after=nodeValues(node);require(std::abs(after[0]-linkedSnapBefore.at(node)[0]-snapDX)<1e-6&&std::abs(after[1]-linkedSnapBefore.at(node)[1]-snapDY)<1e-6&&std::abs(after[2]-linkedSnapBefore.at(node)[2]-snapDZ)<1e-6,"Linked outlet did not follow the snap translation");}undoKey();require(snapshot()==linkedSnapBefore,"Linked snap undo failed");std::cout<<"PASS linked snap moves the whole booster group and undoes it\n";gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(linkNos_),false);
        auto beforeSpace=snapshot();key(GDK_KEY_space);auto spaceSnapped=nodeValues(selected_);require(spaceSnapped!=beforeSpace.at(selected_),"Space snap did not change placement");undoKey();require(snapshot()==beforeSpace,"Space snap undo failed");
        key(GDK_KEY_space,GDK_CONTROL_MASK);auto conformed=nodeValues(selected_);require(conformed!=beforeSpace.at(selected_),"Ctrl+Space conform did not change placement");undoKey();require(snapshot()==beforeSpace,"Ctrl+Space undo failed");
        std::cout<<"PASS Space snap and Ctrl+Space conform\n";
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(fields_[1]),nodeValues(selected_)[1]+.1);auto editedBeforeReset=snapshot();auto imported=nodeValues(selected_);gboolean resetUsed=false;GdkEventKey resetEvent{};resetEvent.type=GDK_KEY_PRESS;resetEvent.keyval=GDK_KEY_z;resetEvent.state=GDK_CONTROL_MASK|GDK_SHIFT_MASK;g_signal_emit_by_name(dialog_,"key-press-event",&resetEvent,&resetUsed);require(resetUsed,"Ctrl+Shift+Z not handled");auto afterReset=nodeValues(selected_);require(afterReset[1]!=imported[1],"Ctrl+Shift+Z did not reset the attachment");undoKey();require(snapshot()==editedBeforeReset,"Reset was not undoable");std::cout<<"PASS Ctrl+Shift+Z reset and undo\n";
        auto selectedPaint=paintSelection();GdkRGBA color{.2,.4,.6,1};gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(paintColors_[0].second),&color);g_signal_emit_by_name(paintColors_[0].second,"color-set");require(colors_->get(selectedPaint,"col0",3)==std::vector<float>({51,102,153}),"Imported color control failed");
        auto templateDecal=appearance_->imageTemplate(selectedPaint);appearance_->importPNG(selectedPaint,diagnostic(templateDecal.texture));refreshPaint();auto decalId=appearance_->decal(selectedPaint)->name;
        gtk_entry_set_text(GTK_ENTRY(carName_),"Impala Customs Test");gtk_entry_set_text(GTK_ENTRY(paintName_),"Custom Test Paint");applyNames();
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(paint_),"1");gtk_combo_box_set_active_id(GTK_COMBO_BOX(paint_),std::to_string(selectedPaint.packIndex).c_str());require(appearance_->decal(selectedPaint)->name==decalId&&colors_->get(selectedPaint,"col0",3)==std::vector<float>({51,102,153}),"Paint selection lost appearance edits");
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_),0);require(!placement_,"HUD placement did not close");
        std::cout<<"PASS imported color control, PNG retention, name editing and opt-in placement UI\n";
        auto templatePath=output.parent_path()/(output.filename().u8string()+"-template");saveCarTemplateTo(templatePath);auto templateCheck=loadCarTemplate(templatePath);require(templateCheck.manifest.at("import_recipe").is_object()&&templateCheck.manifest.at("source_sha256").get<std::string>().size()==64,"Imported car template recipe missing");require(!templateCheck.manifest.contains("source_path")&&!templateCheck.manifest.contains("source_glb"),"Template stored a source path or GLB");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(fields_[1]),gtk_spin_button_get_value(GTK_SPIN_BUTTON(fields_[1]))+.05);require(mod_.cars.at(currentCar_)->changed(),"GUI edit did not reach writer");mod_.exportCopy(output,extraFiles());
        for(int wait=0;wait<30;wait++){while(gtk_events_pending())gtk_main_iteration();g_usleep(1000);}
        auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gtk_widget_get_allocated_width(dialog_),gtk_widget_get_allocated_height(dialog_));auto *cr=cairo_create(surface);gtk_widget_draw(dialog_,cr);cairo_destroy(cr);cairo_surface_write_to_png(surface,(output.u8string()+".png").c_str());cairo_surface_destroy(surface);
        gtk_widget_destroy(dialog_);dialog_=nullptr;}
};
}
