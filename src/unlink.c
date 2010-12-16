#include "glob.h"

int
dfs_unlink(const char *path)
{
        LOG("path=%s", path);

        dpl_status_t rc = dpl_unlink(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_unlink, rc, path);

        char local[4096] = "";
        snprintf(local, sizeof local, "/tmp/%s/%s", ctx->cur_bucket, path);
        if (-1 == unlink(local))
                LOG("unlink cache file(%s): %s", local, strerror(errno));

        g_hash_table_remove(hash, (char *)path);

        return 0;
}
