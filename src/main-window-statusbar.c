/*
 *      main-window-statusbar.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *      Copyright 2012 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
 *      Copyright 2013 - 2014 Vadim Ushakov <igeekless@gmail.com>
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

/*****************************************************************************/

static void update_statusbar(FmMainWin * win);

/*****************************************************************************/

static void on_statusbar_icon_scale_value_changed_handler(GtkRange * widget, FmMainWin * win)
{
    if (!win->folder_view)
        return;
    FmStandardView * view = FM_STANDARD_VIEW(win->folder_view);
    if (!view)
        return;

    int value = gtk_range_get_value(widget);

    {
        char * tooltip = g_strdup_printf(
            ngettext("Icon Size: %d pixel", "Icon Size: %d pixels", value),
            value);
        gtk_widget_set_tooltip_text(GTK_WIDGET(widget), tooltip);
        g_free(tooltip);
    }

    FmStandardViewModeSettings mode_settings = fm_standard_view_get_mode_settings(view);

    if (mode_settings.icon_size == value)
        return;

    gint * p = NULL;
    char * s = NULL;
    if (g_strcmp0(mode_settings.icon_size_id, "small_icon") == 0)
    {
        p = &fm_config->small_icon_size;
        s = "small_icon_size";
    }
    else if (g_strcmp0(mode_settings.icon_size_id, "big_icon") == 0)
    {
        p = &fm_config->big_icon_size;
        s = "big_icon_size";
    }
    else if (g_strcmp0(mode_settings.icon_size_id, "thumbnail") == 0)
    {
        p = &fm_config->thumbnail_size;
        s = "thumbnail_size";
    }

    if (p && *p != value)
    {
        *p = value;
        fm_config_emit_changed(fm_config, s);
    }
}

static void update_statusbar_icon_scale(FmMainWin * win)
{
    if (!app_config->show_zoom_slider)
        goto hide;

    if (!win->folder_view)
        goto hide;

    FmStandardView * view = FM_STANDARD_VIEW(win->folder_view);
    if (!view)
        goto hide;

    FmStandardViewModeSettings mode_settings = fm_standard_view_get_mode_settings(view);

    gint value = -1;

    if (g_strcmp0(mode_settings.icon_size_id, "small_icon") == 0)
        value = fm_config->small_icon_size;
    else if (g_strcmp0(mode_settings.icon_size_id, "big_icon") == 0)
        value = fm_config->big_icon_size;
    else if (g_strcmp0(mode_settings.icon_size_id, "thumbnail") == 0)
        value = fm_config->thumbnail_size;

    if (value < 0)
        goto hide;

    if (gtk_range_get_value(GTK_RANGE(win->statusbar.icon_scale)) != value)
    {
        gtk_range_set_value(GTK_RANGE(win->statusbar.icon_scale), value);
    }

    gtk_widget_show(win->statusbar.icon_scale);
    return;

hide:
    gtk_widget_hide(win->statusbar.icon_scale);
}

static void on_changed_icon_size(FmConfig * config, FmMainWin * win)
{
    update_statusbar_icon_scale(win);
}

/*****************************************************************************/

static gboolean on_statusbar_event_box_button_press_event(GtkWidget * widget, GdkEventButton * event, FmMainWin * win)
{
    if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
        do_popup_menu(widget, gtk_ui_manager_get_widget(win->ui, "/statusbar_popup"), event);
        return TRUE;
    }

    return FALSE;
}

static gboolean on_statusbar_event_box_popup_menu_handler(GtkWidget * widget, FmMainWin * win)
{
    do_popup_menu(widget, gtk_ui_manager_get_widget(win->ui, "/statusbar_popup"), NULL);
    return TRUE;
}

static void on_changed_show_space_information(FmConfig * config, FmMainWin * win)
{
    gtk_toggle_action_set_active(
        GTK_TOGGLE_ACTION(gtk_action_group_get_action(win->action_group, "StatusbarShowSpaceInformation")),
        app_config->show_space_information);
    gtk_action_set_sensitive(
        gtk_action_group_get_action(win->action_group, "StatusbarShowSpaceInformationInProgressBar"),
        app_config->show_space_information);
    update_statusbar(win);
}

