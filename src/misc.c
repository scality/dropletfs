#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "misc.h"
#include "tmpstr.h"

#define MKDIR(path, mode) do {                                          \
                if (-1 == mkdir(path, mode) && EEXIST != errno)         \
                        LOG(LOG_ERR, "mkdir(%s): %s",                   \
                            path, strerror(errno));                     \
        } while (0)

void
mkdir_tree(const char *dir) {
        char *tmp = NULL;
        char *p = NULL;

        LOG(LOG_DEBUG, "dir='%s'", dir);

        if (! dir)
                return;

        tmp = tmpstr_printf("%s", dir);

        for (p = tmp + 1; *p; p++) {
                if ('/' == *p) {
                        *p = 0;
                        MKDIR(tmp, 0755);
                        *p = '/';
                }
        }

        MKDIR(tmp, 0755);
}
