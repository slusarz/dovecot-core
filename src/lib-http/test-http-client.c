/* Copyright (c) 2013-2018 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "safe-memset.h"
#include "ioloop.h"
#include "istream.h"
#include "write-full.h"
#include "http-url.h"
#include "http-client.h"
#include "dns-lookup.h"
#include "iostream-ssl.h"
#include "iostream-openssl.h"
#include "settings.h"
#include "lib-event-private.h"

#include <fcntl.h>
#include <unistd.h>

static struct settings_root *set_root;

struct http_test_request {
	struct io *io;
	struct istream *payload;
	bool write_output;
};

static struct ioloop *ioloop;

static void payload_input(struct http_test_request *req)
{
	const unsigned char *data;
	size_t size;
	int ret;

	/* read payload */
	while ((ret=i_stream_read_more(req->payload, &data, &size)) > 0) {
		i_info("DEBUG: got data (size=%d)", (int)size);
		if (req->write_output)
			if (write_full(1, data, size) < 0)
				i_error("REQUEST PAYLOAD WRITE ERROR: %m");
		i_stream_skip(req->payload, size);
	}

	if (ret == 0) {
		i_info("DEBUG: REQUEST: NEED MORE DATA");
		/* we will be called again for this request */
	} else {
		if (req->payload->stream_errno != 0) {
			i_error("REQUEST PAYLOAD READ ERROR: %s",
				i_stream_get_error(req->payload));
		} else
			i_info("DEBUG: REQUEST: Finished");
		io_remove(&req->io);
		i_stream_unref(&req->payload);
		i_free(req);
	}
}

static void
got_request_response(const struct http_response *response,
		     struct http_test_request *req)
{
	io_loop_stop(ioloop);

	if (response->status / 100 != 2) {
		i_error("HTTP Request failed: %s", response->reason);
		i_free(req);
		/* payload (if any) is skipped implicitly */
		return;
	}

	i_info("DEBUG: REQUEST SUCCEEDED: %s", response->reason);

	if (response->payload == NULL) {
		i_free(req);
		return;
	}

	i_info("DEBUG: REQUEST: Got payload");
	i_stream_ref(response->payload);
	req->payload = response->payload;
	req->io = io_add_istream(response->payload, payload_input, req);
	payload_input(req);
}

static const char *test_query1 = "data=Frop&submit=Submit";
static const char *test_query2 = "data=This%20is%20a%20test&submit=Submit";
static const char *test_query3 = "foo=bar";

