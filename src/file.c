#include <assert.h>
#include <droplet.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>

#include "log.h"
#include "file.h"
#include "tmpstr.h"
#include "metadata.h"
#include "zip.h"
#include "timeout.h"

#define WRITE_BLOCK_SIZE (1000*1000)

extern struct conf *conf;
extern dpl_ctx_t *ctx;

char *
ftype_to_str(dpl_ftype_t type)
{
        switch (type) {
        case DPL_FTYPE_REG:
                return "regular file";
        case DPL_FTYPE_DIR:
                return "directory";
        default:
                return "unknown";
        }
        return "unknown type";
}

char *
flags_to_str(int flags)
{
        switch (flags & O_ACCMODE) {
        case O_RDONLY:
                return "read only";
        case O_WRONLY:
                return "write only";
        case O_RDWR:
                return "read/write";
        }

        assert(! "impossible");
        return "invalid";
}

int
write_all(int fd,
          char *buf,
          int len)
{
        LOG(LOG_DEBUG, "fd=%d, len=%d", fd, len);

	ssize_t cc;
	int remain;

	remain = len;
	while (1) {
          again:
		cc = write(fd, buf, remain);
		if (-1 == cc) {
			if (EINTR == errno)
				goto again;
			return -1;
		}

		remain -= cc;
		buf += cc;

		if (! remain)
			return 0;
	}
}

int
read_write_all_vfile(int fd,
                     dpl_vfile_t *vfile)
{
        dpl_status_t rc = DPL_FAILURE;
        int blksize = WRITE_BLOCK_SIZE;
        char *buf = NULL;

        LOG(LOG_DEBUG, "fd=%d", fd);
        buf = alloca(blksize);
        while (1) {
                int r = read(fd, buf, blksize);
                if (-1 == r) {
                        LOG(LOG_ERR, "read (fd=%d): %s",
                            fd, strerror(errno));
                        return -1;
                }

                if (0 == r)
                        break;

                rc = dpl_write(vfile, buf, r);
                if (DPL_SUCCESS != rc) {
                        LOG(LOG_ERR, "dpl_write: %s (%d)",
                            dpl_status_str(rc), rc);
                        return -1;
                }
        }

        return 0;
}

int
cb_get_buffered(void *arg,
                char *buf,
                unsigned len)
{
        dpl_status_t ret = DPL_SUCCESS;
        struct get_data *get_data = NULL;

        get_data = arg;

        ret = write_all(get_data->fd, buf, len);

        if (DPL_SUCCESS != ret)
                return -1;

        return 0;
}

/* the caller has to free() metadatap */
static int
download_headers(char * path,
                 dpl_dict_t **headersp)
{
        int ret;
        dpl_status_t rc;
        dpl_ino_t ino, obj_ino;
        dpl_dict_t *dict = NULL;

        if (! headersp) {
                ret = -1;
                goto err;
        }

        rc = dfs_namei_timeout(ctx, path, ctx->cur_bucket,
                               ino, NULL, &obj_ino, NULL);
        if (DPL_ENOENT == rc ) {
                LOG(LOG_INFO, "dfs_namei_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_namei_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        rc = dfs_head_all_timeout(ctx, ctx->cur_bucket, obj_ino.key,
                                  NULL, NULL, &dict);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_head_all_timeout: %s", dpl_status_str(rc));
                ret = 1;
                goto err;
        }

        *headersp = dict;
        ret = 0;

  err:
        return ret;
}

