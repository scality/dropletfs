#include <errno.h>
#include <glib.h>

#include "open.h"
#include "log.h"
#include "glob.h"
#include "file.h"
#include "tmpstr.h"

extern GHashTable *hash;

int
dfs_open(const char *path,
         struct fuse_file_info *info)
{
        pentry_t *pe = NULL;
        char *file = NULL;
        int fd = -1;
        int ret = 0;

        LOG("%s", path);
        PRINT_FLAGS(path, info);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG("'%s': entry not found in hashtable", path);
                pe = pentry_new();
                pentry_set_fd(pe, -1);
        } else {
                fd = pentry_get_fd(pe);
                LOG("'%s': found in the hashtable, fd=%d", path, fd);
        }

        info->fh = (uint64_t)pe;
        info->direct_io = 1;

        file = build_cache_tree(path);

        if (! file) {
                LOG("build_cache_tree(%s) was unable to build a path", path);
                ret = -1;
                goto err;
        }

        /* open in order to write on remote fs */
        if (O_WRONLY == (info->flags & O_ACCMODE)) {
                /* TODO: handle the case where we overwrite a file */
                LOG("opening cache file '%s'", file);
                fd = open(file, O_RDWR|O_CREAT|O_TRUNC, 0644);
                if (-1 == fd) {
                        LOG("%s: %s", file, strerror(errno));
                        ret = -1;
                        goto err;
                }
                pentry_set_fd(pe, fd);
        } else {
                fd = pentry_get_fd(pe);
                /* otherwise we simply want to read the file */
                if (fd < 0) {
                        fd = dfs_get_local_copy(pe, path);
                        pentry_set_fd(pe, fd);
                }
        }

        ret = 0;
        g_hash_table_insert(hash, (char *)path, pe);

  err:
        LOG("@pentry=%p, fd=%d, flags=0x%X", pe, fd, info->flags);
        return ret;
}
