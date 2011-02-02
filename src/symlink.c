#include <errno.h>
#include <unistd.h>
#include <droplet.h>

#include "symlink.h"
#include "log.h"
#include "tmpstr.h"
#include "file.h"

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
        size_t size = 0;
        char *opath = NULL;
        char *size_str = NULL;

        LOG(LOG_DEBUG, "%s -> %s, rootdir = %s", oldpath, newpath, env->root_dir);

 retry:
        rc = dpl_mknod(ctx, (char *)newpath);

        if (DPL_SUCCESS != rc) {
                if (tries < env->max_retry) {
                        LOG(LOG_NOTICE, "mknod: timeout? (%s)",
                            dpl_status_str(rc));
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        goto retry;
                }
                LOG(LOG_ERR, "dpl_mknod: %s", dpl_status_str(rc));
                ret = rc;
                goto err;
        }

        opath = strstr(oldpath, env->root_dir);
        if (opath)
                opath += strlen(env->root_dir) + 1;
        else
                opath = (char *)oldpath;

        dict = dpl_dict_new(13);
        if (! dict) {
                LOG(LOG_ERR, "dpl_dict_new can't allocate memory");
                ret = -1;
                goto err;
        }

        rc = dpl_dict_add(dict, "symlink", opath, 0);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_dict_add: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        size = strlen(opath);
        size_str = tmpstr_printf("%zu", size);

        rc = dpl_dict_add(dict, "size", size_str, 0);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_dict_add: %s", dpl_status_str(rc));
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
