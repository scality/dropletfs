#include "file.h"

int
dfs_rename(const char *oldpath,
           const char *newpath)
{
        LOG("%s -> %s", oldpath, newpath);

        struct get_data get_data = { .fd = -1, .buf = NULL, .fp = NULL };
        dpl_vfile_t *vfile = NULL;
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_dict_t *dict = NULL;
        dpl_status_t rc = dpl_openread(ctx,
                                       (char *)oldpath,
                                       0u,
                                       NULL,
                                       cb_get_buffered,
                                       &get_data,
                                       &dict);

        if (DPL_FAILURE == rc)
                goto failure;

        size_t size = 0;
        char *str_size = dpl_dict_get_value(dict, "size");
        if (str_size)
                size = (size_t)strtoul(str_size, NULL, 10);

        rc = dpl_openwrite(ctx,
                           (char *)newpath,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           size,
                           &vfile);

        if (DPL_FAILURE == rc)
                goto failure;

        return dfs_unlink(oldpath);;

failure:
        LOG("%s (%d)", dpl_status_str(rc), rc);
        return -1;
}
