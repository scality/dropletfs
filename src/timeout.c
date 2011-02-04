#include <unistd.h>

#include "timeout.h"
#include "log.h"

dpl_status_t
dfs_getattr_timeout(dpl_ctx_t *ctx,
                    const char *path,
                    dpl_dict_t **metadata)
{
        int tries = 0;
        int delay = 1;
        dpl_status_t rc;

  retry:
        rc = dpl_getattr(ctx, (char *)path, metadata);
        if (DPL_FAILURE == rc) {
                if (DPL_ENOENT != rc && (tries < conf->max_retry)) {
                        LOG(LOG_NOTICE, "dpl_getattr: timeout? (%s)",
                            dpl_status_str(rc));
                        sleep(delay);
                        tries++;
                        delay *= 2;
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_setattr_timeout(dpl_ctx_t *ctx,
                    const char *path,
                    dpl_dict_t *metadata)
{
        int tries = 0;
        int delay = 1;
        dpl_status_t rc;

  retry:
        rc = dpl_setattr(ctx, (char *)path, metadata);
        if (DPL_FAILURE == rc) {
                if (DPL_ENOENT != rc && (tries < conf->max_retry)) {
                        LOG(LOG_NOTICE, "dpl_getattr: timeout? (%s)",
                            dpl_status_str(rc));
                        sleep(delay);
                        tries++;
                        delay *= 2;
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_mknod_timeout(dpl_ctx_t *ctx,
                  const char *path)
{
        dpl_status_t rc;
        int tries = 0;
        int delay = 1;

        LOG(LOG_DEBUG, "%s", path);

 retry:
        rc = dpl_mknod(ctx, (char *)path);
        if (DPL_SUCCESS != rc) {
                if (tries < conf->max_retry) {
                        LOG(LOG_NOTICE, "mknod: timeout? (%s)",
                            dpl_status_str(rc));
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_mkdir_timeout(dpl_ctx_t *ctx,
                  const char *path)
{
        dpl_status_t rc;
        int tries = 0;
        int delay = 1;

  retry:
        rc = dpl_mkdir(ctx, (char *)path);
        if (DPL_FAILURE == rc) {
                if (DPL_ENOENT != rc && (tries < conf->max_retry)) {
                        LOG(LOG_NOTICE, "dpl_mkdir: timeout? (%s)",
                            dpl_status_str(rc));
                        sleep(delay);
                        tries++;
                        delay *= 2;
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_unlink_timeout(dpl_ctx_t *ctx,
                   const char *path)
{
        dpl_status_t rc;
        int tries = 0;
        int delay = 1;

  retry:
        rc = dpl_unlink(ctx, (char *)path);
        if (DPL_FAILURE == rc) {
                if (DPL_ENOENT != rc && (tries < conf->max_retry)) {
                        LOG(LOG_NOTICE, "dpl_unlink: timeout? (%s)",
                            dpl_status_str(rc));
                        sleep(delay);
                        tries++;
                        delay *= 2;
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_rmdir_timeout(dpl_ctx_t *ctx,
                  const char *path)
{
        dpl_status_t rc;
        int tries = 0;
        int delay = 1;

  retry:
        rc = dpl_rmdir(ctx, (char *)path);
        if (DPL_FAILURE == rc) {
                if (DPL_ENOENT != rc && (tries < conf->max_retry)) {
                        LOG(LOG_NOTICE, "dpl_rmdir: timeout? (%s)",
                            dpl_status_str(rc));
                        sleep(delay);
                        tries++;
                        delay *= 2;
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_fcopy_timeout(dpl_ctx_t *ctx,
                  const char *oldpath,
                  const char *newpath)
{
        dpl_status_t rc;
        int tries = 0;
        int delay = 1;

  retry:
        rc = dpl_fcopy(ctx, (char *)oldpath, (char *)newpath);
        if (DPL_FAILURE == rc) {
                if (DPL_ENOENT != rc && (tries < conf->max_retry)) {
                        LOG(LOG_NOTICE, "dpl_fcopy: timeout? (%s)",
                            dpl_status_str(rc));
                        sleep(delay);
                        tries++;
                        delay *= 2;
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_chdir_timeout(dpl_ctx_t *ctx,
                  const char *path)
{
        dpl_status_t rc;
        int tries = 0;
        int delay = 1;

  retry:
        rc = dpl_chdir(ctx, (char *)path);
        if (DPL_FAILURE == rc) {
                if (DPL_EINVAL != rc && (tries < conf->max_retry)) {
                        LOG(LOG_NOTICE, "dpl_chdir: timeout? (%s)",
                            dpl_status_str(rc));
                        sleep(delay);
                        tries++;
                        delay *= 2;
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_opendir_timeout(dpl_ctx_t *ctx,
                    const char *path,
                    void **dir_hdl)
{
        dpl_status_t rc;
        int tries = 0;
        int delay = 1;

  retry:
        rc = dpl_opendir(ctx, (char *)path, dir_hdl);
        if (DPL_FAILURE == rc) {
                if (DPL_ENOENT != rc && (tries < conf->max_retry)) {
                        LOG(LOG_NOTICE, "dpl_opendir: timeout? (%s)",
                            dpl_status_str(rc));
                        sleep(delay);
                        tries++;
                        delay *= 2;
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_namei_timeout(dpl_ctx_t *ctx,
                  const char *path,
                  char *bucket,
                  dpl_ino_t ino,
                  dpl_ino_t *parent_ino,
                  dpl_ino_t *obj_ino,
                  dpl_ftype_t *type)
{
        int tries = 0;
        int delay = 1;
        dpl_status_t rc;

  retry:
        rc = dpl_namei(ctx, (char *)path, ctx->cur_bucket,
                       ino, parent_ino, obj_ino, type);

        LOG(LOG_DEBUG,
            "path=%s, dpl_namei: %s, parent_ino=%s, obj_ino=%s",
            path, dpl_status_str(rc), parent_ino->key, obj_ino->key);

        if (DPL_SUCCESS != rc) {
                if (DPL_ENOENT != rc && (tries < conf->max_retry)) {
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        LOG(LOG_NOTICE, "dpl_namei timeout? (%s)",
                            dpl_status_str(rc));
                        goto retry;
                }
        }

        return rc;
}

dpl_status_t
dfs_head_all_timeout(dpl_ctx_t *ctx,
                     char *bucket,
                     char *resource,
                     char *subresource,
                     dpl_condition_t *condition,
                     dpl_dict_t **metadatap)
{
        int tries = 0;
        int delay = 1;
        dpl_status_t rc;

  retry:
        rc = dpl_head_all(ctx,
                          bucket,
                          resource,
                          subresource,
                          condition,
                          metadatap);
        if (DPL_SUCCESS != rc) {
                if (DPL_ENOENT != rc && (tries < conf->max_retry)) {
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        LOG(LOG_NOTICE, "dpl_head_all timeout? (%s)",
                            dpl_status_str(rc));
                        goto retry;
                }
        }

        return rc;
}
