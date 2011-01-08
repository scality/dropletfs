#include <fuse.h>
#include <droplet.h>
#include <errno.h>

#include "open.h"
#include "create.h"
#include "glob.h"
#include "log.h"
#include "file.h"
#include "metadata.h"
#include "hash.h"

int
dfs_create(const char *path,
           mode_t mode,
           struct fuse_file_info *info)
{
        dpl_ftype_t type;
        dpl_ino_t ino, obj, parent;
        int ret = 0;
        dpl_status_t rc = DPL_FAILURE;
        struct pentry *pe = NULL;
        struct stat st;
        int fd = -1;
        dpl_dict_t *meta = NULL;

        LOG("%s, mode=0x%x", path, (unsigned)mode);
        PRINT_FLAGS(path, info);

        if (! S_ISREG(mode)) {
                LOG("%s: not a regular file", path);
                ret = -1;
                goto err;
        }

        ino = dpl_cwd(ctx, ctx->cur_bucket);
        rc = dpl_namei(ctx, (char *)path, ctx->cur_bucket,
                       ino, &parent, &obj, &type);

        LOG("path=%s, ino=%s, parent=%s, obj=%s, type=%s, rc=%s",
            path, ino.key, parent.key, obj.key, dfs_ftypetostr(type),
            dpl_status_str(rc));

        if (DPL_ENOENT != rc) {
                /* TODO handle the following cases here:
                 *   - we overwrite the current file
                 *   - we do not have permissions to do so
                 */
                LOG("dpl_namei: %s", dpl_status_str(rc));
                return -1;
        }

        (void)dfs_open(path, info);

        pe = (pentry_t *)info->fh;

        fd = pentry_get_fd(pe);
        if (-1 == fd)
                goto err;

        if (-1 == fstat(fd, &st)) {
                LOG("fstat: %s", strerror(errno));
                ret = -errno;
                goto err;
        }

        fchmod(fd, mode);

        meta = pentry_get_metadata(pe);
        if (! meta) {
                LOG("NULL metadata associated with fd=%d", fd);
                goto err;
        }

        fill_metadata_from_stat(meta, &st);
        assign_meta_to_dict(meta, "mode", &mode);

        rc = dpl_mknod(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_mknod (%s)", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

  err:
        LOG("exiting function (return value=%d)", ret);
        return ret;
}
