#include "glob.h"


static int
fuse_filler(void *buf,
            const char *name,
            const struct stat *stbuf,
            off_t off)
{
        return 0;
}


int
dfs_readdir(const char *path,
            void *data,
            fuse_fill_dir_t fill,
            off_t offset,
            struct fuse_file_info *info)
{
        LOG("path=%s, data=%p, fill=%p, offset=%lld, info=%p",
            path, data, (void *)fill, (long long)offset, (void *)info);

        void *dir_hdl;
        dpl_dirent_t dirent;

        dpl_status_t rc = dpl_chdir(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_chdir, rc, path);

        rc = dpl_opendir(ctx, ".", &dir_hdl);
        DPL_CHECK_ERR(dpl_opendir, rc, ".");

        while (DPL_SUCCESS == dpl_readdir(dir_hdl, &dirent)) {
                if (0 != fill(data, dirent.name, NULL, 0))
                        break;
        }

        dpl_closedir(dir_hdl);

        return 0;
}