static void run_tests(struct http_client *http_client)
{
	struct http_client_request *http_req;
	struct http_test_request *test_req;
	struct istream *post_payload;

	// JigSAW is useful for testing: http://jigsaw.w3.org/HTTP/

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "pigeonhole.dovecot.org", "/",
		got_request_response, test_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "pigeonhole.dovecot.org", "/download.html",
		got_request_response, test_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "jigsaw.w3.org", "/HTTP/300/301.html",
		got_request_response, test_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "pigeonhole.dovecot.org", "/frop.html",
		got_request_response, test_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "jigsaw.w3.org", "/HTTP/300/307.html",
		got_request_response, test_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "pigeonhole.dovecot.org", "/documentation.html",
		got_request_response, test_req);
	http_client_request_set_urgent(http_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "jigsaw.w3.org", "/HTTP/300/302.html",
		got_request_response, test_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"POST", "test.dovecot.org", "/http/post/index.php",
		got_request_response, test_req);
	post_payload = i_stream_create_from_data
		((const unsigned char *)test_query1, strlen(test_query1));
	http_client_request_set_payload(http_req, post_payload, FALSE);
	i_stream_unref(&post_payload);
	http_client_request_add_header(http_req,
		"Content-Type", "application/x-www-form-urlencoded");
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"POST", "test.dovecot.org", "/http/post/index.php",
		got_request_response, test_req);
	post_payload = i_stream_create_from_data
		((const unsigned char *)test_query2, strlen(test_query2));
	http_client_request_set_payload(http_req, post_payload, TRUE);
	i_stream_unref(&post_payload);
	http_client_request_add_header(http_req,
		"Content-Type", "application/x-www-form-urlencoded");
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "pigeonhole.dovecot.org", "/",
		got_request_response, test_req);
	http_client_request_set_port(http_req, 81);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"HEAD", "pigeonhole.dovecot.org", "/download.html",
		got_request_response, test_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "pigeonhole.dovecot.org", "/",
		got_request_response, test_req);
	http_client_request_set_ssl(http_req, TRUE);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "pigeonhole.dovecot.org", "/download.html",
		got_request_response, test_req);
	http_client_request_set_ssl(http_req, TRUE);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "pigeonhole.dovecot.org", "/documentation.html",
		got_request_response, test_req);
	http_client_request_set_ssl(http_req, TRUE);
	http_client_request_submit(http_req);
	http_client_request_abort(&http_req);
	i_free(test_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"POST", "posttestserver.com", "/post.php",
		got_request_response, test_req);
	post_payload = i_stream_create_from_data
		((const unsigned char *)test_query1, strlen(test_query1));
	http_client_request_set_payload(http_req, post_payload, TRUE);
	i_stream_unref(&post_payload);
	http_client_request_set_ssl(http_req, TRUE);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"POST", "posttestserver.com", "/post.php",
		got_request_response, test_req);
	post_payload = i_stream_create_from_data
		((const unsigned char *)test_query1, strlen(test_query1));
	http_client_request_set_payload(http_req, post_payload, TRUE);
	i_stream_unref(&post_payload);
	http_client_request_set_ssl(http_req, TRUE);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"POST", "posttestserver.com", "/post.php",
		got_request_response, test_req);
	post_payload = i_stream_create_from_data
		((const unsigned char *)test_query1, strlen(test_query1));
	http_client_request_set_payload(http_req, post_payload, TRUE);
	i_stream_unref(&post_payload);
	http_client_request_set_ssl(http_req, TRUE);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "doc.dovecot.org", "/configuration_manual/sieve/",
		got_request_response, test_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "jigsaw.w3.org", "/HTTP/ChunkedScript",
		got_request_response, test_req);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"POST", "jigsaw.w3.org", "/HTTP/300/Go_307",
		got_request_response, test_req);
	post_payload = i_stream_create_from_data
		((const unsigned char *)test_query3, strlen(test_query3));
	http_client_request_set_payload(http_req, post_payload, FALSE);
	i_stream_unref(&post_payload);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"POST", "jigsaw.w3.org", "/HTTP/300/Go_307",
		got_request_response, test_req);
	post_payload = i_stream_create_from_data
		((const unsigned char *)test_query3, strlen(test_query3));
	http_client_request_set_payload(http_req, post_payload, FALSE);
	i_stream_unref(&post_payload);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"POST", "jigsaw.w3.org", "/HTTP/300/Go_307",
		got_request_response, test_req);
	post_payload = i_stream_create_from_data
		((const unsigned char *)test_query3, strlen(test_query3));
	http_client_request_set_payload(http_req, post_payload, FALSE);
	i_stream_unref(&post_payload);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"GET", "jigsaw.w3.org", "/HTTP/Basic/",
		got_request_response, test_req);
	http_client_request_set_auth_simple
		(http_req, "guest", "guest");
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request(http_client,
		"PUT", "test.dovecot.org", "/http/put/put.php",
		got_request_response, test_req);
	post_payload = i_stream_create_file("Makefile.am", 10);
	http_client_request_set_payload(http_req, post_payload, TRUE);
	i_stream_unref(&post_payload);
	http_client_request_submit(http_req);

	test_req = i_new(struct http_test_request, 1);
	http_req = http_client_request_url_str(http_client,
		"GET", "https://invalid.dovecot.org/",
		got_request_response, test_req);
	http_client_request_submit(http_req);
}

static void
test_http_request_init(struct http_client *http_client,
		       const char *method, const char *url_str,
		       struct http_client_request **http_req_r,
		       struct http_test_request **test_req_r)
{
	struct http_client_request *http_req;
	struct http_test_request *test_req;
	struct http_url *url;
	const char *error;

	if (http_url_parse(url_str, NULL, 0, pool_datastack_create(),
			   &url, &error) < 0)
		i_fatal("Invalid URL %s: %s", url_str, error);

	test_req = i_new(struct http_test_request, 1);
	test_req->write_output = TRUE;
	http_req = http_client_request(http_client,
		method, url->host.name,
		t_strconcat("/", url->path, url->enc_query, NULL),
		got_request_response, test_req);
	if (url->port != 0)
		http_client_request_set_port(http_req, url->port);
	if (url->have_ssl)
		http_client_request_set_ssl(http_req, TRUE);

	*http_req_r = http_req;
	*test_req_r = test_req;
}

static void run_http_get(struct http_client *http_client, const char *url_str)
{
	struct http_client_request *http_req;
	struct http_test_request *test_req;

	test_http_request_init(http_client, "GET", url_str, &http_req, &test_req);
	http_client_request_submit(http_req);
}

