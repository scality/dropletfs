#include <droplet.h>

#include "mknod.h"
#include "log.h"
#include "timeout.h"

extern dpl_ctx_t *ctx;

int
dfs_mknod(const char *path,
          mode_t mode,
          dev_t dev)
{
        dpl_status_t rc = DPL_FAILURE;
        int ret;

        LOG(LOG_DEBUG, "%s, mode=0x%X", path, (unsigned)mode);

        rc = dfs_mknod_timeout(ctx, path);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_mknod_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        return ret;
}
