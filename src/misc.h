#ifndef MISC_H
#define MISC_H

#define ASSERT_COMPILE(x) { int a[(x) ? 1 : -1]; }

#define NB_ELEMS(arr) (sizeof arr / sizeof *arr)

#define _STRIZE(arg) #arg
#define STRIZE(x)  _STRIZE(x)


#endif
