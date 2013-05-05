/*
 *      desktop.c
 *
 *      Copyright 2010 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *      Copyright 2012 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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
#include <config.h>
#endif

#include "desktop.h"
#include "pcmanfm.h"
#include "app-config.h"

#include <glib/gi18n.h>

#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <math.h>

#include <cairo-xlib.h>

#include "pref.h"
#include "main-win.h"

#include "gseal-gtk-compat.h"

typedef struct _FmBackgroundCache FmBackgroundCache;

struct _FmBackgroundCache
{
    FmBackgroundCache *next;
    char *filename;
#if GTK_CHECK_VERSION(3, 0, 0)
    cairo_surface_t *bg;
#else
    GdkPixmap *bg;
#endif
    FmWallpaperMode wallpaper_mode;
};

static Atom XA_NET_WORKAREA = 0;
static Atom XA_NET_NUMBER_OF_DESKTOPS = 0;
static Atom XA_NET_CURRENT_DESKTOP = 0;
static Atom XA_XROOTMAP_ID = 0;
static Atom XA_XROOTPMAP_ID = 0;

static FmBackgroundCache* all_wallpapers = NULL;


void wallpaper_manager_update_background(FmDesktop* desktop, int is_it)
{
    GtkWidget* widget = (GtkWidget*)desktop;
    GdkPixbuf* pix, *scaled;
    cairo_t* cr;
    GdkWindow* root = gdk_screen_get_root_window(gtk_widget_get_screen(widget));
    GdkWindow *window = gtk_widget_get_window(widget);
    FmBackgroundCache *cache;
#if GTK_CHECK_VERSION(3, 0, 0)
    cairo_pattern_t *pattern;
#endif

    Display* xdisplay;
    Pixmap xpixmap = 0;
    Window xroot;
    int screen_num;

    char *wallpaper;

    if (!app_config->wallpaper_common)
    {
        guint32 cur_desktop = desktop->cur_desktop;

        if(is_it >= 0) /* signal "changed::wallpaper" */
        {
            if((gint)cur_desktop >= app_config->wallpapers_configured)
            {
                register int i;

                app_config->wallpapers = g_renew(char *, app_config->wallpapers, cur_desktop + 1);
                for(i = MAX(app_config->wallpapers_configured,0); i <= (gint)cur_desktop; i++)
                    app_config->wallpapers[i] = NULL;
                app_config->wallpapers_configured = cur_desktop + 1;
            }
            wallpaper = app_config->wallpaper;
            g_free(app_config->wallpapers[cur_desktop]);
            app_config->wallpapers[cur_desktop] = g_strdup(wallpaper);
        }
        else /* desktop refresh */
        {
            if((gint)cur_desktop < app_config->wallpapers_configured)
                wallpaper = app_config->wallpapers[cur_desktop];
            else
                wallpaper = NULL;
            g_free(app_config->wallpaper); /* update to current desktop */
            app_config->wallpaper = g_strdup(wallpaper);
        }
    }
    else
        wallpaper = app_config->wallpaper;

    if(app_config->wallpaper_mode != FM_WP_COLOR && wallpaper && *wallpaper)
    {
        for(cache = all_wallpapers; cache; cache = cache->next)
            if(strcmp(wallpaper, cache->filename) == 0)
                break;
        if(cache && cache->wallpaper_mode == app_config->wallpaper_mode)
            pix = NULL; /* no new pix for it */
        else if((pix = gdk_pixbuf_new_from_file(wallpaper, NULL)))
        {
            if(cache)
            {
                /* the same file but mode was changed */
#if GTK_CHECK_VERSION(3, 0, 0)
                cairo_surface_destroy(cache->bg);
#else
                g_object_unref(cache->bg);
#endif
                cache->bg = NULL;
            }
            else if(all_wallpapers)
            {
                for(cache = all_wallpapers; cache->next; )
                    cache = cache->next;
                cache->next = g_new0(FmBackgroundCache, 1);
                cache = cache->next;
            }
            else
                all_wallpapers = cache = g_new0(FmBackgroundCache, 1);
            if(!cache->filename)
                cache->filename = g_strdup(wallpaper);
            g_debug("adding new FmBackgroundCache for %s", wallpaper);
        }
        else
            /* if there is a cached image but with another mode and we cannot
               get it from file for new mode then just leave it in cache as is */
            cache = NULL;
    }
    else
        cache = NULL;

    if(!cache) /* solid color only */
    {
#if GTK_CHECK_VERSION(3, 0, 0)
        pattern = cairo_pattern_create_rgb(app_config->desktop_bg.red / 65535.0,
                                           app_config->desktop_bg.green / 65535.0,
                                           app_config->desktop_bg.blue / 65535.0);
        gdk_window_set_background_pattern(window, pattern);
        cairo_pattern_destroy(pattern);
#else
        GdkColor bg = app_config->desktop_bg;

        gdk_colormap_alloc_color(gdk_drawable_get_colormap(window), &bg, FALSE, TRUE);
        gdk_window_set_back_pixmap(window, NULL, FALSE);
        gdk_window_set_background(window, &bg);
#endif
        gdk_window_invalidate_rect(window, NULL, TRUE);
        return;
    }

    if(!cache->bg) /* no cached image found */
    {
        int src_w, src_h;
        int dest_w, dest_h;
        int x = 0, y = 0;
        src_w = gdk_pixbuf_get_width(pix);
        src_h = gdk_pixbuf_get_height(pix);
        if(app_config->wallpaper_mode == FM_WP_TILE)
        {
            dest_w = src_w;
            dest_h = src_h;
        }
        else
        {
            GdkScreen* screen = gtk_widget_get_screen(widget);
            GdkRectangle geom;
            gdk_screen_get_monitor_geometry(screen, desktop->monitor, &geom);
            dest_w = geom.width;
            dest_h = geom.height;
        }
#if GTK_CHECK_VERSION(3, 0, 0)
        cache->bg = cairo_image_surface_create(CAIRO_FORMAT_RGB24, dest_w, dest_h);
        cr = cairo_create(cache->bg);
#else
        cache->bg = gdk_pixmap_new(window, dest_w, dest_h, -1);
        cr = gdk_cairo_create(cache->bg);
#endif
        if(gdk_pixbuf_get_has_alpha(pix)
            || app_config->wallpaper_mode == FM_WP_CENTER
            || app_config->wallpaper_mode == FM_WP_FIT)
        {
            gdk_cairo_set_source_color(cr, &app_config->desktop_bg);
            cairo_rectangle(cr, 0, 0, dest_w, dest_h);
            cairo_fill(cr);
        }

        switch(app_config->wallpaper_mode)
        {
        case FM_WP_TILE:
            break;
        case FM_WP_STRETCH:
            if(dest_w == src_w && dest_h == src_h)
                scaled = (GdkPixbuf*)g_object_ref(pix);
            else
                scaled = gdk_pixbuf_scale_simple(pix, dest_w, dest_h, GDK_INTERP_BILINEAR);
            g_object_unref(pix);
            pix = scaled;
            break;
        case FM_WP_FIT:
            if(dest_w != src_w || dest_h != src_h)
            {
                gdouble w_ratio = (float)dest_w / src_w;
                gdouble h_ratio = (float)dest_h / src_h;
                gdouble ratio = MIN(w_ratio, h_ratio);
                if(ratio != 1.0)
                {
                    src_w *= ratio;
                    src_h *= ratio;
                    scaled = gdk_pixbuf_scale_simple(pix, src_w, src_h, GDK_INTERP_BILINEAR);
                    g_object_unref(pix);
                    pix = scaled;
                }
            }
            /* continue to execute code in case FM_WP_CENTER */
        case FM_WP_CENTER:
            x = (dest_w - src_w)/2;
            y = (dest_h - src_h)/2;
            break;
        case FM_WP_COLOR: ; /* handled above */
        }
        gdk_cairo_set_source_pixbuf(cr, pix, x, y);
        cairo_paint(cr);
        cairo_destroy(cr);
        cache->wallpaper_mode = app_config->wallpaper_mode;
    }
