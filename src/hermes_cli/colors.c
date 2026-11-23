/*
 * colors.c — C port of upstream Hermes hermes_cli/colors.py.
 * PoP: colors_should_use_color @ hermes_cli/colors.py:should_use_color
 * PoP: colors_color @ hermes_cli/colors.py:color
 */
#include "noc_colors.h"
#include <stdlib.h>
#include <string.h>

#define COLORS_RESET "\033[0m"

int colors_should_use_color_ex(int tty, const char *no_color, const char *term) {
    /* `os.environ.get("NO_COLOR") is not None` — even an empty string counts. */
    if (no_color != NULL || (term && strcmp(term, "dumb") == 0)) return 0;
    return tty ? 1 : 0; /* bool(sys.stdout.isatty()) */
}
int colors_should_use_color(int tty) {
    return colors_should_use_color_ex(tty, getenv("NO_COLOR"), getenv("TERM"));
}

char *colors_color_ex(int tty, const char *text, char **codes, size_t ncodes,
                      const char *no_color, const char *term) {
    const char *t = text ? text : "";
    if (!colors_should_use_color_ex(tty, no_color, term)) return strdup(t);

    size_t cap = strlen(t) + strlen(COLORS_RESET) + 16;
    for (size_t i = 0; i < ncodes; i++)
        if (codes[i]) cap += strlen(codes[i]);

    char *out = (char *)malloc(cap);
    if (!out) return strdup(t);
    out[0] = '\0';
    /* "".join(codes) + text + Colors.RESET */
    for (size_t i = 0; i < ncodes; i++)
        if (codes[i]) strcat(out, codes[i]);
    strcat(out, t);
    strcat(out, COLORS_RESET);
    return out;
}

char *colors_color(int tty, const char *text, char **codes, size_t ncodes) {
    return colors_color_ex(tty, text, codes, ncodes,
                           getenv("NO_COLOR"), getenv("TERM"));
}
