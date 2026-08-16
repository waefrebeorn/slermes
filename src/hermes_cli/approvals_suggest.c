/*
 * port_approvals_suggest.c — Faithful C11 port of
 * hermes_cli/approvals_suggest.py (the DB-scan + CLI dispatch layer).
 *
 * Ports: default_db_path, _connect_readonly, _iter_terminal_calls,
 *   _blocked_tool_call_ids, scan_approval_history, apply_proposals,
 *   _render_text, suggest_command, approvals_command.
 *
 * Reuses:
 *   - build_proposals() + proposal_*  (port_approvals_suggest_proposals.c)
 *   - approval_normalize_command(), approval_is_unsafe_class(),
 *     approval_derive_glob(), approval_detect_dangerous_command(),
 *     approval_detect_hardline_command()
 *   - save_permanent_allowlist() / load_permanent_allowlist()
 *   - slermes_home() + SLERMES_FILE_STATE_DB
 *
 * Single cohesive PoP of ONE Python module (approvals_suggest.py).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sqlite3.h>

#include "slermes_home.h"
#include "port_approvals_suggest_proposals.h"
#include "approval.h"

/* ---- records: array of (command, description) ---- */
typedef struct { char *command; char *description; } as_record_t;
typedef struct { as_record_t *items; size_t n; size_t cap; } as_records_t;

static void as_records_append(as_records_t *r, const char *cmd, const char *desc) {
    if (r->n >= r->cap) {
        size_t nc = r->cap ? r->cap * 2 : 16;
        as_record_t *tmp = (as_record_t *)realloc(r->items, nc * sizeof(*tmp));
        if (!tmp) return;
        r->items = tmp; r->cap = nc;
    }
    r->items[r->n].command = strdup(cmd ? cmd : "");
    r->items[r->n].description = strdup(desc ? desc : "");
    r->n++;
}
static void as_records_free(as_records_t *r) {
    for (size_t i = 0; i < r->n; i++) {
        free(r->items[i].command); free(r->items[i].description);
    }
    free(r->items); r->items = NULL; r->n = r->cap = 0;
}

/* ---- _block markers (mirrors _BLOCK_MARKERS, checked via substring) ---- */
static const char *_k_block_markers[] = {
    "BLOCKED (hardline)", "BLOCKED: User denied", "BLOCKED: Action ",
    "BLOCKED: Command flagged as dangerous", "BLOCKED: approval required",
    "BLOCKED: Failed to send approval request", "The user has NOT consented",
    "Asking the user for approval", "approval_required",
    "BLOCKED by user deny rule", NULL
};
static bool _is_blocked_result(const char *content) {
    if (!content) return false;
    for (int i = 0; _k_block_markers[i]; i++)
        if (strstr(content, _k_block_markers[i])) return true;
    return false;
}

/* ---- blocked set (NULL-terminated array of tool_call_id) ---- */
typedef struct { char **items; size_t n; size_t cap; } as_set_t;
static void as_set_append(as_set_t *s, const char *id) {
    if (s->n >= s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8;
        char **tmp = (char **)realloc(s->items, (nc + 1) * sizeof(*s->items));
        if (!tmp) return;
        s->items = tmp; s->cap = nc;
    }
    s->items[s->n++] = strdup(id);
    s->items[s->n] = NULL;
}
static bool as_set_contains(as_set_t *s, const char *id) {
    if (!s || !id || !*id) return false;
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->items[i], id) == 0) return true;
    return false;
}
static void as_set_free(as_set_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items); s->items = NULL; s->n = s->cap = 0;
}

/* PoP: asuggest_default_db_path @ hermes_cli/approvals_suggest.py:default_db_path */
char *asuggest_default_db_path(void) {
    const char *home = slermes_home();
    if (!home) return NULL;
    char *path = NULL;
    if (asprintf(&path, "%s/%s", home, SLERMES_FILE_STATE_DB) < 0) return NULL;
    return path;
}

