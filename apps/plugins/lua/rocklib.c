/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2008 Dan Everton (safetydan)
 * Copyright (C) 2009 Maurus Cuelenaere
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#define lrocklib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "rocklib_img.h"
#include "rocklib.h"
#include "lib/helper.h"
#include "lib/pluginlib_actions.h"

/*
 * http://www.lua.org/manual/5.1/manual.html#lua_CFunction
 *
 * In order to communicate properly with Lua, a C function must use the following protocol,
 * which defines the way parameters and results are passed: a C function receives its arguments
 * from Lua in its stack in direct order (the first argument is pushed first). To return values to Lua,
 * a C function just pushes them onto the stack, in direct order (the first result is pushed first),
 * and returns the number of results. Any other value in the stack below the results will be properly
 * discarded by Lua. Like a Lua function, a C function called by Lua can also return many results. 
 *
 * When porting new functions, don't forget to check rocklib_aux.pl whether it automatically creates
 * wrappers for the function and if so, add the function names to @forbidden_functions. This is to
 * prevent namespace collisions and adding duplicate wrappers.
 */

/*
 * -----------------------------
 *
 *  Rockbox wrappers start here!
 *
 * -----------------------------
 */

#define RB_WRAP(func) static int rock_##func(lua_State UNUSED_ATTR *L)
#define SIMPLE_VOID_WRAPPER(func) RB_WRAP(func) { (void)L; func(); return 0; }

/* Helper function for opt_viewport */
static void check_tablevalue(lua_State *L,
                             const char* key,
                             int tablepos,
                             void* res,
                             bool is_unsigned)
{
    lua_getfield(L, tablepos, key); /* Find table[key] */

    int val = lua_tointeger(L, -1);

    if(is_unsigned)
        *(unsigned*)res = (unsigned) val;
    else
        *(int*)res = val;

    lua_pop(L, 1); /* Pop the value off the stack */
}

static inline struct viewport* opt_viewport(lua_State *L,
                                     int narg,
                                     struct viewport* vp,
                                     struct viewport* alt)
{
    if(lua_isnoneornil(L, narg))
        return alt;

    luaL_checktype(L, narg, LUA_TTABLE);

    check_tablevalue(L, "x", narg, &vp->x, false);
    check_tablevalue(L, "y", narg, &vp->y, false);
    check_tablevalue(L, "width", narg, &vp->width, false);
    check_tablevalue(L, "height", narg, &vp->height, false);
#ifdef HAVE_LCD_BITMAP
    check_tablevalue(L, "font", narg, &vp->font, false);
    check_tablevalue(L, "drawmode", narg, &vp->drawmode, false);
#endif
#if LCD_DEPTH > 1
    check_tablevalue(L, "fg_pattern", narg, &vp->fg_pattern, true);
    check_tablevalue(L, "bg_pattern", narg, &vp->bg_pattern, true);
#endif

    return vp;
}

RB_WRAP(set_viewport)
{
    static struct viewport vp;
    int screen = luaL_optint(L, 2, SCREEN_MAIN);
    rb->screens[screen]->set_viewport(opt_viewport(L, 1, &vp, NULL));
    return 0;
}

RB_WRAP(clear_viewport)
{
    int screen = luaL_optint(L, 1, SCREEN_MAIN);
    rb->screens[screen]->clear_viewport();
    return 0;
}

RB_WRAP(current_tick)
{
    lua_pushinteger(L, *rb->current_tick);
    return 1;
}

RB_WRAP(kbd_input)
{
    luaL_Buffer b;
    luaL_buffinit(L, &b);

    const char *input = lua_tostring(L, 1);
    char *buffer = luaL_prepbuffer(&b);

    if(input != NULL)
        rb->strlcpy(buffer, input, LUAL_BUFFERSIZE);
    else
        buffer[0] = '\0';

    if(!rb->kbd_input(buffer, LUAL_BUFFERSIZE))
    {
        luaL_addsize(&b, strlen(buffer));
        luaL_pushresult(&b);
    }
    else
        lua_pushnil(L);

    return 1;
}

#ifdef HAVE_TOUCHSCREEN
RB_WRAP(action_get_touchscreen_press)
{
    short x, y;
    int result = rb->action_get_touchscreen_press(&x, &y);

    lua_pushinteger(L, result);
    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    return 3;
}

RB_WRAP(touchscreen_set_mode)
{
    enum touchscreen_mode mode = luaL_checkint(L, 1);
    rb->touchscreen_set_mode(mode);
    return 0;
}

RB_WRAP(touchscreen_get_mode)
{
    lua_pushinteger(L, rb->touchscreen_get_mode());
    return 1;
}
#endif

RB_WRAP(font_getstringsize)
{
    const unsigned char* str = luaL_checkstring(L, 1);
    int fontnumber = luaL_checkint(L, 2);
    int w, h;

    if (fontnumber == FONT_UI)
        fontnumber = rb->global_status->font_id[SCREEN_MAIN];
    else
        fontnumber = FONT_SYSFIXED;

    int result = rb->font_getstringsize(str, &w, &h, fontnumber);
    lua_pushinteger(L, result);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);

    return 3;
}

