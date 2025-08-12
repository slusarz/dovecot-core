/* Copyright (c) 2024 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "array.h"
#include "message-snippet-driver.h"

static ARRAY(const struct message_snippet_driver *) message_snippet_drivers;

void message_snippet_driver_register(const struct message_snippet_driver *driver)
{
	if (!array_is_created(&message_snippet_drivers))
		i_array_init(&message_snippet_drivers, 4);

	/* make sure it's not already there */
	const struct message_snippet_driver *const *drivers;
	unsigned int i, count;

	drivers = array_get(&message_snippet_drivers, &count);
	for (i = 0; i < count; i++) {
		if (drivers[i] == driver)
			return;
	}
	array_push_back(&message_snippet_drivers, &driver);
}

void message_snippet_driver_unregister(const struct message_snippet_driver *driver)
{
	const struct message_snippet_driver *const *drivers;
	unsigned int i, count;

	if (!array_is_created(&message_snippet_drivers))
		return;

	drivers = array_get(&message_snippet_drivers, &count);
	for (i = 0; i < count; i++) {
		if (drivers[i] == driver) {
			array_delete(&message_snippet_drivers, i, 1);
			break;
		}
	}
	if (array_count(&message_snippet_drivers) == 0)
		array_free(&message_snippet_drivers);
}

const struct message_snippet_driver *
message_snippet_driver_find(const char *name)
{
	const struct message_snippet_driver *const *drivers;

	if (!array_is_created(&message_snippet_drivers))
		return NULL;

	array_foreach(&message_snippet_drivers, drivers) {
		if (strcmp((*drivers)->name, name) == 0)
			return *drivers;
	}
	return NULL;
}

const struct message_snippet_driver *const *
message_snippet_driver_get_list(void)
{
	if (!array_is_created(&message_snippet_drivers)) {
		static const struct message_snippet_driver *empty_list = NULL;
		return &empty_list;
	}
	return array_front(&message_snippet_drivers);
}

void message_snippet_drivers_register_all(void)
{
	message_snippet_driver_register(&message_snippet_driver_internal);
	message_snippet_driver_register(&message_snippet_driver_ollama);
}

void message_snippet_drivers_unregister_all(void)
{
	if (!array_is_created(&message_snippet_drivers))
		return;
	message_snippet_driver_unregister(&message_snippet_driver_internal);
	message_snippet_driver_unregister(&message_snippet_driver_ollama);
}
