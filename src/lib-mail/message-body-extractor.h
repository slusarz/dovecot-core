/* Copyright (c) 2024 Dovecot authors, see the included COPYING file */

#ifndef MESSAGE_BODY_EXTRACTOR_H
#define MESSAGE_BODY_EXTRACTOR_H

struct istream;

/* Extracts the plain text body from the given mail input stream.
   It finds the first text/plain or text/html part, converts it to
   plain text, and stores it in the body_r buffer.
   The extraction is limited to max_size bytes. If max_size is 0,
   the full body is extracted.
   Returns 0 if ok, -1 if I/O error.
*/
int message_body_extractor_get_text(pool_t pool, struct istream *input,
				    uoff_t max_size, string_t *body_r);

#endif
