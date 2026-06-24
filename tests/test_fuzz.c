/*
 * test_fuzz.c — Fuzz-style robustness tests for Hermes C parsers.
 * T08: Fuzz testing for JSON/YAML/template/regex/URL parsers.
 *
 * Feeds edge-case, malformed, and extreme inputs to parsers and
 * verifies they handle them gracefully (no crashes, no infinite loops).
 *
 * Extended Phase 317: Added YAML, template, regex, URL fuzz tests.
 */

#include "hermes_json.h"
#include <yaml.h>
#include <template.h>
#include <hermes_regex.h>
#include <html.h>
#include <path.h>
#include <cron.h>
#include <toml.h>
#include <csv.h>
#include <base64.h>
#include <datetime.h>
#include <website_policy.h>
#include <fuzzy_match.h>
#include <schema_sanitizer.h>
#include <hermes_glob.h>
#include <hash.h>
#include <difflib.h>
#include <uuid.h>
#include <textwrap.h>
#include <dotenv.h>
#include <binary.h>
#include <interrupt.h>
#include <ansi_strip.h>
#include <budget_config.h>
#include <env_passthrough.h>
#include <threat_patterns.h>
#include <rate_limit.h>
#include <slash_confirm.h>
#include <hermes_signal.h>
#include <error_classifier.h>
#include <tool_output.h>
#include <file_state.h>
#include <debug_helpers.h>
#include <crypto.h>
#include <skill_utils.h>
#include <json5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  TEST: %s\n", name); } while(0)
#define PASS() do { printf("    PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("    FAIL: %s\n", msg); fail++; } while(0)

static void test_json_malformed(void) {
    TEST("JSON: malformed inputs (14 cases)");
    const char *inputs[] = {
        "",           /* empty */
        "{",          /* truncated object */
        "}",          /* lone close */
        "[",          /* truncated array */
        "]",          /* lone close */
        "{,}",        /* empty key */
        "{\"key\":}",  /* missing value */
        "{:}",        /* missing key */
        "[,]",        /* empty array element */
        "'single'",   /* single quotes (invalid JSON) */
        "1e999",      /* overflow */
        "-1e999",     /* underflow */
        "\"\\u\"",    /* truncated unicode escape */
        "\"\\x41\"",  /* invalid escape */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        json_free(doc);
        free(err);
    }
    PASS();
}

static void test_json_large(void) {
    TEST("JSON: large nested structure (1000 levels)");
    char buf[32768];
    int pos = 0;
    buf[pos++] = '{';
    for (int i = 0; i < 1000 && pos < (int)sizeof(buf) - 20; i++) {
        int n = snprintf(buf + pos, sizeof(buf) - pos,
                         "\"k%d\":{%s},", i, i < 999 ? "" : "");
        if (n > 0) pos += n;
    }
    for (int i = 0; i < 1000 && pos < (int)sizeof(buf) - 2; i++)
        buf[pos++] = '}';
    buf[pos] = '\0';
    char *err = NULL;
    json_t *doc = json_parse(buf, &err);
    json_free(doc);
    free(err);
    PASS();
}

static void test_json_long_string(void) {
    TEST("JSON: extremely long string (10K chars)");
    char *buf = malloc(12000);
    if (!buf) { FAIL("malloc"); return; }
    int pos = 0;
    buf[pos++] = '"';
    for (int i = 0; i < 10000 && pos < 11000; i++)
        buf[pos++] = 'A' + (i % 26);
    buf[pos++] = '"';
    buf[pos] = '\0';
    char *err = NULL;
    json_t *doc = json_parse(buf, &err);
    json_free(doc);
    free(err);
    free(buf);
    PASS();
}

static void test_json_unicode(void) {
    TEST("JSON: unicode/multi-byte");
    const char *inputs[] = {
        "\"\u00e9\"",
        "\"\u4e2d\u56fd\"",
        "\"\\u00e9\"",
        "\"\\u0041\"",
        "\"emoji: \\ud83d\\ude0a\"",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        json_free(doc);
        free(err);
    }
    PASS();
}

static void test_json_edge(void) {
    TEST("JSON: edge-case valid inputs (20 cases)");
    const char *inputs[] = {
        "{\"key\": 0}", "{\"key\": -0}", "{\"key\": 1.0}",
        "{\"key\": 1e10}", "{\"key\": 1e-10}",
        "[1,2,3,4,5]", "[1,[2,[3]]]",
        "{\"a\":1,\"b\":2}", "{\"\": 1}",
        "{\"key\twith\ttabs\": 1}", "  {\"key\": 1}  ", "{\"key\": 1}\n",
        /* Array/object boundary fuzz */
        "[[]]", "[[],[]]", "{\"a\":[],\"b\":{}}",
        /* Deeply nested arrays */
        "[[[[[[]]]]]]", "[[[[[[[[[[[]]]]]]]]]]]",
        /* Null and boolean values */
        "null", "true", "false",
        /* Unicode in keys */
        "{\"\\u00e9\":1}", "{\"\\u4e2d\\u56fd\":2}",
        /* Scientific notation edge cases */
        "1.0e+10", "1e-300", "1e308",
        /* Trailing whitespace */
        "  [1, 2, 3]  \n\n  ",
    };

    // Also test JavaScript-style comments (JSON5 extensions)
    const char *json5_inputs[] = {
        "// comment\n{\"key\":1}",
        "/* block */{\"key\":1}",
        "{\"key\":1,}",  // trailing comma
        "{key:1}",       // unquoted key
    };

    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        json_free(doc);
        free(err);
    }
    // JSON5-specific inputs go through json5_parse
    int n5 = sizeof(json5_inputs) / sizeof(json5_inputs[0]);
    for (int i = 0; i < n5; i++) {
        char *err = NULL;
        json_t *doc = json5_parse(json5_inputs[i], &err);
        if (doc) json5_free(doc);
        free(err);
    }
    PASS();
}

/* JSON: Unicode edge cases — surrogates, control chars, BOM */
static void test_json_unicode_edge(void) {
    TEST("JSON: unicode edge cases (8 cases)");
    const char *inputs[] = {
        "\"\\uD83D\\uDE0A\"",    /* surrogate pair (emoji) */
        "\"\\u0000\"",           /* null byte escape */
        "\"\\u0001\"",           /* control char */
        "\"\\u007F\"",           /* DEL char */
        "\"\\u0080\"",           /* extended ASCII */
        "\"\\u2028\"",           /* line separator */
        "\"\\u2029\"",           /* paragraph separator */
        "\"\\uFEFF\"",           /* BOM */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        json_free(doc);
        free(err);
    }
    PASS();
}

/* ─── YAML fuzz tests ────────────────────────────────────────── */
static void test_yaml_malformed(void) {
    TEST("YAML: malformed inputs (8 cases)");
    const char *inputs[] = {
        "", "key: value\n", ":", "[\n",
        "key:\n  nested: value\n", "a: 1\nb: 2\nc: 3\n",
        "|", ">\n  folded\n",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(inputs[i], &err);
        free(err);
        if (doc) yaml_free(doc);
    }
    PASS();
}

static void test_yaml_large(void) {
    TEST("YAML: large sequence (1000 items)");
    char buf[65536];
    int pos = 0;
    for (int i = 0; i < 1000 && pos < (int)sizeof(buf) - 20; i++) {
        int n = snprintf(buf + pos, sizeof(buf) - pos, "  - item %d\n", i);
        if (n > 0) pos += n;
    }
    buf[pos] = '\0';
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(buf, &err);
    free(err);
    if (doc) yaml_free(doc);
    PASS();
}

/* ─── Template fuzz tests ────────────────────────────────────── */
static void test_template_malformed(void) {
    TEST("Template: malformed inputs (6 cases)");
    const char *inputs[] = {
        "", "plain text", "{{ var }}",
        "{% if x %}", "{% for i in list %}", "{{ ",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *tmpl_err = NULL;
        template_t *tmpl = template_compile(inputs[i], &tmpl_err);
        free(tmpl_err);
        if (tmpl) {
            const char *ctx = "{}";
            char *result = template_render(tmpl, ctx);
            free(result);
            template_free(tmpl);
        }
    }
    PASS();
}

static void test_template_deep_nested(void) {
    TEST("Template: deeply nested if/for (20 levels)");
    char tmpl_buf[8192];
    int pos = 0;
    for (int i = 0; i < 20 && pos < (int)sizeof(tmpl_buf) - 30; i++) {
        int n = snprintf(tmpl_buf + pos, sizeof(tmpl_buf) - pos,
                         "{%% if x%d %%}level%d ", i, i);
        if (n > 0) pos += n;
    }
    for (int i = 0; i < 20 && pos < (int)sizeof(tmpl_buf) - 10; i++) {
        int n = snprintf(tmpl_buf + pos, sizeof(tmpl_buf) - pos, "{%% endif %%}");
        if (n > 0) pos += n;
    }
    tmpl_buf[pos] = '\0';
    char *tmpl_err = NULL;
    template_t *tmpl = template_compile(tmpl_buf, &tmpl_err);
    free(tmpl_err);
    if (tmpl) {
        const char *ctx = "{}";
        char *result = template_render(tmpl, ctx);
        free(result);
        template_free(tmpl);
    }
    PASS();
}

static void test_template_unmatched_tags(void) {
    TEST("Template: unmatched/edge tags (4 cases)");
    const char *inputs[] = {
        "{%% endif %%}",                   /* lone endif */
        "{%% if x %%}",                    /* unclosed if */
        "{{ ",                             /* unclosed var */
        "{% invalid_tag %}",               /* unknown tag */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *tmpl_err = NULL;
        template_t *tmpl = template_compile(inputs[i], &tmpl_err);
        free(tmpl_err);
        if (tmpl) {
            const char *ctx = "{}";
            char *result = template_render(tmpl, ctx);
            free(result);
            template_free(tmpl);
        }
    }
    PASS();
}

/* ─── Regex fuzz tests ───────────────────────────────────────── */
static void test_regex_malformed(void) {
    TEST("Regex: malformed patterns (6 cases)");
    const char *patterns[] = {
        "", ".*", "[invalid", "(", "\\", "***",
    };
    const char *test_str = "hello world 123";
    int n = sizeof(patterns) / sizeof(patterns[0]);
    for (int i = 0; i < n; i++) {
        char *result = regex_extract(patterns[i], test_str, 0);
        free(result);
    }
    PASS();
}

static void test_regex_large(void) {
    TEST("Regex: large input (100K chars)");
    char *buf = malloc(100100);
    if (!buf) { FAIL("malloc"); return; }
    memset(buf, 'a', 100000);
    buf[100000] = '\0';
    char *result = regex_extract("a{100}", buf, 0);
    free(result);
    free(buf);
    PASS();
}

