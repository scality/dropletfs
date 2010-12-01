#include "glob.h"
#include "file.h"

int
dfs_open(const char *path,
         struct fuse_file_info *info)
{
        LOG("%s", path);

        struct pentry *pe = NULL;

        PRINT_FLAGS(path, info);
        pe = g_hash_table_find(hash, pentry_cmp_callback, (char *)path);
        if (! pe) {
                pe = pentry_new();
                pentry_ctor(pe, -1);
        }

        /* open in order to write on remote fs */
        if (O_WRONLY == (info->flags & O_ACCMODE)) {
                /* TODO: handle the case where we overwrite a file */
                char local[128] = "";
                snprintf(local, sizeof local, "/tmp/%s/%s",
                         ctx->cur_bucket, path);
                pe->fd = open(local, O_WRONLY|O_CREAT|O_EXCL);
        } else {
                /* otherwise we simply want to read the file */
                if (-1 == pe->fd) {
                        pe->fd = dfs_get_local_copy(ctx, path);
                        LOG("info->fh = dfs_get_local_copy(...)");
                }
        }

        if (-1 == pe->fd)
                LOG("%s: %s", path, strerror(errno));

        info->fh = (uint64_t)pe;

        LOG("open @pentry=%p, fd=%d, flags=0x%X",
            pe, pe->fd, info->flags);
        return 0;
}
