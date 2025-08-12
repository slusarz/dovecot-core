/* Copyright (c) 2024 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "buffer.h"
#include "str.h"
#include "istream.h"
#include "message-body-extractor.h"
#include "message-snippet-driver.h"

#include <ctype.h>

#define INTERNAL_SNIPPET_MAX_BODY_SIZE 4096

enum snippet_state {
	/* beginning of the line */
	SNIPPET_STATE_NEWLINE = 0,
	/* within normal text */
	SNIPPET_STATE_NORMAL,
	/* within quoted text - skip until EOL */
	SNIPPET_STATE_QUOTED
};

struct snippet_data {
	string_t *snippet;
	unsigned int chars_left;
};

struct snippet_context {
	struct snippet_data snippet;
	struct snippet_data quoted_snippet;
	enum snippet_state state;
	bool add_whitespace;
};

static void snippet_add_content(struct snippet_context *ctx,
				struct snippet_data *target,
				const unsigned char *data, size_t size,
				size_t *count_r)
{
	i_assert(target != NULL);
	if (size == 0)
		return;
	if (size >= 3 &&
	     ((data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) ||
	      (data[0] == 0xBF && data[1] == 0xBB && data[2] == 0xEF))) {
		*count_r = 3;
		return;
	}
	if (data[0] == '\0') {
		/* skip NULs without increasing snippet size */
		return;
	}
	if (i_isspace(*data)) {
		/* skip any leading whitespace */
		if (str_len(target->snippet) > 0)
			ctx->add_whitespace = TRUE;
		if (data[0] == '\n')
			ctx->state = SNIPPET_STATE_NEWLINE;
		return;
	}
	if (target->chars_left == 0)
		return;
	target->chars_left--;
	if (ctx->add_whitespace) {
		if (target->chars_left == 0) {
			/* don't add a trailing whitespace */
			return;
		}
		str_append_c(target->snippet, ' ');
		ctx->add_whitespace = FALSE;
		target->chars_left--;
	}
	*count_r = uni_utf8_char_bytes(data[0]);
	i_assert(*count_r <= size);
	str_append_data(target->snippet, data, *count_r);
}

static void snippet_copy(const char *src, string_t *dst)
{
	while (*src != '\0' && i_isspace(*src)) src++;
	str_append(dst, src);
}

static int
message_snippet_driver_internal_generate(const struct message_snippet_settings *set,
					 struct istream *input, string_t *snippet)
{
	struct snippet_context ctx;
	pool_t pool;
	string_t *body;
	const unsigned char *data;
	size_t i, size, count;
	struct snippet_data *target;

	pool = pool_alloconly_create("internal snippet", 2048);
	body = str_new(pool, INTERNAL_SNIPPET_MAX_BODY_SIZE);
	if (message_body_extractor_get_text(pool, input,
					    INTERNAL_SNIPPET_MAX_BODY_SIZE,
					    body) < 0) {
		i_error("message-snippet: internal driver failed to read body: %s",
			i_stream_get_error(input));
		pool_unref(&pool);
		return -1;
	}

	i_zero(&ctx);
	ctx.snippet.snippet = str_new(pool, set->max_snippet_chars);
	ctx.snippet.chars_left = set->max_snippet_chars;
	ctx.quoted_snippet.snippet = str_new(pool, set->max_snippet_chars);
	ctx.quoted_snippet.chars_left = set->max_snippet_chars > 0 ?
		set->max_snippet_chars - 1 : 0; /* -1 for '>' */

	data = str_data(body);
	size = str_len(body);

	if (ctx.state == SNIPPET_STATE_QUOTED)
		target = &ctx.quoted_snippet;
	else
		target = &ctx.snippet;

	for (i = 0; i < size; i += count) {
		count = 1;
		switch (ctx.state) {
		case SNIPPET_STATE_NEWLINE:
			if (data[i] == '>') {
				ctx.state = SNIPPET_STATE_QUOTED;
				i++;
				target = &ctx.quoted_snippet;
			} else {
				ctx.state = SNIPPET_STATE_NORMAL;
				target = &ctx.snippet;
			}
			/* fallthrough */
		case SNIPPET_STATE_NORMAL:
		case SNIPPET_STATE_QUOTED:
			snippet_add_content(&ctx, target, CONST_PTR_OFFSET(data, i),
					    size-i, &count);
			if (ctx.snippet.chars_left == 0)
				goto end;
			break;
		}
	}
end:
	if (ctx.snippet.snippet->used != 0)
		snippet_copy(str_c(ctx.snippet.snippet), snippet);
	else if (ctx.quoted_snippet.snippet->used != 0) {
		str_append_c(snippet, '>');
		snippet_copy(str_c(ctx.quoted_snippet.snippet), snippet);
	}
	pool_unref(&pool);
	return 0;
}

const struct message_snippet_driver message_snippet_driver_internal = {
	.name = "internal",
	.v = {
		.generate = message_snippet_driver_internal_generate,
	}
};
