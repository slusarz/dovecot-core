/* Copyright (c) 2024 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "str.h"
#include "istream.h"
#include "message-parser.h"
#include "message-decoder.h"
#include "mail-html2text.h"
#include "message-body-extractor.h"

int message_body_extractor_get_text(pool_t pool, struct istream *input,
				    uoff_t max_size, string_t *body_r)
{
	const struct message_parser_settings parser_set = { .flags = 0 };
	struct message_parser_ctx *parser;
	struct message_part *parts;
	struct message_decoder_context *decoder;
	struct message_block raw_block, block;
	struct mail_html2text *html2text = NULL;
	buffer_t *plain_output = NULL;
	int ret = 0;

	if (max_size == 0)
		max_size = UOFF_T_MAX;

	str_truncate(body_r, 0);

	parser = message_parser_init(pool_datastack_create(), input, &parser_set);
	decoder = message_decoder_init(NULL, 0);
	while ((ret = message_parser_parse_next_block(parser, &raw_block)) > 0) {
		if (!message_decoder_decode_next_block(decoder, &raw_block, &block))
			continue;
		if (block.size == 0) {
			const char *ct;
			if (block.hdr != NULL)
				continue;
			if (str_len(body_r) > 0)
				break; /* already found a part */

			ct = message_decoder_current_content_type(decoder);
			if (ct == NULL)
				/* text/plain */ ;
			else if (mail_html2text_content_type_match(ct)) {
				mail_html2text_deinit(&html2text);
				html2text = mail_html2text_init(0);
				if (plain_output == NULL)
					plain_output = buffer_create_dynamic(pool, 1024);
			} else if (!str_begins_icase_with(ct, "text/")) {
				/* skip this part's body */
				message_parser_skip_body(parser);
			}
		} else {
			const unsigned char *data = block.data;
			size_t size = block.size;

			if (html2text != NULL) {
				buffer_set_used_size(plain_output, 0);
				mail_html2text_more(html2text, data, size, plain_output);
				data = plain_output->data;
				size = plain_output->used;
			}
			if (str_len(body_r) + size > max_size)
				size = max_size - str_len(body_r);
			str_append_data(body_r, data, size);
			if (str_len(body_r) >= max_size)
				break;
		}
	}
	message_decoder_deinit(&decoder);
	message_parser_deinit(&parser, &parts);
	mail_html2text_deinit(&html2text);
	if (plain_output != NULL)
		buffer_free(&plain_output);
	return (ret < 0 && input->stream_errno != 0) ? -1 : 0;
}
