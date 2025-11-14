include $(top_srcdir)/Makefile.test.include

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	-I$(top_srcdir)/src/lib-mail \
	-I$(top_srcdir)/src/lib-imap \
	-I$(top_srcdir)/src/lib-lua \
	$(LUA_CFLAGS)

NOPLUGIN_LDFLAGS =

module_LTLIBRARIES = \
	lib20_imap_append_scan_lua_plugin.la

lib20_imap_append_scan_lua_plugin_la_LDFLAGS = -module -avoid-version $(LUA_LIBS)
lib20_imap_append_scan_lua_plugin_la_LIBADD = \
	$(LIBDOVECOT)
lib20_imap_append_scan_lua_plugin_la_SOURCES = \
	imap-append-scan-lua-plugin.c \
	imap-append-scan-lua-settings.c \
	imap-append-scan-lua-mail.c

noinst_HEADERS = \
	imap-append-scan-lua-settings.h \
	imap-append-scan-lua-mail.h

dist_data_DATA = imap-append-scan-lua.lua