static void test_regex_more_edge(void) {
    TEST("Regex: additional edge patterns (5 cases)");
    const char *patterns[] = {
        "[\\]",           /* escaped bracket */
        "(a|b|c|d|e)",    /* alternation */
        "a+b+c+d+e+",     /* long chain */
        "\\d{3}-\\d{2}-\\d{4}",  /* realistic pattern */
        "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$", /* email */
    };
    const char *test_str = "hello world 123 test@example.com";
    int n = sizeof(patterns) / sizeof(patterns[0]);
    for (int i = 0; i < n; i++) {
        char *result = regex_extract(patterns[i], test_str, 0);
        free(result);
    }
    PASS();
}

/* ─── HTML fuzz tests ────────────────────────────────────────── */
static void test_html_malformed(void) {
    TEST("HTML: malformed/strip inputs (14 cases)");
    const char *inputs[] = {
        "", "plain text", "<p>hello</p>",
        "<script>alert(1)</script>",
        "<div><span>nested</span></div>",
        "<unclosed>", "extra <<<< brackets >>>>",
        "&\namp;\n",
        /* New edge cases */
        "<img src=x onerror=alert(1)>",   /* event handler */
        "<script>/* nested */<script>inner</script>",  /* nested script */
        "<div style=\"x:y\">text</div>",   /* style attribute */
        "<!--[if IE]> bad <![endif]-->",   /* conditional comment */
        "hello world with \x00 byte",       /* text-like */
        "a < b > c"                         /* angle brackets in text */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *stripped = html_strip_tags(inputs[i]);
        free(stripped);
    }
    PASS();
}

static void test_html_extreme_nested(void) {
    TEST("HTML: 100-level deep nesting");
    char buf[4096];
    int pos = 0;
    for (int i = 0; i < 100 && pos < (int)sizeof(buf) - 20; i++) {
        int n = snprintf(buf + pos, sizeof(buf) - pos, "<div id=\"l%d\">", i);
        if (n > 0) pos += n;
    }
    buf[pos] = '\0';
    char *stripped = html_strip_tags(buf);
    free(stripped);
    /* Now close all tags */
    int lvl = 0;
    for (int i = 99; i >= 0 && pos < (int)sizeof(buf) - 20; i--, lvl++) {
        int n = snprintf(buf + pos, sizeof(buf) - pos, "</div>");
        if (n > 0) pos += n;
    }
    buf[pos] = '\0';
    char *stripped2 = html_strip_tags(buf);
    free(stripped2);
    PASS();
}

/* ─── Path fuzz tests ────────────────────────────────────────── */
static void test_path_malformed(void) {
    TEST("Path: traversal/extreme inputs (11 cases)");
    const char *inputs[] = {
        "", "/", "../..", "../../../etc/passwd",
        "//foo///bar//", "/../", "/./",
        /* New edge cases */
        "~", "~/config",                    /* home dir */
        "/../../../",                       /* over-traversal */
        "a/../../b/../c",                   /* complex mixed */
        "C:\\Windows\\System32",            /* Windows path */
        "//hostname/share/path",            /* UNC path */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *norm = path_normalize(inputs[i]);
        bool trav = path_has_traversal(inputs[i]);
        free(norm);
        (void)trav;
    }
    PASS();
}

static void test_path_very_long(void) {
    TEST("Path: very long path (4096 chars)");
    char buf[5000];
    int pos = 0;
    for (int i = 0; i < 200 && pos < 4000; i++) {
        int n = snprintf(buf + pos, sizeof(buf) - pos, "subdir%d/", i);
        if (n > 0) pos += n;
    }
    buf[pos] = '\0';
    char *norm = path_normalize(buf);
    free(norm);
    PASS();
}

/* ─── Cron expression fuzz tests ──────────────────────────────── */
static void test_cron_malformed(void) {
    TEST("Cron: malformed expressions (16 cases)");
    const char *inputs[] = {
        "", "* * * *", "a b c d e", "*/x * * * *",
        "60 * * * *", "* 24 * * *",
        "@invalid", "0 0 0 0 0 0",
        "*/0 * * * *",              /* zero step */
        "1-10/0 * * * *",           /* zero step in range */
        "0 0 0 0 7",                /* valid day-of-week 7 */
        "0 0 0 0 0,2,4",           /* list in day-of-week */
        "@yearly",                   /* special string */
        "0 0 1 1 0",               /* valid full spec */
        "0 0 0 0 0-5",             /* range */
        "*****",                    /* no separators */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        cron_expr_t *cexpr = cron_parse(inputs[i], &err);
        free(err);
        if (cexpr) cron_free(cexpr);
    }
    PASS();
}

/* ─── TOML malformed fuzz ──────────────────────────────────── */
static void test_toml_malformed(void) {
    TEST("TOML: malformed inputs (12 cases)");
    const char *inputs[] = {
        "",                        /* empty */
        "[table",                  /* truncated table header */
        "key = ",                  /* missing value */
        "key = [1, 2",             /* truncated array */
        "key = {a = 1, b =",      /* truncated inline table */
        "[[array",                 /* truncated array of tables */
        "a = true false",          /* two values */
        "+++invalid",              /* garbage */
        "\"key = 1",               /* quoted key not closed */
        "key = 2024-13-01",        /* invalid date (month 13) */
        "key = 1\nkey = 2",        /* duplicate key */
        "[a]\n[a]\n",              /* duplicate table */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        toml_doc_t *doc = toml_parse(inputs[i]);
        /* Just verify it doesn't crash — NULL return is OK */
        if (doc) toml_free(doc);
    }
    PASS();
}

/* ─── CSV malformed fuzz ──────────────────────────────────── */
static void test_csv_malformed(void) {
    TEST("CSV: malformed inputs (10 cases)");
    const char *inputs[] = {
        "",                          /* empty */
        "a,b,c",                     /* basic (should succeed) */
        "\"unclosed",               /* unclosed quote */
        "\"quo,ted\",value",         /* quoted field with comma */
        "a,\"quo\"te\",b",            /* nested quotes */
        "a\nb\nc",                  /* multi-line */
        "a,,c",                      /* empty field */
        ",",                         /* single empty delimiter */
        "a|\"b|c\"|d",              /* pipe delimiter + quoted pipe */
        "line1\nline2\n",            /* trailing newline */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        csv_reader_t *r = csv_reader_open_string(inputs[i], strlen(inputs[i]), ',');
        if (r) {
            int count = 0;
            char **fields;
            while ((fields = csv_reader_read_row(r, &count)) != NULL) {
                csv_free_fields(fields, count);
            }
            csv_reader_close(r);
        }
    }
    PASS();
}

/* ─── Base64 decode fuzz ──────────────────────────────────── */
static void test_base64_decode(void) {
    TEST("Base64: decode malformed (8 cases)");
    const char *inputs[] = {
        "",                          /* empty */
        "a",                         /* single char (invalid length) */
        "abc",                       /* 3 chars (invalid length) */
        "!!!",                       /* garbage characters */
        "a b",                       /* spaces */
        "QUJDRA==",                  /* valid 4-byte (should succeed) */
        "QUJD=",                    /* valid 3-byte with padding */
        "QUJD",                      /* valid without padding (3 bytes) */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        size_t out_len = 0;
        unsigned char *decoded = base64_decode(inputs[i], &out_len);
        /* NULL return for invalid is OK — just verify no crash */
        free(decoded);
    }
    PASS();
}

/* ─── Datetime ISO8601/RFC3339 parse fuzz ─────────────────── */
static void test_datetime_parse(void) {
    TEST("Datetime: ISO8601/RFC3339 malformed (10 cases)");
    const char *inputs[] = {
        "",                          /* empty */
        "not-a-date",                /* garbage */
        "2024-13-01",                /* invalid month */
        "2024-01-32",                /* invalid day */
        "2024-01-01T25:00:00",       /* invalid hour */
        "2024-01-01T00:60:00",       /* invalid minute */
        "2024-01-01T00:00:61",       /* invalid second */
        "2024-01-01",                /* date only (valid ISO8601) */
        "2024-01-01T00:00:00Z",      /* valid UTC */
        "2024-01-01T00:00:00+05:30", /* valid with tz offset */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        /* Just verify no crash on either parser */
        time_t t1 = datetime_parse_iso8601(inputs[i]);
        time_t t2 = datetime_parse_rfc3339(inputs[i]);
        (void)t1; (void)t2;
    }
    PASS();
}

/* ─── Website policy pattern match fuzz ──────────────────── */
static void test_website_patterns(void) {
    TEST("Website: host pattern match fuzz (10 cases)");
    const char *patterns[] = {"*.example.com", "example.*", "*",
        "ex?mple.com", "[abc]example.com", "example..com",
        "*.co.uk", "", "exa mple.com", "*.test.example.com"};
    const char *hosts[]   = {"sub.example.com", "example.org", "anything",
        "exmple.com", "aexample.com", "example..com",
        "sub.test.co.uk", "", "exa mple.com", "deep.test.example.com"};
    int n = sizeof(patterns) / sizeof(patterns[0]);
    for (int i = 0; i < n; i++) {
        bool result = website_match_host(hosts[i], patterns[i]);
        (void)result;
    }
    PASS();
}

/* ─── Property-style tests ───────────────────────────────────── */
static void test_property_json_roundtrip(void) {
    TEST("Property: JSON serialize/parse round-trip (10 cases)");
    const char *inputs[] = {
        "{\"key\": \"value\"}", "{\"num\": 42}",
        "{\"arr\": [1, 2, 3]}", "{\"nested\": {\"a\": 1}}",
        "{\"empty\": null}", "[\"a\", \"b\", \"c\"]",
        "42", "\"string\"", "true", "false",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        free(err);
        if (doc) {
            char *serialized = json_serialize(doc);
            if (serialized) {
                char *err2 = NULL;
                json_t *doc2 = json_parse(serialized, &err2);
                free(err2);
                if (doc2) {
                    char *s2 = json_serialize(doc2);
                    if (!s2 || strcmp(serialized, s2) != 0) {
                        FAIL("JSON round-trip mismatch");
                        printf("        original: %s\n", inputs[i]);
                    }
                    free(s2);
                    json_free(doc2);
                }
                free(serialized);
            }
            json_free(doc);
        }
    }
    PASS();
}

static void test_property_json_numeric_edge(void) {
    TEST("Property: JSON numeric edge cases (6 cases)");
    const char *inputs[] = {
        "0.0", "-0.5", "1e-10", "-1e10", "0.0000001", "9999999999999999",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        free(err);
        if (doc) {
            char *serialized = json_serialize(doc);
            if (serialized) {
                char *err2 = NULL;
                json_t *doc2 = json_parse(serialized, &err2);
                free(err2);
                if (doc2) {
                    json_free(doc2);
                }
                free(serialized);
            }
            json_free(doc);
        }
    }
    PASS();
}

static void test_property_json_recursive_array(void) {
    TEST("Property: deeply nested array round-trip (50 levels)");
    char buf[4096];
    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < 50 && pos < (int)sizeof(buf) - 5; i++) {
        int n = snprintf(buf + pos, sizeof(buf) - pos, "[");
        if (n > 0) pos += n;
    }
    buf[pos++] = '1';
    for (int i = 0; i < 50 && pos < (int)sizeof(buf) - 2; i++)
        buf[pos++] = ']';
    buf[pos] = '\0';
    char *err = NULL;
    json_t *doc = json_parse(buf, &err);
    free(err);
    if (doc) {
        char *s = json_serialize(doc);
        free(s);
        json_free(doc);
    }
    PASS();
}

