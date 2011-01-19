#include <droplet.h>

#include "glob.h"
#include "log.h"
#include "setxattr.h"

int
dfs_setxattr(const char *path,
             const char *name,
             const char *value,
             size_t size,
             int flag)
{
        LOG(LOG_DEBUG, "path=%s, name=%s, value=%s, size=%zu, flag=%d",
            path, name, value, size, flag);

        return 0;
}
