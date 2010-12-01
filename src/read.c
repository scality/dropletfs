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

        ret = pread(fd, buf, size, offset);

        if (-1 == ret) {
                LOG("%s (fd=%d) - %s (%d)", path, fd, strerror(errno), errno);
                return -1;
        }

        LOG("%s - %d bytes read", path, ret);
        return ret;
}
