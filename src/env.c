#include <droplet.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <stdio.h>

#include "env.h"
#include "log.h"
#include "tmpstr.h"
#include "regex.h"

#define DEFAULT_COMPRESSION_METHOD "NONE"
#define DEFAULT_ZLIB_LEVEL 3
#define DEFAULT_CACHE_DIR "/tmp"
#define DEFAULT_MAX_RETRY 5
#define DEFAULT_GC_LOOP_DELAY 60
#define DEFAULT_GC_AGE_THRESHOLD 0
#define DEFAULT_LOG_LEVEL LOG_ERR
#define DEFAULT_EXCLUSION_REGEXP NULL

extern struct env *env;
extern dpl_ctx_t *ctx;


struct env *
env_new(void)
{
        struct env *env = NULL;

        env = malloc(sizeof *env);
        if (! env) {
                perror("malloc");
                exit(EXIT_FAILURE);
        }

        memset(env, 0, sizeof *env);

        return env;
}

static void
env_set_root_dir(struct env *env,
                 char *root_dir)
{
        char *p = NULL;

        if (! root_dir)
                return;

        if (env->root_dir)
                free(env->root_dir);

        /* remove the trailing slashes */
        p = root_dir + strlen(root_dir) - 1;
        while (p && *p && '/' == *p)
                *p-- = 0;

        env->root_dir = strdup(root_dir);
        if (! env->root_dir) {
                perror("strdup");
                exit(EXIT_FAILURE);
        }
}

static int
env_set_cache_dir(struct env *env)
{
        char *tmp = NULL;
        char *p = NULL;
        char *cache = NULL;
        int ret;

        tmp = getenv("DROPLETFS_CACHE_DIR");
        if (! tmp)
                tmp = DEFAULT_CACHE_DIR;

        /* remove the trailing slashes */
        p = tmp + strlen(tmp) - 1;
        while (p && *p && '/' == *p) {
                *p = 0;
                p--;
        }

        cache = tmpstr_printf("%s/%s", tmp, ctx->cur_bucket);

        env->cache_dir = strdup(cache);
        if (! env->cache_dir) {
                perror("strdup");
                ret = -1;
                goto err;
        }

        if (-1 == mkdir(env->cache_dir, 0777) && EEXIST != errno) {
                perror("mkdir");
                ret = -1;
                goto err;
        }

        /* remove the trailing slashes */
        p = env->cache_dir + strlen(env->cache_dir) - 1;
        while (p && *p && '/' == *p) {
                *p = 0;
                p--;
        }

        ret = 0;
  err:
        return ret;
}

static void
env_set_gc_loop_delay(struct env *env)
{
        char *tmp = NULL;

        tmp = getenv("DROPLETFS_GC_LOOP_DELAY");

        if (tmp)
                env->gc_loop_delay = strtoul(tmp, NULL, 10);
        else
                env->gc_loop_delay = DEFAULT_GC_LOOP_DELAY;
}

static void
env_set_gc_age_threshold(struct env *env)
{
        char *tmp = NULL;

        tmp = getenv("DROPLETFS_GC_AGE_THRESHOLD");

        if (tmp)
                env->gc_age_threshold = strtoul(tmp, NULL, 10);
        else
                env->gc_age_threshold = DEFAULT_GC_AGE_THRESHOLD;
}

static char *
log_level_to_str(int level)
{
#define case_log(x) case x: return #x
        switch(level) {
                case_log(LOG_EMERG);
                case_log(LOG_ALERT);
                case_log(LOG_CRIT);
                case_log(LOG_ERR);
                case_log(LOG_WARNING);
                case_log(LOG_NOTICE);
                case_log(LOG_INFO);
                case_log(LOG_DEBUG);
        }
#undef case_log

        assert(! "impossible case");
        return "INVALID";
}