/* PoP: asuggest_connect_readonly @ hermes_cli/approvals_suggest.py:_connect_readonly */
sqlite3 *asuggest_connect_readonly(const char *db_path) {
    sqlite3 *db = NULL;
    char uri[2048];
    snprintf(uri, sizeof(uri), "file:%s?mode=ro", db_path);
    if (sqlite3_open_v2(uri, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    sqlite3_busy_timeout(db, 5000);
    return db;
}

/* PoP: asuggest_blocked_tool_call_ids @ hermes_cli/approvals_suggest.py:_blocked_tool_call_ids */
/* Returns 0 ok (set filled); fills the caller-owned set. Non-zero on sqlite error. */
int asuggest_blocked_tool_call_ids(sqlite3 *db, double since_ts, as_set_t *out) {
    static const char SQL[] =
        "SELECT tool_call_id, content FROM messages "
        "WHERE role='tool' AND tool_call_id IS NOT NULL AND timestamp >= ? "
        "AND (content LIKE '%BLOCKED%' OR content LIKE '%approval%')";
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db, SQL, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_double(st, 1, since_ts);
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *tcid = sqlite3_column_text(st, 0);
        const unsigned char *content = sqlite3_column_text(st, 1);
        if (!tcid || !content) continue;
        if (!_is_blocked_result((const char *)content)) continue;
        as_set_append(out, (const char *)tcid);
    }
    sqlite3_finalize(st);
    return 0;
}

/* Parse one tool_calls JSON blob; yield (id, command) for terminal calls.
 * Calls cb(ctx, tool_call_id, command); nonzero return aborts. */
typedef int (*as_terminal_cb)(void *ctx, const char *tool_call_id, const char *command);

/* PoP: asuggest_iter_terminal_calls @ hermes_cli/approvals_suggest.py:_iter_terminal_calls */
int asuggest_iter_terminal_calls(sqlite3 *db, double since_ts, as_terminal_cb cb, void *ctx) {
    static const char SQL[] =
        "SELECT tool_calls FROM messages "
        "WHERE role='assistant' AND tool_calls IS NOT NULL "
        "AND tool_calls LIKE '%terminal%' AND timestamp >= ?";
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db, SQL, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_double(st, 1, since_ts);
    int stop = 0;
    while (!stop && sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *raw = sqlite3_column_text(st, 0);
        if (!raw) continue;
        json_t *arr = json_parse((const char *)raw, NULL);
        if (!arr) continue;
        for (size_t i = 0; ; i++) {
            json_t *call = json_get(arr, i);
            if (!call) break;
            json_t *fn = json_obj_get(call, "function");
            if (!fn) continue;
            const char *fname = json_object_get_string(fn, "name", "");
            if (strcmp(fname, "terminal") != 0) continue;
            const char *tc_id = json_object_get_string(call, "id", "");
            const char *args_str = json_object_get_string(fn, "arguments", "{}");
            json_t *args = json_parse(args_str, NULL);
            const char *command = args ? json_object_get_string(args, "command", "") : "";
            if (command && *command) {
                int cbr = cb(ctx, tc_id, command);
                if (cbr != 0) { stop = 1; break; }
            }
            if (args) json_free(args);
        }
        json_free(arr);
    }
    sqlite3_finalize(st);
    return stop ? -1 : 0;
}

/* ---- callback context for scan ---- */
typedef struct { as_records_t *recs; as_set_t *blocked; } as_scan_ctx_t;

/* PoP: _record_denial is in port_approval_helpers2.c; reused here only if needed.
 * scan_approval_history: mine implied approvals from session DB. */
static int _scan_on_terminal(void *ctxp, const char *tc_id, const char *command) {
    as_scan_ctx_t *ctx = (as_scan_ctx_t *)ctxp;
    if (as_set_contains(ctx->blocked, tc_id)) return 0;
    char *hard = approval_detect_hardline_command(command);
    if (hard) {
        /* "true\t<desc>" means hardline; free and skip */
        bool is_hardline = (hard[0] == 't' && hard[1] == 'r' && hard[2] == 'u' && hard[3] == 'e');
        free(hard);
        if (is_hardline) return 0;
    }
    char *det = apr_detect_dangerous_command(command);
    if (!det) return 0;
    /* det = "true\t<key>\t<description>" or "false\t\t" */
    const char *description = "";
    if (det[0] == 't' && det[1] == 'r' && det[2] == 'u' && det[3] == 'e') {
        char *tab1 = strchr(det, '\t');
        char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
        description = (tab2 && *(tab2 + 1)) ? (tab2 + 1) : "";
    }
    as_records_append(ctx->recs, command, description);
    free(det);
    return 0;
}

/* PoP: asuggest_scan_approval_history @ hermes_cli/approvals_suggest.py:scan_approval_history */
int asuggest_scan_approval_history(const char *db_path, int days,
                                   as_records_t *out) {
    if (!out) return -1;
    out->items = NULL; out->n = out->cap = 0;
    char *real_path = NULL;
    if (db_path && *db_path) real_path = strdup(db_path);
    else real_path = asuggest_default_db_path();
    if (!real_path) return 0; /* empty — no home */
    sqlite3 *db = asuggest_connect_readonly(real_path);
    if (!db) { free(real_path); return 0; /* empty: db not found */ }
    double since_ts = (days <= 0) ? 0.0 : (double)time(NULL) - (double)days * 86400.0;
    as_set_t blocked = {0};
    asuggest_blocked_tool_call_ids(db, since_ts, &blocked);
    as_scan_ctx_t ctx = { out, &blocked };
    asuggest_iter_terminal_calls(db, since_ts, _scan_on_terminal, &ctx);
    as_set_free(&blocked);
    sqlite3_close(db);
    free(real_path);
    return 0;
}

/* ---- args shim ---- */
typedef struct {
    const char *db;
    int         days;
    int         min_count;
    int         limit;
    const char *apply_indices;
    bool        json_out;
} as_suggest_args_t;

/* (parse_apply_indices is declared in approval.h; reused from tools/approval.c) */

/* PoP: asuggest_apply_proposals @ hermes_cli/approvals_suggest.py:apply_proposals */
int asuggest_apply_proposals(proposal_t **props, size_t n, const int *indices, int n_idx,
                             char ***out_merged, size_t *out_n) {
    if (out_merged) *out_merged = NULL;
    if (out_n) *out_n = 0;
    size_t pn = 0; char **existing = approval_permanent_allowlist_list(&pn);

    char **merged = NULL; size_t mn = 0, mc = 0;
    for (size_t i = 0; i < pn; i++) {
        if (mn >= mc) { mc = mc ? mc * 2 : 8; char **t = realloc(merged, (mc+1)*sizeof(*merged)); if (!t) break; merged = t; }
        merged[mn++] = strdup(existing[i]);
    }
    for (size_t i = 0; i < pn; i++) free(existing[i]);
    free(existing);

    for (int i = 0; i < n_idx; i++) {
        int idx = indices[i];
        if (idx < 0 || (size_t)idx >= n) continue;
        const char *pattern = proposal_pattern(props[idx]);
        if (!pattern) continue;
        bool dup = false;
        for (size_t j = 0; j < mn; j++) if (strcmp(merged[j], pattern) == 0) { dup = true; break; }
        if (!dup) {
            if (mn >= mc) { mc = mc ? mc * 2 : 8; char **t = realloc(merged, (mc+1)*sizeof(*merged)); if (!t) break; merged = t; }
            merged[mn++] = strdup(pattern);
        }
    }
    if (merged) merged[mn] = NULL;
    save_permanent_allowlist((const char **)merged, mn);
    approval_load_permanent((const char **)merged, mn);
    if (out_merged) *out_merged = merged;
    if (out_n) *out_n = mn;
    return 0;
}

/* PoP: asuggest_render_text @ hermes_cli/approvals_suggest.py:_render_text */
void asuggest_render_text(proposal_t **props, size_t n, int days) {
    const char *window = (days <= 0) ? "all history" : NULL;
    char *winbuf = NULL;
    if (window == NULL) {
        asprintf(&winbuf, "last %d days", days);
        window = winbuf;
    }
    if (n == 0) {
        printf("No allowlist candidates found in approval history (%s).\n"
               "Either nothing dangerous was approved often enough "
               "(see --min-count/--days), or the approved classes are "
               "excluded for safety.\n", window);
        free(winbuf);
        return;
    }
    printf("Proposed command_allowlist additions (from approval history, %s):\n\n", window);
    for (size_t i = 0; i < n; i++) {
        const char *kind = proposal_kind(props[i]);
        printf("  %zu. %s    — approved %dx%s\n", i + 1,
               proposal_pattern(props[i]), proposal_count(props[i]),
               (kind && strcmp(kind, "class") == 0) ? " (class key)" : "");
        size_t clen = 0; const char **classes = proposal_classes(props[i], &clen);
        for (size_t j = 0; j < clen; j++) printf("       class: %s\n", classes[j]);
        size_t elen = 0; const char **examples = proposal_examples(props[i], &elen);
        for (size_t j = 0; j < elen; j++) printf("       e.g. %s\n", examples[j]);
    }
    printf("\nNothing has been changed. Apply selected entries with:\n"
           "  hermes approvals suggest --apply 1,3\n"
           "Entries are merged into command_allowlist in ~/.hermes/config.yaml.\n");
    free(winbuf);
}

/* PoP: asuggest_suggest_command @ hermes_cli/approvals_suggest.py:suggest_command */
int asuggest_suggest_command(const as_suggest_args_t *args) {
    const char *db_path = args ? args->db : NULL;
    int days = args ? (args->days ? args->days : 90) : 90;
    int min_count = args ? (args->min_count ? args->min_count : 2) : 2;
    int limit = args ? (args->limit ? args->limit : 20) : 20;
    bool json_out = args ? args->json_out : false;
    const char *apply_spec = args ? args->apply_indices : NULL;

    char *real_db = NULL;
    if (db_path && *db_path) real_db = strdup(db_path);
    else real_db = asuggest_default_db_path();
    if (!real_db) { fprintf(stderr, "Session database not found\n"); return 1; }

    as_records_t recs;
    asuggest_scan_approval_history(real_db, days, &recs);
    const char *db_str = real_db;  /* for JSON output (freed at done) */

    /* existing allowlist → exclusion set */
    size_t pn = 0; char **existing = approval_permanent_allowlist_list(&pn);

    size_t n_props = 0;
    /* build_proposals takes records as const char*** pairs */
    const char ***pairs = NULL;
    if (recs.n) {
        pairs = (const char ***)malloc(recs.n * sizeof(*pairs));
        if (pairs) for (size_t i = 0; i < recs.n; i++) {
            pairs[i] = (const char **)malloc(2 * sizeof(char *));
            pairs[i][0] = recs.items[i].command;
            pairs[i][1] = recs.items[i].description;
        }
    }
    proposal_t **props = build_proposals(pairs, recs.n,
                                         (const char **)existing, pn,
                                         min_count, limit, &n_props);
    free(pairs);

    int rc = 0;
    if (apply_spec) {
        int indices[64];
        int nidx = approval_parse_apply_indices(apply_spec, (int)n_props,
                                                indices, 64);
        if (nidx < 0) {
            printf("--apply error: invalid --apply spec %s\n", apply_spec);
            rc = 1; goto done;
        }
        char **merged = NULL; size_t merged_n = 0;
        asuggest_apply_proposals(props, n_props, indices, nidx, &merged, &merged_n);
        char **applied = (char **)malloc((nidx + 1) * sizeof(char *));
        int ai = 0;
        for (int i = 0; i < nidx; i++)
            if (indices[i] >= 0 && (size_t)indices[i] < n_props)
                applied[ai++] = strdup(proposal_pattern(props[indices[i]]));
        applied[ai] = NULL;
        if (json_out) {
            printf("[");
            for (int i = 0; i < ai; i++) {
                printf(i ? ", \"%s\"" : "\"%s\"", applied[i]);
            }
            printf("], {\"applied\":[");
            for (int i = 0; i < ai; i++) printf(i ? ",\"%s\"" : "\"%s\"", applied[i]);
            printf("],\"allowlist_size\":%zu}\n", merged_n);
        } else {
            printf("Added to command_allowlist:\n");
            for (int i = 0; i < ai; i++) printf("  + %s\n", applied[i]);
            printf("\ncommand_allowlist now has %zu entries "
                   "(~/.hermes/config.yaml).\n", merged_n);
        }
        for (int i = 0; i < ai; i++) free(applied[i]);
        free(applied);
        free(merged);
    } else if (json_out) {
        printf("{\"db\": \"%s\", \"days\": %d, \"proposals\": [", db_str, days);
        /* re-open to read db path — simpler: just emit path passed */
        for (size_t i = 0; i < n_props; i++) {
            printf(i ? ",\n      {" : "\n      {");
            printf("\"n\": %zu, \"pattern\": \"%s\", \"kind\": \"%s\", \"count\": %d",
                   i + 1, proposal_pattern(props[i]),
                   proposal_kind(props[i]) ? proposal_kind(props[i]) : "",
                   proposal_count(props[i]));
            printf(", \"classes\": [");
            size_t cl = 0; const char **cs = proposal_classes(props[i], &cl);
            for (size_t j = 0; j < cl; j++) printf(j ? ", \"%s\"" : "\"%s\"", cs[j]);
            printf("], \"examples\": [");
            size_t el = 0; const char **ex = proposal_examples(props[i], &el);
            for (size_t j = 0; j < el; j++) printf(j ? ", \"%s\"" : "\"%s\"", ex[j]);
            printf("]}");
        }
        printf("\n    ]}\n");
    } else {
        asuggest_render_text(props, n_props, days);
    }
done:
    for (size_t i = 0; i < n_props; i++) proposal_free(props[i]);
    free(props);
    as_records_free(&recs);
    free(real_db);
    return rc;
}

/* PoP: asuggest_approvals_command @ hermes_cli/approvals_suggest.py:approvals_command */
int asuggest_approvals_command(const char *sub, const as_suggest_args_t *args) {
    if (!sub || !*sub || strcmp(sub, "suggest") == 0) return asuggest_suggest_command(args);
    /* 'test' subcommand (hermes_cli.approvals_test.approvals_test_command) is
     * a separate port — not in scope here; surface a stub line. */
    printf("hermes approvals %s — (see approvals suggest)\n", sub);
    return 1;
}

/* PoP: approvals_test_render_text @ hermes_cli/approvals_test.py:_render_text */
/* Returns malloc'd text representation of a verdict. verdict_json = {"command":...,"env_type":...,"verdict":...,"exit_code":...,"rule":...,"detail":...,"normalized_variants":[...]}. Caller frees. */
char *approvals_test_render_text(const char *verdict_json) {
    if (!verdict_json) return strdup("");
    char *err = NULL;
    json_t *v = json_parse(verdict_json, &err);
    if (err || !v || v->type != JSON_OBJECT) { if (err) free(err); if (v) json_free(v); return strdup(""); }
    json_t *command = json_obj_get(v, "command");
    json_t *env_type = json_obj_get(v, "env_type");
    json_t *verdict = json_obj_get(v, "verdict");
    json_t *exit_code = json_obj_get(v, "exit_code");
    json_t *rule = json_obj_get(v, "rule");
    json_t *detail = json_obj_get(v, "detail");
    json_t *variants = json_obj_get(v, "normalized_variants");
    char *result = NULL;
    char *line = NULL;
    asprintf(&line, "command : %s\n", command && command->type == JSON_STRING ? command->str_val : "");
    result = line;
    asprintf(&line, "%senv-type: %s\n", result ? result : "", env_type && env_type->type == JSON_STRING ? env_type->str_val : "");
    free(result); result = line;
    asprintf(&line, "%sverdict : %s  (exit %ld)\n", result ? result : "",
             verdict && verdict->type == JSON_STRING ? verdict->str_val : "",
             exit_code && exit_code->type == JSON_NUMBER ? (long)exit_code->num_val : 0);
    free(result); result = line;
    if (rule && rule->type == JSON_STRING && rule->str_val[0]) {
        asprintf(&line, "%srule    : %s\n", result ? result : "", rule->str_val);
        free(result); result = line;
    }
    if (detail && detail->type == JSON_STRING && detail->str_val[0]) {
        asprintf(&line, "%sdetail  : %s\n", result ? result : "", detail->str_val);
        free(result); result = line;
    }
    asprintf(&line, "%snormalized trace (variants the detectors evaluated):\n", result ? result : "");
    free(result); result = line;
    if (variants && variants->type == JSON_ARRAY) {
        for (size_t i = 0; i < variants->c.count; i++) {
            if (variants->c.items[i] && variants->c.items[i]->type == JSON_STRING) {
                asprintf(&line, "%s  - %s\n", result ? result : "", variants->c.items[i]->str_val);
                free(result); result = line;
            }
        }
    }
    json_free(v);
    return result ? result : strdup("");
}
