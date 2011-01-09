#include <assert.h>

#include "log.h"
#include "metadata.h"
#include "tmpstr.h"


static void
cb_var_print(dpl_var_t *var,
             void *arg)
{
        (void)arg;
        LOG("var=%s, value=%s", var->key, var->value);
}

void
print_metadata(dpl_dict_t *dict)
{
        dpl_dict_iterate(dict, cb_var_print, NULL);
}


void
assign_meta_to_dict(dpl_dict_t *dict,
                    char *meta,
                    void *v)
{
        unsigned long val = 0;
        unsigned long size = 0;
        dpl_var_t *var = NULL;
        char *buf = NULL;

        val = *(unsigned long*)v;

        if (NULL != (var = dpl_dict_get(dict, meta))) {
                if (!strcmp("size", meta)) {
                        size = strtoul(var->value, NULL, 10);
                        val += size;
                }
                dpl_dict_remove(dict, var);
        }

        buf = tmpstr_printf("%lu", val);
        dpl_dict_add(dict, meta, buf, 0);
}

void
fill_metadata_from_stat(dpl_dict_t *dict,
                        struct stat *st)
{
        assign_meta_to_dict(dict, "mode", &st->st_mode);
        assign_meta_to_dict(dict, "size", &st->st_size);
        assign_meta_to_dict(dict, "uid", &st->st_uid);
        assign_meta_to_dict(dict, "gid", &st->st_gid);
        assign_meta_to_dict(dict, "atime", &st->st_atime);
        assign_meta_to_dict(dict, "mtime", &st->st_mtime);
        assign_meta_to_dict(dict, "ctime", &st->st_ctime);
}

void
fill_default_metadata(dpl_dict_t *dict)
{
        time_t t;

        t = time(NULL);
        assign_meta_to_dict(dict, "mode", (mode_t []){umask(S_IWGRP|S_IWOTH)});
        assign_meta_to_dict(dict, "uid", (uid_t []){getuid()});
        assign_meta_to_dict(dict, "gid", (gid_t []){getgid()});
        assign_meta_to_dict(dict, "atime", &t);
        assign_meta_to_dict(dict, "ctime", &t);
        assign_meta_to_dict(dict, "mtime", &t);

}

static long long
metadatatoll(dpl_dict_t *dict,
             const char *const name)
{
        char *value = NULL;
        long long v = 0;

        value = dpl_dict_get_value(dict, (char *)name);

        if (! value) {
                LOG("can't grab any meta '%s'", name);
                return -1;
        }

        v = strtoull(value, NULL, 10);
        if (0 == strcmp(name, "mode"))
                LOG("meta=%s, value=0x%x", name, (unsigned)v);
        else
                LOG("meta=%s, value=%s", name, value);

        return v;
}

#define STORE_META(st, dict, name, type) do {                           \
                long long v = metadatatoll(dict, #name);                \
                if (-1 != v)                                            \
                        st->st_##name = (type)v;                        \
        } while (0 /*CONSTCOND*/)

void
fill_stat_from_metadata(struct stat *st,
                        dpl_dict_t *dict)
{
        LOG("entering function");
        STORE_META(st, dict, size, size_t);
        STORE_META(st, dict, mode, mode_t);
        STORE_META(st, dict, uid, uid_t);
        STORE_META(st, dict, gid, gid_t);
        STORE_META(st, dict, atime, time_t);
        STORE_META(st, dict, ctime, time_t);
        STORE_META(st, dict, mtime, time_t);
}
