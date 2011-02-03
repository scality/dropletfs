#ifndef LOG_H
#define LOG_H

#include <syslog.h>

#include "conf.h"

extern struct conf *conf;

#define LOG(priority, fmt, ...)                                         \
        do {                                                            \
                if (priority > conf->log_level) break;                  \
                syslog(priority, "%s:%s():%d -- " fmt "",               \
                       __FILE__, __func__, __LINE__, ##__VA_ARGS__);    \
        } while (/*CONSTCOND*/0)



#endif

