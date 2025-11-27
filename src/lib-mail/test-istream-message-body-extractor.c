#include "lib.h"
#include "istream.h"
#include "istream-message-body-extractor.h"
#include "test-common.h"

static void test_istream_message_body_extractor_simple(void)
{
	test_begin("istream message body extractor simple");
	// TODO: implement this
	test_end();
}

int main(void)
{
	static void (*const test_functions[])(void) = {
		test_istream_message_body_extractor_simple,
		NULL
	};
	return test_run(test_functions);
}
