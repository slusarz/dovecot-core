#include "lib.h"
#include "str.h"
#include "mail-storage.h"
#include "mail-search.h"
#include "mail-search-mime.h"
#include "index-search-private.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Mock normalizer: converts to lowercase */
static int test_normalizer(const void *data, size_t size, string_t *dest)
{
	const char *str = data;
	for (size_t i = 0; i < size; i++)
		str_append_c(dest, i_tolower(str[i]));
	return 0;
}

/* Rename global functions to avoid linker conflicts with libdovecot-storage */
#define index_search_mime_arg_match test_index_search_mime_arg_match
#define index_search_mime_arg_deinit test_index_search_mime_arg_deinit

/* Include the source file directly to test static function */
#include "index-search-mime.c"

static void run_test(const char *name, const char *key, const char *value, int expected_ret)
{
	struct search_mimepart_context mpctx;
	struct index_search_context ictx;
	struct mail_search_mime_arg arg;
	int ret;

	/* Initialize structures */
	i_zero(&mpctx);
	i_zero(&ictx);
	i_zero(&arg);

	mpctx.index_ctx = &ictx;
	ictx.mail_ctx.normalizer = test_normalizer;
	mpctx.pool = pool_alloconly_create("test_pool", 1024);

	mpctx.buf = NULL; /* buf is allocated inside the function */

	arg.type = SEARCH_MIME_DESCRIPTION;
	arg.value.str = key;
	arg.context = NULL;

	printf("Test '%s': key='%s', value='%s' ... ", name, key ? key : "(null)", value ? value : "(null)");

	ret = search_arg_mime_substring_match(&mpctx, &arg, value);

	if (ret == expected_ret) {
		printf("PASS\n");
	} else {
		printf("FAIL (Expected %d, got %d)\n", expected_ret, ret);
	}

	/* Clean up */
	if (arg.context)
		i_free(arg.context);
	if (mpctx.buf)
		str_free(&mpctx.buf);
	pool_unref(&mpctx.pool);
}

int main(void)
{
	lib_init();

	/* 1. Exact match */
	run_test("Exact Match", "Hello", "Hello World", 1);

	/* 2. Case mismatch */
	run_test("Case Mismatch 1", "hello", "HELLO WORLD", 1);
	run_test("Case Mismatch 2", "HELLO", "hello world", 1);

	/* 3. Partial match */
	run_test("Partial Match", "World", "Hello World", 1);

	/* 4. No match */
	run_test("No Match", "Goodbye", "Hello World", 0);

	/* 5. NULL value */
	run_test("NULL Value", "Hello", NULL, 0);

	/* 6. Caching check */
	{
		struct search_mimepart_context mpctx;
		struct index_search_context ictx;
		struct mail_search_mime_arg arg;
		int ret;

		i_zero(&mpctx);
		i_zero(&ictx);
		i_zero(&arg);

		mpctx.index_ctx = &ictx;
		ictx.mail_ctx.normalizer = test_normalizer;
		mpctx.pool = pool_alloconly_create("test_pool_cache", 1024);
		mpctx.buf = NULL;

		arg.type = SEARCH_MIME_DESCRIPTION;
		arg.value.str = "CacheTest";
		arg.context = NULL;

		printf("Test 'Caching': First call ... ");
		ret = search_arg_mime_substring_match(&mpctx, &arg, "Contains CacheTest Here");
		if (ret == 1 && arg.context != NULL) printf("PASS (Context Cached)\n");
		else printf("FAIL (Ret=%d, Context=%p)\n", ret, arg.context);

		printf("Test 'Caching': Second call ... ");
		ret = search_arg_mime_substring_match(&mpctx, &arg, "Another Cachetest Instance");
		if (ret == 1) printf("PASS\n");
		else printf("FAIL\n");

		if (arg.context) i_free(arg.context);
		if (mpctx.buf) str_free(&mpctx.buf);
		pool_unref(&mpctx.pool);
	}

	lib_deinit();
	return 0;
}
