/*
 * approval_provenance.c — C home for upstream Hermes tools/approval_provenance.py.
 * PoP: w12_approvalpr_bfd8ef_preapproved_result @ tools/approval_provenance.py:preapproved_result
 * PoP: w12_approvalpr_bfd8ef_allowlisted_result @ tools/approval_provenance.py:allowlisted_result
 * PoP: w12_approvalpr_bfd8ef_preapproval_note @ tools/approval_provenance.py:preapproval_note
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "noc_w12_approvalpr_bfd8ef.h"

/* --- tiny growable string buffer --- */
typedef struct { char *data; size_t len, cap; } w12_sb_t;

static void w12_sb_init(w12_sb_t *b) {
    b->cap = 256; b->len = 0;
    b->data = (char *)malloc(b->cap);
    if (b->data) b->data[0] = '\0';
}
static void w12_sb_free(w12_sb_t *b) {
    free(b->data); b->data = NULL; b->len = b->cap = 0;
}
static void w12_sb_addn(w12_sb_t *b, const char *s, size_t n) {
    if (!b->data || !s || !n) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap;
        while (nc < b->len + n + 1) nc *= 2;
        char *nd = (char *)realloc(b->data, nc);
        if (!nd) return;
        b->data = nd; b->cap = nc;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0'; /* NUL at the (possibly shortened) logical length */
}
static void w12_sb_add(w12_sb_t *b, const char *s) {
    if (s) w12_sb_addn(b, s, strlen(s));
}
static char *w12_sb_take(w12_sb_t *b) {
    char *p = b->data; b->data = NULL; b->len = b->cap = 0; return p;
}

static const char *w12_jstr(const json_t *n, const char *def) {
    return (n && n->type == JSON_STRING && n->str_val) ? n->str_val : def;
}

json_t *w12_approvalpr_bfd8ef_preapproved_result(const json_t *findings, const json_t *scopes) {
    json_t *res = json_object();
    json_set(res, "approved", json_bool(true));
    json_set(res, "message", json_null());
    json_t *pre = json_array();
    if (findings && findings->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(findings); i++) {
            const json_t *pair = json_get(findings, i);
            const char *key = w12_jstr(pair ? json_get(pair, 0) : NULL, "");
            const char *text = w12_jstr(pair ? json_get(pair, 1) : NULL, "");
            const char *scope = w12_jstr(scopes ? json_obj_get(scopes, key) : NULL, "");
            json_t *e = json_object();
            json_set(e, "description", json_string(text));
            json_set(e, "scope", json_string(scope));
            json_append(pre, e);
        }
    }
    json_set(res, "pre_approved", pre);
    return res;
}

json_t *w12_approvalpr_bfd8ef_allowlisted_result(const char *rule, const char *description) {
    json_t *res = json_object();
    json_set(res, "approved", json_bool(true));
    json_set(res, "message", json_null());
    json_t *pre = json_array();
    json_t *e = json_object();
    json_set(e, "description", json_string(description ? description : ""));
    json_set(e, "scope", json_string("allowlist"));
    json_set(e, "rule", json_string(rule ? rule : ""));
    json_append(pre, e);
    json_set(res, "pre_approved", pre);
    return res;
}

char *w12_approvalpr_bfd8ef_preapproval_note(const json_t *entries) {
    size_t n = (entries && entries->type == JSON_ARRAY) ? json_len(entries) : 0;
    w12_sb_t flagged, body;
    w12_sb_init(&flagged); w12_sb_init(&body);

    /* reasons: at most one unique phrase per entry */
    char **reasons = (char **)calloc(n ? n : 1, sizeof(char *));
    size_t nr = 0;

    for (size_t i = 0; i < n; i++) {
        const json_t *e = json_get(entries, i);
        const char *desc = w12_jstr(e ? json_obj_get(e, "description") : NULL, "");
        const char *scope = w12_jstr(e ? json_obj_get(e, "scope") : NULL, "");
        const char *rule = w12_jstr(e ? json_obj_get(e, "rule") : NULL, "");

        if (i) w12_sb_add(&flagged, "; ");
        w12_sb_add(&flagged, desc);

        char phrase[2048];
        if (!strcmp(scope, "allowlist"))
            snprintf(phrase, sizeof(phrase), "the command_allowlist entry `%s` in config.yaml", rule);
        else if (!strcmp(scope, "permanent"))
            snprintf(phrase, sizeof(phrase), "%s", "an 'always' approval the user saved earlier");
        else if (!strcmp(scope, "session"))
            snprintf(phrase, sizeof(phrase), "%s", "an approval the user gave earlier in this session");
        else
            phrase[0] = '\0';

        char full[4096];
        if (n > 1) snprintf(full, sizeof(full), "%s: %s", desc, phrase);
        else snprintf(full, sizeof(full), "%s", phrase);

        int seen = 0;
        for (size_t k = 0; k < nr; k++) if (reasons[k] && !strcmp(reasons[k], full)) { seen = 1; break; }
        if (!seen && nr < n) { reasons[nr] = strdup(full); nr++; }
    }

    w12_sb_add(&body, "Command was flagged (");
    if (flagged.data) w12_sb_add(&body, flagged.data);
    w12_sb_add(&body, ") and auto-approved by ");
    for (size_t k = 0; k < nr; k++) {
        if (k) w12_sb_add(&body, "; ");
        w12_sb_add(&body, reasons[k]);
    }
    w12_sb_add(&body, ".");

    for (size_t k = 0; k < nr; k++) free(reasons[k]);
    free(reasons);
    w12_sb_free(&flagged);
    return w12_sb_take(&body);
}
