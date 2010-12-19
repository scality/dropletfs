#include "glob.h"

int
dfs_release(const char *path,
            struct fuse_file_info *info)
{
        struct pentry *pe = (struct pentry *)info->fh;
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;
        dpl_status_t rc = DPL_FAILURE;
        dpl_dict_t *dict = NULL;
        struct stat st;
        int ret = 0;

        if (! pe)
                return 0;

        LOG("%s, fd=%d", path, pe->fd);

        if (FLAG_DIRTY != pe->flag)
                goto err;

        if (-1 == fstat(pe->fd, &st))
                goto err;

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
        if (-1 != pe->fd)
                close(pe->fd);

        if (dict)
                dpl_dict_free(dict);

        if (vfile)
                dpl_close(vfile);

        return ret;
}