static void run_http_post(struct http_client *http_client, const char *url_str,
			  const char *path)
{
	struct http_client_request *http_req;
	struct http_test_request *test_req;
	struct istream *input;

	test_http_request_init(http_client, "POST", url_str, &http_req, &test_req);
	input = i_stream_create_file(path, IO_BLOCK_SIZE);
	http_client_request_set_payload(http_req, input, FALSE);
	i_stream_unref(&input);
	http_client_request_submit(http_req);
}

struct test_stats_context {
	struct http_client_request *req;
	bool *finished;
};

static void
test_stats_on_failure_callback(const struct http_response *response,
			       struct test_stats_context *ctx)
{
	struct http_client_request_stats stats;

	i_info("test_stats_on_failure_callback: Request failed as expected: %u %s",
	       response->status, response->reason);

	http_client_request_get_stats(ctx->req, &stats);

	i_info("DNS failure stats: total=%u, 1st_sent=%u, last_sent=%u, "
	       "http_ioloop=%u, other_ioloop=%u, lock=%u, "
	       "attempts=%u, send_attempts=%u",
	       stats.total_msecs, stats.first_sent_msecs, stats.last_sent_msecs,
	       stats.http_ioloop_msecs, stats.other_ioloop_msecs,
	       stats.lock_msecs, stats.attempts, stats.send_attempts);

	/* The bug was that lock_msecs was huge. Let's check it's small.
	   It can be non-zero if other tests are running and using locks.
	   But it shouldn't be thousands of seconds. Let's say < 5 seconds. */
	if (stats.lock_msecs > 5000) {
		i_fatal("stats.lock_msecs is too high: %u", stats.lock_msecs);
	}
	if (stats.other_ioloop_msecs > 5000) {
		i_fatal("stats.other_ioloop_msecs is too high: %u",
			stats.other_ioloop_msecs);
	}
	if (stats.total_msecs > 35000) {
		/* dns timeout is 30s, so this shouldn't be much more */
		i_fatal("stats.total_msecs is too high: %u", stats.total_msecs);
	}

	*ctx->finished = TRUE;
	i_free(ctx);
	if (ioloop != NULL)
		io_loop_stop(ioloop);
}

static void
run_test_request_stats_on_failure(struct http_client *http_client)
{
	struct http_client_request *http_req;
	struct test_stats_context *ctx;
	bool finished = FALSE;

	i_info("--- test: request stats on failure ---");

	ctx = i_new(struct test_stats_context, 1);
	ctx->finished = &finished;

	http_req = http_client_request(http_client,
		"GET", "this-host-does-not-exist.dovecot.example.com", "/",
		test_stats_on_failure_callback, ctx);
	ctx->req = http_req;
	http_client_request_submit(http_req);

	while (!finished)
		io_loop_run(ioloop);
	i_info("--- test: request stats on failure finished ---");
}

static bool
test_event_callback(struct event *event,
		    enum event_callback_type type,
		    struct failure_context *ctx ATTR_UNUSED,
		    const char *fmt ATTR_UNUSED,
		    va_list args ATTR_UNUSED)
{
	if (type == EVENT_CALLBACK_TYPE_CREATE && event->parent == NULL)
		event_set_ptr(event, SETTINGS_EVENT_ROOT, set_root);
	return TRUE;
}

