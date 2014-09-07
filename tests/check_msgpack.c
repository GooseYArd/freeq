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

/* START_TEST (test_freeq_col_pack_unpack) */
/* { */
/* 	struct freeq_ctx *ctx; */
/* 	struct freeq_table *t = 0, *t2 = 0; */
	
/* 	GSList *data_one = NULL; */
/* 	GSList *data_two = NULL; */

/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(10)); */
/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(20)); */
/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(30)); */
/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(40)); */
/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(50)); */
/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(60)); */
/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(70)); */
/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(80)); */
	
/* 	data_two = g_slist_append(data_two, "one"); */
/* 	data_two = g_slist_append(data_two, "two"); */
/* 	data_two = g_slist_append(data_two, "one"); */
/* 	data_two = g_slist_append(data_two, "two"); */
/* 	data_two = g_slist_append(data_two, "one"); */
/* 	data_two = g_slist_append(data_two, "two"); */
/* 	data_two = g_slist_append(data_two, "one"); */
/* 	data_two = g_slist_append(data_two, "two"); */
	
/* 	freeq_new(&ctx, appname, identity); */
/* 	freeq_table_new(ctx, */
/* 			"foo", */
/* 			2, */
/* 			(freeq_coltype_t *)&coltypes, */
/* 			(const char **)&colnames, */
/* 			&t, */
/* 			NULL, */
/* 			data_one, */
/* 			data_two); */
	
/* 	t->numrows = 8; */
/* 	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; */
/* 	int fd = open("poop.txt", O_WRONLY | O_CREAT | O_TRUNC, mode); */
/* 	freeq_set_log_priority(ctx, 10); */
/* 	freeq_table_write(ctx, t, fd); */
/* 	close(fd); */

/* 	fd = open("poop.txt", O_RDONLY, mode); */
/* 	freeq_table_read(ctx, &t2, fd); */

/* 	freeq_table_print(ctx, t2, stdout); */
/* 	fprintf(stderr, "done printing\n"); */
/* 	freeq_table_unref(t); */
/* 	freeq_table_unref(t2); */
	
/* 	g_slist_free(data_one); */
/* 	g_slist_free(data_two); */
/* 	freeq_unref(ctx); */
/* } */
/* END_TEST */

bool compare_tables(struct freeq_table *t1, struct freeq_table *t2)
{
	fprintf(stderr, "comparing names %s and %s\n", t1->name, t2->name);
	if (strcmp(t1->name, t2->name) != 0)
		return false;

	fprintf(stderr, "name matches\n");
	if (t1->numcols != t2->numcols)
		return false;

	fprintf(stderr, "name and numcols match\n");
	for (int i=0; i < t1->numcols; i++)
	{
		fprintf(stderr, "checking column %d\n", i);
		if (t1->columns[i].coltype != t2->columns[i].coltype)
		{
			fprintf(stderr, "column type mistmach: %d != %d\n", t1->columns[i].coltype, t2->columns[i].coltype);
			return false;
		}
		fprintf(stderr, "%d: type matches\n", i);
		if (strcmp(t1->columns[i].name, t2->columns[i].name) != 0)
			return false;		
		fprintf(stderr, "%d: name matches\n", i);

		GSList *c1 = t1->columns[i].data;
		GSList *c2 = t2->columns[i].data;
		while (c1 != NULL) {
			switch (t1->columns[i].coltype) {
			case FREEQ_COL_STRING:
				if (strcmp(c1->data, c2->data) != 0) {
					fprintf(stderr, "got %s should be %s\n", c1->data, c2->data);
					return false;
				}
				break;
			case FREEQ_COL_NUMBER:
				if (c1->data != c2->data) {
					fprintf(stderr, "got %d should be %d\n", GPOINTER_TO_INT(c1->data), GPOINTER_TO_INT(c2->data));
					return false;
				}
				break;
			default:
				break;
			}

			c1 = g_slist_next(c1);
			c2 = g_slist_next(c2);
		}
		fprintf(stderr, "%d: data matches\n", i);
		if (c2 != NULL)
			return false;
		fprintf(stderr, "column %d ok\n", i);
	}
	fprintf(stderr, "everything matches\n");
	return true;
}

START_TEST (test_freeq_write_read_bio)
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
	BIO *out, *in;
	out = BIO_new_file("poop2.txt", "w");

	freeq_set_log_priority(ctx, 10);
	freeq_table_bio_write(ctx, t, out);
	BIO_free(out);

	in = BIO_new_file("poop2.txt", "r");	
	freeq_table_bio_read(ctx, &t2, in);
	BIO_free(in);

	freeq_table_print(ctx, t2, stdout);
	fprintf(stderr, "done printing\n");
	ck_assert(compare_tables(t, t2));

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
	/* tcase_add_test(tc_core, test_freeq_col_pack_unpack); */
	tcase_add_test(tc_core, test_freeq_write_read_bio);
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
