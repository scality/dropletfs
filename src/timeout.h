#ifndef TIMEOUT_H
#define TIMEOUT_H

#include <droplet.h>

dpl_status_t dfs_namei_timeout(dpl_ctx_t *, const char *, char *, dpl_ino_t, dpl_ino_t *,  dpl_ino_t *, dpl_ftype_t *);
dpl_status_t dfs_getattr_timeout(dpl_ctx_t *, const char *, dpl_dict_t **);
dpl_status_t dfs_setattr_timeout(dpl_ctx_t *, const char *, dpl_dict_t *);
dpl_status_t dfs_head_all_timeout(dpl_ctx_t *, char *, char *, char *, dpl_condition_t *, dpl_dict_t **);
dpl_status_t dfs_mkdir_timeout(dpl_ctx_t *, const char *);
dpl_status_t dfs_rmdir_timeout(dpl_ctx_t *, const char *);
dpl_status_t dfs_chdir_timeout(dpl_ctx_t *, const char *);
dpl_status_t dfs_opendir_timeout(dpl_ctx_t *, const char *, void **);
dpl_status_t dfs_unlink_timeout(dpl_ctx_t *, const char *);
dpl_status_t dfs_fcopy_timeout(dpl_ctx_t *, const char *, const char *);
dpl_status_t dfs_mknod_timeout(dpl_ctx_t *, const char *);

#endif /* TIMEOUT_H */
