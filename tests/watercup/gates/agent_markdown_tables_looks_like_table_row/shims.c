#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* shim: looks_like_table_row — mirrors Python agent/markdown_tables.py::looks_like_table_row */

bool looks_like_table_row(const char *row) {
    if (!row) return false;
    /* Python raises TypeError on None check, so NULL -> false */
    /* if "|" not in row: return False */
    if (!strchr(row, '|')) return false;
    /* stripped = row.strip() */
    const char *s = row;
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
    /* if not stripped: return False */
    if (len == 0) return false;
    /* if stripped.startswith("|"): return True */
    if (s[0] == '|') return true;
    /* return row.count("|") >= 2 (on stripped content) */
    int pipes = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '|') pipes++;
    }
    return pipes >= 2;
}
