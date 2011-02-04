#include <droplet.h>

#include "readdir.h"
#include "log.h"
#include "timeout.h"

extern dpl_ctx_t *ctx;

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

        LOG(LOG_DEBUG, "path=%s, data=%p, fill=%p, offset=%lld, info=%p",
            path, data, (void *)fill, (long long)offset, (void *)info);

        rc = dfs_chdir_timeout(ctx, path);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_chdir_timeout: %s", dpl_status_str(rc));
                return rc;
        }

        rc = dfs_opendir_timeout(ctx, ".", &dir_hdl);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_opendir_timeout: %s", dpl_status_str(rc));
                return rc;
        }

        while (DPL_SUCCESS == dpl_readdir(dir_hdl, &dirent)) {
                if (0 != fill(data, dirent.name, NULL, 0))
                        break;
        }

        dpl_closedir(dir_hdl);

        return 0;
}
