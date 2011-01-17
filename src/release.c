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
static int
compress_before_sending(FILE *fpsrc,
                        FILE *fpdst,
                        dpl_dict_t *dict,
                        size_t *size)
{
        int zfd;
        struct stat zst;
        dpl_status_t rc;
        ssize_t ret;
        int zrc;

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

        ret = zfd;

        if (size)
                *size = zst.st_size;

  end:

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
        size_t size = 0;
        size_t zsize = 0;
        int fd = -1;
        int zfd = -1;
        char *local = NULL;
        char *zlocal = NULL;
        FILE *fpsrc = NULL;
        FILE *fpdst = NULL;
        int tries = 0;
        int delay = 1;

        LOG("path=%s", path);
        PRINT_FLAGS(path, info);

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
        if (O_RDONLY == (info->flags & O_ACCMODE)) {
                LOG("fd=%d was opened with O_RDONLY mode", fd);
                goto end;
        }

        size = st.st_size;
        dict = dpl_dict_new(13);
        fill_metadata_from_stat(dict, &st);

        local = tmpstr_printf("%s/%s", cache_dir, path);

        if (0 == strncasecmp(compression_method, "zlib", strlen("zlib"))) {
                zlocal = tmpstr_printf("%s.tmp", local);

                fpsrc = fopen(local, "r");
                if (! fpsrc) {
                        LOG("fopen: %s", strerror(errno));
                        ret = -1;
                        goto err;
                }

                fpdst = fopen(zlocal, "w+");
                if (! fpdst) {
                        LOG("fopen: %s", strerror(errno));
                        LOG("send the file uncompressed");
                        ret = 0;
                        goto retry;
                }

                zfd = compress_before_sending(fpsrc, fpdst, dict, &zsize);

                /* error in source file, don't upload it */
                if (zfd < 0) {
                        ret = -1;
                        goto err;
                }

                /* file correctly compressed, send it */
                if (zfd > 0) {
                        size = zsize;
                        fd = zfd;
                }
        }

 retry:
        vfile = NULL;
        LOG("calling dpl_openwrite");
        rc = dpl_openwrite(ctx,
                           (char *)path,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           size,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                if (rc != DPL_ENOENT && (tries < max_retry)) {
                        LOG("dpl_openwrite timeout? (delay=%d, vfile@%p)",
                            delay, (void *)vfile);
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        goto retry;
                }
                LOG("dpl_openwrite: %s (%d)", dpl_status_str(rc), rc);
                ret = -1;
                goto err;
        }

        LOG("calling read_all(fd=%d, vfile=%p)", fd, (void *)vfile);
        if (-1 == read_all(fd, vfile)) {
                ret = -1;
                goto err;
        }

  err:
        if (dict)
                dpl_dict_free(dict);

        if (vfile) {
                rc = dpl_close(vfile);
                if (DPL_SUCCESS != rc) {
                        LOG("dpl_close: %s", dpl_status_str(rc));
                        ret = -1;
                }
        }

        if (pe) {
                pentry_set_flag(pe, FLAG_CLEAN);
                LOG("pentry_unlock(fd=%d)..", pentry_get_fd(pe));
                (void)pentry_unlock(pe);
                LOG("pentry_unlock(fd=%d) finished!", pentry_get_fd(pe));
        }

        if (fpsrc)
                fclose(fpsrc);

        if (fpdst)
                fclose(fpdst);

        if (zlocal && -1 == unlink(zlocal))
                LOG("unlink: %s", strerror(errno));

  end:
        if (-1 != fd && zfd != fd) {
                if(-1 == lseek(fd, SEEK_SET, 0)) {
                        LOG("lseek(fd=%d, SEEK_SET, 0): %s",
                            fd, strerror(errno));
                        ret = -1;
                }
        }

        LOG("return value=%d", ret);
        return ret;
}