static int
str_to_log_level(char *str)
{
#define case_str(prio) if (! strcmp(str, #prio)) return prio;
        case_str(LOG_EMERG);
        case_str(LOG_ALERT);
        case_str(LOG_CRIT);
        case_str(LOG_ERR);
        case_str(LOG_WARNING);
        case_str(LOG_NOTICE);
        case_str(LOG_INFO);
        case_str(LOG_DEBUG);
#undef case_str

        /* the environment variable was set with an erroneous value */
        return DEFAULT_LOG_LEVEL;
}

static void
env_set_log_level(struct env *env)
{
        char *tmp = NULL;

        tmp = getenv("DROPLETFS_LOG_LEVEL");

        if (tmp)
                env->log_level = str_to_log_level(tmp);
        else
                env->log_level = DEFAULT_LOG_LEVEL;

        if (env->log_level > LOG_DEBUG || env->log_level < LOG_EMERG) {
                fprintf(stderr, "invalid debug level (%d), set to %d",
                        env->log_level, DEFAULT_LOG_LEVEL);
                env->log_level = DEFAULT_LOG_LEVEL;
        }

}

static void
env_set_compression(struct env *env)
{
        char *tmp = NULL;

        tmp = getenv("DROPLETFS_COMPRESSION_METHOD");

        if (tmp) {
                if (0 != strncasecmp(tmp, "zlib", strlen("zlib")))
                        env->compression_method = strdup(DEFAULT_COMPRESSION_METHOD);
                else
                        env->compression_method = strdup(tmp);
        } else {
                env->compression_method = strdup(DEFAULT_COMPRESSION_METHOD);
	}

        tmp = getenv("DROPLETFS_ZLIB_LEVEL");

        if (tmp)
                env->zlib_level = strtoul(tmp, NULL, 10);
        else
                env->zlib_level = DEFAULT_ZLIB_LEVEL;
}

static void
env_set_max_retry(struct env *env)
{
        char *tmp = NULL;

        tmp = getenv("DROPLETFS_MAX_RETRY");

        env->max_retry = DEFAULT_MAX_RETRY;
        if (tmp)
                env->max_retry = strtoul(tmp, NULL, 10);
}

static void
env_set_exclusion_pattern(struct env *env)
{
        char *tmp = NULL;

        tmp = getenv("DROPLETFS_EXCLUSION_PATTERN");

        (void)re_ctor(&env->regex, tmp, REG_EXTENDED);
}

void
env_set_debug(struct env *env,
              int debug)
{
        if (debug)
                env->log_level = LOG_DEBUG;
}

void
env_ctor(struct env *env,
         char *root_dir)
{
        env_set_log_level(env);
        env_set_compression(env);
        env_set_max_retry(env);
        env_set_gc_loop_delay(env);
        env_set_gc_age_threshold(env);
        env_set_exclusion_pattern(env);
        env_set_root_dir(env, root_dir);

        if (-1 == env_set_cache_dir(env)) {
                fprintf(stderr, "can't create any cache directory");
                exit(EXIT_FAILURE);
        }
}

void
env_dtor(struct env *env)
{
        if (env->regex.set)
                re_dtor(&env->regex);
}

void
env_free(struct env *env)
{
        env_dtor(env);

        if (env->compression_method)
                free(env->compression_method);

        if (env->cache_dir)
                free(env->cache_dir);

        free(env);
}

void
env_log(struct env *env)
{
        LOG(LOG_ERR, "zlib compression level set to: %d",
            env->zlib_level);
        LOG(LOG_ERR, "zlib compression method set to: %s",
            env->compression_method);
        LOG(LOG_ERR, "local cache directory set to: %s",
            env->cache_dir);
        LOG(LOG_ERR, "max number of network i/o attempts set to: %d",
            env->max_retry);
        LOG(LOG_ERR, "garbage collector loop delay set to: %d",
            env->gc_loop_delay);
        LOG(LOG_ERR, "garbage collector age threshold set to: %d",
            env->gc_age_threshold);
        LOG(LOG_ERR, "debug level set to: %d (%s)",
            env->log_level, log_level_to_str(env->log_level));
        LOG(LOG_ERR, "exclusion regex set to: '%s'", env->regex.str);
}
