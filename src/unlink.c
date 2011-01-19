#include <droplet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h>

#include "unlink.h"
#include "hash.h"
#include "glob.h"
#include "log.h"
#include "tmpstr.h"

extern GHashTable *hash;
extern char *cache_dir;

int
dfs_unlink(const char *path)
{
        dpl_status_t rc = DPL_FAILURE;
        char *local = NULL;
        pentry_t *pe = NULL;
        int ret;

        LOG(LOG_DEBUG, "path=%s", path);

        rc = dpl_unlink(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_unlink: %s", dpl_status_str(rc));
                ret = 1;
                goto end;
        }

        local = tmpstr_printf("%s/%s", cache_dir, path);
        if (-1 == unlink(local))
                LOG(LOG_INFO, "unlink cache file (%s): %s",
                    local, strerror(errno));

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_ERR, "path entry not found");
                ret = 0;
                goto end;
        }

        if (FALSE == g_hash_table_remove(hash, path))
                /* Ok, we can't lighten the hastable from this entry,
                 * but we successfully removed the local file, so, from
                 * a filesystem point of view, we do not want to return
                 * an error
                 */
                LOG(LOG_NOTICE, "%s: can't remove the cell from the hashtable",
                    path);

        ret = 0;
 end:
        return ret;
}
