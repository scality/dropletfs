#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <sys/statvfs.h>
#include <libgen.h> /* dirmame() */

#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <droplet.h>


dpl_ctx_t *ctx;
FILE *fp;
static mode_t root_mode = 0;
static bool debug = false;


struct get_data {
        int fd;
        FILE *fp;
};

#define LOG(fmt, ...)                                                   \
        do {                                                            \
                if (! debug) break;                                     \
                time_t t = time(NULL);                                  \
                char buf[256] = "";                                     \
                strftime(buf, sizeof buf, "%T", gmtime(&t));            \
                fprintf(fp, "%s %s:%s():%d -- " fmt "\n",               \
                        buf, __FILE__, __func__, __LINE__, ##__VA_ARGS__); \
        } while (/*CONSTCOND*/0)

#define DPL_CHECK_ERR(func, rc, path)                                   \
        do {                                                            \
                if (DPL_SUCCESS == rc) break;                           \
                LOG("%s - %s", path, dpl_status_str(rc));               \
                return rc;                                              \
        } while(/*CONSTCOND*/0)


static char *
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

static void
display_attribute(dpl_var_t *var,
                  void *arg)
{
        LOG("attribute for object %s: %s=%s",
            (char *)arg, var->key, var->value);
}


static int
dfs_getattr(const char *path,
            struct stat *buf)
{
        LOG("path=%s, buf=%p", path, (void *)buf);

        assert(buf);
        memset(buf, 0, sizeof *buf);

        dpl_dict_t *metadata = NULL;

        /*
         * why setting st_nlink to 1?
         * see http://sourceforge.net/apps/mediawiki/fuse/index.php?title=FAQ
         * Section 3.3.5 "Why doesn't find work on my filesystem?"
         */
        buf->st_nlink = 1;

	if (strcmp(path, "/") == 0) {
		buf->st_mode = root_mode | S_IFDIR;
                return DPL_SUCCESS;
	}

        dpl_ftype_t type;
        dpl_ino_t ino, parent_ino, obj_ino;

        ino = dpl_cwd(ctx, ctx->cur_bucket);
        dpl_status_t rc = dpl_namei(ctx, (char *)path, ctx->cur_bucket,
                                    ino, &parent_ino, &obj_ino, &type);

        LOG("dpl_namei returned %s (%d), type=%s, parent_ino=%s, obj_ino=%s",
            dpl_status_str(rc), rc, dfs_ftypetostr(type),
            parent_ino.key, obj_ino.key);

        switch (type) {
        case DPL_FTYPE_DIR:
                buf->st_mode |= S_IFDIR;
                break;
        case DPL_FTYPE_REG:
                buf->st_mode |= S_IFREG;
                break;
        }

        if (DPL_SUCCESS != rc)
                goto err;

        rc = dpl_getattr(ctx, (char *)path, &metadata);
        if (DPL_SUCCESS != rc) {
                LOG("dpl_getattr %s: %s", path, dpl_status_str(rc));
                if (DPL_EISDIR == rc) rc = DPL_SUCCESS;
        }

  err:
        if (metadata) {
                dpl_dict_iterate(metadata, display_attribute, obj_ino.key);
                dpl_dict_free(metadata);
        }

        return rc;
}

static int
dfs_mkdir(const char *path,
          mode_t mode)
{
        LOG("path=%s, mode=0x%x", path, (int)mode);

        dpl_status_t rc = dpl_mkdir(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_mkdir, rc, path);

        return DPL_SUCCESS;
}

static int
dfs_rmdir(const char *path)
{
        LOG("path=%s", path);

        dpl_status_t rc = dpl_rmdir(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_rmdir, rc, path);

        return DPL_SUCCESS;
}

static int
dfs_unlink(const char *path)
{
        LOG("path=%s", path);

        dpl_status_t rc = dpl_unlink(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_unlink, rc, path);

        return DPL_SUCCESS;
}

static int
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



static dpl_status_t
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
                        return DPL_FAILURE;
                }
        }
        else {
                ret = write_all(get_data->fd, buf, len);
                if (DPL_SUCCESS != ret) {
                        ret = DPL_FAILURE;
                        goto end;
                }
        }

        ret = DPL_SUCCESS;
end:

        return ret;
}


static int
dfs_read(const char *path,
         char *buf,
         size_t size,
         off_t offset,
         struct fuse_file_info *info)
{
        LOG("path=%s, buf=%p, size=%zu, offset=%llu, fd=%"PRIu64,
            path, (void *)buf, size, (long long)offset, info->fh);

