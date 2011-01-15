#include <droplet.h>

#include "mknod.h"
#include "glob.h"
#include "log.h"

extern int max_retry;

int
dfs_mknod(const char *path,
          mode_t mode,
          dev_t dev)
{
        dpl_status_t rc = DPL_FAILURE;
        int tries = 0;
        int delay = 1;

        LOG("%s, mode=0x%X", path, (unsigned)mode);

        rc = dpl_mknod(ctx, (char *)path);

 retry:
        if (DPL_SUCCESS != rc) {
                if (tries < max_retry) {
                        LOG("mknod: timeout? (%s)", dpl_status_str(rc));
                        tries++;
                        sleep(delay);
                        delay *= 2;
                        goto retry;
                }
                LOG("dpl_mknod: %s", dpl_status_str(rc));
                return rc;
        }

        return 0;
}