/* ─── Fuzzy match fuzz ─────────────────────────────────────── */
static void test_fuzzy_match_fuzzy(void) {
    TEST("FuzzyMatch: edge inputs (10 cases)");
    /* Case 1: empty content */
    fuzzy_result_t res = fuzzy_find_and_replace("", "find", "replace", false);
    fuzzy_result_free(&res);

    /* Case 2: empty old_string */
    res = fuzzy_find_and_replace("content", "", "replace", false);
    fuzzy_result_free(&res);

    /* Case 3: content not found (returns original) */
    res = fuzzy_find_and_replace("hello world", "xyzzy", "XXXX", false);
    fuzzy_result_free(&res);

    /* Case 4: exact match */
    res = fuzzy_find_and_replace("hello world", "hello", "hi", false);
    fuzzy_result_free(&res);

    /* Case 5: whitespace difference (tabs vs spaces) */
    res = fuzzy_find_and_replace("hello\tworld", "hello world", "hi world", false);
    fuzzy_result_free(&res);

    /* Case 6: replace_all */
    res = fuzzy_find_and_replace("a b a b a", "a", "X", true);
    fuzzy_result_free(&res);

    /* Case 7: special characters (backslash, quotes) */
    res = fuzzy_find_and_replace("it's \"done\"", "it's \"done\"", "ok", false);
    fuzzy_result_free(&res);

    /* Case 8: multi-line content */
    res = fuzzy_find_and_replace("line1\nline2\nline3", "line2", "L2", false);
    fuzzy_result_free(&res);

    /* Case 9: newline in search string */
    res = fuzzy_find_and_replace("hello\nworld", "hello\nworld", "goodbye\nuniverse", false);
    fuzzy_result_free(&res);

    /* Case 10: NULL content (error path) */
    res = fuzzy_find_and_replace(NULL, "find", "replace", false);
    fuzzy_result_free(&res);
    PASS();
}

/* ─── Schema sanitizer fuzz ───────────────────────────────── */
static void test_schema_sanitizer_fuzzy(void) {
    TEST("SchemaSanitizer: malformed/edge inputs (10 cases)");
    /* Case 1: empty string */
    char *result = sanitize_tool_schemas("");
    free(result);

    /* Case 2: valid array */
    result = sanitize_tool_schemas("[{\"name\":\"test\",\"parameters\":{}}]");
    free(result);

    /* Case 3: bare-string type */
    result = sanitize_tool_schemas("[{\"name\":\"t\",\"parameters\":{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"string\"}}}}]");
    free(result);

    /* Case 4: array type (string+null union) */
    result = sanitize_tool_schemas("[{\"name\":\"t\",\"parameters\":{\"type\":[\"string\",\"null\"],\"properties\":{}}}]");
    free(result);

    /* Case 5: NULL input */
    result = sanitize_tool_schemas(NULL);
    free(result);

    /* Case 6: missing properties */
    result = sanitize_tool_schemas("[{\"name\":\"t\",\"parameters\":{\"type\":\"object\"}}]");
    free(result);

    /* Case 7: pattern/format strip */
    int count = 0;
    result = strip_pattern_and_format("[{\"name\":\"t\",\"parameters\":{\"pattern\":\".*\"}}]", &count);
    free(result);

    /* Case 8: enum with slash */
    count = 0;
    result = strip_slash_enum("[{\"name\":\"t\",\"parameters\":{\"properties\":{\"x\":{\"type\":\"string\",\"enum\":[\"a/b\",\"c/d\"]}}}}]", &count);
    free(result);

    /* Case 9: deeply nested schema */
    result = sanitize_tool_schemas("[{\"name\":\"t\",\"parameters\":{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"object\",\"properties\":{\"b\":{\"type\":\"object\",\"properties\":{\"c\":{\"type\":\"string\"}}}}}}}}]");
    free(result);

    /* Case 10: empty tools array */
    result = sanitize_tool_schemas("[]");
    free(result);
    PASS();
}

/* ─── Glob pattern match fuzz ───────────────────────────────── */
static void test_glob_pattern_fuzzy(void) {
    TEST("Glob: pattern matching edge cases (10 cases)");
    /* Case 1: empty pattern */
    bool m = glob_match("", "file.c");
    (void)m;

    /* Case 2: empty path */
    m = glob_match("*.c", "");
    (void)m;

    /* Case 3: both empty */
    m = glob_match("", "");
    (void)m;

    /* Case 4: only special chars */
    m = glob_match("***", "anything");
    (void)m;

    /* Case 5: character class with special chars */
    m = glob_match("[!]*?[", "x");
    (void)m;

    /* Case 6: many ? wildcards */
    m = glob_match("????????????????????????????????", "abcdefghijklmnopqrstuvwxyz");
    (void)m;

    /* Case 7: mixed ** with path */
    m = glob_match("a/**/b/**/c", "a/x/y/b/z/c");
    (void)m;

    /* Case 8: long alternating pattern */
    m = glob_match("a*b*c*d*e*f*g*h*i*j*k*l*m*n*o*p", "axbxcxdxexfxgxhxixjxkxlxmxnxoxp");
    (void)m;

    /* Case 9: [] with negation and range */
    m = glob_match("[!a-z]file", "Afile");
    (void)m;

    /* Case 10: trailing ** */
    m = glob_match("prefix/**", "prefix/deep/nested/path");
    (void)m;
    PASS();
}

/* ─── Hash function fuzz ───────────────────────────────────── */
static void test_hash_fuzzy(void) {
    TEST("Hash: edge-case inputs (8 cases)");
    /* Case 1: empty data SHA-256 */
    unsigned char *h = hash_sha256((const unsigned char*)"", 0);
    free(h);

    /* Case 2: large data SHA-256 (64KB) */
    char *big = malloc(65536);
    if (big) {
        memset(big, 'A', 65536);
        h = hash_sha256((const unsigned char*)big, 65536);
        free(h);
        free(big);
    }

    /* Case 3: binary data with null bytes SHA-256 */
    unsigned char bin[] = {'\x00', '\x01', '\xFF', '\xAB', '\x00', '\x7F'};
    h = hash_sha256(bin, sizeof(bin));
    free(h);

    /* Case 4: empty data SHA-1 */
    h = hash_sha1((const unsigned char*)"", 0);
    free(h);

    /* Case 5: empty data MD5 */
    h = hash_md5((const unsigned char*)"", 0);
    free(h);

    /* Case 6: HMAC-SHA256 with empty key, empty data */
    h = hash_hmac_sha256((const unsigned char*)"", 0, (const unsigned char*)"", 0);
    free(h);

    /* Case 7: HMAC-SHA256 with data only */
    h = hash_hmac_sha256((const unsigned char*)"key", 3, (const unsigned char*)"", 0);
    free(h);

    /* Case 8: hex conversion round-trip */
    char *hex = hash_sha256_hex((const unsigned char*)"test", 4);
    if (hex) {
        size_t out_len = 0;
        unsigned char *bin2 = hash_hex_to_bytes(hex, &out_len);
        free(bin2);
        free(hex);
    }
    PASS();
}

/* ─── Difflib fuzz ─────────────────────────────────────────── */
static void test_difflib_fuzzy(void) {
    TEST("Difflib: edge-case inputs (8 cases)");
    /* Case 1: empty strings */
    double r = difflib_ratio("", "");
    (void)r;

    /* Case 2: one empty, one non-empty */
    r = difflib_ratio("hello", "");
    (void)r;
    r = difflib_ratio("", "world");
    (void)r;

    /* Case 3: identical strings */
    r = difflib_ratio("the quick brown fox", "the quick brown fox");
    (void)r;

    /* Case 4: completely different */
    r = difflib_ratio("abc", "xyz");
    (void)r;

    /* Case 5: unified diff empty inputs */
    char *diff = difflib_unified_diff("", "", 3);
    free(diff);

    /* Case 6: unified diff one-line change */
    diff = difflib_unified_diff("hello\nworld\n", "hello\nuniverse\n", 3);
    free(diff);

    /* Case 7: simple diff empty inputs */
    diff = difflib_simple_diff("", "");
    free(diff);

    /* Case 8: simple diff with content */
    diff = difflib_simple_diff("line1\nline2\nline3", "line1\nmodified\nline3");
    free(diff);
    PASS();
}

static void test_uuid_fuzzy(void) {
    TEST("UUID: fuzz malformed inputs (10 cases)");

    /* Case 1: empty string */
    bool ok = uuid_is_valid("");
    if (ok) { FAIL("empty string should not be valid"); } else { /* expected */ }

    /* Case 2: null pointer for v4 */
    /* Shouldn't crash — uuid_v4 returns NULL on failure */
    char *u = uuid_v4();
    if (!u) { FAIL("uuid_v4 returned NULL"); }
    free(u);

    /* Case 3: truncated UUID (too short) */
    ok = uuid_is_valid("550e8400-e29b-41d4-a716-44665544000");  /* 35 chars */
    if (ok) { FAIL("truncated UUID should not be valid"); } else { /* expected */ }

    /* Case 4: UUID with spaces */
    ok = uuid_is_valid("550e8400 e29b 41d4 a716 446655440000");
    if (ok) { FAIL("spaces should be invalid"); } else { /* expected */ }

    /* Case 5: UUID with wrong separators */
    ok = uuid_is_valid("550e8400:e29b:41d4:a716:446655440000");
    if (ok) { FAIL("colon separators should be invalid"); } else { /* expected */ }

    /* Case 6: non-hex characters */
    ok = uuid_is_valid("zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz");
    if (ok) { FAIL("non-hex chars should be invalid"); } else { /* expected */ }

    /* Case 7: very long string */
    char longbuf[128];
    memset(longbuf, 'a', 127);
    longbuf[127] = '\0';
    ok = uuid_is_valid(longbuf);
    if (ok) { FAIL("long string should not be valid"); } else { /* expected */ }

    /* Case 8: v5 with empty name */
    u = uuid_v5((const uint8_t *)UUID_NS_DNS, "", 0);
    if (!u) { FAIL("uuid_v5 empty name returned NULL"); }
    free(u);

    /* Case 9: parse malformed UUID */
    uint8_t bytes[UUID_LEN];
    ok = uuid_parse("not-a-uuid-at-all!", bytes);
    if (ok) { FAIL("parse malformed should return false"); } else { /* expected */ }

    /* Case 10: unparse all-zeros (valid, should not crash) */
    uint8_t zero_bytes[UUID_LEN] = {0};
    u = uuid_unparse(zero_bytes);
    if (!u) { FAIL("uuid_unparse NULL"); }
    ok = uuid_is_valid(u);
    if (!ok) { FAIL("uuid_unparse result should be valid"); }
    free(u);

    PASS();
}

