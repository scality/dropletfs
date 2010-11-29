#include "glob.h"

int
dfs_unlink(const char *path)
{
        LOG("path=%s", path);

        dpl_status_t rc = dpl_unlink(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_unlink, rc, path);

        return 0;
}
