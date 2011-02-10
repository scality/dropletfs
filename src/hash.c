#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <glib.h>

#include "log.h"
#include "hash.h"
#include "metadata.h"
#include "tmpstr.h"
#include "list.h"

extern GHashTable *hash;
extern struct conf *conf;

/* path entry on remote storage file system */
struct pentry {
        int fd;
        char *path;
        struct stat st;
        char digest[MD5_DIGEST_LENGTH];
        dpl_dict_t *metadata;
        pthread_mutex_t md_mutex;
        pthread_mutex_t mutex;
        sem_t refcount;
        int flag;
        int exclude;
        filetype_t filetype;
        struct list *dirent;
        int ondisk;
        time_t atime;
};


pentry_t *
pentry_new(void)
{
        pentry_t *pe = NULL;
        pthread_mutexattr_t attr;
        pthread_mutexattr_t md_attr;
        int rc;

        pe = malloc(sizeof *pe);
        if (! pe) {
                LOG(LOG_CRIT, "out of memory");
                return NULL;
        }

        rc = sem_init(&pe->refcount, 0, 0);
        if (-1 == rc) {
                LOG(LOG_INFO, "sem_init sem@%p: %s",
                    (void *)&pe->refcount, strerror(errno));
                goto release;
        }

        rc = pthread_mutexattr_init(&attr);
        if (rc) {
                LOG(LOG_INFO, "pthread_mutexattr_init mutex@%p: %s",
                    (void *)&pe->mutex, strerror(rc));
                goto release;
        }

        rc = pthread_mutex_init(&pe->mutex, &attr);
        if (rc) {
                LOG(LOG_INFO, "pthread_mutex_init mutex@%p %s",
                    (void *)&pe->mutex, strerror(rc));
                goto release;
        }

        rc = pthread_mutexattr_init(&md_attr);
        if (rc) {
                LOG(LOG_INFO, "pthread_mutexattr_init mutex@%p: %s",
                    (void *)&pe->md_mutex, strerror(rc));
                goto release;
        }

        rc = pthread_mutex_init(&pe->md_mutex, &attr);
        if (rc) {
                LOG(LOG_INFO, "pthread_mutex_init mutex@%p %s",
                    (void *)&pe->md_mutex, strerror(rc));
                goto release;
        }

        pe->metadata = NULL;
        pe->ondisk = FILE_UNSET;
        pe->fd = -1;
        pe->dirent = NULL;
        pe->path = NULL;
        pe->exclude = 0;
        pe->flag = FLAG_DIRTY;
        return pe;

  release:
        pentry_free(pe);
        return NULL;
}

void
pentry_free(pentry_t *pe)
{
        if (-1 != pe->fd)
                close(pe->fd);

        if (pe->metadata)
                dpl_dict_free(pe->metadata);

        if (pe->path)
                free(pe->path);

        (void)pthread_mutex_destroy(&pe->mutex);
        (void)sem_destroy(&pe->refcount);

        list_free(pe->dirent);
        free(pe);
}

void
pentry_set_atime(pentry_t *pe)
{
        assert(pe);

        pe->atime = time(NULL);
}

time_t
pentry_get_atime(pentry_t *pe)
{
        assert(pe);

        return pe->atime;
}

char *
pentry_placeholder_to_str(int flag)
{
        switch (flag) {
        case FILE_LOCAL: return "local";
        case FILE_REMOTE: return "remote";
        case FILE_UNSET: return "unset";
        }

        assert(! "impossible case");
        return "invalid";
}

void
pentry_set_placeholder(pentry_t *pe,
                       int flag)
{
        assert(pe);

        pe->ondisk = flag;
}

int
pentry_get_placeholder(pentry_t *pe)
{
        assert(pe);

        return pe->ondisk;
}

static int
cb_compare_path(void *p1,
                void *p2)
{
        char *path1 = p1;
        char *path2 = p2;

        assert(p1);
        assert(p2);

        return ! strcmp(path1, path2);
}

