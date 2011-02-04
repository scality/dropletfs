#include <droplet.h>

#include "chmod.h"
#include "log.h"
#include "metadata.h"
#include "timeout.h"

extern dpl_ctx_t *ctx;

int
dfs_chmod(const char *path,
          mode_t mode)
{
        dpl_dict_t *metadata = NULL;
        dpl_status_t rc;
        int ret;

        LOG(LOG_DEBUG, "%s", path);

        rc = dfs_getattr_timeout(ctx, path, &metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_getattr_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (! metadata)
                metadata = dpl_dict_new(13);

        assign_meta_to_dict(metadata, "mode", &mode);

        rc = dfs_setattr_timeout(ctx, path, metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_setattr_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        if (metadata)
                dpl_dict_free(metadata);

        return ret;
}
