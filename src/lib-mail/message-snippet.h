#ifndef MESSAGE_SNIPPET_H
#define MESSAGE_SNIPPET_H

/* Generate UTF-8 text snippet from the beginning of the given mail.
   Returns 0 if ok, -1 if I/O error.

   Currently only Content-Type: text/ is supported, others will result in an
   empty string. */
int message_snippet_generate(struct mail *mail,
			     unsigned int max_snippet_chars,
			     string_t *snippet);

#endif