static void test_textwrap_fuzzy(void) {
    TEST("Textwrap: fuzz edge-case inputs (10 cases)");

    /* Case 1: NULL text — should not crash */
    textwrap_result_t r = textwrap_wrap(NULL, 80);
    (void)r;

    /* Case 2: empty string */
    r = textwrap_wrap("", 80);
    (void)r;

    /* Case 3: single character, width=1 */
    r = textwrap_wrap("X", 1);
    (void)r;

    /* Case 4: very long single word (no breakpoints) */
    char longword[512];
    memset(longword, 'A', 511);
    longword[511] = '\0';
    r = textwrap_wrap(longword, 40);
    (void)r;

    /* Case 5: width=0 (edge for division / clamping) */
    r = textwrap_wrap("hello world", 0);
    (void)r;

    /* Case 6: width=1 with multi-word text */
    r = textwrap_wrap("ab cd ef", 1);
    (void)r;

    /* Case 7: fill with empty string */
    char *filled = textwrap_fill("", 80);
    free(filled);

    /* Case 8: dedent with no indentation */
    filled = textwrap_dedent("hello\nworld");
    free(filled);

    /* Case 9: shorten with text already under limit */
    filled = textwrap_shorten("short", 80);
    free(filled);

    /* Case 10: chunk with empty text */
    size_t count;
    char **chunks = textwrap_chunk("", 80, &count);
    if (chunks) {
        for (size_t i = 0; i < count; i++) free(chunks[i]);
        free(chunks);
    }

    PASS();
}

/* ─── Dotenv parse fuzz ──────────────────────────────────── */
static void test_dotenv_fuzzy(void) {
    TEST("Dotenv: malformed/edge inputs (10 cases)");

    /* Case 1: empty string */
    char *err = NULL;
    env_t *env = dotenv_parse("", &err);
    free(err);
    if (env) dotenv_free(env);

    /* Case 2: comment lines only */
    err = NULL;
    env = dotenv_parse("# comment\n; another\n# third", &err);
    free(err);
    if (env) dotenv_free(env);

    /* Case 3: key without = separator */
    err = NULL;
    env = dotenv_parse("KEYWITHOUTVALUE", &err);
    free(err);
    if (env) dotenv_free(env);

    /* Case 4: key=value basic */
    err = NULL;
    env = dotenv_parse("KEY=VALUE", &err);
    free(err);
    if (env) dotenv_free(env);

    /* Case 5: quoted value */
    err = NULL;
    env = dotenv_parse("KEY=\"quoted value with spaces\"", &err);
    free(err);
    if (env) dotenv_free(env);

    /* Case 6: empty value after = */
    err = NULL;
    env = dotenv_parse("EMPTY=", &err);
    free(err);
    if (env) dotenv_free(env);

    /* Case 7: special characters in value */
    err = NULL;
    env = dotenv_parse("PATH=/usr/bin:/usr/local/bin\nTOKEN=abc123!@#$%^&*()", &err);
    free(err);
    if (env) dotenv_free(env);

    /* Case 8: leading/trailing whitespace */
    err = NULL;
    env = dotenv_parse("  SPACED_KEY=value  ", &err);
    free(err);
    if (env) dotenv_free(env);

    /* Case 9: very long single line */
    err = NULL;
    char longval[4096];
    memset(longval, 'x', sizeof(longval) - 1);
    longval[sizeof(longval) - 1] = '\0';
    char *longline = malloc(4200);
    if (longline) {
        int n = snprintf(longline, 4200, "LONG_KEY=%s", longval);
        if (n > 0) {
            env = dotenv_parse(longline, &err);
            free(err);
            if (env) dotenv_free(env);
        }
        free(longline);
    }

    /* Case 10: multiple lines with blanks */
    err = NULL;
    env = dotenv_parse("\n\nKEY1=a\n\nKEY2=b\n\n\nKEY3=c\n", &err);
    free(err);
    if (env) dotenv_free(env);

    PASS();
}

/* ─── Binary extension check fuzz ─────────────────────────── */
static void test_binary_fuzzy(void) {
    TEST("Binary: has_binary_extension edge cases (10 cases)");

    /* Case 1: NULL path */
    bool r = has_binary_extension(NULL);
    (void)r;

    /* Case 2: empty path */
    r = has_binary_extension("");
    (void)r;

    /* Case 3: no extension at all */
    r = has_binary_extension("file");
    (void)r;

    /* Case 4: known binary extension */
    r = has_binary_extension("program.exe");
    (void)r;

    /* Case 5: shared library extension */
    r = has_binary_extension("libfoo.so");
    (void)r;

    /* Case 6: mixed case binary extension */
    r = has_binary_extension("setup.EXE");
    (void)r;

    /* Case 7: dotted file (no actual extension) */
    r = has_binary_extension(".hidden");
    (void)r;

    /* Case 8: path with directory prefix */
    r = has_binary_extension("/usr/local/bin/tool.exe");
    (void)r;

    /* Case 9: text file extension */
    r = has_binary_extension("readme.txt");
    (void)r;

    /* Case 10: very long path */
    char longpath[4096];
    memset(longpath, 'a', 1000);
    longpath[0] = '/';
    memcpy(longpath + 900, "/tool.exe", 10);
    longpath[910] = '\0';
    r = has_binary_extension(longpath);
    (void)r;

    PASS();
}

/* ─── Interrupt state management fuzz ─────────────────────── */
static void test_interrupt_fuzzy(void) {
    TEST("Interrupt: state management edge cases (10 cases)");

    /* Case 1: initial state — no interrupts */
    bool r = interrupt_is_interrupted();
    int c = interrupt_count();
    (void)r; (void)c;

    /* Case 2: set interrupt for current thread */
    interrupt_set(true, 0);
    r = interrupt_is_interrupted();
    c = interrupt_count();
    (void)r; (void)c;

    /* Case 3: clear interrupt */
    interrupt_set(false, 0);
    r = interrupt_is_interrupted();
    c = interrupt_count();
    (void)r; (void)c;

    /* Case 4: double set (should be idempotent) */
    interrupt_set(true, 0);
    interrupt_set(true, 0);
    c = interrupt_count();
    (void)c;
    interrupt_set(false, 0);

    /* Case 5: clear when none set (no-op) */
    interrupt_set(false, 0);
    c = interrupt_count();
    (void)c;

    /* Case 6: set with explicit thread ID */
    interrupt_set(true, 42);
    c = interrupt_count();
    (void)c;

    /* Case 7: multiple threads */
    interrupt_set(true, 100);
    interrupt_set(true, 200);
    c = interrupt_count();
    (void)c;

    /* Case 8: clear one specific thread */
    interrupt_set(false, 100);
    c = interrupt_count();
    (void)c;

    /* Case 9: clear all */
    interrupt_clear_all();
    c = interrupt_count();
    (void)c;

    /* Case 10: set after clear all */
    interrupt_set(true, 0);
    c = interrupt_count();
    (void)c;
    interrupt_clear_all();

    PASS();
}

/* ─── ANSI strip fuzz ─────────────────────────────────────── */
static void test_ansi_fuzzy(void) {
    TEST("ANSI: strip edge cases (10 cases)");

    /* Case 1: NULL input */
    char *result = ansi_strip(NULL);
    if (result != NULL) { FAIL("expected NULL for NULL input"); goto case2; }
    PASS(); goto next1;
case2:
    /* Case 2: empty string */
    result = ansi_strip("");
    if (!result || strcmp(result, "") != 0) { FAIL("expected empty string"); free(result); goto case3; }
    free(result); PASS(); goto next2;
case3:
    /* Case 3: plain text (no ANSI) */
    result = ansi_strip("hello world");
    if (!result || strcmp(result, "hello world") != 0) { FAIL("plain text pass-through failed"); free(result); goto case4; }
    free(result); PASS(); goto next3;
case4:
    /* Case 4: simple color code */
    result = ansi_strip("\033[31mred\033[0m");
    if (!result || strcmp(result, "red") != 0) { FAIL("color strip failed"); free(result); goto case5; }
    free(result); PASS(); goto next4;
case5:
    /* Case 5: multiple mixed colors and styles */
    result = ansi_strip("\033[1m\033[32mbold green\033[0m and \033[4munderline\033[0m");
    if (!result || strcmp(result, "bold green and underline") != 0) { FAIL("mixed strip failed"); free(result); goto case6; }
    free(result); PASS(); goto next5;
case6:
    /* Case 6: OSC title sequence (BEL-terminated) */
    result = ansi_strip("\033]0;MyTitle\007content");
    if (!result || strcmp(result, "content") != 0) { FAIL("OSC BEL strip failed"); free(result); goto case7; }
    free(result); PASS(); goto next6;
case7:
    /* Case 7: C1 8-bit CSI (0x9B) */
    result = ansi_strip("\x9b" "31m8bit_red\x9b" "0m");
    if (!result || strcmp(result, "8bit_red") != 0) { FAIL("8-bit CSI strip failed"); free(result); goto case8; }
    free(result); PASS(); goto next7;
case8:
    /* Case 8: long sequence with many params */
    result = ansi_strip("\033[38;2;255;128;0;48;2;0;0;0;1;4mstyled\033[0m");
    if (!result || strcmp(result, "styled") != 0) { FAIL("long param strip failed"); free(result); goto case9; }
    free(result); PASS(); goto next8;
case9:
    /* Case 9: truncated/incomplete sequence (no terminator) */
    result = ansi_strip("\033[31mtruncated");
    if (!result || strcmp(result, "truncated") != 0) { FAIL("truncated sequence strip failed"); free(result); goto case10; }
    free(result); PASS(); goto next9;
case10:
    /* Case 10: ansi_has_escape detection */
    {
        bool has1 = ansi_has_escape("plain");
        bool has2 = ansi_has_escape("\033[31mred\033[0m");
        if (has1) { FAIL("plain text should not have escapes"); goto end; }
        if (!has2) { FAIL("colored text should have escapes"); goto end; }
        PASS();
    }
    goto end;
next1: next2: next3: next4: next5: next6: next7: next8: next9:
end:
    ;
}

/* ─── Budget config fuzz ──────────────────────────────────── */
static void test_budget_config_fuzzy(void) {
    TEST("Budget: config edge cases (10 cases)");

    /* Case 1: NULL config to init (no crash) */
    budget_config_init(NULL);
    PASS(); goto next1;
case2:
    /* Case 2: default config has correct values */
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        if (cfg.default_result_size == BUDGET_DEFAULT_RESULT_SIZE_CHARS &&
            cfg.turn_budget == BUDGET_DEFAULT_TURN_BUDGET_CHARS &&
            cfg.preview_size == BUDGET_DEFAULT_PREVIEW_SIZE_CHARS &&
            cfg.overrides == NULL) { PASS(); }
        else { FAIL("default values mismatch"); goto case3; }
    }
    goto next2;
