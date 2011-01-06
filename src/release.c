#include <droplet.h>
#include <errno.h>

#include "release.h"
#include "tmpstr.h"
#include "glob.h"
#include "file.h"
#include "metadata.h"
#include "log.h"
#include "zip.h"

extern unsigned long zlib_level;

int
dfs_release(const char *path,
            struct fuse_file_info *info)
{
        struct pentry *pe = NULL;
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;
        dpl_status_t rc = DPL_FAILURE;
        dpl_dict_t *dict = NULL;
        struct stat st;
        int ret = 0;
        char *local = NULL;
        char *zlocal = NULL;
        char zlib_level_str[32];
        size_t size;

        pe = (struct pentry *)info->fh;
        if (! pe)
                return 0;

        LOG("%s, fd=%d", path, pe->fd);

        if (-1 == fstat(pe->fd, &st)) {
                LOG("fstat(%d, %p) = %s", pe->fd, (void *)&st, strerror(errno));
                goto err;
        }

        size = st.st_size;

        local = tmpstr_printf("/tmp/%s/%s", ctx->cur_bucket, path);
        zlocal = tmpstr_printf("%s.tmp", local);
        snprintf(zlib_level_str, sizeof zlib_level_str, "%lu", zlib_level);

        dict = dpl_dict_new(13);
        fill_metadata_from_stat(dict, &st);

        ret = zip(local, zlocal, zlib_level);
        if (Z_OK == ret) {
                rc = dpl_dict_update_value(dict, "compression", "zlib");
                if (DPL_SUCCESS != rc) {
                        LOG("can't update metadata: %s", dpl_status_str(rc));
                        goto send;
                }

                struct stat zst;
                if (-1 == stat(zlocal, &zst)) {
                        LOG("stat: %s", strerror(errno));
                        goto err;
                }
                size = zst.st_size;
        }

  send:
        rc = dpl_openwrite(ctx,
                           (char *)path,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           size,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_openwrite: %s (%d)", dpl_status_str(rc), rc);
                ret = -1;
                goto err;
        }

        if (-1 == read_all(pe->fd, vfile)) {
                ret = -1;
                goto err;
        }

  err:
        if (-1 != pe->fd)
                lseek(pe->fd, SEEK_SET, 0);

        if (dict)
                dpl_dict_free(dict);

        if (vfile)
                dpl_close(vfile);

        if (-1 == unlink(zlocal))
                LOG("unlink: %s", strerror(errno));

        return ret;
}
