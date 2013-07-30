/*
 *      fm-folder-icon-view.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *      Copyright 2012-2013 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "gtk-compat.h"

#include "fm.h"
#include "fm-folder-view.h"
#include "fm-gtk-marshal.h"
#include "fm-cell-renderer-text.h"
#include "fm-cell-renderer-pixbuf.h"
#include "fm-gtk-utils.h"

#include "exo/exo-icon-view.h"

#include "fm-dnd-src.h"
#include "fm-dnd-dest.h"
#include "fm-dnd-auto-scroll.h"

#define FM_FOLDER_ICON_VIEW_TYPE             (fm_folder_icon_view_get_type())
#define FM_FOLDER_ICON_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_FOLDER_ICON_VIEW_TYPE, FmFolderIconView))
#define FM_FOLDER_ICON_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_FOLDER_ICON_VIEW_TYPE, FmFolderIconViewClass))
#define FM_IS_FOLDER_ICON_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_FOLDER_ICON_VIEW_TYPE))
#define FM_IS_FOLDER_ICON_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_FOLDER_ICON_VIEW_TYPE))

typedef struct _FmFolderIconView             FmFolderIconView;
typedef struct _FmFolderIconViewClass        FmFolderIconViewClass;

GType       fm_folder_icon_view_get_type(void);


struct _FmFolderIconView
{
    GtkScrolledWindow parent;

    FmStandardViewMode mode;
    GtkSelectionMode sel_mode;

    gboolean show_hidden;

    ExoIconView* view;
    FmFolderModel* model; /* FmFolderIconView doesn't use abstract GtkTreeModel! */
    FmCellRendererPixbuf* renderer_pixbuf;
    FmCellRendererText* renderer_text;
    guint icon_size_changed_handler;
    guint show_full_names_handler;

    FmDndSrc* dnd_src; /* dnd source manager */
    FmDndDest* dnd_dest; /* dnd dest manager */

    /* for very large folder update */
    guint sel_changed_idle;
    gboolean sel_changed_pending;

    FmFileInfoList* cached_selected_files;
    FmPathList* cached_selected_file_paths;

    /* callbacks to creator */
    FmFolderViewUpdatePopup update_popup;
    FmLaunchFolderFunc open_folders;
};

struct _FmFolderIconViewClass
{
    GtkScrolledWindowClass parent_class;

    /* signal handlers */
    /* void (*column_widths_changed)(); */
};

static void fm_folder_icon_view_dispose(GObject *object);

static void fm_folder_icon_view_view_init(FmFolderViewInterface* iface);

G_DEFINE_TYPE_WITH_CODE(FmFolderIconView, fm_folder_icon_view, GTK_TYPE_SCROLLED_WINDOW,
                        G_IMPLEMENT_INTERFACE(FM_TYPE_FOLDER_VIEW, fm_folder_icon_view_view_init))

static gboolean on_standard_view_focus_in(GtkWidget* widget, GdkEventFocus* evt);

static gboolean on_btn_pressed(ExoIconView* view, GdkEventButton* evt, FmFolderIconView* fv);
static void on_sel_changed(GObject* obj, FmFolderIconView* fv);

static void on_dnd_src_data_get(FmDndSrc* ds, FmFolderIconView* fv);

static void on_single_click_changed(FmConfig* cfg, FmFolderIconView* fv);
static void on_big_icon_size_changed(FmConfig* cfg, FmFolderIconView* fv);
static void on_small_icon_size_changed(FmConfig* cfg, FmFolderIconView* fv);
static void on_thumbnail_size_changed(FmConfig* cfg, FmFolderIconView* fv);

static void fm_folder_icon_view_set_model(FmFolderView* ffv, FmFolderModel* model);
static void _fm_folder_icon_view_set_mode(FmFolderIconView* fv, FmStandardViewMode mode);

static void fm_folder_icon_view_class_init(FmFolderIconViewClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass *widget_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_folder_icon_view_dispose;
    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->focus_in_event = on_standard_view_focus_in;

    fm_folder_icon_view_parent_class = (GtkScrolledWindowClass*)g_type_class_peek(GTK_TYPE_SCROLLED_WINDOW);
}