#if GTK_CHECK_VERSION(3, 0, 0)
    pattern = cairo_pattern_create_for_surface(cache->bg);
    gdk_window_set_background_pattern(window, pattern);
    cairo_pattern_destroy(pattern);
#else
    gdk_window_set_back_pixmap(window, cache->bg, FALSE);
#endif

    /* set root map here */
    screen_num = gdk_screen_get_number(gtk_widget_get_screen(widget));
    xdisplay = GDK_WINDOW_XDISPLAY(root);
    xroot = RootWindow(xdisplay, screen_num);

#if GTK_CHECK_VERSION(3, 0, 0)
    xpixmap = cairo_xlib_surface_get_drawable(cache->bg);
#else
    xpixmap = GDK_WINDOW_XWINDOW(cache->bg);
#endif

    XChangeProperty(xdisplay, GDK_WINDOW_XID(root),
                    XA_XROOTMAP_ID, XA_PIXMAP, 32, PropModeReplace, (guchar*)&xpixmap, 1);

    XGrabServer (xdisplay);

#if 0
    result = XGetWindowProperty (display,
                                 RootWindow (display, screen_num),
                                 gdk_x11_get_xatom_by_name ("ESETROOT_PMAP_ID"),
                                 0L, 1L, False, XA_PIXMAP,
                                 &type, &format, &nitems,
                                 &bytes_after,
                                 &data_esetroot);

    if (data_esetroot != NULL) {
            if (result == Success && type == XA_PIXMAP &&
                format == 32 &&
                nitems == 1) {
                    gdk_error_trap_push ();
                    XKillClient (display, *(Pixmap *)data_esetroot);
                    gdk_error_trap_pop_ignored ();
            }
            XFree (data_esetroot);
    }

    XChangeProperty (display, RootWindow (display, screen_num),
                     gdk_x11_get_xatom_by_name ("ESETROOT_PMAP_ID"),
                     XA_PIXMAP, 32, PropModeReplace,
                     (guchar *) &xpixmap, 1);
