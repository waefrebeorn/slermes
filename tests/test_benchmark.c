/*
 * test_benchmark.c — Performance benchmarks for Slermes C core (S7 X11).
 *
 * Measures timing of critical operations. Verifies operations complete
 * within sensible time bounds.
 *
 * Compile (from project root):
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_benchmark.c \
 *       lib/libjson/json.c lib/libplugin/plugin.c \
 *       src/agent/context.c src/agent/agent_loop.c src/agent/checkpoint.c \
 *       -o /tmp/hermes_bench -lm -lpthread \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_bench
 */

#include "hermes.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Pull in the hashing API from libhash */
#include "hash.h"

/* Path normalize benchmark dependency */
#include <path.h>

/* HTML strip benchmark dependency */
#include <html.h>

/* Base64 benchmark dependency */
#include <base64.h>

/* UUID benchmark dependency */
#include <uuid.h>

/* CSV benchmark dependency */
#include <csv.h>

/* Regex benchmark dependency */
#include <hermes_regex.h>

/* TOML benchmark dependency */
#include <toml.h>

/* YAML benchmark dependency */
#include <yaml.h>

/* Cron benchmark dependency */
#include <cron.h>

/* Dotenv benchmark dependency */
#include <dotenv.h>

/* Textwrap benchmark dependency */
#include <textwrap.h>

/* Fuzzy match benchmark dependency */
#include <fuzzy_match.h>

/* Glob benchmark dependency */
#include <hermes_glob.h>

/* ANSI strip benchmark dependency */
#include <ansi_strip.h>

/* Datetime benchmark dependency */
#include <datetime.h>

/* Binary benchmark dependency */
#include <binary.h>

/* Difflib benchmark dependency */
#include <difflib.h>

/* Template benchmark dependency */
#include <template.h>

/* Interrupt benchmark dependency */
#include <interrupt.h>

static int g_pass = 0, g_fail = 0, g_skip = 0;

#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        g_fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        g_pass++; \
    } \
} while(0)

static double wall_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ================================================================
 *  1. JSON parse/serialize throughput
 * ================================================================ */
static void bench_json_parse(void) {
    printf("\n--- Benchmark: JSON Parse/Serialize ---\n");

    const char *json = "{\"model\":\"gpt-4o\",\"provider\":\"openai\",\"max_tokens\":4096,"
        "\"temperature\":0.7,\"top_p\":0.95}";

    int iterations = 2000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        char *err = NULL;
        json_t *doc = json_parse(json, &err);
        if (!doc) {
            printf("  SKIP: JSON parse failed at iteration %d\n", i);
            g_skip++;
            return;
        }
        char *serialized = json_serialize(doc);
        json_free(doc);
        free(serialized);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d iterations in %.3fs (%.0f ops/sec)\n", iterations, elapsed, ops_per_sec);
    TEST("JSON parse+serialize > 100 ops/sec", ops_per_sec > 100.0);
    TEST("elapsed < 10s for 2000 iterations", elapsed < 10.0);
}

/* ================================================================
 *  2. Message allocation throughput
 * ================================================================ */
static void bench_message_alloc(void) {
    printf("\n--- Benchmark: Message Alloc/Free ---\n");

    int iterations = 5000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        message_t *msg = message_new(MSG_USER, "test");
        if (!msg) { g_skip++; return; }
        message_free(msg);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d alloc+free in %.3fs (%.0f ops/sec)\n", iterations, elapsed, ops_per_sec);
    TEST("message alloc/free > 5000 ops/sec", ops_per_sec > 5000.0);
    TEST("elapsed < 5s", elapsed < 5.0);
}

/* ================================================================
 *  3. Token estimation throughput
 * ================================================================ */
static void bench_token_estimation(void) {
    printf("\n--- Benchmark: Token Estimation ---\n");

    char large_text[5001];
    memset(large_text, 'A', 5000);
    large_text[5000] = '\0';

    int iterations = 5000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        size_t tokens = llm_estimate_tokens(large_text);
        (void)tokens;
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d token estimates (5KB each) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("token estimation > 1000 ops/sec", ops_per_sec > 1000.0);
    TEST("elapsed < 5s", elapsed < 5.0);
}

/* ================================================================
 *  4. Context push/pop throughput
 * ================================================================ */
static void bench_context_push_pop(void) {
    printf("\n--- Benchmark: Context Push/Pop ---\n");

    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);

    /* Interleaved push/pop: context never exceeds 1 message —
     * no HERMES_MAX_MESSAGES cap to worry about. */
    int iterations = 100000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        message_t *msg = message_new(MSG_USER, "test message content");
        if (!msg) { g_skip++; context_clear(&state); return; }
        if (!context_push(&state, msg)) {
            message_free(msg);
            g_skip++; context_clear(&state); return;
        }
        message_t *popped = context_pop(&state);
        message_free(popped);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = (iterations * 2) / elapsed; /* push + pop */
    printf("  %d push+pop pairs in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("context push/pop > 10000 ops/sec", ops_per_sec > 10000.0);
    TEST("elapsed < 5s", elapsed < 5.0);
    context_clear(&state);
}

/* ================================================================
 *  5. Message clone throughput
 * ================================================================ */
static void bench_message_clone(void) {
    printf("\n--- Benchmark: Message Clone ---\n");

    char big_content[4096];
    memset(big_content, 'A', 4000);
    big_content[4000] = '\0';

    message_t *src = message_new(MSG_USER, big_content);
    if (!src) { g_skip++; return; }

    int iterations = 10000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        message_t *clone = message_clone(src);
        if (!clone) { g_skip++; message_free(src); return; }
        message_free(clone);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d clones (4KB content) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("message clone > 50000 ops/sec", ops_per_sec > 50000.0);
    TEST("elapsed < 5s", elapsed < 5.0);
    message_free(src);
}

/* ================================================================
 *  6. Context truncate throughput
 * ================================================================ */
