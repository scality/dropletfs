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
#include "misc.h"
#include "conf.h"

extern dpl_ctx_t *ctx;

static int
env_generic_set_int(int *var,
                    const char * const id)
{
        char *tmp = NULL;
        int ret;

        if (! var) {
                ret = -1;
                goto end;
        }

        tmp = getenv(id);
        if (! tmp || ! *tmp) {
                ret = -1;
                goto end;
        }

        *var = strtoul(tmp, NULL, 10);

        ret = 0;
  end:
        return ret;
}

static int
env_generic_set_str(char **var,
                    const char * const id)
{
        char *tmp = NULL;
        char *aux = NULL;
        int ret;

        if (! var) {
                ret = -1;
                goto end;
        }

        tmp = getenv(id);
        if (! tmp || ! *tmp) {
                ret = -1;
                goto end;
        }

        aux = strdup(tmp);
        if (! aux) {
                perror("strdup");
                ret = -1;
                goto end;
        }

        *var = aux;
        ret = 0;
  end:
        return ret;
}

static int
env_set_cache_dir(struct conf *conf)
{
        char *tmp = NULL;
        int ret;

        if (-1 == env_generic_set_str(&tmp, "DROPLETFS_CACHE_DIR")) {
                ret = 0;
                goto err;
        }

        if (-1 == conf_set_full_cache_dir(conf)) {
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        if (tmp)
                free(tmp);

        return ret;
}

static void
env_set_gc_loop_delay(struct conf *conf)
{
        (void)env_generic_set_int(&conf->gc_loop_delay,
                                  "DROPLETFS_GC_LOOP_DELAY");
}

static void
env_set_gc_age_threshold(struct conf *conf)
{
        (void)env_generic_set_int(&conf->gc_age_threshold,
                                  "DROPLETFS_GC_AGE_THRESHOLD");
}

static void
env_set_compression_method(struct conf *conf)
{
        (void)env_generic_set_str(&conf->compression_method,
                                  "DROPLETFS_COMPRESSION_METHOD");
}

static void
env_set_compression_level(struct conf *conf)
{
        (void)env_generic_set_int(&conf->zlib_level, "DROPLETFS_ZLIB_LEVEL");
}

static void
env_set_max_retry(struct conf *conf)
{
        (void)env_generic_set_int(&conf->max_retry, "DROPLETFS_MAX_RETRY");
}

static void
env_set_exclusion_pattern(struct conf *conf)
{
        char *tmp = NULL;

        tmp = getenv("DROPLETFS_EXCLUSION_PATTERN");
        if (! tmp)
                return;

        re_dtor(&conf->regex);
        (void)re_ctor(&conf->regex, tmp, REG_EXTENDED);
}

void
env_override_conf(struct conf *conf)
{
        env_set_compression_method(conf);
        env_set_compression_level(conf);
        env_set_max_retry(conf);
        env_set_gc_loop_delay(conf);
        env_set_gc_age_threshold(conf);
        env_set_exclusion_pattern(conf);

        if (-1 == env_set_cache_dir(conf)) {
                fprintf(stderr, "can't create any cache directory\n");
                exit(EXIT_FAILURE);
        }
}
