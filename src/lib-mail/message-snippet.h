#ifndef MESSAGE_SNIPPET_H
#define MESSAGE_SNIPPET_H

#include "message-snippet-driver.h"

/* Generate UTF-8 text snippet from the beginning of the given mail input
   stream. The stream is expected to start at the MIME part's headers whose
   snippet is being generated. Returns 0 if ok, -1 if I/O error. */
int message_snippet_generate(const struct message_snippet_settings *set,
			     struct istream *input,
			     string_t *snippet);

#endif
