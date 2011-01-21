#include <droplet.h>
#include <errno.h>

#include "release.h"
#include "tmpstr.h"
#include "file.h"
#include "metadata.h"
#include "log.h"
#include "zip.h"

extern dpl_ctx_t *ctx;
extern struct env *env;

/*
 * compress a file,  set `size' to the resulting file size
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

        LOG(LOG_INFO, "start compression before upload");
        zrc = zip(fpsrc, fpdst, env->zlib_level);
        if (Z_OK != zrc) {
                LOG(LOG_ERR, "zip failed: %s", zerr_to_str(zrc));
                ret = 0;
                goto end;
        }

        fflush(fpdst);
        zfd = fileno(fpdst);
        if (-1 == zfd) {
                LOG(LOG_ERR, "fileno: %s", strerror(errno));
                LOG(LOG_NOTICE, "send the file uncompressed");
                ret = 0;
                goto end;
        }

        if (-1 == fstat(zfd, &zst)) {
                LOG(LOG_ERR, "fstat: %s", strerror(errno));
                ret = 0;
                goto end;
        }

        /* please rewind before sending the data */
        lseek(zfd, 0, SEEK_SET);
        LOG(LOG_INFO, "compressed file: fd=%d, size=%llu",
            zfd, (unsigned long long)zst.st_size);

        rc = dpl_dict_update_value(dict, "compression", "zlib");
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "can't update metadata: %s", dpl_status_str(rc));
                ret = 0;
                goto end;
        }

        ret = zfd;

        if (size)
                *size = zst.st_size;

  end:

        LOG(LOG_DEBUG, "return value=%zd", ret);
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
        int ret = -1;
        size_t size = 0;
        size_t zsize = 0;
        int fd = -1;
        int zfd = -1;
        int fd_tosend;
        char *local = NULL;
        char *zlocal = NULL;
        FILE *fpsrc = NULL;
        FILE *fpdst = NULL;
        int tries = 0;
        int delay = 1;

        LOG(LOG_DEBUG, "path=%s", path);
        PRINT_FLAGS(path, info);

        pe = (pentry_t *)info->fh;
        if (! pe) {
                LOG(LOG_ERR, "no path entry");
                goto err;
        }

        fd = pentry_get_fd(pe);
        if (fd < 0) {
                LOG(LOG_ERR, "unusable file descriptor fd=%d", fd);
                goto err;
        }

        LOG(LOG_DEBUG, "%s, fd=%d", path, fd);

        if (-1 == fstat(fd, &st)) {
                LOG(LOG_ERR, "fstat(fd=%d) = %s", fd, strerror(errno));
                goto err;
        }

        pentry_dec_refcount(pe);

        /* We opened a file but we do not want to update it on the server since
         * it was for read-only purposes */
        if (O_RDONLY == (info->flags & O_ACCMODE)) {
                LOG(LOG_DEBUG, "fd=%d was opened in O_RDONLY mode", fd);
                ret = 0;
                goto end;
        }

        size = st.st_size;
        dict = dpl_dict_new(13);
        fill_metadata_from_stat(dict, &st);
        fd_tosend = fd;

        local = tmpstr_printf("%s/%s", env->cache_dir, path);

        if (0 == strncasecmp(env->compression_method, "zlib", strlen("zlib"))) {
                zlocal = tmpstr_printf("%s.tmp", local);

                fpsrc = fopen(local, "r");
                if (! fpsrc) {
                        LOG(LOG_ERR, "fopen: %s", strerror(errno));
                        ret = -1;
                        goto err;
                }

                fpdst = fopen(zlocal, "w+");
                if (! fpdst) {
                        LOG(LOG_ERR, "fopen: %s", strerror(errno));
                        LOG(LOG_NOTICE, "send the file uncompressed");
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
                        fd_tosend = zfd;
                }
        }

  retry:
        vfile = NULL;
        rc = dpl_openwrite(ctx,
                           (char *)path,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           size,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                if (rc != DPL_ENOENT && (tries < env->max_retry)) {
                        LOG(LOG_NOTICE,
                            "dpl_openwrite timeout? (delay=%d, vfile@%p)",
                            delay, (void *)vfile);
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        goto retry;
                }
                LOG(LOG_ERR, "dpl_openwrite: %s (%d)", dpl_status_str(rc), rc);
                ret = -1;
                goto err;
        }

        if (-1 == read_write_all_vfile(fd_tosend, vfile)) {
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        if (dict)
                dpl_dict_free(dict);

        if (vfile) {
                rc = dpl_close(vfile);
                if (DPL_SUCCESS != rc) {
                        LOG(LOG_ERR, "dpl_close: %s", dpl_status_str(rc));
                        ret = -1;
                }
        }

        if (pe) {
                pentry_set_flag(pe, FLAG_CLEAN);
                (void)pentry_unlock(pe);
        }

        if (fpsrc)
                fclose(fpsrc);

        if (-1 == lseek(fd, 0, SEEK_SET))
                LOG(LOG_ERR, "lseek(fd=%d, 0, SEEK_SET): %s",
                    fd, strerror(errno));

        if (fpdst)
                fclose(fpdst);

        if (zlocal) {
                LOG(LOG_INFO, "removing cache file '%s'", zlocal);
                if (-1 == unlink(zlocal))
                        LOG(LOG_ERR, "unlink: %s", strerror(errno));
        }

  end:
        LOG(LOG_DEBUG, "return value=%d", ret);
        return ret;
}
