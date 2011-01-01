#ifndef CREATE_H
#define CREATE_H

#include <fuse.h>

int dfs_create(const char *, mode_t, struct fuse_file_info *);

#endif /* CREATE_H */
