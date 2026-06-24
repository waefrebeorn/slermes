/* test_render.c — Integration test for chat_render (markdown + syntax highlighting) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat_render.h"

int main(void) {
    int failures = 0;
    int tests = 0;

    printf("=== test_render.c — Chat Render Integration ===\n");

    /* Test 1: chat_render_message */
    tests++;
    chat_rendered_msg_t *msg = chat_render_message("Hello **world**!", "user");
    if (msg) {
        printf("[PASS] test_render_message: %d tokens\n", msg->token_count);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_render_message: NULL\n");
        failures++;
    }

    /* Test 2: chat_render_markdown */
    tests++;
    msg = chat_render_markdown("# Heading\n\nSome *italic* and **bold** text.\n\n- item 1\n- item 2");
    if (msg && msg->token_count > 0) {
        printf("[PASS] test_render_markdown: %d tokens\n", msg->token_count);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_render_markdown: %d tokens\n", msg ? msg->token_count : -1);
        failures++;
    }

    /* Test 3: chat_render_code_block */
    tests++;
    msg = chat_render_code_block("int main() { return 0; }", "c");
    if (msg && msg->token_count > 0) {
        printf("[PASS] test_render_code_block: %d tokens\n", msg->token_count);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_render_code_block: %d tokens\n", msg ? msg->token_count : -1);
        failures++;
    }

    /* Test 4: chat_render_tool_call */
    tests++;
    msg = chat_render_tool_call("read_file", "{\"path\": \"/tmp/test.txt\"}", "file contents here");
    if (msg && msg->token_count > 0) {
        printf("[PASS] test_render_tool_call: %d tokens\n", msg->token_count);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_render_tool_call: %d tokens\n", msg ? msg->token_count : -1);
        failures++;
    }

    /* Test 5: chat_render_highlight (syntax highlighting) */
    tests++;
    msg = chat_render_highlight("int main() {\n    return 0;\n}", "c");
    if (msg && msg->token_count > 0) {
        int kw_count = 0;
        for (int i = 0; i < msg->token_count; i++) {
            if (msg->tokens[i].type == TOKEN_KEYWORD) kw_count++;
        }
        printf("[PASS] test_highlight: %d tokens, %d keywords\n", msg->token_count, kw_count);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_highlight: %d tokens\n", msg ? msg->token_count : -1);
        failures++;
    }

    /* Test 6: chat_render_plain_text */
    tests++;
    msg = chat_render_message("Hello **world**!", "user");
    if (msg) {
        char *plain = chat_render_plain_text(msg);
        if (plain && strcmp(plain, "Hello world!") == 0) {
            printf("[PASS] test_plain_text: '%s'\n", plain);
        } else {
            printf("[FAIL] test_plain_text: got '%s'\n", plain ? plain : "(null)");
            failures++;
        }
        free(plain);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_plain_text: render failed\n");
        failures++;
    }

    /* Test 7: chat_render_language_normalize */
    tests++;
    const char *norm = chat_render_language_normalize("js");
    if (norm) {
        printf("[PASS] test_language_normalize: 'js' → '%s'\n", norm);
    } else {
        printf("[FAIL] test_language_normalize: NULL\n");
        failures++;
    }

    /* Test 8: chat_render_language_from_filename */
    tests++;
    const char *lang = chat_render_language_from_filename("test.py");
    if (lang && strcmp(lang, "python") == 0) {
        printf("[PASS] test_language_from_filename: 'test.py' → '%s'\n", lang);
    } else {
        printf("[FAIL] test_language_from_filename: got '%s'\n", lang ? lang : "(null)");
        failures++;
    }

    /* Test 9: chat_render_tool_result (success) */
    tests++;
    msg = chat_render_tool_result("bash", "{\"cmd\": \"ls\"}", "file1.txt\nfile2.txt", false);
    if (msg && msg->token_count > 0) {
        printf("[PASS] test_tool_result_success: %d tokens\n", msg->token_count);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_tool_result_success: %d tokens\n", msg ? msg->token_count : -1);
        failures++;
    }

    /* Test 10: chat_render_tool_result (error) */
    tests++;
    msg = chat_render_tool_result("bash", "{\"cmd\": \"bad\"}", "command not found", true);
    if (msg && msg->token_count > 0) {
        printf("[PASS] test_tool_result_error: %d tokens\n", msg->token_count);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_tool_result_error: %d tokens\n", msg ? msg->token_count : -1);
        failures++;
    }

    /* Test 11: chat_render_thinking_block */
    tests++;
    msg = chat_render_thinking_block("Let me think about this...", "claude");
    if (msg && msg->token_count > 0) {
        printf("[PASS] test_thinking_block: %d tokens\n", msg->token_count);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_thinking_block: %d tokens\n", msg ? msg->token_count : -1);
        failures++;
    }

    /* Test 12: chat_render_expandable */
    tests++;
    msg = chat_render_expandable("Details", "Hidden content here", false);
    if (msg && msg->token_count > 0) {
        printf("[PASS] test_expandable: %d tokens\n", msg->token_count);
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_expandable: %d tokens\n", msg ? msg->token_count : -1);
        failures++;
    }

    /* Test 13: Token access */
    tests++;
    msg = chat_render_message("test", "user");
    if (msg) {
        int count = chat_render_token_count(msg);
        const chat_render_token_t *tok = chat_render_get_token(msg, 0);
        if (count > 0 && tok) {
            printf("[PASS] test_token_access: count=%d, first_type=%d\n", count, tok->type);
        } else {
            printf("[FAIL] test_token_access: count=%d, tok=%p\n", count, (void*)tok);
            failures++;
        }
        chat_render_free(msg);
    } else {
        printf("[FAIL] test_token_access: render failed\n");
        failures++;
    }

    printf("\n=== Results: %d/%d passed, %d failed ===\n", tests - failures, tests, failures);
    return failures > 0 ? 1 : 0;
}
