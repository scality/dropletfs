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
#include "gc.h"

extern struct conf *conf;

static void
gc_callback(gpointer key,
            gpointer value,
            gpointer user_data)
{
        GHashTable *hash = user_data;
        char *path = key;
        pentry_t *pe = value;
        int fd;
        struct stat st;
        time_t t;
        char *local = NULL;
        int refcount = 0;
        int threshold = conf->gc_age_threshold;

        assert(pe);

        refcount = pentry_get_refcount(pe);
        if (refcount)
                /* open (either r or rw), don't touch this cell */
                return;

        if (pentry_trylock(pe))
                return;

        fd = pentry_get_fd(pe);
        if (-1 == fd)
                /* nothing to do, the pentry_t cell is allocated but no
                 * file descriptor/path is affected now
                 */
                goto release;

        if (fd < 0)  {
                /* negative but not -1? it's an error*/
                LOG(LOG_ERR, "bad file descriptor (%d), remove the cell!", fd);
                goto remove;
        }

        if (-1 == fstat(fd, &st)) {
                LOG(LOG_ERR, "fstat(fd=%d, %p): %s, remove the cell",
                    fd, (void *)&st, strerror(errno));
                goto remove;
        }

        if (pentry_get_exclude(pe))
                threshold *= 10;

        t = time(NULL);
        if (t < st.st_atime + threshold &&
            t < st.st_mtime + threshold &&
            t < st.st_ctime + threshold)
                goto release;

        LOG(LOG_DEBUG, "%s file too old: now=%d, atime=%d, mtime=%d, ctime=%d",
            path, (int)t, (int)st.st_atime, (int)st.st_mtime, (int)st.st_ctime);

  remove:
        local = tmpstr_printf("%s/%s", conf->cache_dir, path);
        LOG(LOG_INFO, "removing cache file '%s'", local);
        if (-1 == unlink(local))
                LOG(LOG_ERR, "unlink(%s): %s", local, strerror(errno));

        LOG(LOG_DEBUG, "path=%s remove from the hashtable", path);
        if (FALSE == g_hash_table_remove(hash, path))
                LOG(LOG_WARNING, "can't remove the cell from the hashtable");

        return;

  release:
        (void)pentry_unlock(pe);
}

void *
thread_gc(void *cb_arg)
{
        GHashTable *hash = cb_arg;

        LOG(LOG_DEBUG, "entering thread");

        if (conf->gc_loop_delay && conf->gc_age_threshold) {
                while (1) {
                        sleep(conf->gc_loop_delay);
                        g_hash_table_foreach(hash, gc_callback, hash);
                }
        }

        return NULL;
}