        int ret = pread(info->fh, buf, size, offset);
        if (-1 == ret) {
                LOG("%s - %s (%d)", path, strerror(errno), errno);
                close(info->fh);
                return EOF;
        }

        LOG("%s - successfully read %d bytes", path, ret);
        return ret;
}

static int
dfs_write(const char *path,
          const char *buf,
          size_t size,
          off_t offset,
          struct fuse_file_info *info)
{
        LOG("path=%s, buf=%p, size=%zu, offset=%lld, fd=%"PRIu64,
            path, (void *)buf, size, (long long)offset, info->fh);

        int ret = pwrite(info->fh, buf, size, offset);
        DPL_CHECK_ERR(pwrite, ret, path);


        LOG("%s - successfully wrote %d bytes", path, ret);
        return size;
}

static int
fuse_filler(void *buf,
            const char *name,
            const struct stat *stbuf,
            off_t off)
{
        return DPL_SUCCESS;
}


static int
dfs_readdir(const char *path,
            void *data,
            fuse_fill_dir_t fill,
            off_t offset,
            struct fuse_file_info *info)
{
        LOG("path=%s, data=%p, fill=%p, offset=%lld, info=%p",
            path, data, (void *)fill, (long long)offset, (void *)info);

        void *dir_hdl;
        dpl_dirent_t dirent;

        dpl_status_t rc = dpl_chdir(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_chdir, rc, path);

        rc = dpl_opendir(ctx, ".", &dir_hdl);
        DPL_CHECK_ERR(dpl_opendir, rc, ".");

        while (DPL_SUCCESS == dpl_readdir(dir_hdl, &dirent)) {
                if (0 != fill(data, dirent.name, NULL, 0))
                        break;
        }

        dpl_closedir(dir_hdl);

        return DPL_SUCCESS;
}


static int
dfs_opendir(const char *path,
            struct fuse_file_info *info)
{
        LOG("path=%s, info=%p", path, (void *)info);

        void *dir_hdl;
        dpl_status_t rc = dpl_opendir(ctx, (char *)path, &dir_hdl);
        DPL_CHECK_ERR(dpl_opendir, rc, path);

        return DPL_SUCCESS;
}


static int
dfs_statfs(const char *path, struct statvfs *buf)
{
        LOG("path=%s, buf=%p", path, (void *)buf);

        buf->f_flag = ST_RDONLY;
        buf->f_namemax = 255;
        buf->f_bsize = 4096;
        buf->f_frsize = buf->f_bsize;
        buf->f_blocks = buf->f_bfree = buf->f_bavail =
                (1000ULL * 1024) / buf->f_frsize;
        buf->f_files = buf->f_ffree = 1000000000;

        return DPL_SUCCESS;
}

static int
dfs_release(const char *path,
            struct fuse_file_info *info)
{
        int fd = info->fh;
        LOG("%s, fd=%d", path, fd);

        if (-1 != fd) {
                if (-1 == close(fd)) {
                        LOG("%s - %s", path, strerror(errno));
                        return DPL_EIO;
                }
        }

        return DPL_SUCCESS;
}




/* return the fd of a local copy, to operate on */
static int
dfs_get_local_copy(dpl_ctx_t *ctx,
                   const char *remote)
{
        char local[256] = "";
        dpl_dict_t *metadata = NULL;
        struct get_data get_data;
        get_data.fd = -1;
        get_data.fp = NULL;

        snprintf(local, sizeof local, "/tmp/%s/%s", ctx->cur_bucket, remote);

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
        if (0 == access(local, F_OK)) unlink(local);

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
        if (DPL_SUCCESS != rc)
                LOG("status: %s (%d)\n", dpl_status_str(rc), rc);

err:

        if (metadata) {
                dpl_dict_iterate(metadata, display_attribute, (void *)remote);
                dpl_dict_free(metadata);
        }

        return get_data.fd;
}

static int
dfs_open(const char *path,
         struct fuse_file_info *info)
{
        LOG("%s", path);

        int fd = dfs_get_local_copy(ctx, path);

        if (-1 == fd) goto err;

        info->fh = fd;
        info->flags = O_RDONLY;

err:
        LOG("open fd=%d, flags=0x%X", fd, info->flags);
        return DPL_SUCCESS;
}

static int
dfs_fsync(const char *path,
          int issync,
          struct fuse_file_info *info)
{
        LOG("%s", path);

