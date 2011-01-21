#include <droplet.h>

#include "mkdir.h"
#include "log.h"

extern dpl_ctx_t *ctx;

int
dfs_mkdir(const char *path,
          mode_t mode)
{
        dpl_status_t rc = DPL_FAILURE;

        LOG(LOG_DEBUG, "path=%s, mode=0x%x", path, (int)mode);

        rc = dpl_mkdir(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_mkdir: %s", dpl_status_str(rc));
                return rc;
        }

        return 0;
}
