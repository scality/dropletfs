#include <libgen.h>
#include <unistd.h>
#include <errno.h>

#include "glob.h"
#include "misc.h"
#include "log.h"
#include "file.h"
#include "tmpstr.h"
#include "metadata.h"
#include "zip.h"

extern unsigned long zlib_level;
extern char *cache_dir;
extern int max_retry;

char *
dfs_ftypetostr(dpl_ftype_t type)
{
        switch (type) {
        case DPL_FTYPE_REG:
                return "regular file";
        case DPL_FTYPE_DIR:
                return "directory";
        }
        return "unknown type";
}


int
write_all(int fd,
          char *buf,
          int len)
{
        LOG("fd=%d, len=%d", fd, len);

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
read_all(int fd,
         dpl_vfile_t *vfile)
{
        dpl_status_t rc = DPL_FAILURE;
        static int blksize = 4096;
        char *buf = NULL;
        int tries = 0;
        int delay = 1;

        buf = alloca(blksize);
        while (1)
        {
                int r = read(fd, buf, blksize);
                if (-1 == r) {
                        LOG("read on fd=%d %s", fd, strerror(errno));
                        return -1;
                }

                if (0 == r)
                        break;
        retry_write:
                rc = dpl_write(vfile, buf, r);
                if (DPL_SUCCESS != rc) {
                        if (tries < max_retry) {
                                LOG("dpl_write: timeout?");
                                tries++;
                                sleep(delay);
                                delay *= 2;
                                goto retry_write;
                        }
                        LOG("dpl_write: %s (%d)", dpl_status_str(rc), rc);
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


static int
dfs_md5cmp(pentry_t *pe,
           char *path)
{
        dpl_dict_t *dict = NULL;
        char *remote_md5 = NULL;
        int diff = 1;
        dpl_status_t rc = DPL_FAILURE;
        dpl_ino_t ino, obj_ino;
        char *digest = NULL;
        int tries = 0;
        int delay = 1;

        digest = pentry_get_digest(pe);
        if (! digest) {
                LOG("no digest");
                goto err;
        }

 namei_retry:
        rc = dpl_namei(ctx, path, ctx->cur_bucket, ino, NULL, &obj_ino, NULL);
        if (DPL_ENOENT == rc) {
                if (DPL_ENOENT != rc && (tries < max_retry)) {
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        LOG("namei timeout? (%s)", dpl_status_str(rc));
                        goto namei_retry;
                }
                LOG("dpl_namei: %s", dpl_status_str(rc));
                goto err;
        }

        rc = dpl_head_all(ctx, ctx->cur_bucket, obj_ino.key, NULL, NULL, &dict);
        if (DPL_SUCCESS != rc) {
                LOG("dpl_head_all: %s", dpl_status_str(rc));
                goto err;
        }

        print_metadata(dict);

        if (dict)
                remote_md5 = dpl_dict_get_value(dict, "etag");

        if (remote_md5) {
                LOG("remote md5=%s", remote_md5);
                LOG("local md5=%.*s", MD5_DIGEST_LENGTH, digest);
                diff = memcmp(digest, remote_md5, MD5_DIGEST_LENGTH);
                if (diff) {
                        pentry_set_digest(pe, remote_md5);
                        LOG("updated local md5=%.*s",
                            MD5_DIGEST_LENGTH, pentry_get_digest(pe));
                }
        }

  err:
        if (dict)
                dpl_dict_free(dict);

        return diff;

}


char *
build_cache_tree(const char *path)
{
        LOG("building cache dir for '%s'", path);

        char *local = NULL;
        char *tmp_local = NULL;

        /* ignore the leading spaces */
        while (path && '/' == *path)
                path++;

        local = tmpstr_printf("%s/%s", cache_dir, path);

        tmp_local = strdup(local);

        if (! tmp_local) {
                LOG("strdup: %s (%d)", strerror(errno), errno);
                return NULL;
        }

        mkdir_tree(dirname(tmp_local));
        free(tmp_local);

        return local;
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
                LOG("%s: uncompressed remote file", remote);
                ret = 0;
                goto end;
        }

#define NONE "none"
#define ZLIB "zlib"
        if (0 == strncmp(compressed, NONE, strlen(NONE))) {
                LOG("compression method: 'none'");
                ret = 0;
                goto end;
        }

        if (0 != strncmp(compressed, ZLIB, strlen(ZLIB))) {
                LOG("compression method not supported '%s'", compressed);
                ret = -1;
                goto end;
        }

        uzlocal = tmpstr_printf("%s.tmp", local);

        fpsrc = fopen(local, "r");
        if (! fpsrc) {
                LOG("fopen: %s", strerror(errno));
                ret = -1;
                goto end;
        }

        fpdst = fopen(uzlocal, "w");
        if (! fpdst) {
                LOG("fopen: %s", strerror(errno));
                ret = -1;
                goto end;
        }

        LOG("uncompressing local file '%s'", local);

        zret = unzip(fpsrc, fpdst);
        if (Z_OK != zret) {
                LOG("unzip failed: %s", zerr_to_str(zret));
                ret = -1;
                goto end;
        }

        rc = dpl_dict_update_value(metadata, "compression", "none");
        if (DPL_SUCCESS != rc) {
                LOG("unable to update 'compression' metadata");
                ret = -1;
                goto end;
        }

        if (-1 == rename(uzlocal, local)) {
                LOG("rename: %s", strerror(errno));
                ret = 1;
                goto end;
        }

        close(get_data->fd);
        get_data->fd = open(local, O_RDONLY);
        if (-1 == get_data->fd) {
                LOG("open: %s", strerror(errno));
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


/* return the fd of a local copy, to operate on */
int
dfs_get_local_copy(pentry_t *pe,
                   const char *remote)
{
        dpl_dict_t *metadata = NULL;
        struct get_data get_data = { .fd = -1, .buf = NULL };
        dpl_status_t rc = DPL_FAILURE;
        char *local = NULL;

        local = tmpstr_printf("%s/%s", cache_dir, remote);
        LOG("bucket=%s, path=%s, local=%s", ctx->cur_bucket, remote, local);

        /* If the remote MD5 matches a cache file, we don't have to download
         * it again, just return the (open) file descriptor of the cache file
         */
        if (0 == dfs_md5cmp(pe, (char *)remote))
                return pentry_get_fd(pe);

        /* a cache file already exist, its MD5 digest is different, so...
         * just remove it */
        if (0 == access(local, F_OK))
                unlink(local);

        get_data.fd = open(local, O_RDWR|O_CREAT, 0600);
        if (-1 == get_data.fd) {
                LOG("open: %s: %s (%d)", local, strerror(errno), errno);
                return -1;
        }

        rc = dpl_openread(ctx,
                          (char *)remote,
                          0u, /* no encryption */
                          NULL,
                          cb_get_buffered,
                          &get_data,
                          &metadata);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_openread: %s", dpl_status_str(rc));
                close(get_data.fd);
                return -1;
        }

        /* If the file is compressed, uncompress it! */
        if(-1 == handle_compression(remote, local, &get_data, metadata))
                goto end;

  end:
        if (metadata) {
                pentry_set_metadata(pe, metadata);
                dpl_dict_free(metadata);
        }

        return get_data.fd;
}
