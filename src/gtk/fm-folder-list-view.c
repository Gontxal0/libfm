/*
 *      fm-folder-list-view.c
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

#include "exo/exo-tree-view.h"

#include "fm-dnd-src.h"
#include "fm-dnd-dest.h"
#include "fm-dnd-auto-scroll.h"

#define FM_FOLDER_LIST_VIEW_TYPE             (fm_folder_list_view_get_type())
#define FM_FOLDER_LIST_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_FOLDER_LIST_VIEW_TYPE, FmFolderListView))
#define FM_FOLDER_LIST_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_FOLDER_LIST_VIEW_TYPE, FmFolderListViewClass))
#define FM_IS_FOLDER_LIST_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_FOLDER_LIST_VIEW_TYPE))
#define FM_IS_FOLDER_LIST_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_FOLDER_LIST_VIEW_TYPE))

typedef struct _FmFolderListView             FmFolderListView;
typedef struct _FmFolderListViewClass        FmFolderListViewClass;

GType       fm_folder_list_view_get_type(void);


struct _FmFolderListView
{
    GtkScrolledWindow parent;

    GtkSelectionMode sel_mode;

    gboolean show_hidden;

    GtkTreeView* view;
    FmFolderModel* model; /* FmFolderListView doesn't use abstract GtkTreeModel! */
    FmCellRendererPixbuf* renderer_pixbuf;
    FmCellRendererText* renderer_text;
    guint icon_size_changed_handler;

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

    /* for columns width handling */
    gint updated_col;
    gboolean name_updated;
};

struct _FmFolderListViewClass
{
    GtkScrolledWindowClass parent_class;

    /* signal handlers */
    /* void (*column_widths_changed)(); */
};

static void fm_folder_list_view_dispose(GObject *object);

static void fm_folder_list_view_view_init(FmFolderViewInterface* iface);

G_DEFINE_TYPE_WITH_CODE(FmFolderListView, fm_folder_list_view, GTK_TYPE_SCROLLED_WINDOW,
                        G_IMPLEMENT_INTERFACE(FM_TYPE_FOLDER_VIEW, fm_folder_list_view_view_init))

static GList* fm_folder_list_view_get_selected_tree_paths(FmFolderListView* fv);

static gboolean on_standard_view_focus_in(GtkWidget* widget, GdkEventFocus* evt);

static gboolean on_btn_pressed(GtkTreeView* view, GdkEventButton* evt, FmFolderListView* fv);
static void on_sel_changed(GObject* obj, FmFolderListView* fv);

static void on_dnd_src_data_get(FmDndSrc* ds, FmFolderListView* fv);
static gboolean on_drag_motion(GtkWidget *dest_widget, GdkDragContext *drag_context,
                               gint x, gint y, guint time, FmFolderListView* fv);

static void on_single_click_changed(FmConfig* cfg, FmFolderListView* fv);
static void on_small_icon_size_changed(FmConfig* cfg, FmFolderListView* fv);

static void fm_folder_list_view_set_model(FmFolderView* ffv, FmFolderModel* model);
static inline void create_list_view(FmFolderListView* fv, GList* sels);

static FmFolderViewColumnInfo* _sv_column_info_new(FmFolderModelCol col_id)
{
    FmFolderViewColumnInfo* info = g_slice_new0(FmFolderViewColumnInfo);
    info->col_id = col_id;
    return info;
}

static void _sv_column_info_free(gpointer info)
{
    g_slice_free(FmFolderViewColumnInfo, info);
}

static void fm_folder_list_view_class_init(FmFolderListViewClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass *widget_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_folder_list_view_dispose;
    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->focus_in_event = on_standard_view_focus_in;

    fm_folder_list_view_parent_class = (GtkScrolledWindowClass*)g_type_class_peek(GTK_TYPE_SCROLLED_WINDOW);
}

static gboolean on_standard_view_focus_in(GtkWidget* widget, GdkEventFocus* evt)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(widget);
    if( fv->view )
    {
        gtk_widget_grab_focus(GTK_WIDGET(fv->view));
        return TRUE;
    }
    return FALSE;
}

static void on_single_click_changed(FmConfig* cfg, FmFolderListView* fv)
{
    exo_tree_view_set_single_click(EXO_TREE_VIEW(fv->view), fm_config->single_click);
}

static void on_tree_view_row_activated(GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, FmFolderListView* fv)
{
    GtkTreeIter it;
    /* #3578780: activating item is ambiguos when there are more than one item
       selected. Let do it the same way as other file managers do it, i.e.
       unselect all other items first and then activate only current one */
    fm_folder_view_unselect_all(FM_FOLDER_VIEW(fv));
    gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, path);
    GtkTreeSelection* sel = gtk_tree_view_get_selection(fv->view);
    gtk_tree_selection_select_iter(sel, &it);
    fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), path, FM_FV_ACTIVATED);
}

static void fm_folder_list_view_init(FmFolderListView *self)
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

    self->updated_col = -1;
}

