#ifndef WRITE_H
#define WRITE_H

#include <fuse.h>

int dfs_write(const char *, const char *, size_t, off_t, struct fuse_file_info *);

#endif /* WRITE_H */