static int
compare_digests(pentry_t *pe,
                dpl_dict_t *dict)
{
        char *remote = NULL;
        int ret;
        char *digest = NULL;

        if (FILE_LOCAL != pentry_get_placeholder(pe)) {
                LOG(LOG_DEBUG, "no local file");
                ret = -1;
                goto err;
        }

        digest = pentry_get_digest(pe);
        if (! digest) {
                LOG(LOG_NOTICE, "no digest");
                ret = -1;
                goto err;
        }

        print_metadata(dict);
        ret = -1;

        remote = dpl_dict_get_value(dict, "etag");
        if (remote) {
                LOG(LOG_DEBUG, "remote md5=%s", remote);
                LOG(LOG_DEBUG, "local md5=\"%.*s\"", MD5_DIGEST_LENGTH, digest);
                if (0 == memcmp(digest, remote, MD5_DIGEST_LENGTH)) {
                        ret = 0;
                } else {
                        pentry_set_digest(pe, remote);
                        LOG(LOG_DEBUG, "updated local md5=\"%.*s\"",
                            MD5_DIGEST_LENGTH, pentry_get_digest(pe));
                }
        }

  err:
        return ret;

}

static int
handle_compression(const char *remote,
                   char *local,
                   struct get_data *get_data,
                   dpl_dict_t *metadata)
{
        dpl_status_t rc;
        char *uzlocal = NULL;
        FILE *fpsrc = NULL;
        FILE *fpdst = NULL;
        char *compressed = NULL;
        int zret;
        int ret;

        compressed = dpl_dict_get_value(metadata, "compression");
        if (! compressed) {
                LOG(LOG_INFO, "%s: uncompressed remote file", remote);
                ret = 0;
                goto end;
        }

#define NONE "none"
#define ZLIB "zlib"
        if (0 == strncmp(compressed, NONE, strlen(NONE))) {
                LOG(LOG_INFO, "compression method: 'none'");
                ret = 0;
                goto end;
        }

        if (0 != strncmp(compressed, ZLIB, strlen(ZLIB))) {
                LOG(LOG_ERR, "compression method not supported '%s'",
                    compressed);
                ret = -1;
                goto end;
        }

        uzlocal = tmpstr_printf("%s.tmp", local);

        fpsrc = fopen(local, "r");
        if (! fpsrc) {
                LOG(LOG_ERR, "fopen: %s", strerror(errno));
                ret = -1;
                goto end;
        }

        fpdst = fopen(uzlocal, "w");
        if (! fpdst) {
                LOG(LOG_ERR, "fopen: %s", strerror(errno));
                ret = -1;
                goto end;
        }

        LOG(LOG_INFO, "uncompressing local file '%s'", local);

        zret = unzip(fpsrc, fpdst);
        if (Z_OK != zret) {
                LOG(LOG_ERR, "unzip failed: %s", zerr_to_str(zret));
                ret = -1;
                goto end;
        }

        rc = dpl_dict_update_value(metadata, "compression", "none");
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "unable to update 'compression' metadata");
                ret = -1;
                goto end;
        }

        if (-1 == rename(uzlocal, local)) {
                LOG(LOG_ERR, "rename: %s", strerror(errno));
                ret = 1;
                goto end;
        }

        close(get_data->fd);
        get_data->fd = open(local, O_RDONLY);
        if (-1 == get_data->fd) {
                LOG(LOG_ERR, "open: %s", strerror(errno));
                ret = -1;
                goto end;
        }
#undef ZLIB
#undef NONE

        ret = 0;

  end:
        if (fpsrc)
                fclose(fpsrc);

        if (fpdst)
                fclose(fpdst);

        return ret;
}

/* TODO: code a proper permission mechanism */
static int
check_permissions(pentry_t *pe,
                  dpl_dict_t *metadata)
{
        return 0;
}

static unsigned int
check_encryption_flag(dpl_dict_t *metadata)
{
        char *cipher = NULL;
        char *cipher_type = NULL;
        unsigned int ret = 0;

        if (strncasecmp(conf->encryption_method, "aes", strlen("aes")))
                goto end;

        cipher = dpl_dict_get_value(metadata, "cipher");
        cipher_type = dpl_dict_get_value(metadata, "cipher-type");

        if (! cipher || ! cipher_type) {
                ret = 0;
                goto end;
        }

        if (strncasecmp(cipher, "yes", strlen("yes")))
                ret = 0;
                goto end;

        if (strncasecmp(cipher_type, "aes-256-cfb", strlen("aes-256-cfb"))) {
                LOG(LOG_ERR, "unsupported cipher-type: %s", cipher_type);
                ret = 0;
                goto end;
        }

        ret |= DPL_VFILE_FLAG_ENCRYPT;
  end:
        LOG(LOG_DEBUG, "flags = 0x%x", ret);
        return ret;
}

