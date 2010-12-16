#include "glob.h"

int
dfs_release(const char *path,
            struct fuse_file_info *info)
{
        struct pentry *pe = (struct pentry *)info->fh;
        if (! pe)
                return 0;

        LOG("%s, fd=%d", path, pe->fd);

        if (-1 != pe->fd)
                close(pe->fd);

        return 0;
}
