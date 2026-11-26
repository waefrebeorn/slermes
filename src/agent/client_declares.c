/* client_declares — capability declarations for the auxiliary client.
 *
 * Python reads an attribute off the client object (agent/auxiliary_client.py:_client_declares):
 *
 *     bool(getattr(client_obj, flag, False))   # any exception -> False
 *
 * i.e. "the client declares this capability"; absent means no. C clients have no attribute bag,
 * so the same thing is a registry they set at construction: client_declare(kind, flag). This used
 * to be a stub returning 0 for every input, which read as "no client ever declares anything" --
 * a plausible negative that no caller could distinguish from a real answer.
 */
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define CLIENT_DECL_MAX 128

typedef struct {
    char kind[64];
    char flag[64];
} client_decl_t;

static client_decl_t g_decls[CLIENT_DECL_MAX];
static size_t g_decl_count;

static int same(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

/* Register that `client_obj` declares `flag`. Idempotent; false when full or on bad input. */
int client_declare(const char *client_obj, const char *flag) {
    if (!client_obj || !*client_obj || !flag || !*flag) return 0;
    for (size_t i = 0; i < g_decl_count; i++)
        if (same(g_decls[i].kind, client_obj) && same(g_decls[i].flag, flag)) return 1;
    if (g_decl_count >= CLIENT_DECL_MAX) return 0;
    snprintf(g_decls[g_decl_count].kind, sizeof g_decls[g_decl_count].kind, "%s", client_obj);
    snprintf(g_decls[g_decl_count].flag, sizeof g_decls[g_decl_count].flag, "%s", flag);
    g_decl_count++;
    return 1;
}

/* Drop every declaration (tests, reconfiguration). */
void client_declare_reset(void) {
    memset(g_decls, 0, sizeof g_decls);
    g_decl_count = 0;
}

/* Python's _client_declares: truthy attribute, absent -> 0, never an error. */
int client_declares(const char *client_obj, const char *flag) {
    if (!client_obj || !*client_obj || !flag || !*flag) return 0;
    for (size_t i = 0; i < g_decl_count; i++)
        if (same(g_decls[i].kind, client_obj) && same(g_decls[i].flag, flag)) return 1;
    return 0;
}
