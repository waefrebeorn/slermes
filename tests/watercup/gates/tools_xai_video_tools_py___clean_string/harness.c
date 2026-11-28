/* gate: tools_xai_video_tools_py___clean_string -> real _clean_string -> owned string or NULL (cup "null"). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *tools_xai_video_tools_u_clean_string(const char *value);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <value>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_xai_video_tools_u_clean_string");
        fflush(stdout);
        char *r = tools_xai_video_tools_u_clean_string(argv[i]);
        WCUP_VALSTR(r ? r : "null");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("tools_xai_video_tools_u_clean_string");
        free(r);
    }
    return 0;
}
