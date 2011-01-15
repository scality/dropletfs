#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/statvfs.h>
#include <libgen.h>
#include <glib.h>

#define FUSE_USE_VERSION 29
#include <fuse.h>

#include "hash.h"
#include "tmpstr.h"
#include "log.h"
#include "glob.h"

#include "open.h"
#include "create.h"
#include "statfs.h"
#include "mknod.h"
#include "read.h"
#include "write.h"
#include "release.h"
#include "unlink.h"

#include "getattr.h"
#include "setxattr.h"

#include "opendir.h"
#include "mkdir.h"
#include "readdir.h"
#include "rmdir.h"

#include "fsync.h"
#include "rename.h"
#include "chmod.h"
#include "chown.h"


#define DEFAULT_COMPRESSION_METHOD "NONE"
#define DEFAULT_ZLIB_LEVEL 3
#define DEFAULT_CACHE_DIR "/tmp"

dpl_ctx_t *ctx = NULL;
FILE *fp = NULL;
mode_t root_mode = 0;
int debug = 0;
GHashTable *hash = NULL;
unsigned long zlib_level = 0;
char *compression_method = NULL;
char *cache_dir = NULL;

static void
atexit_callback(void)
{
        if (hash)
                g_hash_table_remove_all(hash);
	dpl_free();
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
        return 0;
}

static int
dfs_listxattr(const char *path,
              char *list,
              size_t size)
{
        LOG("path=%s, list=%s, size=%zu", path, list, size);
        return 0;
}

static int
dfs_removexattr(const char *path,
                const char *name)
{
        LOG("path=%s, name=%s", path, name);
        return 0;
}

static int
dfs_flush(const char *path,
          struct fuse_file_info *info)
{
        (void)info;

        LOG("%s", path);
        return 0;
}

static int
dfs_readlink(const char *path,
             char *buf,
             size_t bufsiz)
{
        (void)buf;
        (void)bufsiz;

        LOG("%s", path);
        return 0;
}

static int
dfs_symlink(const char *oldpath,
            const char *newpath)
{
        LOG("%s -> %s", oldpath, newpath);
        return 0;
}

static int
dfs_truncate(const char *path,
             off_t offset)
{
        (void)offset;

        LOG("%s", path);
        return 0;
}

static int
dfs_utime(const char *path,
          struct utimbuf *times)
{
        (void)times;

        LOG("%s", path);
        return 0;
}

static int
dfs_releasedir(const char *path,
               struct fuse_file_info *info)
{
        (void)info;

        LOG("%s", path);
        return 0;
}

static int
dfs_fsyncdir(const char *path,
             int datasync,
             struct fuse_file_info *info)
{
        (void)datasync;
        (void)info;

        LOG("%s", path);
        return 0;
}

static void *
dfs_init(struct fuse_conn_info *conn)
{
        (void)conn;

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
        (void)perm;

        LOG("%s", path);
        return 0;
}

static int
dfs_ftruncate(const char *path,
              off_t offset,
              struct fuse_file_info *info)
{
        (void)offset;
        (void)info;

        LOG("%s", path);
        return 0;
}

static int
dfs_lock(const char *path,
         struct fuse_file_info *info,
         int cmd,
         struct flock *flock)
{
        (void)info;
        (void)cmd;
        (void)flock;

        LOG("%s", path);
        return 0;
}

static int
dfs_utimens(const char *path,
            const struct timespec tv[2])
{
        (void)path;
        (void)tv;

        LOG("%s", path);
        return 0;
}

static int
dfs_bmap(const char *path,
         size_t blocksize,
         uint64_t *idx)
{
        (void)blocksize;
        (void)idx;

        LOG("%s", path);
        return 0;
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
        return 0;
}

