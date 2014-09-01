#include <check.h>
#include "src/freeq/libfreeq.h"
//#include "src/libfreeq-private.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

const char *identity = "identity";
const char *appname = "appname";

const char *colnames[] = { "one", "two" };
freeq_coltype_t coltypes[] = { FREEQ_COL_NUMBER, FREEQ_COL_STRING };


START_TEST (test_freeq_col_pack_unpack)
{
	struct freeq_ctx *ctx;
	struct freeq_table *t = 0, *t2 = 0;
	
	GSList *data_one = NULL;
	GSList *data_two = NULL;

	data_one = g_slist_append(data_one, GINT_TO_POINTER(10));
	data_one = g_slist_append(data_one, GINT_TO_POINTER(20));
	data_one = g_slist_append(data_one, GINT_TO_POINTER(30));
	data_one = g_slist_append(data_one, GINT_TO_POINTER(40));
	data_one = g_slist_append(data_one, GINT_TO_POINTER(50));
	data_one = g_slist_append(data_one, GINT_TO_POINTER(60));
	data_one = g_slist_append(data_one, GINT_TO_POINTER(70));
	data_one = g_slist_append(data_one, GINT_TO_POINTER(80));
	
	data_two = g_slist_append(data_two, "one");
	data_two = g_slist_append(data_two, "two");
	data_two = g_slist_append(data_two, "one");
	data_two = g_slist_append(data_two, "two");
	data_two = g_slist_append(data_two, "one");
	data_two = g_slist_append(data_two, "two");
	data_two = g_slist_append(data_two, "one");
	data_two = g_slist_append(data_two, "two");
				  
	freeq_new(&ctx, appname, identity);
	freeq_table_new(ctx,
			"foo",
			2,
			(freeq_coltype_t *)&coltypes,
			(const char **)&colnames,
			&t,
			NULL,
			data_one,
			data_two);

	t->numrows = 8;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int fd = open("poop.txt", O_WRONLY | O_CREAT | O_TRUNC, mode);
	freeq_set_log_priority(ctx, 10);
	freeq_table_write(ctx, t, fd);
	close(fd);

	fd = open("poop.txt", O_RDONLY, mode);
	freeq_table_read(ctx, &t2, fd);

	freeq_table_print(ctx, t2, stdout);

	freeq_table_unref(t);
	freeq_table_unref(t2);
	
	g_slist_free(data_one);
	g_slist_free(data_two);
	freeq_unref(ctx);
}
END_TEST

/* START_TEST (test_freeq_col_pack_unpack_check_data) */
/* { */
/* 	struct freeq_ctx *ctx; */
/* 	struct freeq_table *t, *t2; */
/* 	int data[10] = {0,1,2,3,4,5,6,7,8,9}; */
/* 	msgpack_sbuffer sbuf; */
	
/* 	freeq_new(&ctx, appname, identity); */
/* 	freeq_table_new_from_string(ctx, "foo", &t); */
/* 	freeq_table_column_new(ctx, t, "bar", FREEQ_COL_NUMBER, &data, 10); */
	
/* 	msgpack_sbuffer_init(&sbuf); */

/* 	freeq_table_pack_msgpack(&sbuf, ctx, t); */
/* 	freeq_table_header_from_msgpack(ctx, sbuf.data, sbuf.size, &t2); */
	
/* 	ck_assert_str_eq(t->name, "foo"); */
/* 	ck_assert_int_eq(t2->numcols, 1); */
/* 	ck_assert_int_eq(t2->numrows, 10); */
/* 	ck_assert_str_eq(t->columns->name, "bar"); */
/* 	ck_assert_int_eq(t2->columns->segments->len, 10); */

/* 	for (int i = 0; i < 10; i++) { */
/* 		ck_assert_int_eq(((int *)t->columns->segments->data)[i], */
/* 				 ((int *)t2->columns->segments->data)[i]); */
/* 	} */

/* 	msgpack_sbuffer_destroy(&sbuf); */
/* 	freeq_table_unref(t); */
/* 	freeq_table_unref(t2); */
/* 	freeq_unref(ctx); */
/* } */
/* END_TEST */

/* START_TEST (test_freeq_col_pack_something) */
/* { */
/*  	struct freeq_ctx *ctx; */
/* 	struct freeq_table *t, *t2; */
/* 	int *data; */
/* 	//int data[2] = {1,0}; */

/* 	int res; */
/* 	//int data[10] = {0,1,2,3,4,5,6,7,8,9}; */
/* 	msgpack_sbuffer sbuf; */
	
/* 	freeq_new(&ctx, appname, identity); */
/* 	freeq_table_new_from_string(ctx, "foo", &t); */
/* 	freeq_table_column_new(ctx, t, "bar", FREEQ_COL_NUMBER, &data, 10);	 */

/* 	msgpack_sbuffer_init(&sbuf); */
	
/* 	res = freeq_table_pack_msgpack(&sbuf, ctx, t); */
/* 	freeq_table_header_from_msgpack(ctx, sbuf.data, sbuf.size, &t2); */
/* 	ck_assert_int_eq(res,0); */
	
/* } */
/* END_TEST */

Suite *
freeq_basic_suite (void)
{
	Suite *s = suite_create("freeq_msgpack");
	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, test_freeq_col_pack_unpack);
	/*tcase_add_test(tc_core, test_freeq_col_pack_unpack_check_data);
	tcase_add_test(tc_core, test_freeq_col_pack_something);*/

	suite_add_tcase(s, tc_core);

	return s;
}

int
main (void)
{
	int number_failed;
	Suite *s = freeq_basic_suite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_NOFORK);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
