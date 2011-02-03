#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <glib.h>

#include "log.h"
#include "hash.h"
#include "metadata.h"
#include "tmpstr.h"

extern GHashTable *hash;
extern struct conf *conf;

/* path entry on remote storage file system */
struct pentry {
        int fd;
        char *path;
        struct stat st;
        char digest[MD5_DIGEST_LENGTH];
        dpl_dict_t *metadata;
        pthread_mutex_t mutex;
        sem_t refcount;
        int flag;
        int exclude;
};


pentry_t *
pentry_new(void)
{
        pentry_t *pe = NULL;
        pthread_mutexattr_t attr;
        int rc;

        pe = malloc(sizeof *pe);
        if (! pe) {
                LOG(LOG_CRIT, "out of memory");
                return NULL;
        }

        pe->metadata = dpl_dict_new(13);
        if (! pe->metadata) {
                LOG(LOG_ERR, "dpl_dict_new: can't allocate memory");
                goto release;
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

        free(pe);
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

int
pentry_trylock(pentry_t *pe)
{
        int ret;

        assert(pe);

        LOG(LOG_DEBUG, "path=%s: trylock(fd=%d)...", pe->path, pe->fd);

        ret = pthread_mutex_trylock(&pe->mutex);

        LOG(LOG_DEBUG, "path=%s fd=%d: %sacquired",
            pe->path, pe->fd, ret ? "not " : "");

        return ret;
}

int
pentry_lock(pentry_t *pe)
{
        int ret;

        assert(pe);

        LOG(LOG_DEBUG, "path=%s: lock(fd=%d)...", pe->path, pe->fd);

        ret = pthread_mutex_lock(&pe->mutex);
        if (ret)
                LOG(LOG_ERR, "path=%s, fd=%d: %s",
                    pe->path, pe->fd, strerror(errno));
        else
                LOG(LOG_DEBUG, "path=%s, fd=%d: acquired",
                    pe->path, pe->fd);


        return ret;
}

int
pentry_unlock(pentry_t *pe)
{
        int ret;

        assert(pe);

        LOG(LOG_DEBUG, "path=%s, unlock(fd=%d)...", pe->path, pe->fd);

        ret = pthread_mutex_unlock(&pe->mutex);
        if (ret)
                LOG(LOG_ERR, "path=%s, fd=%d: %s",
                    pe->path, pe->fd, strerror(errno));
        else
                LOG(LOG_DEBUG, "path=%s, fd=%d: released", pe->path, pe->fd);

        return ret;
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


static void
print(void *key, void *data, void *user_data)
{
        char *path = key;
        pentry_t *pe = data;
        LOG(LOG_DEBUG, "key=%s, path=%s, fd=%d, digest=%.*s",
            path, pe->path, pe->fd, MD5_DIGEST_LENGTH, pe->digest);
}

void
hash_print_all(void)
{
        g_hash_table_foreach(hash, print, NULL);
}
