#ifndef DROPLET_METADATA_H
#define DROPLET_METADATA_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <droplet.h>

void print_metadata(dpl_dict_t *dict);

void assign_meta_to_dict(dpl_dict_t *dict, char *meta, void *v);

void fill_metadata_from_stat(dpl_dict_t *dict, struct stat *st);

void fill_default_metadata(dpl_dict_t *dict);

void fill_stat_from_metadata(struct stat *st, dpl_dict_t *dict);


#endif
