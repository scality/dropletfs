#include <droplet.h>

#include "glob.h"
#include "log.h"
#include "rmdir.h"

int
dfs_rmdir(const char *path)
{
        LOG("path=%s", path);

        dpl_status_t rc = dpl_rmdir(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_rmdir failed: %s", dpl_status_str(rc));
                return rc;
        }

        return 0;
}