static gboolean on_standard_view_focus_in(GtkWidget* widget, GdkEventFocus* evt)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(widget);
    if( fv->view )
    {
        gtk_widget_grab_focus(GTK_WIDGET(fv->view));
        return TRUE;
    }
    return FALSE;
}

static void on_single_click_changed(FmConfig* cfg, FmFolderIconView* fv)
{
    exo_icon_view_set_single_click(fv->view, cfg->single_click);
}

static void on_icon_view_item_activated(ExoIconView* iv, GtkTreePath* path, FmFolderIconView* fv)
{
    GtkTreeIter it;
    /* #3578780: activating item is ambiguos when there are more than one item
       selected. Let do it the same way as other file managers do it, i.e.
       unselect all other items first and then activate only current one */
    fm_folder_view_unselect_all(FM_FOLDER_VIEW(fv));
    gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, path);
    fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), path, FM_FV_ACTIVATED);
}

static void fm_folder_icon_view_init(FmFolderIconView *self)
{
    gtk_scrolled_window_set_hadjustment((GtkScrolledWindow*)self, NULL);
    gtk_scrolled_window_set_vadjustment((GtkScrolledWindow*)self, NULL);
    gtk_scrolled_window_set_policy((GtkScrolledWindow*)self, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* config change notifications */
    g_signal_connect(fm_config, "changed::single_click", G_CALLBACK(on_single_click_changed), self);

    /* dnd support */
    self->dnd_src = fm_dnd_src_new(NULL);
    g_signal_connect(self->dnd_src, "data-get", G_CALLBACK(on_dnd_src_data_get), self);

    self->dnd_dest = fm_dnd_dest_new_with_handlers(NULL);

    self->mode = -1;
}

static FmFolderIconView* _fm_folder_icon_view_new(FmStandardViewMode mode,
                                                  FmFolderViewUpdatePopup update_popup,
                                                  FmLaunchFolderFunc open_folders)
{
    FmFolderIconView* fv = (FmFolderIconView*)g_object_new(FM_FOLDER_ICON_VIEW_TYPE, NULL);
    AtkObject *obj = gtk_widget_get_accessible(GTK_WIDGET(fv));

    _fm_folder_icon_view_set_mode(fv, mode);
    fv->update_popup = update_popup;
    fv->open_folders = open_folders;
    atk_object_set_description(obj, _("View of folder contents"));
    return fv;
}

FmFolderView *_fm_folder_icon_view_new_for_id(FmFolderView *old_fv, gint id,
                                           FmFolderViewUpdatePopup update_popup,
                                           FmLaunchFolderFunc open_folders)
{
    FmFolderIconView *fv;
    FmPathList *sels;

    if (old_fv == NULL)
        return (FmFolderView*)_fm_folder_icon_view_new(id, update_popup, open_folders);
    if (FM_IS_FOLDER_ICON_VIEW(old_fv))
    {
        _fm_folder_icon_view_set_mode((FmFolderIconView*)old_fv, id);
        return g_object_ref(old_fv);
    }
    fv = _fm_folder_icon_view_new(id, update_popup, open_folders);
    fm_folder_icon_view_set_model((FmFolderView*)fv, fm_folder_view_get_model(old_fv));
    /* FIXME: do something with focus? */
    sels = fm_folder_view_dup_selected_file_paths(old_fv);
    if (sels)
    {
        fm_folder_view_select_file_paths((FmFolderView*)fv, sels);
        fm_path_list_unref(sels);
    }
    return (FmFolderView*)fv;
}

static void unset_model(FmFolderIconView* fv)
{
    if(fv->model)
    {
        FmFolderModel* model = fv->model;
        /* g_debug("unset_model: %p, n_ref = %d", model, G_OBJECT(model)->ref_count); */
        g_object_unref(model);
        fv->model = NULL;
    }
}

static void unset_view(FmFolderIconView* fv);
static void fm_folder_icon_view_dispose(GObject *object)
{
    FmFolderIconView *self;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FOLDER_ICON_VIEW(object));
    self = (FmFolderIconView*)object;
    /* g_debug("fm_folder_icon_view_dispose: %p", self); */

    unset_model(self);

    if(G_LIKELY(self->view))
        unset_view(self);

    if(self->renderer_pixbuf)
    {
        g_object_unref(self->renderer_pixbuf);
        self->renderer_pixbuf = NULL;
    }

    if(self->renderer_text)
    {
        g_object_unref(self->renderer_text);
        self->renderer_text = NULL;
    }

    if(self->cached_selected_files)
    {
        fm_file_info_list_unref(self->cached_selected_files);
        self->cached_selected_files = NULL;
    }

    if(self->cached_selected_file_paths)
    {
        fm_path_list_unref(self->cached_selected_file_paths);
        self->cached_selected_file_paths = NULL;
    }

    if(self->dnd_src)
    {
        g_signal_handlers_disconnect_by_func(self->dnd_src, on_dnd_src_data_get, self);
        g_object_unref(self->dnd_src);
        self->dnd_src = NULL;
    }
    if(self->dnd_dest)
    {
        g_object_unref(self->dnd_dest);
        self->dnd_dest = NULL;
    }

    g_signal_handlers_disconnect_by_func(fm_config, on_single_click_changed, object);

    if(self->sel_changed_idle)
    {
        g_source_remove(self->sel_changed_idle);
        self->sel_changed_idle = 0;
    }

    if(self->icon_size_changed_handler)
    {
        g_signal_handler_disconnect(fm_config, self->icon_size_changed_handler);
        self->icon_size_changed_handler = 0;
    }
    if(self->show_full_names_handler)
    {
        g_signal_handler_disconnect(fm_config, self->show_full_names_handler);
        self->show_full_names_handler = 0;
    }
    (* G_OBJECT_CLASS(fm_folder_icon_view_parent_class)->dispose)(object);
}

