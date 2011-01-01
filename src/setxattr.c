#include <droplet.h>

#include "glob.h"
#include "log.h"
#include "setxattr.h"

int
dfs_setxattr(const char *path,
             const char *name,
             const char *value,
             size_t size,
             int flag)
{
        LOG("path=%s, name=%s, value=%s, size=%zu, flag=%d",
            path, name, value, size, flag);

        dpl_dict_t *metadata = NULL;

        dpl_status_t rc = dpl_dict_add(metadata, (char *)name, (char *)value, 0);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_dict_add failed: %s", dpl_status_str(rc));
                return rc;
        }

        rc = dpl_setattr(ctx, (char *)path, metadata);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_setattr failed: %s", dpl_status_str(rc));
                return rc;
        }

        return 0;
}
