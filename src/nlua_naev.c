/*
 * See Licensing and Copyright notice in naev.h
 */
/**
 * @file nlua_naev.c
 *
 * @brief Contains Naev generic Lua bindings.
 */
/** @cond */
#include <lauxlib.h>

#include "naev.h"
/** @endcond */

#include "nlua_naev.h"

#include "array.h"
#include "console.h"
#include "hook.h"
#include "input.h"
#include "land.h"
#include "log.h"
#include "nlua_evt.h"
#include "nlua_misn.h"
#include "nlua_system.h"
#include "nluadef.h"
#include "nstring.h"
#include "player.h"
#include "plugin.h"
#include "semver.h"

static int cache_table = LUA_NOREF; /* No reference. */

/* Naev methods. */
static int naevL_version( lua_State *L );
static int naevL_versionTest( lua_State *L );
static int naevL_language( lua_State *L );
static int naevL_lastplayed( lua_State *L );
static int naevL_ticks( lua_State *L );
static int naevL_ticksGame( lua_State *L );
static int naevL_clock( lua_State *L );
static int naevL_keyGet( lua_State *L );
static int naevL_keyEnable( lua_State *L );
static int naevL_keyEnableAll( lua_State *L );
static int naevL_keyDisableAll( lua_State *L );
static int naevL_eventStart( lua_State *L );
static int naevL_eventReload( lua_State *L );
static int naevL_missionStart( lua_State *L );
static int naevL_missionReload( lua_State *L );
static int naevL_shadersReload( lua_State *L );
static int naevL_isSimulation( lua_State *L );
static int naevL_conf( lua_State *L );
static int naevL_confSet( lua_State *L );
static int naevL_cache( lua_State *L );
static int naevL_trigger( lua_State *L );
static int naevL_claimTest( lua_State *L );
static int naevL_plugins( lua_State *L );
static const luaL_Reg naev_methods[] = {
   { "version", naevL_version },
   { "versionTest", naevL_versionTest },
   { "language", naevL_language },
   { "lastplayed", naevL_lastplayed },
   { "ticks", naevL_ticks },
   { "ticksGame", naevL_ticksGame },
   { "clock", naevL_clock },
   { "keyGet", naevL_keyGet },
   { "keyEnable", naevL_keyEnable },
   { "keyEnableAll", naevL_keyEnableAll },
   { "keyDisableAll", naevL_keyDisableAll },
   { "eventStart", naevL_eventStart },
   { "eventReload", naevL_eventReload },
   { "missionStart", naevL_missionStart },
   { "missionReload", naevL_missionReload },
   { "shadersReload", naevL_shadersReload },
   { "isSimulation", naevL_isSimulation },
   { "conf", naevL_conf },
   { "confSet", naevL_confSet },
   { "cache", naevL_cache },
   { "trigger", naevL_trigger },
   { "claimTest", naevL_claimTest },
   { "plugins", naevL_plugins },
   {0,0}
}; /**< Naev Lua methods. */

/**
 * @brief Loads the Naev Lua library.
 *
 *    @param env Lua environment.
 *    @return 0 on success.
 */
int nlua_loadNaev( nlua_env env )
{
   nlua_register(env, "naev", naev_methods, 0);

   /* Create cache. */
   if (cache_table == LUA_NOREF) {
      lua_newtable( naevL );
      cache_table = luaL_ref(naevL, LUA_REGISTRYINDEX);
   }

   return 0;
}

/**
 * @brief Naev generic Lua bindings.
 *
 * @luamod naev
 */

/**
 * @brief Gets the version of Naev and the save game.
 *
 * @usage game_version, save_version = naev.version()
 *
 *    @luatreturn string The version of the game.
 *    @luatreturn string Version of current loaded save or nil if not loaded.
 * @luafunc version
 */
static int naevL_version( lua_State *L )
{
   lua_pushstring( L, naev_version(0) );
   if (player.loaded_version==NULL)
      lua_pushnil( L );
   else
      lua_pushstring( L, player.loaded_version );
   return 2;
}

/**
 * @brief Tests two semver version strings.
 *
 *    @luatparam string v1 Version 1 to test.
 *    @luatparam string v2 Version 2 to test.
 *    @luatreturn number Positive if v1 is newer or negative if v2 is newer.
 * @luafunc versionTest
 */
