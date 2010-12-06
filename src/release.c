#include "glob.h"

int
dfs_release(const char *path,
            struct fuse_file_info *info)
{
        struct pentry *pe = info->fh;
        if (! pe)
                return 0;

        LOG("%s, fd=%d", path, pe->fd);

        if (-1 != pe->fd)
                close(pe->fd);

/*         char *local = tmpstr_printf("/tmp/%s/%s", ctx->cur_bucket, path); */
/*         if (-1 == unlink(local)) */
/*                 LOG("unlink cache file(%s): %s", local); */

/*         g_hash_table_remove(hash, (char *)path); */

        return 0;
}
