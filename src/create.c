#include <fuse.h>
#include <droplet.h>
#include <errno.h>

#include "open.h"
#include "create.h"
#include "env.h"
#include "log.h"
#include "file.h"
#include "metadata.h"
#include "hash.h"

extern struct env *env;
extern dpl_ctx_t *ctx;

int
dfs_create(const char *path,
           mode_t mode,
           struct fuse_file_info *info)
{
        dpl_ftype_t type;
        dpl_ino_t ino, obj, parent;
        int ret = -1;
        dpl_status_t rc = DPL_FAILURE;
        struct pentry *pe = NULL;
        struct stat st;
        int fd = -1;
        dpl_dict_t *meta = NULL;
        int tries = 0;
        int delay = 1;

        LOG(LOG_DEBUG, "%s, mode=0x%x", path, (unsigned)mode);
        PRINT_FLAGS(path, info);

        if (! S_ISREG(mode)) {
                LOG(LOG_ERR, "%s: not a regular file", path);
                ret = -1;
                goto err;
        }

        ino = dpl_cwd(ctx, ctx->cur_bucket);

 namei_retry:
        rc = dpl_namei(ctx, (char *)path, ctx->cur_bucket,
                       ino, &parent, &obj, &type);

        LOG(LOG_DEBUG, "path=%s, ino=%s, parent=%s, obj=%s, type=%s, rc=%s",
            path, ino.key, parent.key, obj.key, dfs_ftypetostr(type),
            dpl_status_str(rc));

        if (DPL_SUCCESS != rc && DPL_ENOENT != rc) {
                if (tries < env->max_retry) {
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        LOG(LOG_NOTICE, "namei timeout? (%s)",
                            dpl_status_str(rc));
                        goto namei_retry;
                }
                /* TODO handle the following cases here:
                 *   - we overwrite the current file
                 *   - we do not have permissions to do so
                 */
                LOG(LOG_ERR, "dpl_namei: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        (void)dfs_open(path, info);

        pe = (pentry_t *)info->fh;

        fd = pentry_get_fd(pe);
        if (-1 == fd) {
                ret = -1;
                goto err;
        }

        if (-1 == fstat(fd, &st)) {
                LOG(LOG_ERR, "fstat: %s", strerror(errno));
                ret = -errno;
                goto err;
        }

        fchmod(fd, mode);

        meta = pentry_get_metadata(pe);
        if (! meta) {
                LOG(LOG_DEBUG, "NULL metadata associated with fd=%d", fd);
                goto err;
        }

        fill_metadata_from_stat(meta, &st);
        assign_meta_to_dict(meta, "mode", &mode);

        delay = 1;
        tries = 0;
  retry:
        rc = dpl_mknod(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                if ((tries < env->max_retry)) {
                        LOG(LOG_NOTICE, "mknod: timeout? (%s)",
                            dpl_status_str(rc));
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        goto retry;
                }
                LOG(LOG_ERR, "dpl_mknod (%s)", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        LOG(LOG_DEBUG, "exiting function (return value=%d)", ret);
        return ret;
}