static int naevL_versionTest( lua_State *L )
{
   const char *s1, *s2;
   semver_t sv1, sv2;
   int res;
   /* Parse inputs. */
   s1 = luaL_checkstring(L,1);
   s2 = luaL_checkstring(L,2);
   if (semver_parse( s1, &sv1 ))
      WARN( _("Failed to parse version string '%s'!"), s1 );
   if (semver_parse( s2, &sv2 ))
      WARN( _("Failed to parse version string '%s'!"), s2 );

   /* Check version. */
   res = semver_compare( sv1, sv2 );

   /* Cleanup. */
   semver_free( &sv1 );
   semver_free( &sv2 );

   lua_pushinteger(L,res);
   return 1;
}

/**
 * @brief Gets the current language locale.
 *
 *    @luatreturn string Current language locale (such as "en" for English, "de" for German, or "ja" for Japanese).
 * @luafunc language
 */
static int naevL_language( lua_State *L )
{
   lua_pushstring( L, gettext_getLanguage() );
   return 1;
}

/**
 * @brief Gets how many days it has been since the player last played Naev.
 *
 *    @luatreturn number Number of days since the player last played.
 * @luafunc lastplayed
 */
static int naevL_lastplayed( lua_State *L )
{
   double d = difftime( time(NULL), player.last_played );
   lua_pushnumber(L, d/(3600.*24.)); /*< convert to days */
   return 1;
}

/**
 * @brief Gets the game seconds since the program started running.
 *
 * These are modified by whatever speed up the player has.
 *
 *    @luatreturn number The seconds since the application started running.
 * @luafunc ticksGame
 */
static int naevL_ticksGame( lua_State *L )
{
   lua_pushnumber(L, elapsed_time_mod );
   return 1;
}

/**
 * @brief Gets the seconds since the program started running.
 *
 * Useful for doing timing on Lua functions.
 *
 *    @luatreturn number The seconds since the application started running.
 * @luafunc ticks
 */
static int naevL_ticks( lua_State *L )
{
   lua_pushnumber(L, (double)SDL_GetTicks() / 1000.);
   return 1;
}

/**
 * @brief Gets the approximate CPU processing time.
 *
 *    @luatreturn number Seconds elapsed since start of the process.
 * @luafunc clock
 */
static int naevL_clock( lua_State *L )
{
   lua_pushnumber(L, (double)clock() / (double)CLOCKS_PER_SEC );
   return 1;
}

/**
 * @brief Gets a human-readable name for the key bound to a function.
 *
 * @usage bindname = naev.keyGet( "accel" )
 *
 *    @luatparam string keyname Name of the keybinding to get value of. Valid values are listed in src/input.c: keybind_info.
 * @luafunc keyGet
 */
static int naevL_keyGet( lua_State *L )
{
   char buf[128];
   const char *keyname = luaL_checkstring( L, 1 );

   input_getKeybindDisplay( keyname, buf, sizeof(buf) );
   lua_pushstring( L, buf );

   return 1;
}

/**
 * @brief Disables or enables a specific keybinding.
 *
 * Use with caution, this can make the player get stuck.
 *
 * @usage naev.keyEnable( "accel", false ) -- Disables the acceleration key
 *    @luatparam string keyname Name of the key to disable (for example "accel").
 *    @luatparam[opt=false] boolean enable Whether to enable or disable.
 * @luafunc keyEnable
 */
static int naevL_keyEnable( lua_State *L )
{
   const char *key = luaL_checkstring(L,1);
   int enable = lua_toboolean(L,2);

   input_toggleEnable( key, enable );
   return 0;
}

/**
 * @brief Enables all inputs.
 *
 * @usage naev.keyEnableAll() -- Enables all inputs
 * @luafunc keyEnableAll
 */
static int naevL_keyEnableAll( lua_State *L )
{
   (void) L;
   input_enableAll();
   return 0;
}

/**
 * @brief Disables all inputs.
 *
 * @usage naev.keyDisableAll() -- Disables all inputs
 * @luafunc keyDisableAll
 */
static int naevL_keyDisableAll( lua_State *L )
{
   (void) L;
   input_disableAll();
   return 0;
}

/**
 * @brief Starts an event, does not start check conditions.
 *
 * @usage naev.eventStart( "Some Event" )
 *    @luatparam string evtname Name of the event to start.
 *    @luatreturn boolean true on success.
 * @luafunc eventStart
 */
