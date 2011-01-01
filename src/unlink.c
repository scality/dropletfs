#include <droplet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "hash.h"
#include "glob.h"
#include "log.h"
#include "unlink.h"

int
dfs_unlink(const char *path)
{
        dpl_status_t rc = DPL_FAILURE;
        char local[4096] = "";

        LOG("path=%s", path);

        rc = dpl_unlink(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_unlink: %s", dpl_status_str(rc));
                return rc;
        }

        snprintf(local, sizeof local, "/tmp/%s/%s", ctx->cur_bucket, path);
        if (-1 == unlink(local))
                LOG("unlink cache file (%s): %s", local, strerror(errno));

        g_hash_table_remove(hash, (char *)path);

        return 0;
}
