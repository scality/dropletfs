#include <unistd.h>
#include <droplet.h>

#include "symlink.h"
#include "log.h"

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
        dpl_vfile_t *vfile = NULL;
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;

        LOG(LOG_DEBUG, "%s -> %s", oldpath, newpath);

        dict = dpl_dict_new(13);
        if (! dict) {
                ret = -1;
                goto err;
        }

        dpl_dict_add(dict, "symlink", (char *)oldpath, 0);

  retry:
        vfile = NULL;
        rc = dpl_openwrite(ctx,
                           (char *)newpath,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           0,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                if (rc != DPL_ENOENT && (tries < env->max_retry)) {
                        LOG(LOG_NOTICE,
                            "dpl_openwrite timeout? (delay=%d, vfile@%p)",
                            delay, (void *)vfile);
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        goto retry;
                }
                LOG(LOG_ERR, "dpl_openwrite: %s (%d)", dpl_status_str(rc), rc);
                ret = -1;
                goto err;
        }


        ret = 0;
  err:
        if (dict)
                dpl_dict_free(dict);

        return ret;
}
