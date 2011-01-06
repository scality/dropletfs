#ifndef ZIP_H
#define ZIP_H

#include <zlib.h>

int zip(char *src, char *dst, int level);
int unzip(char *src, char *dst);

#endif /* ZIP_H */
