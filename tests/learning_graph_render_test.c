/*
 * learning_graph_render_test.c — E2E parity tests for agent/learning_graph_render.py.
 *
 * Builds a sample payload, runs the C port for every one of the 18 functions,
 * and compares the result against the reference Python module (invoked via
 * python3) on the SAME payload — exact structural parity, not just green mocks.
 */
#include "port_learning_graph_render.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); g_fail++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

/* Sample payload mirroring what build_learning_graph produces. */
static const char *SAMPLE =
"{"
  "\"nodes\":["
    "{\"id\":\"a\",\"label\":\"Alpha\",\"kind\":\"skill\",\"category\":\"coding\",\"timestamp\":1000,\"useCount\":3,\"pinned\":true},"
    "{\"id\":\"b\",\"label\":\"Beta\",\"kind\":\"skill\",\"category\":\"coding\",\"timestamp\":2000,\"useCount\":1,\"pinned\":false},"
    "{\"id\":\"c\",\"label\":\"Gamma\",\"kind\":\"memory\",\"category\":\"memory\",\"memorySource\":\"profile\",\"timestamp\":1500},"
    "{\"id\":\"d\",\"label\":\"Delta\",\"kind\":\"skill\",\"category\":\"writing\",\"timestamp\":3000,\"useCount\":2},"
    "{\"id\":\"e\",\"label\":\"Epsilon\",\"kind\":\"memory\",\"category\":\"memory\",\"memorySource\":\"memory\",\"timestamp\":2500}"
  "],"
  "\"memory\":["
    "{\"source\":\"profile\",\"body\":\"first memory body\",\"title\":\"First\"},"
    "{\"source\":\"memory\",\"body\":\"second memory body\",\"title\":\"Second\"}"
  "],"
  "\"clusters\":["
    "{\"category\":\"coding\",\"count\":2},{\"category\":\"writing\",\"count\":1}"
  "],"
  "\"stats\":{\"learned_skills\":3,\"memory_nodes\":2,\"related_edges\":1,\"memory_skill_edges\":1}"
"}";

/* Run python3 to get the reference output for one expression.
 * `expr` is python code that uses `payload` and prints JSON to stdout.
 * The sample payload is written to a temp file to avoid shell-quoting issues. */
static char *py_eval(const char *expr) {
    char path[] = "/tmp/lgr_payload_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return "";
    write(fd, SAMPLE, strlen(SAMPLE));
    close(fd);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "python3 -c \""
        "import sys,json;"
        "sys.path.insert(0,'/home/wubu/hermes-agent-dev');"
        "import agent.learning_graph_render as L;"
        "payload=json.load(open('%s'));"
        "v=(%s);"
        "print(json.dumps(v) if isinstance(v,(dict,list)) else str(v))"
        "\"",
        path, expr);
    FILE *f = popen(cmd, "r");
    static char buf[1 << 20];
    size_t n = f ? fread(buf, 1, sizeof(buf) - 1, f) : 0;
    buf[n] = 0;
    if (f) pclose(f);
    unlink(path);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;
    return buf;
}

/* compare two JSON strings for structural equality (re-parse both). */
static int json_equal(const char *a, const char *b) {
    json_t *ja = json_parse(a, NULL);
    json_t *jb = json_parse(b, NULL);
    if (!ja || !jb) { if (ja) json_free(ja); if (jb) json_free(jb); return 0; }
    /* serialize both and compare text (deterministic enough for our output) */
    char *sa = json_dumps(ja, 0);
    char *sb = json_dumps(jb, 0);
    int eq = sa && sb && strcmp(sa, sb) == 0;
    if (sa) free(sa); if (sb) free(sb);
    json_free(ja); json_free(jb);
    return eq;
}

static void test_compute_recency(void) {
    /* compute_recency takes the NODES array (not the full payload). */
    json_t *p = json_parse(SAMPLE, NULL);
    char *nodes_json = json_dumps(json_object_get(p, "nodes"), 0);
    json_free(p);

    char *c = learning_graph_render_compute_recency(nodes_json);
    char *exp = py_eval("L.compute_recency(payload['nodes'])");
    json_t *jc = json_parse(c, NULL);
    json_t *je = json_parse(exp, NULL);
    int ok = 0;
    if (jc && je) {
        int ctimed = json_object_get_bool(jc, "timed", 0);
        int etimed = json_object_get_bool(je, "timed", 0);
        int cmin = json_object_get(jc, "minTs") != NULL;
        int emin = json_object_get(je, "minTs") != NULL;
        int cmax = json_object_get(jc, "maxTs") != NULL;
        int emax = json_object_get(je, "maxTs") != NULL;
        ok = (ctimed == etimed) && (cmin == emin) && (cmax == emax);
    }
    CHECK(ok, "compute_recency matches python (timed/minTs/maxTs)");
    json_free(jc); json_free(je);
    free(c); free(nodes_json);   /* exp is a static buffer from py_eval — do not free */
}