static void set_icon_size(FmFolderIconView* fv, guint icon_size)
{
    FmCellRendererPixbuf* render = fv->renderer_pixbuf;

    fm_cell_renderer_pixbuf_set_fixed_size(render, icon_size, icon_size);

    if(!fv->model)
        return;

    fm_folder_model_set_icon_size(fv->model, icon_size);

    if( fv->mode != FM_FV_LIST_VIEW ) /* this is an ExoIconView */
    {
        /* set row spacing in range 2...12 pixels */
        gint c_size = MIN(12, 2 + icon_size / 8);
        exo_icon_view_set_row_spacing(fv->view, c_size);
    }
}

static void on_big_icon_size_changed(FmConfig* cfg, FmFolderIconView* fv)
{
    guint item_width = cfg->big_icon_size + 40;
    /* reset ExoIconView item text sizes */
    g_object_set((GObject*)fv->renderer_text, "wrap-width", item_width, NULL);
    set_icon_size(fv, cfg->big_icon_size);
}

static void on_small_icon_size_changed(FmConfig* cfg, FmFolderIconView* fv)
{
    set_icon_size(fv, cfg->small_icon_size);
}

static void on_thumbnail_size_changed(FmConfig* cfg, FmFolderIconView* fv)
{
    guint item_width = MAX(cfg->thumbnail_size, 96);
    /* reset ExoIconView item text sizes */
    g_object_set((GObject*)fv->renderer_text, "wrap-width", item_width, NULL);
    /* FIXME: thumbnail and icons should have different sizes */
    /* maybe a separate API: fm_folder_model_set_thumbnail_size() */
    set_icon_size(fv, cfg->thumbnail_size);
}

static void on_show_full_names_changed(FmConfig* cfg, FmFolderIconView* fv)
{
    g_return_if_fail(fv->renderer_text);
    if(fv->mode == FM_FV_ICON_VIEW)
        g_object_set((GObject*)fv->renderer_text,
                     "max-height", cfg->show_full_names ? 0 : 70, NULL);
    else /* thumbnail view */
        g_object_set((GObject*)fv->renderer_text,
                     "max-height", cfg->show_full_names ? 0 : 90, NULL);
    /* FIXME: does it require redraw request? */
}

