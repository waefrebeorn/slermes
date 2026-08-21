#define _POSIX_C_SOURCE 200809L
/* goals_pure.c — Faithful port of the pure, deterministic helpers in
 * hermes_cli/goals.py (the /goal structured-completion contract).
 *
 * Ports the side-effect-free surface: the GoalContract dataclass, the
 * GoalGate dataclass serialization, and the parse_contract inline-parser.
 * workspace_fingerprint()/run_gate() shell out to git/subprocess and are
 * intentionally NOT ported here (not faithfully oracle-able standalone).
 *
 * PoP (Python -> C):
 *   parse_contract           -> goals_parse_contract
 *   GoalContract.is_empty     -> goals_contract_is_empty
 *   GoalContract.to_dict      -> goals_contract_to_json
 *   GoalContract.from_dict    -> goals_contract_from_json
 *   GoalContract.render_block -> goals_contract_render_block
 *   GoalGate.to_dict          -> goals_gate_to_json
 *   GoalGate.from_dict        -> goals_gate_from_json
 *
 * Callers receive freshly malloc'd strings (free with free()) or NULL.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define GOALS_DEFAULT_GATE_TIMEOUT_SECONDS 300
#define GOALS_DEFAULT_GATE_MAX_RETRIES     3
#define GOALS_GATE_OUTPUT_TAIL_CHARS       3000

/* canonical contract field order */
static const char *CONTRACT_FIELDS[5] = {"outcome", "verification", "constraints", "boundaries", "stop_when"};
static const char *CONTRACT_LABELS[5] = {"Outcome", "Verification", "Constraints", "Boundaries", "Stop when blocked"};

/* GoalContract as a fixed struct (opaque to callers). */
typedef struct goals_contract {
    char outcome[1024];
    char verification[1024];
    char constraints[1024];
    char boundaries[1024];
    char stop_when[1024];
} goals_contract_t;

static const char *contract_field_ptr(const goals_contract_t *c, int i) {
    switch (i) {
        case 0: return c->outcome;
        case 1: return c->verification;
        case 2: return c->constraints;
        case 3: return c->boundaries;
        case 4: return c->stop_when;
    }
    return "";
}
static void contract_field_set(goals_contract_t *c, int i, const char *v) {
    char *dst = NULL;
    switch (i) {
        case 0: dst = c->outcome; break;
        case 1: dst = c->verification; break;
        case 2: dst = c->constraints; break;
        case 3: dst = c->boundaries; break;
        case 4: dst = c->stop_when; break;
    }
    if (dst) { strncpy(dst, v ? v : "", 1023); dst[1023] = '\0'; }
}

static void contract_trim(char *s) {
    /* strip leading/trailing whitespace */
    size_t l = strlen(s);
    size_t st = 0;
    while (st < l && isspace((unsigned char)s[st])) st++;
    size_t en = l;
    while (en > st && isspace((unsigned char)s[en - 1])) en--;
    if (st > 0) memmove(s, s + st, en - st);
    s[en - st] = '\0';
}

