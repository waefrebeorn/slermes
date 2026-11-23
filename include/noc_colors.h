#ifndef NOC_COLORS_H
#define NOC_COLORS_H
#include <stddef.h>
int    colors_should_use_color(int tty); /* tty = sys.stdout.isatty() */
/* Test seam: explicit env values (NULL = absent, "" = present-but-empty).
 * Windows CRT cannot represent an empty env var, so the differential gate
 * injects them; production wrapper reads the real environment. */
int    colors_should_use_color_ex(int tty, const char *no_color, const char *term);
char  *colors_color(int tty, const char *text, char **codes, size_t ncodes); /* malloc'd */
char  *colors_color_ex(int tty, const char *text, char **codes, size_t ncodes,
                       const char *no_color, const char *term); /* malloc'd */
#endif /* NOC_COLORS_H */