static FmFolderListView* _fm_folder_list_view_new(FmStandardViewMode mode,
                                              FmFolderViewUpdatePopup update_popup,
                                              FmLaunchFolderFunc open_folders)
{
    FmFolderListView* fv = (FmFolderListView*)g_object_new(FM_FOLDER_LIST_VIEW_TYPE, NULL);
    AtkObject *obj = gtk_widget_get_accessible(GTK_WIDGET(fv));

    fv->update_popup = update_popup;
    fv->open_folders = open_folders;
    atk_object_set_description(obj, _("View of folder contents"));
    return fv;
}

FmFolderView *_fm_folder_list_view_new_for_id(FmFolderView *old_fv, gint id,
                                              FmFolderViewUpdatePopup update_popup,
                                              FmLaunchFolderFunc open_folders)
{
    FmFolderListView *fv;
    FmPathList *sels;
    GtkWidget *view;
    //gboolean has_focus;

    if (old_fv == NULL)
        return (FmFolderView*)_fm_folder_list_view_new(id, update_popup, open_folders);
    if (FM_IS_FOLDER_LIST_VIEW(old_fv))
        return g_object_ref(old_fv);
    /* ouch, it was another widget... set data from it then */
    fv = _fm_folder_list_view_new(id, update_popup, open_folders);
    fm_folder_list_view_set_model((FmFolderView*)fv, fm_folder_view_get_model(old_fv));
    //has_focus = gtk_widget_has_focus(old_fv->view);
    sels = fm_folder_view_dup_selected_file_paths(old_fv);

    create_list_view(fv, sels ? fm_path_list_peek_head_link(sels) : NULL);
    if (sels)
        fm_path_list_unref(sels);

    /* FIXME: maybe calling set_icon_size here is a good idea */

    view = GTK_WIDGET(fv->view);
    fm_dnd_src_set_widget(fv->dnd_src, view);
    fm_dnd_dest_set_widget(fv->dnd_dest, view);
    g_signal_connect_after(view, "drag-motion", G_CALLBACK(on_drag_motion), fv);
    /* connecting it after sometimes conflicts with system configuration
       (bug #3559831) so we just hope here it will be handled in order
       of connecting, i.e. after ExoIconView or ExoTreeView handler */
    g_signal_connect(view, "button-press-event", G_CALLBACK(on_btn_pressed), fv);

    fm_dnd_set_dest_auto_scroll(view,
                                gtk_scrolled_window_get_hadjustment((GtkScrolledWindow*)fv),
                                gtk_scrolled_window_get_vadjustment((GtkScrolledWindow*)fv));

    gtk_widget_show(view);
    gtk_container_add(GTK_CONTAINER(fv), view);

//    if(has_focus) /* restore the focus if needed. */
//        gtk_widget_grab_focus(view);
    return (FmFolderView*)fv;
}

static void _reset_columns_widths(GtkTreeView* view)
{
    GList* cols = gtk_tree_view_get_columns(view);
    GList* l;

    for(l = cols; l; l = l->next)
    {
        FmFolderViewColumnInfo* info = g_object_get_qdata(l->data, fm_qdata_id);
        if(info)
            info->reserved1 = 0;
    }
    g_list_free(cols);
}

static void on_row_changed(GtkTreeModel *tree_model, GtkTreePath *path,
                           GtkTreeIter *iter, FmFolderListView* fv)
{
    _reset_columns_widths(fv->view);
}

static void on_row_deleted(GtkTreeModel *tree_model, GtkTreePath  *path,
                           FmFolderListView* fv)
{
    _reset_columns_widths(fv->view);
}

static void on_row_inserted(GtkTreeModel *tree_model, GtkTreePath *path,
                            GtkTreeIter *iter, FmFolderListView* fv)
{
    _reset_columns_widths(fv->view);
}

static void unset_model(FmFolderListView* fv)
{
    if(fv->model)
    {
        FmFolderModel* model = fv->model;
        /* g_debug("unset_model: %p, n_ref = %d", model, G_OBJECT(model)->ref_count); */
        g_object_unref(model);
        g_signal_handlers_disconnect_by_func(model, on_row_inserted, fv);
        g_signal_handlers_disconnect_by_func(model, on_row_deleted, fv);
        g_signal_handlers_disconnect_by_func(model, on_row_changed, fv);
        fv->model = NULL;
    }
}

static void unset_view(FmFolderListView* fv);
static void fm_folder_list_view_dispose(GObject *object)
{
    FmFolderListView *self;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FOLDER_LIST_VIEW(object));
    self = (FmFolderListView*)object;
    /* g_debug("fm_folder_list_view_dispose: %p", self); */

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
    (* G_OBJECT_CLASS(fm_folder_list_view_parent_class)->dispose)(object);
}

static void set_icon_size(FmFolderListView* fv, guint icon_size)
{
    FmCellRendererPixbuf* render = fv->renderer_pixbuf;

    fm_cell_renderer_pixbuf_set_fixed_size(render, icon_size, icon_size);

    if(!fv->model)
        return;

    fm_folder_model_set_icon_size(fv->model, icon_size);
}

