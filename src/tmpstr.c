#include <stdarg.h>
#include <pthread.h>

#include "tmpstr.h"

int vsnprintf(char *, size_t, const char *, va_list);


#define TMPSTR_MAX  2048
#define TMPSTR_NB 32

static __thread char strings[TMPSTR_NB][TMPSTR_MAX];
static __thread unsigned current;

char *
tmpstr_new(void)
{
        if (++current > TMPSTR_NB)
                current = 0;

        return strings[current];
}

char *
tmpstr_printf(char const *fmt, ...)
{
        char *buf = tmpstr_new();
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, TMPSTR_MAX, fmt, ap);
        va_end(ap);
        return buf;
}
