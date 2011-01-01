#include <droplet.h>

#include "mknod.h"
#include "glob.h"
#include "log.h"

int
dfs_mknod(const char *path,
          mode_t mode,
          dev_t dev)
{
        dpl_status_t rc = DPL_FAILURE;

        LOG("%s, mode=0x%X", path, (unsigned)mode);

        rc = dpl_mknod(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_mknod: %s", dpl_status_str(rc));
                return rc;
        }

        return 0;
}
