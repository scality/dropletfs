#include <assert.h>

#include "log.h"
#include "hash.h"
#include "metadata.h"


/* path entry on remote storage file system */
struct pentry {
        int fd;
        struct stat st;
        char digest[MD5_DIGEST_LENGTH];
        dpl_dict_t *metadata;
};


pentry_t *
pentry_new(void)
{
        pentry_t *pe = NULL;

        pe = malloc(sizeof *pe);
        if (! pe) {
                LOG("out of memory");
                return NULL;
        }

        pe->metadata = dpl_dict_new(13);
        return pe;
}

void
pentry_free(pentry_t *pe)
{
        if (-1 != pe->fd)
                close(pe->fd);

        dpl_dict_free(pe->metadata);

        free(pe);
}

void
pentry_set_fd(pentry_t *pe,
              int fd)
{
        assert(pe);

        pe->fd = fd;
}

int
pentry_get_fd(pentry_t *pe)
{
        assert(pe);

        return pe->fd;
}

int
pentry_set_metadata(pentry_t *pe,
                    dpl_dict_t *meta)
{
        assert(pe);

        if (DPL_FAILURE == dpl_dict_copy(pe->metadata, meta))
                return -1;

        return 0;
}

dpl_dict_t *
pentry_get_metadata(pentry_t *pe)
{
        assert(pe);

        return pe->metadata;
}

int
pentry_set_digest(pentry_t *pe,
                  const char *digest)
{
        assert(pe);

        if (! digest)
                return -1;

        memcpy(pe->digest, digest, MD5_DIGEST_LENGTH);
        return 0;
}

char *
pentry_get_digest(pentry_t *pe)
{
        assert(pe);

        return pe->digest;
}