/* ─── contract lifecycle ─────────────────────────────────────────── */
int goals_contract_is_empty(const goals_contract_t *c) {
    for (int i = 0; i < 5; i++) {
        char buf[1024];
        strncpy(buf, contract_field_ptr(c, i), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        contract_trim(buf);
        if (buf[0]) return 0;
    }
    return 1;
}

char *goals_contract_to_json(const goals_contract_t *c) {
    size_t cap = 1;
    for (int i = 0; i < 5; i++) cap += strlen(CONTRACT_FIELDS[i]) + strlen(contract_field_ptr(c, i)) * 2 + 8;
    char *out = (char *)malloc(cap);
    out[0] = '\0';
    strcat(out, "{");
    for (int i = 0; i < 5; i++) {
        if (i) strcat(out, ",");
        char esc[2048];
        const char *v = contract_field_ptr(c, i);
        size_t o = 0;
        for (size_t k = 0; v[k]; k++) {
            char ch = v[k];
            if (ch == '"' || ch == '\\') { esc[o++] = '\\'; esc[o++] = ch; }
            else if (ch == '\n') { esc[o++] = '\\'; esc[o++] = 'n'; }
            else esc[o++] = ch;
        }
        esc[o] = '\0';
        char kv[2300];
        snprintf(kv, sizeof(kv), "\"%s\":\"%s\"", CONTRACT_FIELDS[i], esc);
        strcat(out, kv);
    }
    strcat(out, "}");
    return out;
}

/* populates *c from a JSON object string; returns 0 on success. */
int goals_contract_from_json(goals_contract_t *c, const char *json_str) {
    if (!c) return -1;
    memset(c, 0, sizeof(*c));
    if (!json_str || !*json_str) return 0;
    json_t *root = json_parse(json_str, NULL);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return 0; }
    for (int i = 0; i < 5; i++) {
        json_t *v = json_object_get(root, CONTRACT_FIELDS[i]);
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : "";
        char buf[1024];
        if (v && v->type == JSON_NUMBER) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else strncpy(buf, s ? s : "", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        contract_trim(buf);
        contract_field_set(c, i, buf);
    }
    json_free(root);
    return 0;
}

char *goals_contract_render_block(const goals_contract_t *c) {
    char *out = (char *)malloc(1);
    out[0] = '\0';
    size_t cap = 1;
    for (int i = 0; i < 5; i++) {
        char buf[1024];
        strncpy(buf, contract_field_ptr(c, i), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        contract_trim(buf);
        if (!buf[0]) continue;
        size_t need = strlen(out) + strlen(CONTRACT_LABELS[i]) + strlen(buf) + 8;
        if (need > cap) { cap = need * 2; out = (char *)realloc(out, cap); }
        char line[1280];
        snprintf(line, sizeof(line), "- %s: %s\n", CONTRACT_LABELS[i], buf);
        strcat(out, line);
    }
    size_t l = strlen(out);
    if (l > 0 && out[l - 1] == '\n') out[l - 1] = '\0';
    return out;
}

/* ─── parse_contract ─────────────────────────────────────────────── */
/* maps a lowercase prefix to a canonical field index, or -1. */
static int contract_alias_field(const char *prefix_lower) {
    /* build alias table inline (prefix -> field idx) */
    struct { const char *alias; int field; } table[] = {
        {"outcome", 0}, {"goal", 0}, {"done", 0}, {"done when", 0},
        {"verification", 1}, {"verify", 1}, {"verified by", 1}, {"evidence", 1}, {"proof", 1},
        {"constraints", 2}, {"constraint", 2}, {"preserve", 2}, {"must not", 2}, {"do not change", 2},
        {"boundaries", 3}, {"boundary", 3}, {"scope", 3}, {"allowed", 3}, {"files", 3},
        {"stop when", 4}, {"stop_when", 4}, {"blocked", 4}, {"stop if blocked", 4}, {"give up when", 4},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strcmp(table[i].alias, prefix_lower) == 0) return table[i].field;
    }
    return -1;
}

/* Returns a JSON object: {"headline": "...", "contract": { ... }} */
char *goals_parse_contract(const char *text) {
    goals_contract_t c;
    memset(&c, 0, sizeof(c));
    char headline[4096];
    headline[0] = '\0';

    if (!text || !*text) {
        char *out = (char *)malloc(64);
        snprintf(out, 64, "{\"headline\":\"\",\"contract\":{}}");
        return out;
    }

    char **fields[5];
    for (int i = 0; i < 5; i++) fields[i] = NULL;
    size_t fcount[5] = {0};
    size_t fcap[5] = {0};

    /* iterate lines */
    char *dup = strdup(text);
    char *save = dup;
    char *line;
    while ((line = strtok_r(save, "\n", &save)) != NULL) {
        char *l = strdup(line);
        contract_trim(l);
        if (!l[0]) { free(l); continue; }
        int matched = 0;
        char *colon = strchr(l, ':');
        if (colon) {
            *colon = '\0';
            char *prefix = strdup(l);
            char *value = colon + 1;
            /* lowercase prefix */
            char lowbuf[256];
            size_t pl = strlen(prefix);
            for (size_t k = 0; k < pl && k < sizeof(lowbuf) - 1; k++)
                lowbuf[k] = (char)tolower((unsigned char)prefix[k]);
            lowbuf[pl < sizeof(lowbuf) - 1 ? pl : sizeof(lowbuf) - 2] = '\0';
            contract_trim(prefix);
            int field = contract_alias_field(lowbuf);
            /* value.strip() */
            char *v = strdup(value);
            contract_trim(v);
            if (field >= 0 && v[0]) {
                if (fcount[field] == fcap[field]) {
                    fcap[field] = fcap[field] ? fcap[field] * 2 : 4;
                    fields[field] = (char **)realloc(fields[field], sizeof(char *) * fcap[field]);
                }
                fields[field][fcount[field]++] = v;
                matched = 1;
            }
            free(v);
            free(prefix);
        }
        if (!matched) {
            size_t hl = strlen(headline);
            size_t need = hl + strlen(l) + 2;
            if (need > sizeof(headline)) need = sizeof(headline);
            if (hl) {
                if (hl + 1 < sizeof(headline)) { headline[hl] = ' '; headline[hl+1] = '\0'; }
            }
            strncat(headline, l, sizeof(headline) - strlen(headline) - 1);
        }
        free(l);
    }
    free(dup);

    contract_trim(headline);

    /* join field lists into the contract struct */
    for (int i = 0; i < 5; i++) {
        char joined[1024];
        joined[0] = '\0';
        for (size_t k = 0; k < fcount[i]; k++) {
            if (k) strncat(joined, " ", sizeof(joined) - strlen(joined) - 1);
            strncat(joined, fields[i][k], sizeof(joined) - strlen(joined) - 1);
            free(fields[i][k]);
        }
        contract_trim(joined);
        contract_field_set(&c, i, joined);
        free(fields[i]);
    }

    char *cjson = goals_contract_to_json(&c);
    size_t cap = strlen(headline) * 2 + strlen(cjson) + 64;
    char *out = (char *)malloc(cap);
    char hesc[8192];
    size_t o = 0;
    for (size_t k = 0; headline[k]; k++) {
        char ch = headline[k];
        if (ch == '"' || ch == '\\') { hesc[o++] = '\\'; hesc[o++] = ch; }
        else if (ch == '\n') { hesc[o++] = '\\'; hesc[o++] = 'n'; }
        else hesc[o++] = ch;
    }
    hesc[o] = '\0';
    snprintf(out, cap, "{\"headline\":\"%s\",\"contract\":%s}", hesc, cjson);
    free(cjson);
    return out;
}

/* ─── GoalGate lifecycle ─────────────────────────────────────────── */
/* GoalGate as a fixed struct. */
typedef struct goals_gate {
    char command[1024];
    int timeout_seconds;
    int max_retries;
    int attempts;
    int last_exit_code;   /* -1 sentinel for None */
    char last_output_tail[1024];
    char last_failed_fingerprint[1024];
} goals_gate_t;

static void json_str_escape(const char *s, char *out, size_t outsz) {
    size_t o = 0; out[0] = '\0';
    if (!s) return;
    for (size_t k = 0; s[k] && o + 1 < outsz; k++) {
        char ch = s[k];
        if (ch == '"' || ch == '\\') { if (o+2 < outsz){out[o++]='\\';out[o++]=ch;} }
        else if (ch == '\n') { if (o+2 < outsz){out[o++]='\\';out[o++]='n';} }
        else out[o++] = ch;
    }
    out[o] = '\0';
}

char *goals_gate_to_json(const goals_gate_t *g) {
    char cmd[2048], tail[2048], fp[2048];
    json_str_escape(g->command, cmd, sizeof(cmd));
    json_str_escape(g->last_output_tail, tail, sizeof(tail));
    json_str_escape(g->last_failed_fingerprint, fp, sizeof(fp));
    char ec[8];
    if (g->last_exit_code < 0) strcpy(ec, "null");
    else snprintf(ec, sizeof(ec), "%d", g->last_exit_code);
    size_t cap = strlen(cmd) + strlen(tail) + strlen(fp) + 256;
    char *out = (char *)malloc(cap);
    snprintf(out, cap,
        "{\"command\":\"%s\",\"timeout_seconds\":%d,\"max_retries\":%d,"
        "\"attempts\":%d,\"last_exit_code\":%s,\"last_output_tail\":\"%s\","
        "\"last_failed_fingerprint\":\"%s\"}",
        cmd, g->timeout_seconds, g->max_retries, g->attempts, ec, tail, fp);
    return out;
}

int goals_gate_from_json(goals_gate_t *g, const char *json_str) {
    if (!g) return -1;
    g->command[0] = '\0';
    g->timeout_seconds = GOALS_DEFAULT_GATE_TIMEOUT_SECONDS;
    g->max_retries = GOALS_DEFAULT_GATE_MAX_RETRIES;
    g->attempts = 0;
    g->last_exit_code = -1;
    g->last_output_tail[0] = '\0';
    g->last_failed_fingerprint[0] = '\0';
    if (!json_str || !*json_str) return 0;
    json_t *root = json_parse(json_str, NULL);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return 0; }
    const char *cmd = json_get_str(root, "command", "");
    strncpy(g->command, cmd, sizeof(g->command) - 1); g->command[sizeof(g->command)-1] = '\0';
    g->timeout_seconds = (int)json_get_num(root, "timeout_seconds", GOALS_DEFAULT_GATE_TIMEOUT_SECONDS);
    if (g->timeout_seconds <= 0) g->timeout_seconds = GOALS_DEFAULT_GATE_TIMEOUT_SECONDS;
    g->max_retries = (int)json_get_num(root, "max_retries", GOALS_DEFAULT_GATE_MAX_RETRIES);
    if (g->max_retries <= 0) g->max_retries = GOALS_DEFAULT_GATE_MAX_RETRIES;
    g->attempts = (int)json_get_num(root, "attempts", 0);
    if (g->attempts < 0) g->attempts = 0;
    json_t *lec = json_object_get(root, "last_exit_code");
    if (lec && (lec->type != JSON_NULL)) g->last_exit_code = (int)json_get_num(root, "last_exit_code", -1);
    else g->last_exit_code = -1;
    const char *t = json_get_str(root, "last_output_tail", "");
    strncpy(g->last_output_tail, t, sizeof(g->last_output_tail) - 1); g->last_output_tail[sizeof(g->last_output_tail)-1]='\0';
    const char *f = json_get_str(root, "last_failed_fingerprint", "");
    strncpy(g->last_failed_fingerprint, f, sizeof(g->last_failed_fingerprint) - 1); g->last_failed_fingerprint[sizeof(g->last_failed_fingerprint)-1]='\0';
    json_free(root);
    return 0;
}


/* ─── opaque allocators (match public header) ────────────────────── */
goals_contract_t *goals_contract_new(void) {
    return (goals_contract_t *)calloc(1, sizeof(goals_contract_t));
}
void goals_contract_free(goals_contract_t *c) { free(c); }
goals_gate_t *goals_gate_new(void) {
    goals_gate_t *g = (goals_gate_t *)calloc(1, sizeof(goals_gate_t));
    g->last_exit_code = -1;
    g->timeout_seconds = GOALS_DEFAULT_GATE_TIMEOUT_SECONDS;
    g->max_retries = GOALS_DEFAULT_GATE_MAX_RETRIES;
    return g;
}
void goals_gate_free(goals_gate_t *g) { free(g); }