static int naevL_eventStart( lua_State *L )
{
   const char *str = luaL_checkstring(L, 1);
   int ret = event_start( str, NULL );

   if (cli_isOpen() && landed)
      bar_regen();

   lua_pushboolean( L, !ret );
   return 1;
}

/**
 * @brief Starts a mission, does no check start conditions.
 *
 * @usage naev.missionStart( "Some Mission" )
 *    @luatparam string misnname Name of the mission to start.
 *    @luatreturn boolean true if mission was either accepted, or started without misn.finish() getting called in create.
 *    @luatreturn boolean true whether or not the mission was accepted.
 * @luafunc missionStart
 */
static int naevL_missionStart( lua_State *L )
{
   const char *str = luaL_checkstring(L, 1);
   int ret = mission_start( str, NULL );

   if (cli_isOpen() && landed)
      bar_regen();

   lua_pushboolean( L, (ret==0) || (ret==3) );
   lua_pushboolean( L, (ret==3) );
   return 2;
}

/**
 * @brief Reloads an event's script, providing a convenient way to test and hopefully not corrupt the game's state.
 *        Use with caution, and only during development as a way to get quicker feedback.
 *
 * @usage naev.eventReload( "Some Event" )
 *    @luatparam string evtname Name of the event to start.
 *    @luatreturn boolean true on success.
 * @luafunc eventReload
 */
static int naevL_eventReload( lua_State *L )
{
   const char *str = luaL_checkstring(L, 1);
   int ret = event_reload( str );

   lua_pushboolean( L, !ret );
   return 1;
}

/**
 * @brief Reloads a mission's script, providing a convenient way to test and hopefully not corrupt the game's state.
 *        Use with caution, and only during development as a way to get quicker feedback.
 *
 * @usage naev.missionReload( "Some Mission" )
 *    @luatparam string misnname Name of the mission to start.
 *    @luatreturn boolean true on success.
 * @luafunc missionReload
 */
static int naevL_missionReload( lua_State *L )
{
   const char *str = luaL_checkstring(L, 1);
   int ret = mission_reload( str );

   lua_pushboolean( L, !ret );
   return 1;
}

/**
 * @brief Reloads all the Naev shaders excluding those created by the shader library.
 *
 * @luafunc shadersReload
 */
static int naevL_shadersReload( lua_State *L )
{
   (void) L;
   shaders_unload();
   shaders_load();
   return 0;
}

/**
 * @brief Gets whether or not the universe is being simulated or not.
 *
 *    @luatreturn boolean true if the world is being simulated.
 * @luafunc isSimulation
 */
static int naevL_isSimulation( lua_State *L )
{
   lua_pushboolean( L, space_isSimulation() );
   return 1;
}

#define PUSH_STRING( L, name, value ) \
lua_pushstring( L, name ); \
lua_pushstring( L, value ); \
lua_rawset( L, -3 )
#define PUSH_DOUBLE( L, name, value ) \
lua_pushstring( L, name ); \
lua_pushnumber( L, value ); \
lua_rawset( L, -3 )
#define PUSH_INT( L, name, value ) \
lua_pushstring( L, name ); \
lua_pushinteger( L, value ); \
lua_rawset( L, -3 )
#define PUSH_BOOL( L, name, value ) \
lua_pushstring( L, name ); \
lua_pushboolean( L, value ); \
lua_rawset( L, -3 )
/**
 * @brief Gets the configuration information.
 *
 *    @luatreturn table Table of configuration values as they appear in the configuration file.
 * @luafunc conf
 */
