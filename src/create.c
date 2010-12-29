#include "glob.h"
#include "file.h"

int
dfs_create(const char *path,
           mode_t mode,
           struct fuse_file_info *info)
{
        LOG("%s, mode=0x%x", path, (unsigned)mode);

        PRINT_FLAGS(path, info);
        dpl_ftype_t type;
        dpl_ino_t ino, obj, parent;
        int ret = 0;

        if (! S_ISREG(mode)) {
                LOG("%s: not a regular file", path);
                ret = -1;
                goto err;
        }

        ino = dpl_cwd(ctx, ctx->cur_bucket);
        dpl_status_t rc = dpl_namei(ctx, (char *)path, ctx->cur_bucket,
                                    ino, &parent, &obj, &type);

        LOG("path=%s, ino=%s, parent=%s, obj=%s, type=%s, rc=%s (%d)",
            path, ino.key, parent.key, obj.key, dfs_ftypetostr(type),
            dpl_status_str(rc), rc);
        if (DPL_ENOENT != rc) {
                /* TODO handle the following cases here:
                 *   - we overwrite the current file
                 *   - we do not have permissions to do so
                 */
                LOG("dpl_namei: %s (%d)", dpl_status_str(rc), rc);
                return -1;
        }

        (void)dfs_open(path, info);

        struct pentry *pe = (struct pentry *)info->fh;

        struct stat st;
        if (-1 == fstat(pe->fd, &st)) {
                LOG("fstat failed: %s", strerror(errno));
                ret = -errno;
                goto err;
        }

        fchmod(pe->fd, mode);

        fill_metadata_from_stat(pe->metadata, &st);
        assign_meta_to_dict(pe->metadata, "mode", &mode);

        rc = dpl_mknod(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_mknod failed (%s)", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

  err:
        LOG("exiting function (return value=%d)", ret);
        return ret;
}
