/* gate: terminal_tool_rewrite_compound_background — drives the REAL
 * src/tools/terminal.c::terminal_rewrite_compound_background (linked via
 * extra_src). LIVE twin: tools/terminal_tool.py::_rewrite_compound_background.
 *
 * Input: one shell command per cup.
 * Rung order (must match twin.py EXACTLY):
 *   B1  rewrite found (result != command)
 *   V   whitespace-normalized rewritten command
 *
 * Agreed sub-contract: V hashes the whitespace-normalized result ('  '-collapse
 * both sides) because the C rewrite-application pass inserts an extra space
 * before '&' vs LIVE ("{ B  & }" vs "{ B & }") — scan-loop parity is exact,
 * see gate.json notes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

char *terminal_rewrite_compound_background(const char *command);

/* ── Link shims: terminal.c's process/env surface is off the rewrite path. ── */
void *registry_register(void *a, void *b) { (void)a; (void)b; return NULL; }
bool approval_is_terminal_dangerous(const char *c) { (void)c; return false; }
char *approval_normalize_command(const char *c) { (void)c; return NULL; }
int daytona_run_command(void) { return -1; }
void env_passthrough_free_list(void *l) { (void)l; }
void **env_passthrough_get_all(void) { return NULL; }
int env_passthrough_register(const char *k, const char *v) { (void)k; (void)v; return 0; }
void file_sync_collect(void) {}
void file_sync_list_free(void *l) { (void)l; }
char *file_sync_mkdir_cmd(void) { return NULL; }
int sandbox_escape_check(const char *c) { (void)c; return 0; }
bool term_check_terminal_requirements(void) { return false; }
void term_cleanup_all_environments(void) {}
void term_cleanup_vm(const char *id) { (void)id; }
bool term_is_persistent_env(const char *id) { (void)id; return false; }
const char *tool_config_get(const char *k) { (void)k; return NULL; }
bool tool_config_get_bool(const char *k, bool d) { (void)k; return d; }
long tool_config_get_int(const char *k, long d) { (void)k; return d; }
long tool_output_get_max_bytes(void) { return 0; }
int forkpty(int *a, char *b, const void *c, const void *d) { (void)a; (void)b; (void)c; (void)d; return -1; }

/* Whitespace-normalize: same semantics as Python " ".join(s.split()). */
static void normalize(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    const char *p = in;
    int pending_space = 0;
    while (*p) {
        if (isspace((unsigned char)*p)) { pending_space = o > 0; p++; continue; }
        if (pending_space && o + 1 < outsz) out[o++] = ' ';
        pending_space = 0;
        if (o + 1 < outsz) out[o++] = *p;
        p++;
    }
    out[o] = '\0';
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rw <command>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("rewrite_compound_background");
        char *result = terminal_rewrite_compound_background(argv[i]);
        const char *res = result ? result : "";
        WCUP_BRANCH(strcmp(res, argv[i]) != 0);      /* B1 */
        char norm[4096];
        normalize(res, norm, sizeof norm);            /* V */
        WCUP_VALSTR(norm);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("rewrite_compound_background");
        free(result);
    }
    return 0;
}
