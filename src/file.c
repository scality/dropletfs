#include <libgen.h>
#include <unistd.h>
#include <errno.h>

#include "glob.h"
#include "misc.h"
#include "log.h"
#include "file.h"
#include "tmpstr.h"
#include "metadata.h"

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

                rc = dpl_write(vfile, buf, r);
                if (DPL_SUCCESS != rc) {
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



void
dfs_put_local_copy(dpl_dict_t *dict,
                   struct fuse_file_info *info,
                   const char *remote)
{
        int fd = ((struct pentry *)info->fh)->fd;
        LOG("entering... remote=%s, info->fh=%d", remote, fd);
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;
        dpl_status_t rc = DPL_FAILURE;
        char *s = NULL;
        unsigned long size = 0;

        if (-1 == fd) {
                LOG("invalid fd");
                return;
        }

        s = dpl_dict_get_value(dict, "size");
        size = s ? strtoul(s, NULL, 10) : 0;
        LOG("size=%lu", size);

        rc = dpl_openwrite(ctx,
                           (char *)remote,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           size,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_openwrite: %s", dpl_status_str(rc));
                goto err;
        }

        if (-1 == read_all(fd, vfile))
                goto err;

  err:

        if (vfile)
                dpl_close(vfile);
}

static int
dfs_md5cmp(struct pentry *pe,
           char *path)
{
        dpl_dict_t *dict = NULL;
        char *remote_md5 = NULL;
        int diff = 1;
        dpl_status_t rc = DPL_FAILURE;
        dpl_ino_t ino, obj_ino;

        rc = dpl_namei(ctx, path, ctx->cur_bucket, ino, NULL, &obj_ino, NULL);

        if (DPL_ENOENT == rc)
                return diff;

        rc = dpl_head_all(ctx, ctx->cur_bucket, obj_ino.key, NULL, NULL, &dict);
        print_metadata(dict);

        if (DPL_SUCCESS != rc)
                goto err;


        if (dict)
                remote_md5 = dpl_dict_get_value(dict, "etag");

        if (remote_md5) {
                LOG("remote md5=%s", remote_md5);
                LOG("local md5=%.*s", MD5_DIGEST_LENGTH, pe->digest);
                diff = strncasecmp(pe->digest, remote_md5, MD5_DIGEST_LENGTH);
                if (diff) {
                        pentry_set_digest(pe, remote_md5);
                        LOG("[updated] local md5=%s", pe->digest);
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

        local = tmpstr_printf("/tmp/%s/%s", ctx->cur_bucket, path);

        tmp_local = strdup(local);

        if (! tmp_local) {
                LOG("strdup: %s (%d)", strerror(errno), errno);
                return NULL;
        }

        mkdir_tree(dirname(tmp_local));
        free(tmp_local);

        return local;
}


/* return the fd of a local copy, to operate on */
int
dfs_get_local_copy(struct pentry *pe,
                   const char *remote)
{
        dpl_dict_t *metadata = NULL;
        struct get_data get_data = { .fd = -1, .buf = NULL };
        dpl_status_t rc = DPL_FAILURE;
        char *local = NULL;

        local = tmpstr_printf("/tmp/%s/%s", ctx->cur_bucket, remote);
        LOG("bucket=%s, path=%s, local=%s", ctx->cur_bucket, remote, local);

        /* If the remote MD5 matches a cache file, we don't have to download
         * it again, just return the (open) file descriptor of the cache file
         */
        if (0 == dfs_md5cmp(pe, (char *)remote))
                return pe->fd;

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

        if (metadata) {
                pentry_set_metadata(pe, metadata);
                dpl_dict_free(metadata);
        }

        fsync(get_data.fd);
        return get_data.fd;
}