static void on_small_icon_size_changed(FmConfig* cfg, FmFolderListView* fv)
{
    set_icon_size(fv, cfg->small_icon_size);
}

static GtkTreePath* get_drop_path_list_view(FmFolderListView* fv, gint x, gint y)
{
    GtkTreePath* tp = NULL;
    GtkTreeViewColumn* col;

    gtk_tree_view_convert_widget_to_bin_window_coords(fv->view, x, y, &x, &y);
    if(gtk_tree_view_get_path_at_pos(fv->view, x, y, &tp, &col, NULL, NULL))
    {
        if(gtk_tree_view_column_get_sort_column_id(col)!=FM_FOLDER_MODEL_COL_NAME)
        {
            gtk_tree_path_free(tp);
            tp = NULL;
        }
    }
    if(tp)
        gtk_tree_view_set_drag_dest_row(fv->view, tp,
                                        GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
    return tp;
}

static gboolean on_drag_motion(GtkWidget *dest_widget,
                                 GdkDragContext *drag_context,
                                 gint x,
                                 gint y,
                                 guint time,
                                 FmFolderListView* fv)
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
        GtkTreePath* tp = get_drop_path_list_view(fv, x, y);
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

static void _update_width_sizing(GtkTreeViewColumn* col, gint width)
{
    if(width > 0)
    {
        gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_fixed_width(col, width);
    }
    else
    {
        gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(col, TRUE);
    }
    gtk_tree_view_column_queue_resize(col);
}

/* Each change will generate notify for all columns from first to last.
 * 1) on window resizing only column Name may change - the size may grow to
 *    fill any additional space
 * 2) on manual column resize the resized column will change; last column
 *    will change size too if horizontal scroll bar isn't visible */
static void on_column_width_changed(GtkTreeViewColumn* col, GParamSpec *pspec,
                                    FmFolderListView* view)
{
    FmFolderViewColumnInfo *info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
    GList *cols = gtk_tree_view_get_columns(view->view);
    int width;
    guint pos;

    pos = g_list_index(cols, col);
    width = gtk_tree_view_column_get_width(col);
    /* g_debug("column width changed: [%u] id %u: %d", pos, info->col_id, width); */
    /* use info->reserved1 as 'last width' */
    if(width != info->reserved1)
    {
        if(info->col_id == FM_FOLDER_MODEL_COL_NAME)
            view->name_updated = TRUE;
        else if(info->reserved1 && view->updated_col < 0)
            view->updated_col = pos;
        info->reserved1 = width;
    }
    if(pos == g_list_length(cols) - 1) /* got all columns, decide what we got */
    {
        if(!view->name_updated && view->updated_col >= 0)
        {
            col = g_list_nth_data(cols, view->updated_col);
            info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
            if(info)
            {
                info->width = info->reserved1;
                /* g_debug("column %u changed width to %d", info->col_id, info->width); */
                fm_folder_view_columns_changed(FM_FOLDER_VIEW(view));
            }
        }
        /* FIXME: how to detect manual change of Name mix width reliably? */
        view->updated_col = -1;
        view->name_updated = FALSE;
    }
    g_list_free(cols);
}

static void on_column_hide(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    GtkWidget* view = gtk_tree_view_column_get_tree_view(col);
    gtk_tree_view_remove_column(GTK_TREE_VIEW(view), col);
    fm_folder_view_columns_changed(FM_FOLDER_VIEW(gtk_widget_get_parent(view)));
}

static void on_column_move_left(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    GtkTreeView* view = GTK_TREE_VIEW(gtk_tree_view_column_get_tree_view(col));
    GList* list, *l;

    list = gtk_tree_view_get_columns(view);
    l = g_list_find(list, col);
    if(l && l->prev)
    {
        gtk_tree_view_move_column_after(view, col,
                                        l->prev->prev ? l->prev->prev->data : NULL);
        fm_folder_view_columns_changed(FM_FOLDER_VIEW(gtk_widget_get_parent(GTK_WIDGET(view))));
    }
    g_list_free(list);
}

static void on_column_move_right(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    GtkTreeView* view = GTK_TREE_VIEW(gtk_tree_view_column_get_tree_view(col));
    GList* list, *l;

    list = gtk_tree_view_get_columns(view);
    l = g_list_find(list, col);
    if(l && l->next)
    {
        gtk_tree_view_move_column_after(view, col, l->next->data);
        fm_folder_view_columns_changed(FM_FOLDER_VIEW(gtk_widget_get_parent(GTK_WIDGET(view))));
    }
    g_list_free(list);
}

static GtkTreeViewColumn* create_list_view_column(FmFolderListView* fv, FmFolderViewColumnInfo *set);

static void on_column_add(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    GtkWidget *view = gtk_tree_view_column_get_tree_view(col);
    GtkWidget *fv = gtk_widget_get_parent(view);
    GtkTreeViewColumn *new_col;
    FmFolderViewColumnInfo info;
    g_return_if_fail(FM_IS_FOLDER_LIST_VIEW(fv));
    memset(&info, 0, sizeof(info));
    info.col_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item), "col_id"));
    new_col = create_list_view_column((FmFolderListView*)fv, &info);
    if(new_col) /* skip it if failed */
    {
        gtk_tree_view_move_column_after(GTK_TREE_VIEW(view), new_col, col);
        fm_folder_view_columns_changed(FM_FOLDER_VIEW(fv));
    }
}

