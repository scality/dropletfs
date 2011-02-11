#include <time.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "cachedir.h"
#include "tmpstr.h"
#include "log.h"
#include "hash.h"
#include "getattr.h"
#include "timeout.h"
#include "file.h"
#include "list.h"

#define MAX_CHILDREN 10

extern struct conf *conf;
extern dpl_ctx_t *ctx;
extern GHashTable *hash;

static void
cb_map_dirents(void *elem, void *cb_arg)
{
        char *path = NULL;
        dpl_dict_t *metadata = NULL;
        dpl_status_t rc;
        dpl_ftype_t type;
        dpl_ino_t ino, parent_ino, obj_ino;
        pentry_t *pe_dirent = NULL;
        pentry_t *pe = NULL;

        path = elem;
        pe = cb_arg;

        LOG(LOG_DEBUG, "path='%s', dirent='%s'", path, pentry_get_path(pe));

        pe_dirent = g_hash_table_lookup(hash, path);
        if (! pe_dirent) {
                LOG(LOG_ERR, "'%s' is not an entry anymore in '%s'",
                    path, pentry_get_path(pe));
                goto end;
        }

        rc = dfs_namei_timeout(ctx, path, ctx->cur_bucket,
                               ino, &parent_ino, &obj_ino, &type);

        LOG(LOG_DEBUG, "path=%s, dpl_namei: %s, type=%s, parent_ino=%s, obj_ino=%s",
            path, dpl_status_str(rc), ftype_to_str(type),
            parent_ino.key, obj_ino.key);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_NOTICE, "dfs_namei_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        rc = dfs_getattr_timeout(ctx, path, &metadata);
        if (DPL_SUCCESS != rc && DPL_EISDIR != rc) {
                LOG(LOG_ERR, "dfs_getattr_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        if (pentry_md_trylock(pe_dirent))
                goto end;

        if (metadata)
                pentry_set_metadata(pe_dirent, metadata);

        pentry_set_atime(pe_dirent);

        (void)pentry_md_unlock(pe_dirent);
  end:
        if (metadata)
                dpl_dict_free(metadata);

}

static void *
update_metadata(void *cb_arg)
{
        pentry_t *pe = NULL;
        char *path = NULL;
        dpl_ftype_t type;
        dpl_ino_t ino, parent_ino, obj_ino;
        dpl_status_t rc;
        dpl_dict_t *metadata = NULL;
        struct list *dirent = NULL;

        pe = cb_arg;
        path = pentry_get_path(pe);

        LOG(LOG_DEBUG, "path=%s", path);

        ino = dpl_cwd(ctx, ctx->cur_bucket);

        rc = dfs_namei_timeout(ctx, path, ctx->cur_bucket,
                               ino, &parent_ino, &obj_ino, &type);

        LOG(LOG_DEBUG, "path=%s, dpl_namei: %s, type=%s, parent_ino=%s, obj_ino=%s",
            path, dpl_status_str(rc), ftype_to_str(type),
            parent_ino.key, obj_ino.key);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_NOTICE, "dfs_namei_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        rc = dfs_getattr_timeout(ctx, path, &metadata);
        if (DPL_SUCCESS != rc && DPL_EISDIR != rc) {
                LOG(LOG_ERR, "dfs_getattr_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        /* If this is a directory, update its entries' metadata */
        if (DPL_FTYPE_DIR == type) {
                dirent = pentry_get_dirents(pe);
                if (dirent)
                        list_map(dirent, cb_map_dirents, pe);
        }

        if (pentry_md_trylock(pe))
                goto end;

        if (metadata)
                pentry_set_metadata(pe, metadata);

        pentry_set_atime(pe);

        (void)pentry_md_unlock(pe);
  end:
        if (metadata)
                dpl_dict_free(metadata);

        pthread_exit(NULL);
        return NULL;
}

static void
cachedir_callback(gpointer key,
                  gpointer value,
                  gpointer user_data)
{
        static int children;
        GHashTable *hash = NULL;
        char *path = NULL;
        pentry_t *pe = NULL;
        time_t age;
        pthread_t update_md;
        pthread_attr_t update_md_attr;
        int total_size;

        LOG(LOG_DEBUG, "Entering function");

        path = key;
        pe = value;
        hash = user_data;

        age = time(NULL) - pentry_get_atime(pe);

        LOG(LOG_DEBUG, "%s, age=%d sec, children=%d", path, (int)age, children);

        /* 64 is a bold estimation of a path length */
        total_size = g_hash_table_size(hash) * (pentry_sizeof() + 64);
        if (total_size > conf->hashtable_max_size)
                return;

        if (age < conf->sc_age_threshold)
                return;

        if (children > MAX_CHILDREN)
                return;

        children++;

        pthread_attr_init(&update_md_attr);
        pthread_attr_setdetachstate(&update_md_attr, PTHREAD_CREATE_JOINABLE);
        pthread_create(&update_md, &update_md_attr, update_metadata, pe);

        children--;

        return;
}

static void
root_dir_preload(GHashTable *hash)
{
        char *root_dir = "/";
        void *dir_hdl = NULL;
        dpl_dirent_t dirent;
        dpl_status_t rc = DPL_FAILURE;
        pentry_t *pe = NULL;
        char *direntname = NULL;
        char *key = NULL;
        struct stat st;

        pe = g_hash_table_lookup(hash, root_dir);
        if (! pe) {
                pe = pentry_new();
                if (! pe) {
                        LOG(LOG_ERR, "%s: can't add a new cell", root_dir);
                        goto err;
                }
                pentry_set_path(pe, root_dir);
                key = strdup(root_dir);
                if (! key) {
                        LOG(LOG_ERR, "%s: strdup: %s",
                            root_dir, strerror(errno));
                        pentry_free(pe);
                        goto err;
                }
                g_hash_table_insert(hash, key, pe);
        }

        rc = dfs_chdir_timeout(ctx, root_dir);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_chdir_timeout: %s", dpl_status_str(rc));
                goto err;
        }

        rc = dfs_opendir_timeout(ctx, ".", &dir_hdl);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_opendir_timeout: %s", dpl_status_str(rc));
                goto err;
        }

        while (DPL_SUCCESS == dpl_readdir(dir_hdl, &dirent)) {
                direntname = tmpstr_printf("%s%s", root_dir, dirent.name);
                memset(&st, 0, sizeof st);
                (void)dfs_getattr(direntname, &st);
        }

        pentry_set_atime(pe);
  err:
        if (dir_hdl)
                dpl_closedir(dir_hdl);
}

void *
thread_cachedir(void *cb_arg)
{
        GHashTable *hash = cb_arg;

        LOG(LOG_DEBUG, "entering thread");

        if (! conf->sc_loop_delay && ! conf->sc_age_threshold)
                return NULL;

        root_dir_preload(hash);

        while (1) {
                LOG(LOG_DEBUG, "updating cache directories");
                g_hash_table_foreach(hash, cachedir_callback, hash);
                sleep(conf->sc_loop_delay);
        }

        return NULL;
}
