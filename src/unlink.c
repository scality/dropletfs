#include <droplet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h>

#include "unlink.h"
#include "hash.h"
#include "log.h"
#include "tmpstr.h"
#include "timeout.h"

extern GHashTable *hash;
extern struct conf *conf;
extern dpl_ctx_t *ctx;

int
dfs_unlink(const char *path)
{
        dpl_status_t rc = DPL_FAILURE;
        char *local = NULL;
        pentry_t *pe = NULL;
        int ret;
        char *p = NULL;
        pentry_t *pe_dir = NULL;
        char *dirname = NULL;

        LOG(LOG_DEBUG, "path=%s", path);

        rc = dfs_unlink_timeout(ctx, path);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_unlink_timeout: %s", dpl_status_str(rc));
                ret = 1;
                goto end;
        }

        local = tmpstr_printf("%s/%s", conf->cache_dir, path);
        if (-1 == unlink(local))
                LOG(LOG_INFO, "unlink cache file (%s): %s",
                    local, strerror(errno));

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_ERR, "%s; entry not found in the hashtable", path);
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

        dirname = (char *)path;
        p = strrchr(dirname, '/');
        if (! p) {
                LOG(LOG_ERR, "%s: no root dir", path);
        } else {
                if (p == path)
                        dirname = "/";
                else
                        *p = 0;

                pe_dir = g_hash_table_lookup(hash, dirname);

                if (! *p)
                        *p = '/';

                if (pe_dir)
                        (void)pentry_remove_dirent(pe_dir, path);
        }

        ret = 0;
  end:
        return ret;
}
