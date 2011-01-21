#include <droplet.h>

#include "log.h"
#include "rmdir.h"

extern dpl_ctx_t *ctx;

int
dfs_rmdir(const char *path)
{
        dpl_status_t rc = DPL_FAILURE;

        LOG(LOG_DEBUG, "path=%s", path);

        rc = dpl_rmdir(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_rmdir: %s", dpl_status_str(rc));
                return rc;
        }

        return 0;
}
