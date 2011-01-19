#include <droplet.h>

#include "glob.h"
#include "chown.h"
#include "log.h"
#include "metadata.h"

int
dfs_chown(const char *path,
          uid_t uid,
          gid_t gid)
{
        dpl_dict_t *metadata = NULL;
        dpl_status_t rc = DPL_FAILURE;

        LOG(LOG_DEBUG, "%s", path);

        rc = dpl_getattr(ctx, (char *)path, &metadata);

        if (DPL_FAILURE == rc)
                goto failure;

        if (! metadata)
                return 0;

        assign_meta_to_dict(metadata, "uid", &uid);
        assign_meta_to_dict(metadata, "gid", &gid);
        rc = dpl_setattr(ctx, (char *)path, metadata);

        if (metadata)
                dpl_dict_free(metadata);

        if (DPL_FAILURE == rc)
                goto failure;

        return 0;

failure:
        LOG(LOG_ERR, "dpl_getattr: %s (%d)", dpl_status_str(rc), rc);
        return -1;
}
