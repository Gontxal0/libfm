//      fm-app.h
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


#ifndef __FM_APP_H__
#define __FM_APP_H__

#include <gio/gio.h>
#include "fm-file-info.h"
#include "fm-path.h"

G_BEGIN_DECLS


#define FM_TYPE_APP                (fm_app_get_type())
#define FM_APP(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_TYPE_APP, FmApp))
#define FM_APP_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_TYPE_APP, FmAppClass))
#define FM_IS_APP(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_TYPE_APP))
#define FM_IS_APP_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_TYPE_APP))
#define FM_APP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),\
            FM_TYPE_APP, FmAppClass))

enum _FmAppType
{
    FM_APP_TYPE_NONE,
    FM_APP_APPLICATION,
    FM_APP_LINK
};
typedef enum _FmAppType FmAppType;

typedef struct _FmApp            FmApp;
typedef struct _FmAppClass        FmAppClass;

struct _FmApp
{
    GObject parent;
    char* id;
    char* name;
    char* comment;
    char* icon_name;
    GIcon* icon;
    char* path;
    union
    {
        char* exec; /* if type = FM_APP_APPLICATION */
        char* url; /* if type = FM_APP_LINK */
    };
    FmAppType type : 3; /* Application or Link */
    gboolean use_startup_notification:1;
    gboolean use_terminal:1;
    gboolean is_hidden:1;
    guint32 show_in;
};

struct _FmAppClass
{
    GObjectClass parent_class;
};

void _fm_app_init();
void _fm_app_finalize();

GType        fm_app_get_type        (void);
GAppInfo* fm_app_new(const char* desktop_id);
GAppInfo* fm_app_new_for_file(const char* filename);
GAppInfo* fm_app_new_for_keyfile(GKeyFile* kf);

const char* fm_app_get_icon_name(FmApp* app);
gboolean fm_app_get_use_startup_notification(FmApp* app);
gboolean fm_app_get_use_terminal(FmApp* app);
gboolean fm_app_is_deleted(FmApp* app);

/* launch a list of FmFileInfo or FmPath objects */
/*
gboolean fm_app_launch_files(FmApp* app, FmFileInfoList* fis, GAppLaunchContext* ctx, GError** err);
gboolean fm_app_launch_paths(FmApp* app, FmPathList* paths, GAppLaunchContext* ctx, GError** err);
*/

void fm_app_set_desktop_env(const char* de_name);

G_END_DECLS

#endif /* __FM_APP_H__ */
