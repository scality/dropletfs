#include "glob.h"
#include "file.h"

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

        pe->flag = FLAG_DIRTY;

        LOG("return value = %d", ret);
        return ret;
}