static gboolean on_drag_motion(GtkWidget *dest_widget,
                                 GdkDragContext *drag_context,
                                 gint x,
                                 gint y,
                                 guint time,
                                 FmFolderIconView* fv)
{
    gboolean ret;
    GdkDragAction action = 0;
    GdkAtom target = fm_dnd_dest_find_target(fv->dnd_dest, drag_context);

    if(target == GDK_NONE)
        return FALSE;

    ret = FALSE;
    /* files are being dragged */
    if(fm_dnd_dest_is_target_supported(fv->dnd_dest, target))
    {
        GtkTreePath* tp = exo_icon_view_get_path_at_pos(fv->view, x, y);
        exo_icon_view_set_drag_dest_item(fv->view, tp, EXO_ICON_VIEW_DROP_INTO);
        if(tp)
        {
            GtkTreeIter it;
            if(gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, tp))
            {
                FmFileInfo* fi;
                gtk_tree_model_get(GTK_TREE_MODEL(fv->model), &it, FM_FOLDER_MODEL_COL_INFO, &fi, -1);
                fm_dnd_dest_set_dest_file(fv->dnd_dest, fi);
            }
            gtk_tree_path_free(tp);
        }
        else
        {
            FmFolderModel* model = fv->model;
            if (model)
            {
                FmFolder* folder = fm_folder_model_get_folder(model);
                fm_dnd_dest_set_dest_file(fv->dnd_dest, fm_folder_get_info(folder));
            }
            else
                fm_dnd_dest_set_dest_file(fv->dnd_dest, NULL);
        }
        action = fm_dnd_dest_get_default_action(fv->dnd_dest, drag_context, target);
        ret = action != 0;
    }
    gdk_drag_status(drag_context, action, time);

    return ret;
}

static inline void create_icon_view(FmFolderIconView* fv, GList* sels)
{
    GList *l;
    GtkCellRenderer* render;
    FmFolderModel* model = fv->model;
    int icon_size = 0, item_width;

    fv->view = EXO_ICON_VIEW(exo_icon_view_new());

    if(fv->renderer_pixbuf)
        g_object_unref(fv->renderer_pixbuf);
    fv->renderer_pixbuf = g_object_ref_sink(fm_cell_renderer_pixbuf_new());
    render = (GtkCellRenderer*)fv->renderer_pixbuf;

    g_object_set((GObject*)render, "follow-state", TRUE, NULL );
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(fv->view), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render, "pixbuf", FM_FOLDER_MODEL_COL_ICON );
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render, "info", FM_FOLDER_MODEL_COL_INFO );

    if(fv->mode == FM_FV_COMPACT_VIEW) /* compact view */
    {
        fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_small_icon_size_changed), fv);
        icon_size = fm_config->small_icon_size;
        fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
        if(model)
            fm_folder_model_set_icon_size(model, icon_size);

        render = fm_cell_renderer_text_new();
        g_object_set((GObject*)render,
                     "xalign", 1.0, /* FIXME: why this needs to be 1.0? */
                     "yalign", 0.5,
                     NULL );
        exo_icon_view_set_layout_mode( (ExoIconView*)fv->view, EXO_ICON_VIEW_LAYOUT_COLS );
        exo_icon_view_set_orientation( (ExoIconView*)fv->view, GTK_ORIENTATION_HORIZONTAL );
    }
    else /* big icon view or thumbnail view */
    {
        if(fv->show_full_names_handler == 0)
            fv->show_full_names_handler = g_signal_connect(fm_config, "changed::show_full_names", G_CALLBACK(on_show_full_names_changed), fv);
        if(fv->mode == FM_FV_ICON_VIEW)
        {
            fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::big_icon_size", G_CALLBACK(on_big_icon_size_changed), fv);
            icon_size = fm_config->big_icon_size;
            fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
            if(model)
                fm_folder_model_set_icon_size(model, icon_size);

            render = fm_cell_renderer_text_new();
            item_width = icon_size + 40;
            g_object_set((GObject*)render,
                         "wrap-mode", PANGO_WRAP_WORD_CHAR,
                         "wrap-width", item_width,
                         "max-height", fm_config->show_full_names ? 0 : 70,
                         "alignment", PANGO_ALIGN_CENTER,
                         "xalign", 0.5,
                         "yalign", 0.0,
                         NULL );
            exo_icon_view_set_column_spacing( (ExoIconView*)fv->view, 4 );
        }
        else
        {
            fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::thumbnail_size", G_CALLBACK(on_thumbnail_size_changed), fv);
            icon_size = fm_config->thumbnail_size;
            fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
            if(model)
                fm_folder_model_set_icon_size(model, icon_size);

            render = fm_cell_renderer_text_new();
            item_width = MAX(icon_size, 96);
            g_object_set((GObject*)render,
                         "wrap-mode", PANGO_WRAP_WORD_CHAR,
                         "wrap-width", item_width,
                         "max-height", fm_config->show_full_names ? 0 : 90,
                         "alignment", PANGO_ALIGN_CENTER,
                         "xalign", 0.5,
                         "yalign", 0.0,
                         NULL );
            exo_icon_view_set_column_spacing( (ExoIconView*)fv->view, 8 );
        }
    }
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(fv->view), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render,
                                "text", FM_FOLDER_MODEL_COL_NAME );
    if(fv->renderer_text)
        g_object_unref(fv->renderer_text);
    fv->renderer_text = g_object_ref_sink(render);
    exo_icon_view_set_search_column((ExoIconView*)fv->view, FM_FOLDER_MODEL_COL_NAME);
    g_signal_connect(fv->view, "item-activated", G_CALLBACK(on_icon_view_item_activated), fv);
    g_signal_connect(fv->view, "selection-changed", G_CALLBACK(on_sel_changed), fv);
    exo_icon_view_set_model(fv->view, (GtkTreeModel*)fv->model);
    exo_icon_view_set_selection_mode(fv->view, fv->sel_mode);
    exo_icon_view_set_single_click(fv->view, fm_config->single_click);
    exo_icon_view_set_single_click_timeout((ExoIconView*)fv->view,
                                           fm_config->auto_selection_delay);

    for(l = sels;l;l=l->next)
        exo_icon_view_select_path(fv->view, l->data);
}

