/* Copyright (c) 2025 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "otp.h"
#include "sasl-server-protected.h"
#include "test-common.h"
#include <unistd.h>
#include <time.h>
#include <stdio.h>

/* Mock function prototypes */
void test_sasl_server_request_failure(struct sasl_server_mech_request *mreq);
void test_sasl_server_request_password_mismatch(struct sasl_server_mech_request *mreq);
void test_sasl_server_request_set_credentials(
	struct sasl_server_mech_request *mreq,
	const char *scheme, const char *data,
	sasl_server_mech_passdb_callback_t *callback);

/* Mocked dependencies - rename before including */
#define sasl_server_request_failure test_sasl_server_request_failure
#define sasl_server_request_password_mismatch test_sasl_server_request_password_mismatch
#define sasl_server_request_set_credentials test_sasl_server_request_set_credentials
#define sasl_server_mech_register_otp test_sasl_server_mech_register_otp

/* Include the source file to access static functions */
#include "sasl-server-mech-otp.c"

/* State tracking */
static bool g_failed = FALSE;
static bool g_success = FALSE;
static bool g_password_mismatch = FALSE;

void test_sasl_server_request_failure(struct sasl_server_mech_request *mreq ATTR_UNUSED)
{
	g_failed = TRUE;
}

void test_sasl_server_request_password_mismatch(struct sasl_server_mech_request *mreq ATTR_UNUSED)
{
	g_password_mismatch = TRUE;
}

void test_sasl_server_request_set_credentials(
	struct sasl_server_mech_request *mreq ATTR_UNUSED,
	const char *scheme ATTR_UNUSED, const char *data ATTR_UNUSED,
	sasl_server_mech_passdb_callback_t *callback ATTR_UNUSED)
{
	g_success = TRUE;
}

static void setup_otp_request(struct otp_auth_request *req, unsigned int algo, const unsigned char *target_hash)
{
	static struct sasl_server_mech dummy_mech;

	memset(req, 0, sizeof(*req));
	req->auth_request.mech = &dummy_mech;
	req->lock = FALSE; /* Prevent otp_unlock from trying to access hash table */

	req->state.algo = algo;
	req->state.seq = 100;
	memcpy(req->state.hash, target_hash, OTP_HASH_SIZE);
	/* seed doesn't matter for verify */
}

static void run_timing_test(unsigned long iterations)
{
	struct otp_auth_request req;
	unsigned char secret_hash[OTP_HASH_SIZE];
	unsigned char input_hash[OTP_HASH_SIZE];
	char input_hex[OTP_HASH_SIZE * 2 + 1];
	unsigned int i;

	/* Setup a target hash (secret) */
	memset(secret_hash, 0xAA, sizeof(secret_hash));

	/* Setup an input that produces a hash that differs from secret */
	/* We need to reverse the hash chain or just pick a random input and compute its hash */
	memset(input_hash, 0xBB, sizeof(input_hash));

	/* otp_next_hash(algo, input_hash, output_hash) */
	/* mech_otp_verify computes next_hash(input) and compares with secret */

	/* To test timing, we want to control the COMPARISON */
	/* mech_otp_verify:
	     otp_parse_response(input_hex, parsed_hash)
	     otp_next_hash(algo, parsed_hash, cur_hash)
	     memcmp(cur_hash, state->hash)
	*/

	/* So if we want memcmp(A, B), we set state->hash = B.
	   And we need to find input_hex such that otp_next_hash(algo, parsed_hash) == A.
	*/

	unsigned char A[OTP_HASH_SIZE];
	unsigned char B[OTP_HASH_SIZE];

	/* Case 1: Match (A == B) */
	memset(A, 0xCC, sizeof(A));
	memset(B, 0xCC, sizeof(B));

	/* We need input_hex such that next_hash(parse(input_hex)) == A */
	/* This is hard because next_hash is one-way.
	   However, for the purpose of this test, we can just cheat and set state->hash
	   to whatever next_hash produces!
	*/

	unsigned char parsed_hash[OTP_HASH_SIZE];
	memset(parsed_hash, 0x12, sizeof(parsed_hash));

	/* Compute A = next_hash(parsed_hash) */
	otp_next_hash(OTP_HASH_MD5, parsed_hash, A);

	/* Convert parsed_hash to hex string for input */
	const char *hex_chars = "0123456789abcdef";
	for (i = 0; i < OTP_HASH_SIZE; i++) {
		input_hex[i*2] = hex_chars[parsed_hash[i] >> 4];
		input_hex[i*2+1] = hex_chars[parsed_hash[i] & 0x0F];
	}
	input_hex[OTP_HASH_SIZE*2] = '\0';

	/* Run Match Case */
	setup_otp_request(&req, OTP_HASH_MD5, A);

	/* Reset stats */
	g_success = FALSE;
	g_password_mismatch = FALSE;
	g_failed = FALSE;

	/* Run once to verify correctness */
	mech_otp_verify(&req, input_hex, TRUE);

	if (!g_success) {
		fprintf(stderr, "Error: Match case failed (success=%d, mismatch=%d, failed=%d)\n",
			g_success, g_password_mismatch, g_failed);
		exit(1);
	}

	printf("Verified match case works.\n");

	/* Run Mismatch Case (first byte different) */
	B[0] = A[0] ^ 0xFF;
	memcpy(B+1, A+1, OTP_HASH_SIZE-1);

	setup_otp_request(&req, OTP_HASH_MD5, B);
	g_success = FALSE;
	g_password_mismatch = FALSE;
	g_failed = FALSE;

	mech_otp_verify(&req, input_hex, TRUE);

	if (!g_password_mismatch) {
		fprintf(stderr, "Error: Mismatch case failed to report mismatch (success=%d, mismatch=%d)\n",
			g_success, g_password_mismatch);
		exit(1);
	}

	printf("Verified mismatch case works.\n");

	/* Now timing loop */
	if (iterations > 0) {
		printf("Running %lu iterations for timing analysis...\n", iterations);

		/* We can alternate or just run one case.
		   For timing attack, usually we measure the time of mismatch cases.
		   Here we just exercise the code path.
		*/

		clock_t start = clock();
		for (unsigned long j = 0; j < iterations; j++) {
			/* Reset request state hash to force mismatch at different bytes if we wanted,
			   but for now just constant mismatch */
			setup_otp_request(&req, OTP_HASH_MD5, B);
			mech_otp_verify(&req, input_hex, TRUE);
		}
		clock_t end = clock();
		double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
		printf("Time taken: %f seconds\n", cpu_time_used);
	}
}

int main(int argc, char *argv[])
{
	unsigned long iterations = 1000000;

	if (argc > 1) {
		iterations = strtoul(argv[1], NULL, 10);
	}

	lib_init();
	test_init();

	run_timing_test(iterations);

	lib_deinit();

	return 0;
}
