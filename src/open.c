#include "glob.h"
#include "file.h"

int
dfs_open(const char *path,
         struct fuse_file_info *info)
{
        LOG("%s", path);

        struct pentry *pentry = NULL;

        PRINT_FLAGS(path, info);
        pentry = g_hash_table_find(hash, pentry_cmp_callback, (char *)path);
        if (! pentry) {
                pentry = pentry_new();
                pentry_ctor(pentry, -1);
        }

        /* open in order to write on remote fs */
        if (O_WRONLY == (info->flags & O_ACCMODE)) {
                /* TODO: handle the case where we overwrite a file */

        } else {
                /* otherwise we simply want to read the file */
                if (-1 == pentry->fd) {
                        pentry->fd = dfs_get_local_copy(ctx, path);
                        LOG("info->fh = dfs_get_local_copy(...)");
                }
        }

        info->fh = (uint64_t)pentry;

        LOG("open @pentry=%p, fd=%d, flags=0x%X",
            pentry, pentry->fd, info->flags);
        return 0;
}
