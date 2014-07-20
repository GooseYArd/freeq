#ifndef QSUTIL_H
#define QSUTIL_H

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int sleep(unsigned int);

extern void log1();
/*extern void log2();*/
extern void log3();
extern void logsa();
extern void nomem();
extern void pausedir();
extern void logsafe();

#ifdef __cplusplus
}
#endif

#endif
