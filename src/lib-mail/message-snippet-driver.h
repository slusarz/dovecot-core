/* Copyright (c) 2024 Dovecot authors, see the included COPYING file */

#ifndef MESSAGE_SNIPPET_DRIVER_H
#define MESSAGE_SNIPPET_DRIVER_H

#include "unlib.h"

struct istream;
struct message_snippet_driver;

struct message_snippet_settings {
	const char *driver;
	unsigned int max_snippet_chars;

	/* ollama driver settings */
	const char *ollama_url;
	const char *ollama_model;
	const char *ollama_prompt;
	uoff_t ollama_max_input_size;
	unsigned int ollama_timeout;
};

struct message_snippet_driver_vfuncs {
	/* Called to generate the snippet. */
	int (*generate)(const struct message_snippet_settings *set,
			struct istream *input, string_t *snippet);
};

struct message_snippet_driver {
	const char *name;
	struct message_snippet_driver_vfuncs v;
};

/* Register/unregister snippet drivers. */
void message_snippet_driver_register(const struct message_snippet_driver *driver);
void message_snippet_driver_unregister(const struct message_snippet_driver *driver);

/* Returns a list of all available drivers. */
const struct message_snippet_driver *const *
message_snippet_driver_get_list(void) ATTR_PURE;

/* Find a driver by its name. */
const struct message_snippet_driver *
message_snippet_driver_find(const char *name);

extern const struct message_snippet_driver message_snippet_driver_internal;
extern const struct message_snippet_driver message_snippet_driver_ollama;

#endif
