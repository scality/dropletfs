#ifndef DROPLET_HASH_H
#define DROPLET_HASH_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <droplet.h>

typedef struct pentry pentry_t;

enum {
        FLAG_CLEAN,
        FLAG_DIRTY,
};

typedef enum {
        FILE_REG,
        FILE_DIR,
        FILE_SYMLINK,
} filetype_t;

enum {
        FILE_LOCAL,
        FILE_REMOTE,
        FILE_UNSET,
};

void hash_print_all(void);

pentry_t *pentry_new(void);
void pentry_free(pentry_t *);

char *pentry_placeholder_to_str(int);
void pentry_set_placeholder(pentry_t *, int);
int pentry_get_placeholder(pentry_t *);

int pentry_remove_dirent(pentry_t *, const char *);
void pentry_add_dirent(pentry_t *, const char *);
struct list;
struct list *pentry_get_dirents(pentry_t *);

void pentry_unlink_cache_file(pentry_t *);

int pentry_lock(pentry_t *);
int pentry_trylock(pentry_t *);
int pentry_unlock(pentry_t *);

void pentry_inc_refcount(pentry_t *);
void pentry_dec_refcount(pentry_t *);

char *pentry_get_path(pentry_t *);
void pentry_set_path(pentry_t *, const char *);

void pentry_set_exclude(pentry_t *, int);
int pentry_get_exclude(pentry_t *);

int pentry_get_flag(pentry_t *);
void pentry_set_flag(pentry_t *, int);

int pentry_get_refcount(pentry_t *);

char *pentry_type_to_str(filetype_t);
filetype_t pentry_get_filetype(pentry_t *);
void pentry_set_filetype(pentry_t *, filetype_t);

void pentry_set_fd(pentry_t *, int);
int pentry_get_fd(pentry_t *);

/* return 0 on success, -1 on failure */
int pentry_set_metadata(pentry_t *, dpl_dict_t *);
dpl_dict_t *pentry_get_metadata(pentry_t *);

/* return 0 on success, -1 on failure */
int pentry_set_digest(pentry_t *, const char *);
char *pentry_get_digest(pentry_t *);

#endif
