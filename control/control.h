#ifndef CONTROL_H
#define CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

extern int control_init();
extern int control_readline();
extern int control_rldef();
extern int control_readint();
extern int control_readfile(stralloc *, char *, int);

#ifdef __cplusplus
}
#endif


#endif
