/*
 * cli_command_registry_test.c — behavioral test for port_cli_command_registry.c
 *
 * Exercises resolve_command, the COMMANDS/SUBCOMMANDS index builders, and
 * completion_walk (the _walk port) feeding generate_bash.
 */

#include "cli_command_registry.h"
#include "completion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int checks = 0, failures = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } } while (0)
#define HAS(hay, needle, msg) do { checks++; const char *_h=(hay), *_n=(needle); \
    if (!_h || !_n || strstr(_h,_n)==NULL) { failures++; printf("FAIL: %s\n  missing=%s\n", msg, _n?_n:"(null)"); } } while (0)

static void test_resolve_command(void) {
    const cli_command_def_t *c;
    c = cli_resolve_command("goal");
    CHECK(c && strcmp(c->name, "goal") == 0, "resolve 'goal'");
    c = cli_resolve_command("/goal");
    CHECK(c && strcmp(c->name, "goal") == 0, "resolve '/goal' (slash stripped)");
    c = cli_resolve_command("GOAL");
    CHECK(c && strcmp(c->name, "goal") == 0, "resolve 'GOAL' (case-insensitive)");
    c = cli_resolve_command("bg");                  /* alias of background */
    CHECK(c && strcmp(c->name, "background") == 0, "resolve alias 'bg' -> background");
    c = cli_resolve_command("reset");               /* alias of new */
    CHECK(c && strcmp(c->name, "new") == 0, "resolve alias 'reset' -> new");
    c = cli_resolve_command("nonexistent_cmd_xyz");
    CHECK(c == NULL, "resolve unknown -> NULL");
}

static void test_indexes(void) {
    cli_index_entry_t *cmds = cli_build_commands_index();
    size_t cnt = 0;
    for (size_t i = 0; cmds[i].key; i++) cnt++;
    /* should equal number of non-gateway commands (+ aliases) */
    size_t non_gw = 0, aliases = 0;
    for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
        const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
        if (!c) continue;
        if (c->gateway_only) continue;
        non_gw++;
        for (const char *const *a = c->aliases; a && *a; a++) aliases++;
    }
    CHECK(cnt == non_gw + aliases, "commands index count matches registry");
    /* "/goal" present with a description */
    bool found_goal = false;
    for (size_t i = 0; cmds[i].key; i++)
        if (strcmp(cmds[i].key, "/goal") == 0 && cmds[i].value && *cmds[i].value) found_goal = true;
    CHECK(found_goal, "/goal present with description");

    cli_index_entry_t *subs = cli_build_subcommands_index();
    size_t scnt = 0;
    for (size_t i = 0; subs[i].key; i++) scnt++;
    /* kanban has many subcommands; ensure some exist */
    CHECK(scnt > 50, "subcommands index non-empty");
    bool found_kanban_init = false;
    for (size_t i = 0; subs[i].key; i++)
        if (strcmp(subs[i].key, "/kanban") == 0 && strcmp(subs[i].value, "init") == 0) found_kanban_init = true;
    CHECK(found_kanban_init, "/kanban init subcommand present");

    cli_free_index(cmds);
    cli_free_index(subs);
}

static void test_completion_walk(void) {
    struct completion_node_t **tree = completion_walk(NULL, NULL);
    CHECK(tree != NULL, "walk returns a tree");
    /* count top-level nodes */
    size_t n = 0;
    for (size_t i = 0; tree[i]; i++) n++;
    size_t non_gw = 0;
    for (size_t i = 0; i < CLI_COMMAND_REGISTRY_COUNT; i++) {
        const cli_command_def_t *c = CLI_COMMAND_REGISTRY[i];
        if (c && !c->gateway_only) non_gw++;
    }
    CHECK(n == non_gw, "walk node count == non-gateway commands");

    /* generate bash; should reference real command names */
    char *bash = completion_generate_bash(tree);
    CHECK(bash != NULL, "generate_bash succeeds");
    HAS(bash, "goal", "bash completion references 'goal'");
    HAS(bash, "kanban", "bash completion references 'kanban'");
    /* 'init' should appear as a kanban subcommand completion */
    HAS(bash, "init", "bash completion references 'init' subcommand");

    /* flags derived from args_hint pipe patterns: timestamps [on|off|status] */
    bool found_ts = false;
    for (size_t i = 0; tree[i]; i++)
        if (strcmp(tree[i]->name, "timestamps") == 0) {
            found_ts = (tree[i]->flags != NULL);
        }
    CHECK(found_ts, "timestamps node carries derived flags (on/off/status)");

    free(bash);
    for (size_t i = 0; tree[i]; i++) completion_node_free(tree[i]);
    free(tree);
}

int main(void) {
    test_resolve_command();
    test_indexes();
    test_completion_walk();
    printf("cli_command_registry_test: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