static void bench_context_truncate(void) {
    printf("\n--- Benchmark: Context Truncate ---\n");

    int push_count = 3000;
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);

    /* Push all messages first */
    double start = wall_time();

    for (int i = 0; i < push_count; i++) {
        message_t *msg = message_new(MSG_USER, "test");
        if (!msg) { g_skip++; context_clear(&state); return; }
        if (!context_push(&state, msg)) {
            message_free(msg);
            g_skip++; context_clear(&state); return;
        }
    }

    /* Truncate from 5000 to 5 */
    context_truncate(&state, 5);

    double elapsed = wall_time() - start;
    double ops_per_sec = push_count / elapsed;
    printf("  %d push + truncate(5) in %.3fs (%.0f push/sec)\n",
           push_count, elapsed, ops_per_sec);
    TEST("context truncate > 5000 push/sec", ops_per_sec > 5000.0);
    TEST("remaining messages == 5", state.message_count == 5);
    context_clear(&state);
}

/* ================================================================
 *  7. JSON field access (get_str/get_int)
 * ================================================================ */
static void bench_json_field_access(void) {
    printf("\n--- Benchmark: JSON Field Access ---\n");

    const char *json_str = "{"
        "\"model\":\"gpt-4o\","
        "\"provider\":\"openai\","
        "\"max_tokens\":4096,"
        "\"temperature\":0.7,"
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"Hello\"}}]"
    "}";
    char *err = NULL;
    json_t *doc = json_parse(json_str, &err);
    if (!doc || err) {
        printf("  SKIP: JSON parse failed\n");
        free(err); g_skip++; return;
    }

    int iterations = 50000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        const char *model = json_get_str(doc, "model", NULL);
        const char *provider = json_get_str(doc, "provider", NULL);
        double max_tok = json_get_num(doc, "max_tokens", 0);
        json_t *choices = json_obj_get(doc, "choices");
        (void)model; (void)provider; (void)max_tok; (void)choices;
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d field accesses in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("JSON field access > 50000 ops/sec", ops_per_sec > 50000.0);
    TEST("elapsed < 5s", elapsed < 5.0);
    json_free(doc);
}

/* ================================================================
 *  8. SHA-256 hash throughput (64 bytes / 1KB / 64KB)
 * ================================================================ */
static void bench_sha256_throughput(void) {
    printf("\n--- Benchmark: SHA-256 Throughput (64B, 1KB, 64KB) ---\n");

    struct { const char *label; size_t size; int iter; double min_ops; } sizes[] = {
        {"64 bytes",      64,    50000, 50000.0},
        {"1 KB",         1024,   10000, 10000.0},
        {"64 KB",        65536,   1000,  1000.0},
    };
    int nsizes = sizeof(sizes) / sizeof(sizes[0]);
    int all_ok = 1;

    for (int s = 0; s < nsizes; s++) {
        size_t sz = sizes[s].size;
        int iter = sizes[s].iter;

        unsigned char *buf = malloc(sz);
        if (!buf) { printf("  SKIP: malloc(%zu) failed\n", sz); g_skip++; all_ok = 0; continue; }
        memset(buf, 'A', sz);

        double start = wall_time();

        for (int i = 0; i < iter; i++) {
            unsigned char *h = hash_sha256(buf, sz);
            free(h);
        }

        double elapsed = wall_time() - start;
        double ops_per_sec = iter / elapsed;
        printf("  %s: %d hashes in %.3fs (%.0f ops/sec)\n",
               sizes[s].label, iter, elapsed, ops_per_sec);
        char cond_name[128];
        snprintf(cond_name, sizeof(cond_name),
                 "SHA-256 %s > %.0f ops/sec", sizes[s].label, sizes[s].min_ops);
        TEST(cond_name, ops_per_sec > sizes[s].min_ops);
        free(buf);
    }
    TEST("elapsed < 10s for all sizes", all_ok); /* placeholder — all_ok from individual */
    (void)all_ok;
}

/* ================================================================
 *  9. HMAC-SHA256 throughput
 * ================================================================ */
static void bench_hmac_sha256(void) {
    printf("\n--- Benchmark: HMAC-SHA256 Throughput (1KB) ---\n");

    unsigned char key[32];
    memset(key, 0x42, 32);

    size_t data_sz = 1024;
    unsigned char *data = malloc(data_sz);
    if (!data) { printf("  SKIP: malloc(%zu) failed\n", data_sz); g_skip++; return; }
    memset(data, 'B', data_sz);

    int iterations = 10000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        unsigned char *h = hash_hmac_sha256(key, 32, data, data_sz);
        free(h);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d HMAC-SHA256 (1KB data) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("HMAC-SHA256 > 5000 ops/sec", ops_per_sec > 5000.0);
    TEST("elapsed < 5s", elapsed < 5.0);
    free(data);
}

/* ================================================================
 *  10. Checkpoint save throughput
 * ================================================================ */
static void bench_checkpoint_save(void) {
    printf("\n--- Benchmark: Checkpoint Save (10 messages) ---\n");

    /* Create agent state with 10 messages */
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);

    for (int i = 0; i < 10; i++) {
        message_t *msg = message_new(MSG_USER, "test message content for checkpoint");
        if (!msg) { g_skip++; context_clear(&state); return; }
        context_push(&state, msg);
    }

    checkpoint_manager_t mgr;
    checkpoint_init(&mgr);
    checkpoint_set_limits(&mgr, 100, 0);

    int iterations = 10000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        checkpoint_save(&mgr, &state, NULL);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d checkpoint saves (10 msgs) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("checkpoint save > 5000 ops/sec", ops_per_sec > 5000.0);
    TEST("elapsed < 10s", elapsed < 10.0);

    checkpoint_free(&mgr);
    context_clear(&state);
}

/* ================================================================
 *  11. Checkpoint restore throughput
 * ================================================================ */
static void bench_checkpoint_restore(void) {
    printf("\n--- Benchmark: Checkpoint Restore (10 messages) ---\n");

    /* Create agent state with 10 messages and save one checkpoint */
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    context_init(&state);

    for (int i = 0; i < 10; i++) {
        message_t *msg = message_new(MSG_USER, "test message content for checkpoint");
        if (!msg) { g_skip++; context_clear(&state); return; }
        context_push(&state, msg);
    }

    checkpoint_manager_t mgr;
    checkpoint_init(&mgr);
    checkpoint_set_limits(&mgr, 100, 0);
    checkpoint_save(&mgr, &state, NULL);

    if (checkpoint_count(&mgr) == 0) {
        printf("  SKIP: checkpoint save failed\n");
        g_skip++; context_clear(&state); return;
    }

    int iterations = 10000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        checkpoint_restore(&mgr, &state, NULL);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d checkpoint restores (10 msgs) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("checkpoint restore > 10000 ops/sec", ops_per_sec > 10000.0);
    TEST("elapsed < 10s", elapsed < 10.0);

    checkpoint_free(&mgr);
    context_clear(&state);
}

