#ifndef TMPSTR_H
#define TMPSTR_H

#define TMPSTR_MAX  2048
#define TMPSTR_NB 32

char *tmpstr_new(void);
char *tmpstr_printf(char const *fmt, ...);

#endif
