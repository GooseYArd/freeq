#ifndef CONSTMAP_H
#define CONSTMAP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long constmap_hash;

struct constmap {
  int num;
  constmap_hash mask;
  constmap_hash *hash;
  int *first;
  int *next;
  char **input;
  int *inputlen;
} ;

extern int constmap_init(struct constmap *, char *, int, int);
extern void constmap_free(struct constmap *);
extern char *constmap();

#ifdef __cplusplus
}
#endif

#endif
