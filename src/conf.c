#include <assert.h>
#include <droplet.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "conf.h"
#include "log.h"
#include "tmpstr.h"
#include "env.h"

#define DEFAULT_CONFIG_FILE ".dplfsrc"

#define DEFAULT_COMPRESSION_METHOD "NONE"
#define DEFAULT_ZLIB_LEVEL 3
#define DEFAULT_CACHE_DIR "/tmp"
#define DEFAULT_MAX_RETRY 5
#define DEFAULT_GC_LOOP_DELAY 60
#define DEFAULT_GC_AGE_THRESHOLD 900
#define DEFAULT_LOG_LEVEL LOG_ERR
#define DEFAULT_LOG_LEVEL_STR STRIZE(DEFAULT_LOG_LEVEL)
#define DEFAULT_EXCLUSION_REGEXP NULL

#define COMPRESSION_METHOD "compression_method"
#define COMPRESSION_METHOD_LEN strlen(COMPRESSION_METHOD)
#define ZLIB_LEVEL "zlib_level"
#define ZLIB_LEVEL_LEN strlen(ZLIB_LEVEL)
#define MAX_RETRY "max_retry"
#define MAX_RETRY_LEN strlen(MAX_RETRY)
#define GC_LOOP_DELAY "gc_loop_delay"
#define GC_LOOP_DELAY_LEN strlen(GC_LOOP_DELAY)
#define GC_AGE_THRESHOLD "gc_age_threshold"
#define GC_AGE_THRESHOLD_LEN strlen(GC_AGE_THRESHOLD)
#define CACHE_DIR "cache_dir"
#define CACHE_DIR_LEN strlen(CACHE_DIR)
#define EXCLUSION_PATTERN "exclusion_pattern"
#define EXCLUSION_PATTERN_LEN strlen(EXCLUSION_PATTERN)
#define LOG_LEVEL "log_level"
#define LOG_LEVEL_LEN strlen(LOG_LEVEL)

extern dpl_ctx_t *ctx;

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

struct conf *
conf_new(void)
{
        struct conf *conf = NULL;

        conf = malloc(sizeof *conf);
        if (! conf) {
                perror("malloc");
                exit(EXIT_FAILURE);
        }

        memset(conf, 0, sizeof *conf);

        return conf;
}

void
conf_dtor(struct conf *conf)
{
        if (conf->regex.set)
                re_dtor(&conf->regex);
}

void
conf_free(struct conf *conf)
{
        conf_dtor(conf);

        if (conf->compression_method)
                free(conf->compression_method);

        if (conf->cache_dir)
                free(conf->cache_dir);

        free(conf);
}

void
conf_log(struct conf *conf)
{
        LOG(LOG_ERR, "zlib level: %d", conf->zlib_level);
        LOG(LOG_ERR, "compression method: %s", conf->compression_method);
        LOG(LOG_ERR, "local cache directory: %s", conf->cache_dir);
        LOG(LOG_ERR, "max number I/O attempts: %d", conf->max_retry);
        LOG(LOG_ERR, "gc loop delay: %d", conf->gc_loop_delay);
        LOG(LOG_ERR, "gc age threshold: %d", conf->gc_age_threshold);
        LOG(LOG_ERR, "debug level: %d (%s)",
            conf->log_level, log_level_to_str(conf->log_level));
        LOG(LOG_ERR, "exclusion regex: '%s'", conf->regex.str);
}

static void
conf_set_root_dir(struct conf *conf,
                  char *root_dir)
{
        char *p = NULL;

        if (! root_dir)
                return;

        if (conf->root_dir)
                free(conf->root_dir);

        /* remove the trailing slashes */
        p = root_dir + strlen(root_dir) - 1;
        while (p && *p && '/' == *p)
                *p-- = 0;

        conf->root_dir = strdup(root_dir);
        if (! conf->root_dir) {
                perror("strdup");
                exit(EXIT_FAILURE);
        }
}

