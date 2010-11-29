#include "glob.h"

int
dfs_opendir(const char *path,
            struct fuse_file_info *info)
{
        LOG("path=%s, info=%p", path, (void *)info);

        void *dir_hdl;
        dpl_status_t rc = dpl_opendir(ctx, (char *)path, (void *[]){NULL});
        DPL_CHECK_ERR(dpl_opendir, rc, path);

        return 0;
}
