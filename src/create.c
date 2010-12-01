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
        int ret = -1;

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

        dpl_dict_t *dict = dpl_dict_new(13);
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;

        ret = dfs_open(path, info);
        if (ret < 0) {
                ret = -1;
                goto err;
        }

        ret = 0;

        struct pentry *pe = (struct pentry *)info->fh;
        struct stat st;
        if (-1 == fstat(pe->fd, &st)) {
                ret = -errno;
                goto err;
        }

        fchmod(pe->fd, mode);

        fill_metadata_from_stat(dict, &st);
        assign_meta_to_dict(dict, "mode", &mode);

        rc = dpl_openwrite(ctx,
                           (char *)path,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           0,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                ret = -1;
                goto err;
        }

        if (vfile)
                rc = dpl_close(vfile);

  err:
        dpl_dict_free(dict);
        LOG("exiting function (return value=%d)", ret);

        return ret;
}
