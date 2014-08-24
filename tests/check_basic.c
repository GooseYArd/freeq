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

static const int ints32[] = {INT_MIN, 
			     -0x1000000, 
			     -1,
			     0,
			     1,
			     0x10000000,
			     INT_MAX
};

START_TEST (test_varint_32)
{
	uint8_t buffer[10];
	union {		
		int32_t i;
		struct longlong s;	
	} result;
	fprintf(stderr, "check: %x\n", ints32[_i]);

	_pbcV_zigzag32(ints32[_i], buffer);			
	_pbcV_decode(buffer, &(result.s));
	_pbcV_dezigzag32(&(result.s));

	ck_assert_int_eq(ints32[_i], result.i);
}
END_TEST

static const uint32_t uints32[] = { 0,
				   1,
				   0x10000000,
				   UINT_MAX };

START_TEST (test_varint_u32)
{
	uint8_t buffer[10];
	union {		
		uint32_t i;
		struct longlong s;	
	} result;
	fprintf(stderr, "check: %x\n", ints32[_i]);

	_pbcV_encode32(uints32[_i], buffer);			
	_pbcV_decode(buffer, &(result.s));
	ck_assert_int_eq(uints32[_i], result.i);
}
END_TEST

static const int64_t ints64[] = {LONG_MIN, 
				 -0x1000000000000000, 
				 -1,
				 0,
				 1,
				  0x1000000000000000,
				  LONG_MAX
};

START_TEST (test_varint_64)
{
	uint8_t buffer[10];
	union {		
		int64_t i;
		struct longlong s;	
	} result;
	_pbcV_zigzag(ints64[_i], buffer);			
	_pbcV_decode(buffer, &(result.s));
	_pbcV_dezigzag64(&(result.s));
	ck_assert_int_eq(ints64[_i], result.i);
}
END_TEST


static const uint64_t uints64[] = {0,
				 1,
				  0x1000000000000000,
				  ULONG_MAX
};

START_TEST (test_varint_u64)
{
	uint8_t buffer[10];
	union {		
		uint64_t i;
		struct longlong s;	
	} result;
	_pbcV_encode(uints64[_i], buffer);			
	_pbcV_decode(buffer, &(result.s));
	ck_assert_int_eq(uints64[_i], result.i);
}
END_TEST

START_TEST (test_buffer_putvarint32)
{
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;      
	int fd = open("poop.txt", O_WRONLY | O_CREAT | O_TRUNC, mode);	
	char buf[4096];	
	uint32_t c = 123456;
	union {		
		uint64_t i;
		struct longlong s;	
	} result;
	
	buffer output, input;
	buffer_init(&output,write,fd,buf,sizeof buf);

	buffer_putvarint32(&output, c);
	buffer_flush(&output);
	buffer_close(&output);
			   
	fd = open("poop.txt", O_RDONLY, mode);	
	buffer_init(&input,read,fd,buf,sizeof buf);

	buffer_getvarint(&input, &(result.s));
	//ck_assert_int_eq(uints64[_i], result.i);
}
END_TEST


/* START_TEST (test_freeq_col_pack_unpack) */
/* { */
/* 	struct freeq_ctx *ctx; */
/* 	struct freeq_table *t, *t2; */
/* 	GSList *data_one = NULL; */
/* 	GSList *data_two = NULL; */
/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(1)); */
/* 	data_one = g_slist_append(data_one, GINT_TO_POINTER(2)); */
/* 	data_two = g_slist_append(data_two, "one"); */
/* 	data_two = g_slist_append(data_two, "two"); */
	
/* 	freeq_new(&ctx, appname, identity); */
	
/* 	freeq_table_new(ctx,  */
/* 			"foo", */
/* 			2, */
/* 			(freeq_coltype_t *)&coltypes,  */
/* 			(const char **)&colnames,  */
/* 			&t, */
/* 			data_one, */
/* 			data_two */
/* 		); */
		
/* 	msgpack_sbuffer_init(&sbuf); */
/* 	ck_assert_int_eq(freeq_table_pack_msgpack(&sbuf, ctx, t), 0); */
/* 	ck_assert_int_eq(freeq_table_header_from_msgpack(ctx, sbuf.data, sbuf.size, &t2), 0); */

/* 	msgpack_sbuffer_destroy(&sbuf); */
/* 	freeq_table_unref(t); */
/* 	freeq_table_unref(t2); */
/* 	freeq_unref(ctx); */
/* } */
/* END_TEST */

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
	tcase_add_loop_test (tc_core, test_varint_32, 0, 7);
	tcase_add_loop_test (tc_core, test_varint_u32, 0, 4);
	tcase_add_loop_test (tc_core, test_varint_64, 0, 7);
	tcase_add_loop_test (tc_core, test_varint_u64, 0, 4);
	tcase_add_test (tc_core, test_buffer_putvarint32);

/*	tcase_add_test(tc_core, test_freeq_col_new_ret);
	tcase_add_test(tc_core, test_freeq_col_new_ptr); 
	tcase_add_test(tc_core, test_freeq_col_pack_unpack); 
*/
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
