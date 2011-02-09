#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <assert.h>

#include "list.h"
#include "log.h"

struct list *
list_ctor(void)
{
        return NULL;
}

struct list *
list_add(struct list *ll,
         void *data)
{
        struct list *n = malloc(sizeof *n);
        if (! n) {
                LOG(LOG_ERR, "could not allocate memory");
                return ll;
        }

        n->data = data;
        n->next = ll;

        return n;
}

struct list *
list_remove(struct list *ll,
            void *data,
            compare_cb_t* fn)
{
        if (! data)
                return ll;

        if (! ll)
                return NULL;

        struct list *orig = ll;
        struct list *prev = ll;

        while (ll) {
                if (fn(data, ll->data)) {
                        if (ll == orig) {
                                ll = orig->next;
                                free(orig->data);
                                free(orig);
                                return ll;
                        } else {
                                prev->next = ll->next;
                                free(ll->data);
                                free(ll);
                                return orig;
                        }
                }
                prev = ll;
                ll = ll->next;
        }

        return orig;
}

struct list *
list_remove_all(struct list *ll,
                void *data,
                compare_cb_t* fn)
{
        if(! data)
                return ll;

        struct list *orig = ll;
        struct list *prev = ll;

        if (! ll)
                return NULL;

        while (ll) {
                if (fn(data, ll->data)) {
                        if(orig == ll) {
                                ll = orig->next;
                                free(orig->data);
                                free(orig);
                                prev = orig = ll;
                        } else {
                                struct list *tmp = ll;
                                ll = ll->next;
                                free(tmp->data);
                                free(tmp);
                                prev->next = ll;
                        }
                } else {
                        prev = ll;
                        ll = ll->next;
                }
        }

        if (ll && ! ll->data && ! ll->next) {
                free(ll);
                ll = NULL;
                return orig;
        }

        return orig;
}

void *
list_search(struct list *ll,
            void *data,
            compare_cb_t* fn)
{
        while (ll) {
                if (fn(data, ll->data))
                        return ll->data;
                ll = ll->next;
        }

        return NULL;
}

void
list_print(struct list *ll,
           cellprint_cb_t* print)
{
        while (ll && ll->data) {
                print(ll->data);
                ll = ll->next;
        }
}



#define __filter(ll, cb, ctx, pred) do {                                \
                struct list *orig = ll;                                 \
                struct list *prev = ll;                                 \
                                                                        \
                if (! ll)                                               \
                        return NULL;                                    \
                                                                        \
                while (ll) {                                            \
                        if ((! ll->data) || pred fn(ll->data, ctx)) {   \
                                struct list *tmp = ll;                  \
                                if (tmp == orig)                        \
                                        orig = orig->next;              \
                                prev->next = ll->next;                  \
                                ll = prev->next;                        \
                                free(tmp->data);                        \
                                free(tmp);                              \
                                tmp = NULL;                             \
                        } else {                                        \
                                prev = ll;                              \
                                ll = ll->next;                          \
                        }                                               \
                }                                                       \
                                                                        \
                return orig;                                            \
        } while(/*CONSTCOND*/0)

struct list *
list_filter(struct list *ll,
            criteria_cb_t* fn,
            void *ctx)
{
        __filter(ll, fn, ctx,);
}

struct list *
list_rfilter(struct list *ll,
             criteria_cb_t *fn,
             void *ctx)
{
        __filter(ll, fn, ctx, !);
}

void
list_free(struct list *ll)
{
        if (! ll)
                return;

        while (ll) {
                struct list *tmp = ll;
                if (tmp->data) {
                        free(tmp->data);
                        tmp->data = NULL;
                }
                ll = ll->next;
                free(tmp);
        }
}

int
list_length(struct list *ll)
{
        int ret = 0;
        while (ll) {
                ret++;
                ll = ll->next;
        }
        return ret;
}

int
list_nb_cells(struct list *ll)
{
        int ret = 0;
        while (ll) {
                if (ll->data)
                        ret++;
                ll = ll->next;
        }
        return ret;
}

void
list_map(struct list*ll,
         mapping_cb_t *fn,
         void *ctx)
{
        while (ll) {
                if (ll->data)
                        fn(ll->data, ctx);
                ll = ll->next;
        }
}