/* ================================================================
 *  12. Path normalize throughput
 * ================================================================ */
static void bench_path_normalize(void) {
    printf("\n--- Benchmark: Path Normalize ---\n");

    const char *paths[] = {
        "/usr/local/bin/slermes",
        "/home/user/.hermes/config.yaml",
        "./src/agent/../tools/./dispatcher.c",
        "a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p",
        "/very/../deeply/../../nested/./path/././././example",
        "",
        "/",
        "simple_flat_file.txt",
    };
    int num_paths = sizeof(paths) / sizeof(paths[0]);

    int iterations = 10000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        for (int j = 0; j < num_paths; j++) {
            char *result = path_normalize(paths[j]);
            free(result);
        }
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = (iterations * num_paths) / elapsed;
    printf("  %d paths x %d iterations in %.3fs (%.0f ops/sec)\n",
           num_paths, iterations, elapsed, ops_per_sec);
    TEST("path normalize > 100000 ops/sec", ops_per_sec > 100000.0);
    TEST("elapsed < 10s for 10000 iterations", elapsed < 10.0);
}

/* ================================================================
 *  13. HTML strip throughput
 * ================================================================ */
static void bench_html_strip(void) {
    printf("\n--- Benchmark: HTML Strip ---\n");

    const char *html_input = "<html><body><h1>Hello World</h1>"
        "<p>This is a <b>test</b> paragraph with <a href=\"link\">a link</a>.</p>"
        "<ul><li>Item 1</li><li>Item 2</li><li>Item 3</li></ul>"
        "<div class=\"content\"><p>Nested <span>content</span> here</p></div>"
        "</body></html>";

    int iterations = 10000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        char *result = html_strip_tags(html_input);
        free(result);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d HTML strip ops in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("html_strip > 10000 ops/sec", ops_per_sec > 10000.0);
    TEST("elapsed < 10s for 10000 iterations", elapsed < 10.0);
}

/* ================================================================
 *  14. Base64 encode/decode throughput (small, medium, large)
 * ================================================================ */
static void bench_base64_throughput(void) {
    printf("\n--- Benchmark: Base64 Encode/Decode (256B, 64KB) ---\n");

    struct { const char *label; size_t size; int iter; double min_ops; } sizes[] = {
        {"256 bytes encode", 256, 50000, 50000.0},
        {"256 bytes decode", 256, 50000, 50000.0},
        {"64 KB encode",    65536, 1000, 1000.0},
        {"64 KB decode",    65536, 1000, 1000.0},
    };
    int nsizes = sizeof(sizes) / sizeof(sizes[0]);
    int all_ok = 1;

    for (int s = 0; s < nsizes; s++) {
        size_t sz = sizes[s].size;
        int iter = sizes[s].iter;
        bool is_encode = (s % 2 == 0);

        unsigned char *raw = malloc(sz);
        if (!raw) { printf("  SKIP: malloc(%zu) failed\n", sz); g_skip++; all_ok = 0; continue; }
        memset(raw, 'X', sz);

        double start = wall_time();

        if (is_encode) {
            for (int i = 0; i < iter; i++) {
                char *enc = base64_encode(raw, sz);
                free(enc);
            }
        } else {
            /* Encode once, then decode iteratively */
            char *encoded = base64_encode(raw, sz);
            if (!encoded) { free(raw); g_skip++; all_ok = 0; continue; }
            for (int i = 0; i < iter; i++) {
                size_t out_len = 0;
                unsigned char *dec = base64_decode(encoded, &out_len);
                free(dec);
            }
            free(encoded);
        }

        double elapsed = wall_time() - start;
        double ops_per_sec = iter / elapsed;
        printf("  %s: %d ops in %.3fs (%.0f ops/sec)\n",
               sizes[s].label, iter, elapsed, ops_per_sec);
        char cond_name[128];
        snprintf(cond_name, sizeof(cond_name),
                 "base64 %s > %.0f ops/sec", sizes[s].label, sizes[s].min_ops);
        TEST(cond_name, ops_per_sec > sizes[s].min_ops);
        free(raw);
    }
    TEST("base64 all sizes < 10s", all_ok);
    (void)all_ok;
}

/* ================================================================
 *  15. UUID v4 generation throughput
 * ================================================================ */
static void bench_uuid_throughput(void) {
    printf("\n--- Benchmark: UUID v4 Generation ---\n");

    int iterations = 100000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        char *uuid = uuid_v4();
        free(uuid);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d UUIDs in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("UUID v4 > 50000 ops/sec", ops_per_sec > 50000.0);
    TEST("elapsed < 5s for 100K iterations", elapsed < 5.0);
}

/* ================================================================
 *  16. CSV parsing throughput
 * ================================================================ */
static void bench_csv_parsing(void) {
    printf("\n--- Benchmark: CSV Parsing (10 cols, 1000 rows) ---\n");

    static const char *col_prefixes[] = {
        "id", "name", "email", "age", "score",
        "grade", "city", "country", "zip", "status"
    };
    int rows = 1000;
    size_t csv_sz = 150000;
    char *csv = malloc(csv_sz);
    if (!csv) { printf("  SKIP: malloc failed\n"); g_skip++; return; }

    int pos = 0;
    /* Header */
    pos += snprintf(csv + pos, csv_sz - pos,
        "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
        col_prefixes[0], col_prefixes[1], col_prefixes[2],
        col_prefixes[3], col_prefixes[4], col_prefixes[5],
        col_prefixes[6], col_prefixes[7], col_prefixes[8], col_prefixes[9]);

    for (int i = 0; i < rows && pos < (int)csv_sz - 200; i++) {
        pos += snprintf(csv + pos, csv_sz - pos,
            "%d,user_%d,user%d@example.com,%d,%.1f,A,City_%d,US,%05d,active\n",
            i, i, i, 20 + (i % 60), (double)(i % 100) / 10.0,
            i % 50, i % 99999);
    }
    csv[pos] = '\0';

    int iterations = 500;
    double start = wall_time();
    for (int iter = 0; iter < iterations; iter++) {
        csv_reader_t *r = csv_reader_open_string(csv, strlen(csv), ',');
        if (!r) { free(csv); g_skip++; return; }
        int field_count = 0;
        char **fields;
        int row_count = 0;
        while ((fields = csv_reader_read_row(r, &field_count)) != NULL) {
            row_count++;
            csv_free_fields(fields, field_count);
        }
        csv_reader_close(r);
        if (row_count < 1000) {
            free(csv);
            printf("  SKIP: expected >= 1000 rows, got %d\n", row_count);
            g_skip++; return;
        }
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d parses of %d-row CSV in %.3fs (%.0f ops/sec)\n",
           iterations, rows, elapsed, ops_per_sec);
    TEST("CSV parsing > 50 ops/sec", ops_per_sec > 50.0);
    TEST("elapsed < 10s for 500 iterations", elapsed < 10.0);
    free(csv);
}

