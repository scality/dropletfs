#include <errno.h>
#include <droplet.h>
#include <unistd.h>

#include "read.h"
#include "hash.h"
#include "log.h"

ssize_t pread(int, void *, size_t, off_t);

int
dfs_read(const char *path,
         char *buf,
         size_t size,
         off_t offset,
         struct fuse_file_info *info)
{
        int ret = 0;
        int fd = 0;

        fd = ((struct pentry *)info->fh)->fd;
        LOG("path=%s, buf=%p, size=%zu, offset=%lld, fd=%d",
            path, (void *)buf, size, (long long)offset, fd);

        ret = pread(fd, buf, size, offset);

        if (-1 == ret) {
                LOG("%s (fd=%d) - %s (%d)", path, fd, strerror(errno), errno);
                return -errno;
        }

        LOG("%s - %d bytes read", path, ret);
        return ret;
}
