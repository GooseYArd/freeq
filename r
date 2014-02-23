make  all-recursive
make[1]: Entering directory `/home/rbailey/src/freeq'
Making all in contrib
make[2]: Entering directory `/home/rbailey/src/freeq/contrib'
make[2]: Nothing to be done for `all'.
make[2]: Leaving directory `/home/rbailey/src/freeq/contrib'
Making all in lib
make[2]: Entering directory `/home/rbailey/src/freeq/lib'
make  all-recursive
make[3]: Entering directory `/home/rbailey/src/freeq/lib'
make[4]: Entering directory `/home/rbailey/src/freeq/lib'
gcc -std=gnu99 -DHAVE_CONFIG_H -I. -I..   -I/home/rbailey/src/install/include  -ggdb -O0 -I/home/rbailey/src/install/include -MT libfreeq.o -MD -MP -MF .deps/libfreeq.Tpo -c -o libfreeq.o libfreeq.c
libfreeq.c:240:8: error: redefinition of 'struct freeq_table'
 struct freeq_table {
        ^
In file included from libfreeq.c:30:0:
../freeq/libfreeq.h:72:8: note: originally defined here
 struct freeq_table {
        ^
libfreeq.c:249:34: error: conflicting types for 'freeq_table_ref'
 FREEQ_EXPORT struct freeq_table *freeq_table_ref(struct freeq_table *table)
                                  ^
In file included from libfreeq.c:30:0:
../freeq/libfreeq.h:97:21: note: previous declaration of 'freeq_table_ref' was here
 struct freeq_table *freeq_table_ref(struct freeq_table *table);
                     ^
libfreeq.c:257:34: error: conflicting types for 'freeq_table_unref'
 FREEQ_EXPORT struct freeq_table *freeq_table_unref(struct freeq_table *table)
                                  ^
In file included from libfreeq.c:30:0:
../freeq/libfreeq.h:98:21: note: previous declaration of 'freeq_table_unref' was here
 struct freeq_table *freeq_table_unref(struct freeq_table *table);
                     ^
libfreeq.c:292:32: error: conflicting types for 'freeq_table_get_ctx'
 FREEQ_EXPORT struct freeq_ctx *freeq_table_get_ctx(struct freeq_table *table)
                                ^
In file included from libfreeq.c:30:0:
../freeq/libfreeq.h:99:19: note: previous declaration of 'freeq_table_get_ctx' was here
 struct freeq_ctx *freeq_table_get_ctx(struct freeq_table *table);
                   ^
libfreeq.c:297:18: error: conflicting types for 'freeq_table_new_from_string'
 FREEQ_EXPORT int freeq_table_new_from_string(struct freeq_ctx *ctx, const char *string, struct freeq_table **table)
                  ^
In file included from libfreeq.c:30:0:
../freeq/libfreeq.h:100:5: note: previous declaration of 'freeq_table_new_from_string' was here
 int freeq_table_new_from_string(struct freeq_ctx *ctx, const char *string, struct freeq_table **table);
     ^
libfreeq.c:313:18: error: conflicting types for 'freeq_table_column_new'
 FREEQ_EXPORT int freeq_table_column_new(struct freeq_table *table, const char *name, freeq_coltype_t coltype)
                  ^
In file included from libfreeq.c:30:0:
../freeq/libfreeq.h:81:5: note: previous declaration of 'freeq_table_column_new' was here
 int freeq_table_column_new(struct freeq_table *table, const char *name, freeq_coltype_t coltype);
     ^
libfreeq.c:334:35: error: conflicting types for 'freeq_table_get_some_column'
 FREEQ_EXPORT struct freeq_column *freeq_table_get_some_column(struct freeq_table *table)
                                   ^
In file included from libfreeq.c:30:0:
../freeq/libfreeq.h:101:22: note: previous declaration of 'freeq_table_get_some_column' was here
 struct freeq_column *freeq_table_get_some_column(struct freeq_table *table);
                      ^
make[4]: *** [libfreeq.o] Error 1
make[4]: Leaving directory `/home/rbailey/src/freeq/lib'
make[3]: *** [all-recursive] Error 1
make[3]: Leaving directory `/home/rbailey/src/freeq/lib'
make[2]: *** [all] Error 2
make[2]: Leaving directory `/home/rbailey/src/freeq/lib'
make[1]: *** [all-recursive] Error 1
make[1]: Leaving directory `/home/rbailey/src/freeq'
make: *** [all] Error 2
