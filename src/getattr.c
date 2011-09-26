#include <libgen.h>
#include <errno.h>
#include <glib.h>
#include <droplet.h>
#include <time.h>

#include "getattr.h"
#include "log.h"
#include "file.h"
#include "metadata.h"
#include "timeout.h"
#include "list.h"

extern dpl_ctx_t *ctx;
extern struct conf *conf;
extern GHashTable *hash;

static void
set_default_stat(struct stat *st,
                 dpl_ftype_t type)
{
        /* default modes, if no corresponding metadata is found */
        switch (type) {
        case DPL_FTYPE_DIR:
                st->st_mode |= (S_IFDIR | S_IRUSR | S_IXUSR);
                break;
        case DPL_FTYPE_REG:
                st->st_mode |= (S_IFREG | S_IRUSR | S_IWUSR );
                break;
        default:
                st->st_mode = 0;
                break ;
        }

        st->st_uid = getuid();
        st->st_gid = getgid();

        st->st_atime = st->st_mtime = st->st_ctime = time(NULL);
        st->st_size = 0;
}

static void
set_filetype_from_stat(pentry_t *pe,
                       struct stat *st)
{
        if (st->st_mode | S_IFREG)
                pentry_set_filetype(pe, FILE_REG);
        else if (st->st_mode | S_IFDIR)
                pentry_set_filetype(pe, FILE_DIR);
        else if (st->st_mode | S_IFLNK)
                pentry_set_filetype(pe, FILE_SYMLINK);
}

static int
hash_fill_dirent(GHashTable *hash,
                 const char *path)
{
        char *dirname = NULL;
        char *p = NULL;
        int ret;
        pentry_t *dir = NULL;

        dirname = (char *)path;
        p = strrchr(dirname, '/');
        if (! p) {
                LOG(LOG_ERR, "%s: no root dir in path", path);
                ret = -1;
                goto err;
        }

        /* we are in the root dir */
        if (p == path)
                dirname = "/";
        else
                *p = 0;

        dir = g_hash_table_lookup(hash, dirname);
        if (! dir) {
                LOG(LOG_ERR, "'%s' not found in hashtable", dirname);
                ret = -1;
                goto err;
        }

        if (! *p)
                *p = '/';

        pentry_add_dirent(dir, path);

        ret = 0;
  err:
        return ret;
}

static int
getattr_remote(pentry_t *pe,
              const char *path,
              struct stat *st)
{
        int ret;
        dpl_dict_t *meta = NULL;

        LOG(LOG_DEBUG, "%s: get remote metadata through hashtable", path);

        if (pentry_md_lock(pe)) {
                ret = -1;
                goto err;
        }
        meta = pentry_get_metadata(pe);
        if (meta) {
                fill_stat_from_metadata(st, meta);
                if (dpl_dict_get(meta, "symlink"))
                        st->st_mode |= S_IFLNK;
        }

        if (pentry_md_unlock(pe)) {
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        return ret;
}

static int
getattr_local(pentry_t *pe,
              const char *path,
              struct stat *st)
{
        int fd;
        int ret;
        int exclude;

        exclude = pentry_get_exclude(pe);
        fd = pentry_get_fd(pe);

        LOG(LOG_DEBUG, "%s: get local metadata through fstat(fd=%d)", path, fd);

        if (fd < 0) {
                LOG(LOG_ERR, "path=%s: invalid fd=%d", path, fd);
                ret = -1;
                goto err;
        }

        if (-1 == fstat(fd, st)) {
                LOG(LOG_ERR, "path=%s: fstat(fd=%d, ...): %s",
                    path, fd, strerror(errno));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        return ret;
}

static int
getattr_unset(pentry_t *pe,
              const char *path,
              struct stat *st)
{
        dpl_ftype_t type;
        dpl_ino_t ino, parent_ino, obj_ino;
        dpl_status_t rc;
        dpl_dict_t *metadata = NULL;
        int ret;

        LOG(LOG_DEBUG, "%s: get remote metadata with dpl_getattr()", path);

        ino = dpl_cwd(ctx, ctx->cur_bucket);

        rc = dfs_namei_timeout(ctx, path, ctx->cur_bucket,
                               ino, &parent_ino, &obj_ino, &type);

        LOG(LOG_DEBUG, "path=%s, dpl_namei: %s, type=%s, parent_ino=%s, obj_ino=%s",
            path, dpl_status_str(rc), ftype_to_str(type),
            parent_ino.key, obj_ino.key);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_NOTICE, "dfs_namei_timeout: %s", dpl_status_str(rc));
                ret = rc;
                goto end;
        }

        rc = dfs_getattr_timeout(ctx, path, &metadata);
        if (DPL_SUCCESS != rc && DPL_EISDIR != rc) {
                LOG(LOG_ERR, "dfs_getattr_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto end;
        }

        set_default_stat(st, type);
        if (metadata) {
                if (dpl_dict_get(metadata, "symlink"))
                        st->st_mode |= S_IFLNK;
                fill_stat_from_metadata(st, metadata);

                if (pentry_md_lock(pe)) {
                        ret = -1;
                        goto end;
                }

                pentry_set_metadata(pe, metadata);

                if (pentry_md_unlock(pe)) {
                        ret = -1;
                        goto end;
                }

                pentry_set_placeholder(pe, FILE_REMOTE);
        }

        set_filetype_from_stat(pe, st);
        (void)hash_fill_dirent(hash, path);

        ret = 0;
  end:
        if (metadata)
                dpl_dict_free(metadata);

        return ret;
}

int
dfs_getattr(const char *path,
            struct stat *st)
{
        pentry_t *pe = NULL;
        int ret;
        char *key = NULL;

        LOG(LOG_DEBUG, "path=%s, st=%p", path, (void *)st);

        memset(st, 0, sizeof *st);

        /*
         * why setting st_nlink to 1?
         * see http://sourceforge.net/apps/mediawiki/fuse/index.php?title=FAQ
         * Section 3.3.5 "Why doesn't find work on my filesystem?"
         */
        st->st_nlink = 1;

	if (strcmp(path, "/") == 0) {
		st->st_mode = S_IFDIR;
                ret = 0;
                goto end;
	}

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                pe = pentry_new();
                if (! pe) {
                        LOG(LOG_ERR, "%s: can't add a new cell", path);
                        ret = -1;
                        goto end;
                }
                pentry_set_path(pe, path);
                key = strdup(path);
                if (! key) {
                        LOG(LOG_ERR, "%s: strdup: %s", path, strerror(errno));
                        pentry_free(pe);
                        ret = -1;
                        goto end;
                }
                g_hash_table_insert(hash, key, pe);
        }

        int (*cb[]) (pentry_t *, const char *, struct stat *) = {
                [FILE_REMOTE] = getattr_remote,
                [FILE_LOCAL]  = getattr_local,
                [FILE_UNSET]  = getattr_unset,
        };

        ret = cb[pentry_get_placeholder(pe)](pe, path, st);
        pentry_set_atime(pe);

  end:
        return ret;
}
