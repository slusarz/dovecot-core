/* Copyright (c) 2024 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "istream.h"
#include "message-snippet.h"
#include "message-snippet-driver.h"

int message_snippet_generate(const struct message_snippet_settings *set,
			     struct istream *input,
			     string_t *snippet)
{
	const struct message_snippet_driver *driver;

	driver = message_snippet_driver_find(set->driver);
	if (driver == NULL) {
		i_error("message_snippet_generate: unknown driver '%s'",
			set->driver);
		driver = &message_snippet_driver_internal;
	}
	return driver->v.generate(set, input, snippet);
}
