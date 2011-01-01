#ifndef GETATTR_H
#define GETATTR_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int dfs_getattr(const char *, struct stat *);

#endif /* GETATTR_H */
