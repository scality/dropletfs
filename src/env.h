#ifndef ENV_H
#define ENV_H

struct env {
        char *cache_dir; /* cache directory */
        char *compression_method; /* "zlib" or "none" */
        int zlib_level; /* from 1 to 9 */
        int gc_loop_delay; /* in seconds */
        int gc_age_threshold; /* in seconds */
        int max_retry; /* before a timeout */
        int log_level; /* from sys/syslog.h */
} *env;


struct env *env_new(void);
void set_dplfs_env(struct env* env, int debug);

#endif /* ENV_H */
