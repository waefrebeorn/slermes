/* test_browser_e2e.c — Browser tool E2E integration tests.
 *
 * Tests browser tool registration, handler dispatch, error paths,
 * and URL validation for all 9 browser tools.
 *
 * S12 Browser E2E (P2) — v574.
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Forward declarations from browser.c */
extern void browser_init(void);
extern void browser_cleanup(void);
extern char *browser_navigate_handler(const char *args_json, const char *task_id);
extern char *browser_snapshot_handler(const char *args_json, const char *task_id);
extern char *browser_click_handler(const char *args_json, const char *task_id);
extern char *browser_type_handler(const char *args_json, const char *task_id);
extern char *browser_scroll_handler(const char *args_json, const char *task_id);
extern char *browser_back_handler(const char *args_json, const char *task_id);
extern char *browser_forward_handler(const char *args_json, const char *task_id);
extern char *browser_press_handler(const char *args_json, const char *task_id);
extern char *browser_get_images_handler(const char *args_json, const char *task_id);

static int passed = 0, failed = 0;

#define TEST(name, expr) do {                                    \
    if (expr) { passed++; printf("  PASS: %s\n", name); }         \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

static int is_valid_json(const char *s) {
    if (!s) return 0;
    char *err = NULL;
    json_t *j = json_parse(s, &err);
    if (j) { json_free(j); return 1; }
    free(err);
    return 0;
}

static int has_error(const char *json) {
    return json && strstr(json, "\"error\"") != NULL;
}

static char *call_handler(char *(*handler)(const char*, const char*), const char *args) {
    return handler(args ? args : "{}", NULL);
}

