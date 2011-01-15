#include <droplet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h>

#include "unlink.h"
#include "hash.h"
#include "glob.h"
#include "log.h"
#include "tmpstr.h"

extern GHashTable *hash;
extern char *cache_dir;

int
dfs_unlink(const char *path)
{
        dpl_status_t rc = DPL_FAILURE;
        char *local = NULL;
        pentry_t *pe = NULL;

        LOG("path=%s", path);

        rc = dpl_unlink(ctx, (char *)path);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_unlink: %s", dpl_status_str(rc));
                return rc;
        }

        local = tmpstr_printf("%s/%s", cache_dir, path);
        if (-1 == unlink(local))
                LOG("unlink cache file (%s): %s", local, strerror(errno));

        pe = g_hash_table_lookup(hash, path);
        if (! pe)
                LOG("path entry not found");
        else
                g_hash_table_remove(hash, path);

        return 0;
}
