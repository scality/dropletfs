#ifndef DROPLET_FILE_H
#define DROPLET_FILE_H

#include <droplet.h>
#include <fuse.h>

#include "hash.h"

struct get_data {
        struct buf *buf;
        int fd;
};

char *dfs_ftypetostr(dpl_ftype_t);
int write_all(int, char *, int);
int read_all(int, dpl_vfile_t *);
int cb_get_buffered(void *, char *, unsigned);
char *build_cache_tree(const char *);
void dfs_put_local_copy(dpl_dict_t *, struct fuse_file_info *, const char *);
/* return the fd of a local copy, to operate on */
int dfs_get_local_copy(pentry_t *, const char *);


#endif