        if (! issync && -1 == fsync((int)info->fh)) {
                LOG("%s - %s", path, strerror(errno));
                return DPL_EIO;
        }

        return DPL_SUCCESS;
}

static int
dfs_setxattr(const char *path,
             const char *name,
             const char *value,
             size_t size,
             int flag)
{
        LOG("path=%s, name=%s, value=%s, size=%zu, flag=%d",
            path, name, value, size, flag);

        dpl_dict_t *metadata = NULL;

        dpl_status_t rc = dpl_dict_add(metadata, (char *)name, (char *)value, 0);
        DPL_CHECK_ERR(dpl_dict_add, rc, path);

        rc = dpl_setattr(ctx, (char *)path, metadata);
        DPL_CHECK_ERR(dpl_setattr, rc, path);

        return DPL_SUCCESS;
}

static int
dfs_create(const char *path,
           mode_t mode,
           struct fuse_file_info *info)
{
        LOG("%s", path);

        dpl_ftype_t type;
        dpl_ino_t ino, obj, parent;

        ino = dpl_cwd(ctx, ctx->cur_bucket);
        dpl_status_t rc = dpl_namei(ctx, (char *)path, ctx->cur_bucket,
                                    ino, &parent, &obj, &type);

        LOG("path=%s, ino=%s, parent=%s, obj=%s, type=%s, rc=%s (%d)",
            path, ino.key, parent.key, obj.key, dfs_ftypetostr(type),
            dpl_status_str(rc), rc);
        if (DPL_ENOENT != rc) {
                LOG("dpl_namei: %s (%d)", dpl_status_str(rc), rc);
                return rc;
        }

        /* OK, is it a regular file? */
        switch (DPL_FTYPE_REG != type) {
                LOG("Unsupported filetype %s (%d)", dfs_ftypetostr(type), type);
                return DPL_EIO;
        }

        info->flags = mode;
        return dfs_open(path, info);
}



/* Not implemented yet */

static int
dfs_getxattr(const char *path,
             const char *name,
             char *value,
             size_t size)
{
        LOG("path=%s, name=%s, value=%s, size=%zu",
            path, name, value, size);
        return DPL_SUCCESS;
}

static int
dfs_listxattr(const char *path,
              char *list,
              size_t size)
{
        LOG("path=%s, list=%s, size=%zu", path, list, size);
        return DPL_SUCCESS;
}

static int
dfs_removexattr(const char *path,
                const char *name)
{
        LOG("path=%s, name=%s", path, name);
        return DPL_SUCCESS;
}

static int
dfs_flush(const char *path,
          struct fuse_file_info *info)
{
        LOG("%s, fd=%d", path, (int)info->fh);
        return DPL_SUCCESS;
}

static int
dfs_mknod(const char *path,
          mode_t mode)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_readlink(const char *path,
             char *buf,
             size_t bufsiz)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_symlink(const char *oldpath,
            const char *newpath)
{
        LOG("%s -> %s", oldpath, newpath);
        return DPL_SUCCESS;
}

static int
dfs_rename(const char *oldpath,
           const char *newpath)
{
        LOG("%s -> %s", oldpath, newpath);
        return DPL_SUCCESS;
}

static int
dfs_chmod(const char *path,
          mode_t mode)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_chown(const char *path,
          uid_t uid,
          gid_t gid)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_truncate(const char *path,
             off_t offset)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_utime(const char *path,
          struct utimbuf *times)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_releasedir(const char *path,
               struct fuse_file_info *info)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_fsyncdir(const char *path,
             int datasync,
             struct fuse_file_info *info)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static void *
dfs_init(struct fuse_conn_info *conn)
{
        LOG("Entering function");
        return NULL;
}

static void
dfs_destroy(void *arg)
{
        LOG("%p", arg);
}

static int
dfs_access(const char *path, int perm)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_ftruncate(const char *path,
              off_t offset,
              struct fuse_file_info *info)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_fgetattr(const char *path,
             struct stat *buf,
             struct fuse_file_info *info)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_lock(const char *path,
         struct fuse_file_info *info,
         int cmd,
         struct flock *flock)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_utimens(const char *path,
            const struct timespec tv[2])
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_bmap(const char *path,
         size_t blocksize,
         uint64_t *idx)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

#if 0
static int
dfs_ioctl(const char *path,
          int cmd,
          void *arg,
          struct fuse_file_info *info,
          unsigned int flags,
          void *data)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}

