#include <check.h>
#include "src/freeq/libfreeq.h"
//#include "src/libfreeq-private.h"

const char *identity = "identity";
const char *appname = "appname";

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

START_TEST (test_freeq_table_new_from_string_retcode)
{
	struct freeq_ctx *ctx;
	struct freeq_table *t;
	freeq_new(&ctx, appname, identity);
	ck_assert_int_eq(freeq_table_new_from_string(ctx, "foo", &t), 0);
	freeq_table_unref(t);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_table_new_from_string_ptr)
{
	struct freeq_ctx *ctx;
	struct freeq_table *t;
	freeq_new(&ctx, appname, identity);
	freeq_table_new_from_string(ctx, "foo", &t);
	ck_assert_ptr_ne(t, NULL);
	ck_assert_ptr_ne(freeq_get_identity(ctx), NULL);
	t = freeq_table_unref(t);
	ck_assert_ptr_eq(t, NULL);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_col_new_ret)
{
	struct freeq_ctx *ctx;
	struct freeq_table *t;
	freeq_new(&ctx, appname, identity);
	freeq_table_new_from_string(ctx, "foo", &t);

	ck_assert_int_eq(freeq_table_column_new(t, "bar", FREEQ_COL_NUMBER, NULL), 0);

	freeq_table_unref(t);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_col_new_ptr)
{
	struct freeq_ctx *ctx;
	struct freeq_table *t;
	freeq_new(&ctx, appname, identity);
	freeq_table_new_from_string(ctx, "foo", &t);
	freeq_table_column_new(t, "bar", FREEQ_COL_NUMBER, NULL);
	ck_assert_ptr_ne(t->columns, NULL);
	ck_assert_int_eq(t->columns->coltype, FREEQ_COL_NUMBER);
	ck_assert_str_eq(t->columns->name, "bar");
	ck_assert_ptr_eq(t->columns->next, NULL);
	ck_assert_ptr_eq(t->columns->segments->data, NULL);
	freeq_table_unref(t);
	freeq_unref(ctx);
}
END_TEST

START_TEST (test_freeq_col_pack_unpack)
{
	struct freeq_ctx *ctx;
	struct freeq_table *t, *t2;
	int data[10] = {0,1,2,3,4,5,6,7,8,9};
	msgpack_sbuffer sbuf;
	int res;
	
	freeq_new(&ctx, appname, identity);
	freeq_table_new_from_string(ctx, "foo", &t);
	freeq_table_column_new(t, "bar", FREEQ_COL_NUMBER, &data);
	ck_assert_ptr_eq(t->columns->segments->data, &data);
	ck_assert_ptr_ne(freeq_get_identity(ctx), NULL);
	
	msgpack_sbuffer_init(&sbuf);
	ck_assert_int_eq(freeq_table_pack_msgpack(&sbuf, ctx, t), 0);	
	ck_assert_int_eq(freeq_table_header_from_msgpack(ctx, sbuf.data, sbuf.size, &t2), 0);

	
	msgpack_sbuffer_destroy(&sbuf);
	freeq_table_unref(t);
	freeq_table_unref(t2);
	freeq_unref(ctx);
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
	tcase_add_test(tc_core, test_freeq_table_new_from_string_retcode);
	tcase_add_test(tc_core, test_freeq_table_new_from_string_ptr);
	tcase_add_test(tc_core, test_freeq_col_new_ret);
	tcase_add_test(tc_core, test_freeq_col_new_ptr);
	tcase_add_test(tc_core, test_freeq_col_pack_unpack);
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
