#ifndef CONF_H
#define CONF_H

#include "regex.h"

struct conf {
        char *root_dir; /* the mountpoint */
        char *cache_dir; /* cache directory */
        char *compression_method; /* "zlib" or "none" */
        int zlib_level; /* from 1 to 9 */
        int gc_loop_delay; /* in seconds */
        int gc_age_threshold; /* in seconds */
        int sc_loop_delay; /* in seconds */
        int sc_age_threshold; /* in seconds */
        int max_retry; /* before a timeout */
        int log_level; /* from sys/syslog.h */
        int cache_max_size; /* in bytes */
        struct re regex; /* do not upload files matching this regex */
        char *encryption_method; /* "aes" or "none" */
        int debug;
} *conf;

char *log_level_to_str(int);
int str_to_log_level(char *);

struct conf *conf_new(void);
int conf_ctor(struct conf *, char *, int);
void conf_dtor(struct conf *);
void conf_free(struct conf *);


#endif /* CONF_H */
