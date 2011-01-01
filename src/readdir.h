#ifndef READDIR_H
#define READDIR_H

#include <fuse.h>

int dfs_readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);

#endif /* READDIR_H */
