#include <libgen.h>
#include <errno.h>
#include <glib.h>
#include <droplet.h>

#include "getattr.h"
#include "glob.h"
#include "log.h"
#include "file.h"
#include "metadata.h"

extern int max_retry;
extern GHashTable *hash;

static void
set_default_stat(struct stat *st, dpl_ftype_t type)
{
        /* default modes, if no corresponding metadata is found */
        switch (type) {
        case DPL_FTYPE_DIR:
                st->st_mode |= (S_IFDIR | S_IRUSR | S_IXUSR);
                break;
        case DPL_FTYPE_REG:
                st->st_mode |= (S_IFREG | S_IRUSR | S_IWUSR );
                break;
        }

        st->st_uid = getuid();
        st->st_gid = getgid();

        st->st_atime = st->st_mtime = st->st_ctime = time(NULL);
        st->st_size = 0;
}


static int
dfs_getattr_cached(pentry_t *pe,
                   struct stat *st)
{
        int ret;
        int fd;

        fd = pentry_get_fd(pe);
        LOG("pe@%p, fd=%d", (void *)pe, fd);

        /* if the flag is CLEAN, then the file was successfully uploaded,
         * just grab its metadata */
        if (FLAG_CLEAN == pentry_get_flag(pe)) {
                LOG("file upload isn't finished yet");
                ret = -1;
        }

        /* otherwise, use the `struct stat' info of local cache file
         * descriptor */
        if (-1 == fstat(fd, st)) {
                LOG("fstat: %s", strerror(errno));
                ret = -1;
                goto err;
        }

        LOG("use the cache file descriptor (%d) to fill struct stat", fd);
        ret = 0;
  err:
        return ret;
}

int
dfs_getattr(const char *path,
            struct stat *st)
{
        dpl_ftype_t type;
        dpl_ino_t ino, parent_ino, obj_ino;
        dpl_status_t rc;
        dpl_dict_t *metadata = NULL;
        pentry_t *pe = NULL;
        int ret;
        int tries = 0;
        int delay = 1;

        LOG("path=%s, st=%p", path, (void *)st);

        memset(st, 0, sizeof *st);

        /* if the file isn't fully uploaded, get its metadata */
        pe = g_hash_table_lookup(hash, path);
        if (pe) {
                if (! dfs_getattr_cached(pe, st))  {
                        ret = 0;
                        goto end;
                }
        }

        LOG("we have to retrieve metadata to check synchronization");

        /*
         * why setting st_nlink to 1?
         * see http://sourceforge.net/apps/mediawiki/fuse/index.php?title=FAQ
         * Section 3.3.5 "Why doesn't find work on my filesystem?"
         */
        st->st_nlink = 1;

	if (strcmp(path, "/") == 0) {
		st->st_mode = root_mode | S_IFDIR;
                ret = 0;
                goto end;
	}

        ino = dpl_cwd(ctx, ctx->cur_bucket);

 namei_retry:
        rc = dpl_namei(ctx, (char *)path, ctx->cur_bucket,
                       ino, &parent_ino, &obj_ino, &type);

        LOG("dpl_namei returned %s, type=%s, parent_ino=%s, obj_ino=%s",
            dpl_status_str(rc), dfs_ftypetostr(type),
            parent_ino.key, obj_ino.key);

        if (DPL_SUCCESS != rc) {
                if (DPL_ENOENT != rc && (tries < max_retry)) {
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        LOG("namei timeout? (%s)", dpl_status_str(rc));
                        goto namei_retry;
                }
                ret = rc;
                goto end;
        }

        delay = 1;
        tries = 0;
 getattr_retry:
        rc = dpl_getattr(ctx, (char *)path, &metadata);

        if (DPL_SUCCESS != rc && (DPL_EISDIR != rc)) {
                if (tries < max_retry) {
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        LOG("getattr timeout? (%s)", dpl_status_str(rc));
                        goto getattr_retry;
                }
                LOG("dpl_getattr: %s", dpl_status_str(rc));
                if (metadata)
                        dpl_dict_free(metadata);
                ret = -1;
                goto end;
        }

        set_default_stat(st, type);
        if (metadata) {
                fill_stat_from_metadata(st, metadata);
                dpl_dict_free(metadata);
        }

        ret = 0;
  end:
        return ret;
}