/* return the fd of a local copy, to operate on */
int
dfs_get_local_copy(pentry_t *pe,
                   const char * const remote,
                   int flags)
{
        int fd;
        dpl_dict_t *metadata = NULL;
        dpl_dict_t *headers = NULL;
        struct get_data get_data = { .fd = -1, .buf = NULL };
        dpl_status_t rc = DPL_FAILURE;
        char *local = NULL;
        unsigned encryption = 0;

        local = tmpstr_printf("%s%s", conf->cache_dir, remote);
        LOG(LOG_DEBUG, "bucket=%s, path=%s, local=%s",
            ctx->cur_bucket, remote, local);

        if (-1 == download_headers((char *)remote, &headers)) {
                LOG(LOG_NOTICE, "%s: can't download headers", remote);
                fd = -1;
                goto end;
        }

        metadata = dpl_dict_new(13);
        if (! metadata) {
                LOG(LOG_ERR, "dpl_dict_new: can't allocate memory");
                fd = -1;
                goto end;
        }

        if (DPL_FAILURE == dpl_get_metadata_from_headers(
#if defined(DPL_VERSION_MAJOR) && defined(DPL_VERSION_MINOR) && DPL_VERSION_MAJOR >= 0 && DPL_VERSION_MINOR >= 2
                                                         ctx,
#endif
                                                         headers, metadata)) {
                LOG(LOG_ERR, "%s: metadata extraction failed", remote);
                fd = -1;
                goto end;
        }

        if (-1 == pentry_set_metadata(pe, metadata)) {
                LOG(LOG_ERR, "can't update metadata");
                fd = -1;
                goto end;
        }

        if (-1 == check_permissions(pe, metadata)) {
                LOG(LOG_NOTICE, "permission denied");
                fd = -1;
                goto end;
        }

        /* If the remote MD5 matches a cache file, we don't have to download
         * it again, just return the (open) file descriptor of the cache file
         */
        if (0 == compare_digests(pe, headers))  {
                fd = pentry_get_fd(pe);
                goto end;
        }

        /* a cache file already exists, its MD5 digest is different, so
         * just remove it */
        if (0 == access(local, F_OK)) {
                LOG(LOG_DEBUG, "removing cache file '%s'", local);
                if (-1 == unlink(local))
                        LOG(LOG_ERR, "unlink(%s): %s", local, strerror(errno));
        }

        get_data.fd = open(local, O_RDWR|O_CREAT|O_TRUNC, 0600);
        if (-1 == get_data.fd) {
                LOG(LOG_ERR, "open: %s: %s (%d)",
                    local, strerror(errno), errno);
                fd = -1;
                goto end;
        }

        encryption = check_encryption_flag(metadata);

        rc = dpl_openread(ctx,
                          (char *)remote,
                          encryption,
                          NULL,
                          cb_get_buffered,
                          &get_data,
                          &metadata);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_openread: %s", dpl_status_str(rc));
                close(get_data.fd);
                fd = -1;
                goto end;
        }

        /* If the file is compressed, uncompress it! */
        if(-1 == handle_compression(remote, local, &get_data, metadata)) {
                fd = -1;
                goto end;
        }

        if (-1 == close(get_data.fd)) {
                LOG(LOG_ERR, "close(path=%s, fd=%d): %s",
                    local, get_data.fd, strerror(errno));
                fd = -1;
                goto end;
        }

        fd = open(local, flags, 0600);
        if (-1 == fd) {
                LOG(LOG_ERR, "open(path=%s, fd=%d): %s",
                    local, fd, strerror(errno));
                fd = -1;
                goto end;
        }

  end:
        if (metadata)
                dpl_dict_free(metadata);

        if (headers)
                dpl_dict_free(headers);

        return fd;
}
