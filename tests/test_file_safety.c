/* File safety tests — verify write-deny and read-block path checks.
 *
 * Tests use file_safety_set_test_paths() to avoid env dependencies.
 *
 * Tests:
 * 1. Deny SSH authorized_keys
 * 2. Deny ~/.ssh/ prefix
 * 3. Deny /etc/sudoers
 * 4. Deny /etc/passwd
 * 5. Deny ~/.bashrc
 * 6. Deny ~/.aws/ prefix
 * 7. Deny hermes_home/.env
 * 8. Deny hermes_home/auth.json (control file)
 * 9. Deny hermes_root/.env
 * 10. Allow regular file (tmp)
 * 11. Allow NULL/empty
 * 12. Read-block: credential file
 * 13. Read-block: skills/.hub/ cache
 * 14. Read-block: mcp-tokens/ path
 * 15. Read-block: safe file returns NULL
 * 16. Deny mcp-tokens/ directory prefix
 */

#include "hermes_file_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s\n", name); \
    } else { \
        passed++; \
    } \
} while(0)

int main(void)
{
    /* Setup: point test paths to known locations under /tmp */
    file_safety_set_test_paths("/tmp/hermes-test-home", "/tmp/hermes-test-root");

    /* Test 1: Exact denied paths */
    TEST("deny authorized_keys", is_write_denied("~/.ssh/authorized_keys"));
    TEST("deny id_rsa", is_write_denied("~/.ssh/id_rsa"));

    /* Test 2: Denied prefixes */
    TEST("deny .ssh dir", is_write_denied("~/.ssh/some_new_file"));

    /* Test 3: Absolute denied paths */
    TEST("deny /etc/sudoers", is_write_denied("/etc/sudoers"));
    TEST("deny /etc/passwd", is_write_denied("/etc/passwd"));

    /* Test 4: Shell configs */
    TEST("deny .bashrc", is_write_denied("~/.bashrc"));
    TEST("deny .zshrc", is_write_denied("~/.zshrc"));

    /* Test 5: Directory prefixes */
    TEST("deny .aws/ prefix", is_write_denied("~/.aws/credentials"));

    /* Test 6: Hermes control files under test home */
    TEST("deny home/.env", is_write_denied("/tmp/hermes-test-home/.env"));
    TEST("deny auth.json", is_write_denied("/tmp/hermes-test-home/auth.json"));
    TEST("deny config.yaml", is_write_denied("/tmp/hermes-test-home/config.yaml"));

    /* Test 7: Hermes root control files */
    TEST("deny root/.env", is_write_denied("/tmp/hermes-test-root/.env"));

    /* Test 8: mcp-tokens */
    TEST("deny mcp-tokens dir", is_write_denied("/tmp/hermes-test-home/mcp-tokens"));
    TEST("deny mcp-tokens file", is_write_denied("/tmp/hermes-test-home/mcp-tokens/some_token.json"));

    /* Test 9: Allow regular file */
    TEST("allow /tmp/foo", !is_write_denied("/tmp/test_allowed_file.txt"));
    TEST("allow relative file", !is_write_denied("/var/log/syslog"));

    /* Test 10: Empty / NULL */
    TEST("deny NULL path", is_write_denied(NULL));
    TEST("deny empty path", is_write_denied(""));

    /* Test 11: Read-block — credential files */
    {
        char *err = get_read_block_error("/tmp/hermes-test-home/.env");
        TEST("read-block .env", err != NULL);
        if (err) {
            TEST("read-block .env msg", strstr(err, "Access denied") != NULL);
            free(err);
        }
    }

    /* Test 12: Read-block — auth.json */
    {
        char *err = get_read_block_error("/tmp/hermes-test-home/auth.json");
        TEST("read-block auth.json", err != NULL);
        free(err);
    }

    /* Test 13: Read-block — skills/.hub/ cache */
    {
        char *err = get_read_block_error("/tmp/hermes-test-home/skills/.hub/index-cache/foo");
        TEST("read-block skills cache", err != NULL);
        free(err);
    }

    /* Test 14: Read-block — mcp-tokens */
    {
        char *err = get_read_block_error("/tmp/hermes-test-home/mcp-tokens/token.json");
        TEST("read-block mcp-tokens", err != NULL);
        if (err) {
            TEST("mcp-tokens msg", strstr(err, "MCP token") != NULL);
            free(err);
        }
    }

    /* Test 15: Read-block — safe file returns NULL */
    {
        char *err = get_read_block_error("/tmp/safe_file.txt");
        TEST("read-block safe file", err == NULL);
    }

    /* Test 16: Read-block — NULL input */
    {
        char *err = get_read_block_error(NULL);
        TEST("read-block NULL", err == NULL);
    }

    /* Test 17: get_write_denied_error — credential path */
    {
        char *err = get_write_denied_error("/tmp/hermes-test-home/.env", "Write");
        TEST("write-error credential", err != NULL);
        if (err) {
            TEST("write-error credential msg", strstr(err, "Write denied") != NULL);
            free(err);
        }
    }

    /* Test 18: get_write_denied_error — allowed path returns NULL */
    {
        char *err = get_write_denied_error("/tmp/allowed_write_test.txt", "Write");
        TEST("write-error allowed", err == NULL);
        free(err);
        (void)err;
    }

    /* Test 19: raise_if_read_blocked — no crash on safe path */
    {
        raise_if_read_blocked("/tmp/safe_file.txt");
        TEST("raise safe no crash", 1);
    }

    /* Test 20: raise_if_read_blocked — no crash on blocked path */
    {
        raise_if_read_blocked("/tmp/hermes-test-home/.env");
        TEST("raise blocked no crash", 1);
    }

    printf("file_safety: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
