#ifndef ENV_H
#define ENV_H

#include "regex.h"

struct env {
        char *root_dir; /* the mountpoint */
        char *cache_dir; /* cache directory */
        char *compression_method; /* "zlib" or "none" */
        int zlib_level; /* from 1 to 9 */
        int gc_loop_delay; /* in seconds */
        int gc_age_threshold; /* in seconds */
        int max_retry; /* before a timeout */
        int log_level; /* from sys/syslog.h */
        struct re regex; /* do not upload files matching this regex */
} *env;


struct env *env_new(void);
void env_ctor(struct env *, char *);
void env_dtor(struct env *);
void env_free(struct env *);
void env_set_debug(struct env *, int);
void env_log(struct env *);

#endif /* ENV_H */
