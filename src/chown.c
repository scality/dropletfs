#include <droplet.h>

#include "chown.h"
#include "log.h"
#include "metadata.h"
#include "timeout.h"
#include "tmpstr.h"

extern dpl_ctx_t *ctx;

int
dfs_chown(const char *path,
          uid_t uid,
          gid_t gid)
{
        dpl_dict_t *metadata = NULL;
        dpl_status_t rc;
        int ret;
        char *buf = NULL;

        LOG(LOG_DEBUG, "%s, uid=%llu, gid=%llu",
            path, (unsigned long long)uid, (unsigned long long)gid);

        rc = dfs_getattr_timeout(ctx, path, &metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_getattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (! metadata)
                metadata = dpl_dict_new(13);

        buf = tmpstr_printf("%llu", (unsigned long long)uid);
        if (DPL_SUCCESS != dpl_dict_update_value(metadata, "uid", buf)) {
                ret = -1;
                goto err;
        }

        buf = tmpstr_printf("%llu", (unsigned long long)gid);
        if (DPL_SUCCESS != dpl_dict_update_value(metadata, "gid", buf)) {
                ret = -1;
                goto err;
        }

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