RB_WRAP(current_path)
{
    const char *current_path = get_current_path(L, 1);
    if(current_path != NULL)
        lua_pushstring(L, current_path);
    else
        lua_pushnil(L);

    return 1;
}

static void fill_text_message(lua_State *L, struct text_message * message,
                              int pos)
{
    int i;
    luaL_checktype(L, pos, LUA_TTABLE);
    int n = luaL_getn(L, pos);

    const char **lines = (const char**) lua_newuserdata(L, n * sizeof(const char*));

    for(i=1; i<=n; i++)
    {
        lua_rawgeti(L, pos, i);
        lines[i-1] = luaL_checkstring(L, -1);
        lua_pop(L, 1);
    }
    message->message_lines = lines;
    message->nb_lines = n;
}

RB_WRAP(gui_syncyesno_run)
{
    struct text_message main_message, yes_message, no_message;
    struct text_message *yes = NULL, *no = NULL;

    lua_settop(L, 3); /* newuserdata will be pushed onto stack after args*/
    fill_text_message(L, &main_message, 1);
    if(!lua_isnoneornil(L, 2))
        fill_text_message(L, (yes = &yes_message), 2);
    if(!lua_isnoneornil(L, 3))
        fill_text_message(L, (no = &no_message), 3);

    enum yesno_res result = rb->gui_syncyesno_run(&main_message, yes, no);

    lua_pushinteger(L, result);
    return 1;
}

RB_WRAP(do_menu)
{
    struct menu_callback_with_desc menu_desc = {NULL, NULL, Icon_NOICON};
    struct menu_item_ex menu = {MT_RETURN_ID | MENU_HAS_DESC, {.strings = NULL},
                                {.callback_and_desc = &menu_desc}};
    int i, n, start_selected;
    const char **items, *title;

    title = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    start_selected = lua_tointeger(L, 3);

    n = luaL_getn(L, 2);

    /* newuserdata will be pushed onto stack after args*/
    items = (const char**) lua_newuserdata(L, n * sizeof(const char*));

    for(i=1; i<=n; i++)
    {
        lua_rawgeti(L, 2, i); /* Push item on the stack */
        items[i-1] = luaL_checkstring(L, -1);
        lua_pop(L, 1); /* Pop it */
    }

    menu.strings = items;
    menu.flags |= MENU_ITEM_COUNT(n);
    menu_desc.desc = (unsigned char*) title;

    int result = rb->do_menu(&menu, &start_selected, NULL, false);

    lua_pushinteger(L, result);
    return 1;
}

RB_WRAP(playlist_sync)
{
    /* just pass NULL to work with the current playlist */
    rb->playlist_sync(NULL);
    return 1;
}

RB_WRAP(playlist_remove_all_tracks)
{
    /* just pass NULL to work with the current playlist */
    int result = rb->playlist_remove_all_tracks(NULL);
    lua_pushinteger(L, result);
    return 1;
}

RB_WRAP(playlist_insert_track)
{
    const char *filename;
    int position, queue, sync;

    /* for now don't take a playlist_info pointer, but just pass NULL to use
	   the current playlist. If this changes later, all the other
	   parameters can be shifted back. */
    filename = luaL_checkstring(L, 1); /* only required parameter */
    position = luaL_optint(L, 2, PLAYLIST_INSERT);
    queue = lua_toboolean(L, 3); /* default to false */
    sync = lua_toboolean(L, 4); /* default to false */

    int result = rb->playlist_insert_track(NULL, filename, position,
        queue, sync);

    lua_pushinteger(L, result);
    return 1;
}

RB_WRAP(playlist_insert_directory)
{
    const char* dirname;
    int position, queue, recurse;

    /* just like in playlist_insert_track, only works with current playlist. */
    dirname = luaL_checkstring(L, 1); /* only required parameter */
    position = luaL_optint(L, 2, PLAYLIST_INSERT);
    queue = lua_toboolean(L, 3); /* default to false */
    recurse = lua_toboolean(L, 4); /* default to false */

    int result = rb->playlist_insert_directory(NULL, dirname, position,
        queue, recurse);

    lua_pushinteger(L, result);
    return 1;
}

SIMPLE_VOID_WRAPPER(backlight_force_on);
SIMPLE_VOID_WRAPPER(backlight_use_settings);

#ifdef HAVE_REMOTE_LCD
SIMPLE_VOID_WRAPPER(remote_backlight_force_on);
SIMPLE_VOID_WRAPPER(remote_backlight_use_settings);
#endif

#ifdef HAVE_BUTTON_LIGHT
SIMPLE_VOID_WRAPPER(buttonlight_force_on);
SIMPLE_VOID_WRAPPER(buttonlight_use_settings);
#endif

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
RB_WRAP(backlight_brightness_set)
{
    int brightness = luaL_checkint(L, 1);
    backlight_brightness_set(brightness);

    return 0;
}
SIMPLE_VOID_WRAPPER(backlight_brightness_use_setting);
#endif

