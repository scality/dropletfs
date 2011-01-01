#ifndef MKNOD_H
#define MKNOD_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int dfs_mknod(const char *, mode_t, dev_t);

#endif /* MKNOD_H */
