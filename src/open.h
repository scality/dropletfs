#ifndef OPEN_H
#define OPEN_H

#include <fuse.h>

int dfs_open(const char *, struct fuse_file_info *);

#endif /* OPEN_H */
