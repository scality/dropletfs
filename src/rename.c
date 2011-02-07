#include <droplet.h>

#include "rename.h"
#include "unlink.h"
#include "file.h"
#include "log.h"
#include "timeout.h"

extern dpl_ctx_t *ctx;

static int
rename_file(const char *oldpath,
            const char *newpath)
{
        int ret;
        dpl_status_t rc;

        rc = dfs_fcopy_timeout(ctx, oldpath, newpath);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_fcopy_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = dfs_unlink(oldpath);
  err:
        return ret;
}

static int
rename_directory(const char *oldpath,
                 const char *newpath)
{
        LOG(LOG_ERR, "directory rename not supported yet");
        return -1;
}

int
dfs_rename(const char *oldpath,
           const char *newpath)
{
        dpl_status_t rc;
        dpl_ftype_t type;
        dpl_ino_t ino;
        char *p = NULL;
        int ret = 0;

        LOG(LOG_DEBUG, "%s -> %s", oldpath, newpath);

        if (0 == strcmp(newpath, ".")) {
                p = strrchr(oldpath, '/');
                newpath = p ? p + 1 : oldpath;
        }

        rc = dfs_namei_timeout(ctx, oldpath, ctx->cur_bucket,
                               ino, NULL, NULL, &type);
        if (! DPL_SUCCESS == rc && (DPL_ENOENT != rc)) {
                LOG(LOG_ERR, "dpl_namei: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (DPL_FTYPE_DIR != type && DPL_FTYPE_REG != type) {
                LOG(LOG_ERR, "unsupported: '%s'", ftype_to_str(type));
                ret = -1;
                goto err;
        }

        int (*cb[])(const char *, const char *) = {
                [DPL_FTYPE_REG] = rename_file,
                [DPL_FTYPE_DIR] = rename_directory,
        };

        if (-1 == cb[type](oldpath, newpath)) {
                ret = -1;
                goto err;
        }

  err:
        return ret;
}
