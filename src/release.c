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
extern char *compression_method;
extern char *cache_dir;

/*
 * compress a file, return its size and set `fd' to its file descriptor value
 *
 *
 * return the size of the compressed file
 * ret = 0 -> no compression
 * ret < 0 -> an error occurred
 * ret > 0 -> everything's ok
 *
 *
 */
static ssize_t
compress_before_sending(char *local,
                        char *zlocal,
                        dpl_dict_t *dict,
                        int *fd)
{
        FILE *fpsrc = NULL;
        FILE *fpdst = NULL;
        int zfd;
        struct stat zst;
        dpl_status_t rc;
        ssize_t ret;
        int zrc;

        if (strncasecmp(compression_method, "zlib", strlen("zlib"))) {
                LOG("compression = '%s' - do not compress", compression_method);
                ret = 0;
                goto end;
        }

        fpsrc = fopen(local, "r");
        if (! fpsrc) {
                LOG("fopen: %s", strerror(errno));
                ret = -1;
                goto end;
        }

        fpdst = fopen(zlocal, "w+");
        if (! fpdst) {
                LOG("fopen: %s", strerror(errno));
                LOG("send the file uncompressed");
                ret = 0;
                goto end;
        }

        LOG("start compression before upload");
        zrc = zip(fpsrc, fpdst, zlib_level);
        if (Z_OK != zrc) {
                LOG("zip failed: %s", zerr_to_str(zrc));
                ret = -1;
                goto end;
        }

        fflush(fpdst);
        zfd = fileno(fpdst);
        if (-1 == zfd) {
                LOG("fileno: %s", strerror(errno));
                LOG("send the file uncompressed");
                ret = 0;
                goto end;
        }

        if (-1 == fstat(zfd, &zst)) {
                LOG("fstat: %s", strerror(errno));
                ret = -1;
                goto end;
        }

        /* please rewind before sending the data */
        lseek(zfd, 0, SEEK_SET);
        LOG("compressed file: fd=%d, size=%llu",
            zfd, (unsigned long long)zst.st_size);

        rc = dpl_dict_update_value(dict, "compression", "zlib");
        if (DPL_SUCCESS != rc) {
                LOG("can't update metadata: %s", dpl_status_str(rc));
                ret = 0;
                goto end;
        }

        ret = zst.st_size;

        if (fd)
                *fd = zfd;

  end:
        if (fpsrc)
                fclose(fpsrc);

        if (fpdst)
                fclose(fpdst);

        return ret;

}

int
dfs_release(const char *path,
            struct fuse_file_info *info)
{
        pentry_t *pe = NULL;
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;
        dpl_status_t rc = DPL_FAILURE;
        dpl_dict_t *dict = NULL;
        struct stat st;
        int ret = 0;
        size_t size;
        ssize_t zsize;
        int fd = -1;
        char *local = NULL;
        char *zlocal = NULL;

        pe = (pentry_t *)info->fh;
        if (! pe) {
                LOG("no path entry");
                goto err;
        }

        fd = pentry_get_fd(pe);
        if (fd < 0) {
                LOG("unusable file descriptor fd=%d", fd);
                goto err;
        }

        LOG("%s, fd=%d", path, fd);

        if (-1 == fstat(fd, &st)) {
                LOG("fstat(%d, %p) = %s", fd, (void *)&st, strerror(errno));
                goto err;
        }

        /* We opened a file but we do not want to update it on the server since
         * it was only for read-only purposes */
        if (O_RDONLY == (info->flags & O_ACCMODE))
                goto err;

        size = st.st_size;
        dict = dpl_dict_new(13);
        fill_metadata_from_stat(dict, &st);

        local = tmpstr_printf("%s/%s", cache_dir, path);
        zlocal = tmpstr_printf("%s.tmp", local);

        zsize = compress_before_sending(local, zlocal, dict, &fd);
        if (0 == zsize)
                goto send;
        if (zsize < 0)
                goto err;

        size = zsize;

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
        if (-1 != fd)
                lseek(fd, SEEK_SET, 0);

        if (dict)
                dpl_dict_free(dict);

        if (vfile)
                dpl_close(vfile);

        if (pe)
                pentry_set_flag(pe, FLAG_CLEAN);

        if (zlocal && -1 == unlink(zlocal))
                LOG("unlink: %s", strerror(errno));

        (void)pentry_unlock(pe);

        return ret;
}
