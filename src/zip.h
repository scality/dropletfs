#ifndef ZIP_H
#define ZIP_H

#include <zlib.h>

int zip(FILE *source, FILE *dest, int level);

int unzip(FILE *source, FILE *dest);

char *zerr_to_str(int ret);

#endif /* ZIP_H */
