#ifndef IMAP_APPEND_SCAN_LUA_MAIL_H
#define IMAP_APPEND_SCAN_LUA_MAIL_H

#include "dlua-script.h"

struct imap_append_scan_lua_mail;

void imap_append_scan_lua_register_mail(struct dlua_script *script);
void imap_append_scan_lua_push_mail(lua_State *L, const char *username, struct istream *input);

#endif
