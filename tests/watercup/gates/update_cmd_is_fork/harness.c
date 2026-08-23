/* gate: update_cmd_is_fork — traced C copy of
 * src/hermes_cli/update_cmd.c::uc_is_fork_origin
 * (LIVE twin: hermes_cli/update_cmd.py::_is_fork).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: empty origin_url
 *   V1: normalized (rstrip "/", strip ".git")
 *   official loop (L ticks): B2: normalized == official_normalized?
 *                              -> return false
 *   B3: no official matched -> true
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

/* LIVE OFFICIAL_REPO_URLS is a set; traced in SORTED order so the cup is
 * deterministic across Python hash seeds. */
static const char *UC_OFFICIAL_REPO_URLS[] = {
    "git@github.com:NousResearch/hermes-agent",
    "git@github.com:NousResearch/hermes-agent.git",
    "https://github.com/NousResearch/hermes-agent",
    "https://github.com/NousResearch/hermes-agent.git",
    NULL,
};

static bool uc_is_fork_origin(const char *origin_url) {
    WCUP_BEGIN("is_fork");
    bool result = true;
    if (!origin_url || !*origin_url) {
        WCUP_BRANCH(1);                                  /* B1 */
        result = false;
        goto out;
    }
    WCUP_BRANCH(0);
    {
        size_t len = strlen(origin_url);
        while (len > 0 && origin_url[len - 1] == '/') len--;
        if (len >= 4 && strncmp(origin_url + len - 4, ".git", 4) == 0)
            len -= 4;
        char norm[1024];
        if (len >= sizeof(norm)) len = sizeof(norm) - 1;
        memcpy(norm, origin_url, len); norm[len] = '\0';
        WCUP_VALSTR(norm);                               /* V1 */
        for (int i = 0; UC_OFFICIAL_REPO_URLS[i]; i++) {
            const char *official = UC_OFFICIAL_REPO_URLS[i];
            size_t o_len = strlen(official);
            while (o_len > 0 && official[o_len - 1] == '/') o_len--;
            if (o_len >= 4 && strncmp(official + o_len - 4, ".git", 4) == 0)
                o_len -= 4;
            WCUP_LOOP_TICK();
            int eq = (len == o_len && strncmp(norm, official, len) == 0);
            WCUP_BRANCH(eq);                             /* B2 */
            if (eq) { result = false; goto out; }
        }
        WCUP_BRANCH(1);                                  /* B3 */
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("is_fork");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_fork <origin_url>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) uc_is_fork_origin(argv[i]);
    return 0;
}
