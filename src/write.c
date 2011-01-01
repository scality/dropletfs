#include <errno.h>

#include "log.h"
#include "write.h"
#include "hash.h"

ssize_t pwrite(int, const void *, size_t, off_t);

int
dfs_write(const char *path,
          const char *buf,
          size_t size,
          off_t offset,
          struct fuse_file_info *info)
{
        struct pentry *pe = (struct pentry *)info->fh;
        int ret = 0;

        LOG("path=%s, buf=%p, size=%zu, offset=%lld, fd=%d",
            path, (void *)buf, size, (long long)offset, pe->fd);

        ret = pwrite(pe->fd, buf, size, offset);

        if (-1 == ret)
                return -errno;

        LOG("return value = %d", ret);
        return ret;
}
