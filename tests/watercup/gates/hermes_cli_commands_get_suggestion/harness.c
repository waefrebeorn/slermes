/* gate: hermes_cli_commands_get_suggestion — traced copy of
 * src/hermes_cli/commands_wrappers.c::cmd_get_suggestion.
 * LIVE twin: hermes_cli/commands.py::SlashCommandAutoSuggest.get_suggestion
 * (still-typing-the-command-name branch).
 *
 * The real C registry data is linked via extra_src
 * (src/hermes_cli/command_registry.c + completion.c); the traced ladder below
 * is a faithful copy of cmd_get_suggestion's loop:
 *   B1 NULL partial            -> NULL
 *   B2 leading slash stripped  (accept with or without '/')
 *   B3 empty word              -> NULL
 *   (registry walk: gateway_only skip, prefix match, exact-match skip,
 *    shortest-name-wins — no per-iteration events: the C registry and the
 *    LIVE COMMANDS map differ in membership/order, so only the resolved
 *    outcome is the contract)
 *   B4 best match found?       -> remainder or NULL
 * V-event: ghost-text remainder string ("" for NULL).
 *
 * DOCUMENTED DIVERGENCES (inputs excluded): LIVE's COMMANDS includes aliases
 * and its iteration order is length-sorted, so some prefixes resolve to a
 * different shortest command than the C registry (/re -> "set" vs "try",
 * /b -> "g" vs "usy", /s, /t, /i, /h, /in, /lo, /memory, /learn). Empty word
 * ("") diverges too (LIVE would suggest the first command; C returns NULL).
 * Non-slash input diverges by design (LIVE falls back to history). Only
 * prefixes where both registries agree on outcome AND remainder are gated.
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "cli_command_registry.h"
#include "watercup.h"

extern const cli_command_def_t *const CLI_COMMAND_REGISTRY[];
extern size_t CLI_COMMAND_REGISTRY_COUNT;

static const char *wc_suggest(const char *partial_command) {
    WCUP_BEGIN("get_suggestion");
    static char buf[128];
    const char *out = NULL;
    if (!partial_command) {                                     /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    const char *word = partial_command;
    if (*word == '/') {                                         /* B2 */
        WCUP_BRANCH(1);
        word++;
    } else {
        WCUP_BRANCH(0);
    }
    size_t wlen = strlen(word);
    if (wlen == 0) {                                            /* B3 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    {
        const cli_command_def_t *best = NULL;
        for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
            const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
            if (c->gateway_only) continue;
            if (strncmp(c->name, word, wlen) != 0) continue;
            if (strlen(c->name) == wlen) continue;
            if (!best || strlen(c->name) < strlen(best->name)) best = c;
        }
        if (!best) {                                            /* B4 */
            WCUP_BRANCH(1);
            goto out;
        }
        WCUP_BRANCH(0);
        snprintf(buf, sizeof(buf), "%s", best->name + wlen);
        out = buf;
    }
out:
    WCUP_VALSTR(out ? out : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("get_suggestion");
    return out;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_suggestion <partial>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_suggest(argv[i]);
    return 0;
}
