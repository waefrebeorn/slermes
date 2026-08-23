#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
/* gate: traced copy of src/tools/cronjob_tools.c::cronjob_format_job
 * (PoP: tools/cronjob_tools.py:_format_job) — the display-name /
 * prompt-preview / schedule rungs. Input encoding:
 *   id\x1fname\x1fprompt\x1fskill\x1fschedule_display   ('-' absent, '~' empty).
 * Value: "name|prompt_preview|schedule". */
static const char *F(const char *s) {
    if (!s) return NULL;
    return strcmp(s, "-") == 0 ? NULL : (strcmp(s, "~") == 0 ? "" : s);
}
static void run_one(const char *id_f, const char *name_f, const char *prompt_f,
                    const char *skill_f, const char *sched_f) {
    const char *prompt = F(prompt_f); if (!prompt) prompt = "";
    const char *job_id = F(id_f);     if (!job_id || !*job_id) job_id = "unknown";
    const char *name   = F(name_f);
    const char *skill  = F(skill_f);
    const char *sched  = F(sched_f);
    char out[4096];
    WCUP_BEGIN("format_job");
    if (!name || !*name) {
        WCUP_BRANCH(1);
        if (prompt && *prompt) {
            WCUP_BRANCH(1);
            size_t n = strlen(prompt);
            name = strndup(prompt, n > 50 ? 50 : n);
        } else {
            WCUP_BRANCH(0);
        }
        if (!name || !*name) {
            WCUP_BRANCH(1);
            if (skill && *skill) { WCUP_BRANCH(1); name = skill; }
            else { WCUP_BRANCH(0); name = (job_id && *job_id) ? job_id : "cron job"; }
        } else {
            WCUP_BRANCH(0);
        }
    } else {
        WCUP_BRANCH(0);
    }
    char preview[128];
    if (prompt && *prompt) {
        WCUP_BRANCH(1);
        size_t n = strlen(prompt);
        if (n > 100) {
            WCUP_BRANCH(1);
            memcpy(preview, prompt, 100);
            strcpy(preview + 100, "...");
        } else {
            WCUP_BRANCH(0);
            snprintf(preview, sizeof preview, "%s", prompt);
        }
    } else {
        WCUP_BRANCH(0);
        preview[0] = '\0';
    }
    snprintf(out, sizeof out, "%s|%s|%s", name,
             preview, (sched && *sched) ? sched : "?");
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("format_job");
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s format_job <id\\x1fname\\x1fprompt\\x1fskill\\x1fsched>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *s = strdup(argv[i]);
        char *f[5] = {s,NULL,NULL,NULL,NULL};
        int k = 1;
        for (char *p = s; *p && k < 5; p++)
            if (*p == '\x1f') { *p = '\0'; f[k++] = p + 1; }
        run_one(f[0], f[1], f[2], f[3], f[4]);
        free(s);
    }
    return 0;
}