static void unset_view(FmFolderIconView* fv)
{
    /* these signals connected by view creators */
    g_signal_handlers_disconnect_by_func(fv->view, on_sel_changed, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_icon_view_item_activated, fv);
    /* these signals connected by fm_folder_icon_view_set_mode() */
    g_signal_handlers_disconnect_by_func(fv->view, on_drag_motion, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_btn_pressed, fv);

    fm_dnd_unset_dest_auto_scroll(GTK_WIDGET(fv->view));
    gtk_widget_destroy(GTK_WIDGET(fv->view));
    fv->view = NULL;
}

static void select_invert_icon_view(FmFolderModel* model, ExoIconView* view)
{
    GtkTreePath* path;
    int i, n;
    n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model), NULL);
    if(n == 0)
        return;
    path = gtk_tree_path_new_first();
    for( i=0; i<n; ++i, gtk_tree_path_next(path) )
    {
        if ( exo_icon_view_path_is_selected(view, path))
            exo_icon_view_unselect_path(view, path);
        else
            exo_icon_view_select_path(view, path);
    }
    gtk_tree_path_free(path);
}

static void _fm_folder_icon_view_set_mode(FmFolderIconView* fv, FmStandardViewMode mode)
{
    if( mode != fv->mode )
    {
        GList *sels;
        GtkWidget *view;
        gboolean has_focus;

        if( G_LIKELY(fv->view) )
        {
            has_focus = gtk_widget_has_focus(GTK_WIDGET(fv->view));
            /* preserve old selections */
            sels = exo_icon_view_get_selected_items(fv->view);

            unset_view(fv); /* it will destroy the fv->view widget */

            /* FIXME: compact view and icon view actually use the same
             * type of widget, ExoIconView. So it may be better to
             * reuse the widget when available. */
        }
        else
        {
            sels = NULL;
            has_focus = FALSE;
        }

        if(fv->icon_size_changed_handler)
        {
            g_signal_handler_disconnect(fm_config, fv->icon_size_changed_handler);
            fv->icon_size_changed_handler = 0;
        }
        if(fv->show_full_names_handler)
        {
            g_signal_handler_disconnect(fm_config, fv->show_full_names_handler);
            fv->show_full_names_handler = 0;
        }

        fv->mode = mode;
        create_icon_view(fv, sels);
        g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
        g_list_free(sels);
        view = GTK_WIDGET(fv->view);

        /* FIXME: maybe calling set_icon_size here is a good idea */

        fm_dnd_src_set_widget(fv->dnd_src, view);
        fm_dnd_dest_set_widget(fv->dnd_dest, view);
        g_signal_connect_after(view, "drag-motion", G_CALLBACK(on_drag_motion), fv);
        /* connecting it after sometimes conflicts with system configuration
           (bug #3559831) so we just hope here it will be handled in order
           of connecting, i.e. after ExoIconView or ExoTreeView handler */
        g_signal_connect(view, "button-press-event", G_CALLBACK(on_btn_pressed), fv);

        fm_dnd_set_dest_auto_scroll(view, gtk_scrolled_window_get_hadjustment((GtkScrolledWindow*)fv), gtk_scrolled_window_get_vadjustment((GtkScrolledWindow*)fv));

        gtk_widget_show(view);
        gtk_container_add(GTK_CONTAINER(fv), view);

        if(has_focus) /* restore the focus if needed. */
            gtk_widget_grab_focus(view);
    }
    else
    {
        /* g_debug("same mode"); */
    }
}