case3:
    /* Case 3: resolve threshold with NULL config */
    {
        int t = budget_config_resolve_threshold(NULL, "read_file");
        if (t == BUDGET_DEFAULT_RESULT_SIZE_CHARS) { PASS(); }
        else { FAIL("unexpected threshold"); goto case4; }
    }
    goto next3;
case4:
    /* Case 4: resolve threshold with NULL tool name */
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        int t = budget_config_resolve_threshold(&cfg, NULL);
        if (t == BUDGET_DEFAULT_RESULT_SIZE_CHARS) { PASS(); }
        else { FAIL("unexpected threshold"); goto case5; }
    }
    goto next4;
case5:
    /* Case 5: resolve threshold for empty tool name */
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        int t = budget_config_resolve_threshold(&cfg, "");
        if (t == BUDGET_DEFAULT_RESULT_SIZE_CHARS) { PASS(); }
        else { FAIL("unexpected threshold"); goto case6; }
    }
    goto next5;
case6:
    /* Case 6: set override with NULL tool name (no crash) */
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        budget_config_set_override(&cfg, NULL, 5000);
        PASS(); goto case7;
    }
    goto next6;
case7:
    /* Case 7: set override then resolve */
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        budget_config_set_override(&cfg, "my_tool", 5000);
        int t = budget_config_resolve_threshold(&cfg, "my_tool");
        if (t == 5000) { PASS(); }
        else { FAIL("override not applied"); goto case8; }
    }
    goto next7;
case8:
    /* Case 8: multiple overrides, resolve non-overridden tool uses default */
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        budget_config_set_override(&cfg, "tool_a", 1000);
        budget_config_set_override(&cfg, "tool_b", 2000);
        int ta = budget_config_resolve_threshold(&cfg, "tool_a");
        int tb = budget_config_resolve_threshold(&cfg, "tool_b");
        int tc = budget_config_resolve_threshold(&cfg, "tool_c");
        if (ta == 1000 && tb == 2000 && tc == BUDGET_DEFAULT_RESULT_SIZE_CHARS) { PASS(); }
        else { FAIL("multiple overrides"); goto case9; }
    }
    goto next8;
case9:
    /* Case 9: set infinite (-1) override */
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        budget_config_set_override(&cfg, "infinite_tool", -1);
        int t = budget_config_resolve_threshold(&cfg, "infinite_tool");
        if (t == -1) { PASS(); }
        else { FAIL("infinite not -1"); goto case10; }
    }
    goto next9;
case10:
    /* Case 10: cleanup with NULL (no crash) */
    budget_config_cleanup(NULL);
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        budget_config_set_override(&cfg, "tool1", 100);
        budget_config_set_override(&cfg, "tool2", 200);
        budget_config_cleanup(&cfg);
        if (cfg.overrides == NULL) { PASS(); }
        else { FAIL("cleanup didn't null overrides"); }
    }
    goto end;
next1: next2: next3: next4: next5: next6: next7: next8: next9:
end:
    ;
}

/* ─── Env Passthrough ─────────────────────────────────────── */
static void test_env_passthrough_fuzzy(void) {
    TEST("Env Passthrough: crash-free edge cases (20 cases)");
    {
        /* Case 1: is_blocked(NULL) – no crash */
        bool r = env_passthrough_is_blocked(NULL);
        if (!r) { PASS(); } else { FAIL("is_blocked(NULL) returned true"); }
    }
    {
        /* Case 2: is_blocked("") – no crash */
        bool r = env_passthrough_is_blocked("");
        if (!r) { PASS(); } else { FAIL("is_blocked('') returned true"); }
    }
    {
        /* Case 3: is_blocked on known blocked var */
        bool r = env_passthrough_is_blocked("OPENAI_API_KEY");
        if (r) { PASS(); } else { FAIL("OPENAI_API_KEY not blocked"); }
    }
    {
        /* Case 4: is_blocked on normal var */
        bool r = env_passthrough_is_blocked("PATH");
        if (!r) { PASS(); } else { FAIL("PATH incorrectly blocked"); }
    }
    {
        /* Case 5: register(NULL) – no crash, returns false */
        bool r = env_passthrough_register(NULL);
        if (!r) { PASS(); } else { FAIL("register(NULL) returned true"); }
    }
    {
        /* Case 6: register("") – returns false */
        bool r = env_passthrough_register("");
        if (!r) { PASS(); } else { FAIL("register('') returned true"); }
    }
    {
        /* Case 7: register blocked var – returns false */
        bool r = env_passthrough_register("ANTHROPIC_API_KEY");
        if (!r) { PASS(); } else { FAIL("register blocked var returned true"); }
    }
    {
        /* Case 8: register normal var – returns true */
        bool r = env_passthrough_register("MY_CUSTOM_VAR");
        if (r) { PASS(); } else { FAIL("register normal var returned false"); }
    }
    {
        /* Case 9: is_allowed(NULL) – no crash */
        bool r = env_passthrough_is_allowed(NULL);
        if (!r) { PASS(); } else { FAIL("is_allowed(NULL) returned true"); }
    }
    {
        /* Case 10: is_allowed on registered var */
        bool r = env_passthrough_is_allowed("MY_CUSTOM_VAR");
        if (r) { PASS(); } else { FAIL("MY_CUSTOM_VAR not allowed after register"); }
    }
    {
        /* Case 11: is_allowed on unregistered var */
        bool r = env_passthrough_is_allowed("NONEXISTENT_VAR");
        if (!r) { PASS(); } else { FAIL("nonexistent var incorrectly allowed"); }
    }
    {
        /* Case 12: get_all with legit pointers, then free */
        char **list = NULL;
        int cnt = 0;
        env_passthrough_get_all(&list, &cnt);
        env_passthrough_free_list(list, cnt);
        PASS();
    }
    {
        /* Case 13: get_all with NULL, NULL – no crash */
        env_passthrough_get_all(NULL, NULL);
        PASS();
    }
    {
        /* Case 14: free_list(NULL, 0) – no crash */
        env_passthrough_free_list(NULL, 0);
        PASS();
    }
    {
        /* Case 15: register_batch with NULL – no crash */
        int n = env_passthrough_register_batch(NULL, 0);
        if (n == 0) { PASS(); } else { FAIL("register_batch(NULL,0) != 0"); }
    }
    {
        /* Case 16: clear then get_all returns empty */
        env_passthrough_clear();
        char **list = NULL;
        int cnt = 0;
        env_passthrough_get_all(&list, &cnt);
        bool ok = (cnt == 0 && list == NULL);
        env_passthrough_free_list(list, cnt);
        if (ok) { PASS(); } else { FAIL("clear + get_all not empty"); }
    }
    {
        /* Case 17: re-register same var (idempotent) */
        env_passthrough_clear();
        bool r1 = env_passthrough_register("FOO");
        bool r2 = env_passthrough_register("FOO");
        if (r1 && r2) { PASS(); } else { FAIL("re-register FOO failed"); }
    }
    {
        /* Case 18: register then clear, is_allowed false */
        env_passthrough_clear();
        env_passthrough_register("TEST_VAR");
        env_passthrough_clear();
        bool r = env_passthrough_is_allowed("TEST_VAR");
        if (!r) { PASS(); } else { FAIL("TEST_VAR still allowed after clear"); }
    }
    {
        /* Case 19: very long env var name (no crash) */
        char long_name[200];
        memset(long_name, 'X', 199);
        long_name[199] = '\0';
        bool r = env_passthrough_register(long_name);
        (void)r;
        PASS();
    }
    {
        /* Case 20: register batch with mixed blocked/allowed */
        env_passthrough_clear();
        const char *vars[] = {"MY_VAR", "OPENAI_API_KEY", "OTHER_VAR"};
        int n = env_passthrough_register_batch(vars, 3);
        if (n == 2) { PASS(); } else { FAIL("register_batch returned wrong count"); }
    }
}

/* ─── Threat Patterns ──────────────────────────────────── */
static void test_threat_patterns_fuzzy(void) {
    int init_count = threat_patterns_init();
    TEST("Threat Patterns: crash-free edge cases (12 cases)");
    {
        /* Case 1: check with NULL content */
        threat_match_t m;
        bool r = threat_patterns_check(NULL, (1 << THREAT_SCOPE_ALL), &m);
        if (!r) { PASS(); } else { FAIL("check(NULL) returned true"); }
    }
    {
        /* Case 2: check with empty string */
        threat_match_t m;
        bool r = threat_patterns_check("", (1 << THREAT_SCOPE_ALL), &m);
        if (!r) { PASS(); } else { FAIL("check('') returned true"); }
    }
    {
        /* Case 3: check with NULL match_out */
        bool r = threat_patterns_check("hello", (1 << THREAT_SCOPE_ALL), NULL);
        if (!r) { PASS(); } else { FAIL("check(..., NULL) returned true"); }
    }
    {
        /* Case 4: check with scope_mask=0 (no scopes) */
        threat_match_t m;
        bool r = threat_patterns_check("hello", 0, &m);
        if (!r) { PASS(); } else { FAIL("check(scope=0) returned true"); }
    }
    {
        /* Case 5: check_all with NULL content */
        threat_match_t m;
        bool r = threat_patterns_check_all(NULL, &m);
        if (!r) { PASS(); } else { FAIL("check_all(NULL) returned true"); }
    }
    {
        /* Case 6: check_all with empty string */
        threat_match_t m;
        bool r = threat_patterns_check_all("", &m);
        if (!r) { PASS(); } else { FAIL("check_all('') returned true"); }
    }
    {
        /* Case 7: check_all with NULL match_out */
        bool r = threat_patterns_check_all("hello", NULL);
        if (!r) { PASS(); } else { FAIL("check_all(..., NULL) returned true"); }
    }
    {
        /* Case 8: check matches a pattern in STRICT scope (authorized_keys) */
        threat_match_t m;
        bool r = threat_patterns_check("authorized_keys",
                                        (1 << THREAT_SCOPE_STRICT), &m);
        if (r && m.matched) { PASS(); } else { FAIL("authorized_keys not detected"); }
    }
    {
        /* Case 9: check matches exfil-read in ALL scope (cat .env file) */
        threat_match_t m;
        bool r = threat_patterns_check("cat ~/.env",
                                        (1 << THREAT_SCOPE_ALL), &m);
        if (r && m.matched) { PASS(); } else { FAIL("exfiltration not detected"); }
    }
    {
        /* Case 10: count returns > 0 */
        int c = threat_patterns_count();
        if (c > 0) { PASS(); } else { FAIL("pattern count is 0"); }
    }
    {
        /* Case 11: init is idempotent */
        int c2 = threat_patterns_init();
        if (c2 == init_count) { PASS(); } else { FAIL("init not idempotent"); }
    }
    {
        /* Case 12: cleanup + re-init works */
        threat_patterns_cleanup();
        int c3 = threat_patterns_init();
        if (c3 > 0) { PASS(); } else { FAIL("re-init after cleanup returned 0"); }
    }
}

