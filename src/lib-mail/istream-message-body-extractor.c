#include "lib.h"
#include "buffer.h"
#include "str.h"
#include "istream.h"
#include "mail-html2text.h"
#include "message-parser.h"
#include "message-decoder.h"
#include "mail-storage.h"
#include "mail-types.h"
#include "message-part-data.h"
#include "message-part.h"
#include "istream-message-body-extractor.h"
#include "istream-private.h"
#include "mailbox.h"


#include <ctype.h>

struct message_body_extractor_istream {
	struct istream_private p;

	struct mail *mail;
	enum message_body_extractor_flags flags;

	struct istream *body_input;
	struct mail_html2text *html2text;
	buffer_t *plain_output;

	bool sent_eof;
};

static void
message_body_extractor_istream_destroy(struct iostream_private *stream);
static ssize_t
message_body_extractor_istream_read(struct istream_private *stream);
static struct message_part *
message_body_extractor_find_part(struct mail *mail,
				 enum message_body_extractor_flags flags,
				 const char **content_type_r);

static void
message_body_extractor_istream_destroy(struct iostream_private *stream)
{
	struct message_body_extractor_istream *bstream =
		(struct message_body_extractor_istream *)stream;

	i_stream_unref(&bstream->body_input);
	mail_html2text_deinit(&bstream->html2text);
	if (bstream->plain_output != NULL)
		buffer_free(&bstream->plain_output);
}

static int message_body_extractor_istream_init(struct message_body_extractor_istream *bstream)
{
	struct message_part *part;
	const char *content_type = "";

	if (bstream->body_input != NULL)
		return 0;

	part = message_body_extractor_find_part(bstream->mail, bstream->flags, &content_type);
	if (part == NULL) {
		bstream->sent_eof = TRUE;
		return 0;
	}

	if (mail_get_binary_stream(bstream->mail, part, TRUE, NULL,
				   &bstream->body_input) < 0) {
		bstream->p.istream.stream_errno = bstream->mail->box->storage->error;
		return -1;
	}

	if (strcmp(content_type, "text/html") == 0) {
		bstream->html2text = mail_html2text_init(0);
		bstream->plain_output = buffer_create_dynamic(default_pool, 1024);
	}
	return 0;
}

static ssize_t
message_body_extractor_istream_read(struct istream_private *stream)
{
	struct message_body_extractor_istream *bstream =
		(struct message_body_extractor_istream *)stream;
	const unsigned char *data;
	size_t size;
	ssize_t ret;

	if (bstream->sent_eof)
		return -1;

	if (message_body_extractor_istream_init(bstream) < 0) {
		bstream->p.istream.stream_errno = bstream->mail->box->storage->error;
		return -1;
	}
	if (bstream->sent_eof) {
		i_stream_set_input_eof(&bstream->p.istream);
		return -1;
	}

	ret = i_stream_read(bstream->body_input);
	if (ret == -1) {
		if (bstream->body_input->stream_errno != 0)
			bstream->p.istream.stream_errno = bstream->body_input->stream_errno;
		bstream->sent_eof = TRUE;
		i_stream_set_input_eof(&bstream->p.istream);
		return -1;
	}
	if (ret == 0)
		return 0;

	data = i_stream_get_data(bstream->body_input, &size);

	if (bstream->html2text != NULL) {
		buffer_set_used_size(bstream->plain_output, 0);
		mail_html2text_more(bstream->html2text, data, size,
				    bstream->plain_output);
		data = bstream->plain_output->data;
		size = bstream->plain_output->used;
	}

	i_stream_skip(bstream->body_input, size);
	i_stream_add_data(&bstream->p.istream, data, size);
	return size;
}

struct istream *
i_stream_create_message_body_extractor(struct mail *mail,
				       enum message_body_extractor_flags flags)
{
	struct message_body_extractor_istream *bstream;

	bstream = i_new(struct message_body_extractor_istream, 1);
	bstream->mail = mail;
	bstream->flags = flags;

	bstream->p.read = message_body_extractor_istream_read;
	bstream->p.iostream.destroy = message_body_extractor_istream_destroy;

	return i_stream_create(&bstream->p, NULL, -1, 0);
}

static struct message_part *
message_body_extractor_find_part(struct mail *mail,
				 enum message_body_extractor_flags flags,
				 const char **content_type_r)
{
	struct message_part *parts;
	const char *plain_ct = "", *html_ct = "";

	if (mail_get_parts(mail, &parts) < 0)
		return NULL;

	struct message_part *part = mail_find_first_text_mime_part(parts, &plain_ct, &html_ct);
	if (content_type_r != NULL) {
		if (part != NULL && part->children == NULL)
			*content_type_r = plain_ct != NULL ? plain_ct : html_ct;
	}
	return part;
}

bool message_body_extractor_part_exists(struct mail *mail,
					enum message_body_extractor_flags flags)
{
	return message_body_extractor_find_part(mail, flags, NULL) != NULL;
}
