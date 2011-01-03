#ifndef DROPLET_HASH_H
#define DROPLET_HASH_H

#include <glib.h>
#include <droplet.h>

/* path entry on remote storage file system */
struct pentry {
        int fd;
        char digest[MD5_DIGEST_LENGTH];
        dpl_dict_t *metadata;
};

extern GHashTable *hash;

struct pentry *pentry_new(void);

void pentry_free(struct pentry *);

void pentry_set_fd(struct pentry *, int);

void pentry_set_metadata(struct pentry *, dpl_dict_t *);

void pentry_set_digest(struct pentry *, const char *);

void pentry_ctor(struct pentry *, int, dpl_dict_t *);

gboolean pentry_cmp_callback(gpointer, gpointer, gpointer);


#endif
