#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <sys/statvfs.h>

#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <droplet.h>

dpl_ctx_t *ctx;
FILE *fp;
static mode_t root_mode = 0;
static int debug = 0;

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
                return -1;                                              \
        } while(/*CONSTCOND*/0)


static char *
dfs_ftypetostr(dpl_ftype_t type)
{
        return DPL_FTYPE_REG == type ? "regular file" : "directory";
}

int
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
                return 0;
	}

        /*
         * TODO: in order to fill the other structure fields, we'd like to
         * parse the http headers, thus grab meta-data info, such as:
         *  -  x-amz-meta-mode
         *  -  x-amz-meta-mtime
         *     ...
         *
         */

        dpl_ftype_t type;
        dpl_ino_t ino, parent_ino, obj_ino;

        dpl_status_t rc = dpl_namei(ctx, (char *)path, ctx->cur_bucket,
                                    ino, &parent_ino, &obj_ino, &type);

        LOG("dpl_namei returned %s (%d), type = %s",
            dpl_status_str(rc), rc, dfs_ftypetostr(type));

        if (DPL_SUCCESS != rc)
                goto err;

        switch (type) {
        case DPL_FTYPE_DIR:
                buf->st_mode = S_IFDIR;
                break;
        case DPL_FTYPE_REG:
                buf->st_mode = S_IFREG;
                break;
        }

        rc = dpl_getattr(ctx, (char *)path, &metadata);
        if (DPL_SUCCESS != rc)
                LOG("dpl_getattr %s: %s\n", path, dpl_status_str(rc));

  err:

        if (metadata)
                free(metadata);

        return rc;
}


int
dfs_mkdir(const char *path,
          mode_t mode)
{
        LOG("path=%s, mode=%d", path, (int)mode);

        dpl_status_t rc = dpl_mkdir(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_mkdir, rc, path);

        return 0;
}

int
dfs_rmdir(const char *path)
{
        LOG("path=%s", path);

        dpl_status_t rc = dpl_rmdir(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_rmdir, rc, path);

        return 0;
}

int
dfs_unlink(const char *path)
{
        LOG("path=%s", path);

        dpl_status_t rc = dpl_unlink(ctx, (char *)path);
        DPL_CHECK_ERR(dpl_unlink, rc, path);

        return 0;
}

int
dfs_write(const char *path,
          const char *buf,
          size_t size,
          off_t offset,
          struct fuse_file_info *info)
{
        LOG("path=%s, buf=%p, size=%zu, offset=%lld, info=%p",
            path, (void *)buf, size, (long long)offset, (void *)info);

        /* missing parameters */
        dpl_status_t rc = dpl_write(NULL, (char *)buf, size);
        DPL_CHECK_ERR(dpl_write, rc, path);

        return 0;
}

static int fuse_filler(void *buf,
                       const char *name,
                       const struct stat *stbuf,
                       off_t off)
{
        return 0;
}


int
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

        return 0;
}


int
dfs_opendir(const char *path,
            struct fuse_file_info *info)
{
        LOG("path=%s, info=%p", path, (void *)info);

        void *dir_hdl;
        dpl_status_t rc = dpl_opendir(ctx, (char *)path, &dir_hdl);
        DPL_CHECK_ERR(dpl_opendir, rc, path);

        return 0;
}


int
dfs_statfs(const char *path, struct statvfs *buf)
{
        LOG("path=%s, buf=%p", path, (void *)buf);

        buf->f_flag = ST_RDONLY;
        buf->f_namemax = 255;
        buf->f_bsize = 4096;
        buf->f_frsize = buf->f_bsize;
        buf->f_blocks = buf->f_bfree = buf->f_bavail =
                1000ULL * 1024 / buf->f_frsize;
        buf->f_files = buf->f_ffree = 1000000000;

        return 0;
}



int dfs_mknod(const char *path, mode_t mode) { return 0; }
int dfs_readlink(const char *path, char *buf, size_t bufsiz) { return 0; }
int dfs_symlink(const char *oldpath, const char *newpath) { return 0; }
int dfs_rename(const char *oldpath, const char *newpath) { return 0; }
int dfs_chmod(const char *path, mode_t mode) { return 0; }
int dfs_chown(const char *path, uid_t uid, gid_t gid) { return 0; }
int dfs_truncate(const char *path, off_t offset) { return 0; }
int dfs_utime(const char *path, struct utimbuf *times) { return 0; }
int dfs_open(const char *path, struct fuse_file_info *info) { return 0; }
int dfs_flush(const char *path, struct fuse_file_info *info) { return 0; }
int dfs_fsync(const char *path, int issync, struct fuse_file_info *info) { return 0; }
int dfs_release(const char *path, struct fuse_file_info *info) { return 0; }
int dfs_read(const char *path, char *buf, size_t siwe, off_t offset, struct fuse_file_info *info) { return 0; }


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

        /* not implemented yet */
        .readlink   = dfs_readlink,
        .symlink    = dfs_symlink,
        .rename     = dfs_rename,
        .chmod      = dfs_chmod,
        .chown      = dfs_chown,
        .truncate   = dfs_truncate,
        .utime      = dfs_utime,
        .open       = dfs_open,
        .flush      = dfs_flush,
        .fsync      = dfs_fsync,
        .release    = dfs_release,
        .read       = dfs_read,

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
                debug = 1;
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

