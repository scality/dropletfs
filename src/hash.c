#include "log.h"
#include "hash.h"
#include "metadata.h"

struct pentry *
pentry_new(void)
{
        struct pentry *pe = NULL;

        pe = malloc(sizeof *pe);
        if (! pe) {
                LOG("out of memory");
                exit(EXIT_FAILURE);
        }

        memset(pe->digest, 0, MD5_DIGEST_LENGTH);
        pe->metadata = dpl_dict_new(13);
        return pe;
}

void
pentry_free(struct pentry *pe)
{
        if (pe->digest)
                free(pe->digest);

        if (-1 != pe->fd)
                close(pe->fd);

        dpl_dict_free(pe->metadata);

        free(pe);
}

void
pentry_set_fd(struct pentry *pe,
              int fd)
{
        pe->fd = fd;
}

void
pentry_set_metadata(struct pentry *pe,
                    dpl_dict_t *meta)
{
        dpl_dict_copy(pe->metadata, meta);
}

void
pentry_set_digest(struct pentry *pe,
                  const char *digest)
{
        memcpy(pe->digest, digest, MD5_DIGEST_LENGTH);
}

void
pentry_ctor(struct pentry *pe,
            int fd,
            dpl_dict_t *meta)
{
        pentry_set_fd(pe, fd);
        pentry_set_metadata(pe, meta);
}

gboolean
pentry_cmp_callback(gpointer key,
                    gpointer value,
                    gpointer data)
{
        return strcmp((char *)value, (char *)data);
}