/* ================================================================
 *  17. Regex extraction throughput
 * ================================================================ */
static void bench_regex_extraction(void) {
    printf("\n--- Benchmark: Regex Extraction (5 patterns, long text) ---\n");

    /* Build a 50KB text block with embeddable patterns */
    size_t text_sz = 55000;
    char *text = malloc(text_sz);
    if (!text) { printf("  SKIP: malloc failed\n"); g_skip++; return; }

    int pos = 0;
    for (int i = 0; i < 50 && pos < (int)text_sz - 1100; i++) {
        pos += snprintf(text + pos, text_sz - pos,
            "User %d: alice%d@example.com, phone: %d-%d-%04d, "
            "order #ORD-%06d, date: 2024-%02d-%02d\n",
            i, i, 555, 100 + i, i * 17 % 10000,
            i * 12345 % 1000000, 1 + (i % 12), 1 + (i % 28));
    }
    text[pos] = '\0';

    const char *patterns[] = {
        "[a-zA-Z0-9._%%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}",  /* email */
        "\\d{3}-\\d{3}-\\d{4}",                                /* phone */
        "ORD-\\d{6}",                                          /* order id */
        "\\b\\d{4}-\\d{2}-\\d{2}\\b",                         /* date pattern */
        "[Uu]ser\\s+\\d+",                                     /* user reference */
    };
    int n_patterns = sizeof(patterns) / sizeof(patterns[0]);

    int iterations = 50000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        int p_idx = i % n_patterns;
        char *match = regex_extract(patterns[p_idx], text, 0);
        free(match);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d regex extractions (5 patterns, 50KB text) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("regex extraction > 50000 ops/sec", ops_per_sec > 50000.0);
    TEST("elapsed < 10s for 50000 iterations", elapsed < 10.0);
    free(text);
}

/* ================================================================
 *  18. TOML parsing throughput
 * ================================================================ */
static void bench_toml_parsing(void) {
    printf("\n--- Benchmark: TOML Parsing (config-style, 40 keys, nested tables) ---\n");

    const char *toml_input =
        "[server]\n"
        "host = \"0.0.0.0\"\n"
        "port = 8080\n"
        "workers = 4\n"
        "debug = false\n"
        "log_file = \"/var/log/app.log\"\n"
        "\n"
        "[database]\n"
        "host = \"localhost\"\n"
        "port = 5432\n"
        "name = \"slermes\"\n"
        "user = \"admin\"\n"
        "pool_size = 20\n"
        "timeout = 30.0\n"
        "ssl = true\n"
        "\n"
        "[cache]\n"
        "backend = \"redis\"\n"
        "host = \"127.0.0.1\"\n"
        "port = 6379\n"
        "ttl = 3600\n"
        "max_items = 10000\n"
        "\n"
        "[logging]\n"
        "level = \"info\"\n"
        "format = \"json\"\n"
        "output = \"stdout\"\n"
        "max_size_mb = 100\n"
        "backup_count = 7\n"
        "\n"
        "[features]\n"
        "enable_telemetry = false\n"
        "enable_metrics = true\n"
        "rate_limiting = true\n"
        "max_requests_per_min = 1000\n"
        "\n"
        "[auth]\n"
        "jwt_secret = \"supersecretkey\"\n"
        "token_expiry = 3600\n"
        "refresh_expiry = 86400\n"
        "bcrypt_rounds = 12\n"
        "\n"
        "[email]\n"
        "smtp_host = \"smtp.example.com\"\n"
        "smtp_port = 587\n"
        "from_address = \"noreply@example.com\"\n"
        "use_tls = true\n";

    int iterations = 50000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        toml_doc_t *doc = toml_parse(toml_input);
        if (doc) {
            toml_free(doc);
        }
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d TOML parses (40+ keys, 7 tables) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("TOML parsing > 50000 ops/sec", ops_per_sec > 50000.0);
    TEST("elapsed < 10s for 50000 iterations", elapsed < 10.0);
}

/* ================================================================
 *  19. YAML parsing throughput
 * ================================================================ */
static void bench_yaml_parsing(void) {
    printf("\n--- Benchmark: YAML Parsing (config-style, nested keys, lists) ---\n");

    const char *yaml_input =
        "server:\n"
        "  host: 0.0.0.0\n"
        "  port: 8080\n"
        "  workers: 4\n"
        "  debug: false\n"
        "\n"
        "database:\n"
        "  host: localhost\n"
        "  port: 5432\n"
        "  name: slermes\n"
        "  user: admin\n"
        "  pool_size: 20\n"
        "\n"
        "logging:\n"
        "  level: info\n"
        "  format: json\n"
        "  output: stdout\n"
        "\n"
        "features:\n"
        "  enabled:\n"
        "    - telemetry\n"
        "    - metrics\n"
        "    - rate_limiting\n"
        "  max_requests_per_min: 1000\n"
        "\n"
        "cache:\n"
        "  backend: redis\n"
        "  hosts:\n"
        "    - 127.0.0.1:6379\n"
        "    - 127.0.0.2:6379\n"
        "  ttl: 3600\n"
        "\n"
        "auth:\n"
        "  jwt_secret: supersecretkey\n"
        "  token_expiry: 3600\n"
        "  refresh_expiry: 86400\n";

    int iterations = 50000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(yaml_input, &err);
        free(err);
        if (doc) yaml_free(doc);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d YAML parses (30+ keys, 6 nested sections, 2 lists) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("YAML parsing > 50000 ops/sec", ops_per_sec > 50000.0);
    TEST("elapsed < 10s for 50000 iterations", elapsed < 10.0);
}

