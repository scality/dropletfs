#include "glob.h"

int
dfs_rmdir(const char *path)
{
        LOG("path=%s", path);

        dpl_status_t rc = dpl_rmdir(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_rmdir, rc, path);

        return 0;
}
