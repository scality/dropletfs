#ifndef DROPLET_HASH_H
#define DROPLET_HASH_H

#include "glob.h"


/* path entry on remote storage file system*/
struct pentry {
        int fd;
        char digest[MD5_DIGEST_LENGTH];
};

struct pentry * pentry_new(void);

void pentry_ctor(struct pentry *pe, int fd);

gboolean pentry_cmp_callback(gpointer key, gpointer value, gpointer data);

int get_fd_from_path(char *path);


#endif
