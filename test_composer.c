/* test_composer.c — Integration test for chat_composer + TUI integration */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat_composer.h"

int main(void) {
    int failures = 0;
    int tests = 0;

    printf("=== test_composer.c — Chat Composer Integration ===\n");

    /* Test 1: composer_create */
    tests++;
    composer_t *c = composer_create();
    if (c) {
        printf("[PASS] test_composer_create: non-NULL\n");
    } else {
        printf("[FAIL] test_composer_create: NULL\n");
        failures++;
        printf("\n=== Results: %d/%d passed ===\n", tests - failures, tests);
        return 1;
    }

    /* Test 2: composer_insert */
    tests++;
    composer_insert(c, "Hello, world!");
    const char *text = composer_get_text(c);
    if (strcmp(text, "Hello, world!") == 0) {
        printf("[PASS] test_composer_insert: '%s'\n", text);
    } else {
        printf("[FAIL] test_composer_insert: got '%s'\n", text);
        failures++;
    }

    /* Test 3: composer_get_length */
    tests++;
    int len = composer_get_length(c);
    if (len == 13) {
        printf("[PASS] test_composer_length: %d\n", len);
    } else {
        printf("[FAIL] test_composer_length: got %d, expected 13\n", len);
        failures++;
    }

    /* Test 4: composer_move_cursor + backspace */
    tests++;
    composer_set_cursor(c, 5);
    composer_backspace(c);
    text = composer_get_text(c);
    if (strcmp(text, "Hell, world!") == 0) {
        printf("[PASS] test_composer_backspace: '%s'\n", text);
    } else {
        printf("[FAIL] test_composer_backspace: got '%s'\n", text);
        failures++;
    }

    /* Test 5: composer_clear */
    tests++;
    composer_clear(c);
    if (composer_get_length(c) == 0) {
        printf("[PASS] test_composer_clear: empty\n");
    } else {
        printf("[FAIL] test_composer_clear: length=%d\n", composer_get_length(c));
        failures++;
    }

    /* Test 6: Slash command detection */
    tests++;
    composer_insert(c, "/help");
    slash_cmd_t cmd = composer_detect_slash(c);
    if (cmd == SLASH_HELP) {
        printf("[PASS] test_slash_detect: /help → SLASH_HELP\n");
    } else {
        printf("[FAIL] test_slash_detect: got %d, expected SLASH_HELP (%d)\n", cmd, SLASH_HELP);
        failures++;
    }

    /* Test 7: Slash command arg extraction */
    tests++;
    composer_clear(c);
    composer_insert(c, "/model claude-sonnet-4");
    cmd = composer_detect_slash(c);
    const char *arg = composer_slash_arg(c);
    if (cmd == SLASH_MODEL && arg && strcmp(arg, "claude-sonnet-4") == 0) {
        printf("[PASS] test_slash_arg: '%s'\n", arg);
    } else {
        printf("[FAIL] test_slash_arg: cmd=%d, arg='%s'\n", cmd, arg ? arg : "(null)");
        failures++;
    }

    /* Test 8: composer_select_all + get_selection */
    tests++;
    composer_clear(c);
    composer_insert(c, "select me");
    composer_select_all(c);
    char *sel = composer_get_selection(c);
    if (sel && strcmp(sel, "select me") == 0) {
        printf("[PASS] test_select_all: '%s'\n", sel);
    } else {
        printf("[FAIL] test_select_all: got '%s'\n", sel ? sel : "(null)");
        failures++;
    }
    free(sel);

    /* Test 9: composer_delete_selection */
    tests++;
    composer_delete_selection(c);
    if (composer_get_length(c) == 0) {
        printf("[PASS] test_delete_selection: empty after delete\n");
    } else {
        printf("[FAIL] test_delete_selection: length=%d\n", composer_get_length(c));
        failures++;
    }

    /* Test 10: composer_submit */
    tests++;
    composer_insert(c, "test message");
    char *submitted = composer_submit(c);
    if (submitted && strcmp(submitted, "test message") == 0) {
        printf("[PASS] test_composer_submit: '%s'\n", submitted);
    } else {
        printf("[FAIL] test_composer_submit: got '%s'\n", submitted ? submitted : "(null)");
        failures++;
    }
    free(submitted);

    /* Test 11: composer_attach_file */
    tests++;
    composer_clear(c);
    int idx = composer_attach_file(c, "/tmp/test.txt", "text/plain");
    if (idx >= 0) {
        printf("[PASS] test_attach_file: index=%d\n", idx);
    } else {
        printf("[FAIL] test_attach_file: returned %d\n", idx);
        failures++;
    }

    /* Test 12: composer_get_attachment */
    tests++;
    const composer_attachment_t *att = composer_get_attachment(c, 0);
    if (att && strcmp(att->path, "/tmp/test.txt") == 0) {
        printf("[PASS] test_get_attachment: path='%s'\n", att->path);
    } else {
        printf("[FAIL] test_get_attachment: unexpected result\n");
        failures++;
    }

    /* Test 13: composer_clear_attachments */
    tests++;
    composer_clear_attachments(c);
    if (composer_get_attachments(c) == 0) {
        printf("[PASS] test_clear_attachments: 0 attachments\n");
    } else {
        printf("[FAIL] test_clear_attachments: count=%d\n", composer_get_attachments(c));
        failures++;
    }

    /* Test 14: composer_history */
    tests++;
    composer_clear(c);
    composer_insert(c, "first message");
    composer_history_push(c);
    composer_clear(c);
    composer_insert(c, "second message");
    composer_history_push(c);
    composer_clear(c);
    bool hist_ok = composer_history_prev(c);
    if (hist_ok && strcmp(composer_get_text(c), "second message") == 0) {
        printf("[PASS] test_history: prev='%s'\n", composer_get_text(c));
    } else {
        printf("[FAIL] test_history: prev returned %d, text='%s'\n", hist_ok, composer_get_text(c));
        failures++;
    }

    /* Test 15: slash_cmd_name / slash_cmd_from_string */
    tests++;
    const char *name = slash_cmd_name(SLASH_HELP);
    slash_cmd_t parsed = slash_cmd_from_string("/help");
    if (name && strcmp(name, "/help") == 0 && parsed == SLASH_HELP) {
        printf("[PASS] test_slash_cmd_name: '%s' → %d\n", name, parsed);
    } else {
        printf("[FAIL] test_slash_cmd_name: name='%s', parsed=%d\n", name ? name : "(null)", parsed);
        failures++;
    }

    /* Test 16: composer_dispose */
    tests++;
    composer_dispose(c);
    printf("[PASS] test_composer_dispose: no crash\n");

    printf("\n=== Results: %d/%d passed, %d failed ===\n", tests - failures, tests, failures);
    return failures > 0 ? 1 : 0;
}
