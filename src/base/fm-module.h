/*
 *      fm-module.h
 *
 *      This file is a part of the Libfm project.
 *
 *      Copyright 2013 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

#ifndef __FM_MODULE_H__
#define __FM_MODULE_H__ 1

#include <gio/gio.h>

#define __FM_MODULE_VERSION__(xx) FM_MODULE_##xx##_VERSION
#define __FM_DEFINE_VERSION__(xx) __FM_MODULE_VERSION__(xx)

/**
 * FM_DEFINE_MODULE
 * @_type_: type of module (e.g. `vfs')
 * @_name_: module type specific key (e.g. `menu')
 *
 * Macro used in module definition. Module should have module specific
 * structure: if @type is vfs then it should be fm_module_init_vfs. See
 * specific header file for some `extern' definition.
 */
#define FM_DEFINE_MODULE(_type_, _name_) \
int module_##_type_##_version = __FM_DEFINE_VERSION__(_type_); \
char module_name[] = #_name_;

typedef gboolean (*FmModuleInitCallback)(const char *, gpointer, int);

/**
 * FM_MODULE_DEFINE_TYPE
 * @_type_: type of module (e.g. `vfs')
 * @_struct_: type of struct with module callbacks
 * @_minver_: minimum version supported
 *
 * Macro used in module caller. Callback is ran when matched module is
 * found, it should return %TRUE on success.
 */
#define FM_MODULE_DEFINE_TYPE(_type_, _struct_, _minver_) \
static gboolean fm_module_callback_##_type_(const char *, gpointer, int ver); \
\
static inline void FM_MODULE_REGISTER_##_type_ (void) { \
    fm_module_register_type(#_type_, \
                            _minver_, __FM_DEFINE_VERSION__(_type_), \
                            fm_module_callback_##_type_); \
}

/* use this whenever extension is about to be used */
#define CHECK_MODULES(...) if(G_UNLIKELY(!fm_modules_loaded)) fm_modules_load()

G_BEGIN_DECLS

/* adds schedule */
void fm_module_register_type(const char *type, int minver, int maxver, FmModuleInitCallback);
/* removes schedule */
void fm_module_unregister_type(const char *type);
/* forces schedules */
void fm_modules_load(void);
/* the flag */
volatile gint fm_modules_loaded;

G_END_DECLS

#endif /* __FM_MODULE_H__ */
