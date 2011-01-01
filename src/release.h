#ifndef RELEASE_H
#define RELEASE_H

#include <fuse.h>

int dfs_release(const char *, struct fuse_file_info *);

#endif /* RELEASE_H */
