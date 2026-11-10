#include <stdio.h>
#include "omarchy_usage.h"

int main(int argc, char **argv) {
    char out[8192];
    const char *home = argc > 1 ? argv[1] : NULL;
    if (omarchy_usage_scan(home, out, sizeof(out)) != 0) { printf("ERR\n"); return 1; }
    printf("%s\n", out);
    return 0;
}
