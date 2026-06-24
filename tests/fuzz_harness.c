/*
 * fuzz_harness.c — Fuzz test harness for Hermes C (T08).
 *
 * Exercises tool JSON parsing, provider config parsing, and other
 * input-processing functions with random/corrupted inputs.
 * Build with: make fuzz
 * Run with:   ./hermes-fuzz [iterations]
 */

#include "hermes_json.h"
#include "json.h"
#include "yaml.h"
#include "cron.h"
#include "dotenv.h"
#include "http.h"
#include "json5.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Number of fuzz iterations per run */
#define DEFAULT_ITERATIONS 10000

/* Max input length */
#define MAX_INPUT 4096

/* ─── Test framework ──────────────────────────────────── */

static int passed = 0;
static int failed = 0;

#define TEST(expr, name) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* ─── 1. JSON parsing with random binary data ─────────── */

static void fuzz_json_random(void) {
    unsigned char buf[MAX_INPUT];
    for (int i = 0; i < 1000; i++) {
        int len = rand() % MAX_INPUT;
        for (int j = 0; j < len; j++)
            buf[j] = (unsigned char)(rand() & 0xFF);
        buf[len] = '\0';
        char *err = NULL;
        json_t *root = json_parse((const char *)buf, &err);
        free(err);
        if (root) json_free(root);
    }
    passed++;
    printf("  PASS: fuzz_json_random (1000 iterations, no crashes)\n");
}

/* ─── 2. Tool argument parsing ────────────────────────── */

static void fuzz_tool_args(void) {
    const char *inputs[] = {
        "null",
        "{}",
        "{\"key\": \"value\"}",
        "{\"key\": 123}",
        "{\"key\": []}",
        "{\"key\": {}}",
        "\"just a string\"",
        "[1, 2, 3]",
        "{\"nested\": {\"deeply\": {\"buried\": \"value\"}}}",
        "{\"empty\": \"\", \"spaces\": \"   \", \"newlines\": \"\n\n\"}",
        "{\"unicode\": \"\u00e9\u00e0\u00fc\u00f1\"}",
        "{\"escaped\": \"tab\there\nand\\\\\"quotes\\\\\"\"}",
        "{\"int_str\": \"42\", \"bool_str\": \"true\", \"float_str\": \"3.14\"}",
        "{\"arr\": [1, [2, [3, [4]]]]}",
        "{\"unicode_esc\": \"\\u0048\\u0065\\u006c\\u006c\\u006f\"}",
        "{\"deep_nesting\": {\"a\": {\"b\": {\"c\": {\"d\": {\"e\": {\"f\": \"deep\"}}}}}}}",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        char *err = NULL;
        json_t *args = json_parse(inputs[i], &err);
        free(err);
        if (args) {
            (void)json_get_str(args, "key", "");
            (void)json_get_num(args, "key", 0);
            (void)json_get_bool(args, "key", false);
            json_free(args);
        }
    }
    passed++;
    printf("  PASS: fuzz_tool_args (%zu inputs)\n",
           sizeof(inputs)/sizeof(inputs[0]) - 1);
}

/* ─── 3. Path resolution edge cases ───────────────────── */

static void fuzz_paths(void) {
    const char *paths[] = {
        "", "/", ".", "..", "/../", "/./",
        "foo/bar/../baz", "///foo///bar///",
        "/../../../../etc/passwd",
        "a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p",
        "....", "..foo..", "foo..bar",
        "~", "~/config", "~/../../tmp",
        "foo/bar/", "/foo/bar/.",
        "with spaces/path",
        "special_chars_@#$%^&*()",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        bool traversal = path_has_traversal(paths[i]);
        (void)traversal;
        char *normalized = path_normalize(paths[i]);
        free(normalized);
    }
    passed++;
    printf("  PASS: fuzz_paths (%zu paths)\n",
           sizeof(paths)/sizeof(paths[0]) - 1);
}

/* ─── 4. YAML parsing with random data ────────────────── */