int
pentry_remove_dirent(pentry_t *pe,
                     const char *path)
{
        int ret;

        assert(pe);

        if (! path) {
                LOG(LOG_ERR, "NULL path");
                ret = -1;
                goto err;
        }

        if (FILE_DIR != pe->filetype) {
                LOG(LOG_ERR, "%s: is not a directory", path);
                ret = -1;
                goto err;
        }

        pe->dirent = list_remove(pe->dirent, (char *)path, cb_compare_path);

        ret = 0;
  err:
        return ret;
}

void
pentry_add_dirent(pentry_t *pe,
                  const char *path)
{
        char *key = NULL;

        assert(pe);

        LOG(LOG_DEBUG, "path='%s', new entry: '%s'", pentry_get_path(pe), path);

        key = strdup(path);
        if (! key)
                LOG(LOG_ERR, "%s: strdup: %s", path, strerror(errno));
        else
                pe->dirent = list_add(pe->dirent, key);
}

struct list *
pentry_get_dirents(pentry_t *pe)
{
        assert(pe);

        return pe->dirent;
}

void
pentry_unlink_cache_file(pentry_t *pe)
{
        char *local = NULL;

        assert(pe);

        if (! pe->path)
                return;

        local = tmpstr_printf("%s/%s", conf->cache_dir, pe->path);

        if (-1 == unlink(local))
                LOG(LOG_INFO, "unlink(%s): %s", local, strerror(errno));
}

void
pentry_inc_refcount(pentry_t *pe)
{
        assert(pe);


        if (-1 == sem_post(&pe->refcount))
                LOG(LOG_INFO, "path=%s, sem_post@%p: %s",
                    pe->path, (void *)&pe->refcount, strerror(errno));
}

void
pentry_dec_refcount(pentry_t *pe)
{
        assert(pe);


        if (-1 == sem_wait(&pe->refcount))
                LOG(LOG_INFO, "path=%s, sem_wait@%p: %s",
                    pe->path, (void *)&pe->refcount, strerror(errno));
}

int
pentry_get_refcount(pentry_t *pe)
{
        assert(pe);

        int ret;
        (void)sem_getvalue(&pe->refcount, &ret);

        return ret;
}

char *
pentry_get_path(pentry_t *pe)
{
        assert(pe);

        return pe->path;
}

void
pentry_set_path(pentry_t *pe,
                const char *path)
{
        assert(pe);

        if (! path) {
                LOG(LOG_ERR, "empty path");
                return;
        }

        pe->path = strdup(path);
        if (! pe->path)
                LOG(LOG_CRIT, "strdup(%s): %s", path, strerror(errno));
}

void
pentry_set_exclude(pentry_t *pe,
                   int exclude)
{
        assert(pe);

        pe->exclude = exclude;
}

int
pentry_get_exclude(pentry_t *pe)
{
        assert(pe);

        return pe->exclude;
}

static int
pentry_gen_trylock(pentry_t *pe,
                   pthread_mutex_t *lock)
{
        int ret;

        assert(pe);

        LOG(LOG_DEBUG, "path=%s: trylock(fd=%d)...", pe->path, pe->fd);

        ret = pthread_mutex_trylock(lock);

        LOG(LOG_DEBUG, "path=%s fd=%d: %sacquired",
            pe->path, pe->fd, ret ? "not " : "");

        return ret;
}

static int
pentry_gen_lock(pentry_t *pe,
                pthread_mutex_t *lock)
{
        int ret;

        assert(pe);

        LOG(LOG_DEBUG, "path=%s: lock(fd=%d)...", pe->path, pe->fd);

        ret = pthread_mutex_lock(lock);
        if (ret)
                LOG(LOG_ERR, "path=%s, fd=%d: %s",
                    pe->path, pe->fd, strerror(errno));
        else
                LOG(LOG_DEBUG, "path=%s, fd=%d: acquired",
                    pe->path, pe->fd);

        return ret;
}

static int
pentry_gen_unlock(pentry_t *pe,
                  pthread_mutex_t *lock)
{
        int ret;

        assert(pe);

        LOG(LOG_DEBUG, "path=%s, unlock(fd=%d)...", pe->path, pe->fd);

        ret = pthread_mutex_unlock(lock);
        if (ret)
                LOG(LOG_ERR, "path=%s, fd=%d: %s",
                    pe->path, pe->fd, strerror(errno));
        else
                LOG(LOG_DEBUG, "path=%s, fd=%d: released", pe->path, pe->fd);

        return ret;
}

