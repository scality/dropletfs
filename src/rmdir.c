#include <droplet.h>

#include "log.h"
#include "rmdir.h"
#include "timeout.h"

extern dpl_ctx_t *ctx;

int
dfs_rmdir(const char *path)
{
        dpl_status_t rc = DPL_FAILURE;
        int ret;

        LOG(LOG_DEBUG, "path=%s", path);

        rc = dfs_rmdir_timeout(ctx, path);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_rmdir_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
 err:
        return ret;
}
