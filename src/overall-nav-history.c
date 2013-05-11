/*
 *      overall-nav-history.c
 *
 *      Copyright 2013 Vadim Ushakov
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

#include "overall-nav-history.h"
#include "app-config.h"
#include "pcmanfm.h"

FmNavHistory* overall_nav_history = NULL;

static guint overall_nav_history_save_id = 0;


void overall_nav_history_initialize()
{
    if (overall_nav_history)
        return;

    overall_nav_history = fm_nav_history_new();
    fm_nav_history_set_max(overall_nav_history, 25);
    fm_nav_history_set_allow_duplicates(overall_nav_history, FALSE);
    char * cache_dir = pcmanfm_get_cache_dir(FALSE);
    char * nav_history_file = g_build_filename(cache_dir, "navigation_history", NULL);
    if (g_file_test(nav_history_file, G_FILE_TEST_IS_REGULAR))
    {
        GSList * list = g_slist_reverse(read_list_from_file(nav_history_file, FALSE));
        GSList * l;
        for (l = list; l; l = l->next)
        {
            FmPath* path = fm_path_new_for_str(l->data);
            overall_nav_history_add_path(path);
            fm_path_unref(path);
        }
    }
    g_free(nav_history_file);
    g_free(cache_dir);
}

static gboolean _save_overall_nav_history(gpointer user_data)
{
    /* Do nothing, if saving of the history is not scheduled. */
    if (!overall_nav_history_save_id)
        return FALSE;

    const GList * l;
    GString * string = g_string_new("");

    for (l = fm_nav_history_list(overall_nav_history); l; l = l->next)
    {
        const FmNavHistoryItem * item = (FmNavHistoryItem *)l->data;
        FmPath * path = item->path;
        char * str = fm_path_to_str(path);
        g_string_append_printf(string, "%s\n", str);
        g_free(str);
    }

    char * cache_dir = pcmanfm_get_cache_dir(TRUE);
    char * nav_history_file = g_build_filename(cache_dir, "navigation_history", NULL);

    g_file_set_contents(nav_history_file, string->str, -1, NULL);

    g_free(nav_history_file);
    g_free(cache_dir);

    g_string_free(string, TRUE);

    overall_nav_history_save_id = 0;

    return FALSE;
}

void save_overall_nav_history()
{
    _save_overall_nav_history(NULL);
}

gboolean overall_nav_history_add_path(FmPath * path)
{
    if (fm_nav_history_chdir(overall_nav_history, path, 0))
    {
        if (!overall_nav_history_save_id)
            overall_nav_history_save_id = g_timeout_add(10 * 1000, _save_overall_nav_history, NULL);
        return TRUE;
    }
    return FALSE;
}
