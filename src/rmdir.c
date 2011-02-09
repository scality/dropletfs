#include <glib.h>
#include <droplet.h>

#include "log.h"
#include "rmdir.h"
#include "timeout.h"
#include "hash.h"

extern dpl_ctx_t *ctx;
extern GHashTable *hash;

int
dfs_rmdir(const char *path)
{
        dpl_status_t rc = DPL_FAILURE;
        int ret;
        pentry_t *pe = NULL;

        LOG(LOG_DEBUG, "path=%s", path);

        rc = dfs_rmdir_timeout(ctx, path);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_rmdir_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                /* the fs should not get an error, but we have to log this
                 * incoherence! */
                LOG(LOG_ERR, "%s: entry not found in the hashtable", path);
                ret = 0;
                goto err;
        }

        (void)pentry_remove_dirent(pe, path);

        ret = 0;
 err:
        return ret;
}
