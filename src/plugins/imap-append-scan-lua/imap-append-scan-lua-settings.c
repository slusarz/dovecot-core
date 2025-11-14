#include "lib.h"
#include "settings-parser.h"
#include "imap-append-scan-lua-settings.h"

#undef DEF
#define DEF(type, name) \
	SETTING_DEFINE_STRUCT_##type(#name, name, struct imap_append_scan_lua_settings)

static const struct setting_define imap_append_scan_lua_setting_defines[] = {
	DEF(STRING, imap_append_scan_lua_script),
	SETTING_DEFINE_LIST_END
};

const struct setting_parser_info imap_append_scan_lua_setting_parser_info = {
	.module_name = "imap_append_scan_lua",
	.defines = imap_append_scan_lua_setting_defines,
	.type_offset = (size_t)-1,
	.struct_size = sizeof(struct imap_append_scan_lua_settings),
	.parent_offset = (size_t)-1
};
