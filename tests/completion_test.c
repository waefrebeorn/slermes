/*
 * completion_test.c — real behavioral test for port_completion.c
 *
 * Builds a small command tree and asserts the generators emit the expected
 * shell-script fragments (mirroring hermes_cli/completion.py output shape).
 */

#include "completion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int checks = 0, failures = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } } while (0)
#define HAS(hay, needle, msg) do { checks++; const char *_h=(hay), *_n=(needle); \
    if (!_h || !_n || strstr(_h,_n)==NULL) { failures++; printf("FAIL: %s\n  missing=%s\n", msg, _n?_n:"(null)"); } } while (0)

static char *strdup_(const char *s) { return s ? strdup(s) : NULL; }

/* Build a small tree:
 *   chat (subcommands: send, history)
 *   profile (subcommands: use, delete; help text with a quote)
 *   doctor (flags: -f, --force)
 */
static completion_node_t **build_tree(void) {
    static char **chat_flags = NULL;
    if (!chat_flags) { chat_flags = malloc(sizeof(char*)); chat_flags[0] = NULL; }
    completion_node_t *chat_send = completion_node_new("send", "Send a message", NULL, NULL);
    completion_node_t *chat_history = completion_node_new("history", "Show history", NULL, NULL);
    static completion_node_t **chat_subs = NULL;
    if (!chat_subs) { chat_subs = malloc(3 * sizeof(completion_node_t*)); chat_subs[0]=chat_send; chat_subs[1]=chat_history; chat_subs[2]=NULL; }
    completion_node_t *chat = completion_node_new("chat", "Chat with Hermes", chat_flags, chat_subs);

    static char **prof_flags = NULL;
    if (!prof_flags) { prof_flags = malloc(sizeof(char*)); prof_flags[0] = NULL; }
    completion_node_t *prof_use = completion_node_new("use", "Switch profile", NULL, NULL);
    completion_node_t *prof_del = completion_node_new("delete", "Remove profile", NULL, NULL);
    static completion_node_t **prof_subs = NULL;
    if (!prof_subs) { prof_subs = malloc(3 * sizeof(completion_node_t*)); prof_subs[0]=prof_use; prof_subs[1]=prof_del; prof_subs[2]=NULL; }
    /* help with shell-unsafe chars to exercise _clean */
    completion_node_t *profile = completion_node_new("profile", "Manage 'profiles' \"here\"", prof_flags, prof_subs);

    static char **doc_flags = NULL;
    if (!doc_flags) { doc_flags = malloc(3 * sizeof(char*)); doc_flags[0]=strdup("-f"); doc_flags[1]=strdup("--force"); doc_flags[2]=NULL; }
    completion_node_t *doctor = completion_node_new("doctor", "Diagnose", doc_flags, NULL);

    static completion_node_t *tree[4];
    tree[0] = chat; tree[1] = profile; tree[2] = doctor; tree[3] = NULL;
    return tree;
}

int main(void) {
    completion_node_t **tree = build_tree();

    /* _clean */
    char *c = completion_clean("He said 'hi' and \\ran", 60);
    HAS(c, "He said hi and ran", "_clean strips quotes+backslash");
    free(c);
    c = completion_clean("abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOP", 20);
    CHECK(strlen(c) == 20, "_clean truncates to maxlen");
    free(c);

    /* bash */
    char *bash = completion_generate_bash(tree);
    HAS(bash, "complete -F _hermes_completion hermes", "bash: completion binding");
    HAS(bash, "chat)", "bash: chat case");
    HAS(bash, "profile)", "bash: profile case");
    HAS(bash, "use|delete|show|alias|rename|export", "bash: profile actions");
    HAS(bash, "COMPREPLY=($(compgen -W \"-f --force\"", "bash: doctor flags");
    HAS(bash, "compgen -W \"chat doctor profile\"", "bash: top-level sorted commands");
    free(bash);

    /* zsh */
    char *zsh = completion_generate_zsh(tree);
    HAS(zsh, "compdef _hermes hermes", "zsh: compdef");
    HAS(zsh, "'chat:Chat with Hermes'", "zsh: top cmd with cleaned help");
    HAS(zsh, "'profile:Manage profiles here'", "zsh: _clean applied to help (quotes stripped)");
    HAS(zsh, "_describe 'chat command' chat_cmds", "zsh: chat subcommand describe (dash->underscore)");
    HAS(zsh, "use|delete|show|alias|rename|export", "zsh: profile action case");
    free(zsh);

    /* fish */
    char *fish = completion_generate_fish(tree);
    HAS(fish, "complete -c hermes -f", "fish: disable file completion");
    HAS(fish, "__fish_seen_subcommand_from chat", "fish: chat subcommand guard");
    HAS(fish, "'Manage profiles here'", "fish: cleaned profile help");
    HAS(fish, "__fish_seen_subcommand_from use; and __fish_seen_subcommand_from profile", "fish: profile action profiles");
    HAS(fish, "-n 'not __fish_seen_subcommand_from chat doctor profile'", "fish: top-level guard with sorted names");
    free(fish);

    /* cleanup */
    for (size_t i = 0; tree[i]; i++) completion_node_free(tree[i]);

    printf("completion_test: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