int main(void) {
    printf("=== Browser Tool E2E Tests ===\n\n");

    browser_init();

    /* ============================================================
     * Phase 1: Handler dispatch — all handlers return valid JSON
     * ============================================================ */
    printf("--- Phase 1: Handler dispatch (valid JSON) ---\n");

    typedef char *(*handler_fn)(const char*, const char*);
    typedef struct {
        const char *name;
        handler_fn handler;
    } handler_spec_t;

    handler_spec_t handlers[] = {
        {"navigate",     browser_navigate_handler},
        {"snapshot",     browser_snapshot_handler},
        {"click",        browser_click_handler},
        {"type",         browser_type_handler},
        {"scroll",       browser_scroll_handler},
        {"back",         browser_back_handler},
        {"forward",      browser_forward_handler},
        {"press",        browser_press_handler},
        {"get_images",   browser_get_images_handler},
        {NULL, NULL}
    };

    for (int i = 0; handlers[i].name; i++) {
        char *r = call_handler(handlers[i].handler, NULL);
        char tn[128];
        snprintf(tn, sizeof(tn), "%s(NULL) returns JSON", handlers[i].name);
        TEST(tn, r != NULL && is_valid_json(r));
        if (r) free(r);
    }

    /* ============================================================
     * Phase 3: Error paths — missing required fields
     * ============================================================ */
    printf("\n--- Phase 3: Error paths ---\n");

    char *res = NULL;

    /* Navigate: empty args → error (needs url) */
    res = browser_navigate_handler("{}", NULL);
    TEST("navigate({}) returns error", res != NULL && has_error(res));
    if (res) free(res);

    /* Navigate: wrong key → error (needs 'url') */
    res = browser_navigate_handler("{\"wrong_key\":\"value\"}", NULL);
    TEST("navigate(wrong key) returns error", res != NULL && has_error(res));
    if (res) free(res);

    /* Navigate: invalid JSON */
    res = browser_navigate_handler("{bad json}", NULL);
    TEST("navigate(invalid JSON) returns error", res != NULL && has_error(res));
    if (res) free(res);

    /* Navigate: valid URL (should not crash — was P2 bug from missing url_safety link) */
    res = browser_navigate_handler("{\"url\":\"http://127.0.0.1:1/\"}", NULL);
    TEST("navigate(valid URL) non-null", res != NULL);
    if (res) free(res);

    /* ============================================================
     * Phase 4: Click/Type/Scroll with specific args
     * ============================================================ */
    printf("\n--- Phase 4: Click/Type/Scroll dispatch ---\n");

    char *r = NULL;

    /* Click: valid target */
    r = browser_click_handler("{\"target\":\"@e1\"}", NULL);
    TEST("click(@e1) non-null", r != NULL);
    if (r) free(r);

    /* Click: missing target → error */
    r = browser_click_handler("{\"wrong_key\":\"val\"}", NULL);
    TEST("click(missing target) returns error", r != NULL && has_error(r));
    if (r) free(r);

    /* Type: missing text → error */
    r = browser_type_handler("{\"target\":\"#input\"}", NULL);
    TEST("type(missing text) returns error", r != NULL && has_error(r));
    if (r) free(r);

    /* Type: valid args */
    r = browser_type_handler("{\"target\":\"#input\",\"text\":\"hello\"}", NULL);
    TEST("type(valid) non-null", r != NULL);
    if (r) free(r);

    /* Scroll: valid direction */
    r = browser_scroll_handler("{\"direction\":\"down\"}", NULL);
    TEST("scroll(down) non-null", r != NULL);
    if (r) free(r);

    /* Scroll: missing direction → error */
    r = browser_scroll_handler("{\"amount\":100}", NULL);
    TEST("scroll(missing direction) returns error", r != NULL && has_error(r));
    if (r) free(r);

    /* Scroll: invalid direction → error */
    r = browser_scroll_handler("{\"direction\":\"invalid\"}", NULL);
    TEST("scroll(invalid direction) returns error", r != NULL && has_error(r));
    if (r) free(r);

    /* ============================================================
     * Phase 5: Press/Back/Forward/GetImages dispatch
     * ============================================================ */
    printf("\n--- Phase 5: Press/Back/Forward/GetImages ---\n");

    /* Press: empty args → error */
    r = browser_press_handler("{}", NULL);
    TEST("press({}) returns error", r != NULL && has_error(r));
    if (r) free(r);

    /* Press: missing key → error */
    r = browser_press_handler("{\"wrong_key\":\"val\"}", NULL);
    TEST("press(wrong key) returns error", r != NULL && has_error(r));
    if (r) free(r);

    /* Press: invalid JSON */
    r = browser_press_handler("{bad json}", NULL);
    TEST("press(invalid JSON) returns error", r != NULL && has_error(r));
    if (r) free(r);

    /* Back: empty args → error (no history) */
    r = browser_back_handler("{}", NULL);
    TEST("back({}) returns error", r != NULL && has_error(r));
    if (r) free(r);

    /* Forward: empty args → error (no history) */
    r = browser_forward_handler("{}", NULL);
    TEST("forward({}) returns error", r != NULL && has_error(r));
    if (r) free(r);

    /* GetImages: empty args */
    r = browser_get_images_handler("{}", NULL);
    TEST("get_images({}) non-null", r != NULL);
    if (r) free(r);

    /* ============================================================
     * Phase 6: NULL safety for all handlers
     * ============================================================ */
    printf("\n--- Phase 6: NULL safety ---\n");

    for (int i = 0; handlers[i].name; i++) {
        char *res = handlers[i].handler(NULL, NULL);
        char tn[128];
        snprintf(tn, sizeof(tn), "%s(NULL,NULL) -> returns valid JSON", handlers[i].name);
        TEST(tn, res != NULL && is_valid_json(res));
        if (res) {
            TEST("  contains error field", has_error(res));
            free(res);
        }
    }

    /* ============================================================
     * Cleanup
     * ============================================================ */
    browser_cleanup();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    printf("%s\n", failed ? "SOME TESTS FAILED" : "All Browser E2E tests PASSED");
    return failed > 0 ? 1 : 0;
}
