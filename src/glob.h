#ifndef DROPLET_GLOB_H
#define DROPLET_GLOB_H

#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib.h>
#include <time.h>
#include <syslog.h>
#include <stdio.h>
#include <errno.h>

#include <fuse.h>
#include <droplet.h>

#include "misc.h"
#include "hash.h"

extern dpl_ctx_t *ctx;
extern FILE *fp;
extern mode_t root_mode;
extern int debug;
extern GHashTable *hash;



#define LOG(fmt, ...)                                                   \
        do {                                                            \
                if (! debug) break;                                     \
                syslog(LOG_INFO, "%s:%s():%d -- " fmt "",               \
                       __FILE__, __func__, __LINE__, ##__VA_ARGS__);    \
        } while (/*CONSTCOND*/0)

#define DPL_CHECK_ERR(func, rc, path)                                   \
        do {                                                            \
                if (DPL_SUCCESS == rc) break;                           \
                LOG("%s - %s: %s", #func, path, dpl_status_str(rc));    \
                return rc;                                              \
        } while(/*CONSTCOND*/0)

#define PRINT_FLAGS(path, info)                                         \
        do {                                                            \
                switch (info->flags & O_ACCMODE) {                      \
                case O_RDONLY:                                          \
                        LOG("%s: read only", path);                     \
                        break;                                          \
                case O_WRONLY:                                          \
                        LOG("%s: write only", path);                    \
                        break;                                          \
                case O_RDWR:                                            \
                        LOG("%s: read/write", path);                    \
                        break;                                          \
                default:                                                \
                        LOG("%s: unknown flags 0x%x",                   \
                            path, info->flags & O_ACCMODE);             \
                }                                                       \
        } while (/*CONSTCOND*/ 0)


struct statvfs;
struct fuse_file_info;


/* FUSE handlers */
int dfs_statfs(const char *, struct statvfs *);

int dfs_open(const char *, struct fuse_file_info *);
int dfs_create(const char *, mode_t, struct fuse_file_info *);
int dfs_mknod(const char *, mode_t, dev_t);
int dfs_read(const char *, char *, size_t, off_t, struct fuse_file_info *);
int dfs_write(const char *, const char *, size_t, off_t, struct fuse_file_info *);
int dfs_release(const char *, struct fuse_file_info *);
int dfs_unlink(const char *);

int dfs_getattr(const char *, struct stat *);
int dfs_setxattr(const char *, const char *, const char *, size_t, int );

int dfs_opendir(const char *, struct fuse_file_info *);
int dfs_mkdir(const char *, mode_t);
int dfs_readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
int dfs_rmdir(const char *);

int dfs_fsync(const char *, int, struct fuse_file_info *);
int dfs_rename(const char *, const char *);
int dfs_chmod(const char *, mode_t);
int dfs_chown(const char *, uid_t, gid_t);



#endif
