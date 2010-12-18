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
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;
        dpl_status_t rc = DPL_FAILURE;
        dpl_dict_t *dict = NULL;
        struct stat st;
        int ret = 0;

        LOG("path=%s, buf=%p, size=%zu, offset=%lld, fd=%d",
            path, (void *)buf, size, (long long)offset, pe->fd);

        ret = pwrite(pe->fd, buf, size, offset);

        if (-1 == ret)
                return -errno;

        dict = dpl_dict_new(13);
        fill_metadata_from_stat(dict, &st);

        rc = dpl_openwrite(ctx,
                           (char *)path,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           st.st_size,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_openwrite: %s (%d)", dpl_status_str(rc), rc);
                ret = -1;
                goto err;
        }

        if (-1 == read_all(pe->fd, vfile)) {
                ret = -1;
                goto err;
        }

  err:
        if (dict)
                dpl_dict_free(dict);

        if (vfile)
                dpl_close(vfile);

        LOG("return value = %d", ret);
        return ret;
}
