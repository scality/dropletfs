#include <fuse.h>
#include <droplet.h>
#include <errno.h>

#include "open.h"
#include "create.h"
#include "log.h"
#include "file.h"
#include "metadata.h"
#include "hash.h"
#include "regex.h"
#include "timeout.h"

extern struct conf *conf;
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
        int exclude;

        LOG(LOG_DEBUG, "%s, mode=0x%x, %s",
            path, (unsigned)mode, flags_to_str(info->flags));

        if (! S_ISREG(mode)) {
                LOG(LOG_ERR, "%s: not a regular file", path);
                ret = -1;
                goto err;
        }

        exclude = re_matcher(&conf->regex, path);

        if (! exclude) {
                ino = dpl_cwd(ctx, ctx->cur_bucket);

                rc = dfs_namei_timeout(ctx, path, ctx->cur_bucket,
                                       ino, &parent, &obj, &type);

                LOG(LOG_DEBUG, "path=%s, ino=%s, parent=%s, obj=%s, type=%s",
                    path, ino.key, parent.key, obj.key, ftype_to_str(type));

                if (DPL_SUCCESS != rc && DPL_ENOENT != rc) {
                        LOG(LOG_ERR, "dfs_namei_timeout: %s",
                            dpl_status_str(rc));
                        ret = -1;
                        goto err;
                }
        }

        (void)dfs_open(path, info);

        pe = (pentry_t *)info->fh;
        if (! pe) {
                ret = -1;
                goto err;
        }

        pentry_set_exclude(pe, exclude);

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
        if (! meta)
                meta = dpl_dict_new(13);

        fill_metadata_from_stat(meta, &st);
        assign_meta_to_dict(meta, "mode", (unsigned long)mode);
        pentry_set_metadata(pe, meta);

        if (! exclude) {
                rc = dfs_mknod_timeout(ctx, path);
                if (DPL_SUCCESS != rc) {
                        LOG(LOG_ERR, "dfs_mknod_timeout: %s", dpl_status_str(rc));
                        ret = -1;
                        goto err;
                }
        }

        ret = 0;
  err:
        if (meta)
                dpl_dict_free(meta);

        LOG(LOG_DEBUG, "exiting function (return value=%d)", ret);
        return ret;

}
