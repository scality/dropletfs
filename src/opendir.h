#ifndef OPENDIR_H
#define OPENDIR_H

#include <fuse.h>

int dfs_opendir(const char *, struct fuse_file_info *);

#endif /* OPENDIR_H */