int main(int argc, char *argv[])
{
	struct dns_client *dns_client;
	struct http_client_settings http_set;
	struct http_client_context *http_cctx;
	struct http_client *http_client1, *http_client2, *http_client3, *http_client4;
	struct ssl_iostream_settings ssl_set;
	struct stat st;
	const char *error;
	const char *dns_client_socket_path = PKG_RUNDIR"/dns-client";

	lib_init();
	set_root = settings_root_init();
	settings_root_override(set_root, "dns_client_timeout", "30s", SETTINGS_OVERRIDE_TYPE_CODE);
	event_register_callback(test_event_callback);

	ssl_iostream_openssl_init();
	ioloop = io_loop_create();
	io_loop_set_running(ioloop);

	/* kludge: use safe_memset() here since otherwise it's not included in
	   the binary in all systems (but is in others! so linking
	   safe-memset.lo directly causes them to fail.) If safe_memset() isn't
	   included, libssl-iostream plugin loading fails. */

	struct dns_client_parameters params = {
		.idle_timeout_msecs = UINT_MAX,
	};
	struct event *event = event_create(NULL);
	/* check if there is a DNS client */
	if (access(dns_client_socket_path, R_OK|W_OK) == 0) {
		if (dns_client_init(&params, event, &dns_client, &error) < 0)
			i_fatal("%s", error);

		if (dns_client_connect(dns_client, &error) < 0)
			i_fatal("Couldn't initialize DNS client: %s", error);
	} else {
		dns_client = NULL;
		settings_root_override(set_root, "dns_client_socket_path", "", SETTINGS_OVERRIDE_TYPE_CODE);
	}
	i_zero(&ssl_set);
	ssl_set.pool = null_pool;
	ssl_set.allow_invalid_cert = TRUE;
	if (stat("/etc/ssl/certs", &st) == 0 && S_ISDIR(st.st_mode))
		ssl_set.ca_dir = "/etc/ssl/certs"; /* debian */
	if (stat("/etc/ssl/certs", &st) == 0 && S_ISREG(st.st_mode)) {
		/* redhat */
		const char *ca_value;
		if (settings_parse_read_file("/etc/pki/tls/cert.pem",
					     "/etc/pki/tls/cert.pem",
					     unsafe_data_stack_pool, NULL,
					     "", &ca_value, &error) < 0)
			i_fatal("%s", error);
		settings_file_get(ca_value, unsafe_data_stack_pool,
				  &ssl_set.ca);
	}

	http_client_settings_init(null_pool, &http_set);
	http_set.max_idle_time_msecs = 5*1000;
	http_set.max_parallel_connections = 4;
	http_set.max_pipelined_requests = 4;
	http_set.request_max_redirects = 2;
	http_set.request_timeout_msecs = 10*1000;
	http_set.request_max_attempts = 1;
	http_set.rawlog_dir = "/tmp/http-test";

	http_cctx = http_client_context_create();

	event_set_forced_debug(event, TRUE);
	http_client1 = http_client_init_shared(http_cctx, &http_set, event);
	http_client2 = http_client_init_shared(http_cctx, &http_set, event);
	http_client3 = http_client_init_shared(http_cctx, &http_set, event);
	http_client4 = http_client_init_shared(http_cctx, &http_set, event);

	http_client_set_ssl_settings(http_client1, &ssl_set);
	http_client_set_ssl_settings(http_client2, &ssl_set);
	http_client_set_ssl_settings(http_client3, &ssl_set);
	http_client_set_ssl_settings(http_client4, &ssl_set);
	http_client_set_dns_client(http_client1, dns_client);
	http_client_set_dns_client(http_client2, dns_client);
	http_client_set_dns_client(http_client3, dns_client);
	http_client_set_dns_client(http_client4, dns_client);

	switch (argc) {
	case 1:
		run_test_request_stats_on_failure(http_client1);
		run_tests(http_client1);
		run_tests(http_client2);
		run_tests(http_client3);
		run_tests(http_client4);
		break;
	case 2:
		run_http_get(http_client1, argv[1]);
		run_http_get(http_client2, argv[1]);
		run_http_get(http_client3, argv[1]);
		run_http_get(http_client4, argv[1]);
		break;
	case 3:
		run_http_post(http_client1, argv[1], argv[2]);
		run_http_post(http_client2, argv[1], argv[2]);
		run_http_post(http_client3, argv[1], argv[2]);
		run_http_post(http_client4, argv[1], argv[2]);
		break;
	default:
		i_fatal("Too many parameters");
	}

	for (;;) {
		bool pending = FALSE;

		if (http_client_get_pending_request_count(http_client1) > 0) {
			i_debug("Requests still pending in client 1");
			pending = TRUE;
		} else if (http_client_get_pending_request_count(http_client2) > 0) {
			i_debug("Requests still pending in client 2");
			pending = TRUE;
		} else if (http_client_get_pending_request_count(http_client3) > 0) {
			i_debug("Requests still pending in client 3");
			pending = TRUE;
		} else if (http_client_get_pending_request_count(http_client4) > 0) {
			i_debug("Requests still pending in client 4");
			pending = TRUE;
		}
		if (!pending)
			break;
		io_loop_run(ioloop);
	}
	http_client_deinit(&http_client1);
	http_client_deinit(&http_client2);
	http_client_deinit(&http_client3);
	http_client_deinit(&http_client4);
	settings_root_deinit(&set_root);
	event_unref(&event);

	http_client_context_unref(&http_cctx);

	if (dns_client != NULL)
		dns_client_deinit(&dns_client);

	io_loop_destroy(&ioloop);
	ssl_iostream_context_cache_free();
	ssl_iostream_openssl_deinit();
	event_unregister_callback(test_event_callback);
	lib_deinit();
}
