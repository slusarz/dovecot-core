/* Copyright (c) 2023 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "module-dir.h"
#include "settings.h"
#include "client-command-private.h"
#include "mail-user.h"
#include "imap-common.h"
#include "dlua-script-private.h"
#include "imap-append-hooks.h"
#include "imap-append-scan-lua-plugin.h"
#include "imap-append-scan-lua-settings.h"
#include "imap-append-scan-lua-mail.h"

static int
imap_append_scan_lua_pre_save(struct client_command_context *cmd,
                              struct istream *input);

static const struct imap_append_hooks imap_append_scan_lua_hooks = {
    .imap_append_pre_save = imap_append_scan_lua_pre_save,
};

static const struct setting_parser_info *imap_append_scan_lua_setting_roots[] = {
    &mail_user_setting_parser_info,
    NULL
};

static const struct setting_addon_info imap_append_scan_lua_setting_addon_info = {
    .part_name = "imap_append_scan_lua",
    .roots = imap_append_scan_lua_setting_roots,
    .addon_info = &imap_append_scan_lua_setting_parser_info,
};

static struct dlua_script *scan_script = NULL;

void imap_append_scan_lua_plugin_init(struct module *module ATTR_UNUSED)
{
    settings_parser_register_addon(&imap_append_scan_lua_setting_addon_info);
    imap_append_hooks_add(&imap_append_scan_lua_hooks);
}

void imap_append_scan_lua_plugin_deinit(void)
{
    settings_parser_unregister_addon(&imap_append_scan_lua_setting_addon_info);
    imap_append_hooks_remove(&imap_append_scan_lua_hooks);
    if (scan_script != NULL)
        dlua_script_unref(&scan_script);
}

static int
imap_append_scan_lua_script_load(const char *file, struct dlua_script **script_r,
                                 const char **error_r)
{
    if (scan_script != NULL) {
        if (strcmp(dlua_script_get_path(scan_script), file) == 0) {
            *script_r = scan_script;
            return 0;
        }
        dlua_script_unref(&scan_script);
    }

    struct dlua_script *script;
    if (dlua_script_create_file(file, &script, NULL, error_r) < 0)
        return -1;
    dlua_dovecot_register(script);
    imap_append_scan_lua_register_mail(script);
    if (dlua_script_init(script, error_r) < 0) {
        dlua_script_unref(&script);
        return -1;
    }
    scan_script = script;
    *script_r = script;
    return 0;
}


static int
imap_append_scan_lua_pre_save(struct client_command_context *cmd,
                              struct istream *input)
{
    const struct imap_append_scan_lua_settings *settings;
    struct mail_user_settings *mail_settings;
    struct dlua_script *script;
    const char *error;
    const char *scan_function_name = "scan_append_message";

    mail_settings = mail_user_get_settings(cmd->client->user);
    settings = p_get_addon_settings(mail_settings, "imap_append_scan_lua");

    if (settings->imap_append_scan_lua_script == NULL ||
        *settings->imap_append_scan_lua_script == '\0') {
        /* No script configured, do nothing */
        return 0;
    }

    if (imap_append_scan_lua_script_load(settings->imap_append_scan_lua_script,
                                         &script, &error) < 0) {
        e_error(cmd->client->event,
                "imap-append-scan-lua: Failed to load script '%s': %s",
                settings->imap_append_scan_lua_script, error);
        /* As per requirements, allow append on script error */
        return 0;
    }

    if (!dlua_script_has_function(script, scan_function_name)) {
        /* The script doesn't have the function, so we do nothing. */
        return 0;
    }

    e_debug(cmd->client->event,
            "imap-append-scan-lua: calling %s()", scan_function_name);

    /* Push the mail object */
    imap_append_scan_lua_push_mail(script->L, cmd->client->user->username, input);

    /* Call the function */
    if (dlua_pcall(script->L, scan_function_name, 1, 1, &error) < 0) {
        e_error(cmd->client->event, "imap-append-scan-lua: %s() failed: %s",
                scan_function_name, error);
        /* On error, allow append */
        return 0;
    }

    /* Get the return value */
    bool allow_append = TRUE;
    if (lua_isboolean(script->L, -1)) {
        allow_append = lua_toboolean(script->L, -1);
    } else if (!lua_isnil(script->L, -1)) {
        e_error(cmd->client->event, "imap-append-scan-lua: %s() returned non-boolean, non-nil value. Allowing append.",
                scan_function_name);
    }
    /* if nil, it's treated as true (default) */

    lua_pop(script->L, 1);
    (void)lua_gc(script->L, LUA_GCCOLLECT, 0);


    if (!allow_append) {
        e_info(cmd->client->event,
               "imap-append-scan-lua: Message append rejected by script '%s'",
               settings->imap_append_scan_lua_script);
        client_send_tagline(cmd, "NO Message rejected due to server policy violation.");
        return -1;
    }

    return 0;
}

const char *imap_append_scan_lua_plugin_version = DOVECOT_ABI_VERSION;
const char imap_append_scan_lua_plugin_binary_dependency[] = "imap";
