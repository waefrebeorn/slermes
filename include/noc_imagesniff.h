#ifndef NOC_IMAGESNIFF_H
#define NOC_IMAGESNIFF_H
#include <stddef.h>
/* True if data starts with a known image magic-byte sequence. */
int imagesniff_looks_like_image(const unsigned char *data, size_t len);
#endif
