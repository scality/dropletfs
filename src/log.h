#ifndef LOG_H
#define LOG_H

#include <syslog.h>

#include "env.h"

extern struct env *env;

#define LOG(priority, fmt, ...)                                         \
        do {                                                            \
                if (priority > env->log_level) break;                   \
                syslog(priority, "%s:%s():%d -- " fmt "",               \
                       __FILE__, __func__, __LINE__, ##__VA_ARGS__);    \
        } while (/*CONSTCOND*/0)



#endif

