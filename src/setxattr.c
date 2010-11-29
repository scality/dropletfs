#include "glob.h"

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
        DPL_CHECK_ERR(dpl_dict_add, rc, path);

        rc = dpl_setattr(ctx, (char *)path, metadata);
        DPL_CHECK_ERR(dpl_setattr, rc, path);

        return 0;
}
