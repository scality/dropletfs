#include <droplet.h>

#include "mkdir.h"
#include "log.h"
#include "glob.h"

int
dfs_mkdir(const char *path,
          mode_t mode)
{
        LOG("path=%s, mode=0x%x", path, (int)mode);

        dpl_status_t rc = dpl_mkdir(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_mkdir failed: %s", dpl_status_str(rc));
                return rc;
        }

        return 0;
}
