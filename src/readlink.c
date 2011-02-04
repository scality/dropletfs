#include <errno.h>
#include <droplet.h>
#include <string.h>

#include "readlink.h"
#include "log.h"
#include "timeout.h"

extern dpl_ctx_t *ctx;

int
dfs_readlink(const char *path,
             char *buf,
             size_t bufsiz)
{
        dpl_dict_t *dict = NULL;
        dpl_status_t rc;
        int ret;
        char *dest = NULL;
        size_t dest_size = 0;

        rc = dfs_getattr_timeout(ctx, path, &dict);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_getattr_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (! dict) {
                LOG(LOG_ERR, "dpl_getattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        dest = dpl_dict_get_value(dict, "symlink");
        if (! dest) {
                LOG(LOG_ERR, "empty link path");
                ret = -1;
                goto err;
        }

        dest_size = strlen(dest);
        if (dest_size > bufsiz) {
                LOG(LOG_NOTICE, "link length too big: '%s'", dest);
                dest_size = bufsiz;
        }

        if (! strncpy(buf, dest, dest_size)) {
                LOG(LOG_ERR, "path=%s: strcpy: %s", path, strerror(errno));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        if (dict)
                dpl_dict_free(dict);

        LOG(LOG_DEBUG, "%s", path);
        return 0;
}