static int naevL_conf( lua_State *L )
{
   lua_newtable(L);
   PUSH_STRING( L, "data", conf.ndata );
   PUSH_STRING( L, "language", conf.language );
   PUSH_STRING( L, "difficulty", conf.difficulty );
   PUSH_INT( L, "fsaa", conf.fsaa );
   PUSH_BOOL( L, "vsync", conf.vsync );
   PUSH_INT( L, "width", conf.width );
   PUSH_INT( L, "height", conf.height );
   PUSH_DOUBLE( L, "scalefactor", conf.scalefactor );
   PUSH_DOUBLE( L, "nebu_scale", conf.nebu_scale );
   PUSH_BOOL( L, "fullscreen", conf.fullscreen );
   PUSH_BOOL( L, "modesetting", conf.modesetting );
   PUSH_BOOL( L, "notresizable", conf.notresizable );
   PUSH_BOOL( L, "borderless", conf.borderless );
   PUSH_BOOL( L, "minimize", conf.minimize );
   PUSH_BOOL( L, "colorblind", conf.colorblind );
   PUSH_DOUBLE( L, "bg_brightness", conf.bg_brightness );
   PUSH_DOUBLE( L, "gamma_correction", conf.gamma_correction );
   PUSH_BOOL( L, "background_fancy", conf.background_fancy );
   PUSH_BOOL( L, "showfps", conf.fps_show );
   PUSH_INT( L, "maxfps", conf.fps_max );
   PUSH_BOOL( L, "showpause", conf.pause_show );
   PUSH_BOOL( L, "al_efx", conf.al_efx );
   PUSH_BOOL( L, "nosound", conf.nosound );
   PUSH_DOUBLE( L, "sound", conf.sound );
   PUSH_DOUBLE( L, "music", conf.music );
   /* joystick */
   PUSH_INT( L, "mesg_visible", conf.mesg_visible );
   PUSH_DOUBLE( L, "map_overlay_opacity", conf.map_overlay_opacity );
   PUSH_BOOL( L, "big_icons", conf.big_icons );
   PUSH_INT( L, "repeat_delay", conf.repeat_delay );
   PUSH_INT( L, "repeat_freq", conf.repeat_freq );
   PUSH_BOOL( L, "zoom_manual", conf.zoom_manual );
   PUSH_DOUBLE( L, "zoom_far", conf.zoom_far );
   PUSH_DOUBLE( L, "zoom_near", conf.zoom_near );
   PUSH_DOUBLE( L, "zoom_speed", conf.zoom_speed );
   PUSH_DOUBLE( L, "zoom_stars", conf.zoom_stars );
   PUSH_INT( L, "font_size_console", conf.font_size_console );
   PUSH_INT( L, "font_size_intro", conf.font_size_intro );
   PUSH_INT( L, "font_size_def", conf.font_size_def );
   PUSH_INT( L, "font_size_small", conf.font_size_small );
   PUSH_DOUBLE( L, "compression_velocity", conf.compression_velocity );
   PUSH_DOUBLE( L, "compression_mult", conf.compression_mult );
   PUSH_BOOL( L, "redirect_file", conf.redirect_file );
   PUSH_BOOL( L, "save_compress", conf.save_compress );
   PUSH_INT( L, "doubletap_sensitivity", conf.doubletap_sens );
   PUSH_INT( L, "mouse_thrust", conf.mouse_thrust );
   PUSH_DOUBLE( L, "mouse_doubleclick", conf.mouse_doubleclick );
   PUSH_DOUBLE( L, "autonav_reset_dist", conf.autonav_reset_dist );
   PUSH_DOUBLE( L, "autonav_reset_shield", conf.autonav_reset_shield );
   PUSH_BOOL( L, "devmode", conf.devmode );
   PUSH_BOOL( L, "devautosave", conf.devautosave );
   PUSH_BOOL( L, "conf_nosave", conf.nosave );
   PUSH_STRING( L, "last_version", conf.lastversion );
   PUSH_BOOL( L, "translation_warning_seen", conf.translation_warning_seen );
   PUSH_BOOL( L, "fpu_except", conf.fpu_except );
   PUSH_STRING( L, "dev_save_sys", conf.dev_save_sys );
   PUSH_STRING( L, "dev_save_map", conf.dev_save_map );
   PUSH_STRING( L, "dev_save_spob", conf.dev_save_spob );
   return 1;
}
#undef PUSH_STRING
#undef PUSH_DOUBLE
#undef PUSH_INT
#undef PUSH_BOOL

/**
 * @brief Sets configuration variables. Note that not all are supported.
 *
 *    @luatparam string name Configuration variable name.
 *    @luatparam number|string value Value to set to.
 * @luafunc confSet
 */
static int naevL_confSet( lua_State *L )
{
   (void) L;
   /* TODO implement. */
   NLUA_ERROR(L, _("unimplemented"));
   return 0;
}

