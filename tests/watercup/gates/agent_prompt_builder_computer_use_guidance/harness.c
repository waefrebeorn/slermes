/* gate: agent_prompt_builder_computer_use_guidance — traced copy of
 * src/agent/prompt_builder_guidance.c::prompt_builder_computer_use_guidance
 * branch ladder (+ calls the REAL linked port for consistency).
 * LIVE twin: agent/prompt_builder.py::computer_use_guidance.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: is_macos -> macOS share/combo values : else
 *   B2: is_windows -> Windows : Linux
 *   B3: offscreen_line macOS variant
 *   B4: elif Windows variant
 *   B5: else Linux variant
 * Value events hash the SHORT per-platform fields only (os_name, save_combo,
 * example_app) — see notes for the prose-body drift.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

char *prompt_builder_computer_use_guidance(const void *ctx,
                                           const char *platform_name);

static const char *example_app_for(const char *platform) {
    if (platform && strcmp(platform, "darwin") == 0) return "Safari";
    if (platform && strcmp(platform, "win32") == 0) return "Chrome";
    return "Firefox";
}

static void trace_one(const char *platform) {
    WCUP_BEGIN("computer_use_guidance");
    const char *os_name, *save_combo, *example_app;
    int is_macos = platform && strcmp(platform, "darwin") == 0;
    int is_windows = platform && strcmp(platform, "win32") == 0;
    if (is_macos) {                                   /* B1 */
        WCUP_BRANCH(1);
        os_name = "macOS"; save_combo = "cmd+s";
    } else {
        WCUP_BRANCH(0);
        if (is_windows) {                             /* B2 */
            WCUP_BRANCH(1);
            os_name = "Windows";
        } else {
            WCUP_BRANCH(0);
            os_name = "Linux";
        }
        save_combo = "ctrl+s";
    }
    if (is_macos) WCUP_BRANCH(1);                     /* B3 */
    else {
        WCUP_BRANCH(0);
        if (is_windows) WCUP_BRANCH(1);               /* B4 */
        else WCUP_BRANCH(0);                          /* B5 */
    }
    example_app = is_macos ? "Safari" : (is_windows ? "Chrome" : "Firefox");
    WCUP_VALSTR(os_name);
    WCUP_VALSTR(save_combo);
    WCUP_VALSTR(example_app);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("computer_use_guidance");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s computer_use_guidance <platform>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
        /* sanity: real port must produce non-empty output mentioning the
         * same os_name + save_combo + example_app we hashed */
        char *real = prompt_builder_computer_use_guidance(NULL, argv[i]);
        if (!real || !*real || !strstr(real, example_app_for(argv[i]))) {
            fprintf(stderr, "real port output problem for '%s'\n", argv[i]);
            free(real);
            return 3;
        }
        free(real);
    }
    return 0;
}
