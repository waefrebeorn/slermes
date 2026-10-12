/* gate: real__tools_read_extract_py___needs_ocr_warning — GAP sentinel for _needs_ocr_warning */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_needs_ocr_warning";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _needs_ocr_warning <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_needs_ocr_warning");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_needs_ocr_warning");
    }
    return 0;
}