static void on_column_auto_adjust(GtkMenuItem* menu_item, GtkTreeViewColumn* col)
{
    FmFolderViewColumnInfo* info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
    info->width = 0;
    info->reserved1 = 0;
    _update_width_sizing(col, 0);
    /* g_debug("auto sizing column %u", info->col_id); */
    fm_folder_view_columns_changed(FM_FOLDER_VIEW(gtk_widget_get_parent(gtk_tree_view_column_get_tree_view(col))));
}

static gboolean on_column_button_press_event(GtkWidget *button,
                                             GdkEventButton *event,
                                             GtkTreeViewColumn* col)
{
    if(event->button == 1)
    {
        GtkWidget* view = gtk_tree_view_column_get_tree_view(col);
        GtkWidget* fv = gtk_widget_get_parent(view);
        FmFolderViewColumnInfo* info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
        g_return_val_if_fail(FM_IS_FOLDER_LIST_VIEW(fv), FALSE);
        return !fm_folder_model_col_is_sortable(FM_FOLDER_LIST_VIEW(fv)->model, info->col_id);
    }
    return FALSE;
}

static gboolean on_column_button_released_event(GtkWidget *button, GdkEventButton *event,
                                        GtkTreeViewColumn* col)
{
    if(event->button == 3)
    {
        GtkWidget* view = gtk_tree_view_column_get_tree_view(col);
        GtkWidget* fv = gtk_widget_get_parent(view);
        GList *columns, *l;
        GSList *cols_list, *ld;
        GtkMenuShell* menu;
        GtkWidget* menu_item;
        const char* label;
        char* menu_item_label;
        FmFolderViewColumnInfo* info;
        guint i;

        g_return_val_if_fail(FM_IS_FOLDER_LIST_VIEW(fv), FALSE);

        columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(view));
        l = g_list_find(columns, col);
        if(l == NULL)
        {
            g_warning("column not found in GtkTreeView");
            g_list_free(columns);
            return FALSE;
        }

        menu = GTK_MENU_SHELL(gtk_menu_new());
        /* destroy the menu when selection is done. */
        g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);

        info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
        label = fm_folder_model_col_get_title(FM_FOLDER_LIST_VIEW(fv)->model, info->col_id);
        menu_item_label = g_strdup_printf(_("_Hide %s"), label);
        menu_item = gtk_menu_item_new_with_mnemonic(menu_item_label);
        g_free(menu_item_label);
        gtk_menu_shell_append(menu, menu_item);
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_hide), col);
        if(info->col_id == FM_FOLDER_MODEL_COL_NAME) /* Name is immutable */
            gtk_widget_set_sensitive(menu_item, FALSE);

        menu_item = gtk_menu_item_new_with_mnemonic(_("_Move left"));
        gtk_menu_shell_append(menu, menu_item);
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_move_left), col);
        if(NULL == l->prev) /* the left most column */
            gtk_widget_set_sensitive(menu_item, FALSE);

        menu_item = gtk_menu_item_new_with_mnemonic(_("Move _right"));
        gtk_menu_shell_append(menu, menu_item);
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_move_right), col);
        if(NULL == l->next) /* the right most column */
            gtk_widget_set_sensitive(menu_item, FALSE);
        g_list_free(columns);

        /* create list of missing columns for 'Add' submenu */
        cols_list = fm_folder_view_get_columns(FM_FOLDER_VIEW(fv));
        menu_item_label = NULL; /* mark for below */
        for(i = 0; fm_folder_model_col_is_valid(i); i++)
        {
            label = fm_folder_model_col_get_title(FM_FOLDER_LIST_VIEW(fv)->model, i);
            if(!label)
                continue;
            for(ld = cols_list; ld; ld = ld->next)
                if(((FmFolderViewColumnInfo*)ld->data)->col_id == i)
                    break;
            /* if the column is already in the folder view, don't add it to the menu */
            if(ld)
                continue;
            if(menu_item_label == NULL)
                gtk_menu_shell_append(menu, gtk_separator_menu_item_new());
            menu_item_label = g_strdup_printf(_("Show %s"), label);
            menu_item = gtk_menu_item_new_with_label(menu_item_label);
            g_object_set_data(G_OBJECT(menu_item), "col_id", GINT_TO_POINTER(i));
            g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_add), col);
            g_free(menu_item_label);
            gtk_menu_shell_append(menu, menu_item);
        }
        g_slist_free(cols_list);

        if(info->width > 0 && info->col_id != FM_FOLDER_MODEL_COL_NAME)
        {
            gtk_menu_shell_append(menu, gtk_separator_menu_item_new());
            menu_item = gtk_menu_item_new_with_mnemonic(_("_Forget width"));
            gtk_menu_shell_append(menu, menu_item);
            g_signal_connect(menu_item, "activate", G_CALLBACK(on_column_auto_adjust), col);
        }

        gtk_widget_show_all(GTK_WIDGET(menu));
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);
        return TRUE;
    }
    else if(event->button == 1)
    {
        GtkWidget* view = gtk_tree_view_column_get_tree_view(col);
        GtkWidget* fv = gtk_widget_get_parent(view);
        FmFolderViewColumnInfo* info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
        g_return_val_if_fail(FM_IS_FOLDER_LIST_VIEW(fv), FALSE);
        return !fm_folder_model_col_is_sortable(FM_FOLDER_LIST_VIEW(fv)->model, info->col_id);
    }
    return FALSE;
}

