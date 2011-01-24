#include <errno.h>
#include <glib.h>
#include <libgen.h>

#include "open.h"
#include "misc.h"
#include "log.h"
#include "glob.h"
#include "file.h"
#include "tmpstr.h"
#include "env.h"

extern GHashTable *hash;
extern struct env *env;

static int
populate_hash(GHashTable *h,
              const char * const path,
              pentry_t **pep)
{
        int ret;
        char *key = NULL;
        pentry_t *pe = NULL;

        if (! pep) {
                ret = -1;
                goto err;
        }

        pe = pentry_new();
        if (! pe) {
                ret = -1;
                goto err;
        }

        pentry_set_fd(pe, -1);
        key = strdup(path);
        if (! key) {
                LOG(LOG_CRIT, "strdup(%s): %s", path, strerror(errno));
                pentry_free(pe);
                ret = -1;
                goto err;
        }

        pentry_set_path(pe, path);
        *pep = pe;
        g_hash_table_insert(h, key, pe);

        ret = 0;
  err:
        return ret;
}

static char *
build_cache_tree(const char *path)
{
        LOG(LOG_DEBUG, "building cache dir for '%s'", path);

        char *local = NULL;
        char *tmp_local = NULL;
        char *dir = NULL;
        struct stat st;

        /* ignore the leading spaces */
        while (path && '/' == *path)
                path++;

        local = tmpstr_printf("%s/%s", env->cache_dir, path);

        tmp_local = strdup(local);

        if (! tmp_local) {
                LOG(LOG_CRIT, "strdup(%s): %s", tmp_local, strerror(errno));
                return NULL;
        }

        dir = tmpstr_printf("%s", dirname(tmp_local));
        if (-1 == stat(dir, &st)) {
                if (ENOENT == errno)
                        mkdir_tree(dir);
                else
                        LOG(LOG_ERR, "stat(%s): %s", dir, strerror(errno));
        }

        free(tmp_local);

        return local;
}

static int
open_read_write(const char * const path,
                pentry_t *pe)
{
        int ret;
        int fd;
        char *local = NULL;

        local = build_cache_tree(path);

        if (! local) {
                LOG(LOG_ERR, "can't create a cache local path (%s)", path);
                ret = -1;
                goto err;
        }

        LOG(LOG_INFO, "opening cache file '%s'", local);
        fd = open(local, O_RDWR|O_CREAT|O_TRUNC, 0644);
        if (-1 == fd) {
                LOG(LOG_ERR, "%s: %s", local, strerror(errno));
                ret = -1;
                goto err;
        }
        pentry_set_fd(pe, fd);
        pentry_set_flag(pe, FLAG_DIRTY);

        ret = 0;
  err:
        return ret;
}

static int
open_read_only(const char * const path,
               pentry_t *pe)
{
        int ret;
        int fd;

        /* get the fd of the file we want to read */
        fd = pentry_get_fd(pe);

        /* negative fd? then we don't have any cache file, get it! */
        if (fd < 0) {
                fd = dfs_get_local_copy(pe, path);
                if (-1 == fd) {
                        ret = -1;
                        goto err;
                }
                pentry_set_fd(pe, fd);
        }

        ret = 0;
  err:
        return ret;
}

int
dfs_open(const char *path,
         struct fuse_file_info *info)
{
        pentry_t *pe = NULL;
        int fd = -1;
        int ret = -1;
        int mode;

        info->fh = 0;
        LOG(LOG_DEBUG, "path=%s %s", path, flag_to_str(info));

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_INFO, "'%s': entry not found in hashtable", path);
                if (-1 == populate_hash(hash, path, &pe)) {
                        ret = -1;
                        goto err;
                }
                LOG(LOG_INFO, "adding file '%s' to the hashtable", path);
        } else {
                fd = pentry_get_fd(pe);
                LOG(LOG_INFO, "%s: found in the hashtable, fd=%d", path, fd);
        }

        info->fh = (uint64_t)pe;
        pentry_inc_refcount(pe);

        mode = info->flags & O_ACCMODE;

        if (O_RDONLY != mode) {
                if (pentry_lock(pe)) {
                        LOG(LOG_DEBUG, "... %s not locked!", path);
                        ret = -1;
                        pentry_dec_refcount(pe);
                        goto err;
                }
                LOG(LOG_DEBUG, "... %s locked!", path);
        }

        info->fh = (uint64_t)pe;

        /* TODO: add other states for O_APPEND, etc. */
        int (*fn[])(const char *, pentry_t *) = {
                [O_RDONLY] = open_read_only,
                [O_WRONLY] = open_read_write,
                [O_RDWR]   = open_read_write,
        };

        if (-1 == fn[mode](path, pe)) {
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        LOG(LOG_DEBUG, "@pentry=%p, fd=%d, flags=0x%X, ret=%d",
            pe, fd, info->flags, ret);
        return ret;
}
