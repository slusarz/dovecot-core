/* Copyright (c) 2024 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "str.h"
#include "istream.h"
#include "ioloop.h"
#include "http-url.h"
#include "http-client.h"
#include "json-parser.h"
#include "message-body-extractor.h"
#include "message-snippet-driver.h"

struct ollama_request_context {
	struct ioloop *ioloop;
	struct http_client_request *http_req;
	pool_t pool;
	string_t *snippet;
	const struct message_snippet_settings *set;
	struct istream *input;

	const char *error;
	bool finished;
};

static void
ollama_http_response_callback(const struct http_response *response,
			      struct ollama_request_context *rctx)
{
	const char *error;

	if (response->status != 200) {
		error = t_strdup_printf("Request failed: %s", response->reason);
		if (response->payload != NULL) {
			error = t_strdup_printf("%s: %s", error,
				i_stream_read_next_line(response->payload));
		}
		rctx->error = p_strdup(rctx->pool, error);
	} else if (response->payload == NULL) {
		rctx->error = p_strdup(rctx->pool, "Request failed: No payload");
	} else {
		struct json_parser *parser = json_parser_init(response->payload);
		const char *key, *value;
		enum json_type type;

		if (json_parse_next(parser, &type, &key) > 0 &&
		    type == JSON_TYPE_OBJECT) {
			while(json_parse_object_next(parser, &key, &type, &value) > 0) {
				if (strcmp(key, "response") == 0 && type == JSON_TYPE_STRING)
					str_append(rctx->snippet, value);
			}
		}
		if (json_parser_deinit(&parser, &error) < 0) {
			rctx->error = p_strdup(rctx->pool,
				t_strdup_printf("Failed to parse JSON response: %s", error));
		}
	}

	rctx->finished = TRUE;
	io_loop_stop(rctx->ioloop);
}

static int
message_snippet_driver_ollama_generate(const struct message_snippet_settings *set,
				       struct istream *input, string_t *snippet)
{
	struct ollama_request_context *rctx;
	struct http_client_context *http_ctx;
	struct http_client *http_client;
	struct http_url *url;
	const char *error;
	pool_t pool;
	string_t *body;
	struct istream *payload;
	int ret = 0;

	if (set->ollama_url == NULL || *set->ollama_url == '\0') {
		i_error("message-snippet: ollama driver enabled, but no url configured");
		return message_snippet_driver_internal.v.generate(set, input, snippet);
	}

	if (http_url_parse(set->ollama_url, NULL, 0, pool_datastack_create(),
			   &url, &error) < 0) {
		i_error("message-snippet: ollama_url is invalid: %s", error);
		return message_snippet_driver_internal.v.generate(set, input, snippet);
	}

	pool = pool_alloconly_create("ollama snippet request", 2048);
	body = str_new(pool, set->ollama_max_input_size);
	if (message_body_extractor_get_text(pool, input,
					    set->ollama_max_input_size,
					    body) < 0) {
		i_error("message-snippet: ollama driver failed to read body: %s",
			i_stream_get_error(input));
		pool_unref(&pool);
		return -1;
	}
	if (str_len(body) == 0) {
		/* no content, don't bother asking ollama */
		pool_unref(&pool);
		return 0;
	}

	rctx = p_new(pool, struct ollama_request_context, 1);
	rctx->pool = pool;
	rctx->set = set;
	rctx->input = input;
	rctx->snippet = snippet;
	rctx->ioloop = io_loop_create();

	http_ctx = http_client_context_create();
	struct http_client_settings http_set = {
		.dns_client = NULL,
		.ssl_client_context = NULL,
		.max_idle_time_msecs = 10000,
		.max_parallel_connections = 1,
		.max_pipelined_requests = 1,
		.request_timeout_msecs = set->ollama_timeout * 1000,
		.max_redirects = 0,
		.max_attempts = 1,
	};
	http_client = http_client_init(http_ctx, &http_set);

	string_t *json_payload = t_str_new(1024);
	struct json_generator *json_gen = json_generator_init(json_payload, FALSE);
	json_generator_obj_open(json_gen);
	json_generator_field(json_gen, "model");
	json_generator_str(json_gen, set->ollama_model);
	json_generator_field(json_gen, "prompt");
	json_generator_str(json_gen, p_strdup_printf(pool, "%s\n\n%s\n\nMax length: %u characters.",
						     set->ollama_prompt, str_c(body),
						     set->max_snippet_chars));
	json_generator_field(json_gen, "stream");
	json_generator_bool(json_gen, FALSE);
	json_generator_obj_close(json_gen);

	payload = i_stream_create_from_data(str_data(json_payload), str_len(json_payload));
	const char *path = url->path == NULL ? "/" : url->path;
	rctx->http_req = http_client_request_create(http_client, "POST", url->host.name,
						    path, ollama_http_response_callback,
						    rctx);
	http_client_request_set_port(rctx->http_req, url->port);
	http_client_request_add_header(rctx->http_req, "Content-Type", "application/json");
	http_client_request_set_payload(rctx->http_req, payload, FALSE);
	i_stream_unref(&payload);

	http_client_request_submit(rctx->http_req);

	if (!rctx->finished)
		io_loop_run(rctx->ioloop);

	if (rctx->error != NULL) {
		i_error("message-snippet: ollama driver failed: %s", rctx->error);
		ret = message_snippet_driver_internal.v.generate(set, input, snippet);
	} else if (str_len(snippet) > set->max_snippet_chars) {
		str_truncate(snippet, set->max_snippet_chars);
	}

	http_client_deinit(&http_client);
	http_client_context_unref(&http_ctx);
	io_loop_destroy(&rctx->ioloop);
	pool_unref(&pool);
	return ret;
}

const struct message_snippet_driver message_snippet_driver_ollama = {
	.name = "ollama",
	.v = {
		.generate = message_snippet_driver_ollama_generate,
	}
};