static GtkTreeViewColumn* create_list_view_column(FmFolderListView* fv,
                                                  FmFolderViewColumnInfo *set)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;
    const char* title;
    FmFolderViewColumnInfo* info;
    GtkWidget *label;
    FmFolderModelCol col_id;

    g_return_val_if_fail(set != NULL, NULL); /* invalid arg */
    col_id = set->col_id;
    title = fm_folder_model_col_get_title(fv->model, col_id);
    g_return_val_if_fail(title != NULL, NULL); /* invalid column */

    /* g_debug("adding column id=%u", col_id); */
    col = gtk_tree_view_column_new();
    render = gtk_cell_renderer_text_new();
    gtk_tree_view_column_set_title(col, title);
    info = _sv_column_info_new(col_id);
    /* TODO: update other data from set - width for example */
    info->width = set->width;
    g_object_set_qdata_full(G_OBJECT(col), fm_qdata_id, info, _sv_column_info_free);

    switch(col_id)
    {
    case FM_FOLDER_MODEL_COL_NAME:
        /* special handling for Name column */
        gtk_tree_view_column_pack_start(col, GTK_CELL_RENDERER(fv->renderer_pixbuf), FALSE);
        gtk_tree_view_column_set_attributes(col, GTK_CELL_RENDERER(fv->renderer_pixbuf),
                                            "pixbuf", FM_FOLDER_MODEL_COL_ICON,
                                            "info", FM_FOLDER_MODEL_COL_INFO, NULL);
        g_object_set(render, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_column_set_expand(col, TRUE);
        if(set->width <= 0)
            info->width = 200;
        break;
    case FM_FOLDER_MODEL_COL_SIZE:
        g_object_set(render, "xalign", 1.0, NULL);
    default:
        if(set->width < 0)
            info->width = fm_folder_model_col_get_default_width(fv->model, col_id);
    }
    _update_width_sizing(col, info->width);

    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_set_attributes(col, render, "text", col_id, NULL);
    gtk_tree_view_column_set_resizable(col, TRUE);
    /* Unfortunately if we don't set it sortable we cannot right-click it too
    if(fm_folder_model_col_is_sortable(fv->model, col_id)) */
        gtk_tree_view_column_set_sort_column_id(col, col_id);
    gtk_tree_view_append_column(fv->view, col);
    if(G_UNLIKELY(col_id == FM_FOLDER_MODEL_COL_NAME))
        /* only this column is activable */
        exo_tree_view_set_activable_column((ExoTreeView*)fv->view, col);

    g_signal_connect(col, "notify::width", G_CALLBACK(on_column_width_changed), fv);

#if GTK_CHECK_VERSION(3, 0, 0)
    label = gtk_tree_view_column_get_button(col);
#else
    /* a little trick to fetch header button, taken from KIWI python library */
    label = gtk_label_new(title);
    gtk_widget_show(label);
    gtk_tree_view_column_set_widget(col, label);
    label = gtk_tree_view_column_get_widget(col);
    while(label && !GTK_IS_BUTTON(label))
        label = gtk_widget_get_parent(label);
#endif
    if(label)
    {
        /* disable left-click handling for non-sortable columns */
        g_signal_connect(label, "button-press-event",
                         G_CALLBACK(on_column_button_press_event), col);
        /* handle right-click on column header */
        g_signal_connect(label, "button-release-event",
                         G_CALLBACK(on_column_button_released_event), col);
        /* FIXME: how to disconnect it later? */
    }

    return col;
}

static void _check_tree_columns_defaults(FmFolderListView* fv)
{
    const FmFolderViewColumnInfo cols[] = {
        {FM_FOLDER_MODEL_COL_NAME},
        {FM_FOLDER_MODEL_COL_DESC},
        {FM_FOLDER_MODEL_COL_SIZE},
        {FM_FOLDER_MODEL_COL_MTIME} };
    GSList* cols_list = NULL;
    GList* tree_columns;
    guint i;

    tree_columns = gtk_tree_view_get_columns(fv->view);
    if(tree_columns != NULL) /* already set */
    {
        g_list_free(tree_columns);
        return;
    }
    /* Set default columns to show in detailed list mode.
     * FIXME: cols should be passed to fm_folder_list_view_new() as a parameter instead.
     * This breaks API/ABI though. Let's do it later. */
    for(i = 0; i < G_N_ELEMENTS(cols); i++)
        cols_list = g_slist_append(cols_list, (gpointer)&cols[i]);
    fm_folder_view_set_columns(FM_FOLDER_VIEW(fv), cols_list);
    g_slist_free(cols_list);
}