static int
dfs_poll(const char *path,
         struct fuse_file_info *info,
         struct fuse_pollhandle *ph,
         unsigned *reventsp)
{
        (void)info;
        (void)ph;
        (void)reventsp;

        LOG("%s", path);
        return 0;
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
        .chmod      = dfs_chmod,
        .chown      = dfs_chown,
        .mknod      = dfs_mknod,

        /* not implemented yet */
        .getxattr   = dfs_getxattr,
        .listxattr  = dfs_listxattr,
        .removexattr= dfs_removexattr,
        .readlink   = dfs_readlink,
        .symlink    = dfs_symlink,
        .rename     = dfs_rename,
        .truncate   = dfs_truncate,
        .utime      = dfs_utime,
        .flush      = dfs_flush,
        .fsyncdir   = dfs_fsyncdir,
        .init       = dfs_init,
        .destroy    = dfs_destroy,
        .access     = dfs_access,
        .releasedir = dfs_releasedir,
        .ftruncate  = dfs_ftruncate,
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
        hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                     free, (GDestroyNotify)pentry_free);
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
        printf("Usage: %s <bucket> [-d] <mount point> [options]\n", prog);
        printf("\t<bucket>\tthe name of your remote bucket\n");
        printf("\t-d\t\tset the debug mode. Information are written in syslog\n");
        printf("\t<mountpoint>\tthe directory you want to use as mount point.\n");
        printf("\t\t\tThe directory must exist\n");
        printf("\t[options]\tfuse/mount options\n");
}

static int
set_cache_dir(void)
{
        char *tmp = NULL;
        char *p = NULL;

        tmp = getenv("DROPLETFS_CACHE_DIR");
        if (! tmp)
                tmp = DEFAULT_CACHE_DIR;

        cache_dir = tmpstr_printf("%s/%s", tmp, ctx->cur_bucket);
        if (-1 == mkdir(cache_dir, 0777) && EEXIST != errno) {
                LOG("mkdir(%s) = %s", cache_dir, strerror(errno));
                return -1;
        }

        LOG("cache directory created: '%s'", cache_dir);
        /* remove the trailing spaces */
        p = cache_dir + strlen(cache_dir) - 1;
        while (p && '/' == *p) {
                *p = 0;
                p--;
        }

        return 0;
}

static void
set_compression_env(void)
{
        char *tmp = NULL;

        tmp = getenv("DROPLETFS_COMPRESSION_METHOD");
        LOG("DROPLETFS_COMPRESSION_METHOD=%s", tmp);

        if (tmp) {
                if (0 == strncasecmp(tmp, "none", strlen("none"))) {
                        compression_method = "none";
                        return;
                }
                if (0 != strncasecmp(tmp, "zlib", strlen("zlib")))
                        LOG("unknown method '%s'", tmp);
        }

        compression_method = DEFAULT_COMPRESSION_METHOD;
        tmp = getenv("DROPLETFS_ZLIB_LEVEL");
        LOG("DROPLETFS_ZLIB_LEVEL=%s", tmp);

        if (tmp)
                zlib_level = strtoul(tmp, NULL, 10);
        else
                zlib_level = DEFAULT_ZLIB_LEVEL;
}


static void
set_dplfs_env(void)
{
        set_compression_env();

        if (-1 == set_cache_dir()) {
                LOG("can't create any cache directory");
                exit(EXIT_FAILURE);
        }
}

int
main(int argc, char **argv)
{
        root_mode = 0;
        debug = 0;
        hash = NULL;
        char *bucket = NULL;
        dpl_status_t rc = DPL_FAILURE;

        atexit(atexit_callback);

        openlog("dplfs", LOG_CONS | LOG_NOWAIT | LOG_PID, LOG_USER);

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

	rc = dpl_init();
	if (DPL_SUCCESS != rc) {
		fprintf(fp, "dpl_init: %s\n", dpl_status_str(rc));
                goto err1;
	}

	ctx = dpl_ctx_new(NULL, NULL);
	if (! ctx)
                goto err2;

        ctx->trace_level = ~0;
        ctx->cur_bucket = strdup(bucket);
        droplet_pp(ctx);

        set_dplfs_env();
        LOG("zlib compression level set to: %lu", zlib_level);
        LOG("zlib compression method set to: %s", compression_method);

        rc = dfs_fuse_main(&args);

        dpl_ctx_free(ctx);
        if (hash)
                g_hash_table_remove_all(hash);
  err2:
	dpl_free();
  err1:
	return rc;
}

