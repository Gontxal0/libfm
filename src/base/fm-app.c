//      fm-app.c
//
//      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

#include "fm-app.h"
#include "fm-config.h"
#include <string.h>

static void fm_app_info_iface_init(GAppInfoIface *iface);

G_DEFINE_TYPE_WITH_CODE(FmApp, fm_app, G_TYPE_OBJECT,
             G_IMPLEMENT_INTERFACE (G_TYPE_APP_INFO,
                        fm_app_info_iface_init))

static void fm_app_finalize(GObject *object)
{
    FmApp *app;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_APP(object));

    app = FM_APP(object);

    g_free(app->name);
    g_free(app->comment);
    g_free(app->icon_name);
    g_free(app->path);
    if(app->icon)
        g_object_unref(app->icon);
    if(app->type == FM_APP_APPLICATION)
        g_free(app->exec);
    else if(app->type == FM_APP_LINK)
        g_free(app->url);

    g_slice_free(FmApp, app);

    G_OBJECT_CLASS(fm_app_parent_class)->finalize(object);
}


static void fm_app_init(FmApp *app)
{

}

GAppInfo* fm_app_new(const char* desktop_id)
{
    FmApp* app;
    char* rel_path = g_strconcat("applications/", desktop_id, NULL);
    GKeyFile* kf = g_key_file_new();
    if(g_key_file_load_from_data_dirs(kf, rel_path, NULL, 0, NULL))
    {
        app = fm_app_new_for_keyfile(kf);
        app->id = g_strdup(desktop_id);
    }
    else
        app = NULL;
    g_free(rel_path);
    g_key_file_free(kf);
    return (GAppInfo*)app;
}

GAppInfo* fm_app_new_for_file(const char* filename)
{
    FmApp* app;
    GKeyFile* kf = g_key_file_new();
    if(g_key_file_load_from_file(kf, filename, 0, NULL))
        app = fm_app_new_for_keyfile(kf);
    else
        app = NULL;
    g_key_file_free(kf);
    return (GAppInfo*)app;
}

GAppInfo* fm_app_new_for_keyfile(GKeyFile* kf)
{
    FmApp* app = NULL;
    FmAppType type = FM_APP_TYPE_NONE;
    char* str;
    str = g_key_file_get_string(kf, "Desktop Entry", "Type", NULL);
    if(str)
    {
        if(strcmp(str, "Application") == 0)
            type = FM_APP_APPLICATION;
        else if(strcmp(str, "Link") == 0)
            type = FM_APP_LINK;
        /* note: "Directory" is not supported */
        g_free(str);
    }

    if(type != FM_APP_TYPE_NONE)
    {
        char** des;
        gsize n_des;

        app = (FmApp*)g_object_new(FM_TYPE_APP, NULL);
        app->name = g_key_file_get_locale_string(kf, "Desktop Entry", "Name", NULL, NULL);
        app->comment = g_key_file_get_locale_string(kf, "Desktop Entry", "Comment", NULL, NULL);
        app->icon_name = g_key_file_get_locale_string(kf, "Desktop Entry", "Icon", NULL, NULL);
        if(app->icon_name[0] != '/') /* this is a icon name, not a full path to icon file. */
        {
            char* dot = strrchr(app->icon_name, '.');
            /* remove file extension */
            if(dot)
            {
                ++dot;
                if(strcmp(dot, "png") == 0 ||
                   strcmp(dot, "svg") == 0 ||
                   strcmp(dot, "xpm") == 0)
                   *(dot-1) = '\0';
            }
        }
        if(type == FM_APP_APPLICATION)
        {
            app->exec = g_key_file_get_string(kf, "Desktop Entry", "Exec", NULL);
            app->path = g_key_file_get_string(kf, "Desktop Entry", "Path", NULL);
        }
        else if(type == FM_APP_LINK)
            app->url = g_key_file_get_string(kf, "Desktop Entry", "URL", NULL);

        app->use_startup_notification = g_key_file_get_boolean(kf, "Desktop Entry", "StartupNotify", NULL);
        app->use_terminal = g_key_file_get_boolean(kf, "Desktop Entry", "Terminal", NULL);
        app->is_hidden = g_key_file_get_boolean(kf, "Desktop Entry", "Hidden", NULL);

        des = g_key_file_get_string_list(kf, "Desktop Entry", "OnlyShowIn", &n_des, NULL);
        if(des)
        {

            g_strfreev(des);
        }
        else
        {
            des = g_key_file_get_string_list(kf, "Desktop Entry", "NotShowIn", &n_des, NULL);
            if(des)
            {
                g_strfreev(des);
            }
        }
    }
    return (GAppInfo*)app;
}

