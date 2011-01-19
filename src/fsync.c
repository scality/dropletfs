#include <unistd.h>
#include <errno.h>
#include <glib.h>

#include "fsync.h"
#include "hash.h"
#include "log.h"
#include "glob.h"

extern GHashTable *hash;

int
dfs_fsync(const char *path,
          int issync,
          struct fuse_file_info *info)
{
        struct pentry *pe = NULL;
        int fd = -1;

        LOG(LOG_DEBUG, "%s", path);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_INFO, "unable to find a path entry (%s)", path);
                goto end;
        }

        fd = pentry_get_fd(pe);
        if (fd < 0) {
                LOG(LOG_ERR, "unusable file descriptor: %d", fd);
                goto end;
        }

        if (issync)
                goto end;

        if (-1 == fsync(fd)) {
                LOG(LOG_ERR, "fsync: %s", strerror(errno));
                return -errno;
        }

  end:
        return 0;
}