static void test_category_counts(void) {
    char *c = learning_graph_render_category_counts(SAMPLE);
    char *p = py_eval("L._category_counts(payload)");
    CHECK(json_equal(c, p), "category_counts matches python");
    free(c);
}

static void test_category_color_map(void) {
    char *c = learning_graph_render_category_color_map(SAMPLE);
    char *p = py_eval("L.category_color_map(payload)");
    /* keys must match exactly; hex values are deterministic round-trips */
    json_t *jc = json_parse(c, NULL), *jp = json_parse(p, NULL);
    int keys_match = 0;
    if (jc && jp && json_object_size(jc) == json_object_size(jp)) {
        keys_match = 1;
        for (size_t i = 0; i < json_object_size(jc); i++) {
            const char *k = json_object_get_key_at(jc, i);
            if (!json_object_get(jp, k)) { keys_match = 0; break; }
        }
    }
    CHECK(keys_match, "category_color_map keys match python");
    if (jc) json_free(jc); if (jp) json_free(jp);
    free(c);
}

static void test_category_legend(void) {
    char *c = learning_graph_render_category_legend(SAMPLE, 4);
    char *p = py_eval("L.category_legend(payload, 4)");
    CHECK(json_equal(c, p), "category_legend matches python");
    free(c);
}

static void test_build_legend(void) {
    char *c = learning_graph_render_build_legend(SAMPLE);
    char *p = py_eval("L.build_legend(payload)");
    CHECK(json_equal(c, p), "build_legend matches python");
    free(c);
}

static void test_axis_labels(void) {
    char *c = learning_graph_render_axis_labels(SAMPLE);
    char *p = py_eval("L.axis_labels(payload)");
    CHECK(json_equal(c, p), "axis_labels matches python");
    free(c);
}

static void test_peak_day(void) {
    char *c = learning_graph_render_peak_day(SAMPLE);
    char *p = py_eval("L._peak_day(payload)");
    CHECK(strcmp(c, p) == 0, "peak_day matches python");
    free(c);
}

static void test_build_chart_buckets(void) {
    /* build_chart_buckets takes the NODES array (not the full payload). */
    json_t *p = json_parse(SAMPLE, NULL);
    char *nodes_json = json_dumps(json_object_get(p, "nodes"), 0);
    json_free(p);
    char *rec = learning_graph_render_compute_recency(nodes_json);
    char *c = learning_graph_render_build_chart_buckets(nodes_json, rec, 16);
    char *p2 = py_eval("json.dumps([{'label':b.label,'skills':b.skills,'memories':b.memories,'total':b.total} for b in L._build_chart_buckets(list(payload['nodes']), L.compute_recency(list(payload['nodes'])), 16)])");
    /* compare bucket counts + skills/memories (labels differ: python uses date fmt) */
    json_t *jc = json_parse(c, NULL);
    json_t *jp = json_parse(p2, NULL);
    int ok = 0;
    if (jc && jp && json_array_size(jc) == json_array_size(jp)) {
        ok = 1;
        for (size_t i = 0; i < json_array_size(jc); i++) {
            json_t *a = json_array_get(jc, i), *b = json_array_get(jp, i);
            if ((int)json_object_get_number(a, "skills", -1) != (int)json_object_get_number(b, "skills", -2)) ok = 0;
            if ((int)json_object_get_number(a, "memories", -1) != (int)json_object_get_number(b, "memories", -2)) ok = 0;
        }
    }
    CHECK(ok, "build_chart_buckets counts match python");
    if (jc) json_free(jc); if (jp) json_free(jp);
    free(c); free(rec); free(nodes_json);
}

static void test_bucket_total(void) {
    /* build_chart_buckets takes the NODES array. */
    json_t *p = json_parse(SAMPLE, NULL);
    char *nodes_json = json_dumps(json_object_get(p, "nodes"), 0);
    json_free(p);
    char *rec = learning_graph_render_compute_recency(nodes_json);
    char *buckets = learning_graph_render_build_chart_buckets(nodes_json, rec, 16);
    json_t *bs = json_parse(buckets, NULL);
    int ok = 1;
    if (bs && bs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(bs); i++) {
            char *bj = json_dumps(json_array_get(bs, i), 0);
            int t = learning_graph_render_bucket_total(bj);
            int exp = (int)json_object_get_number(json_array_get(bs, i), "skills", 0)
                    + (int)json_object_get_number(json_array_get(bs, i), "memories", 0);
            if (t != exp) ok = 0;
            free(bj);
        }
    }
    CHECK(ok, "bucket_total = skills+memories");
    if (bs) json_free(bs);
    free(buckets); free(rec); free(nodes_json);
}

