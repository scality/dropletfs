#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "log.h"
#include "zip.h"

/* code shamelessly stolen from http://www.zlib.net/zlib_how.html */

#define CHUNK 16384

int
zip(char *srcname,
    char *dstname,
    int level)
{
        int ret, flush;
        unsigned have;
        z_stream strm;
        unsigned char in[CHUNK];
        unsigned char out[CHUNK];
        FILE *src = NULL;
        FILE *dst = NULL;

        src = fopen(srcname, "r");
        if (! src) {
                LOG("fopen: %s", strerror(errno));
                ret = -errno;
                goto err;
        }

        dst = fopen(dstname, "w");
        if (! dst) {
                LOG("fopen: %s", strerror(errno));
                ret = -errno;
                goto err;
        }


        /* allocate deflate state */
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;

        ret = deflateInit(&strm, level);
        if (Z_OK != ret)
                return ret;

        /* compress until end of file */
        do {
                strm.avail_in = fread(in, 1, CHUNK, src);
                if (ferror(src)) {
                        (void)deflateEnd(&strm);
                        return Z_ERRNO;
                }
                flush = feof(src) ? Z_FINISH : Z_NO_FLUSH;
                strm.next_in = in;

                /* run deflate() on input until output buffer not full, finish
                   compression if all of source has been read in */
                do {

                        strm.avail_out = CHUNK;
                        strm.next_out = out;
                        ret = deflate(&strm, flush);
                        assert(ret != Z_STREAM_ERROR);

                        have = CHUNK - strm.avail_out;
                        if (fwrite(out, 1, have, dst) != have || ferror(dst)) {
                                (void)deflateEnd(&strm);
                                return Z_ERRNO;
                        }

                } while (0 == strm.avail_out);
                assert(0 == strm.avail_in); /* all input will be used */

                /* done when last data in file processed */
        } while (Z_FINISH != flush);
        assert(Z_STREAM_END == ret);

        /* clean up and return */
        (void)deflateEnd(&strm);
        ret = Z_OK;

  err:
        if (src)
                fclose(src);

        if (dst)
                fclose(dst);

        return ret;

}

int
unzip(char *srcname,
      char *dstname)
{
        int ret;
        unsigned have;
        z_stream strm;
        unsigned char in[CHUNK];
        unsigned char out[CHUNK];
        /* allocate inflate state */
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;
        FILE *src = NULL;
        FILE *dst = NULL;

        src = fopen(srcname, "r");
        if (! src) {
                LOG("fopen: %s", strerror(errno));
                ret = -errno;
                goto err;
        }

        dst = fopen(dstname, "w");
        if (! dst) {
                LOG("fopen: %s", strerror(errno));
                ret = -errno;
                goto err;
        }

        ret = inflateInit(&strm);
        if (Z_OK != ret)
                return ret;

        /* decompress until deflate stream ends or end of file */
        do {

                strm.avail_in = fread(in, 1, CHUNK, src);
                if (ferror(src)) {
                        (void)inflateEnd(&strm);
                        return Z_ERRNO;
                }
                if (0 == strm.avail_in)
                        break;
                strm.next_in = in;

                /* run inflate() on input until output buffer not full */
                do {

                        strm.avail_out = CHUNK;
                        strm.next_out = out;

                        ret = inflate(&strm, Z_NO_FLUSH);
                        assert(Z_STREAM_ERROR != ret);
                        switch (ret) {
                        case Z_NEED_DICT:
                                ret = Z_DATA_ERROR; /* and fall through */
                        case Z_DATA_ERROR:
                        case Z_MEM_ERROR:
                                (void)inflateEnd(&strm);
                                return ret;
                        }

                        have = CHUNK - strm.avail_out;
                        if (have != fwrite(out, 1, have, dst) || ferror(dst)) {
                                (void)inflateEnd(&strm);
                                return Z_ERRNO;
                        }

                } while (0 == strm.avail_out);

                /* done when inflate() says it's done */
        } while (ret != Z_STREAM_END);

        /* clean up and return */
        (void)inflateEnd(&strm);
        ret = (ret == Z_STREAM_END) ? Z_OK : Z_DATA_ERROR;

  err:
        if (src)
                fclose(src);

        if (dst)
                fclose(dst);

        return ret;
}
