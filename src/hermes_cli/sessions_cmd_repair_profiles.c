/*
 * sessions_cmd_repair_profiles.c -- real C11 port of
 * hermes_cli/sessions_cmd_repair_profiles.py.
 *
 * PoP: w22_sessionscm_632617_kind_label @ hermes_cli/sessions_cmd_repair_profiles.py:_KIND_LABELS
 * PoP: w22_sessionscm_632617_group @ hermes_cli/sessions_cmd_repair_profiles.py:_group
 * PoP: w22_sessionscm_632617_report @ hermes_cli/sessions_cmd_repair_profiles.py:_print_report
 * PoP: w22_sessionscm_632617_run @ hermes_cli/sessions_cmd_repair_profiles.py:cmd_repair_profiles
 *
 * The stores, the scan (`enumerate_stores` / `live_gateway_homes` /
 * `scan_stores` / `plan.apply`) and the `[y/N]` prompt are the seams and are
 * injected; the command's branches, text and exit codes are ported. Output is
 * byte-identical to Python's print() stream, non-ASCII glyphs included, and
 * `json.dumps(..., indent=2)` is reproduced by hand (ensure_ascii=True).
 */
#include "hermes_json.h"
#include "noc_w22_sessionscm_632617.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* glyphs, as UTF-8 bytes, so this source stays ASCII */
#define SC_ARROW   "\xe2\x86\x92" /* U+2192 RIGHTWARDS ARROW */
#define SC_CROSS   "\xe2\x9c\x97" /* U+2717 BALLOT X         */
#define SC_CHECK   "\xe2\x9c\x93" /* U+2713 CHECK MARK       */
#define SC_EMDASH  "\xe2\x80\x94" /* U+2014 EM DASH          */

/* --- buffer -------------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int oom;
} scb;

static void sc_reserve(scb *b, size_t extra) {
    size_t need;
    char *nb;
    if (b->oom) return;
    need = b->len + extra + 1;
    if (need <= b->cap) return;
    while (b->cap < need) b->cap = b->cap ? b->cap * 2 : 256;
    nb = (char *)realloc(b->buf, b->cap);
    if (!nb) { b->oom = 1; return; }
    b->buf = nb;
}

static void sc_putc(scb *b, char c) {
    sc_reserve(b, 1);
    if (b->oom) return;
    b->buf[b->len++] = c;
    b->buf[b->len] = '\0';
}

static void sc_add(scb *b, const char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    sc_reserve(b, n);
    if (b->oom) return;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void sc_int(scb *b, long long v) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%lld", v);
    sc_add(b, tmp);
}

static void sc_printf(scb *b, const char *fmt, ...) {
    char stack[512];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(stack, sizeof(stack), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(stack)) { sc_add(b, stack); return; }
    {
        char *heap = (char *)malloc((size_t)n + 1);
        if (!heap) { b->oom = 1; return; }
        va_start(ap, fmt);
        vsnprintf(heap, (size_t)n + 1, fmt, ap);
        va_end(ap);
        sc_add(b, heap);
        free(heap);
    }
}

static char *sc_take(scb *b) {
    if (b->oom) { free(b->buf); return NULL; }
    if (!b->buf) {
        b->buf = (char *)malloc(1);
        if (!b->buf) return NULL;
        b->buf[0] = '\0';
    }
    return b->buf;
}

/* --- json.dumps, ensure_ascii, indent ----------------------------------- */

static unsigned sc_utf8_next(const unsigned char *p, size_t *used) {
    if (p[0] < 0x80) { *used = 1; return p[0]; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *used = 2;
        return ((unsigned)(p[0] & 0x1F) << 6) | (unsigned)(p[1] & 0x3F);
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *used = 3;
        return ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) |
               (unsigned)(p[2] & 0x3F);
    }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
        (p[3] & 0xC0) == 0x80) {
        *used = 4;
        return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3F) << 12) |
               ((unsigned)(p[2] & 0x3F) << 6) | (unsigned)(p[3] & 0x3F);
    }
    *used = 1;
    return p[0];
}