static void fm_folder_icon_view_set_selection_mode(FmFolderView* ffv, GtkSelectionMode mode)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    if(fv->sel_mode != mode)
    {
        fv->sel_mode = mode;
        exo_icon_view_set_selection_mode(fv->view, mode);
    }
}

static GtkSelectionMode fm_folder_icon_view_get_selection_mode(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    return fv->sel_mode;
}

static void fm_folder_icon_view_set_show_hidden(FmFolderView* ffv, gboolean show)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    fv->show_hidden = show;
}

static gboolean fm_folder_icon_view_get_show_hidden(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    return fv->show_hidden;
}

static inline FmFileInfoList* fm_folder_icon_view_get_selected_files(FmFolderIconView* fv)
{
    /* don't generate the data again if we have it cached. */
    if(!fv->cached_selected_files)
    {
        GList* sels = exo_icon_view_get_selected_items(fv->view);
        GList *l, *next;
        if(sels)
        {
            fv->cached_selected_files = fm_file_info_list_new();
            for(l = sels;l;l=next)
            {
                FmFileInfo* fi;
                GtkTreeIter it;
                GtkTreePath* tp = (GtkTreePath*)l->data;
                gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, l->data);
                gtk_tree_model_get(GTK_TREE_MODEL(fv->model), &it, FM_FOLDER_MODEL_COL_INFO, &fi, -1);
                gtk_tree_path_free(tp);
                next = l->next;
                l->data = fm_file_info_ref( fi );
                l->prev = l->next = NULL;
                fm_file_info_list_push_tail_link(fv->cached_selected_files, l);
            }
        }
    }
    return fv->cached_selected_files;
}

static FmFileInfoList* fm_folder_icon_view_dup_selected_files(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    return fm_file_info_list_ref(fm_folder_icon_view_get_selected_files(fv));
}

static FmPathList* fm_folder_icon_view_dup_selected_file_paths(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    if(!fv->cached_selected_file_paths)
    {
        FmFileInfoList* files = fm_folder_icon_view_get_selected_files(fv);
        if(files)
            fv->cached_selected_file_paths = fm_path_list_new_from_file_info_list(files);
        else
            fv->cached_selected_file_paths = NULL;
    }
    return fm_path_list_ref(fv->cached_selected_file_paths);
}

static gint fm_folder_icon_view_count_selected_files(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    return exo_icon_view_count_selected_items(fv->view);
}