static void fuzz_yaml_parse(void) {
    const char *inputs[] = {
        "",
        "key: value\n",
        "nested:\n  key: value\n",
        "list:\n  - item1\n  - item2\n",
        "bool: true\nint: 42\nfloat: 3.14\n",
        "---\ndoc1: first\n---\ndoc2: second\n",
        "multi:\n  deep:\n    nested:\n      key: val\n",
        "key: \"quoted string\"\n",
        "key: 'single quoted'\n",
        "# just a comment\n",
        "key: value\nkey: duplicate\n",
        ":\n",      /* empty key */
        "trailing:\n  - \n",  /* empty list item */
        "special: @#$%^&*()\n",
        "unicode: \u00e9\u00e0\u00fc\n",
        "long:\n"
        "  - very long string that goes on and on and on and on and on\n"
        "  - another long string that also goes on and on and on and on\n",
        "indent:\n    - proper\n      - improper\n",  /* bad indent */
        "bool_values:\n  - yes\n  - no\n  - true\n  - false\n  - on\n  - off\n",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(inputs[i], &err);
        free(err);
        if (doc) {
            const char *v = yaml_get_string(doc, "key");
            (void)v;
            bool b = yaml_get_bool(doc, "bool", false);
            (void)b;
            int n = yaml_get_int(doc, "int", 0);
            (void)n;
            size_t cnt = yaml_list_count(doc, "list");
            (void)cnt;
            yaml_free(doc);
        }
    }
    passed++;
    printf("  PASS: fuzz_yaml_parse (%zu inputs)\n",
           sizeof(inputs)/sizeof(inputs[0]) - 1);
}

/* ─── 5. Multi-document YAML parsing ──────────────────── */

static void fuzz_yaml_multi(void) {
    const char *inputs[] = {
        "---\na: 1\n---\nb: 2\n",
        "---\n",
        "---\na: 1\n...\n---\nb: 2\n",
        "",
        "---\nkey: val\n---\nkey: val2\n---\nkey: val3\n",
        "---\nlist:\n  - 1\n  - 2\n---\nlist:\n  - a\n  - b\n",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        char *err = NULL;
        size_t count = 0;
        yaml_doc_t **docs = yaml_parse_multi(inputs[i], &count, &err);
        free(err);
        if (docs) {
            for (size_t j = 0; j < count; j++) {
                yaml_free(docs[j]);
            }
            free(docs);
        }
    }
    passed++;
    printf("  PASS: fuzz_yaml_multi (%zu inputs)\n",
           sizeof(inputs)/sizeof(inputs[0]) - 1);
}

/* ─── 6. Dotenv parsing with random content ───────────── */

static void fuzz_dotenv_parse(void) {
    const char *inputs[] = {
        "",
        "KEY=value\n",
        "EMPTY=\n",
        "# comment only\n",
        "KEY1=val1\nKEY2=val2\nKEY3=val3\n",
        "QUOTED=\"quoted value\"\n",
        "SINGLE='single quoted'\n",
        "ESCAPED=\"new\\nline\\ttab\"\n",
        "SPECIAL=@#$%^&*()\n",
        "MULTI_LINE=\"line1\\\nline2\"\n",
        "EXPORT=value\n",
        " SPACE_PREFIX=val\n",
        "TRAILING_SPACE=val \n",
        "DOT.KEY=value\n",
        "UNDER_SCORE_KEY=value\n",
        "NUMBER_KEY_123=value\n",
        "LONG=\""
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
        "\"\n",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        char *err = NULL;
        env_t *env = dotenv_parse(inputs[i], &err);
        free(err);
        if (env) {
            dotenv_free(env);
        }
    }
    passed++;
    printf("  PASS: fuzz_dotenv_parse (%zu inputs)\n",
           sizeof(inputs)/sizeof(inputs[0]) - 1);
}

/* ─── 7. Cron expression parsing ──────────────────────── */

static void fuzz_cron_expr(void) {
    const char *exprs[] = {
        "* * * * *",
        "0 0 * * *",
        "*/5 * * * *",
        "0 9-17 * * 1-5",
        "30 4 * * 1-5",
        "0 0 1 * *",
        "0 0 * * 0",
        "*/15 */2 */3 */4 */5",
        "1-59/5 0-23/3 * * *",
        "JAN-DEC 0-23 * * *",
        "@hourly",
        "@daily",
        "@weekly",
        "@monthly",
        "@yearly",
        "0 0 29 2 *",   /* Feb 29 */
        "0 0 31 4 *",   /* Apr 31 — doesn't exist */
        "60 24 * * *",  /* out of range */
        "* * * * * 0",  /* too many fields */
        "invalid",
        "",
        "*/0 * * * *",  /* step 0 — edge case */
        "1-0 * * * *",  /* inverted range */
        NULL
    };

    for (int i = 0; exprs[i]; i++) {
        char *err = NULL;
        cron_expr_t *c = cron_parse(exprs[i], &err);
        free(err);
        if (c) {
            struct tm tm_val;
            memset(&tm_val, 0, sizeof(tm_val));
            tm_val.tm_year = 2025 - 1900;
            tm_val.tm_mon = 5;
            tm_val.tm_mday = 15;
            tm_val.tm_hour = 10;
            tm_val.tm_min = 30;
            bool match = cron_match(c, &tm_val);
            (void)match;

            struct tm next;
            bool has_next = cron_next(c, &tm_val, &next);
            (void)has_next;

            char *desc = cron_describe(c);
            free(desc);
            cron_free(c);
        }
    }
    passed++;
    printf("  PASS: fuzz_cron_expr (%zu expressions)\n",
           sizeof(exprs)/sizeof(exprs[0]) - 1);
}

