#include <glib.h>
#include <droplet.h>

#include "rename.h"
#include "unlink.h"
#include "file.h"
#include "log.h"
#include "timeout.h"
#include "hash.h"
#include "mkdir.h"
#include "rmdir.h"
#include "tmpstr.h"

extern dpl_ctx_t *ctx;
extern GHashTable *hash;

static int
rename_file(const char *oldpath,
            const char *newpath)
{
        int ret;
        dpl_status_t rc;
        char *p = NULL;
        char *dirname = NULL;
        pentry_t *pe_dir = NULL;

        LOG(LOG_DEBUG, "%s -> %s", oldpath, newpath);

        rc = dfs_fcopy_timeout(ctx, oldpath, newpath);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_fcopy_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        dirname = (char *)newpath;
        p = strrchr(dirname, '/');
        if (p) {
                if (p != newpath) {
                        *p = 0;
                        pe_dir = g_hash_table_lookup(hash, dirname);
                        *p = '/';
                        if (pe_dir)
                                (void)pentry_remove_dirent(pe_dir, oldpath);
                }
        }

        ret = dfs_unlink(oldpath);
  err:
        return ret;
}

static int
rename_directory(const char *oldpath,
                 const char *newpath)
{
        void *dir_hdl = NULL;
        dpl_dirent_t dirent;
        dpl_status_t rc;
        dpl_ftype_t type;
        dpl_ino_t ino;
        int ret = 0;
        char *src_entry = NULL;
        char *dst_entry = NULL;

        LOG(LOG_DEBUG, "%s -> %s", oldpath, newpath);

        rc = dfs_namei_timeout(ctx, newpath, ctx->cur_bucket,
                               ino, NULL, NULL, &type);
        if (DPL_SUCCESS != rc && (DPL_ENOENT != rc)) {
                LOG(LOG_ERR, "dpl_namei: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (DPL_ENOENT == rc) {
                rc = dfs_mkdir(newpath, 0755);
                if (rc) {
                        ret = -1;
                        goto err;
                }
        }

        rc = dfs_chdir_timeout(ctx, oldpath);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_chdir_timeout: %s", dpl_status_str(rc));
                ret = rc;
                goto err;
        }

        rc = dfs_opendir_timeout(ctx, ".", &dir_hdl);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_opendir_timeout: %s", dpl_status_str(rc));
                ret = rc;
                goto err;
        }

        while (DPL_SUCCESS == dpl_readdir(dir_hdl, &dirent)) {
                if (! strcmp(dirent.name, "."))
                        continue;
                if (! strcmp(dirent.name, ".."))
                        continue;

                src_entry = tmpstr_printf("%s/%s", oldpath, dirent.name);
                dst_entry = tmpstr_printf("%s/%s", newpath, dirent.name);

                if ('/' == src_entry[strlen(src_entry) -1])
                        src_entry[strlen(src_entry) - 1] = 0;

                if ('/' == dst_entry[strlen(dst_entry) -1])
                        dst_entry[strlen(dst_entry) -1] = 0;

                rc = dfs_namei_timeout(ctx, src_entry, ctx->cur_bucket,
                                       ino, NULL, NULL, &type);

                if (DPL_SUCCESS != rc && (DPL_ENOENT != rc)) {
                        LOG(LOG_ERR, "dpl_namei: %s", dpl_status_str(rc));
                        ret = -1;
                        goto err;
                }

                if (DPL_FTYPE_DIR == type)
                        ret = rename_directory(src_entry, dst_entry);
                else
                        ret = rename_file(src_entry, dst_entry);
        }

        if (-1 == ret)
                goto err;

        dfs_rmdir(oldpath);

        ret = 0;
  err:
        if (dir_hdl)
                dpl_closedir(dir_hdl);

        return ret;
}

int
dfs_rename(const char *oldpath,
           const char *newpath)
{
        dpl_status_t rc;
        dpl_ftype_t type;
        dpl_ino_t ino;
        char *p = NULL;
        int ret = 0;

        LOG(LOG_DEBUG, "%s -> %s", oldpath, newpath);

        if (0 == strcmp(newpath, ".")) {
                p = strrchr(oldpath, '/');
                newpath = p ? p + 1 : oldpath;
        }

        rc = dfs_namei_timeout(ctx, oldpath, ctx->cur_bucket,
                               ino, NULL, NULL, &type);
        if (! DPL_SUCCESS == rc && (DPL_ENOENT != rc)) {
                LOG(LOG_ERR, "dpl_namei: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (DPL_FTYPE_DIR != type && DPL_FTYPE_REG != type) {
                LOG(LOG_ERR, "unsupported: '%s'", ftype_to_str(type));
                ret = -1;
                goto err;
        }

        int (*cb[])(const char *, const char *) = {
                [DPL_FTYPE_REG] = rename_file,
                [DPL_FTYPE_DIR] = rename_directory,
        };

        if (-1 == cb[type](oldpath, newpath)) {
                ret = -1;
                goto err;
        }

  err:
        return ret;
}
