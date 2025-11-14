#include "lib.h"
#include "istream.h"
#include "imap-append-scan-lua-mail.h"
#include "dlua-script-private.h"

#define IMAP_APPEND_SCAN_LUA_MAIL "imap_append_scan_mail"

struct imap_append_scan_lua_mail {
    const char *username;
    struct istream *input;
};

static int imap_append_scan_lua_mail_get_username(lua_State *L)
{
    struct imap_append_scan_lua_mail *mail =
        luaL_checkudata(L, 1, IMAP_APPEND_SCAN_LUA_MAIL);
    lua_pushstring(L, mail->username);
    return 1;
}

static int imap_append_scan_lua_mail_get_stream(lua_State *L)
{
    struct imap_append_scan_lua_mail *mail =
        luaL_checkudata(L, 1, IMAP_APPEND_SCAN_LUA_MAIL);
    dlua_push_istream(L, mail->input);
    return 1;
}

static const struct luaL_Reg imap_append_scan_lua_mail_methods[] = {
    { "get_username", imap_append_scan_lua_mail_get_username },
    { "get_stream", imap_append_scan_lua_mail_get_stream },
    { NULL, NULL }
};

void imap_append_scan_lua_register_mail(struct dlua_script *script)
{
    luaL_newmetatable(script->L, IMAP_APPEND_SCAN_LUA_MAIL);
    lua_pushvalue(script->L, -1);
    lua_setfield(script->L, -2, "__index");
    luaL_setfuncs(script->L, imap_append_scan_lua_mail_methods, 0);
    lua_pop(script->L, 1);
}

void imap_append_scan_lua_push_mail(lua_State *L, const char *username, struct istream *input)
{
    struct imap_append_scan_lua_mail *mail =
        lua_newuserdata(L, sizeof(*mail));
    mail->username = username;
    mail->input = input;
    luaL_setmetatable(L, IMAP_APPEND_SCAN_LUA_MAIL);
}
