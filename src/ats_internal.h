#ifndef __ATS_INTERNAL__
#define __ATS_INTERNAL__

#include <athena.h>
unsigned char *read_nsvg(const char *filename, int *x, int *y);
unsigned char *read_stbi(const char *filename, int *x, int *y);
unsigned char *read_tiff(const char *filename, int *x, int *y);

#endif /* __ATS_INTERNAL__ */
