/*
 *      wallpaper-manager.h
 *
 *      Copyright (c) 2013 Vadim Ushakov
 *      Copyright 2010 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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


#ifndef __WALLPAPER_MANAGER_H__
#define __WALLPAPER_MANAGER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS


/* TODO: оформить как класс */
#if 0
#define FM_TYPE_WALLPAPER_MANAGER             (fm_wallpaper_manager_get_type())
#define FM_WALLPAPER_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_TYPE_WALLPAPER_MANAGER, FmWallpaperManager))
#define FM_WALLPAPER_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_TYPE_WALLPAPER_MANAGER, FmWallpaperManagerClass))
#define FM_IS_WALLPAPER_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_TYPE_WALLPAPER_MANAGER))
#define FM_IS_WALLPAPER_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_TYPE_WALLPAPER_MANAGER))

typedef struct _FmWallpaperManager           FmWallpaperManager;
typedef struct _FmWallpaperManagerClass      FmWallpaperManagerClass;

struct _FmDesktop
{
    GObject parent;
    /*< private >*/
};

struct _FmWallpaperManagerClass
{
    GObjectClass parent_class;
};

typedef struct _FmDesktop           FmDesktop;

GType                fm_wallpaper_manager_get_type     (void);
FmWallpaperManager*  fm_wallpaper_manager__new         (FmDesktop* desktop);
#endif

extern void wallpaper_manager_update_background(FmDesktop* desktop, int is_it);
extern void wallpaper_manager_init();
extern void wallpaper_manager_finalize();

G_END_DECLS

#endif /* __WALLPAPER_MANAGER_H__ */