/* ─── Rate Limit ──────────────────────────────────────── */
static void test_rate_limit_fuzzy(void) {
    TEST("Rate Limit: crash-free edge cases (12 cases)");
    {
        /* Case 1: init on zeroed struct */
        rate_limit_bucket_t b;
        memset(&b, 0, sizeof(b));
        rate_limit_bucket_init(&b);
        if (b.limit == 0 && b.remaining == 0) { PASS(); } else { FAIL("init didn't zero"); }
    }
    {
        /* Case 2: used on zeroed bucket (limit=0) */
        rate_limit_bucket_t b = {0, 0, 0, 0};
        int u = rate_limit_bucket_used(&b);
        if (u == 0) { PASS(); } else { FAIL("used on zero bucket != 0"); }
    }
    {
        /* Case 3: usage_pct on zeroed bucket */
        double p = rate_limit_bucket_usage_pct(& (rate_limit_bucket_t){0,0,0,0});
        if (p == 0.0) { PASS(); } else { FAIL("usage_pct on zero != 0"); }
    }
    {
        /* Case 4: remaining_seconds on zeroed bucket */
        double r = rate_limit_bucket_remaining_seconds(& (rate_limit_bucket_t){0,0,0,0});
        if (r == 0.0) { PASS(); } else { FAIL("remaining on zero != 0"); }
    }
    {
        /* Case 5: state_init with NULL provider */
        rate_limit_state_t s;
        rate_limit_state_init(&s, NULL);
        if (s.provider[0] == '\0') { PASS(); } else { FAIL("NULL provider left non-empty"); }
    }
    {
        /* Case 6: state_init with empty provider */
        rate_limit_state_t s;
        rate_limit_state_init(&s, "");
        if (s.provider[0] == '\0') { PASS(); } else { FAIL("empty provider left non-empty"); }
    }
    {
        /* Case 7: has_data on uninitialized state */
        rate_limit_state_t s = {0};
        bool h = rate_limit_state_has_data(&s);
        if (!h) { PASS(); } else { FAIL("uninitialized has_data returned true"); }
    }
    {
        /* Case 8: age_seconds on uninitialized state (returns INFINITY) */
        rate_limit_state_t s = {0};
        double a = rate_limit_state_age_seconds(&s);
        if (a > 1e9) { PASS(); } else { FAIL("age on uninit should be large"); }
    }
    {
        /* Case 9: parse_headers with NULL arrays */
        rate_limit_state_t s;
        rate_limit_state_init(&s, "test");
        bool r = rate_limit_parse_headers(&s, NULL, NULL, 0, "test");
        if (!r) { PASS(); } else { FAIL("parse_headers(NULL) returned true"); }
    }
    {
        /* Case 10: fmt_count with edge values */
        char buf[32];
        rate_limit_fmt_count(buf, sizeof(buf), 0);
        int ok = (strcmp(buf, "0") == 0);
        rate_limit_fmt_count(buf, sizeof(buf), -1);
        ok = ok && (strcmp(buf, "-1") == 0);
        rate_limit_fmt_count(buf, sizeof(buf), 999999999);
        if (ok) { PASS(); } else { FAIL("fmt_count edge values wrong"); }
    }
    {
        /* Case 11: fmt_seconds with edge values */
        char buf[64];
        rate_limit_fmt_seconds(buf, sizeof(buf), 0.0);
        int ok = (strcmp(buf, "0s") == 0);
        rate_limit_fmt_seconds(buf, sizeof(buf), -5.0);
        ok = ok && (buf[0] != '\0'); /* doesn't crash, any output */
        rate_limit_fmt_seconds(buf, sizeof(buf), 999999.0);
        if (ok) { PASS(); } else { FAIL("fmt_seconds edge values"); }
    }
    {
        /* Case 12: bucket_line with full normal bucket + formatting */
        char buf[128];
        rate_limit_bucket_t b = {100, 75, 30.0, 100.0};
        int n = rate_limit_bucket_line(buf, sizeof(buf), "Requests", &b, 10);
        if (n > 0 && buf[0] != '\0') { PASS(); } else { FAIL("bucket_line returned 0"); }
    }
}

/* ─── Slash Confirm ───────────────────────────────────── */
/* Handler for slashconfirm tests */
static char *test_sc_handler(const char *choice) {
    (void)choice;
    return strdup("confirmed");
}

static void test_slash_confirm_fuzzy(void) {
    slashconfirm_clear_all();
    TEST("Slash Confirm: crash-free edge cases (12 cases)");
    {
        /* Case 1: register with NULL session_key — no crash */
        slashconfirm_register(NULL, "cid", "cmd", test_sc_handler);
        /* No way to verify beyond no crash */
        PASS();
    }
    {
        /* Case 2: register with NULL confirm_id — no crash */
        slashconfirm_register("s", NULL, "cmd", test_sc_handler);
        PASS();
    }
    {
        /* Case 3: register with NULL command — no crash */
        slashconfirm_register("s", "cid", NULL, test_sc_handler);
        PASS();
    }
    {
        /* Case 4: get_pending with NULL session_key */
        slashconfirm_entry_t *e = slashconfirm_get_pending(NULL);
        if (e == NULL) { PASS(); } else { FAIL("get_pending(NULL) not NULL"); slashconfirm_free_entry(e); }
    }
    {
        /* Case 5: get_pending on empty table (key doesn't exist) */
        slashconfirm_clear_all();
        slashconfirm_entry_t *e = slashconfirm_get_pending("nonexistent");
        if (e == NULL) { PASS(); } else { FAIL("get_pending(nonexistent) not NULL"); slashconfirm_free_entry(e); }
    }
    {
        /* Case 6: clear with NULL session_key */
        slashconfirm_clear(NULL);
        PASS();
    }
    {
        /* Case 7: clear_if_stale on nonexistent key */
        bool r = slashconfirm_clear_if_stale("nonexistent", 1.0);
        if (!r) { PASS(); } else { FAIL("clear_if_stale on nonexistent returned true"); }
    }
    {
        /* Case 8: resolve with NULL confirm_id */
        slashconfirm_register("rsess", "rid", "cmd", test_sc_handler);
        char *result = slashconfirm_resolve("rsess", NULL, "once", 999.0);
        if (result == NULL) { PASS(); } else { FAIL("resolve(NULL confirm_id) not NULL"); free(result); }
        slashconfirm_clear("rsess");
    }
    {
        /* Case 9: resolve with NULL choice */
        slashconfirm_register("rsess2", "rid2", "cmd", test_sc_handler);
        char *result = slashconfirm_resolve("rsess2", "rid2", NULL, 999.0);
        if (result == NULL) { PASS(); } else { FAIL("resolve(NULL choice) not NULL"); free(result); }
        slashconfirm_clear("rsess2");
    }
    {
        /* Case 10: resolve with wrong confirm_id leaves entry intact */
        slashconfirm_register("rsess3", "real_id", "cmd", test_sc_handler);
        char *result = slashconfirm_resolve("rsess3", "wrong_id", "once", 999.0);
        bool ok1 = (result == NULL);
        free(result);
        slashconfirm_entry_t *e = slashconfirm_get_pending("rsess3");
        bool ok2 = (e != NULL);
        slashconfirm_free_entry(e);
        slashconfirm_clear("rsess3");
        if (ok1 && ok2) { PASS(); } else { FAIL("wrong confirm_id behavior"); }
    }
    {
        /* Case 11: resolve with stale timeout clears entry */
        slashconfirm_register("rsess4", "rid4", "cmd", test_sc_handler);
        char *result = slashconfirm_resolve("rsess4", "rid4", "once", -1.0);
        /* Stale timeout: entry cleared, result is NULL */
        if (result == NULL) { PASS(); } else { FAIL("stale resolve returned non-NULL"); free(result); }
    }
    {
        /* Case 12: clear_all on empty table */
        slashconfirm_clear_all();
        PASS();
    }
}

static volatile int g_signal_fuzz_flag = 0;

static void fuzz_signal_handler(int signum) {
    (void)signum;
    g_signal_fuzz_flag = 1;
}

static void test_signal_fuzzy(void) {
    g_signal_fuzz_flag = 0;
    signal_default(SIGUSR1);
    signal_default(SIGINT);
    signal_default(SIGTERM);
    TEST("Signal: crash-free edge cases (12 cases)");
    {
        /* Case 1: signal_on with valid signal SIGUSR1 */
        bool ok = signal_on(SIGUSR1, fuzz_signal_handler);
        if (ok) { PASS(); } else { FAIL("signal_on(SIGUSR1) failed"); }
    }
    {
        /* Case 2: signal_on with SIGKILL (uncatchable — should fail) */
        bool ok = signal_on(SIGKILL, fuzz_signal_handler);
        if (!ok) { PASS(); } else { FAIL("signal_on(SIGKILL) should have failed"); }
    }
    {
        /* Case 3: signal_default restores default handler */
        bool ok = signal_default(SIGUSR1);
        if (ok) { PASS(); } else { FAIL("signal_default(SIGUSR1) failed"); }
    }
    {
        /* Case 4: signal_register_common with valid flag pointer */
        volatile int flag = 0;
        bool ok = signal_register_common(&flag);
        if (ok) { PASS(); } else { FAIL("signal_register_common failed"); }
    }
    {
        /* Case 5: signal_register_common with NULL flag */
        bool ok = signal_register_common(NULL);
        if (ok) { PASS(); } else { FAIL("signal_register_common(NULL) failed"); }
    }
    {
        /* Case 6: signal_safe_write with NULL string — no crash */
        signal_safe_write(NULL);
        PASS();
    }
    {
        /* Case 7: signal_safe_write with empty string — no crash */
        signal_safe_write("");
        PASS();
    }
    {
        /* Case 8: signal_safe_write with normal string — no crash */
        signal_safe_write("test");
        PASS();
    }
    {
        /* Case 9: signal_on + raise() triggers handler */
        g_signal_fuzz_flag = 0;
        signal_on(SIGUSR1, fuzz_signal_handler);
        raise(SIGUSR1);
        bool ok = (g_signal_fuzz_flag == 1);
        signal_default(SIGUSR1);
        if (ok) { PASS(); } else { FAIL("raise(SIGUSR1) didn't call handler"); }
    }
    {
        /* Case 10: signal_on + signal_default cycle is idempotent */
        bool a = signal_on(SIGUSR1, fuzz_signal_handler);
        bool b = signal_default(SIGUSR1);
        bool c = signal_on(SIGUSR1, fuzz_signal_handler);
        bool d = signal_default(SIGUSR1);
        if (a && b && c && d) { PASS(); } else { FAIL("signal on/off cycle failed"); }
    }
    {
        /* Case 11: raise SIGINT with registered handler sets flag */
        volatile int flag = 0;
        signal_register_common(&flag);
        raise(SIGINT);
        bool ok = (flag == 1);
        signal_default(SIGINT);
        signal_default(SIGTERM);
        if (ok) { PASS(); } else { FAIL("raise(SIGINT) didn't set flag"); }
    }
    {
        /* Case 12: raise SIGTERM with registered handler sets flag */
        volatile int flag = 0;
        signal_register_common(&flag);
        raise(SIGTERM);
        bool ok = (flag == 1);
        signal_default(SIGINT);
        signal_default(SIGTERM);
        if (ok) { PASS(); } else { FAIL("raise(SIGTERM) didn't set flag"); }
    }
}

