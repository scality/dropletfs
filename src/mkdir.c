#include <sys/stat.h>

#include "glob.h"

int
dfs_mkdir(const char *path,
          mode_t mode)
{
        LOG("path=%s, mode=0x%x", path, (int)mode);

        dpl_status_t rc = dpl_mkdir(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_mkdir, rc, path);

        return 0;
}