static inline void create_list_view(FmFolderListView* fv, GList* sels)
{
    GtkTreeSelection* ts;
    GList *l;
    FmFolderModel* model = fv->model;
    int icon_size = 0;

    fv->view = GTK_TREE_VIEW(exo_tree_view_new());

    if(fv->renderer_pixbuf)
        g_object_unref(fv->renderer_pixbuf);
    fv->renderer_pixbuf = g_object_ref_sink(fm_cell_renderer_pixbuf_new());
    fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_small_icon_size_changed), fv);
    icon_size = fm_config->small_icon_size;
    fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
    if(model)
    {
        fm_folder_model_set_icon_size(model, icon_size);
        _check_tree_columns_defaults(fv);
        gtk_tree_view_set_search_column(fv->view, FM_FOLDER_MODEL_COL_NAME);
    }

    gtk_tree_view_set_rules_hint(fv->view, TRUE);
    gtk_tree_view_set_rubber_banding(fv->view, TRUE);
    exo_tree_view_set_single_click((ExoTreeView*)fv->view, fm_config->single_click);
    exo_tree_view_set_single_click_timeout((ExoTreeView*)fv->view,
                                           fm_config->auto_selection_delay);

    ts = gtk_tree_view_get_selection(fv->view);
    g_signal_connect(fv->view, "row-activated", G_CALLBACK(on_tree_view_row_activated), fv);
    g_signal_connect(ts, "changed", G_CALLBACK(on_sel_changed), fv);
    gtk_tree_view_set_model(fv->view, GTK_TREE_MODEL(model));
    gtk_tree_selection_set_mode(ts, fv->sel_mode);
    for(l = sels;l;l=l->next)
        gtk_tree_selection_select_path(ts, (GtkTreePath*)l->data);
}

static void unset_view(FmFolderListView* fv)
{
    /* these signals connected by view creators */
    GtkTreeSelection* ts = gtk_tree_view_get_selection(fv->view);
    g_signal_handlers_disconnect_by_func(ts, on_sel_changed, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_tree_view_row_activated, fv);
    /* these signals connected by fm_folder_list_view_set_mode() */
    g_signal_handlers_disconnect_by_func(fv->view, on_drag_motion, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_btn_pressed, fv);

    fm_dnd_unset_dest_auto_scroll(GTK_WIDGET(fv->view));
    gtk_widget_destroy(GTK_WIDGET(fv->view));
    fv->view = NULL;
}

static void fm_folder_list_view_set_selection_mode(FmFolderView* ffv, GtkSelectionMode mode)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    GtkTreeSelection* sel;
    if(fv->sel_mode != mode)
    {
        fv->sel_mode = mode;
        sel = gtk_tree_view_get_selection(fv->view);
        gtk_tree_selection_set_mode(sel, mode);
    }
}

static GtkSelectionMode fm_folder_list_view_get_selection_mode(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    return fv->sel_mode;
}

static void fm_folder_list_view_set_show_hidden(FmFolderView* ffv, gboolean show)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    fv->show_hidden = show;
}

static gboolean fm_folder_list_view_get_show_hidden(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    return fv->show_hidden;
}

/* returned list should be freed with g_list_free_full(list, gtk_tree_path_free) */
static GList* fm_folder_list_view_get_selected_tree_paths(FmFolderListView* fv)
{
    GtkTreeSelection* sel;
    sel = gtk_tree_view_get_selection(fv->view);
    return gtk_tree_selection_get_selected_rows(sel, NULL);
}

