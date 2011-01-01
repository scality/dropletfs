#ifndef STATFS_H
#define STATFS_H

#include <sys/statvfs.h>

int dfs_statfs(const char *, struct statvfs *);

#endif /* STATFS_H */
