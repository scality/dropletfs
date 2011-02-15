#include <droplet.h>

#include "mkdir.h"
#include "log.h"
#include "timeout.h"

extern dpl_ctx_t *ctx;

int
dfs_mkdir(const char *path,
          mode_t mode)
{
        dpl_status_t rc;
        int ret;

        LOG(LOG_DEBUG, "path=%s, mode=0x%x", path, (int)mode);

        rc = dfs_mkdir_timeout(ctx, path);
        if (DPL_SUCCESS != rc && DPL_ENOENT != rc) {
                LOG(LOG_ERR, "dfs_mkdir_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
 err:
        return ret;
}
