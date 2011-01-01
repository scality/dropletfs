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
        LOG("path=%s", path);

        dpl_status_t rc = dpl_unlink(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_unlink failed: %s", dpl_status_str(rc));
                return rc;
        }

        char local[4096] = "";
        snprintf(local, sizeof local, "/tmp/%s/%s", ctx->cur_bucket, path);
        if (-1 == unlink(local))
                LOG("unlink cache file (%s): %s", local, strerror(errno));

        g_hash_table_remove(hash, (char *)path);

        return 0;
}
