#ifndef FSYNC_H
#define FSYNC_H

#include <fuse.h>

int dfs_fsync(const char *, int, struct fuse_file_info *);

#endif /* FSYNC_H */
