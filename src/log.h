#ifndef LOG_H
#define LOG_H

#include <syslog.h>

extern int debug;

#define LOG(fmt, ...)                                                   \
        do {                                                            \
                if (! debug) break;                                     \
                syslog(LOG_INFO, "%s:%s():%d -- " fmt "",               \
                       __FILE__, __func__, __LINE__, ##__VA_ARGS__);    \
        } while (/*CONSTCOND*/0)


#define PRINT_FLAGS(path, info)                                         \
        do {                                                            \
                switch (info->flags & O_ACCMODE) {                      \
                case O_RDONLY:                                          \
                        LOG("%s: read only", path);                     \
                        break;                                          \
                case O_WRONLY:                                          \
                        LOG("%s: write only", path);                    \
                        break;                                          \
                case O_RDWR:                                            \
                        LOG("%s: read/write", path);                    \
                        break;                                          \
                default:                                                \
                        LOG("%s: unknown flags 0x%x",                   \
                            path, info->flags & O_ACCMODE);             \
                }                                                       \
        } while (/*CONSTCOND*/ 0)



#endif

