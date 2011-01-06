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
        int fd = -1;
        FILE *fpsrc = NULL;
        FILE *fpdst = NULL;

        pe = (struct pentry *)info->fh;
        if (! pe)
                return 0;

        fd = pe->fd;
        LOG("%s, fd=%d", path, fd);

        if (-1 == fstat(pe->fd, &st)) {
                LOG("fstat(%d, %p) = %s", fd, (void *)&st, strerror(errno));
                goto err;
        }

        /* We opened a file but we do not want to update it on the server since
         * it was only for read-only purposes */
        if (O_RDONLY == (info->flags & O_ACCMODE))
                goto err;

        size = st.st_size;

        local = tmpstr_printf("/tmp/%s/%s", ctx->cur_bucket, path);
        zlocal = tmpstr_printf("%s.tmp", local);
        snprintf(zlib_level_str, sizeof zlib_level_str, "%lu", zlib_level);

        dict = dpl_dict_new(13);
        fill_metadata_from_stat(dict, &st);

        fpsrc = fopen(local, "r");
        if (! fpsrc) {
                LOG("fopen: %s", strerror(errno));
                goto err;
        }

        fpdst = fopen(zlocal, "w");
        if (! fpdst) {
                LOG("fopen: %s", strerror(errno));
                LOG("send the file uncompressed");
                goto send;
        }

        LOG("start compression before upload");
        ret = zip(fpsrc, fpdst, zlib_level);
        if (Z_OK != ret) {
                LOG("zip failed: %s", zerr_to_str(ret));
                goto err;
        }

        fd = fileno(fpdst);
        if (-1 == fd) {
                LOG("fileno: %s", strerror(errno));
                fd = pe->fd;
                goto send;
        } else {
                /* please rewind before sending the data */
                LOG("fd of compressed file: %d", fd);
                lseek(fd, 0, SEEK_SET);
        }

        rc = dpl_dict_update_value(dict, "compression", "zlib");
        if (DPL_SUCCESS != rc) {
                LOG("can't update metadata: %s", dpl_status_str(rc));
        } else {
                struct stat zst;
                if (-1 == fstat(fd, &zst)) {
                        LOG("fstat: %s", strerror(errno));
                        goto err;
                }
                size = zst.st_size;
                LOG("compression done: %zuB -> %zuB", st.st_size, zst.st_size);
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

        if (-1 == read_all(fd, vfile)) {
                ret = -1;
                goto err;
        }

  err:
        if (fpsrc)
                fclose(fpsrc);

        if (fpdst)
                fclose(fpdst);

        if (-1 != pe->fd)
                lseek(pe->fd, SEEK_SET, 0);

        if (dict)
                dpl_dict_free(dict);

        if (vfile)
                dpl_close(vfile);

/*         if (zlocal && -1 == unlink(zlocal)) */
/*                 LOG("unlink: %s", strerror(errno)); */

        return ret;
}
