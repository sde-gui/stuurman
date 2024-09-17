/*
 *      pcmanfm.c
 *
 *      Copyright 2009 - 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <stdio.h>
#include <glib/gi18n.h>

#include <stdlib.h>
#include <string.h>
/* socket is used to keep single instance */
#include <sys/types.h>
#include <signal.h>
#include <unistd.h> /* for getcwd */

#include <libsmfm-gtk/fm-gtk.h>
#include "app-config.h"
#include "main-win.h"
#include "volume-manager.h"
#include "pref.h"
#include "pcmanfm.h"
#include "single-inst.h"

static int signal_pipe[2] = {-1, -1};
static gboolean daemon_mode = FALSE;
static guint save_config_idle = 0;

static char** files_to_open = NULL;
static int n_files_to_open = 0;
static char* profile = NULL;
static gboolean no_desktop = FALSE;
/* static gboolean new_tab = FALSE; */
static gboolean preferences = FALSE;
static gboolean new_win = FALSE;
static gboolean find_files = FALSE;
static char* ipc_cwd = NULL;
static char* window_role = NULL;

static int n_pcmanfm_ref = 0;

static const char * my_command = NULL;

static GOptionEntry opt_entries[] =
{
    /* options only acceptable by first pcmanfm instance. These options are not passed through IPC */
    { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile, N_("Name of configuration profile"), N_("PROFILE") },
    { "daemon-mode", 'd', 0, G_OPTION_ARG_NONE, &daemon_mode, N_("Run PCManFM as a daemon"), NULL },
    { "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop, N_("No function. Just to be compatible with nautilus"), NULL },

    { "preferences", '\0', 0, G_OPTION_ARG_NONE, &preferences, N_("Open Preferences dialog"), NULL },
    { "new-win", 'n', 0, G_OPTION_ARG_NONE, &new_win, N_("Open new window"), NULL },
    /* { "find-files", 'f', 0, G_OPTION_ARG_NONE, &find_files, N_("Open Find Files utility"), NULL }, */
    { "role", '\0', 0, G_OPTION_ARG_STRING, &window_role, N_("Window role for usage by window manager"), N_("ROLE") },
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files_to_open, NULL, N_("[FILE1, FILE2,...]")},
    { NULL }
};

static gboolean pcmanfm_run(gint screen_num);

/* it's not safe to call gtk+ functions in unix signal handler
 * since the process is interrupted here and the state of gtk+ is unpredictable. */
static void unix_signal_handler(int sig_num)
{
    /* postpond the signal handling by using a pipe */
    if (write(signal_pipe[1], &sig_num, sizeof(sig_num)) != sizeof(sig_num)) {
        g_critical("cannot bounce the signal, stop");
        _exit(2);
    }
}

static gboolean on_unix_signal(GIOChannel* ch, GIOCondition cond, gpointer user_data)
{
    int sig_num;
    GIOStatus status;
    gsize got;

    while(1)
    {
        status = g_io_channel_read_chars(ch, (gchar*)&sig_num, sizeof(sig_num),
                                         &got, NULL);
        if(status == G_IO_STATUS_AGAIN) /* we read all the pipe */
        {
            g_debug("got G_IO_STATUS_AGAIN");
            return TRUE;
        }
        if(status != G_IO_STATUS_NORMAL || got != sizeof(sig_num)) /* broken pipe */
        {
            g_debug("signal pipe is broken");
            gtk_main_quit();
            return FALSE;
        }
        g_debug("got signal %d from pipe", sig_num);
        switch(sig_num)
        {
        case SIGTERM:
        default:
            gtk_main_quit();
            return FALSE;
        }
    }
    return TRUE;
}

static void single_inst_cb(const char* cwd, int screen_num)
{
    g_free(ipc_cwd);
    ipc_cwd = g_strdup(cwd);

    if(files_to_open)
    {
        int i;
        n_files_to_open = g_strv_length(files_to_open);
        /* canonicalize filename if needed. */
        for(i = 0; i < n_files_to_open; ++i)
        {
            char* file = files_to_open[i];
            char* scheme = g_uri_parse_scheme(file);
            g_debug("file: %s", file);
            if(scheme) /* a valid URI */
            {
                g_free(scheme);
            }
            else /* a file path */
            {
                files_to_open[i] = fm_canonicalize_filename(file, cwd);
                g_free(file);
            }
        }
    }
    pcmanfm_run(screen_num);
    window_role = NULL; /* reset it for clients callbacks */
}