RB_WRAP(get_plugin_action)
{
    static const struct button_mapping *m1[] = { pla_main_ctx };
    int timeout = luaL_checkint(L, 1);
    int btn;

#ifdef HAVE_REMOTE_LCD
    static const struct button_mapping *m2[] = { pla_main_ctx, pla_remote_ctx };
    bool with_remote = luaL_optint(L, 2, 0);
    if (with_remote)
        btn = pluginlib_getaction(timeout, m2, 2);
    else
#endif
        btn = pluginlib_getaction(timeout, m1, 1);

    lua_pushinteger(L, btn);
    return 1;
}

#define RB_FUNC(func) {#func, rock_##func}
static const luaL_Reg rocklib[] =
{
    /* Kernel */
    RB_FUNC(current_tick),

    /* Buttons */
#ifdef HAVE_TOUCHSCREEN
    RB_FUNC(action_get_touchscreen_press),
    RB_FUNC(touchscreen_set_mode),
    RB_FUNC(touchscreen_get_mode),
#endif

    RB_FUNC(kbd_input),

    RB_FUNC(font_getstringsize),
    RB_FUNC(set_viewport),
    RB_FUNC(clear_viewport),
    RB_FUNC(current_path),
    RB_FUNC(gui_syncyesno_run),
    RB_FUNC(playlist_sync),
    RB_FUNC(playlist_remove_all_tracks),
    RB_FUNC(playlist_insert_track),
    RB_FUNC(playlist_insert_directory),
    RB_FUNC(do_menu),

    /* Backlight helper */
    RB_FUNC(backlight_force_on),
    RB_FUNC(backlight_use_settings),

#ifdef HAVE_REMOTE_LCD
    RB_FUNC(remote_backlight_force_on),
    RB_FUNC(remote_backlight_use_settings),
#endif

#ifdef HAVE_BUTTON_LIGHT
    RB_FUNC(buttonlight_force_on),
    RB_FUNC(buttonlight_use_settings),
#endif

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    RB_FUNC(backlight_brightness_set),
    RB_FUNC(backlight_brightness_use_setting),
#endif

    RB_FUNC(get_plugin_action),

    {NULL, NULL}
};
#undef RB_FUNC

extern const luaL_Reg rocklib_aux[];
extern const luaL_Reg rocklib_img[];

/*
 ** Open Rockbox library
 */
LUALIB_API int luaopen_rock(lua_State *L)
{
    luaL_register(L, LUA_ROCKLIBNAME, rocklib);
    luaL_register(L, LUA_ROCKLIBNAME, rocklib_aux);
    luaL_register(L, LUA_ROCKLIBNAME, rocklib_img);

    static const struct lua_int_reg rlib_const_int[] =
    {
        /* useful integer constants */
        RB_CONSTANT(HZ),

        RB_CONSTANT(LCD_DEPTH),
        RB_CONSTANT(LCD_HEIGHT),
        RB_CONSTANT(LCD_WIDTH),

        RB_CONSTANT(FONT_SYSFIXED),
        RB_CONSTANT(FONT_UI),

        RB_CONSTANT(PLAYLIST_INSERT),
        RB_CONSTANT(PLAYLIST_INSERT_LAST),
        RB_CONSTANT(PLAYLIST_INSERT_FIRST),
        RB_CONSTANT(PLAYLIST_INSERT_LAST_SHUFFLED),
        RB_CONSTANT(PLAYLIST_INSERT_SHUFFLED),
        RB_CONSTANT(PLAYLIST_PREPEND),
        RB_CONSTANT(PLAYLIST_REPLACE),


        RB_CONSTANT(SCREEN_MAIN),
#ifdef HAVE_REMOTE_LCD
        RB_CONSTANT(SCREEN_REMOTE),
#endif

#ifdef HAVE_TOUCHSCREEN
        RB_CONSTANT(TOUCHSCREEN_POINT),
        RB_CONSTANT(TOUCHSCREEN_BUTTON),
#endif

        {NULL, 0}
    };

    static const struct lua_int_reg* rlci = rlib_const_int;
    for (; rlci->name; rlci++) {
        lua_pushinteger(L, rlci->value);
        lua_setfield(L, -2, rlci->name);
    }

    static const struct lua_str_reg rlib_const_str[] =
    {
        /* some useful paths constants */
        RB_STRING_CONSTANT(HOME_DIR),
        RB_STRING_CONSTANT(PLUGIN_DIR),
        RB_STRING_CONSTANT(PLUGIN_APPS_DATA_DIR),
        RB_STRING_CONSTANT(PLUGIN_GAMES_DATA_DIR),
        RB_STRING_CONSTANT(PLUGIN_DATA_DIR),
        RB_STRING_CONSTANT(ROCKBOX_DIR),
        RB_STRING_CONSTANT(VIEWERS_DATA_DIR),
        {NULL,NULL}
    };

    static const struct lua_str_reg* rlcs = rlib_const_str;
    for (; rlcs->name; rlcs++) {
        lua_pushstring(L, rlcs->value);
        lua_setfield(L, -2, rlcs->name);
    }

    rli_init(L);

    return 1;
}

