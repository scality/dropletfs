#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "glob.h"
#include "file.h"

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


int
dfs_getattr(const char *path,
            struct stat *st)
{
        LOG("path=%s, st=%p", path, (void *)st);

        memset(st, 0, sizeof *st);

        dpl_dict_t *metadata = NULL;

        /*
         * why setting st_nlink to 1?
         * see http://sourceforge.net/apps/mediawiki/fuse/index.php?title=FAQ
         * Section 3.3.5 "Why doesn't find work on my filesystem?"
         */
        st->st_nlink = 1;

	if (strcmp(path, "/") == 0) {
		st->st_mode = root_mode | S_IFDIR;
                return 0;
	}

        dpl_ftype_t type;
        dpl_ino_t ino, parent_ino, obj_ino;

        ino = dpl_cwd(ctx, ctx->cur_bucket);
        dpl_status_t rc = dpl_namei(ctx, (char *)path, ctx->cur_bucket,
                                    ino, &parent_ino, &obj_ino, &type);

        LOG("dpl_namei returned %s (%d), type=%s, parent_ino=%s, obj_ino=%s",
            dpl_status_str(rc), rc, dfs_ftypetostr(type),
            parent_ino.key, obj_ino.key);

        if (DPL_SUCCESS != rc)
                return rc;

        rc = dpl_getattr(ctx, (char *)path, &metadata);
        if (DPL_SUCCESS != rc && (DPL_EISDIR != rc)) {
                LOG("dpl_getattr %s: %s", path, dpl_status_str(rc));
                if (metadata)
                        dpl_dict_free(metadata);
                return -1;
        }

        set_default_stat(st, type);
        if (metadata) {
                fill_stat_from_metadata(st, metadata);
                dpl_dict_free(metadata);
        }

        return 0;
}