static void sc_json_string(scb *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char esc[16];
    sc_putc(b, '"');
    while (*p) {
        size_t used = 1;
        unsigned cp = sc_utf8_next(p, &used);
        if (cp == '"') sc_add(b, "\\\"");
        else if (cp == '\\') sc_add(b, "\\\\");
        else if (cp == '\n') sc_add(b, "\\n");
        else if (cp == '\r') sc_add(b, "\\r");
        else if (cp == '\t') sc_add(b, "\\t");
        else if (cp == '\b') sc_add(b, "\\b");
        else if (cp == '\f') sc_add(b, "\\f");
        else if (cp >= 0x20 && cp <= 0x7e) sc_putc(b, (char)cp);
        else if (cp < 0x10000) {
            snprintf(esc, sizeof(esc), "\\u%04x", cp);
            sc_add(b, esc);
        } else {
            unsigned v = cp - 0x10000;
            snprintf(esc, sizeof(esc), "\\u%04x\\u%04x", 0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
            sc_add(b, esc);
        }
        p += used;
    }
    sc_putc(b, '"');
}

/* Python prints an int plainly and a float via repr; the integral check keeps
 * the integer that was in the fixture an integer on the wire. */
static void sc_json_number(scb *b, double d) {
    char tmp[64];
    if (d == (double)(long long)d) snprintf(tmp, sizeof(tmp), "%.0f", d);
    else snprintf(tmp, sizeof(tmp), "%.17g", d);
    sc_add(b, tmp);
}

static void sc_json(scb *b, const json_t *n, int level, int indent) {
    size_t i;
    if (!n) { sc_add(b, "null"); return; }
    switch (n->type) {
    case JSON_NULL: sc_add(b, "null"); break;
    case JSON_BOOL: sc_add(b, n->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: sc_json_number(b, n->num_val); break;
    case JSON_STRING: sc_json_string(b, n->str_val); break;
    case JSON_ARRAY: {
        size_t count = (size_t)json_len(n);
        sc_putc(b, '[');
        for (i = 0; i < count; i++) {
            int j;
            if (i) sc_putc(b, ',');
            if (indent) {
                sc_putc(b, '\n');
                for (j = 0; j < (level + 1) * indent; j++) sc_putc(b, ' ');
            }
            sc_json(b, json_get(n, i), level + 1, indent);
        }
        if (indent && count) {
            int j;
            sc_putc(b, '\n');
            for (j = 0; j < level * indent; j++) sc_putc(b, ' ');
        }
        sc_putc(b, ']');
        break;
    }
    case JSON_OBJECT: {
        size_t count = json_object_size(n);
        sc_putc(b, '{');
        for (i = 0; i < count; i++) {
            int j;
            if (i) sc_putc(b, ',');
            if (indent) {
                sc_putc(b, '\n');
                for (j = 0; j < (level + 1) * indent; j++) sc_putc(b, ' ');
            }
            sc_json_string(b, json_object_get_key_at(n, i));
            sc_add(b, indent ? ": " : ":");
            sc_json(b, json_object_get_at(n, i), level + 1, indent);
        }
        if (indent && count) {
            int j;
            sc_putc(b, '\n');
            for (j = 0; j < level * indent; j++) sc_putc(b, ' ');
        }
        sc_putc(b, '}');
        break;
    }
    }
}

/* --- _KIND_LABELS -------------------------------------------------------- */

typedef struct { const char *kind; const char *label; } sc_label_row;

static const sc_label_row LABELS[] = {
    {"mislabelled", "profile_name disagrees with the row's own session key"},
    {"wrong_store", "rows in another profile's store"},
    {"legacy_main", "legacy agent:main rows inside a named profile's store"},
    {"unclaimed_namespace", "rows keyed to a profile that does not exist"},
    {"crossed_parent", "parent_session_id crossing profile namespaces"},
    {"routing_stray", "routing rows outside the default store"},
    {"routing_unclaimed", "routing rows for a profile that does not exist"},
    {"topic_profile_less", "Telegram topic bindings without their bot's profile"},
    {"voice_profile_less", "voice-mode entries without their bot's profile"},
    {"sessions_json_unclaimed", "sessions.json entries for a profile that does not exist"},
    {NULL, NULL}
};

const char *w22_sessionscm_632617_kind_label(const char *kind) {
    int i;
    if (!kind) return "";
    for (i = 0; LABELS[i].kind; i++) {
        if (!strcmp(kind, LABELS[i].kind)) return LABELS[i].label;
    }
    return kind;
}

/* --- finding accessors --------------------------------------------------- */

static const char *f_str(const json_t *f, const char *key) {
    json_t *v = json_obj_get(f, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static const char *f_kind(const json_t *f) {
    const char *s = f_str(f, "kind");
    return s ? s : "";
}

static int f_repairable(const json_t *f) {
    json_t *v = json_obj_get(f, "repairable");
    return v && v->type == JSON_BOOL && v->bool_val;
}

static size_t f_count(const json_t *findings) {
    return (findings && findings->type == JSON_ARRAY) ? (size_t)json_len(findings) : 0;
}

static size_t f_repairable_count(const json_t *findings) {
    size_t i, n = 0;
    for (i = 0; i < f_count(findings); i++) {
        if (f_repairable(json_get(findings, i))) n++;
    }
    return n;
}

/* --- _group -------------------------------------------------------------- */

/* 1 when `kind` already appeared before index `upto`. */
static int kind_seen_before(const json_t *findings, size_t upto, const char *kind) {
    size_t i;
    for (i = 0; i < upto; i++) {
        if (!strcmp(f_kind(json_get(findings, i)), kind)) return 1;
    }
    return 0;
}

json_t *w22_sessionscm_632617_group(const json_t *findings) {
    json_t *out = json_object();
    size_t i, j;
    for (i = 0; i < f_count(findings); i++) {
        const char *kind = f_kind(json_get(findings, i));
        json_t *rows;
        if (kind_seen_before(findings, i, kind)) continue;
        rows = json_array();
        for (j = i; j < f_count(findings); j++) {
            json_t *row = json_get(findings, j);
            const char *subject;
            if (strcmp(f_kind(row), kind)) continue;
            subject = f_str(row, "subject");
            json_append(rows, json_string(subject ? subject : ""));
        }
        json_set(out, kind, rows);
    }
    return out;
}

/* --- _print_report ------------------------------------------------------- */

char *w22_sessionscm_632617_report(const json_t *findings) {
    scb b;
    size_t i, j;
    memset(&b, 0, sizeof(b));
    for (i = 0; i < f_count(findings); i++) {
        const char *kind = f_kind(json_get(findings, i));
        size_t rows = 0;
        if (kind_seen_before(findings, i, kind)) continue;
        for (j = i; j < f_count(findings); j++) {
            if (!strcmp(f_kind(json_get(findings, j)), kind)) rows++;
        }
        sc_printf(&b, "\n%s (", w22_sessionscm_632617_kind_label(kind));
        sc_int(&b, (long long)rows);
        sc_add(&b, "):\n");
        for (j = i; j < f_count(findings); j++) {
            json_t *row = json_get(findings, j);
            const char *store, *subject, *detail, *action, *reason;
            if (strcmp(f_kind(row), kind)) continue;
            store = f_str(row, "store");
            subject = f_str(row, "subject");
            detail = f_str(row, "detail");
            action = f_str(row, "action");
            reason = f_str(row, "reason");
            sc_printf(&b, "  [%s] %s\n", store ? store : "", subject ? subject : "");
            sc_printf(&b, "      %s\n", detail ? detail : "");
            if (action && action[0]) sc_printf(&b, "      " SC_ARROW " %s\n", action);
            else sc_printf(&b, "      " SC_CROSS " not repaired " SC_EMDASH " %s\n",
                           reason ? reason : "None");
        }
    }
    return sc_take(&b);
}

/* --- cmd_repair_profiles ------------------------------------------------- */

static void sc_profiles_join(scb *b, const json_t *stores) {
    size_t i, n = (stores && stores->type == JSON_ARRAY) ? (size_t)json_len(stores) : 0;
    for (i = 0; i < n; i++) {
        json_t *v = json_get(stores, i);
        if (i) sc_add(b, ", ");
        sc_add(b, (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "");
    }
}

/* Finding.as_dict(): a fresh object in the dataclass field order. */
static json_t *finding_dict(const json_t *row) {
    static const char *const KEYS[6] = {"kind", "store", "subject", "detail", "action", "reason"};
    json_t *out = json_object();
    int i;
    for (i = 0; i < 6; i++) {
        json_t *v = json_obj_get(row, KEYS[i]);
        json_set(out, KEYS[i], v ? json_copy(v) : json_null());
    }
    return out;
}

static json_t *findings_array(const json_t *findings) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < f_count(findings); i++) json_append(arr, finding_dict(json_get(findings, i)));
    return arr;
}

/* Append `json.dumps(doc, indent=2)` plus the newline print() adds. */
static void sc_print_json(scb *out, json_t *doc) {
    scb b;
    char *text;
    memset(&b, 0, sizeof(b));
    sc_json(&b, doc, 0, 2);
    text = sc_take(&b);
    if (text) {
        sc_add(out, text);
        sc_add(out, "\n");
        free(text);
    }
    json_free(doc);
}

static json_t *run_result(int code, scb *out, scb *err, json_t *prompts) {
    json_t *res = json_object();
    char *out_text = sc_take(out);
    char *err_text = sc_take(err);
    json_set(res, "exit", json_number((double)code));
    json_set(res, "stdout", json_string(out_text ? out_text : ""));
    json_set(res, "stderr", json_string(err_text ? err_text : ""));
    json_set(res, "prompts", prompts ? prompts : json_array());
    free(out_text);
    free(err_text);
    return res;
}

json_t *w22_sessionscm_632617_run(const json_t *request) {
    json_t *stores = json_obj_get(request, "stores");
    json_t *live = json_obj_get(request, "live");
    json_t *findings = json_obj_get(request, "findings");
    json_t *result = json_obj_get(request, "result");
    json_t *confirm = json_obj_get(request, "confirm");
    json_t *failures = result ? json_obj_get(result, "failures") : NULL;
    json_t *prompts = json_array();
    int apply = json_get_bool(request, "apply", 0) ? 1 : 0;
    int as_json = json_get_bool(request, "json", 0) ? 1 : 0;
    int yes = json_get_bool(request, "yes", 0) ? 1 : 0;
    size_t n_stores, n_findings, n_repairable, i;
    scb out, err;

    if (!stores || stores->type != JSON_ARRAY) stores = NULL;
    if (!live || live->type != JSON_ARRAY) live = NULL;
    if (!findings || findings->type != JSON_ARRAY) findings = NULL;
    if (!result || result->type != JSON_OBJECT) result = NULL;
    memset(&out, 0, sizeof(out));
    memset(&err, 0, sizeof(err));

    n_stores = stores ? (size_t)json_len(stores) : 0;
    n_findings = f_count(findings);
    n_repairable = f_repairable_count(findings);

    /* `legacy_main` only reaches scan_stores(); it changes no output. */

    if (apply && live && json_len(live) > 0) {
        size_t n = (size_t)json_len(live);
        size_t k;
        sc_add(&err, "A gateway is running for: ");
        for (k = 0; k < n; k++) {
            json_t *pair = json_get(live, k);
            json_t *profile = (pair && pair->type == JSON_ARRAY) ? json_get(pair, 0) : NULL;
            json_t *pid = (pair && pair->type == JSON_ARRAY) ? json_get(pair, 1) : NULL;
            if (k) sc_add(&err, ", ");
            sc_printf(&err, "%s (pid ",
                      (profile && profile->type == JSON_STRING) ? profile->str_val : "");
            if (pid && pid->type == JSON_NUMBER) sc_int(&err, (long long)pid->num_val);
            else if (pid && pid->type == JSON_STRING) sc_add(&err, pid->str_val);
            sc_add(&err, ")");
        }
        sc_add(&err, ". It holds the routing index in memory and would write it back over this "
                     "repair. Stop it (`hermes gateway stop`), then re-run --apply.\n");
        return run_result(1, &out, &err, prompts);
    }

    if (as_json && !apply) {
        json_t *doc = json_object();
        json_t *profiles = json_array();
        for (i = 0; i < n_stores; i++) {
            json_t *v = json_get(stores, i);
            json_append(profiles, json_string((v && v->type == JSON_STRING) ? v->str_val : ""));
        }
        json_set(doc, "stores", profiles);
        json_set(doc, "findings", findings_array(findings));
        json_set(doc, "repairable", json_number((double)n_repairable));
        sc_print_json(&out, doc);
        return run_result(0, &out, &err, prompts);
    }

    if (!as_json) {
        sc_printf(&out, "Scanned %llu profile store(s): ", (unsigned long long)n_stores);
        sc_profiles_join(&out, stores);
        sc_add(&out, "\n");
        if (!n_findings) {
            sc_add(&out, SC_CHECK " No crossed-profile state found.\n");
            return run_result(0, &out, &err, prompts);
        }
        {
            char *report = w22_sessionscm_632617_report(findings);
            if (report) { sc_add(&out, report); free(report); }
        }
    }

    if (!apply) {
        sc_printf(&out, "\n%llu of %llu finding(s) can be repaired. Re-run with --apply to "
                        "perform them.\n",
                  (unsigned long long)n_repairable, (unsigned long long)n_findings);
        return run_result(0, &out, &err, prompts);
    }

    if (!n_repairable) {
        if (as_json) {
            json_t *doc = json_object();
            json_set(doc, "findings", findings_array(findings));
            json_set(doc, "applied", json_object());
            sc_print_json(&out, doc);
        } else {
            sc_add(&out, "\nNothing to repair.\n");
        }
        return run_result(0, &out, &err, prompts);
    }

    if (!yes && !as_json) {
        char prompt[256];
        json_t *entry = json_array();
        snprintf(prompt, sizeof(prompt),
                 "\nApply %llu repair(s)? A snapshot of every store is taken first. [y/N] ",
                 (unsigned long long)n_repairable);
        json_append(entry, json_string(prompt));
        json_append(entry, json_string("Aborted " SC_EMDASH " nothing was changed."));
        json_append(prompts, entry);
        if (confirm && confirm->type == JSON_NUMBER) {
            return run_result((int)confirm->num_val, &out, &err, prompts);
        }
    }

    /* result = plan.apply(session, snapshot=default_snapshot) */
    if (as_json) {
        json_t *doc = json_object();
        json_set(doc, "findings", findings_array(findings));
        json_set(doc, "applied", result ? json_copy(result) : json_object());
        sc_print_json(&out, doc);
    } else {
        json_t *snapshots = result ? json_obj_get(result, "snapshots") : NULL;
        json_t *totals = result ? json_obj_get(result, "totals") : NULL;
        size_t k, n_totals = json_object_size(totals);
        size_t *order = n_totals ? (size_t *)malloc(n_totals * sizeof(size_t)) : NULL;
        size_t written = 0;

        for (k = 0; k < json_object_size(snapshots); k++) {
            const char *profile = json_object_get_key_at(snapshots, k);
            json_t *snap = json_object_get_at(snapshots, k);
            sc_printf(&out, "  snapshot [%s]: ", profile ? profile : "");
            if (snap && snap->type == JSON_STRING && snap->str_val[0]) sc_add(&out, snap->str_val);
            else sc_add(&out, "FAILED");           /* `snap or 'FAILED'` */
            sc_add(&out, "\n");
        }
        sc_add(&out, "\nApplied: ");
        if (order) {
            size_t a, c2;
            for (a = 0; a < n_totals; a++) order[a] = a;
            for (a = 0; a + 1 < n_totals; a++) {       /* insertion sort by key */
                for (c2 = 0; c2 + 1 < n_totals - a; c2++) {
                    const char *ka = json_object_get_key_at(totals, order[c2]);
                    const char *kb = json_object_get_key_at(totals, order[c2 + 1]);
                    if (ka && kb && strcmp(ka, kb) > 0) {
                        size_t tmp = order[c2];
                        order[c2] = order[c2 + 1];
                        order[c2 + 1] = tmp;
                    }
                }
            }
            for (a = 0; a < n_totals; a++) {
                json_t *v = json_object_get_at(totals, order[a]);
                const char *key = json_object_get_key_at(totals, order[a]);
                int truthy;
                truthy = v && !(v->type == JSON_NULL ||
                                (v->type == JSON_NUMBER && v->num_val == 0.0) ||
                                (v->type == JSON_STRING && (!v->str_val || !v->str_val[0])) ||
                                (v->type == JSON_BOOL && !v->bool_val) ||
                                ((v->type == JSON_ARRAY || v->type == JSON_OBJECT) &&
                                 v->c.count == 0));
                if (!truthy || !v) continue;
                if (written) sc_add(&out, ", ");
                sc_printf(&out, "%s=", key ? key : "");
                if (v->type == JSON_NUMBER) sc_json_number(&out, v->num_val);
                else if (v->type == JSON_STRING) sc_add(&out, v->str_val);
                else if (v->type == JSON_BOOL) sc_add(&out, v->bool_val ? "True" : "False");
                else sc_add(&out, "None");
                written++;
            }
            free(order);
        }
        if (!written) sc_add(&out, "nothing");
        sc_add(&out, "\n");
        for (k = 0; k < (size_t)json_len(failures); k++) {
            json_t *failure = json_get(failures, k);
            const char *kind = f_str(failure, "kind");
            const char *subject = f_str(failure, "subject");
            const char *error = f_str(failure, "error");
            sc_printf(&out, "  " SC_CROSS " %s %s: %s\n", kind ? kind : "",
                      subject ? subject : "", error ? error : "");
        }
        sc_add(&out, "Re-run without --apply to confirm the stores are clean.\n");
    }

    return run_result(json_len(failures) > 0 ? 1 : 0, &out, &err, prompts);
}