static inline FmFileInfoList* fm_folder_list_view_get_selected_files(FmFolderListView* fv)
{
    /* don't generate the data again if we have it cached. */
    if(!fv->cached_selected_files)
    {
        GList* sels = fm_folder_list_view_get_selected_tree_paths(fv);
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

static FmFileInfoList* fm_folder_list_view_dup_selected_files(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    return fm_file_info_list_ref(fm_folder_list_view_get_selected_files(fv));
}

static FmPathList* fm_folder_list_view_dup_selected_file_paths(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    if(!fv->cached_selected_file_paths)
    {
        FmFileInfoList* files = fm_folder_list_view_get_selected_files(fv);
        if(files)
            fv->cached_selected_file_paths = fm_path_list_new_from_file_info_list(files);
        else
            fv->cached_selected_file_paths = NULL;
    }
    return fm_path_list_ref(fv->cached_selected_file_paths);
}

static gint fm_folder_list_view_count_selected_files(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    GtkTreeSelection* sel = gtk_tree_view_get_selection(fv->view);
    return gtk_tree_selection_count_selected_rows(sel);
}

static gboolean on_btn_pressed(GtkTreeView* view, GdkEventButton* evt, FmFolderListView* fv)
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
            if(evt->window == gtk_tree_view_get_bin_window(view))
            {
                /* special handling for ExoTreeView */
                /* Fix #2986834: MAJOR PROBLEM: Deletes Wrong File Frequently. */
                GtkTreeViewColumn* col;
                if(gtk_tree_view_get_path_at_pos(view, evt->x, evt->y, &tp, &col, NULL, NULL))
                {
                    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(view);
                    if(!gtk_tree_selection_path_is_selected(tree_sel, tp))
                    {
                        gtk_tree_selection_unselect_all(tree_sel);
                        if(col == exo_tree_view_get_activable_column(EXO_TREE_VIEW(view)))
                        {
                            gtk_tree_selection_select_path(tree_sel, tp);
                            gtk_tree_view_set_cursor(view, tp, NULL, FALSE);
                        }
                    }
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
        sels = fm_folder_list_view_get_selected_tree_paths(fv);
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

static void fm_folder_list_view_select_all(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    GtkTreeSelection * tree_sel;
    tree_sel = gtk_tree_view_get_selection(fv->view);
    gtk_tree_selection_select_all(tree_sel);
}

static void fm_folder_list_view_unselect_all(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    GtkTreeSelection * tree_sel;
    tree_sel = gtk_tree_view_get_selection(fv->view);
    gtk_tree_selection_unselect_all(tree_sel);
}

static void on_dnd_src_data_get(FmDndSrc* ds, FmFolderListView* fv)
{
    FmFileInfoList* files = fm_folder_list_view_dup_selected_files(FM_FOLDER_VIEW(fv));
    fm_dnd_src_set_files(ds, files);
    if(files)
        fm_file_info_list_unref(files);
}

static gboolean on_sel_changed_real(FmFolderListView* fv)
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
    FmFolderListView* fv = (FmFolderListView*)user_data;
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

static void on_sel_changed(GObject* obj, FmFolderListView* fv)
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

static void fm_folder_list_view_select_invert(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    GtkTreeSelection *tree_sel;
    GtkTreeIter it;
    if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(fv->model), &it))
        return;
    tree_sel = gtk_tree_view_get_selection(fv->view);
    do
    {
        if(gtk_tree_selection_iter_is_selected(tree_sel, &it))
            gtk_tree_selection_unselect_iter(tree_sel, &it);
        else
            gtk_tree_selection_select_iter(tree_sel, &it);
    }while( gtk_tree_model_iter_next(GTK_TREE_MODEL(fv->model), &it ));
}

static FmFolder* fm_folder_list_view_get_folder(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    return fv->model ? fm_folder_model_get_folder(fv->model) : NULL;
}

static void fm_folder_list_view_select_file_path(FmFolderView* ffv, FmPath* path)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    FmFolder* folder = fm_folder_list_view_get_folder(ffv);
    FmPath* cwd = folder ? fm_folder_get_path(folder) : NULL;
    if(cwd && fm_path_equal(fm_path_get_parent(path), cwd))
    {
        FmFolderModel* model = fv->model;
        GtkTreeIter it;
        if(fm_folder_model_find_iter_by_filename(model, &it, fm_path_get_basename(path)))
        {
            GtkTreeSelection* sel = gtk_tree_view_get_selection(fv->view);
            gtk_tree_selection_select_iter(sel, &it);
        }
    }
}

static void fm_folder_list_view_get_custom_menu_callbacks(FmFolderView* ffv,
        FmFolderViewUpdatePopup *update_popup, FmLaunchFolderFunc *open_folders)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    *update_popup = fv->update_popup;
    *open_folders = fv->open_folders;
}

#if 0
static gboolean fm_folder_list_view_is_loaded(FmFolderListView* fv)
{
    return fv->folder && fm_folder_is_loaded(fv->folder);
}
#endif

static FmFolderModel* fm_folder_list_view_get_model(FmFolderView* ffv)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    return fv->model;
}

static void fm_folder_list_view_set_model(FmFolderView* ffv, FmFolderModel* model)
{
    FmFolderListView* fv = FM_FOLDER_LIST_VIEW(ffv);
    int icon_size;
    unset_model(fv);
    _check_tree_columns_defaults(fv);
    if(model)
    {
        icon_size = fm_config->small_icon_size;
        fm_folder_model_set_icon_size(model, icon_size);
    }
    gtk_tree_view_set_model(fv->view, GTK_TREE_MODEL(model));
    _reset_columns_widths(fv->view);

    if(model)
    {
        fv->model = (FmFolderModel*)g_object_ref(model);
        g_signal_connect(model, "row-inserted", G_CALLBACK(on_row_inserted), fv);
        g_signal_connect(model, "row-deleted", G_CALLBACK(on_row_deleted), fv);
        g_signal_connect(model, "row-changed", G_CALLBACK(on_row_changed), fv);
    }
}

