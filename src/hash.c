#include <assert.h>
#include <pthread.h>
#include <errno.h>

#include "log.h"
#include "hash.h"
#include "metadata.h"


/* path entry on remote storage file system */
struct pentry {
        int fd;
        struct stat st;
        char digest[MD5_DIGEST_LENGTH];
        dpl_dict_t *metadata;
        pthread_mutex_t mutex;
        int flag;
};


pentry_t *
pentry_new(void)
{
        pentry_t *pe = NULL;
        pthread_mutexattr_t attr;
        int rc;

        pe = malloc(sizeof *pe);
        if (! pe) {
                LOG("out of memory");
                return NULL;
        }

        pe->metadata = dpl_dict_new(13);

        rc = pthread_mutexattr_init(&attr);
        if (rc)
                LOG("pthread_mutexattr_init mutex@%p: %s",
                    (void *)&pe->mutex, strerror(rc));

        rc = pthread_mutex_init(&pe->mutex, &attr);
        if (rc)
                LOG("pthread_mutex_init mutex@%p %s",
                    (void *)&pe->mutex, strerror(rc));

        pe->flag = FLAG_DIRTY;
        return pe;
}

void
pentry_free(pentry_t *pe)
{
        if (-1 != pe->fd)
                close(pe->fd);

        dpl_dict_free(pe->metadata);
        (void)pthread_mutex_destroy(&pe->mutex);

        free(pe);
}

int
pentry_lock(pentry_t *pe)
{
        int ret;

        ret = pthread_mutex_lock(&pe->mutex);
        if (ret)
                LOG("pthread_mutex_lock mutex@%p: %s",
                    (void *)&pe->mutex, strerror(errno));

        return ret;
}

int
pentry_unlock(pentry_t *pe)
{
        int ret;

        ret = pthread_mutex_unlock(&pe->mutex);
        if (ret)
                LOG("pthread_mutex_unlock mutex@%p: %s",
                    (void *)&pe->mutex, strerror(errno));

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
                    dpl_dict_t *meta)
{
        assert(pe);

        if (DPL_FAILURE == dpl_dict_copy(pe->metadata, meta))
                return -1;

        return 0;
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

        memcpy(pe->digest, digest, MD5_DIGEST_LENGTH);
        return 0;
}

char *
pentry_get_digest(pentry_t *pe)
{
        assert(pe);

        return pe->digest;
}