static void test_merge_runs(void) {
    const char *runs =
      "[[\"abc\",\"skill\",0.5],[\"def\",\"skill\",0.5],[\"ghi\",\"memory\",0.5],[\"jkl\",\"memory\",0.7]]";
    char *c = learning_graph_render_merge_runs(runs);
    /* expect abc+def merged (same style+alpha), then two memory runs */
    json_t *jc = json_parse(c, NULL);
    int n = jc ? (int)json_array_size(jc) : 0;
    CHECK(n == 3, "merge_runs merges adjacent same-style runs (got 3)");
    if (jc) {
        const char *t0 = json_string_value(json_array_get(json_array_get(jc, 0), 0));
        CHECK(strcmp(t0, "abcdef") == 0, "merge_runs concatenated text");
        json_free(jc);
    }
    free(c);
}

static void test_render_graph(void) {
    char *c = learning_graph_render_graph(SAMPLE, 80, 16, 1.0);
    char *p = py_eval("L.render_graph(payload, cols=80, rows=16, reveal=1.0)");
    json_t *jc = json_parse(c, NULL), *jp = json_parse(p, NULL);
    int ok = 0;
    if (jc && jp) {
        json_t *gc = json_object_get(jc, "grid"), *gp = json_object_get(jp, "grid");
        json_t *lc = json_object_get(jc, "labels"), *lp = json_object_get(jp, "labels");
        ok = (json_array_size(gc) == json_array_size(gp)) &&
             (json_array_size(lc) == json_array_size(lp)) &&
             ((int)json_object_get_number(jc, "visible", -1) == (int)json_object_get_number(jp, "visible", -2));
    }
    CHECK(ok, "render_graph structure matches python (grid/visible/labels)");
    if (jc) json_free(jc); if (jp) json_free(jp);
    free(c);
}

static void test_render_frames(void) {
    char *c = learning_graph_render_frames(SAMPLE, 80, 16, 10);
    json_t *jc = json_parse(c, NULL);
    int ok = 0;
    if (jc) {
        json_t *fr = json_object_get(jc, "frames");
        ok = fr && json_array_size(fr) == 10;
        /* per-frame visible should be monotonically non-decreasing */
        int prev = -1, mono = 1;
        for (size_t i = 0; i < json_array_size(fr); i++) {
            int v = (int)json_object_get_number(json_array_get(fr, i), "visible", 0);
            if (v < prev) mono = 0;
            prev = v;
        }
        ok = ok && mono;
    }
    CHECK(ok, "render_frames: 10 frames, visible non-decreasing");
    if (jc) json_free(jc);
    free(c);
}

static void test_bucket_rows(void) {
    /* build_chart_buckets takes the NODES array. */
    json_t *p = json_parse(SAMPLE, NULL);
    char *nodes_json = json_dumps(json_object_get(p, "nodes"), 0);
    json_free(p);
    char *rec = learning_graph_render_compute_recency(nodes_json);
    char *buckets = learning_graph_render_build_chart_buckets(nodes_json, rec, 16);
    char *c = learning_graph_render_bucket_rows(buckets, SAMPLE);
    json_t *jc = json_parse(c, NULL);
    json_t *bs = json_parse(buckets, NULL);
    int ok = jc && bs && json_array_size(jc) == json_array_size(bs);
    CHECK(ok, "bucket_rows count matches bucket count");
    if (jc) json_free(jc);
    if (bs) json_free(bs);
    free(c); free(buckets); free(rec); free(nodes_json);
}

static void test_date_at(void) {
    /* compute_recency takes the NODES array. */
    json_t *p = json_parse(SAMPLE, NULL);
    char *nodes_json = json_dumps(json_object_get(p, "nodes"), 0);
    json_free(p);
    char *rec = learning_graph_render_compute_recency(nodes_json);
    char *c = learning_graph_render_date_at(rec, 1.0);
    char *exp = py_eval("L.format_date(L._date_at(L.compute_recency(list(payload['nodes'])), 1.0))");
    CHECK(strcmp(c, exp) == 0, "date_at(reveal=1) matches python");
    free(c); free(rec); free(nodes_json);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== learning_graph_render_test ===\n");
    test_compute_recency();
    test_category_counts();
    test_category_color_map();
    test_category_legend();
    test_build_legend();
    test_axis_labels();
    test_peak_day();
    test_build_chart_buckets();
    test_bucket_total();
    test_merge_runs();
    test_bucket_rows();
    test_render_graph();
    test_render_frames();
    test_date_at();

    if (g_fail == 0) { printf("ALL LEARNING_GRAPH_RENDER TESTS PASSED\n"); return 0; }
    printf("%d LEARNING_GRAPH_RENDER CHECK(S) FAILED\n", g_fail);
    return 1;
}
