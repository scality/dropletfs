#include <errno.h>
#include <unistd.h>
#include <droplet.h>

#include "symlink.h"
#include "log.h"
#include "tmpstr.h"
#include "file.h"
#include "timeout.h"

extern dpl_ctx_t *ctx;

int
dfs_symlink(const char *oldpath,
            const char *newpath)
{
        int ret;
        dpl_status_t rc = DPL_FAILURE;
        dpl_dict_t *dict = NULL;
        size_t size = 0;
        char *opath = NULL;
        char *size_str = NULL;

        LOG(LOG_DEBUG, "%s -> %s, rootdir = %s", oldpath, newpath, conf->root_dir);

        rc = dfs_mknod_timeout(ctx, newpath);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_mknod_timeout: %s", dpl_status_str(rc));
                ret = rc;
                goto err;
        }

        opath = strstr(oldpath, conf->root_dir);
        if (opath)
                opath += strlen(conf->root_dir) + 1;
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

        rc = dfs_setattr_timeout(ctx, newpath, dict);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_setattr_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        if (dict)
                dpl_dict_free(dict);

        return ret;
}
