/* Copyright (c) 2019 Dovecot authors, see the included COPYING file */

#include "test-lib.h"
#include "test-common.h"
#include "str.h"
#include "http-client-private.h"

static void
test_http_client_request_callback(const struct http_response *response ATTR_UNUSED,
				  void *context ATTR_UNUSED)
{
}

static void test_http_client_request_headers(void)
{
	struct http_client_settings set;
	struct http_client *client;
	struct http_client_request *req;

	test_begin("http client request headers");
	http_client_settings_init(null_pool, &set);
	client = http_client_init(&set, NULL);
	req = http_client_request(client, "GET", "host", "target",
				  test_http_client_request_callback, NULL);

	test_assert(http_client_request_lookup_header(req, "qwe") == NULL);

	/* add the first */
	http_client_request_add_header(req, "qwe", "value1");
	test_assert_strcmp(http_client_request_lookup_header(req, "qwe"), "value1");
	test_assert_strcmp(str_c(req->headers), "qwe: value1\r\n");

	/* replace the first with the same length */
	http_client_request_add_header(req, "qwe", "234567");
	test_assert_strcmp(http_client_request_lookup_header(req, "qwe"), "234567");
	test_assert_strcmp(str_c(req->headers), "qwe: 234567\r\n");

	/* replace the first with smaller length */
	http_client_request_add_header(req, "qwe", "xyz");
	test_assert_strcmp(http_client_request_lookup_header(req, "qwe"), "xyz");
	test_assert_strcmp(str_c(req->headers), "qwe: xyz\r\n");

	/* replace the first with longer length */
	http_client_request_add_header(req, "qwe", "abcdefg");
	test_assert_strcmp(http_client_request_lookup_header(req, "qwe"), "abcdefg");
	test_assert_strcmp(str_c(req->headers), "qwe: abcdefg\r\n");

	/* add the second */
	http_client_request_add_header(req, "xyz", "1234");
	test_assert_strcmp(http_client_request_lookup_header(req, "qwe"), "abcdefg");
	test_assert_strcmp(http_client_request_lookup_header(req, "xyz"), "1234");
	test_assert_strcmp(str_c(req->headers), "qwe: abcdefg\r\nxyz: 1234\r\n");

	/* replace second */
	http_client_request_add_header(req, "xyz", "yuiop");
	test_assert_strcmp(http_client_request_lookup_header(req, "qwe"), "abcdefg");
	test_assert_strcmp(http_client_request_lookup_header(req, "xyz"), "yuiop");
	test_assert_strcmp(str_c(req->headers), "qwe: abcdefg\r\nxyz: yuiop\r\n");

	/* replace the first again */
	http_client_request_add_header(req, "qwe", "1234");
	test_assert_strcmp(http_client_request_lookup_header(req, "qwe"), "1234");
	test_assert_strcmp(http_client_request_lookup_header(req, "xyz"), "yuiop");
	test_assert_strcmp(str_c(req->headers), "qwe: 1234\r\nxyz: yuiop\r\n");

	/* remove the headers */
	http_client_request_remove_header(req, "qwe");
	test_assert(http_client_request_lookup_header(req, "qwe") == NULL);
	test_assert_strcmp(http_client_request_lookup_header(req, "xyz"), "yuiop");
	test_assert_strcmp(str_c(req->headers), "xyz: yuiop\r\n");

	http_client_request_remove_header(req, "xyz");
	test_assert(http_client_request_lookup_header(req, "qwe") == NULL);
	test_assert(http_client_request_lookup_header(req, "xyz") == NULL);
	test_assert_strcmp(str_c(req->headers), "");

	/* test _add_missing_header() */
	http_client_request_add_missing_header(req, "foo", "bar");
	test_assert_strcmp(str_c(req->headers), "foo: bar\r\n");
	http_client_request_add_missing_header(req, "foo", "123");
	test_assert_strcmp(str_c(req->headers), "foo: bar\r\n");

	http_client_request_abort(&req);
	http_client_deinit(&client);
	test_end();
}

static void test_http_client_request_stats_unsent(void)
{
	struct http_client_settings set;
	struct http_client *client;
	struct http_client_request *req;
	struct http_client_request_stats stats;

	test_begin("http client request stats unsent");

	i_zero(&set);
	set.max_pipelined_requests = 1;
	set.max_parallel_connections = 1;
	set.connect_backoff_time_msecs = 100;
	set.connect_backoff_max_time_msecs = 1000;
	set.request_timeout_msecs = 1000;

	client = http_client_init(&set, NULL);
	req = http_client_request(client, "GET", "localhost", "/",
				  test_http_client_request_callback, NULL);

	/* submit request, but don't run ioloop so it's not sent */
	http_client_request_submit(req);

	/* let some time pass */
	i_sleep_msecs(10);

	http_client_request_get_stats(req, &stats);
	test_assert(stats.lock_msecs < 1000);
	test_assert(stats.other_ioloop_msecs < 1000);

	http_client_request_abort(&req);
	http_client_deinit(&client);
	test_end();
}

int main(void)
{
	static void (*const test_functions[])(void) = {
		test_http_client_request_headers,
		test_http_client_request_stats_unsent,
		NULL
	};
	return test_run(test_functions);
}