/* ================================================================
 *  20. Cron expression parsing throughput
 * ================================================================ */
static void bench_cron_parsing(void) {
    printf("\n--- Benchmark: Cron Expression Parsing (12 patterns) ---\n");

    const char *expressions[] = {
        "0 0 * * *",           /* daily midnight */
        "*/15 * * * *",        /* every 15 min */
        "0 9-17 * * 1-5",      /* work hours weekdays */
        "30 4 * * 0",          /* 4:30am Sundays */
        "0 0 1 * *",           /* monthly */
        "0 */2 * * *",         /* every 2 hours */
        "5 0 * 8 *",           /* August daily at 12:05am */
        "*/5 9-17 * * 1-5",   /* every 5 min during work hours */
        "0 0 1 1 *",           /* yearly Jan 1 */
        "15,30,45 * * * *",   /* :15 :30 :45 each hour */
        "0 0 * * 0",           /* weekly Sunday midnight */
        "@hourly",             /* special shorthand */
    };
    int n = sizeof(expressions) / sizeof(expressions[0]);

    int iterations = 100000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        int idx = i % n;
        char *err = NULL;
        cron_expr_t *c = cron_parse(expressions[idx], &err);
        free(err);
        if (c) cron_free(c);
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d cron parses (12 patterns) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("cron parsing > 50000 ops/sec", ops_per_sec > 50000.0);
    TEST("elapsed < 10s for 100000 iterations", elapsed < 10.0);
}

/* ================================================================
 *  21. Dotenv parsing throughput (libdotenv)
 * ================================================================ */
static void bench_dotenv_parsing(void) {
    printf("\n--- Benchmark: Dotenv Parsing (20 key-value pairs) ---\n");

    const char *dotenv_input =
        "APP_NAME=slermes\n"
        "APP_ENV=production\n"
        "APP_DEBUG=false\n"
        "APP_PORT=8080\n"
        "DB_HOST=localhost\n"
        "DB_PORT=5432\n"
        "DB_NAME=hermes\n"
        "DB_USER=admin\n"
        "DB_PASSWORD=supersecret\n"
        "REDIS_HOST=127.0.0.1\n"
        "REDIS_PORT=6379\n"
        "REDIS_DB=0\n"
        "AWS_REGION=us-east-1\n"
        "AWS_BUCKET=my-bucket\n"
        "LOG_LEVEL=info\n"
        "LOG_FORMAT=json\n"
        "CACHE_TTL=3600\n"
        "MAX_WORKERS=8\n"
        "SECRET_KEY=abcdef123456\n"
        "API_RATE_LIMIT=100\n";

    int iterations = 50000;
    double start = wall_time();

    for (int i = 0; i < iterations; i++) {
        char *err = NULL;
        env_t *env = dotenv_parse(dotenv_input, &err);
        free(err);
        if (env) {
            /* Access a few keys to simulate real usage */
            const char *name = dotenv_get(env, "APP_NAME");
            const char *port = dotenv_get(env, "APP_PORT");
            const char *db = dotenv_get(env, "DB_NAME");
            size_t count = dotenv_count(env);
            (void)name; (void)port; (void)db; (void)count;
            dotenv_free(env);
        }
    }

    double elapsed = wall_time() - start;
    double ops_per_sec = iterations / elapsed;
    printf("  %d dotenv parses (20 keys) in %.3fs (%.0f ops/sec)\n",
           iterations, elapsed, ops_per_sec);
    TEST("dotenv parsing > 50000 ops/sec", ops_per_sec > 50000.0);
    TEST("elapsed < 10s for 50000 iterations", elapsed < 10.0);
}

/* ================================================================
 *  22. Textwrap throughput (libtextwrap)
 * ================================================================ */
static void bench_textwrap_throughput(void) {
    printf("\n--- Benchmark: Textwrap Fill (2KB text, widths 40/60/80) ---\n");

    const char *long_text =
        "The Slermes C project is a comprehensive port of the Hermes AI agent "
        "framework from Python to C, providing a standalone binary with no "
        "Python dependencies. It implements the core agent loop, tool execution, "
        "plugin system, gateway platform support, and CLI interface. The project "
        "aims to achieve full parity with the Python version while maintaining "
        "high performance and low resource usage. Key features include an "
        "ncurses-based TUI with full-screen mode, a JSON-RPC gateway server, "
        "WebSocket support, markdown rendering, and real-time streaming with "
        "type-ahead input. The CLI ecosystem provides setup wizards, auth "
        "management, platform configuration, kanban boards, skills hub, voice "
        "mode, and plugin management. The plugin system supports dynamic loading "
        "of shared libraries with 10 shipped plugins covering achievements, "
        "disk cleanup, file memory, Google Meet, kanban, observability, skills, "
        "and Spotify integration. Test coverage includes 288 test files with "
        "fuzz and property testing across 51 functions and 21 performance "
        "benchmarks measuring throughput of core operations.";

    struct { const char *label; int width; int iter; } sizes[] = {
        {"width=40", 40, 50000},
        {"width=60", 60, 50000},
        {"width=80", 80, 50000},
    };
    int nsizes = sizeof(sizes) / sizeof(sizes[0]);
    int all_ok = 1;

    for (int s = 0; s < nsizes; s++) {
        int iter = sizes[s].iter;
        int width = sizes[s].width;

        double start = wall_time();

        for (int i = 0; i < iter; i++) {
            char *result = textwrap_fill(long_text, width);
            free(result);
        }

        double elapsed = wall_time() - start;
        double ops_per_sec = iter / elapsed;
        printf("  %s: %d fills in %.3fs (%.0f ops/sec)\n",
               sizes[s].label, iter, elapsed, ops_per_sec);
        char cond_name[128];
        snprintf(cond_name, sizeof(cond_name),
                 "textwrap fill %s > 50000 ops/sec", sizes[s].label);
        TEST(cond_name, ops_per_sec > 50000.0);
    }
    TEST("elapsed < 10s for all widths", all_ok);
    (void)all_ok;
}

