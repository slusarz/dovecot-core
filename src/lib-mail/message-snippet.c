/* Copyright (c) 2015-2018 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "buffer.h"
#include "str.h"
#include "istream.h"
#include "mail-html2text.h"
#include "istream-message-body-extractor.h"
#include "message-snippet.h"

#include <ctype.h>

int message_snippet_generate(struct mail *mail,
			     unsigned int max_snippet_chars,
			     string_t *snippet)
{
	struct istream *input;
	const unsigned char *data;
	size_t size;
	int ret = 0;

	input = i_stream_create_message_body_extractor(mail, 0);
	while (i_stream_read_more(input, &data, &size) > 0) {
		str_append_data(snippet, data, I_MIN(size, max_snippet_chars));
		max_snippet_chars -= I_MIN(size, max_snippet_chars);
		if (max_snippet_chars == 0)
			break;
		i_stream_skip(input, size);
	}
	if (input->stream_errno != 0)
		ret = -1;
	i_stream_destroy(&input);
	return ret;
}
