#include <unistd.h>
#include <droplet.h>

#include "symlink.h"
#include "log.h"
#include "tmpstr.h"

extern dpl_ctx_t *ctx;

int
dfs_symlink(const char *oldpath,
            const char *newpath)
{
        int ret;
        dpl_status_t rc = DPL_FAILURE;
        int tries = 0;
        int delay = 1;
        dpl_dict_t *dict = NULL;
        char *size = NULL;

        LOG(LOG_DEBUG, "%s -> %s", oldpath, newpath);

  retry:
        rc = dpl_mknod(ctx, (char *)newpath);

        if (DPL_SUCCESS != rc) {
                if (rc != DPL_ENOENT && (tries < env->max_retry)) {
                        LOG(LOG_NOTICE,
                            "dpl_openwrite timeout? (delay=%d)", delay);
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        goto retry;
                }
                LOG(LOG_ERR, "dpl_openwrite: %s (%d)", dpl_status_str(rc), rc);
                ret = -1;
                goto err;
        }

        dict = dpl_dict_new(13);
        if (! dict) {
                LOG(LOG_ERR, "dpl_dict_new can't allocate memory");
                ret = -1;
                goto err;
        }

        size = tmpstr_printf("%zu", strlen(oldpath));

        rc = dpl_dict_add(dict, "symlink", (char *)oldpath, 0);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_dict_add: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        rc = dpl_dict_update_value(dict, "size", size);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_dict_update_value: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        rc = dpl_setattr(ctx, (char *)newpath, dict);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_setattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        if (dict)
                dpl_dict_free(dict);

        return ret;
}
