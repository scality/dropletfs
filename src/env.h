#ifndef ENV_H
#define ENV_H

#include "conf.h"

void env_ctor(struct conf *, char *);
void env_dtor(struct conf *);
void env_free(struct conf *);
void env_override_conf(struct conf *);

#endif /* ENV_H */