int main(int argc, char** argv)
{
    FmConfig* config;
    GError* err = NULL;
    SingleInstData inst;

#ifdef ENABLE_NLS
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    /* initialize GTK+ and parse the command line arguments */
    if(G_UNLIKELY(!gtk_init_with_args(&argc, &argv, " ", opt_entries, GETTEXT_PACKAGE, &err)))
    {
        g_printf("%s\n", err->message);
        g_error_free(err);
        return 1;
    }

    /* ensure that there is only one instance of pcmanfm. */
    inst.prog_name = "stuurman";
    inst.cb = single_inst_cb;
    inst.opt_entries = opt_entries + 3;
    inst.screen_num = gdk_x11_get_default_screen();
    switch(single_inst_init(&inst))
    {
    case SINGLE_INST_CLIENT: /* we're not the first instance. */
        single_inst_finalize(&inst);
        gdk_notify_startup_complete();
        return 0;
    case SINGLE_INST_ERROR: /* error happened. */
        single_inst_finalize(&inst);
        return 1;
    case SINGLE_INST_SERVER: ; /* FIXME */
    }

    if(pipe(signal_pipe) == 0)
    {
        GIOChannel* ch = g_io_channel_unix_new(signal_pipe[0]);
        g_io_add_watch(ch, G_IO_IN|G_IO_PRI, (GIOFunc)on_unix_signal, NULL);
        g_io_channel_set_encoding(ch, NULL, NULL);
        g_io_channel_unref(ch);

        /* intercept signals */
        // signal( SIGPIPE, SIG_IGN );
        signal( SIGHUP, unix_signal_handler );
        signal( SIGTERM, unix_signal_handler );
        signal( SIGINT, unix_signal_handler );
    }

    config = fm_app_config_new(); /* this automatically load libfm config file. */

    fm_gtk_init(config);

    /* load pcmanfm-specific config file */
    fm_app_config_load_from_profile(FM_APP_CONFIG(config), profile);

    /* the main part */
    if(pcmanfm_run(gdk_screen_get_number(gdk_screen_get_default())))
    {
        window_role = NULL; /* reset it for clients callbacks */
        fm_volume_manager_init();
        GDK_THREADS_ENTER();
        gtk_main();
        GDK_THREADS_LEAVE();
        /* g_debug("main loop ended"); */

        pcmanfm_save_config(TRUE);
        if(save_config_idle)
        {
            g_source_remove(save_config_idle);
            save_config_idle = 0;
        }
        fm_volume_manager_finalize();
    }

    single_inst_finalize(&inst);
    fm_gtk_finalize();

    g_object_unref(config);
    return 0;
}

gboolean pcmanfm_run(gint screen_num)
{
    FmMainWin *win;
    gboolean ret = TRUE;

    if (!files_to_open)
    {
        if (preferences)
        {
            /* FIXME: pass screen number from client */
            fm_edit_preference(NULL, 0);
            preferences = FALSE;
            return TRUE;
        }
    }

    if(G_UNLIKELY(find_files))
    {
        /* FIXME: find files */
    }
    else
    {
        if(files_to_open)
        {
            char** filename;
            FmPath* cwd = NULL;
            GList* paths = NULL;
            for(filename=files_to_open; *filename; ++filename)
            {
                FmPath* path;
                if( **filename == '/') /* absolute path */
                    path = fm_path_new_for_path(*filename);
                else if(strstr(*filename, ":/") ) /* URI */
                    path = fm_path_new_for_uri(*filename);
                else if( strcmp(*filename, "~") == 0 ) /* special case for home dir */
                {
                    path = fm_path_get_home();
                    win = fm_main_win_add_win(NULL, path);
                    if(new_win && window_role)
                        gtk_window_set_role(GTK_WINDOW(win), window_role);
                    continue;
                }
                else /* basename */
                {
                    if(G_UNLIKELY(!cwd))
                    {
                        char* cwd_str = g_get_current_dir();
                        cwd = fm_path_new_for_str(cwd_str);
                        g_free(cwd_str);
                    }
                    path = fm_path_new_relative(cwd, *filename);
                }
                paths = g_list_append(paths, path);
            }
            if(cwd)
                fm_path_unref(cwd);
            fm_launch_paths_simple(NULL, NULL, paths, pcmanfm_open_folder, NULL);
            g_list_free_full(paths, (GDestroyNotify) fm_path_unref);
            ret = (n_pcmanfm_ref >= 1); /* if there is opened window, return true to run the main loop. */

            g_strfreev(files_to_open);
            files_to_open = NULL;
        }
        else
        {
            static gboolean first_run = TRUE;
            if(first_run && daemon_mode)
            {
                /* If the function is called the first time and we're in daemon mode,
               * don't open any folder.
               * Checking if pcmanfm_run() is called the first time is needed to fix
               * #3397444 - pcmanfm dont show window in daemon mode if i call 'pcmanfm' */
            }
            else
            {
                /* If we're not in daemon mode, or pcmanfm_run() is called because another
               * instance send signal to us, open cwd by default. */
                FmPath* path;
                char* cwd = ipc_cwd ? ipc_cwd : g_get_current_dir();
                path = fm_path_new_for_path(cwd);
                win = fm_main_win_add_win(NULL, path);
                if(new_win && window_role)
                    gtk_window_set_role(GTK_WINDOW(win), window_role);
                fm_path_unref(path);
                g_free(cwd);
                ipc_cwd = NULL;
            }
            first_run = FALSE;
        }
    }
    return ret;
}

