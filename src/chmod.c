#include <droplet.h>

#include "chmod.h"
#include "log.h"
#include "metadata.h"

extern dpl_ctx_t *ctx;

int
dfs_chmod(const char *path,
          mode_t mode)
{
        dpl_dict_t *metadata = NULL;
        dpl_status_t rc = DPL_FAILURE;

        LOG(LOG_DEBUG, "%s", path);

        rc = dpl_getattr(ctx, (char *)path, &metadata);

        if (DPL_FAILURE == rc)
                goto failure;

        if (! metadata)
                metadata = dpl_dict_new(13);

        assign_meta_to_dict(metadata, "mode", &mode);
        rc = dpl_setattr(ctx, (char *)path, metadata);

        dpl_dict_free(metadata);

        if (DPL_FAILURE == rc)
                goto failure;

        return 0;

failure:
        LOG(LOG_ERR, "dpl_setattr: %s (%d)", dpl_status_str(rc), rc);
        return -1;
}
