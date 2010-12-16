#include "glob.h"
#include "file.h"

int
dfs_write(const char *path,
          const char *buf,
          size_t size,
          off_t offset,
          struct fuse_file_info *info)
{
        struct pentry *pe = (struct pentry *)info->fh;
        LOG("path=%s, buf=%p, size=%zu, offset=%lld, fd=%d",
            path, (void *)buf, size, (long long)offset, pe->fd);
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;
        dpl_status_t rc = DPL_FAILURE;

        if (-1 == pe->fd) {
                LOG("invalid fd... exiting");
                return;
        }

        int ret = -1;
        dpl_dict_t *dict = dpl_dict_new(13);

        struct stat st;
        if (-1 == fstat(pe->fd, &st)) {
                LOG("fstat failed: %s (%d)", strerror(errno), errno);
                ret = -errno;
                goto err;
        }

        fill_metadata_from_stat(dict, &st);
        assign_meta_to_dict(dict, "size", &size);

        if (-1 == pwrite(pe->fd, buf, size, offset)) {
                ret = -errno;
                goto err;
        }

        rc = dpl_openwrite(ctx,
                           (char *)path,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           size,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_openwrite: %s (%d)", dpl_status_str(rc), rc);
                goto err;
        }

        if (-1 == read_all(pe->fd, (char *)buf, vfile))
                goto err;

        ret = size;
  err:
        if (dict)
                dpl_dict_free(dict);

        if (vfile)
                dpl_close(vfile);

        return ret;
}
