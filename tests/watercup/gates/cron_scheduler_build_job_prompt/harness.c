/* gate: cron_scheduler_build_job_prompt — traced copy of
 * src/cron/scheduler_prompt.c::scheduler_build_job_prompt, restricted to the
 * pure, I/O-free path: prerun script result ALWAYS supplied by the caller,
 * no context_from, no skills (both sides treat the downstream injection
 * scanner as opaque — benign prompts only).
 *
 * LIVE twin: cron/scheduler.py::_build_job_prompt.
 *
 * Input argv token format: "prompt|has_script|prerun_ok|prerun_output"
 * ('|' may not appear in any field).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: has script?
 *   B2: script success?
 *   B3: non-empty script output? (else SILENT -> None)
 *   B4: has context_from? (always false in this gate)
 *   B5: no effective skills? (always true in this gate)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static const char *CRON_HINT =
    "[IMPORTANT: You are running as a scheduled cron job. "
    "DELIVERY: Your final response will be automatically delivered "
    "to the user \xe2\x80\x94 do NOT use send_message or try to deliver "
    "the output yourself. Just produce your report/output as your "
    "final response and the system handles the rest. "
    "SILENT: If there is genuinely nothing new to report, respond "
    "with exactly \"[SILENT]\" (nothing else) to suppress delivery. "
    "Never combine [SILENT] with content \xe2\x80\x94 either report your "
    "findings normally, or say [SILENT] and nothing more.]\n\n";

static char g_out[8192];

static void wc_build_job_prompt(const char *token) {
    WCUP_BEGIN("build_job_prompt");
    /* split token into 4 '|' fields */
    char prompt[2048] = ""; int has_script = 0; bool ok = false; char out[4096] = "";
    {
        const char *f = token;
        const char *p1 = strchr(f, '|');
        size_t n1 = p1 ? (size_t)(p1 - f) : strlen(f);
        memcpy(prompt, f, n1 < sizeof prompt ? n1 : sizeof prompt - 1);
        if (p1) {
            const char *p2 = strchr(p1 + 1, '|');
            const char *p3 = p2 ? strchr(p2 + 1, '|') : NULL;
            if (p2) has_script = p2 - p1 - 1 > 0 && *(p1 + 1) == '1';
            ok = p3 && *(p2 + 1) == '1';
            if (p3) { strncpy(out, p3 + 1, sizeof out - 1); }
        }
    }

    char blocks[6144] = "";
    bool silent = false;

    /* ── 1. data-collection script ── */
    if (has_script) {                                    /* B1 */
        WCUP_BRANCH(1);
        if (ok) {                                        /* B2 */
            WCUP_BRANCH(1);
            if (out[0]) {                                /* B3 */
                WCUP_BRANCH(1);
                strncat(blocks, "## Script Output\n"
                       "The following data was collected by a pre-run "
                       "script. Use it as context for your analysis.\n\n"
                       "```\n", sizeof blocks - strlen(blocks) - 1);
                strncat(blocks, out, sizeof blocks - strlen(blocks) - 1);
                strncat(blocks, "\n```\n\n", sizeof blocks - strlen(blocks) - 1);
            } else {
                WCUP_BRANCH(0);
                silent = true;   /* no output — skip the AI call entirely */
            }
        } else {
            WCUP_BRANCH(0);
            strncat(blocks, "## Script Error\n"
                   "The data-collection script failed. Report this to "
                   "the user.\n\n```\n", sizeof blocks - strlen(blocks) - 1);
            strncat(blocks, out, sizeof blocks - strlen(blocks) - 1);
            strncat(blocks, "\n```\n\n", sizeof blocks - strlen(blocks) - 1);
        }
    } else {
        WCUP_BRANCH(0);
    }

    /* ── 2. context_from injection ── (absent in every gate input) */
    WCUP_BRANCH(0);                                      /* B4 */

    if (!silent) {
        snprintf(g_out, sizeof g_out, "%s%s%s",
                 CRON_HINT, blocks, prompt);
    } else {
        g_out[0] = '\0';
    }

    /* ── 5. skills normalization: none configured ── */
    WCUP_BRANCH(1);                                      /* B5 */
    /* downstream: scheduler_scan_assembled_cron_prompt / LIVE
     * _scan_assembled_cron_prompt — treated as opaque on both sides
     * (benign prompts pass through unchanged). */

    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("build_job_prompt");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s build_job_prompt <token>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_build_job_prompt(argv[i]);
    return 0;
}
