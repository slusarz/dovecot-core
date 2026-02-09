#ifndef MESSAGE_SNIPPET_DRIVER_H
#define MESSAGE_SNIPPET_DRIVER_H

struct mail;
struct istream;

struct message_snippet_driver {
	const char *name;
	int (*generate)(struct mail *mail, struct istream *input,
			unsigned int max_snippet_chars, string_t *snippet);
};

void message_snippet_driver_register(const struct message_snippet_driver *driver);
void message_snippet_driver_unregister(const struct message_snippet_driver *driver);
const struct message_snippet_driver *message_snippet_driver_find(const char *name);

void message_snippet_driver_init(void);
void message_snippet_driver_deinit(void);

#endif