/* After opening any window/dialog/tool, this should be called. */
void pcmanfm_ref()
{
    ++n_pcmanfm_ref;
    /* g_debug("ref: %d", n_pcmanfm_ref); */
}

/* After closing any window/dialog/tool, this should be called.
 * If the last window is closed and we are not a deamon, pcmanfm will quit.
 */
void pcmanfm_unref()
{
    --n_pcmanfm_ref;
    /* g_debug("unref: %d, daemon_mode=%d, desktop_running=%d", n_pcmanfm_ref, daemon_mode, desktop_running); */
    if( 0 == n_pcmanfm_ref && !daemon_mode)
        gtk_main_quit();
}

gboolean pcmanfm_open_folder(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    GList* l = folder_infos;
    if(new_win)
    {
        FmMainWin *win = fm_main_win_add_win(NULL,
                                fm_file_info_get_path((FmFileInfo*)l->data));
        if(window_role)
            gtk_window_set_role(GTK_WINDOW(win), window_role);
        new_win = FALSE;
        l = l->next;
    }
    for(; l; l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_main_win_open_in_last_active(fm_file_info_get_path(fi));
    }
    return TRUE;
}

static gboolean on_save_config_idle(gpointer user_data)
{
    pcmanfm_save_config(TRUE);
    save_config_idle = 0;
    return FALSE;
}

void pcmanfm_save_config(gboolean immediate)
{
    if(immediate)
    {
        fm_config_save(fm_config, NULL);
        fm_app_config_save_profile(app_config, profile);
    }
    else
    {
        /* install an idle handler to save the config file. */
        if( 0 == save_config_idle)
            save_config_idle = gdk_threads_add_idle_full(G_PRIORITY_LOW, (GSourceFunc)on_save_config_idle, NULL, NULL);
    }
}

void pcmanfm_open_folder_in_terminal(GtkWindow* parent, FmPath* dir)
{
    GAppInfo* app;
    char** argv;
    int argc;
    if(!fm_config->terminal)
    {
        fm_show_error(parent, NULL, _("Terminal emulator is not set."));
        fm_edit_preference(parent, PREF_ADVANCED);
        return;
    }
    if(!g_shell_parse_argv(fm_config->terminal, &argc, &argv, NULL))
        return;
    app = g_app_info_create_from_commandline(argv[0], NULL, 0, NULL);
    g_strfreev(argv);
    if(app)
    {
        GError* err = NULL;
        GdkAppLaunchContext* ctx = gdk_app_launch_context_new();
        char* cwd_str;
        char* old_cwd = g_get_current_dir();
        char* old_pwd = g_strdup(g_getenv("PWD"));

        if(fm_path_is_native(dir))
            cwd_str = fm_path_to_str(dir);
        else
        {
            GFile* gf = fm_path_to_gfile(dir);
            cwd_str = g_file_get_path(gf);
            g_object_unref(gf);
        }
        gdk_app_launch_context_set_screen(ctx, parent ? gtk_widget_get_screen(GTK_WIDGET(parent)) : gdk_screen_get_default());
        gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());
        g_chdir(cwd_str); /* FIXME: currently we don't have better way for this. maybe a wrapper script? */
        g_setenv("PWD", cwd_str, TRUE);
        g_free(cwd_str);

        if(!g_app_info_launch(app, NULL, G_APP_LAUNCH_CONTEXT(ctx), &err))
        {
            fm_show_error(parent, NULL, err->message);
            g_error_free(err);
        }
        g_object_unref(ctx);
        g_object_unref(app);

        /* switch back to old cwd and fix #3114626 - PCManFM 0.9.9 Umount partitions problem */
        g_chdir(old_cwd); /* This is really dirty, but we don't have better solution now. */
        g_setenv("PWD", old_pwd, TRUE);
        g_free(old_cwd);
        g_free(old_pwd);
    }
}

static char* pcmanfm_get_dir(const char * base, gboolean create)
{
    char* dir = g_build_filename(base, config_app_name(), profile ? profile : "default", NULL);
    if(create)
        g_mkdir_with_parents(dir, 0700);
    return dir;
}

char* pcmanfm_get_profile_dir(gboolean create)
{
    return pcmanfm_get_dir(g_get_user_config_dir(), create);
}

char* pcmanfm_get_cache_dir(gboolean create)
{
    return pcmanfm_get_dir(g_get_user_cache_dir(), create);
}

const char * config_app_name(void)
{
    return "stuurman";
}

const char * pcmanfm_get_my_command(void)
{
    return my_command;
}