static void on_changed_show_space_information_in_progress_bar(FmConfig * config, FmMainWin * win)
{
    gtk_toggle_action_set_active(
        GTK_TOGGLE_ACTION(gtk_action_group_get_action(win->action_group, "StatusbarShowSpaceInformationInProgressBar")),
        app_config->show_space_information_in_progress_bar);
    update_statusbar(win);
}

static void on_changed_show_zoom_slider(FmConfig * config, FmMainWin * win)
{
    gtk_toggle_action_set_active(
        GTK_TOGGLE_ACTION(gtk_action_group_get_action(win->action_group, "StatusbarShowZoomSlider")),
        app_config->show_zoom_slider);
    update_statusbar_icon_scale(win);
}


/*****************************************************************************/

static void fm_main_win_inititialize_statusbar(FmMainWin *win)
{
    gtk_rc_parse_string(
        "style \"stuurman-statusbar\" {\n"
        "  GtkStatusbar::shadow-type = GTK_SHADOW_NONE\n"
        "}\n"
        "class \"GtkStatusbar\" style:application \"stuurman-statusbar\"\n"
    );

    win->statusbar.event_box = gtk_event_box_new();

    win->statusbar.statusbar = (GtkStatusbar *) gtk_statusbar_new();
    gtk_container_add(GTK_CONTAINER(win->statusbar.event_box), (GtkWidget *) win->statusbar.statusbar);

    //gtk_widget_style_get(GTK_WIDGET(win->statusbar), "shadow-type", &shadow_type, NULL);

    {
        win->statusbar.icon_scale = gtk_hscale_new_with_range(0, 256, 4);
        gtk_scale_set_draw_value((GtkScale *) win->statusbar.icon_scale, FALSE);
        g_object_set(win->statusbar.icon_scale,
            "width-request", 128,
            "height-request", 16,
            NULL
        );
        gtk_box_pack_end(GTK_BOX(win->statusbar.statusbar), win->statusbar.icon_scale, FALSE, TRUE, 0);
    }

    {
        win->statusbar.volume_progress_bar = gtk_progress_bar_new();
        gtk_box_pack_end(GTK_BOX(win->statusbar.statusbar), win->statusbar.volume_progress_bar, FALSE, TRUE, 0);
    }

    {
        win->statusbar.volume_frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type((GtkFrame *) win->statusbar.volume_frame, GTK_SHADOW_NONE);
        gtk_box_pack_end(GTK_BOX(win->statusbar.statusbar), win->statusbar.volume_frame, FALSE, TRUE, 0);

        win->statusbar.volume_label = gtk_label_new(NULL);
        gtk_container_add(GTK_CONTAINER(win->statusbar.volume_frame), win->statusbar.volume_label);
    }

    win->statusbar.ctx = gtk_statusbar_get_context_id(win->statusbar.statusbar, "status");
    win->statusbar.ctx2 = gtk_statusbar_get_context_id(win->statusbar.statusbar, "status2");

    gtk_widget_show(win->statusbar.event_box);
    gtk_widget_show((GtkWidget *) win->statusbar.statusbar);

    g_signal_connect(win->statusbar.event_box, "button-press-event",
        G_CALLBACK(on_statusbar_event_box_button_press_event), win);
    g_signal_connect(win->statusbar.event_box, "popup-menu",
        G_CALLBACK(on_statusbar_event_box_popup_menu_handler), win);

    g_signal_connect(win->statusbar.icon_scale, "value-changed",
        G_CALLBACK(on_statusbar_icon_scale_value_changed_handler), win);

    win->statusbar.show_space_information_handler =
        g_signal_connect(fm_config, "changed::show_space_information", G_CALLBACK(on_changed_show_space_information), win);
    win->statusbar.show_space_information_in_progress_bar_handler =
        g_signal_connect(fm_config, "changed::show_space_information_in_progress_bar", G_CALLBACK(on_changed_show_space_information_in_progress_bar), win);
    win->statusbar.show_zoom_slider_handler =
        g_signal_connect(fm_config, "changed::show_zoom_slider", G_CALLBACK(on_changed_show_zoom_slider), win);
    win->statusbar.small_icon_size_handler =
        g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_changed_icon_size), win);
    win->statusbar.big_icon_size_handler =
        g_signal_connect(fm_config, "changed::big_icon_size", G_CALLBACK(on_changed_icon_size), win);
    win->statusbar.thumbnail_size_handler =
        g_signal_connect(fm_config, "changed::thumbnail_size", G_CALLBACK(on_changed_icon_size), win);

    on_changed_show_space_information(NULL, win);
    on_changed_show_space_information_in_progress_bar(NULL, win);
    on_changed_show_zoom_slider(NULL, win);
}

