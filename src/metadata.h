#ifndef DROPLET_METADATA_H
#define DROPLET_METADATA_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <droplet.h>

void print_metadata(dpl_dict_t *);
void assign_meta_to_dict(dpl_dict_t *, char *, unsigned long);
void fill_metadata_from_stat(dpl_dict_t *, struct stat *);
void fill_default_metadata(dpl_dict_t *);
void fill_stat_from_metadata(struct stat *, dpl_dict_t *);


#endif