/**
 * @brief Gets the global Lua runtime cache. This is shared among all
 * environments and is cleared when the game is closed.
 *
 * @usage c = naev.cache()
 *
 *    @luatreturn table The Lua global cache.
 * @luafunc cache
 */
static int naevL_cache( lua_State *L )
{
   lua_rawgeti( L, LUA_REGISTRYINDEX, cache_table );
   return 1;
}

/**
 * @brief Triggers manually a hook stack. This is run deferred (next frame). Meant mainly to be used with hook.custom, but can work with other hooks too (if you know what you are doing).
 *
 * @note This will trigger all hooks waiting on a stack.
 *
 * @usage naev.trigger( "my_event", data ) -- data will be passed to the receiving end
 *
 *    @luatparam string hookname Name of the hook to be run.
 *    @luaparam arg Parameter to pass to the hooks.
 * @see custom
 * @luafunc trigger
 */
static int naevL_trigger( lua_State *L )
{
   HookParam hp[HOOK_MAX_PARAM];
   const char *hookname = luaL_checkstring(L,1);

   /* Set up hooks. */
   if (lua_isnoneornil(L,2)) {
      /* Since this doesn't get saved and is triggered by Lua code, we can
       * actually pass references here. */
      hp[0].type = HOOK_PARAM_REF;
      lua_pushvalue(L,2);
      hp[0].u.ref = luaL_ref( L, LUA_REGISTRYINDEX );
      hp[1].type = HOOK_PARAM_SENTINEL;
   }
   else
      hp[0].type = HOOK_PARAM_SENTINEL;

   /* Run the deferred hooks. */
   hooks_runParamDeferred( hookname, hp );
   return 0;
}

/**
 * @brief Tests a claim of a system or strings.
 *
 * @usage if not naev.claimTest( { system.get("Gamma Polaris") } ) then print("Failed to claim!") end
 *
 *    @luatparam System|String|{System,String...} params Table of systems/strings to claim or a single system/string.
 *    @luatparam[opt=false] boolean inclusive Whether or not to allow the claim to include other inclusive claims. Multiple missions/events can inclusively claim the same system, but only one system can exclusively claim it.
 *    @luatreturn boolean true if it is possible to claim, false otherwise.
 * @luafunc claimTest
 */
static int naevL_claimTest( lua_State *L )
{
   int inclusive = lua_toboolean(L,2);
   Claim_t *claim = claim_create( !inclusive );

   if (lua_istable(L,1)) {
      /* Iterate over table. */
      lua_pushnil(L);
      while (lua_next(L, 1) != 0) {
         if (lua_issystem(L,-1))
            claim_addSys( claim, lua_tosystem( L, -1 ) );
         else if (lua_isstring(L,-1))
            claim_addStr( claim, lua_tostring( L, -1 ) );
         lua_pop(L,1);
      }
   }
   else if (lua_issystem(L, 1))
      claim_addSys( claim, lua_tosystem( L, 1 ) );
   else if (lua_isstring(L, 1))
      claim_addStr( claim, lua_tostring( L, 1 ) );
   else
      NLUA_INVALID_PARAMETER(L);

   /* Only test, but don't apply case. */
   lua_pushboolean( L, !claim_test( claim ) );
   claim_destroy( claim );
   return 1;
}

/**
 * @brief Gets the list of available plugins.
 *
 *    @luatreturn table Table containing the list of plugins.
 * @luafunc plugins
 */
static int naevL_plugins( lua_State *L )
{
   const plugin_t *plugins = plugin_list();
   lua_newtable(L);
   for (int i=0; i<array_size(plugins); i++) {
      const plugin_t *plg = &plugins[i];
      lua_newtable(L);

#define STRING(x) \
   lua_pushstring(L,plg->x); \
   lua_setfield(L,-2,#x)
#define INTEGER(x) \
   lua_pushinteger(L,plg->x); \
   lua_setfield(L,-2,#x)
#define BOOL(x) \
   lua_pushboolean(L,plg->x); \
   lua_setfield(L,-2,#x)

      STRING(name);
      STRING(author);
      STRING(version);
      STRING(description);
      STRING(compatibility);
      STRING(mountpoint);

      INTEGER(priority);

      BOOL(compatible);
      BOOL(total_conversion);

#undef BOOL
#undef INTEGER
#undef STRING

      lua_rawseti(L,-2,i+1);
   }
   return 1;
}