/* ─── 8. Host:port splitting ──────────────────────────── */

static void fuzz_host_port(void) {
    const char *inputs[] = {
        "localhost:8080",
        "127.0.0.1:80",
        "[::1]:443",
        "example.com:22",
        "hostname",
        "",
        ":8080",
        "host:",
        "host:0",
        "host:65536",
        "host:abc",
        "192.168.1.1",
        "host.name:1234:5678",  /* multiple colons */
        "  spaced:8080  ",
        "UPPERCASE:443",
        "with_underscore:123",
        "host:12345",
        "a:b:c:d:e:f:g:h:port",
        "[::1]:http",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        char host[256];
        int port = 0;
        bool ok = http_split_host_port(inputs[i], host, sizeof(host), &port);
        (void)ok;
    }
    passed++;
    printf("  PASS: fuzz_host_port (%zu inputs)\n",
           sizeof(inputs)/sizeof(inputs[0]) - 1);
}

/* ─── 9. URL encoding ─────────────────────────────────── */

static void fuzz_url_encode(void) {
    const char *inputs[] = {
        "",
        "simple",
        "hello world",
        "special: @#$%^&*()+=[]{}|;:',.<>?/",
        "unicode: \u00e9\u00e0\u00fc\u00f1\u00a9\u00ae",
        "tabs\tand\nnewlines\nand\rreturns\r",
        "   lots of spaces   ",
        "a\n\n\n\n\na",
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        char *encoded = http_url_encode(inputs[i]);
        if (encoded) {
            free(encoded);
        }
    }
    passed++;
    printf("  PASS: fuzz_url_encode (%zu inputs)\n",
           sizeof(inputs)/sizeof(inputs[0]) - 1);
}

/* ─── 10. Proxy bypass matching ───────────────────────── */

static void fuzz_proxy_bypass(void) {
    const char *hosts[] = {
        "localhost",
        "127.0.0.1",
        "example.com",
        "sub.example.com",
        "192.168.1.1",
        "::1",
        "localhost.localdomain",
        "",
        "*.example.com",
        ".example.com",
        "10.0.0.1",
        NULL
    };

    for (int i = 1; hosts[i]; i++) {
        bool bypass = http_no_proxy_match(hosts[i], hosts[i-1]);
        (void)bypass;
    }
    passed++;
    printf("  PASS: fuzz_proxy_bypass (%zu hosts)\n",
           sizeof(hosts)/sizeof(hosts[0]) - 1);
}

/* ─── 11. Deeply nested JSON parsing ──────────────────── */

static void fuzz_json_nested(void) {
    /* Build deeply nested JSON string */
    char buf[8192];
    int pos = 0;
    int levels = 50;

    pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"a\":");
    for (int i = 0; i < levels; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"b\":");
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "\"deep\"");
    for (int i = 0; i < levels; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "}");
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "}");

    char *err = NULL;
    json_t *root = json_parse(buf, &err);
    free(err);
    if (root) {
        json_free(root);
    }
    passed++;
    printf("  PASS: fuzz_json_nested (%d levels)\n", levels);

    /* Also test array with many elements */
    pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "[");
    for (int i = 0; i < 1000; i++) {
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%d", rand() % 100000);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]");

    err = NULL;
    root = json_parse(buf, &err);
    free(err);
    if (root) {
        json_free(root);
    }
    passed++;
    printf("  PASS: fuzz_json_large_array (1000 elements)\n");
}

/* ─── 12. YAML to JSON serialization ──────────────────── */

