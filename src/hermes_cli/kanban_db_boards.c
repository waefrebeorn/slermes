/*
 * kanban_db_boards.c — C port of upstream Hermes hermes_cli/kanban_db_boards.py.
 * Gate: w15_kanbandbb_576f01
 *
 * PoP: w15_kanbandbb_576f01_default_board_display_name @ hermes_cli/kanban_db_boards.py:_default_board_display_name
 *
 * Python:
 *   " ".join(part.capitalize() for part in slug.replace("_", "-").split("-") if part) or slug
 * str.capitalize() upper-cases the first character and lower-cases the rest.
 * Fixtures use ASCII slugs so C toupper/tolower match Python exactly.
 */
#include "noc_w15_kanbandbb_576f01.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

struct json_t *w15_kanbandbb_576f01_default_board_display_name(const char *slug_in) {
    const char *slug = slug_in ? slug_in : "";
    size_t n = strlen(slug);
    /* Result never longer than 2*n + 2 (one extra space per segment, NUL). */
    char *out = (char *)malloc(n * 2 + 2);
    if (!out) return json_string("");
    size_t j = 0;
    size_t i = 0;
    size_t seg_start = 0;
    int have_part = 0;
    while (i <= n) {
        unsigned char c = (i < n) ? (unsigned char)slug[i] : 0;
        int is_sep = (i < n) && (c == '_' || c == '-');
        if (i == n || is_sep) {
            if (have_part) {
                size_t seg = i - seg_start;
                for (size_t k = 0; k < seg; k++) {
                    unsigned char sc = (unsigned char)slug[seg_start + k];
                    out[j++] = (k == 0) ? (char)toupper(sc) : (char)tolower(sc);
                }
                have_part = 0;
            }
            if (i < n) {
                /* separator: skip it; a following non-empty part adds its own space */
            }
        } else {
            if (!have_part) {
                if (j > 0) out[j++] = ' ';
                seg_start = i;
                have_part = 1;
            }
        }
        i++;
    }
    out[j] = '\0';
    /* Empty join -> Python `or slug`. */
    if (j == 0 && *slug) { free(out); return json_string(slug); }
    json_t *res = json_string(out);
    free(out);
    return res;
}
