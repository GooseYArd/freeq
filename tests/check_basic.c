#include <check.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdint.h>
#include "src/freeq/libfreeq.h"
#include <string.h>
//#include "src/libfreeq-private.h"
#include "src/varint.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "buffer.h"

const char *identity = "identity";
const char *appname = "appname";
const char *colnames[] = { "one", "two" };
freeq_coltype_t coltypes[] = { 1, 2 };

char buf[4096];
typedef union {
	uint64_t i;
	struct longlong s;
} result_t;

START_TEST (test_freeq_new)
{
	struct freeq_ctx *ctx;
	ck_assert_int_eq(freeq_new(&ctx, appname, identity), 0);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_ctx_val)
{
	struct freeq_ctx *ctx;
	freeq_new(&ctx, appname, identity);
	ck_assert_ptr_ne(ctx, NULL);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_ctx_identity)
{
	struct freeq_ctx *ctx;
	freeq_new(&ctx, appname, identity);
	ck_assert_ptr_eq(freeq_get_identity(ctx), identity);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_ctx_default_identity)
{
	struct freeq_ctx *ctx;
	freeq_new(&ctx, appname, NULL);
	ck_assert_ptr_ne(freeq_get_identity(ctx), NULL);
	ck_assert_str_eq(freeq_get_identity(ctx), "unknown");
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_unref)
{
	struct freeq_ctx *ctx;
	freeq_new(&ctx, appname, identity);
	ctx = freeq_unref(ctx);
	ck_assert_ptr_eq(ctx, NULL);
}
END_TEST

START_TEST (test_freeq_log_priority_default)
{
	struct freeq_ctx *ctx;
	freeq_new(&ctx, appname, identity);
	ck_assert_int_eq(freeq_get_log_priority(ctx), 3);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_log_priority_nondefault)
{
	struct freeq_ctx *ctx;
	freeq_new(&ctx, appname, identity);
	freeq_set_log_priority(ctx, 4);
	ck_assert_int_eq(freeq_get_log_priority(ctx), 4);
	freeq_unref(ctx);

}
END_TEST

START_TEST (test_freeq_table_new_retcode)
{
	struct freeq_ctx *ctx;
	struct freeq_table *t;
	int v;

	GSList *data_one = NULL;
	GSList *data_two = NULL;
	data_one = g_slist_append(data_one, GINT_TO_POINTER(1));
	data_one = g_slist_append(data_one, GINT_TO_POINTER(2));
	data_two = g_slist_append(data_two, "one");
	data_two = g_slist_append(data_two, "two");

	freeq_new(&ctx, appname, identity);
	v = freeq_table_new(ctx,
			    "foo",
			    2,
			    (freeq_coltype_t *)&coltypes,
			    (const char **)&colnames,
			    &t,
			    true,
			    data_one,
			    data_two);
	ck_assert_int_eq(v, 0);
	freeq_table_unref(t);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_table_new_ptr_nullcol)
{
	struct freeq_ctx *ctx;
	struct freeq_table *t;
	freeq_new(&ctx, appname, identity);
	freeq_set_log_priority(ctx, 10);
	freeq_table_new(ctx,
			"foo",
			2,
			(freeq_coltype_t *)&coltypes,
			(const char **)&colnames,
			&t,
			false,
			NULL
		);

	ck_assert_ptr_eq(t, NULL);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_table_new_ptr)
{
	struct freeq_ctx *ctx;
	struct freeq_table *t;
	GSList *data_one = NULL;
	GSList *data_two = NULL;
	data_one = g_slist_append(data_one, GINT_TO_POINTER(1));
	data_one = g_slist_append(data_one, GINT_TO_POINTER(2));
	data_two = g_slist_append(data_two, "one");
	data_two = g_slist_append(data_two, "two");

	freeq_new(&ctx, appname, identity);
	freeq_table_new(ctx,
			"foo",
			2,
			(freeq_coltype_t *)&coltypes,
			(const char **)&colnames,
			&t,
			true,
			data_one,
			data_two
		);

	ck_assert_ptr_ne(t, NULL);
	ck_assert_ptr_ne(freeq_get_identity(ctx), NULL);
	t = freeq_table_unref(t);
	ck_assert_ptr_eq(t, NULL);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_varint_32)
{
	static const int vals[] = {INT_MIN,
				   -0x1000000,
				   -1,
				   0,
				   1,
				   0x10000000,
				   INT_MAX
	};

	int b;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int fd = open("poop.txt", O_WRONLY | O_CREAT | O_TRUNC, mode);

	buffer output, input;
	buffer_init(&output,write,fd,buf,sizeof buf);

	int bytes[7];
	for (int i = 0; i < 7; i++)
		bytes[i] = buffer_putvarintsigned32(&output, vals[i]);

	buffer_flush(&output);
	buffer_close(&output);

	fd = open("poop.txt", O_RDONLY, mode);
	buffer_init(&input,read,fd,buf,sizeof buf);

	for (int i = 0; i < 7; i++)
	{
		int32_t v;
		struct longlong r;
		b = buffer_getvarint(&input, &r);
		dezigzag32(&r);
		v = r.low;
		ck_assert_int_eq(b, bytes[i]);
		ck_assert_int_eq(vals[i], v);
	}

	buffer_close(&input);
}
END_TEST


