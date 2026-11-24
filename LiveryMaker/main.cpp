#include "attachment_editor.hpp"
#include "merge_livery.hpp"
#include <gtk/gtk.h>
#include <memory>
using namespace livery;
static void saveSurface(cairo_surface_t *surface,const fs::path &path) {
    Bytes bytes;
    auto status=cairo_surface_write_to_png_stream(surface,+[](void *closure,const unsigned char *data,unsigned length) {
        try {auto &out=*static_cast<Bytes*>(closure);out.insert(out.end(),data,data+length);return CAIRO_STATUS_SUCCESS;}
        catch(...) {return CAIRO_STATUS_WRITE_ERROR;}
    },&bytes);
    require(status==CAIRO_STATUS_SUCCESS,"Cannot encode PNG surface");write(path,bytes);
}
#ifdef _WIN32
#include <glib/gwin32.h>
#include <glib/gstdio.h>
struct RuntimeCache {
    gchar *path=nullptr;
    ~RuntimeCache(){if(path){g_remove(path);g_free(path);}}
};
static void prepareRuntime() {
    gchar *directory=g_win32_get_package_installation_directory_of_module(nullptr);
    auto root=fs::u8path(directory);g_free(directory);
    auto certificates=root/"etc"/"ssl"/"certs"/"ca-bundle.crt";
    if(fs::is_regular_file(certificates)){g_setenv("CURL_CA_BUNDLE",certificates.u8string().c_str(),true);g_setenv("SSL_CERT_FILE",certificates.u8string().c_str(),true);}
    if(!fs::is_directory(root/"share"/"glib-2.0"/"schemas"))return;
    auto env=[&](const char *key,const fs::path &value){g_setenv(key,value.u8string().c_str(),true);};
    env("GTK_DATA_PREFIX",root);env("XDG_DATA_DIRS",root/"share");
    env("GSETTINGS_SCHEMA_DIR",root/"share"/"glib-2.0"/"schemas");
    auto modules=root/"lib"/"gdk-pixbuf-2.0"/"2.10.0"/"loaders";
    env("GDK_PIXBUF_MODULEDIR",modules);
    auto query=(root/"gdk-pixbuf-query-loaders.exe").u8string();
    gchar *args[]={query.data(),nullptr};gchar *output=nullptr,*errors=nullptr;gint status=0;GError *error=nullptr;
    bool ok=g_spawn_sync(nullptr,args,nullptr,G_SPAWN_DEFAULT,nullptr,nullptr,&output,&errors,&status,&error);
    std::string reason=error?error->message:(errors?errors:"");if(error)g_error_free(error);g_free(errors);
    if(!ok || status!=0){g_free(output);throw std::runtime_error("Cannot initialize bundled image loaders: "+reason);}
    static RuntimeCache cache;gint fd=g_file_open_tmp("customs-pixbuf-XXXXXX",&cache.path,nullptr);
    if(fd<0){g_free(output);throw std::runtime_error("Cannot create image-loader cache");}g_close(fd,nullptr);
    ok=g_file_set_contents(cache.path,output,-1,nullptr);g_free(output);
    require(ok,"Cannot write image-loader cache");g_setenv("GDK_PIXBUF_MODULE_FILE",cache.path,true);
}
#else
static void prepareRuntime() {}
#endif

