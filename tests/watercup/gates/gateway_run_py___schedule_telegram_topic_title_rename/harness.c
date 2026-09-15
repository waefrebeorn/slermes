/* gate: gateway_run_py___schedule_telegram_topic_title_rename — traced copy of grun_u_schedule_telegram_topic_title_rename */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_u_schedule_telegram_topic_title_rename(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_u_schedule_telegram_topic_title_rename");
    int result = grun_u_schedule_telegram_topic_title_rename(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_u_schedule_telegram_topic_title_rename");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_u_schedule_telegram_topic_title_rename <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