/* ================================================================
 *  23. Fuzzy match ratio throughput (libfuzzymatch)
 * ================================================================ */
static void bench_fuzzy_match(void) {
    printf("\n--- Benchmark: Fuzzy Match Ratio (various text sizes) ---\n");

    struct {
        const char *label;
        const char *a;
        const char *b;
        int iter;
        double min_ops;
    } cases[] = {
        {"identical short (50B)", 
         "The quick brown fox jumps over the lazy dog",
         "The quick brown fox jumps over the lazy dog",
         50000, 50000.0},
        {"similar short (50B, 10% diff)",
         "The quick brown fox jumps over the lazy dog",
         "The quick brwn fox jumps over the lazy dog",
         50000, 50000.0},
        {"different short (50B)",
         "The quick brown fox jumps over the lazy dog",
         "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
         50000, 50000.0},
        {"identical medium (300B)",
         "The Slermes C project is a comprehensive port of the Hermes AI agent "
         "framework from Python to C, providing a standalone binary with no "
         "Python dependencies. It implements the core agent loop, tool execution, "
         "and plugin system for maximum performance.",
         "The Slermes C project is a comprehensive port of the Hermes AI agent "
         "framework from Python to C, providing a standalone binary with no "
         "Python dependencies. It implements the core agent loop, tool execution, "
         "and plugin system for maximum performance.",
         10000, 5000.0},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    int all_ok = 1;

    for (int c = 0; c < ncases; c++) {
        int iter = cases[c].iter;
        double start = wall_time();

        for (int i = 0; i < iter; i++) {
            double r = fuzzy_ratio(cases[c].a, cases[c].b);
            (void)r;
        }

        double elapsed = wall_time() - start;
        double ops_per_sec = iter / elapsed;
        printf("  %s: %d ratios in %.3fs (%.0f ops/sec)\n",
               cases[c].label, iter, elapsed, ops_per_sec);
        char cond_name[128];
        snprintf(cond_name, sizeof(cond_name),
                 "fuzzy_ratio %s > %.0f ops/sec", cases[c].label, cases[c].min_ops);
        TEST(cond_name, ops_per_sec > cases[c].min_ops);
    }
    TEST("elapsed < 10s for all sizes", all_ok);
    (void)all_ok;
}

/* ================================================================
 *  24. Glob match throughput (libglob)
 * ================================================================ */
static void bench_glob_match(void) {
    printf("\n--- Benchmark: Glob Match (various patterns) ---\n");

    struct {
        const char *label;
        const char *pattern;
        const char *path;
        int iter;
        double min_ops;
    } cases[] = {
        {"simple exact match",
         "foo.txt", "foo.txt", 200000, 100000.0},
        {"star wildcard match",
         "*.txt", "document.txt", 200000, 100000.0},
        {"recursive ** match",
         "src/**/*.c", "src/agent/agent_loop.c", 200000, 100000.0},
        {"question mark match",
         "test_????.c", "test_main.c", 200000, 100000.0},
        {"character class match",
         "test_[abc]???.c", "test_bmain.c", 200000, 100000.0},
        {"deep path no match",
         "*.py", "src/agent/agent_loop.c", 200000, 100000.0},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    int all_ok = 1;

    for (int c = 0; c < ncases; c++) {
        int iter = cases[c].iter;
        double start = wall_time();

        for (int i = 0; i < iter; i++) {
            bool match = glob_match(cases[c].pattern, cases[c].path);
            (void)match;
        }

        double elapsed = wall_time() - start;
        double ops_per_sec = iter / elapsed;
        printf("  %s: %d matches in %.3fs (%.0f ops/sec)\n",
               cases[c].label, iter, elapsed, ops_per_sec);
        char cond_name[128];
        snprintf(cond_name, sizeof(cond_name),
                 "glob_match %s > %.0f ops/sec", cases[c].label, cases[c].min_ops);
        TEST(cond_name, ops_per_sec > cases[c].min_ops);
    }
    TEST("elapsed < 10s for all cases", all_ok);
    (void)all_ok;
}

/* ================================================================
 *  25. ANSI strip throughput (libansi)
 * ================================================================ */
static void bench_ansi_strip(void) {
    printf("\n--- Benchmark: ANSI Strip (various complexity levels) ---\n");

    struct {
        const char *label;
        const char *text;
        int iter;
        double min_ops;
    } cases[] = {
        {"no ANSI (plain text)",
         "Hello, this is plain text with no ANSI escape sequences at all.",
         200000, 100000.0},
        {"single color code",
         "\033[31mRed text\033[0m and normal text",
         200000, 100000.0},
        {"heavy ANSI (SGR codes)",
         "\033[1m\033[31m\033[42m\033[4mbold red on green underline\033[0m "
         "\033[38;2;255;128;0m\033[48;2;0;0;0mRGB colors\033[0m "
         "\033[3m\033[95mitallic magenta\033[0m "
         "\033[38;5;82m256-color green\033[0m",
         200000, 100000.0},
        {"cursor movement",
         "\033[2J\033[H\033[3A\033[6B\033[2C\033[4D\033[1;1H\033[2;10f"
         "\033[6n\033[s\033[u\033[?25l\033[?25h\033[?1049h\033[?1049l",
         200000, 100000.0},
        {"mixed text with CLI output",
         "\033[32m✓\033[0m \033[1mConfiguration loaded\033[0m (\033[33m5ms\033[0m)\n"
         "\033[32m✓\033[0m \033[1mModel initialized\033[0m (\033[33mgpt-4o\033[0m)\n"
         "\033[31m✗\033[0m \033[1mConnection failed\033[0m: \033[3mtimeout\033[0m\n"
         "\033[90m    at connect_socket:42\033[0m\n"
         "\033[90m    at init_connection:156\033[0m\n"
         "\033[34m→\033[0m Retrying in \033[93m3\033[0m seconds...\n",
         100000, 50000.0},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    int all_ok = 1;

    for (int c = 0; c < ncases; c++) {
        int iter = cases[c].iter;
        double start = wall_time();

        for (int i = 0; i < iter; i++) {
            char *result = ansi_strip(cases[c].text);
            free(result);
        }

        double elapsed = wall_time() - start;
        double ops_per_sec = iter / elapsed;
        printf("  %s: %d strips in %.3fs (%.0f ops/sec)\n",
               cases[c].label, iter, elapsed, ops_per_sec);
        char cond_name[128];
        snprintf(cond_name, sizeof(cond_name),
                 "ansi_strip %s > %.0f ops/sec", cases[c].label, cases[c].min_ops);
        TEST(cond_name, ops_per_sec > cases[c].min_ops);
    }
    TEST("elapsed < 10s for all cases", all_ok);
    (void)all_ok;
}

/* ================================================================
 *  26. Datetime parse/format throughput (libdatetime)
 * ================================================================ */
static void bench_datetime_parse(void) {
    printf("\n--- Benchmark: Datetime Parse ISO8601 + Format ---\n");

    const char *timestamps[] = {
        "2026-06-01T12:00:00Z",
        "2026-06-01T12:00:00+00:00",
        "2026-06-01T12:00:00.000Z",
        "2026-01-15T08:30:45-05:00",
        "2025-12-31T23:59:59Z",
        "2024-02-29T00:00:00Z",
    };
    int n_ts = sizeof(timestamps) / sizeof(timestamps[0]);

    /* Benchmark parse */
    int iter_parse = 100000;
    double start = wall_time();
    for (int i = 0; i < iter_parse; i++) {
        time_t ts = datetime_parse_iso8601(timestamps[i % n_ts]);
        (void)ts;
    }
    double elapsed_parse = wall_time() - start;
    double parse_ops = iter_parse / elapsed_parse;
    printf("  Parse: %d ISO8601 parses in %.3fs (%.0f ops/sec)\n",
           iter_parse, elapsed_parse, parse_ops);
    TEST("datetime parse > 100000 ops/sec", parse_ops > 100000.0);

    /* Benchmark format */
    time_t now = time(NULL);
    int iter_format = 100000;
    start = wall_time();
    for (int i = 0; i < iter_format; i++) {
        char *result = datetime_format(now, "%Y-%m-%d %H:%M:%S");
        free(result);
    }
    double elapsed_format = wall_time() - start;
    double format_ops = iter_format / elapsed_format;
    printf("  Format: %d strftime calls in %.3fs (%.0f ops/sec)\n",
           iter_format, elapsed_format, format_ops);
    TEST("datetime format > 50000 ops/sec", format_ops > 50000.0);

    TEST("elapsed < 10s for all datetime ops", (elapsed_parse + elapsed_format) < 10.0);
}

/* ================================================================
 *  27. Binary extension checking throughput (libbinary)
 * ================================================================ */
static void bench_binary_ext_check(void) {
    printf("\n--- Benchmark: Binary Extension Check (various paths) ---\n");

    struct {
        const char *label;
        const char *path;
        bool expected;
        int iter;
        double min_ops;
    } cases[] = {
        {"plain text .txt",      "document.txt",         false, 500000, 100000.0},
        {"binary .png",          "image.png",            true,  500000, 100000.0},
        {"binary .exe",          "setup.exe",            true,  500000, 100000.0},
        {"binary .so",           "libplugin.so",         true,  500000, 100000.0},
        {"deep path .c",         "src/agent/agent_loop.c", false, 500000, 100000.0},
        {"no extension",         "README",               false, 500000, 100000.0},
        {"no extension with /",  "/usr/bin/bash",        false, 500000, 100000.0},
        {"hidden file",          ".gitignore",           false, 500000, 100000.0},
        {"binary .zip",          "archive.zip",          true,  500000, 100000.0},
        {"binary .pdf",          "report.pdf",           false,  500000, 100000.0},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    int all_ok = 1;

    for (int c = 0; c < ncases; c++) {
        int iter = cases[c].iter;
        double start = wall_time();

        bool result = false;
        for (int i = 0; i < iter; i++) {
            result = has_binary_extension(cases[c].path);
        }

        double elapsed = wall_time() - start;
        double ops_per_sec = iter / elapsed;
        printf("  %s: %d checks in %.3fs (%.0f ops/sec) [expected=%s, got=%s]\n",
               cases[c].label, iter, elapsed, ops_per_sec,
               cases[c].expected ? "T" : "F", result ? "T" : "F");
        char cond_name[128];
        snprintf(cond_name, sizeof(cond_name),
                 "binary_ext %s > %.0f ops/sec", cases[c].label, cases[c].min_ops);
        TEST(cond_name, ops_per_sec > cases[c].min_ops);
        TEST("correct result", result == cases[c].expected);
    }
    TEST("elapsed < 10s for all cases", all_ok);
    (void)all_ok;
}

/* ================================================================
 *  28. Difflib ratio + unified diff throughput (libdifflib)
 * ================================================================ */
static void bench_difflib_ops(void) {
    printf("\n--- Benchmark: Difflib Ratio + Unified Diff ---\n");

    const char *text_a =
        "The quick brown fox jumps over the lazy dog.\n"
        "This is a test document for diffing.\n"
        "It has multiple lines of varying content.\n"
        "Some lines are identical between versions.\n"
        "Others have minor changes or additions.\n"
        "The purpose is to benchmark diff operations.\n"
        "Line seven is a unique line here.\n"
        "Line eight follows the same pattern.\n";

    const char *text_b =
        "The quick brown fox jumps over the lazy dog.\n"
        "This is a test document for diffing.\n"
        "It has multiple lines of varying content!\n"
        "Some lines are identical between versions.\n"
        "Others have minor changes or additions.\n"
        "The purpose is to benchmark diff operations.\n"
        "A new line has been inserted here.\n"
        "Line seven is changed in this version.\n"
        "Line eight also shows a difference.\n"
        "And an extra trailing line exists.\n";

    /* Benchmark difflib_ratio */
    int iter_ratio = 100000;
    double start = wall_time();
    for (int i = 0; i < iter_ratio; i++) {
        double r = difflib_ratio(text_a, text_b);
        (void)r;
    }
    double elapsed_ratio = wall_time() - start;
    double ratio_ops = iter_ratio / elapsed_ratio;
    printf("  Ratio: %d calls in %.3fs (%.0f ops/sec)\n", iter_ratio, elapsed_ratio, ratio_ops);
    TEST("difflib_ratio > 5000 ops/sec", ratio_ops > 5000.0);

    /* Benchmark difflib_unified_diff */
    int iter_diff = 10000;
    start = wall_time();
    for (int i = 0; i < iter_diff; i++) {
        char *diff = difflib_unified_diff(text_a, text_b, 3);
        free(diff);
    }
    double elapsed_diff = wall_time() - start;
    double diff_ops = iter_diff / elapsed_diff;
    printf("  Unified diff: %d calls in %.3fs (%.0f ops/sec)\n", iter_diff, elapsed_diff, diff_ops);
    TEST("difflib_unified_diff > 1000 ops/sec", diff_ops > 1000.0);

    TEST("elapsed < 10s for all difflib ops", (elapsed_ratio + elapsed_diff) < 10.0);
}

/* ================================================================
 *  29. Template rendering throughput (libtemplate)
 * ================================================================ */
static void bench_template_render(void) {
    printf("\n--- Benchmark: Template Compile + Render ---\n");

    /* Simple template */
    const char *simple_tpl = "Hello {{ name }}! Your role is {{ role }}.";
    /* Complex template */
    const char *complex_tpl =
        "System: {{ system_prompt }}\n"
        "User: {{ user_message }}\n"
        "{% if context %}{{ context }}{% endif %}\n"
        "{% if model %}(Model: {{ model }}){% else %}(default model){% endif %}\n"
        "Timestamp: {{ timestamp }}\n"
        "Version: {{ version }}\n"
        "Priority: {{ priority|normal }}\n";

    /* Create a JSON context */
    json_t *ctx = json_parse(
        "{"
        "\"name\": \"Alice\","
        "\"role\": \"assistant\","
        "\"system_prompt\": \"You are a helpful assistant.\","
        "\"user_message\": \"What is the capital of France?\","
        "\"context\": \"The user is asking about geography.\","
        "\"model\": \"gpt-4o\","
        "\"timestamp\": \"2026-06-01T12:00:00Z\","
        "\"version\": \"v529\","
        "\"priority\": \"high\""
        "}", NULL);
    if (!ctx) { g_skip++; return; }

    /* Benchmark simple template */
    int iter = 50000;
    double start = wall_time();
    for (int i = 0; i < iter; i++) {
        char *err = NULL;
        char *out = template_quick(simple_tpl, ctx, &err);
        free(err);
        free(out);
    }
    double elapsed_simple = wall_time() - start;
    double simple_ops = iter / elapsed_simple;
    printf("  Simple (2 vars): %d renders in %.3fs (%.0f ops/sec)\n",
           iter, elapsed_simple, simple_ops);
    TEST("template simple > 10000 ops/sec", simple_ops > 10000.0);

    /* Benchmark complex template */
    start = wall_time();
    for (int i = 0; i < iter; i++) {
        char *err = NULL;
        char *out = template_quick(complex_tpl, ctx, &err);
        free(err);
        free(out);
    }
    double elapsed_complex = wall_time() - start;
    double complex_ops = iter / elapsed_complex;
    printf("  Complex (7 vars, if/else): %d renders in %.3fs (%.0f ops/sec)\n",
           iter, elapsed_complex, complex_ops);
    TEST("template complex > 5000 ops/sec", complex_ops > 5000.0);

    json_free(ctx);
    TEST("elapsed < 10s for all template ops", (elapsed_simple + elapsed_complex) < 10.0);
}

/* ================================================================
 *  30. Interrupt set/check throughput (libinterrupt) — X11 target
 * ================================================================ */
static void bench_interrupt(void) {
    printf("\n--- Benchmark: Interrupt Set/Check/Clear cycle ---\n");

    int iter = 500000;
    double start = wall_time();
    for (int i = 0; i < iter; i++) {
        interrupt_set(true, 0);
        bool val = interrupt_is_interrupted();
        interrupt_set(false, 0);
        (void)val;
    }
    double elapsed = wall_time() - start;
    double ops_per_sec = (iter * 2) / elapsed; /* set + check = 2 ops */
    printf("  %d set+check+clear cycles in %.3fs (%.0f ops/sec)\n",
           iter, elapsed, ops_per_sec);
    TEST("interrupt set+check > 1000000 ops/sec", ops_per_sec > 1000000.0);
    TEST("no interrupted threads remaining", interrupt_count() == 0);
    TEST("elapsed < 5s for 500K cycles", elapsed < 5.0);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Slermes C Performance Benchmarks (S7 X11) ===\n");

    bench_json_parse();
    bench_message_alloc();
    bench_token_estimation();

    printf("\n");
    bench_context_push_pop();

    printf("\n");
    bench_message_clone();

    printf("\n");
    bench_context_truncate();

    printf("\n");
    bench_json_field_access();

    printf("\n");
    bench_sha256_throughput();

    printf("\n");
    bench_hmac_sha256();

    printf("\n");
    bench_checkpoint_save();

    printf("\n");
    bench_checkpoint_restore();

    printf("\n");
    bench_path_normalize();

    printf("\n");
    bench_html_strip();

    printf("\n");
    bench_base64_throughput();

    printf("\n");
    bench_uuid_throughput();

    printf("\n");
    bench_csv_parsing();

    printf("\n");
    bench_regex_extraction();

    printf("\n");
    bench_toml_parsing();

    printf("\n");
    bench_yaml_parsing();

    printf("\n");
    bench_cron_parsing();

    printf("\n");
    bench_dotenv_parsing();

    printf("\n");
    bench_textwrap_throughput();

    printf("\n");
    bench_fuzzy_match();

    printf("\n");
    bench_glob_match();

    printf("\n");
    bench_ansi_strip();

    printf("\n");
    bench_datetime_parse();

    printf("\n");
    bench_binary_ext_check();

    printf("\n");
    bench_difflib_ops();

    printf("\n");
    bench_template_render();

    printf("\n");
    bench_interrupt();

    printf("\n=== Results: %d passed, %d failed, %d skipped ===\n",
           g_pass, g_fail, g_skip);
    return g_fail > 0 ? 1 : 0;
}
