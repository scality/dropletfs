#include <time.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "tmpstr.h"
#include "log.h"
#include "hash.h"
#include "cachedir.h"

extern struct conf *conf;

static void
cachedir_callback(gpointer key,
                  gpointer value,
                  gpointer user_data)
{
        return;
}



void *
thread_cachedir(void *cb_arg)
{
        GHashTable *hash = cb_arg;

        LOG(LOG_DEBUG, "entering thread");

        while (1) {
                /* TODO: 10 should be a parameter */
                sleep(10);
                g_hash_table_foreach(hash, cachedir_callback, hash);
        }

        return NULL;
}