static void fm_main_win_destroy_statusbar(FmMainWin * win)
{
    g_signal_handlers_disconnect_by_func(win->statusbar.event_box, on_statusbar_event_box_button_press_event, win);
    g_signal_handlers_disconnect_by_func(win->statusbar.event_box, on_statusbar_event_box_popup_menu_handler, win);

    g_signal_handlers_disconnect_by_func(win->statusbar.icon_scale, on_statusbar_icon_scale_value_changed_handler, win);

    if (win->statusbar.show_space_information_handler)
    {
        g_signal_handler_disconnect(fm_config, win->statusbar.show_space_information_handler);
        win->statusbar.show_space_information_handler = 0;
    }

    if (win->statusbar.show_space_information_in_progress_bar_handler)
    {
        g_signal_handler_disconnect(fm_config, win->statusbar.show_space_information_in_progress_bar_handler);
        win->statusbar.show_space_information_in_progress_bar_handler = 0;
    }

    if (win->statusbar.show_zoom_slider_handler)
    {
        g_signal_handler_disconnect(fm_config, win->statusbar.show_zoom_slider_handler);
        win->statusbar.show_zoom_slider_handler = 0;
    }

    if (win->statusbar.small_icon_size_handler)
    {
        g_signal_handler_disconnect(fm_config, win->statusbar.small_icon_size_handler);
        win->statusbar.small_icon_size_handler = 0;
    }

    if (win->statusbar.big_icon_size_handler)
    {
        g_signal_handler_disconnect(fm_config, win->statusbar.big_icon_size_handler);
        win->statusbar.big_icon_size_handler = 0;
    }

    if (win->statusbar.thumbnail_size_handler)
    {
        g_signal_handler_disconnect(fm_config, win->statusbar.thumbnail_size_handler);
        win->statusbar.thumbnail_size_handler = 0;
    }

    win->statusbar.event_box = NULL;
    win->statusbar.statusbar = NULL;
    win->statusbar.volume_frame = NULL;
    win->statusbar.volume_label = NULL;
    win->statusbar.volume_progress_bar = NULL;
    win->statusbar.icon_scale = NULL;
}

/*****************************************************************************/

