#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* shim: cl2_is_table_divider — mirrors Python agent/markdown_tables.py::is_table_divider
 * Python logic:
 *   if not row or not row.startswith('|') or not row.endswith('|'): return False
 *   cells = [c.strip() for c in row.split('|')[1:-1]]
 *   if len(cells) < 2: return False
 *   if not all(cell_is_divider_regex.match(c) for c in cells): return False
 *   return True
 *   where cell_is_divider_regex = re.compile(r'^[ \t]*:?-{3,}:?[ \t]*$') */

static bool cell_is_divider(const char *s) {
    /* strip leading/trailing whitespace */
    while (*s == ' ' || *s == '\t') s++;
    const char *end = s + strlen(s);
    while (end > s && (*(end-1) == ' ' || *(end-1) == '\t')) end--;
    if (end == s) return false;  /* empty */
    const char *p = s;
    /* optional leading : */
    if (*p == ':') p++;
    /* count dashes */
    int dashes = 0;
    while (*p == '-') { dashes++; p++; }
    if (dashes < 3) return false;
    /* optional trailing : */
    if (*p == ':') p++;
    /* only whitespace allowed after */
    while (*p == ' ' || *p == '\t') p++;
    return p == end;
}

bool cl2_is_table_divider(const char *row) {
    if (!row || !*row) return false;
    /* Python: if not row or not row.startswith('|') or not row.endswith('|'): return False */
    size_t len = strlen(row);
    if (len < 2 || row[0] != '|' || row[len-1] != '|') return false;
    /* Python: cells = [c.strip() for c in row.split('|')[1:-1]] */
    /* Split between first | and last | */
    int cells = 0;
    bool all_cells_divider = true;
    const char *p = row + 1;
    const char *end = row + len - 1;
    while (p < end) {
        const char *cell_start = p;
        while (p < end && *p != '|') p++;
        size_t cell_len = p - cell_start;
        /* strip whitespace to get cell content for divider check */
        const char *cs = cell_start;
        while (cs < cell_start + cell_len && (*cs == ' ' || *cs == '\t')) cs++;
        const char *ce = cell_start + cell_len;
        while (ce > cs && (*(ce-1) == ' ' || *(ce-1) == '\t')) ce--;
        /* Check the STRIPPED content against divider regex */
        if (cs < ce) {
            /* Build a null-terminated copy of stripped content */
            char stripped[256];
            size_t slen = ce - cs;
            if (slen >= sizeof(stripped)) slen = sizeof(stripped) - 1;
            memcpy(stripped, cs, slen);
            stripped[slen] = '\0';
            if (!cell_is_divider(stripped)) {
                all_cells_divider = false;
            }
        } else {
            /* empty cell after stripping — not a divider */
            all_cells_divider = false;
        }
        cells++;
        if (p < end) p++;  /* skip pipe */
    }
    if (cells < 2) return false;
    if (!all_cells_divider) return false;
    return true;
}
