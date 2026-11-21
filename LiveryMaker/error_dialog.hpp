#pragma once
#include <gtk/gtk.h>
#include <string>
namespace livery {
inline void showErrorDialog(GtkWindow *parent,const std::string &text){
    auto *dialog=gtk_dialog_new_with_buttons("Error",parent,GTK_DIALOG_MODAL,"Close",GTK_RESPONSE_CLOSE,nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog),560,150);
    auto *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));gtk_container_set_border_width(GTK_CONTAINER(content),12);
    auto *scroll=gtk_scrolled_window_new(nullptr,nullptr);gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);gtk_widget_set_size_request(scroll,500,90);gtk_box_pack_start(GTK_BOX(content),scroll,true,true,0);
    auto *label=gtk_label_new(text.c_str());gtk_label_set_selectable(GTK_LABEL(label),true);gtk_label_set_line_wrap(GTK_LABEL(label),true);gtk_label_set_max_width_chars(GTK_LABEL(label),72);gtk_label_set_xalign(GTK_LABEL(label),0);gtk_label_set_yalign(GTK_LABEL(label),0);gtk_container_add(GTK_CONTAINER(scroll),label);
    gtk_widget_show_all(dialog);gtk_widget_grab_focus(label);gtk_dialog_run(GTK_DIALOG(dialog));gtk_widget_destroy(dialog);
}
}
