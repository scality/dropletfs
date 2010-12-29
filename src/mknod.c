#include "glob.h"

int
dfs_mknod(const char *path,
          mode_t mode)
{
        dpl_status_t rc;

        LOG("%s, mode=0x%X", path, (unsigned)mode);

        rc = dpl_mknod(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_mknod -- %s: %s", path, dpl_status_str(rc));
                return rc;
        }

        return 0;
}