int
pentry_trylock(pentry_t *pe)
{
        assert(pe);

        LOG(LOG_DEBUG, "path=%s: trylock(fd=%d)...", pe->path, pe->fd);

        return pentry_gen_trylock(pe, &pe->mutex);
}

int
pentry_lock(pentry_t *pe)
{
        assert(pe);

        LOG(LOG_DEBUG, "path=%s: lock(fd=%d)...", pe->path, pe->fd);

        return pentry_gen_lock(pe, &pe->mutex);
}

int
pentry_unlock(pentry_t *pe)
{
        assert(pe);

        LOG(LOG_DEBUG, "path=%s, unlock(fd=%d)...", pe->path, pe->fd);

        return pentry_gen_unlock(pe, &pe->mutex);
}

int
pentry_md_trylock(pentry_t *pe)
{
        assert(pe);

        LOG(LOG_DEBUG, "path=%s: trylock(fd=%d)...", pe->path, pe->fd);

        return pentry_gen_trylock(pe, &pe->md_mutex);
}

int
pentry_md_lock(pentry_t *pe)
{
        assert(pe);

        LOG(LOG_DEBUG, "path=%s: lock(fd=%d)...", pe->path, pe->fd);

        return pentry_gen_lock(pe, &pe->md_mutex);
}

int
pentry_md_unlock(pentry_t *pe)
{
        assert(pe);

        LOG(LOG_DEBUG, "path=%s, unlock(fd=%d)...", pe->path, pe->fd);

        return pentry_gen_unlock(pe, &pe->md_mutex);
}


filetype_t
pentry_get_filetype(pentry_t *pe)
{
        assert(pe);

        return pe->filetype;
}

void
pentry_set_filetype(pentry_t *pe,
                    filetype_t type)
{
        assert(pe);

        pe->filetype = type;
}

void
pentry_set_fd(pentry_t *pe,
              int fd)
{
        assert(pe);

        pe->fd = fd;
}

int
pentry_get_fd(pentry_t *pe)
{
        assert(pe);

        return pe->fd;
}

int
pentry_get_flag(pentry_t *pe)
{
        assert(pe);

        return pe->flag;
}

void
pentry_set_flag(pentry_t *pe, int flag)
{
        assert(pe);

        pe->flag = flag;
}

int
pentry_set_metadata(pentry_t *pe,
                    dpl_dict_t *dict)
{
        int ret;

        assert(pe);

        if (pe->metadata)
                dpl_dict_free(pe->metadata);

        pe->metadata = dpl_dict_new(13);
        if (! pe->metadata) {
                LOG(LOG_ERR, "dpl_dict_new: can't allocate memory");
                ret = -1;
                goto err;
        }

        if (DPL_FAILURE == dpl_dict_copy(pe->metadata, dict)) {
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        return ret;
}

dpl_dict_t *
pentry_get_metadata(pentry_t *pe)
{
        assert(pe);

        return pe->metadata;
}

int
pentry_set_digest(pentry_t *pe,
                  const char *digest)
{
        assert(pe);

        if (! digest)
                return -1;

        memcpy(pe->digest, digest, sizeof pe->digest);
        return 0;
}

char *
pentry_get_digest(pentry_t *pe)
{
        assert(pe);

        return pe->digest;
}

char *
pentry_type_to_str(filetype_t type)
{
        switch (type) {
        case FILE_REG: return "regular file";
        case FILE_DIR: return "directory";
        case FILE_SYMLINK: return "symlink";
        default: return "unknown";
        }
}

static void
print(void *key, void *data, void *user_data)
{
        char *path = key;
        pentry_t *pe = data;
        LOG(LOG_DEBUG, "key=%s, path=%s, fd=%d, type=%s, digest=%.*s",
            path, pe->path, pe->fd, pentry_type_to_str(pe->filetype),
            MD5_DIGEST_LENGTH, pe->digest);
}

void
hash_print_all(void)
{
        g_hash_table_foreach(hash, print, NULL);
}