static std::string settingsFile() {
    return (fs::u8path(g_get_user_config_dir())/"reriff-livery-maker"/"settings.ini").u8string();
}
static std::string rememberedFolder() {
    auto *settings=g_key_file_new();std::string folder;
    if(g_key_file_load_from_file(settings,settingsFile().c_str(),G_KEY_FILE_NONE,nullptr)) {
        auto *value=g_key_file_get_string(settings,"Editor","GameFolder",nullptr);
        if(value){folder=value;g_free(value);}
    }
    g_key_file_unref(settings);return folder;
}
static void rememberFolder(const std::string &folder) {
    auto *settings=g_key_file_new();
    g_key_file_set_string(settings,"Editor","GameFolder",folder.c_str());
    gsize length=0;auto *data=g_key_file_to_data(settings,&length,nullptr);
    auto file=settingsFile();GError *error=nullptr;
    bool ok=g_mkdir_with_parents(fs::u8path(file).parent_path().u8string().c_str(),0700)==0 &&
        g_file_set_contents(file.c_str(),data,length,&error);
    std::string message=error?error->message:"Cannot create editor settings directory";
    if(error)g_error_free(error);
    g_free(data);g_key_file_unref(settings);
    require(ok,message);
}
struct App {
    GtkWidget *addDecalButton{};
    struct AddedDecal {Target target;Image artwork;};
    std::map<size_t,AddedDecal> addedDecals;
    GtkWidget *window{},*carCombo{},*combo{},*status{},*canvas{},*mode{};
    std::unique_ptr<PaintConfig> paintConfig;GtkWidget *exportButton{};bool updatingColors=false;
    std::vector<std::pair<std::string,GtkWidget*>> colorButtons;
    std::unique_ptr<Archive> archive;std::unique_ptr<Target> selected;bool testArtwork=false;
    std::map<std::string,std::string> names;
    std::vector<PaintSelection> paints;std::vector<GtkWidget*> editing;size_t chosen=0;
    Image original,art,preview;std::unique_ptr<Model> model;
    bool edited=false;std::string view="Painted + Livery";
    double carYaw=.65,carZoom=1,dragX=0;bool dragging=false;
    std::string gameFolder;
    void message(const std::string &s) {gtk_label_set_text(GTK_LABEL(status),s.c_str());}
    void error(const std::string &s) {showErrorDialog(GTK_WINDOW(window),s);}
    template<class F> void guard(F f) {try{f();}catch(const std::exception &e){error(e.what());}}
    std::string choose(const char *title,GtkFileChooserAction action,const char *suggest=nullptr) {
        auto *d=gtk_file_chooser_dialog_new(title,GTK_WINDOW(window),action,"Cancel",GTK_RESPONSE_CANCEL,action==GTK_FILE_CHOOSER_ACTION_SAVE?"Save":"Choose",GTK_RESPONSE_ACCEPT,nullptr);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),true);
        if(action==GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER && gameFolder.size())
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d),gameFolder.c_str());
        if(suggest)gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d),suggest);
        std::string p;if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){auto *s=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));p=s;g_free(s);}gtk_widget_destroy(d);return p;
    }
    static std::string exportSlug(const std::string &name,const std::string &fallback){
        std::string base;for(unsigned char c:name){if(std::isalnum(c))base.push_back(char(std::tolower(c)));else if(!base.empty()&&base.back()!='-')base.push_back('-');}
        while(!base.empty()&&base.back()=='-')base.pop_back();
        return base.empty()?fallback:base;
    }
    std::string exportFileName(const char *kind) const {
        if(paints.empty()||chosen>=paints.size())return std::string("car-")+kind+".png";
        auto &paint=paints.at(chosen);auto base=exportSlug(paint.carName,paint.car);
        if(std::string(kind)=="livery"&&!paint.paintName.empty()){auto name=exportSlug(paint.paintName,"");if(!name.empty())base+="-"+name;}return base+"-"+kind+".png";
    }
    void mergeLiveryModsDialog(){
        require(bool(archive),"Open a game folder first");std::vector<fs::path> folders;
        for(;;){auto path=choose("Add livery mod",GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);if(path.empty())break;folders.push_back(fs::u8path(path));auto *q=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,"Add another livery mod?");int answer=gtk_dialog_run(GTK_DIALOG(q));gtk_widget_destroy(q);if(answer!=GTK_RESPONSE_YES)break;}
        require(folders.size()>=2,"Choose at least two livery mods");auto out=choose("Export merged livery mod",GTK_FILE_CHOOSER_ACTION_SAVE,"merged-livery-mod");if(out.empty())return;
        auto merged=mergeLiveryMods(*archive,folders);publishPaintMod(merged.files,merged.metadata,fs::u8path(out));
        message("Merged livery mod: "+out+". Remove or disable the original livery mods; install only this merged folder.");
    }
    void rebuildCarTemplate(){
        require(bool(archive),"Open a game folder first");
        auto templatePath=choose("Open car template",GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);if(templatePath.empty())return;
        auto tpl=loadCarTemplate(fs::u8path(templatePath));require(tpl.manifest.contains("import_recipe")&&tpl.manifest.at("import_recipe").is_object(),"This template has no import recipe; use Apply appearance template in the editor.");
        auto glb=choose("Open source GLB",GTK_FILE_CHOOSER_ACTION_OPEN);if(glb.empty())return;
        auto *dialog=gtk_dialog_new_with_buttons("Rebuild car",GTK_WINDOW(window),GTK_DIALOG_MODAL,"Cancel",GTK_RESPONSE_CANCEL,"Continue",GTK_RESPONSE_ACCEPT,nullptr);
        auto *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_container_set_border_width(GTK_CONTAINER(box),10);auto *label=gtk_label_new("Choose the detail level for the rebuilt car. The template's roles, materials and appearance will be reapplied.");gtk_label_set_line_wrap(GTK_LABEL(label),true);gtk_box_pack_start(GTK_BOX(box),label,false,false,0);
        auto *quality=gtk_combo_box_text_new();for(auto *text:{"Light","Vanilla-matched (recommended)","High detail","Full source detail"})gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(quality),text);int preset=tpl.manifest.at("import_recipe").value("options",Json::object()).value("quality",1);gtk_combo_box_set_active(GTK_COMBO_BOX(quality),std::clamp(preset,0,3));gtk_box_pack_start(GTK_BOX(box),quality,false,false,0);gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),box);gtk_widget_show_all(dialog);
        int response=gtk_dialog_run(GTK_DIALOG(dialog));auto chosen=importer::Options::Quality(std::clamp(gtk_combo_box_get_active(GTK_COMBO_BOX(quality)),0,3));gtk_widget_destroy(dialog);if(response!=GTK_RESPONSE_ACCEPT)return;
        auto out=choose("Export rebuilt car",GTK_FILE_CHOOSER_ACTION_SAVE,"customs-rebuilt-car");if(out.empty())return;
        rebuildFromTemplate(*archive,tpl,fs::u8path(glb),fs::u8path(out),chosen);
        AttachmentEditor(*archive,fs::u8path(out)).run(GTK_WINDOW(window));
    }
    void load(const std::string &p,bool persist=true) {
        auto next=std::make_unique<Archive>((fs::u8path(p)/"vfs.zip").u8string());
        auto labels=displayNames(*next);auto selections=paintSelections(*next,labels);
        require(!selections.empty(),"No paint selections in this game archive");
        auto config=std::make_unique<PaintConfig>(*next);
        names=std::move(labels);paints=std::move(selections);paintConfig=std::move(config);
        addedDecals.clear();selected.reset();model.reset();archive=std::move(next);
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(carCombo));
        std::set<std::string> seen;
        for(const auto &paint:paints)if(seen.insert(paint.car).second)
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(carCombo),paint.car.c_str(),paint.carName.c_str());
        gtk_combo_box_set_active(GTK_COMBO_BOX(carCombo),0);
        require(bool(model),"Could not load the game folder's initial car");
        gameFolder=fs::absolute(fs::u8path(p)).lexically_normal().u8string();
        if(persist)try{rememberFolder(gameFolder);}catch(const std::exception &e){message("Game folder opened, but could not remember it: "+std::string(e.what()));}
    }
    bool restore() {
        auto folder=rememberedFolder();if(folder.empty())return false;
        try{load(folder,false);return true;}
        catch(const std::exception &e){message("Could not reopen saved game folder: "+folder+"\n"+e.what()+"\nUse Open game folder to choose its new location.");return false;}
    }
    void selectCar() {
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));
        auto *car=gtk_combo_box_get_active_id(GTK_COMBO_BOX(carCombo));if(!car)return;
        for(size_t i=0;i<paints.size();i++)if(paints[i].car==car)
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),std::to_string(i).c_str(),paints[i].paintName.c_str());
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);
    }
    void chooseResource(const std::string &resource) {
        for(size_t i=0;i<paints.size();i++)if(paints[i].resource==resource) {
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(carCombo),paints[i].car.c_str());
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo),std::to_string(i).c_str());return;
        }
        throw std::runtime_error("No selectable paint uses this texture");
    }
    void select() {
        if(!archive)return;
        auto *id=gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));if(!id)return;
        chosen=std::stoul(id);const auto &paint=paints.at(chosen);
        selected.reset();edited=false;testArtwork=false;original={};art={};preview={};
        for(auto *widget:editing)gtk_widget_set_sensitive(widget,false);
        model=std::make_unique<Model>(decodeModel(asset(*archive,"graphics/vehicles/"+paint.car+"/lod0_livery.bdae")));
        if(addedDecals.count(chosen)) {
            auto &entry=addedDecals.at(chosen);selected=std::make_unique<Target>(entry.target);
            original=decodeDDS(selected->texture);update(entry.artwork);
            for(auto *widget:editing)gtk_widget_set_sensitive(widget,true);
        } else if(paint.resource.empty()) {
            describe();
        } else {
            selected=std::make_unique<Target>(target(*archive,paint.resource));
            original=decodeDDS(selected->texture);art=original;preview=original;
            for(auto *widget:editing)gtk_widget_set_sensitive(widget,true);
            describe();
        }
        gtk_widget_set_sensitive(addDecalButton,paint.resource.empty() && !selected);
        loadColors();gtk_widget_set_sensitive(exportButton,true);
        gtk_widget_queue_draw(canvas);
    }
    void addDecal() {
        require(archive && paintConfig,"Open a game folder first");
        std::set<std::string> reserved;for(const auto &item:addedDecals)reserved.insert(item.second.target.name);
        auto decal=newDecal(*archive,paints.at(chosen),reserved);
        auto blank=decodeDDS(decal.texture);std::fill(blank.pixels.begin(),blank.pixels.end(),0);
        paintConfig->setLivery(paints.at(chosen),unsigned(std::stoi(decal.name.substr(4))));
        addedDecals.emplace(chosen,AddedDecal{std::move(decal),std::move(blank)});select();
    }
    std::map<std::string,Bytes> exportFiles() const {
        std::map<std::string,Bytes> files;
        if(paintConfig->changed() || !selected)files=paintConfig->files();
        for(const auto &item:addedDecals)files[lower(item.second.target.member)]=editedBatch(item.second.target,&item.second.artwork);
        if(selected && (edited || !paintConfig->changed()))files[lower(selected->member)]=editedBatch(*selected,edited?&art:nullptr);
        return files;
    }
    void resetPaint() {
        if(!paintConfig)return;
        paintConfig->reset(paints.at(chosen));addedDecals.erase(chosen);select();
    }
    void loadColors() {
        updatingColors=true;
        for(auto &control:colorButtons) {
            auto values=paintConfig->get(paints.at(chosen),control.first,3);
            gtk_widget_set_sensitive(control.second,values.size()==3);
            if(values.size()==3) {GdkRGBA rgba{values[0]/255.,values[1]/255.,values[2]/255.,1};gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(control.second),&rgba);}
        }
        updatingColors=false;
    }
    void changeColor(GtkWidget *widget) {
        if(updatingColors || !paintConfig)return;
        for(auto &control:colorButtons)if(control.second==widget) {
            GdkRGBA rgba;gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget),&rgba);
            paintConfig->set(paints.at(chosen),control.first,{float(rgba.red*255),float(rgba.green*255),float(rgba.blue*255)});
        }
        describe();gtk_widget_queue_draw(canvas);
    }
    void describe() {
        if(!paintConfig)return;
        const auto &paint=paints.at(chosen);
        std::string info=paint.carName+" — "+paint.paintName;
        if(selected) {
            auto &t=selected->texture;info+=" • "+std::to_string(t.width)+" × "+std::to_string(t.height)+" • "+t.fourcc;
            info+=addedDecals.count(chosen)?"\nNew decal for this paint only (experimental; verify in-game).":"\nDecal texture is shared by: "+paintNames(*selected,names);
        } else info+="\nNative paint colors only; this paint has no separate PNG decal.";
        info+=paintConfig->changed()?"\nPaint/decal edits pending. Export includes the full paint configuration and runtime script; other paint-config mods do not merge.":"\nPaint colors unchanged. Color edits apply to the selected paint only.";
        if(edited)info+="\nPNG artwork edited; export also includes the decal texture.";
        message(info);
    }
    void update(Image im) {art=std::move(im);auto t=selected->texture;t.dds=encodeDDS(t,art);preview=decodeDDS(t);edited=true;if(addedDecals.count(chosen))addedDecals.at(chosen).artwork=art;describe();gtk_widget_queue_draw(canvas);}
    void exportPackage() {
        require(bool(paintConfig),"Open a game folder first");
        if(selected && (edited || !paintConfig->changed()) && selected->packs.size()>1) {
            auto text="This texture is shared by: "+paintNames(*selected,names)+". Exporting changes all these paints. Continue?";
            auto *dialog=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,"%s",text.c_str());
            bool yes=gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_YES;gtk_widget_destroy(dialog);if(!yes)return;
        }
        if(paintConfig->changed()) {
            auto *dialog=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,"Paint export contains the full native paint configuration and runtime script, including all pending color and added-decal edits. Other paint-config mods will conflict rather than merge. Continue?");
            bool yes=gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_YES;gtk_widget_destroy(dialog);if(!yes)return;
        }
        auto p=choose("Export mod",GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);if(p.empty())return;
        auto *d=gtk_dialog_new_with_buttons("Export mod",GTK_WINDOW(window),GTK_DIALOG_MODAL,"Cancel",GTK_RESPONSE_CANCEL,"Export",GTK_RESPONSE_ACCEPT,nullptr);
        auto *entry=gtk_entry_new();gtk_entry_set_text(GTK_ENTRY(entry),("reriff-paint-"+paints.at(chosen).car+"-"+std::to_string(paints.at(chosen).packIndex+1)).c_str());gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(d))),entry);gtk_widget_show_all(d);
        std::string name;if(gtk_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT)name=gtk_entry_get_text(GTK_ENTRY(entry));gtk_widget_destroy(d);if(name.empty())return;
        require(name!="." && name!=".." && name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")==std::string::npos,"Use letters, numbers, hyphens or underscores for the folder name");
        auto out=fs::u8path(p)/name;
        if(fs::exists(out)) {auto *q=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,"Replace this tool's previous export at %s?",out.u8string().c_str());bool yes=gtk_dialog_run(GTK_DIALOG(q))==GTK_RESPONSE_YES;gtk_widget_destroy(q);if(!yes)return;}
        auto files=exportFiles();
        Json metadata={{"paint_source_sha256",sha(paintConfig->original())},{"paint_configuration_modified",paintConfig->changed()},{"selected_car",paints.at(chosen).car},{"selected_pack_index",paints.at(chosen).packIndex}};
        auto meta=publishMod(files,metadata,out);
        if(!meta["sibling_resource_conflicts"].empty())error("Resource conflicts in the export parent folder: "+meta["sibling_resource_conflicts"].dump()+". Resolve these before testing; native volume priority decides which wins.");
        message("Exported: "+out.u8string()+"\nRequires ReRiff with loose-mod support. Copy the whole folder into the game's mods directory, then fully restart.");
    }
    void uv(cairo_t *cr,double size) {
        if(!model)return;
        cairo_set_source_rgba(cr,0.2,0.95,1,0.45);cairo_set_line_width(cr,0.5);
        for(auto &mesh:model->meshes) for(size_t i=0;i+2<mesh.indices.size();i+=3) {
            for(int k=0;k<3;k++){auto v=mesh.uv.at(mesh.indices[i+k]);if(k==0)cairo_move_to(cr,v[0]*size,v[1]*size);else cairo_line_to(cr,v[0]*size,v[1]*size);}cairo_close_path(cr);
        }cairo_stroke(cr);
    }
    void draw(cairo_t *cr,int w,int h) {
        cairo_set_source_rgb(cr,0.10,0.11,0.13);cairo_paint(cr);
        if(view=="Painted + Livery" || view=="Car preview" || !selected) {
            if(model && paintConfig)drawCar(cr,*model,*paintConfig,paints.at(chosen),w,h,carYaw,carZoom,view=="Painted + Livery" && selected?&preview:nullptr);
            return;
        }
        double size=std::max(1,std::min(w,h)-24);cairo_translate(cr,(w-size)/2,(h-size)/2);cairo_rectangle(cr,0,0,size,size);cairo_clip(cr);
        for(int y=0;y<16;y++)for(int x=0;x<16;x++){double c=((x+y)%2)?0.35:0.25;cairo_set_source_rgb(cr,c,c,c);cairo_rectangle(cr,x*size/16,y*size/16,size/16+1,size/16+1);cairo_fill(cr);}
        const auto &im=view=="Original"?original:preview;
        if(view!="UV template") {
            auto *pix=gdk_pixbuf_new_from_data(im.pixels.data(),GDK_COLORSPACE_RGB,true,8,im.width,im.height,im.width*4,nullptr,nullptr);
            cairo_save(cr);cairo_scale(cr,size/im.width,size/im.height);gdk_cairo_set_source_pixbuf(cr,pix,0,0);cairo_paint(cr);cairo_restore(cr);g_object_unref(pix);
        }
        if(view=="UV template")uv(cr,size);
    }
    void saveUV(const std::string &path) {
        require(bool(selected),"Open a game folder first");
        auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,selected->texture.width,selected->texture.height);auto *cr=cairo_create(surface);uv(cr,selected->texture.width);cairo_destroy(cr);
        saveSurface(surface,fs::u8path(path));cairo_surface_destroy(surface);
    }
    static GtkWidget *button(GtkWidget *box,const char *label,App *app,void(*fn)(App &)) {
        auto *b=gtk_button_new_with_label(label);gtk_box_pack_start(GTK_BOX(box),b,false,false,0);
        struct Call{App *a;void(*f)(App &);};auto *call=new Call{app,fn};
        g_signal_connect_data(b,"clicked",G_CALLBACK(+[](GtkButton*,gpointer p){auto *c=static_cast<Call*>(p);c->a->guard([&]{c->f(*c->a);});}),call,+[](gpointer p,GClosure*){delete static_cast<Call*>(p);},G_CONNECT_DEFAULT);
        return b;
    }
    void build() {
        window=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_title(GTK_WINDOW(window),"Riff Racer Customs");gtk_window_set_default_size(GTK_WINDOW(window),1000,820);
        g_signal_connect(window,"destroy",G_CALLBACK(gtk_main_quit),nullptr);
        auto *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);gtk_container_set_border_width(GTK_CONTAINER(box),14);gtk_container_add(GTK_CONTAINER(window),box);
        auto *title=gtk_label_new("Edit car paints and liveries");gtk_box_pack_start(GTK_BOX(box),title,false,false,0);
        auto *imports=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_pack_start(GTK_BOX(box),imports,false,false,0);
        button(imports,"Import mod folder",this,[](App &a){require(bool(a.archive),"Open a game folder first");auto path=a.choose("Import mod folder",GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);if(!path.empty())AttachmentEditor(*a.archive,fs::u8path(path)).run(GTK_WINDOW(a.window));});
        button(imports,"Import mod ZIP",this,[](App &a){require(bool(a.archive),"Open a game folder first");auto path=a.choose("Import mod ZIP",GTK_FILE_CHOOSER_ACTION_OPEN);if(!path.empty())AttachmentEditor(*a.archive,fs::u8path(path)).run(GTK_WINDOW(a.window));});
        auto *rebuild=button(imports,"Rebuild car from template",this,[](App &a){a.rebuildCarTemplate();});gtk_widget_set_tooltip_text(rebuild,"Rebuilds a car template; verifies the source GLB hash.");
        auto *merge=button(imports,"Merge livery mods",this,[](App &a){a.mergeLiveryModsDialog();});gtk_widget_set_tooltip_text(merge,"Combines livery-only mods for different paints of one car.");
        auto *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_pack_start(GTK_BOX(box),row,false,false,0);
        button(row,"Open game folder",this,[](App &a){auto p=a.choose("Open game folder",GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);if(!p.empty())a.load(p);});
        gtk_box_pack_start(GTK_BOX(row),gtk_label_new("Car"),false,false,0);
        carCombo=gtk_combo_box_text_new();gtk_box_pack_start(GTK_BOX(row),carCombo,false,false,0);
        g_signal_connect(carCombo,"changed",G_CALLBACK(+[](GtkComboBox*,gpointer p){auto &a=*static_cast<App*>(p);a.guard([&]{a.selectCar();});}),this);
        gtk_box_pack_start(GTK_BOX(row),gtk_label_new("Paint"),false,false,0);
        combo=gtk_combo_box_text_new();gtk_box_pack_start(GTK_BOX(row),combo,false,false,0);g_signal_connect(combo,"changed",G_CALLBACK(+[](GtkComboBox*,gpointer p){auto &a=*static_cast<App*>(p);a.guard([&]{a.select();});}),this);
        auto *actions=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_pack_start(GTK_BOX(box),actions,false,false,0);
        editing.push_back(button(actions,"Import PNG",this,[](App &a){require(bool(a.selected),"Open a game folder first");auto p=a.choose("Import PNG artwork",GTK_FILE_CHOOSER_ACTION_OPEN);if(!p.empty()){a.testArtwork=false;a.update(loadPNG(fs::u8path(p),a.selected->texture.width,a.selected->texture.height));}}));
        editing.push_back(button(actions,"Make test livery",this,[](App &a){require(bool(a.selected),"Open a game folder first");a.testArtwork=true;a.update(diagnostic(a.selected->texture));}));
        exportButton=button(actions,"Export mod",this,[](App &a){a.exportPackage();});gtk_widget_set_sensitive(exportButton,false);
        addDecalButton=button(actions,"Add PNG decal",this,[](App &a){a.addDecal();});gtk_widget_set_sensitive(addDecalButton,false);
        auto *paintGrid=gtk_grid_new();gtk_grid_set_column_spacing(GTK_GRID(paintGrid),12);gtk_grid_set_row_spacing(GTK_GRID(paintGrid),6);gtk_box_pack_start(GTK_BOX(box),paintGrid,false,false,0);
        const char *keys[]={"col0","col1","col2","specCol","alloysCol0","alloysCol1"};
        const char *labels[]={"Body color 1","Body color 2","Body color 3","Highlight color","Wheel color 1","Wheel color 2"};
        for(int i=0;i<6;i++) {
            auto *label=gtk_label_new(labels[i]);auto *color=gtk_color_button_new();gtk_color_button_set_title(GTK_COLOR_BUTTON(color),labels[i]);
            gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(color),false);gtk_widget_set_sensitive(color,false);
            gtk_grid_attach(GTK_GRID(paintGrid),label,(i%3)*2,i/3,1,1);gtk_grid_attach(GTK_GRID(paintGrid),color,(i%3)*2+1,i/3,1,1);
            colorButtons.push_back({keys[i],color});g_signal_connect(color,"color-set",G_CALLBACK(+[](GtkColorButton *button,gpointer p){auto &a=*static_cast<App*>(p);a.guard([&]{a.changeColor(GTK_WIDGET(button));});}),this);
        }
        button(actions,"Reset paint",this,[](App &a){a.resetPaint();});
        auto *row2=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_pack_start(GTK_BOX(box),row2,false,false,0);
        mode=gtk_combo_box_text_new();for(auto *v:{"Painted + Livery","Car preview","Texture preview","Original","UV template"})gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mode),v);gtk_combo_box_set_active(GTK_COMBO_BOX(mode),0);gtk_box_pack_start(GTK_BOX(row2),mode,false,false,0);
        g_signal_connect(mode,"changed",G_CALLBACK(+[](GtkComboBox*,gpointer p){auto &a=*static_cast<App*>(p);auto *s=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(a.mode));if(s){a.view=s;g_free(s);}gtk_widget_queue_draw(a.canvas);}),this);
        editing.push_back(button(row2,"Save artwork PNG",this,[](App &a){require(bool(a.selected),"Open a game folder first");auto p=a.choose("Save artwork PNG",GTK_FILE_CHOOSER_ACTION_SAVE,a.exportFileName(a.testArtwork?"test":"livery").c_str());if(!p.empty())savePNG(fs::u8path(p),a.art);}));
        editing.push_back(button(row2,"Save UV template",this,[](App &a){auto p=a.choose("Save UV template",GTK_FILE_CHOOSER_ACTION_SAVE,a.exportFileName("uv").c_str());if(!p.empty())a.saveUV(p);}));
        auto *note=gtk_label_new("Drag the car to rotate • Scroll to zoom • Preview uses approximate lighting and decal blending");gtk_label_set_line_wrap(GTK_LABEL(note),true);gtk_box_pack_start(GTK_BOX(box),note,false,false,0);
        canvas=gtk_drawing_area_new();gtk_widget_set_size_request(canvas,320,320);gtk_box_pack_start(GTK_BOX(box),canvas,true,true,0);g_signal_connect(canvas,"draw",G_CALLBACK(+[](GtkWidget *w,cairo_t *cr,gpointer p)->gboolean{static_cast<App*>(p)->draw(cr,gtk_widget_get_allocated_width(w),gtk_widget_get_allocated_height(w));return FALSE;}),this);
        gtk_widget_add_events(canvas,GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK|GDK_SCROLL_MASK|GDK_SMOOTH_SCROLL_MASK);
        g_signal_connect(canvas,"button-press-event",G_CALLBACK(+[](GtkWidget*,GdkEventButton *e,gpointer p)->gboolean{auto &a=*static_cast<App*>(p);if(e->button!=1)return FALSE;a.dragging=true;a.dragX=e->x;return TRUE;}),this);
        g_signal_connect(canvas,"button-release-event",G_CALLBACK(+[](GtkWidget*,GdkEventButton*,gpointer p)->gboolean{static_cast<App*>(p)->dragging=false;return TRUE;}),this);
        g_signal_connect(canvas,"motion-notify-event",G_CALLBACK(+[](GtkWidget*,GdkEventMotion *e,gpointer p)->gboolean{auto &a=*static_cast<App*>(p);if(!a.dragging)return FALSE;a.carYaw+=(e->x-a.dragX)*.012;a.dragX=e->x;gtk_widget_queue_draw(a.canvas);return TRUE;}),this);
        g_signal_connect(canvas,"scroll-event",G_CALLBACK(+[](GtkWidget*,GdkEventScroll *e,gpointer p)->gboolean{auto &a=*static_cast<App*>(p);double d=e->direction==GDK_SCROLL_SMOOTH?e->delta_y:(e->direction==GDK_SCROLL_UP?-1:1);a.carZoom=std::clamp(a.carZoom*std::exp(-d*.12),.5,2.5);gtk_widget_queue_draw(a.canvas);return TRUE;}),this);
        status=gtk_label_new("Open your game folder, select a car/livery, then import artwork or generate a test livery.");gtk_label_set_line_wrap(GTK_LABEL(status),true);gtk_label_set_selectable(GTK_LABEL(status),true);gtk_box_pack_start(GTK_BOX(box),status,false,false,0);
        for(auto *widget:editing)gtk_widget_set_sensitive(widget,false);
        gtk_widget_show_all(window);
    }
};
int main(int argc,char **argv) {
    try {
        prepareRuntime();
        if(argc==5&&std::string(argv[1])=="--smoke-migration"){gtk_init(&argc,&argv);Archive stock((fs::u8path(argv[2])/"vfs.zip").u8string());AttachmentEditor(stock,fs::u8path(argv[3])).smokeMigration(fs::u8path(argv[4]));return 0;}
        if(argc==5&&std::string(argv[1])=="--smoke-model-import"){gtk_init(&argc,&argv);Archive stock((fs::u8path(argv[2])/"vfs.zip").u8string());auto output=ModelImportDialog(stock,fs::u8path(argv[3])).run(nullptr,fs::u8path(argv[4]));require(!output.empty()&&fs::exists(output/"assets"),"Import did not create a working copy");ImportedMod imported(output,&stock);require(imported.cars.count("spn"),"Working copy did not reimport the authored car");std::cout<<"Model import dialog, validation, working copy and reimport passed\n";return 0;}

        if(argc==4&&std::string(argv[1])=="--smoke-unified") {gtk_init(&argc,&argv);Archive stock((fs::u8path(argv[2])/"vfs.zip").u8string());AttachmentEditor(stock,{}).smokeUnified(fs::u8path(argv[3]));return 0;}
        if(argc==5 && std::string(argv[1])=="--smoke-mod-attachments") {
            gtk_init(&argc,&argv);Archive archive((fs::u8path(argv[2])/"vfs.zip").u8string());
            AttachmentEditor(archive,fs::u8path(argv[3])).smoke(nullptr,fs::u8path(argv[4]));
            std::cout<<"PASS imported mod GUI edit/export\n";return 0;
        }
        if(argc==3 && std::string(argv[1])=="--smoke-package") {
            gtk_init(&argc,&argv);App app;app.build();
            while(gtk_events_pending()){gtk_main_iteration();}
            auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gtk_widget_get_allocated_width(app.window),gtk_widget_get_allocated_height(app.window));
            auto *cr=cairo_create(surface);gtk_widget_draw(app.window,cr);cairo_destroy(cr);
            saveSurface(surface,fs::u8path(argv[2]));cairo_surface_destroy(surface);
            auto *chooser=gtk_file_chooser_dialog_new("Package test",GTK_WINDOW(app.window),GTK_FILE_CHOOSER_ACTION_OPEN,"Cancel",GTK_RESPONSE_CANCEL,nullptr);
            gtk_widget_show_all(chooser);while(gtk_events_pending()){gtk_main_iteration();}gtk_widget_destroy(chooser);
            auto *color=gtk_color_chooser_dialog_new("Color test",GTK_WINDOW(app.window));gtk_widget_show_all(color);
            while(gtk_events_pending()){gtk_main_iteration();}gtk_widget_destroy(color);
            std::cout<<"PASS packaged GTK window, file chooser, color chooser and PNG output\n";return 0;
        }
        if(argc==4 && std::string(argv[1])=="--smoke-livery-preview") {
            gtk_init(&argc,&argv);App app;app.build();app.load(argv[2],false);
            auto out=fs::u8path(argv[3]);fs::create_directories(out);
            auto render=[&](const fs::path &path){auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,900,600);auto *cr=cairo_create(surface);app.draw(cr,900,600);cairo_destroy(cr);saveSurface(surface,path);cairo_surface_destroy(surface);return read(path.u8string());};
            app.chooseResource("alf_01");app.view="Car preview";auto painted=render(out/"paint-only.png");
            app.view="Painted + Livery";require(render(out/"carmine-stock.png")!=painted,"Stock decal missing from car preview");
            Image transparent=app.original;std::fill(transparent.pixels.begin(),transparent.pixels.end(),0);app.update(transparent);
            require(render(out/"transparent.png")==painted,"Transparent decal obscures paint");
            auto diagnosticImage=diagnostic(app.selected->texture);savePNG(out/"input.png",diagnosticImage);
            app.update(loadPNG(out/"input.png",app.selected->texture.width,app.selected->texture.height));
            require(render(out/"carmine-custom.png")!=painted,"Imported PNG missing from car preview");
            for(const auto &id:targets(*app.archive)) {
                bool selectable=false;for(const auto &p:app.paints)if(p.resource==id)selectable=true;
                if(!selectable)continue;
                app.chooseResource(id);render(out/(id+".png"));
            }
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(app.carCombo),"alf");app.addDecal();app.update(diagnostic(app.selected->texture));render(out/"arctic-added.png");
            app.resetPaint();require(!app.selected,"Reset retained added decal");render(out/"arctic-reset.png");
            std::cout<<"PASS stock/imported/added 3D decals, transparent alpha, reset, and all selectable stock textures\n";return 0;
        }
        if(argc==4 && std::string(argv[1])=="--smoke-car") {
            gtk_init(&argc,&argv);App app;app.build();app.load(argv[2],false);
            auto out=fs::u8path(argv[3]);fs::create_directories(out);std::set<std::string> seen;
            auto render=[&](const fs::path &path){auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,900,600);auto *cr=cairo_create(surface);app.draw(cr,900,600);cairo_destroy(cr);saveSurface(surface,path);cairo_surface_destroy(surface);return read(path.u8string());};
            auto baseline=render(out/"pulse-original.png");
            for(const auto &control:app.colorButtons) {
                app.paintConfig->set(app.paints.at(app.chosen),control.first,{255,0,100});
                require(render(out/(control.first+".png"))!=baseline,"Color does not affect preview: "+control.first);
                app.paintConfig->reset(app.paints.at(app.chosen));
            }
            for(const auto &paint:app.paints)if(seen.insert(paint.car).second) {
                gtk_combo_box_set_active_id(GTK_COMBO_BOX(app.carCombo),paint.car.c_str());render(out/(paint.car+".png"));
            }
            app.carYaw+=1;render(out/"rotated.png");
            std::cout<<"PASS painted preview for "<<seen.size()<<" cars and all six color controls\n";return 0;
        }
        if(argc==3 && std::string(argv[1])=="--smoke-all-decals") {
            gtk_init(&argc,&argv);App app;app.build();app.load(argv[2],false);unsigned count=0;
            for(size_t i=0;i<app.paints.size();i++) {
                const auto &paint=app.paints[i];
                gtk_combo_box_set_active_id(GTK_COMBO_BOX(app.carCombo),paint.car.c_str());
                gtk_combo_box_set_active_id(GTK_COMBO_BOX(app.combo),std::to_string(i).c_str());
                require(bool(gtk_widget_get_sensitive(app.addDecalButton))==paint.resource.empty(),"Incorrect Add PNG decal availability");
                if(paint.resource.empty()) {
                    app.addDecal();require(app.selected && app.selected->car==paint.car,"Wrong car after adding decal");
                    require(app.exportFiles().size()==3,"Incomplete new-decal export");app.resetPaint();++count;
                }
            }
            std::cout<<"PASS Add PNG decal enabled, created, exported and reset for all "<<count<<" undecorated paints\n";return 0;
        }
        if(argc==4 && std::string(argv[1])=="--test-added-decal") {
            gtk_init(&argc,&argv);App app;app.build();app.load(argv[2],false);
            require(app.paints.at(app.chosen).paintName=="Arctic","Expected Pulse Arctic");
            app.addDecal();require(app.selected->name=="alf_03","Expected isolated Pulse decal ID");
            app.update(diagnostic(app.selected->texture));auto expected=app.art.pixels;
            app.chooseResource("alf_01");app.selectCar();
            require(app.selected && app.art.pixels==expected,"Added artwork lost on reselection");
            auto out=fs::u8path(argv[3]);fs::create_directories(out);
            publishMod(app.exportFiles(),{{"test","Pulse Arctic new decal"}},out/"customs-arctic-decal-test");
            savePNG(out/"arctic-decal.png",app.art);app.saveUV((out/"arctic-uv.png").u8string());
            while(gtk_events_pending())gtk_main_iteration();
            auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gtk_widget_get_allocated_width(app.window),gtk_widget_get_allocated_height(app.window));
            auto *cr=cairo_create(surface);gtk_widget_draw(app.window,cr);cairo_destroy(cr);saveSurface(surface,out/"editor.png");cairo_surface_destroy(surface);
            app.resetPaint();require(!app.selected && !app.paintConfig->changed() && app.addedDecals.empty(),"Reset left a decal assignment");
            std::cout<<"PASS Arctic new decal, reselection, complete export and reset\n";return 0;
        }
        // Headless mode uses exactly the same C++ encoding/export pipeline.
        if(argc>=2 && std::string(argv[1])=="--export") {
            require(argc==7,"Usage: --export GAME_DIR LIVERY_ID diagnostic|noop|PNG MOD_FOLDER ARTWORK_FOLDER");
            Archive a((fs::u8path(argv[2])/"vfs.zip").u8string());auto t=target(a,argv[3]);std::string mode=argv[4];Image im;
            if(mode!="noop")im=mode=="diagnostic"?diagnostic(t.texture):loadPNG(fs::u8path(mode),t.texture.width,t.texture.height);
            auto meta=exportMod(t,mode=="noop"?nullptr:&im,fs::u8path(argv[5]));fs::path art=safePath(fs::absolute(fs::u8path(argv[6])));require(!fs::exists(art),"Artwork folder already exists");fs::create_directories(art);
            savePNG(art/"original.png",decodeDDS(t.texture));if(mode!="noop") {savePNG(art/"artwork.png",im);auto encoded=t.texture;encoded.dds=encodeDDS(encoded,im);savePNG(art/"encoded-preview.png",decodeDDS(encoded));}save(art/"export.json",meta);std::cout<<meta.dump(2)<<'\n';return 0;
        }
        if(argc==5 && (std::string(argv[1])=="--smoke-gui" || std::string(argv[1])=="--smoke-paint")) {
            bool paintTest=std::string(argv[1])=="--smoke-paint";
            gtk_init(&argc,&argv);App app;app.build();app.load(argv[2],false);
            require(!app.selected && app.model,"Default paint should have no standalone decal");
            for(auto *widget:app.editing)require(!gtk_widget_get_sensitive(widget),"No-decal editing should be disabled");
            if(paintTest) {
                GdkRGBA green{0.1,0.9,0.2,1};
                for(size_t i=0;i<3;i++) {gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(app.colorButtons[i].second),&green);app.changeColor(app.colorButtons[i].second);}
                require(app.paintConfig->changed(),"Paint picker did not update native config");
                publishMod(app.paintConfig->files(),{{"test","paint-only"}},fs::u8path(argv[4]).parent_path()/"reriff-paint-test");
                app.selectCar();require(app.paintConfig->get(app.paints.at(app.chosen),"col0",3)[1]>220,"Paint edit lost on reselection");
            }
            app.chooseResource("bui_02");require(app.selected && app.selected->packs.size()==6,"Shared Rocket decal selection failed");
            app.chooseResource("alf_01");
            require(app.paints.at(app.chosen).paintName=="Carmine","Graphical selection lost its paint identity");
            app.update(loadPNG(fs::u8path(argv[3]),app.selected->texture.width,app.selected->texture.height));
            while(gtk_events_pending()){gtk_main_iteration();}
            app.saveUV((fs::u8path(argv[4]).parent_path()/"uv-template.png").u8string());
            auto *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,gtk_widget_get_allocated_width(app.window),gtk_widget_get_allocated_height(app.window));
            auto *cr=cairo_create(surface);gtk_widget_draw(app.window,cr);cairo_destroy(cr);
            saveSurface(surface,fs::u8path(argv[4]));cairo_surface_destroy(surface);return 0;
        }
        if(argc==3 && std::string(argv[1])=="--smoke-remember") {
            gtk_init(&argc,&argv);App app;app.build();app.load(argv[2]);
            require(rememberedFolder()==app.gameFolder,"Game folder was not saved");return 0;
        }
        if(argc==2 && std::string(argv[1])=="--smoke-restore") {
            gtk_init(&argc,&argv);App app;app.build();require(app.restore() && app.model,"Saved folder did not reopen");
            std::cout<<"PASS reopened "<<app.gameFolder<<'\n';return 0;
        }
        if(argc==3 && std::string(argv[1])=="--smoke-update-dialog") {
            gtk_init(&argc,&argv);
            update::UpdatePanel panel(CUSTOMS_VERSION,[]{return true;},[]{});
            panel.renderPreview(fs::u8path(argv[2]));
            std::cout<<"PASS update dialog layout and release notes\n";return 0;
        }
        if(argc>=2 && std::string(argv[1])=="--check-updates") {
            auto settings=update::readSettings();
            auto channel=argc>=3?update::channelFromKey(argv[2]):settings.channel;
            auto state=std::make_shared<update::State>();
            update::checkWorker(state,channel,true);
            std::lock_guard<std::mutex> lock(state->mutex);
            if(state->stage!=update::Stage::Ready) {std::cerr<<state->error<<'\n';return 1;}
            std::cout<<"channel="<<update::channelKey(channel)<<" installed="<<CUSTOMS_VERSION
                     <<" latest="<<state->version<<" install="<<(state->installable?"yes":"no")
                     <<" asset="<<state->asset<<"\n";
            if(!state->notes.empty())std::cout<<state->notes<<'\n';
            return 0;
        }
        gtk_init(&argc,&argv);std::string folder=argc==2?argv[1]:rememberedFolder();fs::path mod;int width=1100,height=760;bool maximized=false;
        for(;;){
            if(folder.empty()||!fs::exists(fs::u8path(folder)/"vfs.zip")){auto *chooser=gtk_file_chooser_dialog_new("Open game folder",nullptr,GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,"Cancel",GTK_RESPONSE_CANCEL,"Open",GTK_RESPONSE_ACCEPT,nullptr);if(gtk_dialog_run(GTK_DIALOG(chooser))!=GTK_RESPONSE_ACCEPT){gtk_widget_destroy(chooser);return 0;}auto *value=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));folder=value;g_free(value);gtk_widget_destroy(chooser);}
            try{Archive stock((fs::u8path(folder)/"vfs.zip").u8string());AttachmentEditor editor(stock,mod);try{rememberFolder(folder);}catch(const std::exception &e){std::cerr<<"Could not remember game folder: "<<e.what()<<'\n';}editor.windowWidth=width;editor.windowHeight=height;editor.maximized=maximized;editor.run(nullptr);if(!editor.restartRequested)return 0;width=editor.windowWidth;height=editor.windowHeight;maximized=editor.maximized;
                if(editor.requestedAction==0){folder=editor.requestedPath;mod.clear();}else mod=editor.requestedAction==3?fs::path{}:fs::u8path(editor.requestedPath);
            }catch(const std::exception &e){showErrorDialog(nullptr,e.what());folder.clear();mod.clear();}
        }
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
