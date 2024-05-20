/*
 *      pref.c
 *
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libsmfm-core/fm.h>

#include "pcmanfm.h"

#include "pref.h"
#include "app-config.h"

#define INIT_BOOL(b, st, name, changed_notify)  init_bool(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)
#define INIT_COMBO(b, st, name, changed_notify) init_combo(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)
#define INIT_ICON_SIZES(b, name) init_icon_sizes(b, #name, G_STRUCT_OFFSET(FmConfig, name))
#define INIT_COLOR(b, st, name, changed_notify)  init_color(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)
#define INIT_SPIN(b, st, name, changed_notify)  init_spin(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)
#define INIT_ENTRY(b, st, name, changed_notify)  init_entry(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)

static GtkWindow* pref_dlg = NULL;
static GtkNotebook* notebook = NULL;
/*
static GtkWidget* icon_size_combo[3] = {0};
static GtkWidget* bookmark_combo = NULL
static GtkWidget* use_trash;
*/

static void on_response(GtkDialog* dlg, int res, GtkWindow** pdlg)
{
    *pdlg = NULL;
    pcmanfm_save_config(TRUE);
    gtk_widget_destroy(GTK_WIDGET(dlg));
}

static void on_icon_size_changed(GtkComboBox* combo, gpointer _off)
{
    GtkTreeIter it;
    if(gtk_combo_box_get_active_iter(combo, &it))
    {
        gsize off = GPOINTER_TO_SIZE(_off);
        int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
        int size;
        GtkTreeModel* model = gtk_combo_box_get_model(combo);
        gtk_tree_model_get(model, &it, 1, &size, -1);
        if(size != *val)
        {
            const char* name = gtk_buildable_get_name((GtkBuildable*)combo);
            *val = size;
            fm_config_emit_changed(fm_config, name);
        }
    }
}

static void init_icon_sizes(GtkBuilder* builder, const char* name, gsize off)
{
    GtkComboBox* combo = (GtkComboBox*)gtk_builder_get_object(builder, name);
    GtkTreeModel* model = gtk_combo_box_get_model(combo);
    GtkTreeIter it;
    int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
    gtk_tree_model_get_iter_first(model, &it);
    gtk_combo_box_set_active_iter(combo, &it);
    do{
        int size;
        gtk_tree_model_get(model, &it, 1, &size, -1);
        if(size == *val)
        {
            gtk_combo_box_set_active_iter(combo, &it);
            break;
        }
    }while(gtk_tree_model_iter_next(model, &it));
    g_signal_connect(combo, "changed", G_CALLBACK(on_icon_size_changed), GSIZE_TO_POINTER(off));
}