static GAppInfo * fm_app_dup(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    FmApp* app2 = g_object_new(FM_TYPE_APP, NULL);

    app2->type = app->type;
    app2->id = g_strdup(app->id);
    app2->name = g_strdup(app->name);
    app2->comment = g_strdup(app->comment);
    app2->icon_name = g_strdup(app->icon_name);
    app2->icon = app->icon ? (GIcon*)g_object_ref(app->icon) : NULL;
    app2->path = g_strdup(app->path);
    if(app->type == FM_APP_APPLICATION)
        app2->exec = g_strdup(app->exec);
    else if(app->type == FM_APP_LINK)
        app2->url = g_strdup(app->url);
    app2->use_startup_notification = app->use_startup_notification;
    app2->use_terminal = app->use_terminal;
    app2->is_hidden = app->is_hidden;
    app2->show_in = app->show_in;

    return (GAppInfo*)app2;
}

static gboolean fm_app_equal(GAppInfo *appinfo1, GAppInfo *appinfo2)
{
    FmApp* app1 = FM_APP(appinfo1);
    FmApp* app2 = FM_APP(appinfo2);
    if(!app1 || app2)
        return FALSE;
    return (g_strcmp0(app1->id, app2->id) == 0);
}

static const char * fm_app_get_id(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    return app->id;
}

static const char * fm_app_get_name(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    return app->name;
}

static const char *fm_app_get_description(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    return app->comment;
}

static const char * fm_app_get_executable(GAppInfo *appinfo)
{
    /* FIXME: */
    return NULL;
}

static GIcon * fm_app_get_icon(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    if(!app->icon && app->icon_name)
    {
        /* NOTE: application icons are not as frequently used as file icons.
         * So we don't use FmIcon here since caching is not needed. */
        if(g_path_is_absolute(app->icon_name))
        {
            GFile* gf = g_file_new_for_path(app->icon_name);
            app->icon = g_file_icon_new(gf);
            g_object_unref(gf);
        }
        else
            app->icon = g_themed_icon_new(app->icon_name);
    }
    return app->icon;
}

const char* fm_app_get_icon_name(FmApp* app)
{
    return app->icon_name;
}

gboolean fm_app_get_use_startup_notification(FmApp* app)
{
    return app->use_startup_notification;
}

gboolean fm_app_get_use_terminal(FmApp* app)
{
    return app->use_terminal;
}


gboolean fm_app_is_deleted(FmApp* app)
{
    return app->is_hidden;
}

/* launch a list of FmFileInfo or FmPath objects */
/*
gboolean fm_app_launch_files(FmApp* app, FmFileInfoList* fis, GAppLaunchContext* ctx, GError** err)
{
    return FALSE;
}

gboolean fm_app_launch_paths(FmApp* app, FmPathList* paths, GAppLaunchContext* ctx, GError** err)
{
    return FALSE;
}
*/

static void append_file_to_cmd(GFile* gf, GString* cmd)
{
    char* file = g_file_get_path(gf);
    char* quote = g_shell_quote(file);
    g_string_append(cmd, quote);
    g_string_append_c(cmd, ' ');
    g_free(quote);
    g_free(file);
}

static void append_uri_to_cmd(GFile* gf, GString* cmd)
{
    char* uri = g_file_get_uri(gf);
    char* quote = g_shell_quote(uri);
    g_string_append(cmd, quote);
    g_string_append_c(cmd, ' ');
    g_free(quote);
    g_free(uri);
}

static char* expand_exec_macros(FmApp* app, GList* gfiles)
{
    char* ret;
    GString* cmd;
    const char* p;
    gboolean files_added = FALSE;

    cmd = g_string_sized_new(1024);
    for(p = app->exec; *p; ++p)
    {
        if(*p == '%')
        {
            ++p;
            if(!*p)
                break;
            switch(*p)
            {
            case 'f':
                if(gfiles)
                    append_file_to_cmd(G_FILE(gfiles->data), cmd);
                files_added = TRUE;
                break;
            case 'F':
                g_list_foreach(gfiles, (GFunc)append_file_to_cmd, cmd);
                files_added = TRUE;
                break;
            case 'u':
                if(gfiles)
                    append_uri_to_cmd(G_FILE(gfiles->data), cmd);
                files_added = TRUE;
                break;
            case 'U':
                g_list_foreach(gfiles, (GFunc)append_uri_to_cmd, cmd);
                files_added = TRUE;
                break;
            case '%':
                g_string_append_c(cmd, '%');
                break;
            case 'i':
                if(app->icon_name)
                {
                    g_string_append(cmd, "--icon ");
                    g_string_append(cmd, app->icon_name);
                }
                break;
            case 'c':
                if(app->name)
                    g_string_append(cmd, app->name);
                break;
            case 'k':
                /* append the file path of the desktop file */
                break;
            }
        }
        else
            g_string_append_c(cmd, *p);
    }

    /* if files are provided but the Exec key doesn't contain %f, %F, %u, or %U */
    if(gfiles && !files_added)
    {
        /* treat as %f */
        append_file_to_cmd(G_FILE(gfiles->data), cmd);
    }

    /* add terminal emulator command */
    if(app->use_terminal)
    {
        /* FIXME: this is unsafe */
        if(strstr(fm_config->terminal, "%s"))
            ret = g_strdup_printf(fm_config->terminal, cmd->str);
        else /* if %s is not found, fallback to -e */
            ret = g_strdup_printf("%s -e %s", fm_config->terminal, cmd->str);
        g_string_free(cmd, TRUE);
    }
    else
        ret = g_string_free(cmd, FALSE);
    return ret;
}

