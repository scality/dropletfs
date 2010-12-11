#include <libgen.h>

#include "file.h"
#include "tmpstr.h"

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
	while (/*CONSTCOND*/ 1)	{
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
         char *buf,
         dpl_vfile_t *vfile)
{
        dpl_status_t ret = DPL_FAILURE;
        static int blksize = 4096;

        while (1)
        {
                int r = read(fd, buf, blksize);
                if (-1 == r) {
                        LOG("read on fd=%d %s", fd, strerror(errno));
                        return -1;
                }

                if (0 == r)
                        break;

                ret = dpl_write(vfile, buf, r);
                if (DPL_SUCCESS != ret) {
                        LOG("dpl_write: %s (%d)", dpl_status_str(ret), ret);
                        return -1;
                }
        }

        return 0;
}


dpl_status_t
cb_get_buffered(void *arg,
                char *buf,
                unsigned len)
{
        struct get_data *get_data = arg;
        dpl_status_t ret = DPL_SUCCESS;

        if (NULL != get_data->fp) {
                size_t s;

                s = fwrite(buf, 1, len, get_data->fp);
                if (s != len) {
                        perror("fwrite");
                        return -1;
                }
        } else {
                ret = write_all(get_data->fd, buf, len);
                if (DPL_SUCCESS != ret) {
                        ret = -1;
                        goto end;
                }
        }

        ret = 0;
end:

        return ret;
}



void
dfs_put_local_copy(dpl_ctx_t *ctx,
                   dpl_dict_t *dict,
                   struct fuse_file_info *info,
                   const char *remote)
{
        int fd = ((struct pentry *)info->fh)->fd;
        LOG("entering... remote=%s, info->fh=%d", remote, fd);
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;
        dpl_status_t ret = DPL_FAILURE;
        if (-1 == fd) {
                LOG("invalid fd (-1)");
                return;
        }

        char *s = dpl_dict_get_value(dict, "size");
        unsigned long size = s ? strtoul(s, NULL, 10) : 0;
        LOG("size=%lu", size);

        ret = dpl_openwrite(ctx,
                            (char *)remote,
                            DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                            dict,
                            canned_acl,
                            size,
                            &vfile);

        if (DPL_SUCCESS != ret) {
                LOG("dpl_openwrite: %s (%d)", dpl_status_str(ret), ret);
                goto err;
        }

        char *buf = malloc(size);
        if (! buf) {
                LOG("malloc(%ld bytes): %s (%d)",
                    size, strerror(errno), errno);
                goto err;
        }

        if (-1 == read_all(fd, buf, vfile))
                goto err;

  err:
        if (buf)
                free(buf);

        if (vfile)
                dpl_close(vfile);
}



/* return the fd of a local copy, to operate on */
int
dfs_get_local_copy(dpl_ctx_t *ctx,
                   const char *remote)
{
        dpl_dict_t *metadata = NULL;
        struct get_data get_data = { .fd = -1, .fp = NULL, .buf = NULL };

        char *local = tmpstr_printf("/tmp/%s/%s", ctx->cur_bucket, remote);

        LOG("bucket=%s, path=%s, local=%s", ctx->cur_bucket, remote, local);

        char *tmp_local = strdup(local);
        if (! tmp_local) {
                LOG("strdup: %s (%d)", strerror(errno), errno);
                return -1;
        }

        char *dir = dirname(tmp_local);
        if (-1 == mkdir(dir, 0755) && (EEXIST != errno))
                LOG("%s: %s (%d)", dir, strerror(errno), errno);

        free(tmp_local);

        /* a cache file already exist, remove it */
        if (0 == access(local, F_OK))
                unlink(local);

        get_data.fd = open(local, O_RDWR|O_CREAT, 0600);
        if (-1 == get_data.fd) {
                LOG("open: %s: %s (%d)", local, strerror(errno), errno);
                return -1;
        }

        dpl_status_t rc = dpl_openread(ctx,
                                       (char *)remote,
                                       0u,
                                       NULL,
                                       cb_get_buffered,
                                       &get_data,
                                       &metadata);

        if (DPL_SUCCESS != rc) {
                LOG("fd=%d, status: %s (%d)",
                    get_data.fd, dpl_status_str(rc), rc);
                close(get_data.fd);
                return -1;
        }

        if (metadata)
                dpl_dict_free(metadata);

err:
        fsync(get_data.fd);
        return get_data.fd;
}
