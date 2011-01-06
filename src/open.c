#include <errno.h>

#include "open.h"
#include "log.h"
#include "glob.h"
#include "file.h"
#include "tmpstr.h"


int
dfs_open(const char *path,
         struct fuse_file_info *info)
{
        struct pentry *pe = NULL;
        char *file = NULL;

        LOG("%s", path);
        PRINT_FLAGS(path, info);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG("'%s': entry not found in hashtable", path);
                pe = pentry_new();
                pentry_set_fd(pe, -1);
        } else {
                LOG("'%s': found in the hashtable, fd=%d", path, pe->fd);
        }

        info->fh = (uint64_t)pe;
        info->direct_io = 1;

        file = build_cache_tree(path);

        if (! file) {
                LOG("build_cache_tree(%s) was unable to build a path", path);
                goto err;
        }

        /* open in order to write on remote fs */
        if (O_WRONLY == (info->flags & O_ACCMODE)) {
                /* TODO: handle the case where we overwrite a file */
                LOG("opening cache file '%s'", file);
                pe->fd = open(file, O_RDWR|O_CREAT|O_TRUNC, 0644);
                if (-1 == pe->fd) {
                        LOG("%s: %s", file, strerror(errno));
                        goto err;
                }
        } else {
                /* otherwise we simply want to read the file */
                if (-1 == pe->fd)
                        pe->fd = dfs_get_local_copy(pe, path);
        }

        g_hash_table_insert(hash, (char *)path, pe);
  err:
        LOG("@pentry=%p, fd=%d, flags=0x%X", pe, pe->fd, info->flags);
        return 0;
}