typedef struct
{
    GtkTreeViewColumn* col;
    FmFolderViewColumnInfo* info;
} _ColumnsCache;

static gboolean _fm_folder_list_view_set_columns(FmFolderView* fv, const GSList* cols)
{
    FmFolderListView* view;
    GtkTreeViewColumn *col, *last;
    FmFolderViewColumnInfo* info;
    _ColumnsCache* old_cols;
    const GSList* l;
    GList *cols_list, *ld;
    guint i, n_cols;

    if(!FM_IS_FOLDER_LIST_VIEW(fv))
        return FALSE;
    view = (FmFolderListView*)fv;

    cols_list = gtk_tree_view_get_columns(view->view);
    n_cols = g_list_length(cols_list);
    if(n_cols > 0)
    {
        /* create more convenient for us list of columns */
        old_cols = g_new(_ColumnsCache, n_cols);
        for(ld = cols_list, i = 0; ld; ld = ld->next, i++)
        {
            col = ld->data; /* column */
            info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id); /* info */
            old_cols[i].col = col;
            old_cols[i].info = info;
        }
        g_list_free(cols_list);
    }
    last = NULL;
    for(l = cols; l; l = l->next)
    {
        info = l->data;
        /* find old one and move here */
        for(i = 0; i < n_cols; i++)
            if(old_cols[i].info && old_cols[i].info->col_id == info->col_id)
                break;
        if(i < n_cols)
        {
            /* we found it so just move it here */
            col = old_cols[i].col;
            /* update all other data - width for example */
            if(info->col_id != FM_FOLDER_MODEL_COL_NAME)
            {
                old_cols[i].info->width = info->width;
                if(info->width < 0)
                    old_cols[i].info->width = fm_folder_model_col_get_default_width(view->model, info->col_id);
                old_cols[i].info->reserved1 = 0;
                _update_width_sizing(col, info->width);
            }
            old_cols[i].col = NULL; /* we removed it from its place */
            old_cols[i].info = NULL; /* don't try to use it again */
        }
        else if(!fm_folder_model_col_is_valid(0))
            /* workaround for case when there is no model init yet, the most
               probably bug #3596550 is about this (it creates column with
               empty title), can g_return_val_if_fail() not fail somehow? */
            continue;
        else
        {
            /* if not found then append new one */
            col = create_list_view_column(view, info);
            if(col == NULL) /* failed! skipping it */
                continue;
        }
        gtk_tree_view_move_column_after(view->view, col, last);
        last = col;
    }

    /* remove abandoned columns from view */
    for(i = 0; i < n_cols; i++)
        if(old_cols[i].col != NULL)
            gtk_tree_view_remove_column(view->view, old_cols[i].col);
    if(n_cols > 0)
        g_free(old_cols);
    return TRUE;
}

static GSList* _fm_folder_list_view_get_columns(FmFolderView* fv)
{
    FmFolderListView* view;
    GSList* list;
    GList *cols_list, *ld;

    if(!FM_IS_FOLDER_LIST_VIEW(fv))
        return NULL;
    view = (FmFolderListView*)fv;

    cols_list = gtk_tree_view_get_columns(view->view);
    if(cols_list == NULL)
        return NULL;
    list = NULL;
    for(ld = cols_list; ld; ld = ld->next)
    {
        GtkTreeViewColumn *col = ld->data;
        FmFolderViewColumnInfo* info = g_object_get_qdata(G_OBJECT(col), fm_qdata_id);
        list = g_slist_append(list, info); /* info */
    }
    g_list_free(cols_list);
    return list;
}

static gint _fm_folder_list_view_get_view_id(FmFolderView* fv)
{
    return FM_FV_LIST_VIEW;
}

static void fm_folder_list_view_view_init(FmFolderViewInterface* iface)
{
    iface->set_sel_mode = fm_folder_list_view_set_selection_mode;
    iface->get_sel_mode = fm_folder_list_view_get_selection_mode;
    iface->set_show_hidden = fm_folder_list_view_set_show_hidden;
    iface->get_show_hidden = fm_folder_list_view_get_show_hidden;
    iface->get_folder = fm_folder_list_view_get_folder;
    iface->set_model = fm_folder_list_view_set_model;
    iface->get_model = fm_folder_list_view_get_model;
    iface->count_selected_files = fm_folder_list_view_count_selected_files;
    iface->dup_selected_files = fm_folder_list_view_dup_selected_files;
    iface->dup_selected_file_paths = fm_folder_list_view_dup_selected_file_paths;
    iface->select_all = fm_folder_list_view_select_all;
    iface->unselect_all = fm_folder_list_view_unselect_all;
    iface->select_invert = fm_folder_list_view_select_invert;
    iface->select_file_path = fm_folder_list_view_select_file_path;
    iface->get_custom_menu_callbacks = fm_folder_list_view_get_custom_menu_callbacks;
    iface->set_columns = _fm_folder_list_view_set_columns;
    iface->get_columns = _fm_folder_list_view_get_columns;
    iface->get_view_id = _fm_folder_list_view_get_view_id;
}
