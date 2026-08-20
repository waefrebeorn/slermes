/* gate: format_size — traced C copy of src/tools/curator_backup.c::format_size.
 * Faithful port of LIVE Python hermes_cli/sizefmt.py::format_bytes.
 * WCUP_* records the SAME control structure LIVE takes, so the cup matches iff
 * the C port executes the identical branch ladder as LIVE Python.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "watercup.h"

static const char *format_size(unsigned long long bytes, char *buf, size_t bufsz) {
    WCUP_BEGIN("format_size");
    static const char *units[] = {"B", "KB", "MB", "GB", "TB", NULL};
    double val = (double)bytes;
    int u = 0;
    if (val < 1024.0) {
        WCUP_BRANCH(1);
        snprintf(buf, bufsz, "%llu B", bytes);
    } else {
        WCUP_BRANCH(0);
        while (units[u + 1] && val >= 1024.0) {
            WCUP_BRANCH(1);   /* took another 1024 step */
            val /= 1024.0; u++;
        }
        snprintf(buf, bufsz, "%.1f %s", val, units[u]);
    }
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("format_size");
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s format_size <bytes>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[32];
        format_size((unsigned long long)strtoull(argv[i], NULL, 10), buf, sizeof buf);
    }
    return 0;
}
