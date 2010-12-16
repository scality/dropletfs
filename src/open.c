#include "glob.h"
#include "file.h"
#include "tmpstr.h"

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
                pentry_set_fd(pe, -1);
        }

        info->fh = (uint64_t)pe;
        info->direct_io = 1;

        /* open in order to write on remote fs */
        if (O_WRONLY == (info->flags & O_ACCMODE)) {
                /* TODO: handle the case where we overwrite a file */
                char *file = tmpstr_printf("/tmp/%s/%s", ctx->cur_bucket, path);
                LOG("opening cache file '%s'", file);
                pe->fd = open(file, O_RDWR|O_CREAT|O_TRUNC, 0644);
                if (-1 == pe->fd) {
                        LOG("%s: %s", file, strerror(errno));
                        goto err;
                }
                g_hash_table_insert(hash, (char *)path, pe);
        } else {
                /* otherwise we simply want to read the file */
                if (-1 == pe->fd)
                        pe->fd = dfs_get_local_copy(ctx, pe, path);
        }

  err:
        LOG("@pentry=%p, fd=%d, flags=0x%X", pe, pe->fd, info->flags);
        return 0;
}
