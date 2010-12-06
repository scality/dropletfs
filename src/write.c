#include "glob.h"
#include "file.h"

int
dfs_write(const char *path,
          const char *buf,
          size_t size,
          off_t offset,
          struct fuse_file_info *info)
{
        int fd = ((struct pentry *)info->fh)->fd;
        LOG("entering... remote=%s, info->fh=%d", path, fd);
        dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
        dpl_vfile_t *vfile = NULL;
        dpl_status_t rc = DPL_FAILURE;
        if (-1 == fd) {
                LOG("invalid fd (-1)");
                return;
        }

        int ret = -1;
        dpl_dict_t *dict = dpl_dict_new(13);

        struct stat st;
        if (-1 == fstat(fd, &st)) {
                LOG("fstat failed: %s (%d)", strerror(errno), errno);
                ret = -errno;
                goto err;
        }

        fill_metadata_from_stat(dict, &st);
        assign_meta_to_dict(dict, "size", &size);

        struct pentry *pe = NULL;
        pe = g_hash_table_find(hash, pentry_cmp_callback, (char *)path);
        if (! pe) {
                char *file = tmpstr_printf("/tmp/%s/%s", ctx->cur_bucket, path);
                pe = pentry_new();
                pentry_ctor(pe, -1);
                pe->fd = open(file, O_WRONLY);
        }

        if (-1 == pe->fd) {
                ret = -errno;
                goto err;
        }

        if (-1 == pwrite(pe->fd, buf, size, offset)) {
                ret = -errno;
                goto err;
        }

        rc = dpl_openwrite(ctx,
                           (char *)path,
                           DPL_VFILE_FLAG_CREAT|DPL_VFILE_FLAG_MD5,
                           dict,
                           canned_acl,
                           size,
                           &vfile);

        if (DPL_SUCCESS != rc) {
                LOG("dpl_openwrite: %s (%d)", dpl_status_str(rc), rc);
                goto err;
        }

        if (-1 == read_all(pe->fd, (char *)buf, vfile))
                goto err;

        ret = size;
  err:
        if (dict)
                dpl_dict_free(dict);

        if (vfile)
                dpl_close(vfile);

        return ret;
}
