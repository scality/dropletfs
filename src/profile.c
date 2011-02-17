#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <errno.h>
#include <stdlib.h>

#define __USE_GNU
#include <dlfcn.h>
#undef __USE_GNU

#include "profile.h"
#include "conf.h"
#include "utils.h"
#include "log.h"

extern struct conf *conf;

static FILE *fp_log;
static int log_started = 0;
static int depth = 1;
static __thread struct timeval tv_enter;
static __thread struct timeval tv_exit;
static GHashTable *symbols;

/* symbol entry in the hashtable */
struct sentry {
        char *name;
        unsigned int n_calls;
        unsigned int sum_duration;
};

static struct sentry *
sentry_new(void)
{
        struct sentry *se = NULL;

        se = malloc(sizeof *se);
        if (! se) {
                LOG(LOG_ERR, "malloc: %s", strerror(errno));
                return NULL;
        }

        se->name = NULL;
        se->n_calls = 0;
        se->sum_duration = 0;

        return se;
}

static void
sentry_free(struct sentry *se)
{
        if (! se)
                return;

        if (se->name)
                free(se->name);

        free(se);
}

static void __attribute__((no_instrument_function))
__cyg_profile_func_enter(void *this,
                         void *call_site)
{
        Dl_info info;
        struct sentry *se = NULL;
        char *key = NULL;

        if (! log_started || ! conf->profiling)
                return;

        dladdr(__builtin_return_address(0), &info);

        if (! info.dli_sname)
                return;

        if (strncasecmp("dfs_", info.dli_sname, 4))
                return;

        gettimeofday(&tv_enter, NULL);

        se = g_hash_table_lookup(symbols, info.dli_sname);
        if (! se) {
                key = strdup(info.dli_sname);
                if (! key) {
                        LOG(LOG_ERR, "strdup(%s): %s",
                            info.dli_sname, strerror(errno));
                } else {
                        se = sentry_new();
                        se->name = strdup(key);
                        if (! se->name) {
                                LOG(LOG_ERR, "strdup(%s): %s",
                                    info.dli_sname, strerror(errno));
                                sentry_free(se);
                        } else {
                                LOG(LOG_DEBUG, "%s: insertion in symbol hashtable", key);
                                g_hash_table_insert(symbols, key, se);
                        }
                }
        }

        fprintf(fp_log, "%d.%06d %.*s %s@%p\n",
                (int)tv_enter.tv_sec, (int)tv_enter.tv_usec,
                depth, ">", info.dli_sname, this);
        depth++;

}

static void __attribute__((no_instrument_function))
__cyg_profile_func_exit(void *this,
                        void *call_site)
{
        Dl_info info;
        struct sentry *se = NULL;
        int tdiff = 0;

        if (! log_started || ! conf->profiling)
                return;

        if (! fp_log)
                return;

        dladdr(__builtin_return_address(0), &info);

        if (! info.dli_sname)
                return;

        if (strncasecmp("dfs_", info.dli_sname, 4))
                return;

        gettimeofday(&tv_exit, NULL);
        tdiff = time_diff(&tv_enter, &tv_exit);

        depth--;
        fprintf(fp_log, "%d.%06d %.*s %s@%p -- %dms\n",
                (int)tv_exit.tv_sec, (int)tv_exit.tv_usec,
                depth, "<", info.dli_sname, this, tdiff);

        se = g_hash_table_lookup(symbols, info.dli_sname);
        if (se) {
                se->n_calls++;
                se->sum_duration += tdiff;
        }
}

void
profile_init(void)
{
        if (conf->profiling) {
                fp_log = fopen(conf->profiling_logfile, "a");
                if (! fp_log) {
                        LOG(LOG_ERR, "fopen(%s): %s",
                            conf->profiling_logfile, strerror(errno));
                        return;
                }

                log_started = 1;
                symbols = g_hash_table_new_full(g_str_hash, g_str_equal, free,
                                                (GDestroyNotify)sentry_free);
        }
}

static void
cb_hash_report(gpointer key,
               gpointer value,
               gpointer user_data)
{
        (void)key;
        (void)user_data;
        struct sentry *se = value;

        LOG(LOG_DEBUG, "dumping stats for function '%s'", se->name);
        fprintf(fp_log, "symbol %s: #calls: %d, average call duration: %dms\n",
                se->name, se->n_calls,
                se->n_calls ? se->sum_duration / se->n_calls : 0);
}

void
profile_fini(void)
{
        LOG(LOG_DEBUG, "");
        if (fp_log) {
                LOG(LOG_DEBUG, "dumping profiling statistics");
                if (symbols)  {
                        fprintf(fp_log, "\n\t\t\t\t-- report --\n");
                        g_hash_table_foreach(symbols, cb_hash_report, NULL);
                }
                fclose(fp_log);
        }

        if (symbols) {
                LOG(LOG_DEBUG, "releasing symbols hashtable");
                g_hash_table_remove_all(symbols);
        }
}
