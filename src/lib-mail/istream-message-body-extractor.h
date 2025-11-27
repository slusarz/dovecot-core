#ifndef ISTREAM_MESSAGE_BODY_EXTRACTOR_H
#define ISTREAM_MESSAGE_BODY_EXTRACTOR_H

#include "istream.h"

struct mail;

enum message_body_extractor_flags {
	/* By default, the first text part found is used. */
	MESSAGE_BODY_EXTRACTOR_FLAG_PREFER_TEXT_PLAIN	= 0x01,
	MESSAGE_BODY_EXTRACTOR_FLAG_PREFER_TEXT_HTML	= 0x02,
	MESSAGE_BODY_EXTRACTOR_FLAG_ONLY_TEXT_PLAIN	= 0x04,
	MESSAGE_BODY_EXTRACTOR_FLAG_ONLY_TEXT_HTML	= 0x08,

	/* By default, quoting is removed. */
	MESSAGE_BODY_EXTRACTOR_FLAG_RETAIN_QUOTING	= 0x10,
	/* By default, newlines are preserved. */
	MESSAGE_BODY_EXTRACTOR_FLAG_REMOVE_NEWLINES	= 0x20
};

struct istream *
i_stream_create_message_body_extractor(struct mail *mail,
				       enum message_body_extractor_flags flags);
bool message_body_extractor_part_exists(struct mail *mail,
					enum message_body_extractor_flags flags);

#endif
