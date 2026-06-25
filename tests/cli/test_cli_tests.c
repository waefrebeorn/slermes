/*
 * test_cli_tests.c — CLI Integration Tests
 * Tests CLI behavior: help, version, config, setup detection.
 *
 * Build: gcc -O2 -g -I include -o test_cli tests/cli/test_cli_tests.c \
 *        lib/libjson/json.o -lm -lpthread
 *
 * Run: ./test_cli /path/to/slermes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_OUTPUT 8192
#define TEST_PASS 0
#define TEST_FAIL 1

static int g_pass = 0;
static int g_fail = 0;
static char g_binary[512] = "./slermes";

static int run_cli(const char *arg, char *out, size_t outsz) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s %s 2>&1", g_binary, arg);

    FILE *f = popen(cmd, "r");
    if (!f) return -1;

    size_t total = 0;
    size_t n;
    while ((n = fread(out + total, 1, outsz - total - 1, f)) > 0) {
        total += n;
    }
    out[total] = '\0';
    int status = pclose(f);
    return WEXITSTATUS(status);
}

static void test_assert(const char *name, int condition) {
    if (condition) {
        printf("  PASS: %s\n", name);
        g_pass++;
    } else {
        printf("  FAIL: %s\n", name);
        g_fail++;
    }
}

int main(int argc, char **argv) {
    if (argc > 1) snprintf(g_binary, sizeof(g_binary), "%s", argv[1]);

    char out[MAX_OUTPUT];

    printf("=== CLI Tests (binary: %s) ===\n\n", g_binary);

    /* Help */
    printf("--- Help ---\n");
    int exit_code = run_cli("--help", out, sizeof(out));
    test_assert("--help exits 0", exit_code == 0);
    test_assert("--help shows Usage", strstr(out, "Usage") != NULL || strstr(out, "usage") != NULL);

    /* Version */
    printf("\n--- Version ---\n");
    exit_code = run_cli("--version", out, sizeof(out));
    test_assert("--version exits 0", exit_code == 0);
    test_assert("--version shows slermes", strstr(out, "slermes") != NULL || strstr(out, "Slermes") != NULL);

    /* Invalid flag */
    printf("\n--- Error Handling ---\n");
    exit_code = run_cli("--invalid-flag-xyz", out, sizeof(out));
    test_assert("--invalid-flag exits non-zero", exit_code != 0);

    /* Empty input */
    printf("\n--- Empty Input ---\n");
    exit_code = run_cli("", out, sizeof(out));
    test_assert("empty args handled", exit_code == 0 || exit_code == 1);

    /* Doctor */
    printf("\n--- Doctor ---\n");
    exit_code = run_cli("doctor", out, sizeof(out));
    test_assert("doctor exits", exit_code == 0 || exit_code == 1);
    test_assert("doctor shows output", strlen(out) > 10);

    /* Config detection */
    printf("\n--- Config ---\n");
    exit_code = run_cli("config show", out, sizeof(out));
    test_assert("config show handled", exit_code == 0 || exit_code == 1);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