struct ChildSetup
{
    char* display;
    char* sn_id;
};

static void child_setup(gpointer user_data)
{
    struct ChildSetup* data = (struct ChildSetup*)user_data;
    if(data->display)
        g_setenv ("DISPLAY", data->display, TRUE);
    if(data->sn_id)
        g_setenv ("DESKTOP_STARTUP_ID", data->sn_id, TRUE);
}

gboolean fm_app_launch(GAppInfo *appinfo, GList* gfiles, GAppLaunchContext* ctx, GError** err)
{
    gboolean ret = FALSE;
    FmApp* app = FM_APP(appinfo);
    if(app->type == FM_APP_APPLICATION)
    {
        char* cmd;
        char** argv;
        int argc;
        if(!app->exec || !*app->exec)
            return FALSE;
        cmd = expand_exec_macros(app, gfiles);
        g_debug("FmApp: launch command: `%s'", cmd);
        if(g_shell_parse_argv(cmd, &argv, &argc, err))
        {
            struct ChildSetup data;
            if(ctx)
            {
                data.display = g_app_launch_context_get_display(ctx, appinfo, gfiles);
                data.sn_id = g_app_launch_context_get_startup_notify_id(ctx, appinfo, gfiles);
            }
            else
            {
                data.display = NULL;
                data.sn_id = NULL;
            }

            ret = g_spawn_async(app->path, argv, NULL,
                                G_SPAWN_SEARCH_PATH,
                                child_setup, &data, NULL, err);
            g_free(data.display);
            g_free(data.sn_id);

            g_strfreev(argv);
        }
        g_free(cmd);
    }
    else if(app->type == FM_APP_LINK)
    {
        g_assert(files == NULL);
    }
    return ret;
}

static gboolean fm_app_supports_uris(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    return app->type == FM_APP_APPLICATION
        && app->exec
        && (strstr(app->exec, "%u") || strstr(app->exec, "%U"));
}

static gboolean fm_app_supports_files(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    return app->type == FM_APP_APPLICATION
        && app->exec
        && (strstr(app->exec, "%f") || strstr(app->exec, "%F"));
}

static gboolean fm_app_launch_uris(GAppInfo *appinfo, GList *uris,
                                   GAppLaunchContext  *ctx, GError **err)
{
    FmApp* app = FM_APP(appinfo);
    GList* gfiles = NULL;
    gboolean ret;

    for(;uris; uris = uris->next)
    {
        GFile* gf = g_file_new_for_uri((char*)uris->data);
        if(gf)
            gfiles = g_list_prepend(gfiles, gf);
    }

    gfiles = g_list_reverse(gfiles);
    ret = fm_app_launch(appinfo, gfiles, ctx, err);

    g_list_foreach(gfiles, (GFunc)g_object_unref, NULL);
    g_list_free(gfiles);
    return ret;
}

static gboolean fm_app_should_show(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    /* FIXME: implement this */
    return TRUE;
}


static const char* fm_app_get_commandline(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    return app->exec;
}

static const char * fm_app_get_display_name(GAppInfo *appinfo)
{
    FmApp* app = FM_APP(appinfo);
    /* FIXME: what's this? */
    return app->name;
}

static void fm_app_info_iface_init(GAppInfoIface *iface)
{
    iface->dup = fm_app_dup;
    iface->equal = fm_app_equal;
    iface->get_id = fm_app_get_id;
    iface->get_name = fm_app_get_name;
    iface->get_description = fm_app_get_description;
    iface->get_executable = fm_app_get_executable;
    iface->get_icon = fm_app_get_icon;
    iface->launch = fm_app_launch;
    iface->supports_uris = fm_app_supports_uris;
    iface->supports_files = fm_app_supports_files;
    iface->launch_uris = fm_app_launch_uris;
    iface->should_show = fm_app_should_show;
/*
    iface->set_as_default_for_type = fm_app_set_as_default_for_type;
    iface->set_as_default_for_extension = fm_app_set_as_default_for_extension;
    iface->add_supports_type = fm_app_add_supports_type;
    iface->can_remove_supports_type = fm_app_can_remove_supports_type;
    iface->remove_supports_type = fm_app_remove_supports_type;
    iface->can_delete = fm_app_can_delete;
    iface->do_delete = fm_app_delete;
*/
    iface->get_commandline = fm_app_get_commandline;
    iface->get_display_name = fm_app_get_display_name;
}

static void fm_app_class_init(FmAppClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_app_finalize;
}

void _fm_app_init()
{

}

void _fm_app_finalize()
{

}

/* set name of current desktop environment.
 * If not set, the value will be obtained from $XDG_CURRENT_DESKTOP. */
void fm_app_set_desktop_env(const char* de_name)
{

}