static int
dfs_poll(const char *path,
         struct fuse_file_info *info,
         struct fuse_pollhandle *ph,
         unsigned *reventsp)
{
        LOG("%s", path);
        return DPL_SUCCESS;
}
#endif

struct fuse_operations dfs_ops = {
        /* implemented */
        .getattr    = dfs_getattr,
        .mkdir      = dfs_mkdir,
        .write      = dfs_write,
        .readdir    = dfs_readdir,
        .opendir    = dfs_opendir,
        .unlink     = dfs_unlink,
        .rmdir      = dfs_rmdir,
        .statfs     = dfs_statfs,
        .read       = dfs_read,
        .release    = dfs_release,
        .open       = dfs_open,
        .fsync      = dfs_fsync,
        .setxattr   = dfs_setxattr,
        .create     = dfs_create,

        /* not implemented yet */
        .getxattr   = dfs_getxattr,
        .listxattr  = dfs_listxattr,
        .removexattr= dfs_removexattr,
        .readlink   = dfs_readlink,
        .symlink    = dfs_symlink,
        .rename     = dfs_rename,
        .chmod      = dfs_chmod,
        .chown      = dfs_chown,
        .truncate   = dfs_truncate,
        .utime      = dfs_utime,
        .flush      = dfs_flush,
        .setxattr   = dfs_setxattr,
        .fsyncdir   = dfs_fsyncdir,
        .init       = dfs_init,
        .destroy    = dfs_destroy,
        .access     = dfs_access,
        .ftruncate  = dfs_ftruncate,
        .fgetattr   = dfs_fgetattr,
        .lock       = dfs_lock,
        .utimens    = dfs_utimens,
        .bmap       = dfs_bmap,
#if 0
        .iotcl      = dfs_ioctl,
        .poll       = dfs_poll,
#endif
        .getdir     = NULL, /* deprecated */
        .link       = NULL, /* no support needed */
};


static int
dfs_fuse_main(struct fuse_args *args)
{
        return fuse_main(args->argc, args->argv, &dfs_ops, NULL);
}


static void
droplet_pp(dpl_ctx_t *ctx)
{
#define PP(field, fmt) fprintf(stdout, #field": "fmt"\n", ctx->field)
        PP(n_conn_buckets, "%d");
        PP(n_conn_max, "%d");
        PP(n_conn_max_hits, "%d");
        PP(conn_idle_time , "%d");
        PP(conn_timeout, "%d");
        PP(read_timeout, "%d");
        PP(write_timeout, "%d");
        PP(use_https, "%d");
        PP(host, "%s");
        PP(port, "%d");

        PP(access_key, "%s");
        PP(secret_key, "%s");
        PP(ssl_cert_file, "%s");
        PP(ssl_key_file, "%s");
        PP(ssl_password, "%s");
        PP(ssl_ca_list, "%s");

        PP(trace_level, "%d");

        PP(pricing, "%s");
        PP(read_buf_size, "%u");
        PP(encrypt_key, "%s");
        PP(delim, "%s");

        PP(n_conn_fds, "%d");
        PP(cur_bucket, "%s");
        PP(droplet_dir, "%s");
        PP(profile_name, "%s");
#undef PP
}

static void
usage(const char * const prog)
{
        printf("Usage: %s <bucket> <mount point>\n", prog);
}

int
main(int argc, char **argv)
{
        int rc = EXIT_FAILURE;
        char *bucket = NULL;
        fp = fopen("/tmp/fuse.log", "a");
        if (! fp) {
                goto err0;
        }

        if (argc < 3) {
                usage(argv[0]);
                goto err1;
        }

        bucket = argv[1];
        argc -= 1;
        argv += 1;

        if (0 == strncmp(argv[1], "-d", 2)) {
                debug = true;
                argc -= 1;
                argv += 1;
        }

        struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	dpl_status_t st = dpl_init();
	if (DPL_SUCCESS != st) {
		fprintf(fp, "dpl_init: %s\n", dpl_status_str(st));
                goto err1;
	}

	ctx = dpl_ctx_new(NULL, NULL);
	if (! ctx) {
                goto err2;
	}

        ctx->trace_level = ~0;
        ctx->cur_bucket = strdup(bucket);
        droplet_pp(ctx);

        rc = dfs_fuse_main(&args);
        dpl_ctx_free(ctx);
err2:
	dpl_free();
err1:
        fclose(fp);
err0:
	return rc;
}

