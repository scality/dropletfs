#include "glob.h"

int
dfs_fsync(const char *path,
          int issync,
          struct fuse_file_info *info)
{
        LOG("%s", path);

        struct pentry *pe = g_hash_table_find(hash,
                                              pentry_cmp_callback,
                                              (char *)path);
        if (pe && -1 != pe->fd) {
                if (! issync && -1 == fsync(pe->fd)) {
                        LOG("%s - %s", path, strerror(errno));
                        return EIO;
                }
        }

        return 0;
}
