#include <time.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "cachedir.h"
#include "tmpstr.h"
#include "log.h"
#include "hash.h"
#include "getattr.h"
#include "timeout.h"
#include "file.h"

#define MAX_CHILDREN 50

extern struct conf *conf;
extern dpl_ctx_t *ctx;

static void
update_metadata(pentry_t *pe)
{
        char *path = NULL;
        dpl_ftype_t type;
        dpl_ino_t ino, parent_ino, obj_ino;
        dpl_status_t rc;
        dpl_dict_t *metadata = NULL;

        if (pentry_md_trylock(pe))
                return;

        ino = dpl_cwd(ctx, ctx->cur_bucket);

        rc = dfs_namei_timeout(ctx, path, ctx->cur_bucket,
                               ino, &parent_ino, &obj_ino, &type);

        LOG(LOG_DEBUG, "path=%s, dpl_namei: %s, type=%s, parent_ino=%s, obj_ino=%s",
            path, dpl_status_str(rc), ftype_to_str(type),
            parent_ino.key, obj_ino.key);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_NOTICE, "dfs_namei_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        rc = dfs_getattr_timeout(ctx, path, &metadata);
        if (DPL_SUCCESS != rc && DPL_EISDIR != rc) {
                LOG(LOG_ERR, "dfs_getattr_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        if (metadata)
                pentry_set_metadata(pe, metadata);

        pentry_set_atime(pe);

  end:
        if (metadata)
                dpl_dict_free(metadata);

        (void)pentry_md_unlock(pe);
}

static void
cachedir_callback(gpointer key,
                  gpointer value,
                  gpointer user_data)
{
        static int children;
        GHashTable *hash = NULL;
        char *path = NULL;
        pentry_t *pe = NULL;
        time_t age;
        pid_t pid;

        path = key;
        pe = value;
        hash = user_data;

        age = time(NULL) - pentry_get_atime(pe);

        LOG(LOG_DEBUG, "%s, age=%d sec, children=%d", path, (int)age, children);

        if (age < 10)
                return;

        if (children > MAX_CHILDREN)
                return;

        children++;

        pid = fork();
        switch (pid) {
        case -1:
                LOG(LOG_ERR, "fork: %s", strerror(errno));
                break;
        case 0:
                update_metadata(pe);
                exit(1);
        }

        children--;

        return;
}



void *
thread_cachedir(void *cb_arg)
{
        GHashTable *hash = cb_arg;

        LOG(LOG_DEBUG, "entering thread");

        while (1) {
                LOG(LOG_DEBUG, "updating cache directories");
                g_hash_table_foreach(hash, cachedir_callback, hash);
                /* TODO: 3 should be a parameter */
                sleep(3);
        }

        return NULL;
}
