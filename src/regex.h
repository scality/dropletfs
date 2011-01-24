#ifndef REGEX_H
#define REGEX_H

#include <regex.h>

struct re {
        char *str;
        regex_t rx;
        int set;
};

int re_ctor(struct re *, const char *, int);
void re_dtor(struct re *);

int re_match(struct re *, const char *);

#endif /* REGEX_H */