static void fuzz_yaml_to_json(void) {
    const char *inputs[] = {
        "key: value\n",
        "nested:\n  inner: val\n",
        "list:\n  - a\n  - b\n  - c\n",
        "mixed:\n  str: hello\n  num: 42\n  flag: true\n",
        "deep:\n  a:\n    b:\n      c: val\n",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(inputs[i], &err);
        free(err);
        if (doc) {
            char *json_str = yaml_to_json_string(doc, NULL);
            free(json_str);
            yaml_free(doc);
        }
    }
    /* Test yaml_map_keys */
    for (int i = 0; inputs[i]; i++) {
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(inputs[i], &err);
        free(err);
        if (doc) {
            size_t count = 0;
            char **keys = yaml_map_keys(doc, NULL, &count);
            if (keys) {
                for (size_t j = 0; j < count; j++) free(keys[j]);
                free(keys);
            }
            yaml_free(doc);
        }
    }
    passed++;
    printf("  PASS: fuzz_yaml_to_json (%zu inputs)\n",
           sizeof(inputs)/sizeof(inputs[0]) - 1);
}

/* ─── 13. Dotenv iteration and access ─────────────────── */

static void fuzz_dotenv_ops(void) {
    const char *input =
        "STRING=hello\n"
        "EMPTY=\n"
        "INT=42\n"
        "BOOL=true\n"
        "QUOTED=\"quoted value\"\n"
        "SPECIAL=!@#$%^&*()\n";

    char *err = NULL;
    env_t *env = dotenv_parse(input, &err);
    free(err);
    if (env) {
        const char *vals[] = {"STRING", "EMPTY", "INT", "BOOL", "QUOTED",
                              "SPECIAL", "NONEXISTENT", NULL};
        for (int i = 0; vals[i]; i++) {
            const char *v = dotenv_get(env, vals[i]);
            (void)v;
        }

        size_t count = dotenv_count(env);
        TEST(count > 0, "dotenv has entries");

        size_t idx = 0;
        const char *key, *val;
        bool has_more = dotenv_iter(env, &idx, &key, &val);
        (void)has_more;

        bool set_ok = dotenv_set(env, "NEW_KEY", "new_value");
        TEST(set_ok, "dotenv_set works");

        dotenv_free(env);
    }
    passed++;
    printf("  PASS: fuzz_dotenv_ops\n");
}

/* ─── 14. JSON5 edge cases ────────────────────────────── */

static void fuzz_json5(void) {
    const char *inputs[] = {
        "",
        "{}",
        "{\"key\": \"value\"}",
        "{key: \"value\"}",      /* unquoted keys */
        "{key: 'value'}",        /* single-quoted strings */
        "{key: value}",          /* unquoted strings */
        "{key: undefined}",      /* undefined */
        "{key: +42}",            /* leading + */
        "{key: -3.14}",          /* negative float */
        "{key: Infinity}",       /* Infinity */
        "{key: NaN}",            /* NaN */
        "// comment\n{key: val}",
        "/* block comment */{key: val}",
        "{\"trailing\": \"comma\",}",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        char *err = NULL;
        json_t *root = json5_parse(inputs[i], &err);
        free(err);
        if (root) {
            json_free(root);
        }
    }
    passed++;
    printf("  PASS: fuzz_json5 (%zu inputs)\n",
           sizeof(inputs)/sizeof(inputs[0]) - 1);
}

/* ─── Main ────────────────────────────────────────────── */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    srand((unsigned int)time(NULL));

    printf("=== Comprehensive Fuzz Test Harness ===\n");

    printf("\n-- 1. Random JSON binary data --\n");
    fuzz_json_random();

    printf("\n-- 2. Tool argument edge cases --\n");
    fuzz_tool_args();

    printf("\n-- 3. Path resolution --\n");
    fuzz_paths();

    printf("\n-- 4. YAML parsing --\n");
    fuzz_yaml_parse();

    printf("\n-- 5. Multi-document YAML --\n");
    fuzz_yaml_multi();

    printf("\n-- 6. Dotenv parsing --\n");
    fuzz_dotenv_parse();

    printf("\n-- 7. Cron expressions --\n");
    fuzz_cron_expr();

    printf("\n-- 8. Host:port splitting --\n");
    fuzz_host_port();

    printf("\n-- 9. URL encoding --\n");
    fuzz_url_encode();

    printf("\n-- 10. Proxy bypass --\n");
    fuzz_proxy_bypass();

    printf("\n-- 11. Deeply nested JSON --\n");
    fuzz_json_nested();

    printf("\n-- 12. YAML to JSON conversion --\n");
    fuzz_yaml_to_json();

    printf("\n-- 13. Dotenv iteration --\n");
    fuzz_dotenv_ops();

    printf("\n-- 14. JSON5 edge cases --\n");
    fuzz_json5();

    printf("\n%s\n", failed > 0 ? "SOME FUZZ TESTS FAILED" : "ALL FUZZ TESTS PASSED");
    printf("Results: %d passed, %d failed\n", passed, failed);

    return failed > 0 ? 1 : 0;
}