static void test_error_classifier_fuzzy(void) {
    classified_error_t result;
    char buf[1024];
    TEST("Error Classifier: crash-free edge cases (12 cases)");
    {
        /* Case 1: NULL error_body, zero status */
        error_classify(0, NULL, "test_provider", "test_model", 0, 0, &result);
        PASS();
    }
    {
        /* Case 2: empty error_body */
        error_classify(0, "", "test_provider", "test_model", 0, 0, &result);
        PASS();
    }
    {
        /* Case 3: negative status_code */
        error_classify(-1, "error", "test_provider", "test_model", 0, 0, &result);
        PASS();
    }
    {
        /* Case 4: NULL provider */
        error_classify(500, "error", NULL, "test_model", 0, 0, &result);
        PASS();
    }
    {
        /* Case 5: NULL model */
        error_classify(500, "error", "test_provider", NULL, 0, 0, &result);
        PASS();
    }
    {
        /* Case 6: 401 auth error */
        error_classify(401, "{\"error\":\"unauthorized\"}", "openai", "gpt-4", 0, 0, &result);
        PASS();
    }
    {
        /* Case 7: 429 rate limit */
        error_classify(429, "{\"error\":\"rate limit exceeded\"}", "anthropic", "claude-3", 0, 0, &result);
        PASS();
    }
    {
        /* Case 8: 413 payload too large */
        error_classify(413, "payload too large", "openrouter", "gpt-4", 5000, 8192, &result);
        PASS();
    }
    {
        /* Case 9: 503 overloaded */
        error_classify(503, "service unavailable", "openai", "gpt-4", 0, 0, &result);
        PASS();
    }
    {
        /* Case 10: error_format with full result */
        error_classify(429, "rate limited", "anthropic", "claude-opus", 1000, 100000, &result);
        int n = error_format(&result, buf, sizeof(buf));
        if (n >= 0 && buf[0] != '\0') { PASS(); } else { FAIL("error_format returned 0 or empty"); }
    }
    {
        /* Case 11: Provider-specific OpenRouter nested error */
        error_classify(500, "{\"error\":{\"metadata\":{\"raw\":\"insufficient_quota\"}}}",
                       "openrouter", "gpt-4", 0, 0, &result);
        PASS();
    }
    {
        /* Case 12: Very large approx_tokens + context_length */
        error_classify(500, "context window full", "anthropic", "claude-3-opus-20240229",
                       200000, 100000, &result);
        PASS();
    }
}

static void test_tool_output_fuzzy(void) {
    TEST("Tool Output: crash-free edge cases (10 cases)");
    {
        /* Case 1: get_max_bytes with default (no env override) */
        int n = tool_output_get_max_bytes();
        if (n == 50000) { PASS(); } else { FAIL("get_max_bytes default not 50000"); }
    }
    {
        /* Case 2: get_max_lines with default */
        int n = tool_output_get_max_lines();
        if (n == 2000) { PASS(); } else { FAIL("get_max_lines default not 2000"); }
    }
    {
        /* Case 3: get_max_line_length with default */
        int n = tool_output_get_max_line_length();
        if (n == 2000) { PASS(); } else { FAIL("get_max_line_length default not 2000"); }
    }
    {
        /* Case 4: exceeds_byte_limit with zero */
        bool r = tool_output_exceeds_byte_limit(0);
        if (!r) { PASS(); } else { FAIL("exceeds_byte_limit(0) should be false"); }
    }
    {
        /* Case 5: exceeds_byte_limit with exactly at limit */
        bool r = tool_output_exceeds_byte_limit(50000);
        if (!r) { PASS(); } else { FAIL("exceeds_byte_limit(50000) should be false"); }
    }
    {
        /* Case 6: exceeds_byte_limit with one over limit */
        bool r = tool_output_exceeds_byte_limit(50001);
        if (r) { PASS(); } else { FAIL("exceeds_byte_limit(50001) should be true"); }
    }
    {
        /* Case 7: exceeds_line_limit with zero */
        bool r = tool_output_exceeds_line_limit(0);
        if (!r) { PASS(); } else { FAIL("exceeds_line_limit(0) should be false"); }
    }
    {
        /* Case 8: exceeds_line_limit with exactly at limit */
        bool r = tool_output_exceeds_line_limit(2000);
        if (!r) { PASS(); } else { FAIL("exceeds_line_limit(2000) should be false"); }
    }
    {
        /* Case 9: exceeds_line_limit with one over limit */
        bool r = tool_output_exceeds_line_limit(2001);
        if (r) { PASS(); } else { FAIL("exceeds_line_limit(2001) should be true"); }
    }
    {
        /* Case 10: negative line count (int overflow safety) */
        bool r = tool_output_exceeds_line_limit(-1);
        if (!r) { PASS(); } else { FAIL("exceeds_line_limit(-1) should be false"); }
    }
}

static void test_file_state_fuzzy(void) {
    TEST("File State: crash-free edge cases (10 cases)");
    fs_clear();
    fs_init();
    {
        /* Case 1: record_read with NULL task_id */
        fs_record_read(NULL, "/tmp/test_fs_fuzz", false, 0.0);
        PASS();
    }
    {
        /* Case 2: record_read with NULL path */
        fs_record_read("test_agent", NULL, false, 0.0);
        PASS();
    }
    {
        /* Case 3: note_write with NULL task_id */
        fs_note_write(NULL, "/tmp/test_fs_fuzz", 0.0);
        PASS();
    }
    {
        /* Case 4: note_write with NULL path */
        fs_note_write("test_agent", NULL, 0.0);
        PASS();
    }
    {
        /* Case 5: check_stale on unwritten path (file doesn't exist) */
        char *warn = fs_check_stale("test_agent", "/tmp/nonexistent_fs_fuzz_path");
        free(warn);
        PASS();
    }
    {
        /* Case 6: lock_path/unlock_path cycle */
        fs_lock_path("/tmp/fuzz_fs_lock_test");
        fs_unlock_path("/tmp/fuzz_fs_lock_test");
        PASS();
    }
    {
        /* Case 7: lock_path with NULL path */
        fs_lock_path(NULL);
        PASS();
    }
    {
        /* Case 8: fs_clear after operations */
        fs_record_read("agent_a", "/tmp/path_a", false, 12345.0);
        fs_note_write("agent_b", "/tmp/path_b", 12346.0);
        fs_clear();
        fs_init();
        PASS();
    }
    {
        /* Case 9: fs_is_disabled with default (no env) */
        bool disabled = fs_is_disabled();
        if (!disabled) { PASS(); } else { FAIL("fs_is_disabled() should be false by default"); }
    }
    {
        /* Case 10: fs_known_reads on empty state */
        char paths[FS_MAX_PATHS_PER_AGENT][256];
        int count = 0;
        fs_known_reads("nonexistent_agent", paths, &count);
        if (count == 0) { PASS(); } else { FAIL("known_reads on empty agent should be 0"); }
    }
}

static void test_debug_helpers_fuzzy(void) {
    debug_session_t session;
    TEST("Debug Helpers: crash-free edge cases (10 cases)");
    {
        /* Case 1: init with NULL tool_name */
        debug_session_init(&session, NULL, "TEST_ENV", "/tmp");
        PASS();
    }
    {
        /* Case 2: init with NULL env_var */
        debug_session_init(&session, "test_tool", NULL, "/tmp");
        PASS();
    }
    {
        /* Case 3: init with NULL hermes_home */
        debug_session_init(&session, "test_tool", "TEST_ENV", NULL);
        PASS();
    }
    {
        /* Case 4: init with all NULL */
        debug_session_init(&session, NULL, NULL, NULL);
        PASS();
    }
    {
        /* Case 5: active check on default-inited session */
        debug_session_init(&session, "test_tool", "TEST_ENV", "/tmp");
        bool active = debug_session_active(&session);
        (void)active;
        PASS();
    }
    {
        /* Case 6: log_call with NULL call_name */
        debug_session_log_call(&session, NULL, "data");
        PASS();
    }
    {
        /* Case 7: log_call with NULL call_data */
        debug_session_log_call(&session, "call", NULL);
        PASS();
    }
    {
        /* Case 8: log_call with both NULL */
        debug_session_log_call(&session, NULL, NULL);
        PASS();
    }
    {
        /* Case 9: save on uninitialized-ish session */
        debug_session_save(&session, "/tmp");
        PASS();
    }
    {
        /* Case 10: get_info on disabled session */
        char *info = debug_session_get_info(&session);
        if (info == NULL) { PASS(); } else { FAIL("get_info on disabled session should be NULL"); free(info); }
    }
}

/* ─── Crypto fuzz ─────────────────────────────── */
static void test_crypto_fuzzy(void) {
    char *err = NULL;
    size_t out_len = 0;
    TEST("Crypto: crash-free edge cases (10 cases)");
    {
        /* Case 1: JWT decode with completely empty token */
        err = NULL;
        char *payload = crypto_jwt_decode("secret", "", &err);
        if (payload == NULL) { PASS(); } else { FAIL("empty JWT should return NULL"); free(payload); }
        free(err); err = NULL;
    }
    {
        /* Case 2: JWT decode with truncated token (no dots) */
        char *payload = crypto_jwt_decode("secret", "abcdef", &err);
        if (payload == NULL) { PASS(); } else { FAIL("dotless JWT should return NULL"); free(payload); }
        free(err); err = NULL;
    }
    {
        /* Case 3: JWT decode with malformed base64 header */
        char *payload = crypto_jwt_decode("secret", "!!!.eyJzdW...MifQ.sig", &err);
        if (payload == NULL) { PASS(); } else { FAIL("malformed base64 JWT should return NULL"); free(payload); }
        free(err); err = NULL;
    }
    {
        /* Case 4: Base64url decode with empty input */
        out_len = 0;
        unsigned char *decoded = crypto_base64url_decode("", &out_len);
        free(decoded);
        PASS();
    }
    {
        /* Case 5: Base64url decode with invalid characters */
        out_len = 0;
        unsigned char *decoded = crypto_base64url_decode("!!!@@@###", &out_len);
        free(decoded);
        PASS();
    }
    {
        /* Case 6: Hex decode with invalid characters */
        out_len = 0;
        unsigned char *bytes = crypto_hex_decode("ZZZZ", &out_len);
        free(bytes);
        PASS();
    }
    {
        /* Case 7: Hex decode with odd length */
        out_len = 0;
        unsigned char *bytes = crypto_hex_decode("ABC", &out_len);
        free(bytes);
        PASS();
    }
    {
        /* Case 8: Hex encode with empty data */
        char *hex = crypto_hex_encode(NULL, 0);
        free(hex);
        PASS();
    }
    {
        /* Case 9: MD5 hex with empty data */
        unsigned char empty = 0;
        char *hash = crypto_md5_hex(&empty, 0);
        if (hash) { PASS(); } else { FAIL("crypto_md5_hex(NULL,0) should produce hash"); }
        free(hash);
    }
    {
        /* Case 10: Hex encode/decode roundtrip with large size */
        unsigned char data[4096];
        memset(data, 0xAB, sizeof(data));
        char *hex = crypto_hex_encode(data, sizeof(data));
        if (hex) {
            out_len = 0;
            unsigned char *dec = crypto_hex_decode(hex, &out_len);
            if (dec && out_len == sizeof(data) && memcmp(dec, data, sizeof(data)) == 0) {
                PASS();
            } else {
                FAIL("hex roundtrip failed for 4096 bytes");
            }
            free(dec);
        } else {
            FAIL("crypto_hex_encode failed");
        }
        free(hex);
    }
}

