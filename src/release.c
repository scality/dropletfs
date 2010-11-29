#include "glob.h"

int
dfs_release(const char *path,
            struct fuse_file_info *info)
{
        int fd = ((struct pentry *)info->fh)->fd;
        LOG("%s, fd=%d", path, fd);

        if (-1 != fd)
                close(fd);

        return 0;
}
