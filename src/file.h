#ifndef DROPLET_FILE_H
#define DROPLET_FILE_H

#include <droplet.h>
#include <fuse.h>

#include "hash.h"

struct get_data {
        struct buf *buf;
        int fd;
};

char *ftype_to_str(dpl_ftype_t);
char *flags_to_str(int);
int write_all(int, char *, int);
int read_write_all_vfile(int, dpl_vfile_t *);
int cb_get_buffered(void *, char *, unsigned);
/* return the fd of a local copy, to operate on */
int dfs_get_local_copy(pentry_t *, const char *, int);


#endif