static void on_combo_changed(GtkComboBox* combo, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
    int sel = gtk_combo_box_get_active(combo);
    if(sel != *val)
    {
        const char* name = g_object_get_data((GObject*)combo, "changed");
        if(!name)
            name = gtk_buildable_get_name((GtkBuildable*)combo);
        *val = sel;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_combo(GtkBuilder* builder, const char* name, gsize off, const char* changed_notify)
{
    GtkComboBox* combo = (GtkComboBox*)gtk_builder_get_object(builder, name);
    int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
    if(changed_notify)
        g_object_set_data_full(G_OBJECT(combo), "changed", g_strdup(changed_notify), g_free);
    gtk_combo_box_set_active(combo, *val);
    g_signal_connect(combo, "changed", G_CALLBACK(on_combo_changed), GSIZE_TO_POINTER(off));
}

static void on_archiver_combo_changed(GtkComboBox* combo, gpointer user_data)
{
    GtkTreeModel* model = gtk_combo_box_get_model(combo);
    GtkTreeIter it;
    if(gtk_combo_box_get_active_iter(combo, &it))
    {
        FmArchiver* archiver;
        gtk_tree_model_get(model, &it, 1, &archiver, -1);
        if(archiver)
        {
            g_free(fm_config->archiver);
            fm_config->archiver = g_strdup(archiver->program);
            fm_archiver_set_default(archiver);
            fm_config_emit_changed(fm_config, "archiver");
        }
    }
}

/* archiver integration */
static void init_archiver_combo(GtkBuilder* builder)
{
    GtkListStore* model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
    GtkComboBox* combo = (GtkComboBox*)gtk_builder_get_object(builder, "archiver");
    GtkTreeIter it;
    const GList* archivers = fm_archiver_get_all();
    FmArchiver* default_archiver = fm_archiver_get_default();
    const GList* l;

    gtk_combo_box_set_model(combo, GTK_TREE_MODEL(model));

    for(l = archivers; l; l=l->next)
    {
        FmArchiver* archiver = (FmArchiver*)l->data;
        gtk_list_store_insert_with_values(model, &it, -1,
                        0, archiver->program,
                        1, archiver, -1);
        if(archiver == default_archiver)
            gtk_combo_box_set_active_iter(combo, &it);
    }
    g_object_unref(model);
    g_signal_connect(combo, "changed", G_CALLBACK(on_archiver_combo_changed), NULL);
}

static void on_toggled(GtkToggleButton* btn, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    gboolean* val = (gboolean*)G_STRUCT_MEMBER_P(fm_config, off);
    gboolean new_val = gtk_toggle_button_get_active(btn);
    if(*val != new_val)
    {
        const char* name = g_object_get_data((GObject*)btn, "changed");
        if(!name)
            name = gtk_buildable_get_name((GtkBuildable*)btn);
        *val = new_val;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_bool(GtkBuilder* b, const char* name, gsize off, const char* changed_notify)
{
    GtkToggleButton* btn = GTK_TOGGLE_BUTTON(gtk_builder_get_object(b, name));
    gboolean* val = (gboolean*)G_STRUCT_MEMBER_P(fm_config, off);
    if(changed_notify)
        g_object_set_data_full(G_OBJECT(btn), "changed", g_strdup(changed_notify), g_free);
    gtk_toggle_button_set_active(btn, *val);
    g_signal_connect(btn, "toggled", G_CALLBACK(on_toggled), GSIZE_TO_POINTER(off));
}

static void on_spin_changed(GtkSpinButton* btn, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    guint* val = (guint*)G_STRUCT_MEMBER_P(fm_config, off);
    guint new_val = gtk_spin_button_get_value(btn);
    if(*val != new_val)
    {
        const char* name = g_object_get_data((GObject*)btn, "changed");
        if(!name)
            name = gtk_buildable_get_name((GtkBuildable*)btn);
        *val = new_val;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_spin(GtkBuilder* b, const char* name, gsize off, const char* changed_notify)
{
    GtkSpinButton* btn = GTK_SPIN_BUTTON(gtk_builder_get_object(b, name));
    guint* val = (guint*)G_STRUCT_MEMBER_P(fm_config, off);
    if(changed_notify)
        g_object_set_data_full(G_OBJECT(btn), "changed", g_strdup(changed_notify), g_free);
    gtk_spin_button_set_value(btn, *val);
    g_signal_connect(btn, "value-changed", G_CALLBACK(on_spin_changed), GSIZE_TO_POINTER(off));
}

static void on_entry_changed(GtkEntry* entry, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    gchar** val = (gchar**)G_STRUCT_MEMBER_P(fm_config, off);
    const char* new_val = gtk_entry_get_text(entry);
    if(g_strcmp0(*val, new_val))
    {
        const char* name = g_object_get_data((GObject*)entry, "changed");
        if(!name)
            name = gtk_buildable_get_name((GtkBuildable*)entry);
        g_free(*val);
        *val = *new_val ? g_strdup(new_val) : NULL;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_entry(GtkBuilder* b, const char* name, gsize off, const char* changed_notify)
{
    GtkEntry* btn = GTK_ENTRY(gtk_builder_get_object(b, name));
    gchar** val = (gchar**)G_STRUCT_MEMBER_P(fm_config, off);
    if(changed_notify)
        g_object_set_data_full(G_OBJECT(btn), "changed", g_strdup(changed_notify), g_free);
    if(*val)
        gtk_entry_set_text(btn, *val);
    g_signal_connect(btn, "changed", G_CALLBACK(on_entry_changed), GSIZE_TO_POINTER(off));
}

static void on_tab_label_list_sel_changed(GtkTreeSelection* tree_sel, gpointer user_data)
{
    GtkTreePath* tp;
    GtkTreeIter it;
    GtkTreeModel* model;
    gtk_tree_selection_get_selected(tree_sel, &model, &it);
    tp = gtk_tree_model_get_path(model, &it);
    gtk_notebook_set_current_page(notebook, gtk_tree_path_get_indices(tp)[0]);
    gtk_tree_path_free(tp);
}

static void on_notebook_page_changed(GtkNotebook *notebook, gpointer page,
                                     guint n, GtkTreeSelection* tree_sel)
{
    GtkTreeIter it;
    GtkTreeModel* model;
    char path_str[8];

    snprintf(path_str, sizeof(path_str), "%u", n);
    /* g_debug("changed pref page: %u", n); */
    gtk_tree_selection_get_selected(tree_sel, &model, &it);
    gtk_tree_model_get_iter_from_string(model, &it, path_str);
    gtk_tree_selection_select_iter(tree_sel, &it);
}

void fm_edit_preference( GtkWindow* parent, int page )
{
    if(!pref_dlg)
    {
        GtkBuilder* builder = gtk_builder_new();
        GtkTreeView* tab_label_list;
        GtkTreeSelection* tree_sel;
        GtkListStore* tab_label_model;
        GtkTreeIter it;
        int i, n;

        gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/pref.glade", NULL);
        pref_dlg = GTK_WINDOW(gtk_builder_get_object(builder, "dlg"));
        notebook = GTK_NOTEBOOK(gtk_builder_get_object(builder, "notebook"));

        INIT_BOOL(builder, FmConfig, single_click, NULL);
        INIT_BOOL(builder, FmConfig, confirm_del, NULL);
        INIT_BOOL(builder, FmConfig, use_trash, NULL);

        INIT_BOOL(builder, FmConfig, show_thumbnail, NULL);
        INIT_BOOL(builder, FmConfig, thumbnail_local, NULL);
        INIT_SPIN(builder, FmConfig, thumbnail_max, NULL);

        INIT_BOOL(builder, FmAppConfig, mount_on_startup, NULL);
        INIT_BOOL(builder, FmAppConfig, mount_removable, NULL);
        INIT_BOOL(builder, FmAppConfig, autorun, NULL);

        INIT_BOOL(builder, FmAppConfig, full_path_in_title, NULL);
        INIT_BOOL(builder, FmAppConfig, always_show_tabs, NULL);
        INIT_BOOL(builder, FmAppConfig, hide_close_btn, NULL);

        INIT_BOOL(builder, FmConfig, show_full_names, NULL);
        INIT_BOOL(builder, FmConfig, si_unit, NULL);
        INIT_BOOL(builder, FmConfig, backup_as_hidden, NULL);

        INIT_BOOL(builder, FmConfig, app_list_smart_grouping, NULL);

        INIT_COMBO(builder, FmAppConfig, bm_open_method, NULL);
        INIT_COMBO(builder, FmAppConfig, view_mode, NULL);

        INIT_ICON_SIZES(builder, big_icon_size);
        INIT_ICON_SIZES(builder, small_icon_size);
        INIT_ICON_SIZES(builder, thumbnail_size);
        INIT_ICON_SIZES(builder, pane_icon_size);

        INIT_BOOL(builder, FmConfig, highlight_file_names, NULL);

        INIT_ENTRY(builder, FmConfig, terminal, NULL);
        INIT_ENTRY(builder, FmAppConfig, su_cmd, NULL);
        INIT_BOOL(builder, FmConfig, force_startup_notify, NULL);

        INIT_BOOL(builder, FmConfig, places_home, NULL);
        INIT_BOOL(builder, FmConfig, places_desktop, NULL);
        INIT_BOOL(builder, FmConfig, places_applications, NULL);
        INIT_BOOL(builder, FmConfig, places_trash, NULL);
        INIT_BOOL(builder, FmConfig, places_computer, NULL);
        INIT_BOOL(builder, FmConfig, places_root, NULL);

        /* archiver integration */
        init_archiver_combo(builder);

        /* initialize the left side list used for switching among tabs */
        tab_label_list = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tab_label_list"));
        tab_label_model = gtk_list_store_new(1, G_TYPE_STRING);
        n = gtk_notebook_get_n_pages(notebook);
        for(i = 0; i < n; ++i)
        {
            /* this can be less efficient than iterating over a GList obtained by gtk_container_get_children().
            * However, the order of pages does matter here. So we use get_nth_page. */
            GtkWidget* page = gtk_notebook_get_nth_page(notebook, i);
            const char* title = gtk_notebook_get_tab_label_text(notebook, page);
            gtk_list_store_insert_with_values(tab_label_model, NULL, i, 0, title, -1);
        }
        gtk_tree_view_set_model(tab_label_list, GTK_TREE_MODEL(tab_label_model));
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tab_label_model), &it);
        tree_sel = gtk_tree_view_get_selection(tab_label_list);
        gtk_tree_selection_select_iter(tree_sel, &it);
        g_object_unref(tab_label_model);
        g_signal_connect(tree_sel, "changed", G_CALLBACK(on_tab_label_list_sel_changed), notebook);
        g_signal_connect(notebook, "switch-page", G_CALLBACK(on_notebook_page_changed), tree_sel);
        gtk_notebook_set_show_tabs(notebook, FALSE);

        g_signal_connect(pref_dlg, "response", G_CALLBACK(on_response), &pref_dlg);
        g_object_unref(builder);

        pcmanfm_ref();
        g_signal_connect(pref_dlg, "destroy", G_CALLBACK(pcmanfm_unref), NULL);
        if(parent)
            gtk_window_set_transient_for(pref_dlg, parent);
    }
    if(page < 0 || page >= gtk_notebook_get_n_pages(notebook))
        page = 0;
    gtk_notebook_set_current_page(notebook, page);
    gtk_window_present(pref_dlg);
}

