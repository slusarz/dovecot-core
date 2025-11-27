/* Copyright (c) 2015-2018 Dovecot authors, see the included COPYING file */

#include "mail-storage.h"
#include "mail-user.h"
#include "test-common.h"
#include "message-snippet.h"
#include "mailbox-private.h"

static struct mail *test_mail_create(const char *input)
{
	struct mail_storage *storage;
	struct mailbox *box;
	struct mailbox_transaction_context *trans;
	struct mail *mail;
	pool_t pool;
	const char *err = NULL;

	pool = pool_alloconly_create("test mail", 1024);
	storage = p_new(pool, struct mail_storage, 1);
	box = p_new(pool, struct mailbox, 1);
	trans = p_new(pool, struct mailbox_transaction_context, 1);

	storage->set = mail_storage_settings_init(pool);

	box->storage = storage;
	box->pool = pool;
	trans->box = box;
	box->user = mail_user_alloc(NULL, storage->set, &err);

	mail = mail_alloc(trans, 0, NULL);
	mail_set_seq(mail, 1);

	mail->box->input = i_stream_create_from_data(input, strlen(input));

	return mail;
}

static const struct {
	const char *input;
	unsigned int max_snippet_chars;
	const char *output;
} tests[] = {
	{ "Content-Type: text/plain\n"
	  "\n"
	  "1234567890 234567890",
	  12,
	  "1234567890 2" },
	{ "Content-Type: text/plain\n"
	  "\n"
	  "line1\n>quote2\nline2\n",
	  100,
	  "line1 line2" },
	{ "Content-Type: text/plain\n"
	  "\n"
	  "line1\n>quote2\n> quote3\n > line4\n\n  \t\t  \nline5\n  \t ",
	  100,
	  "line1 > line4 line5" },
	{ "Content-Type: text/plain; charset=utf-8\n"
	  "\n"
	  "hyv\xC3\xA4\xC3\xA4 p\xC3\xA4iv\xC3\xA4\xC3\xA4",
	  11,
	  "hyv\xC3\xA4\xC3\xA4 p\xC3\xA4iv\xC3\xA4" },
	{ "Content-Type: text/plain; charset=utf-8\n"
	  "Content-Transfer-Encoding: quoted-printable\n"
	  "\n"
	  "hyv=C3=A4=C3=A4 p=C3=A4iv=C3=A4=C3=A4",
	  11,
	  "hyv\xC3\xA4\xC3\xA4 p\xC3\xA4iv\xC3\xA4" },

	{ "Content-Transfer-Encoding: quoted-printable\n"
	  "Content-Type: text/html;\n"
	  "      charset=utf-8\n"
	  "\n"
	  "<html><head><meta http-equiv=3D\"Content-Type\" content=3D\"text/html =\n"
	  "charset=3Dutf-8\"></head><body style=3D\"word-wrap: break-word; =\n"
	  "-webkit-nbsp-mode: space; -webkit-line-break: after-white-space;\" =\n"
	  "class=3D\"\">Hi,<div class=3D\"\"><br class=3D\"\"></div><div class=3D\"\">How =\n"
	  "is it going? <blockquote>quoted text is ignored</blockquote>\n"
	  "&gt; -foo\n"
	  "</div><br =class=3D\"\"></body></html>=\n",
	  100,
	  "Hi, How is it going?" },

	{ "Content-Transfer-Encoding: quoted-printable\n"
	  "Content-Type: application/xhtml+xml;\n"
	  "      charset=utf-8\n"
	  "\n"
	  "<html><head><meta http-equiv=3D\"Content-Type\" content=3D\"text/html =\n"
	  "charset=3Dutf-8\"></head><body style=3D\"word-wrap: break-word; =\n"
	  "-webkit-nbsp-mode: space; -webkit-line-break: after-white-space;\" =\n"
	  "class=3D\"\">Hi,<div class=3D\"\"><br class=3D\"\"></div><div class=3D\"\">How =\n"
	  "is it going? <blockquote>quoted text is ignored</blockquote>\n"
	  "&gt; -foo\n"
	  "</div><br =class=3D\"\"></body></html>=\n",
	  100,
	  "Hi, How is it going?" },
	{ "Content-Type: text/plain\n"
	  "\n"
	  ">quote1\n>quote2\n",
	  100,
	  ">quote1 quote2" },
	{ "Content-Type: text/plain\n"
	  "\n"
	  ">quote1\n>quote2\nbottom\nposter\n",
	  100,
	  "bottom poster" },
	{ "Content-Type: text/plain\n"
	  "\n"
	  "top\nposter\n>quote1\n>quote2\n",
	  100,
	  "top poster" },
	{ "Content-Type: text/plain\n"
	  "\n"
	  ">quoted long text",
	  7,
	  ">quoted" },
	{ "Content-Type: text/plain\n"
	  "\n"
	  ">quoted long text",
	  8,
	  ">quoted" },
	{ "Content-Type: text/plain\n"
	  "\n"
	  "whitespace and more",
	  10,
	  "whitespace" },
	{ "Content-Type: text/plain\n"
	  "\n"
	  "whitespace and more",
	  11,
	  "whitespace" },
	{ "Content-Type: text/plain; charset=utf-8\n"
	  "\n"
	  "Invalid utf8 \x80\xff\n",
	  100,
	  "Invalid utf8 "UNICODE_REPLACEMENT_CHAR_UTF8 },
	{ "Content-Type: text/plain; charset=utf-8\n"
	  "\n"
	  "Incomplete utf8 \xC3",
	  100,
	  "Incomplete utf8" },
	{ "Content-Transfer-Encoding: quoted-printable\n"
	  "Content-Type: text/html;\n"
	  "      charset=utf-8\n"
	  "\n"
	  "<html><head><meta http-equiv=3D\"Content-Type\" content=3D\"text/html =\n"
	  "charset=3Dutf-8\"></head><body style=3D\"word-wrap: break-word; =\n"
	  "-webkit-nbsp-mode: space; -webkit-line-break: after-white-space;\" =\n"
	  "class=3D\"\"><div><blockquote>quoted text is included</blockquote>\n"
	  "</div><br =class=3D\"\"></body></html>=\n",
	  100,
	  ">quoted text is included" },
	{ "Content-Type: text/plain; charset=utf-8\n"
	 "\n"
	 "I think\n",
	 100,
	 "I think"
	},
	{ "Content-Type: text/plain; charset=utf-8\n"
	 "\n"
	 "  Lorem Ipsum\n",
	 100,
	 "Lorem Ipsum"
	},
	{ "Content-Type: text/plain; charset=utf-8\n"
	 "\n"
	 " I think\n",
	 100,
	 "I think"
	},
	{ "Content-Type: text/plain; charset=utf-8\n"
	 "\n"
	 "   A cat\n",
	 100,
	 "A cat"
	},
	{ "Content-Type: text/plain; charset=utf-8\n"
	 "\n"
	 " \n",
	 100,
	 ""
	},
	{ "MIME-Version: 1.0\n"
	 "Content-Type: multipart/mixed; boundary=a\n"
	 "\n--a\n"
	 "Content-Transfer-Encoding: 7bit\n"
	 "Content-Type: text/html; charset=utf-8\n\n"
	 "<html><head></head><body><p>part one</p></body></head>\n"
	 "\n--a\n"
	 "Content-Transfer-Encoding: 7bit\n"
	 "Content-Type: text/html; charset=utf-8\n\n"
	 "<html><head></head><body><p>part two</p></body></head>\n"
	 "\n--a--\n",
	 100,
	 "part one"
	},
	{ "MIME-Version: 1.0\n"
	 "Content-Type: multipart/alternative; boundary=a\n"
	 "\n--a\n"
	 "Content-Transfer-Encoding: 7bit\n"
	 "Content-Type: text/html; charset=utf-8\n\n"
	 "<html><head></head><body><p>part one</p></body></head>\n"
	 "\n--a\n"
	 "Content-Transfer-Encoding: 7bit\n"
	 "Content-Type: text/plain; charset=utf-8\n\n"
	 "part two\n"
	 "\n--a--\n",
	 100,
	 "part one"
	},
	{ "MIME-Version: 1.0\n"
	  "Content-Type: multipart/mixed; boundary=a\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/html; charset=utf-8\n\n"
	  "<html><head></head><body><div><p></p><!-- comment --></body></head>\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/html; charset=utf-8\n\n"
	  "<html><head></head><body><p>part two</p></body></head>\n"
	  "\n--a--\n",
	  100,
	  "part two"
	},
	{ "MIME-Version: 1.0\n"
	  "Content-Type: multipart/alternative; boundary=a\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	 "Content-Type: text/plain; charset=utf-8\n\n"
	  "> original text\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/plain; charset=utf-8\n\n"
	  "part two\n"
	  "\n--a--\n",
	  100,
	  ">original text"
	},
	{ "MIME-Version: 1.0\n"
	  "Content-Type: multipart/alternative; boundary=a\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/plain; charset=utf-8\n\n"
	  "top poster\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/plain; charset=utf-8\n\n"
	  "> original text\n"
	  "\n--a--\n",
	  100,
	  "top poster"
	},
	{ "MIME-Version: 1.0\n"
	  "Content-Type: multipart/mixed; boundary=a\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/html; charset=utf-8\n\n"
	  "<html><head></head><body><div><p></p><!-- comment --></body></head>\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/html; charset=utf-8\n\n"
	  "<html><head></head><body><blockquote><!-- another --></blockquote>\n"
	 "</body></head>\n"
	  "\n--a--\n",
	  100,
	  ""
	},
	{ "MIME-Version: 1.0\n"
	  "Content-Type: multipart/mixed; boundary=a\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/html; charset=utf-8\n\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/html; charset=utf-8\n\n"
	  "</body></head>\n"
	  "\n--a--\n",
	  100,
	  ""
	},
	{ "MIME-Version: 1.0\n"
	  "Content-Type: multipart/mixed; boundary=a\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: base64\n"
	  "Content-Type: application/octet-stream\n\n"
	  "U2hvdWxkIG5vdCBiZSBpbiBzbmlwcGV0\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: 7bit\n"
	  "Content-Type: text/html; charset=utf-8\n\n"
	  "<html><head></head><body><p>Should be in snippet</p></body></html>\n"
	  "\n--a--\n",
	  100,
	  "Should be in snippet"
	},
	{ "MIME-Version: 1.0\n"
	  "Content-Type: multipart/mixed; boundary=a\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: base64\n"
	  "Content-Type: application/octet-stream\n\n"
	  "U2hvdWxkIG5vdCBiZSBpbiBzbmlwcGV0\n"
	  "\n--a\n"
	  "Content-Transfer-Encoding: base64\n"
	  "Content-Type: TeXT/html; charset=utf-8\n\n"
	  "PGh0bWw+PGhlYWQ+PC9oZWFkPjxib2R5PjxwPlNob3VsZCBiZSBpbiBzbmlwcGV0PC9wPjwvYm9k\n"
	  "eT48L2h0bWw+\n"
	  "\n--a--\n",
	  100,
	  "Should be in snippet"
	},
};

static void test_message_snippet(void)
{
	string_t *str = t_str_new(128);
	struct mail *mail;
	unsigned int i;

	test_begin("message snippet");
	for (i = 0; i < N_ELEMENTS(tests); i++) {
		str_truncate(str, 0);
		mail = test_mail_create(tests[i].input);
		test_assert_idx(message_snippet_generate(mail, tests[i].max_snippet_chars, str) == 0, i);
		test_assert_strcmp_idx(tests[i].output, str_c(str), i);
		mail_free(&mail);
	}
	test_end();
}

static void test_message_snippet_nuls(void)
{
	const char input_text[] = "\nfoo\0bar";
	string_t *str = t_str_new(128);
	struct mail *mail;

	test_begin("message snippet with NULs");

	mail = test_mail_create(input_text);
	test_assert(message_snippet_generate(mail, 5, str) == 0);
	test_assert_strcmp(str_c(str), "fooba");
	mail_free(&mail);
	test_end();
}

int main(void)
{
	static void (*const test_functions[])(void) = {
		test_message_snippet,
		test_message_snippet_nuls,
		NULL
	};
	return test_run(test_functions);
}
