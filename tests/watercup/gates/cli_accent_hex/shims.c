#include <stdlib.h>
#include <string.h>

char *cl2_accent_hex(void) {
    /* Mirrors Python _accent_hex: hardcoded fallback "#FFBF00" */
    return strdup("#FFBF00");
}
