#include <glib.h>
#include <droplet.h>

#include "readdir.h"
#include "log.h"
#include "timeout.h"
#include "hash.h"
#include "list.h"

extern dpl_ctx_t *ctx;
extern GHashTable *hash;

int
dfs_readdir(const char *path,
            void *data,
            fuse_fill_dir_t fill,
            off_t offset,
            struct fuse_file_info *info)
{
        void *dir_hdl;
        dpl_dirent_t dirent;
        dpl_status_t rc = DPL_FAILURE;
        pentry_t *pe = NULL;
        int ret;

        LOG(LOG_DEBUG, "path=%s, data=%p, fill=%p, offset=%lld, info=%p",
            path, data, (void *)fill, (long long)offset, (void *)info);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_ERR, "%s: can't find any entry in hashtable", path);
                ret = -1;
                goto err;

        }

        rc = dfs_chdir_timeout(ctx, path);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_chdir_timeout: %s", dpl_status_str(rc));
                ret = rc;
                goto err;
        }

        rc = dfs_opendir_timeout(ctx, ".", &dir_hdl);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_opendir_timeout: %s", dpl_status_str(rc));
                ret = rc;
                goto err;
        }

        while (DPL_SUCCESS == dpl_readdir(dir_hdl, &dirent)) {
                if (0 != fill(data, dirent.name, NULL, 0))
                        break;
        }

        dpl_closedir(dir_hdl);

        ret = 0;
  err:
        return ret;
}
