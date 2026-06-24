/* test_mcp_integration.c — MCP integration E2E tests.
 *
 * Tests the MCP tool pipeline: registration, dispatch format,
 * error paths, and result parsing. Does NOT require running MCP
 * servers — focuses on the dispatch/formatting layer and error
 * handling for all 7 MCP tools.
 *
 * S12 MCP Integration (P1) — v571.
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
extern void registry_init_mcp(void);
extern tool_registry_t *registry_get(void);
extern size_t registry_count(void);
extern tool_t *registry_find(const char *name);

static int passed = 0, failed = 0;

#define TEST(name, expr) do {                                    \
    if (expr) { passed++; printf("  PASS: %s\n", name); }         \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

static char *call_tool(tool_t *t, const char *args) {
    if (!t) return NULL;
    return t->handler(args ? args : "{}", NULL);
}

int main(void) {
    printf("=== MCP Integration E2E Tests ===\n\n");

    registry_init_mcp();
    size_t total = registry_count();
    TEST("MCP tools registered", total > 0);
    printf("  Total MCP tools: %zu\n\n", total);

    /* ============================================================
     * Phase 1: All 7 MCP tools registered with metadata
     * ============================================================ */
    printf("--- Phase 1: Tool presence ---\n");

    typedef struct {
        const char *name;
        int expected_timeout;
    } tool_spec_t;

    tool_spec_t expected[] = {
        {"mcp_status",        -1},
        {"mcp_tool_call",     180},
        {"mcp_auth",          -1},
        {"mcp_resource_list", -1},
        {"mcp_resource_read", 60},
        {"mcp_prompt_list",   -1},
        {"mcp_prompt_get",    -1},
        {NULL, 0}
    };

    for (int i = 0; expected[i].name; i++) {
        tool_t *t = registry_find(expected[i].name);
        TEST(expected[i].name, t != NULL);
        if (t) {
            TEST("  has description", t->description && t->description[0]);
            TEST("  has schema", t->schema_json && t->schema_json[0]);
            if (expected[i].expected_timeout > 0)
                TEST("  timeout correct", t->timeout_sec == expected[i].expected_timeout);
        }
    }

    /* ============================================================
     * Phase 2: All handlers return valid JSON objects
     * ============================================================ */
    printf("\n--- Phase 2: All handlers return valid JSON ---\n");

    for (int i = 0; expected[i].name; i++) {
        tool_t *t = registry_find(expected[i].name);
        if (t) {
            char *r = call_tool(t, "{}");
            if (r) {
                char tn[128];
                snprintf(tn, sizeof(tn), "%s returns '{'", expected[i].name);
                TEST(tn, r[0] == '{');
                free(r);
            } else {
                char tn[128];
                snprintf(tn, sizeof(tn), "%s returns non-NULL", expected[i].name);
                TEST(tn, 0);
            }
        }
    }

    /* ============================================================
     * Phase 3: Error paths — missing required args
     * ============================================================ */
    printf("\n--- Phase 3: Error paths ---\n");

    /* mcp_tool_call requires server + tool */
    tool_t *tc = registry_find("mcp_tool_call");
    if (tc) {
        char *r = call_tool(tc, "{}");
        TEST("mcp_tool_call({}) non-null", r != NULL);
        if (r) { free(r); }

        r = call_tool(tc, "{\"server\": \"nonexistent\", \"tool\": \"test\"}");
        TEST("mcp_tool_call(bad server) non-null", r != NULL);
        if (r) { free(r); }
    }

    /* mcp_resource_read requires server + uri */
    tool_t *rread = registry_find("mcp_resource_read");
    if (rread) {
        char *r = call_tool(rread, "{}");
        TEST("mcp_resource_read({}) non-null", r != NULL);
        if (r) { free(r); }

        r = call_tool(rread, "{\"server\": \"x\", \"uri\": \"test://r\"}");
        TEST("mcp_resource_read(args) non-null", r != NULL);
        if (r) { free(r); }
    }

    /* mcp_prompt_get requires server + name */
    tool_t *pget = registry_find("mcp_prompt_get");
    if (pget) {
        char *r = call_tool(pget, "{}");
        TEST("mcp_prompt_get({}) non-null", r != NULL);
        if (r) { free(r); }

        r = call_tool(pget, "{\"server\": \"x\", \"name\": \"greeting\"}");
        TEST("mcp_prompt_get(args) non-null", r != NULL);
        if (r) { free(r); }
    }

    /* ============================================================
     * Phase 4: MCP auth subcommand routing
     * ============================================================ */
    printf("\n--- Phase 4: MCP auth subcommands ---\n");

    tool_t *auth = registry_find("mcp_auth");
    if (auth) {
        const char *actions[] = {"status", "grant", "revoke", "list", NULL};
        for (int a = 0; actions[a]; a++) {
            char args[128];
            snprintf(args, sizeof(args), "{\"action\": \"%s\"}", actions[a]);
            char *r = call_tool(auth, args);
            if (r) {
                char tn[128];
                snprintf(tn, sizeof(tn), "mcp_auth(%s)", actions[a]);
                TEST(tn, r[0] == '{');
                free(r);
            }
        }

        char *r = call_tool(auth, "{\"action\": \"grant\", \"server\": \"test\", \"scopes\": [\"read\"]}");
        TEST("mcp_auth(grant with scopes)", r != NULL);
        if (r) { free(r); }
    }

    /* ============================================================
     * Phase 5: Resource/Prompt list handlers
     * ============================================================ */
    printf("\n--- Phase 5: Resource/Prompt list ---\n");

    tool_t *rlist = registry_find("mcp_resource_list");
    if (rlist) {
        char *r = call_tool(rlist, "{}");
        TEST("mcp_resource_list({}) non-null", r != NULL);
        if (r) { free(r); }
    }

    tool_t *plist = registry_find("mcp_prompt_list");
    if (plist) {
        char *r = call_tool(plist, "{}");
        TEST("mcp_prompt_list({}) non-null", r != NULL);
        if (r) { free(r); }
    }

    /* ============================================================
     * Results
     * ============================================================ */
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    printf("%s\n", failed ? "SOME TESTS FAILED" : "All MCP integration tests PASSED");
    return failed > 0 ? 1 : 0;
}
