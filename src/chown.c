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
        return 0;
        dpl_dict_t *metadata = NULL;
        dpl_status_t rc;
        int ret;

        LOG(LOG_DEBUG, "%s, uid=%lu, gid=%lu",
            path, (unsigned long)uid, (unsigned long)gid);

        rc = dfs_getattr_timeout(ctx, path, &metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_getattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (! metadata)
                metadata = dpl_dict_new(13);

        assign_meta_to_dict(metadata, "uid", (unsigned long)uid);
        assign_meta_to_dict(metadata, "gid", (unsigned long)gid);

        rc = dfs_setattr_timeout(ctx, path, metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_setattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        pe = g_hash_table_lookup(hash, path);
        if (pe)
                pentry_set_metadata(pe, metadata);

        ret = 0;
  err:
        if (metadata)
                dpl_dict_free(metadata);

        return ret;
}
