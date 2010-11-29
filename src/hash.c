#include "hash.h"

struct pentry *
pentry_new(void)
{
        struct pentry *pe = malloc(sizeof *pe);
        if (! pe) {
                LOG("out of memory");
                exit(EXIT_FAILURE);
        }

        return pe;
}

void
pentry_ctor(struct pentry *pe,
            int fd)
{
        pe->fd = fd;
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

int
get_fd_from_path(char *path)
{
        struct pentry *pe = g_hash_table_find(hash,
                                              pentry_cmp_callback,
                                              (char *)path);

        if (! pe)
                return -1;

        return pe->fd;
}
