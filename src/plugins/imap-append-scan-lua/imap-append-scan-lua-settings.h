#ifndef IMAP_APPEND_SCAN_LUA_SETTINGS_H
#define IMAP_APPEND_SCAN_LUA_SETTINGS_H

struct imap_append_scan_lua_settings {
	const char *imap_append_scan_lua_script;
};

extern const struct setting_parser_info imap_append_scan_lua_setting_parser_info;

#endif