static int
conf_set_full_cache_dir(struct conf *conf)
{
        char *p = NULL;
        char *cache = NULL;
        int ret;

        if (! conf->cache_dir) {
                fprintf(stderr, "no cache dir set from config\n");
                ret = -1;
                goto err;
        }

        cache = tmpstr_printf("%s/%s", conf->cache_dir, ctx->cur_bucket);
        if (conf->cache_dir)
                free(conf->cache_dir);

        conf->cache_dir = strdup(cache);
        if (! conf->cache_dir) {
                perror("strdup");
                ret = -1;
                goto err;
        }

        if (-1 == mkdir(conf->cache_dir, 0777) && EEXIST != errno) {
                perror("mkdir");
                ret = -1;
                goto err;
        }

        /* remove the trailing slashes */
        p = conf->cache_dir + strlen(conf->cache_dir) - 1;
        while (p && *p && '/' == *p) {
                *p = 0;
                p--;
        }

        ret = 0;
  err:
        return ret;
}

static int
parse_int(int *field,
          char *line)
{
        int ret;
        char *p = NULL;

        /* remove leading garbage */
        p = strchr(line, '=');
        if (! p) {
                fprintf(stderr, "missing assignation in line \"%s\"\n", line);
                ret = -1;
                goto err;
        }

        /* skip the '=' */
        p++;

        while (p && *p && isspace(*p))
                p++;

        if (field)
                *field = strtoul(p, NULL, 10);

        ret = 0;
  err:
        return ret;
}

static int
parse_str(char **field,
          char *line)
{
        int ret;
        char *p = NULL;

        /* remove leading garbage */
        p = strchr(line, '=');
        if (! p) {
                fprintf(stderr, "missing assignation in line \"%s\"\n", line);
                ret = -1;
                goto err;
        }

        /* skip the '=' */
        p++;

        while (p && *p && isspace(*p))
                p++;

        if (p && *p && field) {
                if (*field)
                        free(*field);
                *field = strdup(p);
                if (! *field) {
                        perror("strdup");
                        ret = -1;
                        goto err;
                }
        }

        ret = 0;
  err:
        return ret;
}

static int
parse_token(struct conf * conf,
            char *token)
{
        char *log = NULL;
        int ret;

        if (! strncasecmp(token, COMPRESSION_METHOD, COMPRESSION_METHOD_LEN)) {
                if (-1 == parse_str(&conf->compression_method, token)) {
                        ret = -1;
                        goto err;
                }
        }

        if (! strncasecmp(token, ZLIB_LEVEL, ZLIB_LEVEL_LEN)) {
                if (-1 == parse_int(&conf->zlib_level, token)) {
                        ret = -1;
                        goto err;
                }
        }

        if (! strncasecmp(token, MAX_RETRY, MAX_RETRY_LEN)) {
                if (-1 == parse_int(&conf->max_retry, token)) {
                        ret = -1;
                        goto err;
                }
        }

        if (! strncasecmp(token, GC_LOOP_DELAY, GC_LOOP_DELAY_LEN)) {
                if (-1 == parse_int(&conf->gc_loop_delay, token)) {
                        ret = -1;
                        goto err;
                }
        }

        if (! strncasecmp(token, GC_AGE_THRESHOLD, GC_AGE_THRESHOLD_LEN)) {
                if (-1 == parse_int(&conf->gc_age_threshold, token)) {
                        ret = -1;
                        goto err;
                }
        }

        if (! strncasecmp(token, CACHE_DIR, CACHE_DIR_LEN)) {
                if (-1 == parse_str(&conf->cache_dir, token)) {
                        ret = -1;
                        goto err;
                }
                if (-1 == conf_set_full_cache_dir(conf)) {
                        fprintf(stderr, "can't build the cache directory\n");
                        ret = -1;
                        goto err;
                }
        }

        if (! strncasecmp(token, EXCLUSION_PATTERN, EXCLUSION_PATTERN_LEN)) {
                if (-1 == parse_str(&conf->regex.str, token)) {
                        ret = -1;
                        goto err;
                }
        }

        if (! strncasecmp (token, LOG_LEVEL, LOG_LEVEL_LEN)) {
                if (-1 == parse_str(&log, token)) {
                        fprintf(stderr, "can't parse log_level line: \"%s\"\n",
                                token);
                        ret = -1;
                        goto err;
                }
                conf->log_level = str_to_log_level(log);

                if (conf->log_level > LOG_DEBUG ||
                    conf->log_level < LOG_EMERG) {
                        fprintf(stderr, "invalid debug level (%d), set to %d",
                                conf->log_level, DEFAULT_LOG_LEVEL);
                        conf->log_level = DEFAULT_LOG_LEVEL;
                }
        }

        ret = 0;
  err:
        return ret;
}

