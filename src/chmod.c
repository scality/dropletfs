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
        return 0;
        dpl_dict_t *metadata = NULL;
        dpl_status_t rc;
        int ret;

        LOG(LOG_DEBUG, "%s", path);

        rc = dfs_getattr_timeout(ctx, path, &metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_getattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (! metadata)
                metadata = dpl_dict_new(13);

        assign_meta_to_dict(metadata, "mode", (unsigned long)mode);

        rc = dfs_setattr_timeout(ctx, path, metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_setattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        if (metadata)
                dpl_dict_free(metadata);

        return ret;
}
