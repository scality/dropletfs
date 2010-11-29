#include "glob.h"

int
dfs_statfs(const char *path, struct statvfs *buf)
{
        LOG("path=%s, buf=%p", path, (void *)buf);

        buf->f_flag = ST_RDONLY;
        buf->f_namemax = 255;
        buf->f_bsize = 4096;
        buf->f_frsize = buf->f_bsize;
        buf->f_blocks = buf->f_bfree = buf->f_bavail =
                (1000ULL * 1024) / buf->f_frsize;
        buf->f_files = buf->f_ffree = 1000000000;

        return 0;
}
