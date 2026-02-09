/* Copyright (c) 2002-2018 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "array.h"
#include "str.h"
#include "istream.h"
#include "message-part.h"
#include "message-part-data.h"
#include "mail-storage.h"
#include "message-snippet.h"
#include "message-snippet-driver.h"

#define BODY_SNIPPET_ALGO_V1 "1"

static ARRAY(const struct message_snippet_driver *) message_snippet_drivers;

void message_snippet_driver_register(const struct message_snippet_driver *driver)
{
	if (!array_is_created(&message_snippet_drivers))
		i_array_init(&message_snippet_drivers, 4);
	array_push_back(&message_snippet_drivers, &driver);
}

void message_snippet_driver_unregister(const struct message_snippet_driver *driver)
{
	const struct message_snippet_driver *const *driver_p;
	unsigned int i;

	if (!array_is_created(&message_snippet_drivers))
		return;

	array_foreach(&message_snippet_drivers, driver_p) {
		if (*driver_p == driver) {
			i = array_foreach_idx(&message_snippet_drivers, driver_p);
			array_delete(&message_snippet_drivers, i, 1);
			return;
		}
	}
}

const struct message_snippet_driver *message_snippet_driver_find(const char *name)
{
	const struct message_snippet_driver *const *driver_p;

	if (!array_is_created(&message_snippet_drivers))
		return NULL;

	array_foreach(&message_snippet_drivers, driver_p) {
		if (strcmp((*driver_p)->name, name) == 0)
			return *driver_p;
	}
	return NULL;
}

static struct message_part *
find_first_text_mime_part(struct message_part *parts)
{
	struct message_part_data *body_data = parts->data;
	struct message_part *part;

	i_assert(body_data != NULL);

	if (body_data->content_type == NULL ||
	    strcasecmp(body_data->content_type, "text") == 0) {
		/* use any text/ part, even if we don't know what exactly
		   it is. */
		return parts;
	}
	if (strcasecmp(body_data->content_type, "multipart") != 0) {
		/* for now we support only text Content-Types */
		return NULL;
	}

	if (strcasecmp(body_data->content_subtype, "alternative") == 0) {
		/* text/plain > text/html > text/ */
		struct message_part *html_part = NULL, *text_part = NULL;

		for (part = parts->children; part != NULL; part = part->next) {
			struct message_part_data *sub_body_data =
				part->data;

			i_assert(sub_body_data != NULL);

			if (sub_body_data->content_type == NULL ||
			    strcasecmp(sub_body_data->content_type, "text") == 0) {
				if (sub_body_data->content_subtype == NULL ||
				    strcasecmp(sub_body_data->content_subtype, "plain") == 0)
					return part;
				if (strcasecmp(sub_body_data->content_subtype, "html") == 0)
					html_part = part;
				else
					text_part = part;
			}
		}
		return html_part != NULL ? html_part : text_part;
	}
	/* find the first usable MIME part */
	for (part = parts->children; part != NULL; part = part->next) {
		struct message_part *subpart =
			find_first_text_mime_part(part);
		if (subpart != NULL)
			return subpart;
	}
	return NULL;
}

static int
message_snippet_driver_internal_generate(struct mail *mail,
					 struct istream *input,
					 unsigned int max_snippet_chars,
					 string_t *snippet)
{
	struct message_part *parts, *part;
	struct istream *limit_input;
	int ret;

	if (mail_get_parts(mail, &parts) < 0)
		return -1;

	part = find_first_text_mime_part(parts);
	if (part == NULL) {
		str_append(snippet, BODY_SNIPPET_ALGO_V1);
		return 0;
	}

	if (input == NULL) {
		if (mail_get_stream(mail, NULL, NULL, &input) < 0)
			return -1;
	}

	i_stream_seek(input, part->physical_pos);
	limit_input = i_stream_create_limit(input, part->header_size.physical_size +
					    part->body_size.physical_size);

	str_append(snippet, BODY_SNIPPET_ALGO_V1);
	ret = message_snippet_generate(limit_input, max_snippet_chars, snippet);
	i_stream_destroy(&limit_input);
	return ret;
}

static struct message_snippet_driver message_snippet_driver_internal = {
	.name = "internal",
	.generate = message_snippet_driver_internal_generate,
};

void message_snippet_driver_init(void)
{
	message_snippet_driver_register(&message_snippet_driver_internal);
}

void message_snippet_driver_deinit(void)
{
	message_snippet_driver_unregister(&message_snippet_driver_internal);
	if (array_is_created(&message_snippet_drivers)) {
		i_assert(array_count(&message_snippet_drivers) == 0);
		array_free(&message_snippet_drivers);
	}
}
