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


#define PRINT_FLAGS(path, info)                                         \
        do {                                                            \
                switch (info->flags & O_ACCMODE) {                      \
                case O_RDONLY:                                          \
                        LOG(LOG_DEBUG, "%s: read only", path);          \
                        break;                                          \
                case O_WRONLY:                                          \
                        LOG(LOG_DEBUG, "%s: write only", path);         \
                        break;                                          \
                case O_RDWR:                                            \
                        LOG(LOG_DEBUG, "%s: read/write", path);         \
                        break;                                          \
                default:                                                \
                        LOG(LOG_DEBUG, "%s: unknown flags 0x%x",        \
                            path, info->flags & O_ACCMODE);             \
                }                                                       \
        } while (/*CONSTCOND*/ 0)



#endif