START_TEST (test_varint_u32)
{
	static const uint32_t vals[] = { 0,
					 1,
					 0x10000000,
					 UINT_MAX };

	int b;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int fd = open("poop.txt", O_WRONLY | O_CREAT | O_TRUNC, mode);

	buffer output, input;
	buffer_init(&output,write,fd,buf,sizeof buf);

	int bytes[4];
	for (int i = 0; i < 4; i++)
		bytes[i] = buffer_putvarint32(&output, vals[i]);

	buffer_flush(&output);
	buffer_close(&output);

	fd = open("poop.txt", O_RDONLY, mode);
	buffer_init(&input,read,fd,buf,sizeof buf);

	for (int i = 0; i < 4; i++)
	{
		uint32_t v;
		struct longlong r;
		b = buffer_getvarint(&input, &r);
		v = r.low;
		ck_assert_int_eq(b, bytes[i]);
		ck_assert_int_eq(vals[i], v);
	}
	buffer_close(&input);
}
END_TEST

START_TEST (test_varint_64)
{

	static const int64_t vals[] = {LONG_MIN,
				       -0x1000000000000000,
				       -1,
				       0,
				       1,
				       0x1000000000000000,
				       LONG_MAX
	};

	int b;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int fd = open("poop.txt", O_WRONLY | O_CREAT | O_TRUNC, mode);

	buffer output, input;
	buffer_init(&output,write,fd,buf,sizeof buf);

	int bytes[7];
	for (int i = 0; i < 7; i++)
		bytes[i] = buffer_putvarintsigned(&output, vals[i]);

	buffer_flush(&output);
	buffer_close(&output);

	fd = open("poop.txt", O_RDONLY, mode);
	buffer_init(&input,read,fd,buf,sizeof buf);

	for (int i = 0; i < 7; i++)
	{
		int64_t v;
		result_t r;
		b = buffer_getvarint(&input, &r.s);
		dezigzag64(&r.s);
		v = r.i;
		ck_assert_int_eq(b, bytes[i]);
		ck_assert_int_eq(vals[i], v);
	}

	buffer_close(&input);

}
END_TEST

START_TEST (test_varint_u64)
{
	static const uint64_t vals[] = {0,
					1,
					0x1000000000000000,
					ULONG_MAX
	};

	int b;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int fd = open("poop.txt", O_WRONLY | O_CREAT | O_TRUNC, mode);

	buffer output, input;
	buffer_init(&output,write,fd,buf,sizeof buf);

	int bytes[4];
	for (int i = 0; i < 4; i++)
		bytes[i] = buffer_putvarint(&output, vals[i]);

	buffer_flush(&output);
	buffer_close(&output);

	fd = open("poop.txt", O_RDONLY, mode);
	buffer_init(&input,read,fd,buf,sizeof buf);

	for (int i = 0; i < 4; i++)
	{
		uint64_t v;
		result_t r;
		b = buffer_getvarint(&input, &r.s);		
		v = r.i;
		ck_assert_int_eq(b, bytes[i]);
		ck_assert_int_eq(vals[i], v);
	}

	buffer_close(&input);

}
END_TEST

Suite *
freeq_basic_suite (void)
{
	Suite *s = suite_create("freeq_basic");
	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, test_freeq_new);
	tcase_add_test(tc_core, test_freeq_ctx_val);
	tcase_add_test(tc_core, test_freeq_ctx_identity);
	tcase_add_test(tc_core, test_freeq_ctx_default_identity);
	tcase_add_test(tc_core, test_freeq_unref);
	tcase_add_test(tc_core, test_freeq_log_priority_default);
	tcase_add_test(tc_core, test_freeq_log_priority_nondefault);
	tcase_add_test(tc_core, test_freeq_table_new_retcode);
	tcase_add_test(tc_core, test_freeq_table_new_ptr_nullcol);
	tcase_add_test(tc_core, test_freeq_table_new_ptr);
	tcase_add_test (tc_core, test_varint_32);
	tcase_add_test (tc_core, test_varint_u32);
	tcase_add_test (tc_core, test_varint_64);
	tcase_add_test (tc_core, test_varint_u64);

	suite_add_tcase(s, tc_core);
	return s;
}

int
main (void)
{
	int number_failed;
	Suite *s = freeq_basic_suite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
