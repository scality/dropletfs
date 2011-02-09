#include <glib.h>
#include <droplet.h>
#include <errno.h>

#include "opendir.h"
#include "log.h"
#include "timeout.h"
#include "hash.h"

extern dpl_ctx_t *ctx;
extern GHashTable *hash;

int
dfs_opendir(const char *path,
            struct fuse_file_info *info)
{
        dpl_status_t rc = DPL_FAILURE;
        pentry_t *pe = NULL;
        int ret;
        char *key = NULL;

        LOG(LOG_DEBUG, "path=%s, info=%p", path, (void *)info);

        rc = dfs_opendir_timeout(ctx, path, (void *[]){NULL});
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_opendir_timeout: %s", dpl_status_str(rc));
                ret = rc;
                goto err;
        }

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                pe = pentry_new();
                if (! pe) {
                        LOG(LOG_ERR, "can't create a new hashtable cell");
                        ret = -1;
                        goto err;
                }
                pentry_set_path(pe, path);
                pentry_set_filetype(pe, FILE_DIR);
                key = strdup(path);
                if (! key) {
                        LOG(LOG_ERR, "%s: strdup: %s", path, strerror(errno));
                        pentry_free(pe);
                        ret = -1;
                        goto err;
                }
                g_hash_table_insert(hash, key, pe);
                LOG(LOG_DEBUG, "added a new dir entry in hashtable: %s", path);
        }

        ret = 0;
  err:
        return ret;
}
