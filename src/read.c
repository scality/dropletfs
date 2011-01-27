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
        pentry_t *pe = (pentry_t *)info->fh;

        LOG(LOG_DEBUG, "path=%s, buf=%p, size=%zu, offset=%lld, info=%p",
            path, (void *)buf, size, (long long)offset, (void *)info);

        fd = pentry_get_fd(pe);
        if (fd < 0) {
                LOG(LOG_ERR, "unusable file descriptor fd=%d", fd);
                ret = -EBADFD;
                goto end;
        }

        ret = pread(fd, buf, size, offset);

        if (-1 == ret) {
                LOG(LOG_ERR, "%s (fd=%d) - %s",
                    path, fd, strerror(errno));
                ret = -errno;
                goto end;
        }

  end:
        LOG(LOG_DEBUG, "%s - %d bytes read", path, ret);
        return ret;
}
