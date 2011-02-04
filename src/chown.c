#include <droplet.h>

#include "chown.h"
#include "log.h"
#include "metadata.h"
#include "timeout.h"

extern dpl_ctx_t *ctx;

int
dfs_chown(const char *path,
          uid_t uid,
          gid_t gid)
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

        if (! metadata) {
                LOG(LOG_NOTICE, "no metadata found");
                ret = -1;
                goto err;
        }

        assign_meta_to_dict(metadata, "uid", &uid);
        assign_meta_to_dict(metadata, "gid", &gid);

        rc = dfs_setattr_timeout(ctx, path, metadata);
        if (DPL_FAILURE == rc) {
                LOG(LOG_NOTICE, "dfs_setattr_timeout: %s", dpl_status_str(rc));
                ret = rc;
                goto err;
        }

        return 0;
  err:
        if (metadata)
                dpl_dict_free(metadata);

        return ret;
}
