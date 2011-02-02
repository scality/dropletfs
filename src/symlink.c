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
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;
        dpl_status_t rc = DPL_FAILURE;
        int tries = 0;
        int delay = 1;
        dpl_dict_t *dict = NULL;
        int fd = -1;
        size_t size;
        char pattern[] = "/tmp/dplfs-symlink.XXXXXX";
        char *opath = NULL;
        char *size_str = NULL;

        LOG(LOG_DEBUG, "%s -> %s, rootdir = %s", oldpath, newpath, env->root_dir);

        opath = strstr(oldpath, env->root_dir);
        if (opath)
                opath += strlen(env->root_dir);
        else
                opath = (char *)oldpath;

        fd = mkstemp(pattern);
        if (-1 == fd) {
                LOG(LOG_ERR, "mkstemp: %s", strerror(errno));
                ret = -1;
                goto err;
        }

        size = strlen(opath);
        if (-1 == write(fd, opath, size)) {
                LOG(LOG_ERR, "write: %s", strerror(errno));
                ret = -1;
                goto err;
        }

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

        size_str = tmpstr_printf("%zu", size);

        rc = dpl_dict_add(dict, "size", size_str, 0);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_dict_add: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        newpath++;
  retry:
        vfile = NULL;
        rc = dpl_openwrite(ctx,
                           (char *)newpath,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           size,
                           &vfile);

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

        if (-1 == lseek(fd, 0, SEEK_SET)) {
                LOG(LOG_ERR, "lseek: %s", strerror(errno));
                ret = -1;
                goto err;
        }

        if (-1 == read_write_all_vfile(fd, vfile)) {
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        if (vfile) {
                rc = dpl_close(vfile);
                if (DPL_SUCCESS != rc) {
                        LOG(LOG_ERR, "dpl_close: %s", dpl_status_str(rc));
                        ret = -1;
                }
        }

        if (dict)
                dpl_dict_free(dict);

        if (fd > 0)
                close(fd);

        return ret;
}