/* ─── Skill Utils fuzz ──────────────────────────── */
static void test_skill_utils_fuzzy(void) {
    skill_frontmatter_t fm;
    skill_conditions_t conds;
    skill_config_var_t vars[SKILL_UTILS_CFG_MAX];
    char *ns, *name;
    TEST("Skill Utils: crash-free edge cases (10 cases)");
    {
        /* Case 1: parse_frontmatter with NULL content */
        memset(&fm, 0, sizeof(fm));
        int r = skill_parse_frontmatter(NULL, &fm);
        if (r <= 0) { PASS(); } else { FAIL("parse_frontmatter(NULL) should return <= 0"); }
    }
    {
        /* Case 2: parse_frontmatter with empty content */
        memset(&fm, 0, sizeof(fm));
        int r = skill_parse_frontmatter("", &fm);
        if (r >= 0) { PASS(); } else { FAIL("parse_frontmatter(empty) should return >= 0"); }
    }
    {
        /* Case 3: parse_frontmatter with no frontmatter (plain markdown) */
        memset(&fm, 0, sizeof(fm));
        int r = skill_parse_frontmatter("# Hello\n\nThis is plain markdown without frontmatter.", &fm);
        if (r >= 0) { PASS(); } else { FAIL("parse_frontmatter(no fm) should return >= 0"); }
    }
    {
        /* Case 4: parse_frontmatter with malformed YAML */
        memset(&fm, 0, sizeof(fm));
        int r = skill_parse_frontmatter("---\nkey: \n:\nvalue\n---", &fm);
        if (r >= 0) { PASS(); } else { FAIL("parse_frontmatter(malformed) should not crash"); }
    }
    {
        /* Case 5: fm_get on empty frontmatter */
        memset(&fm, 0, sizeof(fm));
        const char *val = skill_fm_get(&fm, "nonexistent");
        if (val == NULL) { PASS(); } else { FAIL("fm_get(nonexistent) on empty fm should be NULL"); }
    }
    {
        /* Case 6: is_excluded_path with edge paths */
        bool e1 = skill_is_excluded_path(".");
        bool e2 = skill_is_excluded_path("/");
        bool e3 = skill_is_excluded_path("");
        if (!e1 || !e2 || !e3) { PASS(); } else { FAIL("is_excluded_path on edge paths"); }
        (void)e1; (void)e2; (void)e3;
    }
    {
        /* Case 7: extract_conditions on empty frontmatter */
        memset(&fm, 0, sizeof(fm));
        memset(&conds, 0, sizeof(conds));
        skill_extract_conditions(&fm, &conds);
        PASS();
    }
    {
        /* Case 8: extract_config_vars on empty frontmatter */
        memset(&fm, 0, sizeof(fm));
        memset(vars, 0, sizeof(vars));
        int n = skill_extract_config_vars(&fm, vars, SKILL_UTILS_CFG_MAX);
        if (n == 0) { PASS(); } else { FAIL("extract_config_vars on empty should be 0"); }
    }
    {
        /* Case 9: parse_qualified_name edge cases */
        skill_parse_qualified_name("simple", &ns, &name);
        if (ns == NULL && name) {
            bool ok = (strcmp(name, "simple") == 0);
            free(name);
            if (ok) { PASS(); } else { FAIL("qualified_name simple should parse"); }
        } else { FAIL("qualified_name simple gave unexpected result"); free(ns); free(name); }
    }
    {
        /* Case 10: parse_qualified_name with colon, and valid namespace check */
        skill_parse_qualified_name("hermes:skill-name", &ns, &name);
        if (ns && name) {
            bool ns_ok = skill_is_valid_namespace(ns);
            bool name_ok = (strcmp(name, "skill-name") == 0);
            free(ns); free(name);
            if (ns_ok && name_ok) { PASS(); } else { FAIL("qualified_name parse gave wrong parts"); }
        } else { FAIL("qualified_name hermes:skill-name should parse"); free(ns); free(name); }
    }
}

/* ─── JSON5 fuzz ──────────────────────────────── */
static void test_json5_fuzzy(void) {
    TEST("JSON5: crash-free edge cases (10 cases)");
    {
        /* Case 1: completely empty input */
        char *err = NULL;
        json_t *doc = json5_parse("", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
    {
        /* Case 2: comments only (JSON5-style) */
        char *err = NULL;
        json_t *doc = json5_parse("// just a comment\n", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
    {
        /* Case 3: block comment only */
        char *err = NULL;
        json_t *doc = json5_parse("/* block comment */", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
    {
        /* Case 4: single-quoted strings */
        char *err = NULL;
        json_t *doc = json5_parse("{'key': 'value'}", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
    {
        /* Case 5: trailing comma */
        char *err = NULL;
        json_t *doc = json5_parse("{\"a\":1,\"b\":2,}", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
    {
        /* Case 6: unquoted keys */
        char *err = NULL;
        json_t *doc = json5_parse("{key: 'value'}", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
    {
        /* Case 7: hex number */
        char *err = NULL;
        json_t *doc = json5_parse("{val: 0xFF}", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
    {
        /* Case 8: malformed nested block comment */
        char *err = NULL;
        json_t *doc = json5_parse("{\"a\": /* nested /* comment */ 1}", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
    {
        /* Case 9: very long single line with mixed comments */
        char *err = NULL;
        json_t *doc = json5_parse("{\"a\":1 // inline\n,\"b\":2 /* block */,\"c\":3,}", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
    {
        /* Case 10: extremely deep nesting (JSON5 with comments at each level) */
        char *err = NULL;
        json_t *doc = json5_parse("{\"a\":{\"b\":{\"c\":{\"d\":{\"e\":1 // deep\n}}}}}", &err);
        json5_free(doc);
        free(err);
        PASS();
    }
}

/* ─── Property: path normalize idempotency ────────────────── */
static void test_property_path_normalize(void) {
    TEST("Property: path normalize idempotency (10 cases)");

    const char *inputs[] = {
        "",               /* empty */
        "/",              /* root */
        "..",             /* parent */
        "./foo/../bar",   /* relative with traversal */
        "/usr/local/bin", /* absolute normal */
        "a/b/c/d/e/f",    /* deeply nested relative */
        "/a/./b/./c",    /* dots in path */
        "//double//slash", /* double slashes */
        "foo/bar/",       /* trailing slash */
        "/a/b/../../c",   /* over-traversal */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *first = path_normalize(inputs[i]);
        if (first) {
            char *second = path_normalize(first);
            if (second) {
                if (strcmp(first, second) != 0) {
                    FAIL("path normalize not idempotent");
                    printf("        input: \"%s\" -> \"%s\" -> \"%s\"\n",
                           inputs[i], first, second);
                }
                free(second);
            }
            free(first);
        }
    }
    PASS();
}

int main(void) {
    printf("=== Fuzz Tests (T08) ===\n\n");

    printf("-- JSON --\n");
    test_json_malformed();
    test_json_large();
    test_json_long_string();
    test_json_unicode();
    test_json_edge();
    test_json_unicode_edge();

    printf("\n-- YAML --\n");
    test_yaml_malformed();
    test_yaml_large();

    printf("\n-- Template --\n");
    test_template_malformed();
    test_template_deep_nested();
    test_template_unmatched_tags();

    printf("\n-- Regex --\n");
    test_regex_malformed();
    test_regex_large();
    test_regex_more_edge();

    printf("\n-- HTML --\n");
    test_html_malformed();
    test_html_extreme_nested();

    printf("\n-- Path --\n");
    test_path_malformed();
    test_path_very_long();

    printf("\n-- Cron --\n");
    test_cron_malformed();

    printf("\n-- TOML --\n");
    test_toml_malformed();

    printf("\n-- CSV --\n");
    test_csv_malformed();

    printf("\n-- Base64 --\n");
    test_base64_decode();

    printf("\n-- Datetime --\n");
    test_datetime_parse();

    printf("\n-- Website Patterns --\n");
    test_website_patterns();

    printf("\n-- Fuzzy Match --\n");
    test_fuzzy_match_fuzzy();

    printf("\n-- Schema Sanitizer --\n");
    test_schema_sanitizer_fuzzy();

    printf("\n-- Glob --\n");
    test_glob_pattern_fuzzy();

    printf("\n-- Hash --\n");
    test_hash_fuzzy();

    printf("\n-- Difflib --\n");
    test_difflib_fuzzy();

    printf("\n-- UUID --\n");
    test_uuid_fuzzy();

    printf("\n-- Textwrap --\n");
    test_textwrap_fuzzy();

    printf("\n-- Dotenv --\n");
    test_dotenv_fuzzy();

    printf("\n-- Binary --\n");
    test_binary_fuzzy();

    printf("\n-- Interrupt --\n");
    test_interrupt_fuzzy();

    printf("\n-- ANSI --\n");
    test_ansi_fuzzy();

    printf("\n-- Budget Config --\n");
    test_budget_config_fuzzy();

    printf("\n-- Env Passthrough --\n");
    test_env_passthrough_fuzzy();

    printf("\n-- Threat Patterns --\n");
    test_threat_patterns_fuzzy();

    printf("\n-- Rate Limit --\n");
    test_rate_limit_fuzzy();

    printf("\n-- Slash Confirm --\n");
    test_slash_confirm_fuzzy();

    printf("\n-- Signal --\n");
    test_signal_fuzzy();

    printf("\n-- Error Classifier --\n");
    test_error_classifier_fuzzy();

    printf("\n-- Tool Output --\n");
    test_tool_output_fuzzy();

    printf("\n-- File State --\n");
    test_file_state_fuzzy();

    printf("\n-- Debug Helpers --\n");
    test_debug_helpers_fuzzy();

    printf("\n-- Crypto --\n");
    test_crypto_fuzzy();

    printf("\n-- Skill Utils --\n");
    test_skill_utils_fuzzy();

    printf("\n-- JSON5 --\n");
    test_json5_fuzzy();

    printf("\n-- Property tests --\n");
    test_property_json_roundtrip();
    test_property_json_numeric_edge();
    test_property_json_recursive_array();
    test_property_path_normalize();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
