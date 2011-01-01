#include "log.h"
#include "hash.h"
#include "metadata.h"

struct pentry *
pentry_new(void)
{
        struct pentry *pe = malloc(sizeof *pe);
        if (! pe) {
                LOG("out of memory");
                exit(EXIT_FAILURE);
        }

        pe->metadata = dpl_dict_new(13);
        return pe;
}

void
pentry_free(struct pentry *pe)
{
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
        copy_metadata(pe->metadata, meta);
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
        char *value_path = (char *)value;
        char *data_path = (char *)data;

        return strcmp(value_path, data_path);
}
