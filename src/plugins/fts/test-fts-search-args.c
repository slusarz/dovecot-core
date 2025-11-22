/* Copyright (c) 2015-2025 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "array.h"
#include "mail-search.h"
#include "fts-api-private.h"
#include "lang-tokenizer.h"
#include "lang-filter.h"
#include "lang-user.h"
#include "test-common.h"

/* Mocks */
struct lang_tokenizer {
	unsigned int token_idx;
};

static struct lang_tokenizer mock_tokenizer;
static const char *const *mock_tokens;

void lang_tokenizer_reset(struct lang_tokenizer *tokenizer)
{
	tokenizer->token_idx = 0;
}

int lang_tokenizer_next(struct lang_tokenizer *tokenizer,
			const unsigned char *data ATTR_UNUSED, size_t size ATTR_UNUSED,
			const char **token_r, const char **error_r ATTR_UNUSED)
{
	if (mock_tokens[tokenizer->token_idx] == NULL)
		return 0;

	*token_r = mock_tokens[tokenizer->token_idx++];
	return 1;
}

int lang_tokenizer_final(struct lang_tokenizer *tokenizer ATTR_UNUSED,
			 const char **token_r ATTR_UNUSED,
			 const char **error_r ATTR_UNUSED)
{
	return 0;
}

int lang_filter(struct lang_filter *filter ATTR_UNUSED,
		const char **token, const char **error_r ATTR_UNUSED)
{
	/* Simple filter: append "_stem" */
	*token = t_strconcat(*token, "_stem", NULL);
	return 1;
}

bool fts_header_has_language(const char *hdr_name ATTR_UNUSED)
{
	return FALSE;
}

const ARRAY_TYPE(language_user) *lang_user_get_data_languages(struct mail_user *user ATTR_UNUSED)
{
	return NULL;
}

const ARRAY_TYPE(language_user) *lang_user_get_all_languages(struct mail_user *user ATTR_UNUSED)
{
	return NULL;
}

/* Include the source file to test static functions */
#include "fts-search-args.c"

static void test_tokenize_word(void)
{
	struct language_user user_lang = { .search_tokenizer = &mock_tokenizer, .filter = (void*)1 };
	struct mail_search_arg *or_arg, *and_arg, *arg;
	const char *error;
	static const char *const tokens[] = { "foo", NULL };

	test_begin("tokenize word");

	mock_tokens = tokens;

	or_arg = p_new(default_pool, struct mail_search_arg, 1);
	or_arg->type = SEARCH_OR;

	/* orig_arg is just for copying properties */
	struct mail_search_arg orig_arg = { .type = SEARCH_TEXT };

	test_assert(fts_backend_dovecot_tokenize_lang(&user_lang, default_pool, or_arg,
						      &orig_arg, "foo", &error) == 0);

	/* Check structure */
	test_assert(or_arg->phrase_all == FALSE);
	and_arg = or_arg->value.subargs;
	test_assert(and_arg != NULL);
	test_assert(and_arg->type == SEARCH_SUB);

	/* Should have 1 token arg */
	arg = and_arg->value.subargs;
	test_assert(arg != NULL);
	test_assert(arg->next == NULL);

	/* Check flags on leaf */
	struct mail_search_arg *leaf = arg->value.subargs;
	while (leaf) {
		test_assert(leaf->phrase_token == FALSE);
		test_assert(leaf->phrase_first == FALSE);
		leaf = leaf->next;
	}
	test_end();
}

static void test_tokenize_phrase(void)
{
	struct language_user user_lang = { .search_tokenizer = &mock_tokenizer, .filter = (void*)1 };
	struct mail_search_arg *or_arg, *and_arg, *arg_foo, *arg_bar;
	const char *error;
	static const char *const tokens[] = { "foo", "bar", NULL };

	test_begin("tokenize phrase");

	mock_tokens = tokens;

	or_arg = p_new(default_pool, struct mail_search_arg, 1);
	or_arg->type = SEARCH_OR;

	struct mail_search_arg orig_arg = { .type = SEARCH_TEXT };

	test_assert(fts_backend_dovecot_tokenize_lang(&user_lang, default_pool, or_arg,
						      &orig_arg, "foo bar", &error) == 0);

	/* Check structure */
	test_assert(or_arg->phrase_all == TRUE);
	and_arg = or_arg->value.subargs;
	test_assert(and_arg != NULL);

	/* List is reversed: bar -> foo */
	arg_bar = and_arg->value.subargs;
	test_assert(arg_bar != NULL);
	arg_foo = arg_bar->next;
	test_assert(arg_foo != NULL);
	test_assert(arg_foo->next == NULL);

	/* Check foo (first token) */
	struct mail_search_arg *leaf = arg_foo->value.subargs;
	bool found_stem = FALSE;
	while (leaf) {
		test_assert(leaf->phrase_token == TRUE);
		test_assert(leaf->phrase_first == TRUE);
		/* verify string to be sure */
		/* leaf string could be "foo" or "foo_stem" */
		if (strcmp(leaf->value.str, "foo_stem") == 0) found_stem = TRUE;
		leaf = leaf->next;
	}
	test_assert(found_stem == TRUE);

	/* Check bar (second token) */
	leaf = arg_bar->value.subargs;
	found_stem = FALSE;
	while (leaf) {
		test_assert(leaf->phrase_token == TRUE);
		test_assert(leaf->phrase_first == FALSE);
		if (strcmp(leaf->value.str, "bar_stem") == 0) found_stem = TRUE;
		leaf = leaf->next;
	}
	test_assert(found_stem == TRUE);
	test_end();
}

int main(void)
{
	static void (*test_functions[])(void) = {
		test_tokenize_word,
		test_tokenize_phrase,
		NULL
	};
	return test_run(test_functions);
}
