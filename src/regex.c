#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "regex.h"
#include "tmpstr.h"
#include "log.h"

int
re_ctor(struct re *re,
        const char *pattern,
        int flags)
{
        int ret;
        int rc;

        re->set = 0;
        if (! pattern) {
                ret = -1;
                goto err;
        }

        rc = regcomp(&re->rx, pattern, flags);

        if (rc) {
                char *buf = tmpstr_new();
                regerror(rc, &re->rx, buf, TMPSTR_MAX);
                fprintf(stderr, "compile regex '%s' : %s", pattern, buf);
                ret = -1;
                goto err;
        }

        re->str = strdup(pattern);
        if (! re->str) {
                perror("strdup");
                ret = -1;
                goto err;
        }

        re->set = 1;
        ret = 0;
  err:
        return ret;
}

void re_dtor(struct re *re)
{
        free(re->str);
        regfree(&re->rx);
}

int
re_match(struct re *re,
         const char *pattern)
{
        int rc;

        if (! re->set)
                return 0;

        rc = regexec(&re->rx, pattern, 0, NULL, 0);

        if (rc) {
                char *buf = tmpstr_new();
                regerror(rc, &re->rx, buf, TMPSTR_MAX);
                LOG(LOG_ERR, "compile regex '%s' : %s", pattern, buf);
        }

        return 0 == rc;
}