static int
parse_line(struct conf *conf,
           char *line)
{
        int ret;

        if (! line) {
                fprintf(stderr, "NULL line\n");
                ret = -1;
                goto end;
        }

        /* empty line */
        if (! *line) {
                ret = 0;
                goto end;
        }

        /* skip the leading spaces */
        while (line && isspace(*line))
                line++;

        /* nothing but spaces on this line */
        if (! line) {
                fprintf(stderr, "empty line\n");
                ret = 0;
                goto end;
        }

        /* this line is a comment */
        if ('#' == *line) {
                ret = 0;
                goto end;
        }

        ret = parse_token(conf, line);
  end:
        return ret;
}

static int
parse_file(struct conf * conf,
           const char * const path)
{
        FILE *fp = NULL;
        char *buf = NULL;
        int ret;

        fp = fopen(path, "r");
        if (! fp) {
                fprintf(stderr, "No config file, use the factory settings\n");
                ret = -1;
                goto err;
        }

        buf = tmpstr_new();
        while (fgets(buf, TMPSTR_MAX, fp)) {
                if (*buf)
                        buf[strlen(buf) - 1] = 0;
                if (-1 == parse_line(conf, buf)) {
                        fprintf(stderr, "invalid line: \"%s\"\n", buf);
                        ret = -1;
                        goto err;
                }
        }

        ret = 0;
  err:
        return ret;
}

static void
conf_ctor_default(struct conf *conf,
                  char *root_dir)
{
        conf_set_root_dir(conf, root_dir);

        conf->cache_dir = strdup(DEFAULT_CACHE_DIR);
        if (! conf->cache_dir) {
                perror("strdup");
                exit(EXIT_FAILURE);
        }

        conf->compression_method = strdup(DEFAULT_COMPRESSION_METHOD);
        if (! conf->compression_method) {
                perror("strdup");
                exit(EXIT_FAILURE);
        }

        conf->zlib_level = DEFAULT_ZLIB_LEVEL;
        conf->gc_loop_delay = DEFAULT_GC_LOOP_DELAY;
        conf->gc_age_threshold = DEFAULT_GC_AGE_THRESHOLD;
        conf->max_retry = DEFAULT_MAX_RETRY;
        conf->log_level = DEFAULT_LOG_LEVEL;
        re_ctor(&conf->regex, NULL, REG_EXTENDED);
}

int
conf_ctor(struct conf *conf,
          char *root_dir,
          int debug)
{
        int ret;
        char *path = NULL;
        char *tmp = NULL;

        conf_ctor_default(conf, root_dir);

        tmp = getenv("HOME");
        if (! tmp) {
                fprintf(stderr, "no HOME is set, use the default config\n");
                ret = -1;
                goto end;
        }

        path = tmpstr_printf("%s/%s", tmp, DEFAULT_CONFIG_FILE);

        if (-1 == parse_file(conf, path)) {
                ret = -1;
                goto end;
        }

        ret = 0;
  end:
        env_override_conf(conf);
        conf->debug = debug;

        return ret;
}