static gboolean on_btn_pressed(ExoIconView* view, GdkEventButton* evt, FmFolderIconView* fv)
{
    GList* sels = NULL;
    FmFolderViewClickType type = 0;
    GtkTreePath* tp = NULL;

    if(!fv->model)
        return FALSE;

    /* FIXME: handle single click activation */
    if( evt->type == GDK_BUTTON_PRESS )
    {
        /* special handling for ExoIconView */
        if(evt->button != 1)
        {
            /* select the item on right click for ExoIconView */
            if(exo_icon_view_get_item_at_pos(view, evt->x, evt->y, &tp, NULL))
            {
                /* if the hit item is not currently selected */
                if(!exo_icon_view_path_is_selected(view, tp))
                {
                    sels = exo_icon_view_get_selected_items(view);
                    if( sels ) /* if there are selected items */
                    {
                        exo_icon_view_unselect_all(view); /* unselect all items */
                        g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
                        g_list_free(sels);
                    }
                    exo_icon_view_select_path(view, tp);
                    exo_icon_view_set_cursor(view, tp, NULL, FALSE);
                }
            }
        }

        if(evt->button == 2) /* middle click */
            type = FM_FV_MIDDLE_CLICK;
        else if(evt->button == 3) /* right click */
            type = FM_FV_CONTEXT_MENU;
    }

    if( type != FM_FV_CLICK_NONE )
    {
        sels = exo_icon_view_get_selected_items(fv->view);
        if( sels || type == FM_FV_CONTEXT_MENU )
        {
            fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), tp, type);
            if(sels)
            {
                g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
                g_list_free(sels);
            }
        }
    }
    if(tp)
        gtk_tree_path_free(tp);
    return FALSE;
}

static void fm_folder_icon_view_select_all(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    exo_icon_view_select_all(fv->view);
}

static void fm_folder_icon_view_unselect_all(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    exo_icon_view_unselect_all(fv->view);
}

static void on_dnd_src_data_get(FmDndSrc* ds, FmFolderIconView* fv)
{
    FmFileInfoList* files = fm_folder_icon_view_dup_selected_files(FM_FOLDER_VIEW(fv));
    fm_dnd_src_set_files(ds, files);
    if(files)
        fm_file_info_list_unref(files);
}

static gboolean on_sel_changed_real(FmFolderIconView* fv)
{
    /* clear cached selected files */
    if(fv->cached_selected_files)
    {
        fm_file_info_list_unref(fv->cached_selected_files);
        fv->cached_selected_files = NULL;
    }
    if(fv->cached_selected_file_paths)
    {
        fm_path_list_unref(fv->cached_selected_file_paths);
        fv->cached_selected_file_paths = NULL;
    }
    fm_folder_view_sel_changed(NULL, FM_FOLDER_VIEW(fv));
    fv->sel_changed_pending = FALSE;
    return TRUE;
}

/*
 * We limit "sel-changed" emitting here:
 * - if no signal was in last 200ms then signal is emitted immidiately
 * - if there was < 200ms since last signal then it's marked as pending
 *   and signal will be emitted when that 200ms timeout ends
 */
static gboolean on_sel_changed_idle(gpointer user_data)
{
    FmFolderIconView* fv = (FmFolderIconView*)user_data;
    gboolean ret = FALSE;

    GDK_THREADS_ENTER();
    /* check if fv is destroyed already */
    if(g_source_is_destroyed(g_main_current_source()))
        goto _end;
    if(fv->sel_changed_pending) /* fast changing detected! continue... */
        ret = on_sel_changed_real(fv);
    fv->sel_changed_idle = 0;
_end:
    GDK_THREADS_LEAVE();
    return ret;
}

static void on_sel_changed(GObject* obj, FmFolderIconView* fv)
{
    if(!fv->sel_changed_idle)
    {
        fv->sel_changed_idle = g_timeout_add_full(G_PRIORITY_HIGH_IDLE, 200,
                                                  on_sel_changed_idle, fv, NULL);
        on_sel_changed_real(fv);
    }
    else
        fv->sel_changed_pending = TRUE;
}

static void fm_folder_icon_view_select_invert(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    select_invert_icon_view(fv->model, fv->view);
}

static FmFolder* fm_folder_icon_view_get_folder(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    return fv->model ? fm_folder_model_get_folder(fv->model) : NULL;
}

