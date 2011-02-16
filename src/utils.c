#include <inttypes.h>

#include "utils.h"


int
time_diff(struct timeval *start,
          struct timeval *end)
{
        uint64_t start64;
        uint64_t end64;
        int diff;

        start64 = start->tv_sec * 1000000 + start->tv_usec;
        end64 = end->tv_sec * 1000000 + end->tv_usec;

        diff = (int)((end64 - start64)/1000);

        return diff;
}