static void update_status(FmMainWin * win, guint type, const char*  status_text)
{
    if (!win->statusbar.statusbar)
        return;

    switch(type)
    {
        case FM_STATUS_TEXT_NORMAL:
        {
            gtk_statusbar_pop(win->statusbar.statusbar, win->statusbar.ctx);
            if (status_text)
                gtk_statusbar_push(win->statusbar.statusbar, win->statusbar.ctx, status_text);
            gtk_widget_set_tooltip_text(GTK_WIDGET(win->statusbar.statusbar), status_text);
            break;
        }
        case FM_STATUS_TEXT_SELECTED_FILES:
        {
            gtk_statusbar_pop(win->statusbar.statusbar, win->statusbar.ctx2);
            if (status_text)
                gtk_statusbar_push(win->statusbar.statusbar, win->statusbar.ctx2, status_text);
            gtk_widget_set_tooltip_text(GTK_WIDGET(win->statusbar.statusbar), status_text);
            break;
        }
        case FM_STATUS_TEXT_FS_INFO:
        {
            if (status_text && app_config->show_space_information)
            {
                if (app_config->show_space_information_in_progress_bar)
                {
                    gtk_widget_set_tooltip_text(win->statusbar.volume_progress_bar, status_text);
                    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(win->statusbar.volume_progress_bar), status_text);
                    gtk_progress_bar_set_fraction(
                        GTK_PROGRESS_BAR(win->statusbar.volume_progress_bar),
                        1 - fm_tab_page_volume_free_space_fraction(win->current_page));

                    if (!gtk_widget_get_visible(win->statusbar.volume_progress_bar))
                    {
                        GtkRequisition requisition;
                        gtk_widget_size_request(GTK_WIDGET(win->statusbar.statusbar), &requisition);
                        gtk_widget_set_size_request(win->statusbar.volume_progress_bar, -1, requisition.height);
                    }

                    gtk_widget_show(win->statusbar.volume_progress_bar);
                    gtk_widget_hide(win->statusbar.volume_frame);
                }
                else
                {
                    gtk_widget_set_tooltip_text(win->statusbar.volume_label, status_text);
                    gtk_label_set_text((GtkLabel *) win->statusbar.volume_label, status_text);
                    gtk_widget_show(win->statusbar.volume_frame);
                    gtk_widget_show(win->statusbar.volume_label);
                    gtk_widget_hide(win->statusbar.volume_progress_bar);
                }
            }
            else
            {
                gtk_widget_hide(GTK_WIDGET(win->statusbar.volume_frame));
                gtk_widget_hide(GTK_WIDGET(win->statusbar.volume_progress_bar));
            }
            break;
        }
    }
}

static void update_statusbar(FmMainWin * win)
{
    FmTabPage * page = win->current_page;

    if (!win->statusbar.statusbar)
        return;

    if (!page)
        return;

    const char * status_text;

    gtk_widget_set_tooltip_text(GTK_WIDGET(win->statusbar.statusbar), "");

    status_text = fm_tab_page_get_status_text(page, FM_STATUS_TEXT_NORMAL);
    update_status(win, FM_STATUS_TEXT_NORMAL, status_text);

    status_text = fm_tab_page_get_status_text(page, FM_STATUS_TEXT_SELECTED_FILES);
    update_status(win, FM_STATUS_TEXT_SELECTED_FILES, status_text);

    status_text = fm_tab_page_get_status_text(page, FM_STATUS_TEXT_FS_INFO);
    update_status(win, FM_STATUS_TEXT_FS_INFO, status_text);

    update_statusbar_icon_scale(win);
}

static void on_tab_page_status_text(FmTabPage* page, guint type, const char* status_text, FmMainWin* win)
{
    if (page != win->current_page)
        return;
    update_status(win, type, status_text);
}

/*****************************************************************************/

static void on_show_space_information(GtkToggleAction * action, FmMainWin * win)
{
    gboolean active = gtk_toggle_action_get_active(action);
    if (app_config->show_space_information != active)
    {
        app_config->show_space_information = active;
        fm_config_emit_changed(fm_config, "show_space_information");
    }
}

static void on_show_space_information_in_progress_bar(GtkToggleAction * action, FmMainWin * win)
{
    gboolean active = gtk_toggle_action_get_active(action);
    if (app_config->show_space_information_in_progress_bar != active)
    {
        app_config->show_space_information_in_progress_bar = active;
        fm_config_emit_changed(fm_config, "show_space_information_in_progress_bar");
    }
}

static void on_show_zoom_slider(GtkToggleAction * action, FmMainWin * win)
{
    gboolean active = gtk_toggle_action_get_active(action);
    if (app_config->show_zoom_slider != active)
    {
        app_config->show_zoom_slider = active;
        fm_config_emit_changed(fm_config, "show_zoom_slider");
    }
}
