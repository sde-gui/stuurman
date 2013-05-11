/*
 *      utils.c
 *      
 *      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include "utils.h"

#include <string.h>
#include <glib.h>

gchar * ellipsize_string(char * s, int max_size)
{
    int l = strlen(s);

    if (l < max_size)
        return g_strdup(s);

    l = g_utf8_strlen(s, -1);

    if (l < max_size)
        return g_strdup(s);

    int left_chunk_size = (max_size - 3) / 2;
    int right_chunk_size = (max_size - 3) - left_chunk_size;

    gchar * left_chunk = g_utf8_substring(s, 0, left_chunk_size - 1);
    gchar * right_chunk = g_utf8_substring(s, l - right_chunk_size, l - 1);

    gchar * result = g_strconcat(left_chunk, "...", right_chunk, NULL);

    g_free(left_chunk);
    g_free(right_chunk);

    return result;
}

