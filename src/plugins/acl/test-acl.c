/* Copyright (c) 2020 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "array.h"
#include "test-common.h"
#include "str.h"
#include "acl-global-file.h"
#include "acl-api-private.h"

#include <fcntl.h>
#include <unistd.h>

static void test_acl_rights_sort(void)
{
	struct acl_rights rights1 = {
		.rights = t_strsplit("a b a c d b", " "),
		.neg_rights = t_strsplit("e d c a a d b e", " "),
	};
	struct acl_rights rights2 = {
		.rights = t_strsplit("a c x", " "),
		.neg_rights = t_strsplit("b c y", " "),
	};
	struct acl_object obj = {
		.rights_pool = pool_alloconly_create("acl rights", 256)
	};
	const struct acl_rights *rights;

	test_begin("acl_rights_sort");
	t_array_init(&obj.rights, 8);

	/* try with zero rights */
	acl_rights_sort(&obj);
	test_assert(array_count(&obj.rights) == 0);

	/* try with just one right */
	array_push_back(&obj.rights, &rights1);
	acl_rights_sort(&obj);
	test_assert(array_count(&obj.rights) == 1);
	rights = array_idx(&obj.rights, 0);
	test_assert(acl_rights_cmp(rights, &rights1) == 0);

	/* try with two rights that don't have equal ID */
	struct acl_rights rights1_id2 = rights1;
	rights1_id2.identifier = "id2";
	array_push_back(&obj.rights, &rights1_id2);
	acl_rights_sort(&obj);
	test_assert(array_count(&obj.rights) == 2);
	rights = array_idx(&obj.rights, 0);
	test_assert(acl_rights_cmp(&rights[0], &rights1) == 0);
	test_assert(acl_rights_cmp(&rights[1], &rights1_id2) == 0);

	/* try with 3 rights where first has equal ID */
	array_push_back(&obj.rights, &rights2);
	acl_rights_sort(&obj);
	test_assert(array_count(&obj.rights) == 2);
	rights = array_idx(&obj.rights, 0);
	test_assert_strcmp(t_strarray_join(rights[0].rights, " "), "a b c d x");
	test_assert_strcmp(t_strarray_join(rights[0].neg_rights, " "), "a b c d e y");

	pool_unref(&obj.rights_pool);
	test_end();
}

static void test_acl_parse_whitespace(void)
{
	const char *tmpfname = "tmp-test-acl-ws.tmp";
	const char *error;
	/* vpattern, id, rights */
	const char *line1 = "*\tuser=timo lr\n";
	const char *line2 = "\"*\" user=timo\t\tlr\n";
	struct acl_global_file *file;
	ARRAY_TYPE(acl_rights) rights;
	const struct acl_rights *right;
	pool_t pool;
	int fd;

	test_begin("acl parse whitespace");

	fd = open(tmpfname, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	if (fd == -1)
		i_fatal("Cannot create %s: %m", tmpfname);
	if (write(fd, line1, strlen(line1)) < 0)
		i_fatal("Cannot write to %s: %m", tmpfname);
	if (write(fd, line2, strlen(line2)) < 0)
		i_fatal("Cannot write to %s: %m", tmpfname);
	i_close_fd(&fd);

	file = acl_global_file_init(tmpfname, 0, NULL);
	pool = pool_alloconly_create("tmp", 1024);
	t_array_init(&rights, 4);

	/* test global acl file parsing */
	test_assert(acl_global_file_refresh(file) == 0);
	acl_global_file_get(file, "foo", pool, &rights);
	test_assert(array_count(&rights) == 2);

	right = array_idx(&rights, 0);
	test_assert_strcmp(acl_rights_get_id(right), "user=timo");
	test_assert_strcmp(t_strarray_join(right->rights, ","), "lookup,read");
	test_assert(right->neg_rights == NULL);

	right = array_idx(&rights, 1);
	test_assert_strcmp(acl_rights_get_id(right), "user=timo");
	test_assert_strcmp(t_strarray_join(right->rights, ","), "lookup,read");
	test_assert(right->neg_rights == NULL);

	/* test direct parsing of rights line */
	array_clear(&rights);
	test_assert(acl_rights_parse_line("user=timo lr", pool,
					  array_append_space(&rights),
					  &error) == 0);
	test_assert(acl_rights_parse_line("\tuser=timo\t lr", pool,
					  array_append_space(&rights),
					  &error) == 0);
	test_assert(array_count(&rights) == 2);
	right = array_idx(&rights, 0);
	test_assert_strcmp(acl_rights_get_id(right), "user=timo");
	test_assert_strcmp(t_strarray_join(right->rights, ","), "lookup,read");

	/* clean up */
	acl_global_file_deinit(&file);
	pool_unref(&pool);
	i_unlink(tmpfname);
	test_end();
}

int main(void)
{
	static void (*const test_functions[])(void) = {
		test_acl_rights_sort,
		test_acl_parse_whitespace,
		NULL
	};
	return test_run(test_functions);
}