static void fm_folder_icon_view_select_file_path(FmFolderView* ffv, FmPath* path)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    FmFolder* folder = fm_folder_icon_view_get_folder(ffv);
    FmPath* cwd = folder ? fm_folder_get_path(folder) : NULL;
    if(cwd && fm_path_equal(fm_path_get_parent(path), cwd))
    {
        FmFolderModel* model = fv->model;
        GtkTreeIter it;
        if(fm_folder_model_find_iter_by_filename(model, &it, fm_path_get_basename(path)))
        {
            GtkTreePath *tp = gtk_tree_model_get_path(GTK_TREE_MODEL(fv->model), &it);
            if(tp)
            {
                exo_icon_view_select_path(fv->view, tp);
                gtk_tree_path_free(tp);
            }
        }
    }
}

static void fm_folder_icon_view_get_custom_menu_callbacks(FmFolderView* ffv,
        FmFolderViewUpdatePopup *update_popup, FmLaunchFolderFunc *open_folders)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    *update_popup = fv->update_popup;
    *open_folders = fv->open_folders;
}

#if 0
static gboolean fm_folder_icon_view_is_loaded(FmFolderIconView* fv)
{
    return fv->folder && fm_folder_is_loaded(fv->folder);
}
#endif

static FmFolderModel* fm_folder_icon_view_get_model(FmFolderView* ffv)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    return fv->model;
}

static void fm_folder_icon_view_set_model(FmFolderView* ffv, FmFolderModel* model)
{
    FmFolderIconView* fv = FM_FOLDER_ICON_VIEW(ffv);
    int icon_size;
    unset_model(fv);
    if (model)
    {
        switch(fv->mode)
        {
        case FM_FV_ICON_VIEW:
            icon_size = fm_config->big_icon_size;
            break;
        case FM_FV_COMPACT_VIEW:
            icon_size = fm_config->small_icon_size;
            break;
        case FM_FV_THUMBNAIL_VIEW:
        default:
            icon_size = fm_config->thumbnail_size;
            break;
        }
        fm_folder_model_set_icon_size(model, icon_size);
        fv->model = (FmFolderModel*)g_object_ref(model);
        exo_icon_view_set_model(fv->view, GTK_TREE_MODEL(model));
    }
    else
        exo_icon_view_set_model(fv->view, NULL);
}

static gboolean _fm_folder_icon_view_set_columns(FmFolderView* fv, const GSList* cols)
{
    /* not supported */
    return FALSE;
}

static GSList* _fm_folder_icon_view_get_columns(FmFolderView* fv)
{
    /* not supported */
    return NULL;
}

static gint _fm_folder_icon_view_get_view_id(FmFolderView* fv)
{
    if(!FM_IS_FOLDER_ICON_VIEW(fv))
        return -1;

    return ((FmFolderIconView*)fv)->mode;
}

static void fm_folder_icon_view_view_init(FmFolderViewInterface* iface)
{
    iface->set_sel_mode = fm_folder_icon_view_set_selection_mode;
    iface->get_sel_mode = fm_folder_icon_view_get_selection_mode;
    iface->set_show_hidden = fm_folder_icon_view_set_show_hidden;
    iface->get_show_hidden = fm_folder_icon_view_get_show_hidden;
    iface->get_folder = fm_folder_icon_view_get_folder;
    iface->set_model = fm_folder_icon_view_set_model;
    iface->get_model = fm_folder_icon_view_get_model;
    iface->count_selected_files = fm_folder_icon_view_count_selected_files;
    iface->dup_selected_files = fm_folder_icon_view_dup_selected_files;
    iface->dup_selected_file_paths = fm_folder_icon_view_dup_selected_file_paths;
    iface->select_all = fm_folder_icon_view_select_all;
    iface->unselect_all = fm_folder_icon_view_unselect_all;
    iface->select_invert = fm_folder_icon_view_select_invert;
    iface->select_file_path = fm_folder_icon_view_select_file_path;
    iface->get_custom_menu_callbacks = fm_folder_icon_view_get_custom_menu_callbacks;
    iface->set_columns = _fm_folder_icon_view_set_columns;
    iface->get_columns = _fm_folder_icon_view_get_columns;
    iface->get_view_id = _fm_folder_icon_view_get_view_id;
}
