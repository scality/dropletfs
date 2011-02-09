#ifndef LIST_H
#define LIST_H

#include <sys/time.h>
#include <time.h>

struct list
{
    struct timeval tv;
    void *data;
    struct list *next;
};

typedef int compare_cb_t(void *, void *);
typedef void cellprint_cb_t(void *);
typedef int criteria_cb_t(void *, void *);
typedef void mapping_cb_t(void *, void *);

struct list *list_ctor(void);
void list_free(struct list *);

/* the element must be dynamically allocated by the caller */
struct list *list_add(struct list *, void *);
struct list *list_remove(struct list *, void *, compare_cb_t);
struct list *list_remove_all(struct list *, void *, compare_cb_t);
void *list_search(struct list *, void *, compare_cb_t);
void list_print(struct list *, cellprint_cb_t);
void list_clean(struct list **);
struct list *list_rfilter(struct list *, criteria_cb_t, void *);
struct list *list_filter(struct list *, criteria_cb_t, void *);
int list_length(struct list *);
int list_nb_cells(struct list *);
void list_map(struct list  *, mapping_cb_t, void *);

#endif /* LIST_H */
