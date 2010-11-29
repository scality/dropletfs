#include <unistd.h>

#include "glob.h"

int
dfs_read(const char *path,
         char *buf,
         size_t size,
         off_t offset,
         struct fuse_file_info *info)
{
        int ret = -1;
        int fd = ((struct pentry *)info->fh)->fd;
        LOG("path=%s, buf=%p, size=%zu, offset=%lld, fd=%d",
            path, (void *)buf, size, (long long)offset, fd);

        struct stat st;
        if (-1 == fstat(fd, &st))
                goto err;

        if (-1 == lseek(fd, offset, SEEK_SET))
                goto err;

        ret = read(fd, buf, size);

        if (-1 == ret)
                goto err;

        LOG("%s - successfully read %d bytes", path, ret);
        return ret;

  err:
        LOG("%s (fd=%d) - %s (%d)", path, fd, strerror(errno), errno);
        return -1;
}