#endif

    XChangeProperty(xdisplay, xroot, XA_XROOTPMAP_ID, XA_PIXMAP, 32,
                    PropModeReplace, (guchar*)&xpixmap, 1);

    XSetWindowBackgroundPixmap(xdisplay, xroot, xpixmap);
    XClearWindow(xdisplay, xroot);

    XFlush(xdisplay);
    XUngrabServer(xdisplay);

    if(pix)
        g_object_unref(pix);

    gdk_window_invalidate_rect(window, NULL, TRUE);
}

void wallpaper_manager_init()
{
    char* atom_names[] = {"_NET_WORKAREA", "_NET_NUMBER_OF_DESKTOPS",
                          "_NET_CURRENT_DESKTOP", "_XROOTMAP_ID", "_XROOTPMAP_ID"};
    Atom atoms[G_N_ELEMENTS(atom_names)] = {0};

    if(XInternAtoms(gdk_x11_get_default_xdisplay(), atom_names,
                    G_N_ELEMENTS(atom_names), False, atoms))
    {
        XA_NET_WORKAREA = atoms[0];
        XA_NET_NUMBER_OF_DESKTOPS = atoms[1];
        XA_NET_CURRENT_DESKTOP = atoms[2];
        XA_XROOTMAP_ID = atoms[3];
        XA_XROOTPMAP_ID = atoms[4];
    }

}

void wallpaper_manager_finalize()
{
    while(all_wallpapers)
    {
        FmBackgroundCache *bg = all_wallpapers;

        all_wallpapers = bg->next;
#if GTK_CHECK_VERSION(3, 0, 0)
        cairo_surface_destroy(bg->bg);
#else
        g_object_unref(bg->bg);
#endif
        g_free(bg->filename);
        g_free(bg);
    }
}
