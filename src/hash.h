#ifndef DROPLET_HASH_H
#define DROPLET_HASH_H

#include "glob.h"

#define FLAG_DIRTY 1
#define FLAG_CLEAN 0

/* path entry on remote storage file system*/
struct pentry {
        int fd;
        char digest[MD5_DIGEST_LENGTH];
        dpl_dict_t *metadata;
        int flag;
};


struct pentry *pentry_new(void);

void pentry_set_fd(struct pentry *, int);

void pentry_set_metadata(struct pentry *, dpl_dict_t *);

void pentry_ctor(struct pentry *, int, int, dpl_dict_t *);

gboolean pentry_cmp_callback(gpointer, gpointer, gpointer);

int get_fd_from_path(char *);


#endif
