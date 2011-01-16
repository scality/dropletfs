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
extern int max_retry;

/*
 * compress a file, return its size and set `fd' to its file descriptor value
 *
 * return the size of the compressed file
 * ret = 0 -> no compression
 * ret < 0 -> an error occurred
 * ret > 0 -> everything's ok
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
                ret = 0;
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
                ret = 0;
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

        ret = 0;

        if (fd) {
                ret = zst.st_size;
                *fd = zfd;
        }

  end:
        if (fpsrc)
                fclose(fpsrc);

        if (fpdst)
                fclose(fpdst);

        LOG("return value=%zd", ret);
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
        int tries = 0;
        int delay = 1;

        LOG("path=%s", path);

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

        if (0 == strncasecmp(compression_method, "zlib", strlen("zlib"))) {
                zlocal = tmpstr_printf("%s.tmp", local);
                zsize = compress_before_sending(local, zlocal, dict, &fd);

                /* error in source file, don't upload it */
                if (zsize < 0) {
                        ret = -1;
                        goto err;
                }

                /* file correctly compressed, send it */
                if (zsize > 0)
                        size = zsize;
        }

 retry:
        rc = dpl_openwrite(ctx,
                           (char *)path,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           size,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                if (rc != DPL_ENOENT && (tries < max_retry)) {
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        LOG("dpl_openwrite timeout? (delay=%d)", delay);
                        goto retry;
                }
                LOG("dpl_openwrite: %s (%d)", dpl_status_str(rc), rc);
                ret = -1;
                goto err;
        }

        if (-1 == read_all(fd, vfile)) {
                ret = -1;
                goto err;
        }

  err:
        if (-1 != fd) {
                if(-1 == lseek(fd, SEEK_SET, 0)) {
                        LOG("lseek(fd=%d, SEEK_SET, 0): %s",
                            fd, strerror(errno));
                        ret = -1;
                }
        }

        if (dict)
                dpl_dict_free(dict);

        if (vfile)
                dpl_close(vfile);

        if (pe) {
                pentry_set_flag(pe, FLAG_CLEAN);
                LOG("pentry_unlock(fd=%d)..", pentry_get_fd(pe));
                (void)pentry_unlock(pe);
                LOG("pentry_unlock(fd=%d) finished!", pentry_get_fd(pe));
        }

        if (zlocal && -1 == unlink(zlocal))
                LOG("unlink: %s", strerror(errno));

        LOG("return value=%d", ret);
        return ret;
}
