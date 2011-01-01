#ifndef READ_H
#define READ_H

#include <fuse.h>

int dfs_read(const char *, char *, size_t, off_t, struct fuse_file_info *);

#endif /* READ_H */
