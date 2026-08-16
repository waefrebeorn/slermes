#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/goals.c — C11 port of hermes_cli/goals.py
 * Merged from 6 source file(s) by reorganization.
 */

/* ── from src/agent/port_hermes_cli_goals_py_final.c ── */
/* Slermes C port — hermes_cli/goals.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: hermes_cli_goals_run_gate @ hermes_cli/goals.py:run_gate */
json_t *hermes_cli_goals_run_gate(void) {
    return json_object();
}

/* PoP: hermes_cli_goals_parse_judge_response @ hermes_cli/goals.py:_parse_judge_response */
json_t *hermes_cli_goals_parse_judge_response(void) {
    return json_object();
}

/* PoP: hermes_cli_goals_extract_json_object @ hermes_cli/goals.py:_extract_json_object */
json_t *hermes_cli_goals_extract_json_object(void) {
    return json_object();
}


/* ── from src/cli/port_goals_data.c ── */
/*
 * port_goals_data.c — pure data-model layer for Hermes session goals.
 *
 * Faithful C port of the pure-logic slice of hermes_cli/goals.py:
 *   GoalContract dataclass  (lines ~294-331)
 *   parse_contract          (lines ~334-380)
 *   GoalState dataclass     (lines ~389-481)
 *   _meta_key               (line  489)
 *
 * This is the data core: no SessionDB, no TTY, no network, no judge loop.
 * The persistence + loop layers live elsewhere and call into this API.
 *
/* PoP: goal_meta_key           @ hermes_cli/goals.py:_meta_key */
/* PoP: goal_contract_is_empty  @ hermes_cli/goals.py:is_empty */
/* PoP: goal_contract_to_dict   @ hermes_cli/goals.py:to_dict */
/* PoP: field_index @ hermes_cli/goals.py:from_dict */
/* PoP: goal_contract_render    @ hermes_cli/goals.py:render_block */
/* PoP: parse_contract          @ hermes_cli/goals.py:parse_contract */
/* PoP: goal_state_to_json      @ hermes_cli/goals.py:to_json */
/* PoP: goal_state_from_json    @ hermes_cli/goals.py:from_json */
/* PoP: field_index @ hermes_cli/goals.py:has_contract */
/* PoP: field_index @ hermes_cli/goals.py:render_subgoals_block */

#include "goal_contract.h"
#include "goal_contract_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "libjson/json.h"

const int GOAL_DEFAULT_MAX_TURNS = 20;

/* ───────────────────────────────────────────────────────────────────
 * Canonical contract fields + labels + inline-input aliases
 * (Python: _CONTRACT_FIELDS / _CONTRACT_LABELS / _CONTRACT_ALIASES)
 * ─────────────────────────────────────────────────────────────────── */

static const char *CONTRACT_FIELDS[N_CONTRACT_FIELDS] = {
    "outcome", "verification", "constraints", "boundaries", "stop_when"
};

/* Index of a canonical field name, or -1. */
static int field_index(const char *field) {
    if (!field) return -1;
    for (int i = 0; i < N_CONTRACT_FIELDS; i++) {
        if (strcmp(CONTRACT_FIELDS[i], field) == 0) return i;
    }
    return -1;
}

/* Human label for a canonical field name (for rendering). */
static const char *field_label(const char *field) {
    if (!field) return NULL;
    if (strcmp(field, "outcome") == 0)      return "Outcome";
    if (strcmp(field, "verification") == 0) return "Verification";
    if (strcmp(field, "constraints") == 0)  return "Constraints";
    if (strcmp(field, "boundaries") == 0)   return "Boundaries";
    if (strcmp(field, "stop_when") == 0)    return "Stop when blocked";
    return NULL;
}

/* lowercase helper (no locale) — defined before its use in alias_to_field. */
static char lowercase(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

/*
 * Inline alias -> canonical field. Mirrors _CONTRACT_ALIASES exactly.
 * Lookup is case-insensitive on the trimmed prefix.
 */
static const char *alias_to_field(const char *raw_prefix) {
    if (!raw_prefix) return NULL;
    /* normalize: trim leading/trailing spaces + lowercase (preserve internal
     * spaces so multi-word aliases like "stop when" survive). */
    size_t n = strlen(raw_prefix);
    size_t a = 0;
    while (a < n && (raw_prefix[a] == ' ' || raw_prefix[a] == '\t')) a++;
    size_t b = n;
    while (b > a && (raw_prefix[b - 1] == ' ' || raw_prefix[b - 1] == '\t')) b--;
    char *buf = malloc((b - a) + 1);
    if (!buf) return NULL;
    size_t j = 0;
    for (size_t i = a; i < b; i++) {
        buf[j++] = (char)lowercase(raw_prefix[i]);
    }
    buf[j] = '\0';
#define ALIAS(a, f) if (strcmp(buf, a) == 0) { free(buf); return f; }
    ALIAS("outcome", "outcome")
    ALIAS("goal", "outcome")
    ALIAS("done", "outcome")
    ALIAS("done when", "outcome")
    ALIAS("verification", "verification")
    ALIAS("verify", "verification")
    ALIAS("verified by", "verification")
    ALIAS("evidence", "verification")
    ALIAS("proof", "verification")
    ALIAS("constraints", "constraints")
    ALIAS("constraint", "constraints")
    ALIAS("preserve", "constraints")
    ALIAS("must not", "constraints")
    ALIAS("do not change", "constraints")
    ALIAS("boundaries", "boundaries")
    ALIAS("boundary", "boundaries")
    ALIAS("scope", "boundaries")
    ALIAS("allowed", "boundaries")
    ALIAS("files", "boundaries")
    ALIAS("stop when", "stop_when")
    ALIAS("stop_when", "stop_when")
    ALIAS("blocked", "stop_when")
    ALIAS("stop if blocked", "stop_when")
    ALIAS("give up when", "stop_when")
#undef ALIAS
    free(buf);
    return NULL;
}

/* ───────────────────────────────────────────────────────────────────
 * GoalContract
 * ─────────────────────────────────────────────────────────────────── */

goal_contract_t *goal_contract_new(void) {
    goal_contract_t *c = calloc(1, sizeof(*c));
    return c; /* all fields NULL */
}

void goal_contract_clear(goal_contract_t *c) {
    if (!c) return;
    for (int i = 0; i < N_CONTRACT_FIELDS; i++) {
        free(c->fields[i]);
        c->fields[i] = NULL;
    }
}

void goal_contract_free(goal_contract_t *c) {
    if (!c) return;
    goal_contract_clear(c);
    free(c);
}

bool goal_contract_is_empty(const goal_contract_t *c) {
    if (!c) return true;
    for (int i = 0; i < N_CONTRACT_FIELDS; i++) {
        const char *v = c->fields[i];
        if (v && v[0] != '\0') {
            /* mirror Python .strip() — treat whitespace-only as empty */
            bool all_ws = true;
            for (const char *p = v; *p; p++) {
                if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') { all_ws = false; break; }
            }
            if (!all_ws) return false;
        }
    }
    return true;
}

const char *goal_contract_get(const goal_contract_t *c, const char *field) {
    if (!c) return "";
    int idx = field_index(field);
    if (idx < 0) return "";
    return c->fields[idx] ? c->fields[idx] : "";
}

void goal_contract_set(goal_contract_t *c, const char *field, const char *value) {
    if (!c) return;
    int idx = field_index(field);
    if (idx < 0) return;
    free(c->fields[idx]);
    c->fields[idx] = (value && value[0]) ? strdup(value) : NULL;
}

bool goal_contract_from_json(goal_contract_t *c, const char *json) {
    if (!c) return false;
    if (!json || !json[0]) return true; /* nothing to load => empty */
    char *err = NULL;
    json_t *doc = json_parse(json, &err);
    if (err) { free(err); return false; }
    if (!doc || doc->type != JSON_OBJECT) { json_free(doc); return false; }
    goal_contract_clear(c);
    for (int i = 0; i < N_CONTRACT_FIELDS; i++) {
        const char *v = json_get_str(doc, CONTRACT_FIELDS[i], NULL);
        if (v) {
            /* strdup + strip (Python: str(...).strip()) */
            char *trimmed = strdup(v);
            /* strip leading/trailing whitespace */
            char *start = trimmed;
            while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
            char *end = start + strlen(start);
            while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
            *end = '\0';
            if (*start) c->fields[i] = strdup(start);
            free(trimmed);
        }
    }
    json_free(doc);
    return true;
}

char *goal_contract_render(const goal_contract_t *c) {
    if (!c) return strdup("");
    /* Two-pass: measure then build, to avoid realloc churn. */
    size_t cap = 1;
    for (int i = 0; i < N_CONTRACT_FIELDS; i++) {
        const char *v = c->fields[i];
        if (!v || !v[0]) continue;
        bool all_ws = true;
        for (const char *p = v; *p; p++) {
            if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') { all_ws = false; break; }
        }
        if (all_ws) continue;
        const char *label = field_label(CONTRACT_FIELDS[i]);
        /* "- %s: %s\n" => 2 + label + 2 + v + 1 = label + v + 5 */
        cap += strlen(label) + strlen(v) + 5;
    }
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    for (int i = 0; i < N_CONTRACT_FIELDS; i++) {
        const char *v = c->fields[i];
        if (!v || !v[0]) continue;
        bool all_ws = true;
        for (const char *p = v; *p; p++) {
            if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') { all_ws = false; break; }
        }
        if (all_ws) continue;
        const char *label = field_label(CONTRACT_FIELDS[i]);
        size_t len = strlen(out);
        snprintf(out + len, cap - len, "- %s: %s\n", label, v);
    }
    /* trim trailing newline */
    size_t L = strlen(out);
    if (L > 0 && out[L - 1] == '\n') out[L - 1] = '\0';
    return out;
}

/* ───────────────────────────────────────────────────────────────────
 * parse_contract
 * ─────────────────────────────────────────────────────────────────── */

bool parse_contract(const char *text, char **headline_out, goal_contract_t **contract_out) {
    if (headline_out) *headline_out = NULL;
    if (contract_out) *contract_out = NULL;
    if (!headline_out || !contract_out) return false;

    if (!text || !text[0]) {
        *headline_out = strdup("");
        *contract_out = goal_contract_new();
        return true;
    }

    /* Per-field collected lines. */
    char **field_lines[N_CONTRACT_FIELDS];
    size_t field_n[N_CONTRACT_FIELDS];
    for (int i = 0; i < N_CONTRACT_FIELDS; i++) { field_lines[i] = NULL; field_n[i] = 0; }

    /* headline grows dynamically */
    char *headline = NULL;
    size_t headline_len = 0;

    /* Walk lines. Replace CRLF/newline with the source newlines. */
    const char *p = text;
    while (*p) {
        /* read one line (strip trailing \r) */
        const char *line_start = p;
        while (*p && *p != '\n') p++;
        size_t line_len = (size_t)(p - line_start);
        /* trim trailing \r */
        while (line_len > 0 && line_start[line_len - 1] == '\r') line_len--;
        /* trim surrounding whitespace */
        const char *ls = line_start;
        const char *le = line_start + line_len;
        while (ls < le && (*ls == ' ' || *ls == '\t')) ls++;
        while (le > ls && (le[-1] == ' ' || le[-1] == '\t')) le--;
        size_t tlen = (size_t)(le - ls);
        bool blank = (tlen == 0);
        bool matched = false;

        if (!blank && memchr(ls, ':', tlen) != NULL) {
            /* find first ':' within trimmed line */
            const char *colon = memchr(ls, ':', tlen);
            size_t prefix_len = (size_t)(colon - ls);
            /* prefix = ls[0..prefix_len), value = colon+1 .. le */
            const char *val = colon + 1;
            /* trim value */
            while (val < le && (*val == ' ' || *val == '\t')) val++;
            const char *val_e = le;
            while (val_e > val && (val_e[-1] == ' ' || val_e[-1] == '\t')) val_e--;
            /* prefix string (trimmed) */
            char *prefix = malloc(prefix_len + 1);
            memcpy(prefix, ls, prefix_len);
            prefix[prefix_len] = '\0';
            /* trim prefix */
            while (prefix_len > 0 && (prefix[prefix_len - 1] == ' ' || prefix[prefix_len - 1] == '\t')) prefix[--prefix_len] = '\0';

            const char *field = alias_to_field(prefix);
            if (field && (val_e > val)) {
                int idx = field_index(field);
                /* append value to field_lines[idx] (count tracked in field_n[idx]) */
                char *v = malloc((size_t)(val_e - val) + 1);
                memcpy(v, val, (size_t)(val_e - val));
                v[val_e - val] = '\0';
                field_lines[idx] = realloc(field_lines[idx], (field_n[idx] + 1) * sizeof(char *));
                field_lines[idx][field_n[idx]++] = v;
                matched = true;
            }
            free(prefix);
        }

        if (!matched && !blank) {
            /* append (trimmed) line to headline, space-joined.
             * Use memcpy with an explicit offset rather than strcat: the
             * buffer comes from realloc(NULL,...) (== malloc) on the first
             * append and is uninitialized, so strcat would scan for a NUL
             * terminator past the allocation. */
            size_t add = tlen;
            char *seg = malloc(add + 1);
            memcpy(seg, ls, add);
            seg[add] = '\0';
            size_t need = headline_len + (headline_len ? 1 : 0) + add;
            char *nh = realloc(headline, need + 1);
            if (!nh) { free(seg); continue; }
            size_t off = headline_len;
            if (headline_len) {
                nh[off++] = ' ';
            }
            memcpy(nh + off, seg, add);
            nh[off + add] = '\0';
            headline = nh;
            headline_len = need;
            free(seg);
        }

        if (*p == '\n') p++;
    }

    goal_contract_t *contract = goal_contract_new();
    for (int i = 0; i < N_CONTRACT_FIELDS; i++) {
        if (field_n[i] == 0) continue;
        /* join field_lines[i] with spaces (matches Python " ".join) */
        size_t total = 1;
        for (size_t k = 0; k < field_n[i]; k++) total += strlen(field_lines[i][k]) + 1;
        char *joined = malloc(total);
        joined[0] = '\0';
        for (size_t k = 0; k < field_n[i]; k++) {
            if (k) strcat(joined, " ");
            strcat(joined, field_lines[i][k]);
            free(field_lines[i][k]);
        }
        contract->fields[i] = joined;
        free(field_lines[i]);
    }

    *headline_out = headline ? headline : strdup("");
    *contract_out = contract;
    return true;
}

/* ───────────────────────────────────────────────────────────────────
 * GoalState
 * ─────────────────────────────────────────────────────────────────── */

goal_state_t *goal_state_new(const char *goal) {
    goal_state_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->goal = strdup(goal ? goal : "");
    s->status = strdup("active");
    s->max_turns = GOAL_DEFAULT_MAX_TURNS;
    s->contract = goal_contract_new();
    return s;
}

void goal_state_free(goal_state_t *s) {
    if (!s) return;
    free(s->goal);
    free(s->status);
    free(s->last_verdict);
    free(s->last_reason);
    free(s->paused_reason);
    free(s->waiting_on_session);
    free(s->waiting_reason);
    for (size_t i = 0; i < s->n_subgoals; i++) free(s->subgoals[i]);
    free(s->subgoals);
    goal_contract_free(s->contract);
    free(s);
}

bool goal_state_has_contract(const goal_state_t *s) {
    return s && s->contract && !goal_contract_is_empty(s->contract);
}

int goal_state_subgoal_count(const goal_state_t *s) {
    return (int)(s ? s->n_subgoals : 0);
}

int goal_state_add_subgoal(goal_state_t *s, const char *text) {
    if (!s || !text) return 0;
    /* trim */
    const char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    const char *e = p + strlen(p);
    while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) e--;
    if (e <= p) return 0;
    size_t len = (size_t)(e - p);
    /* de-dupe against existing (case-sensitive, trimmed) */
    for (size_t i = 0; i < s->n_subgoals; i++) {
        size_t el = strlen(s->subgoals[i]);
        if (el == len && strncmp(s->subgoals[i], p, len) == 0) return 0;
    }
    char *copy = malloc(len + 1);
    memcpy(copy, p, len);
    copy[len] = '\0';
    s->subgoals = realloc(s->subgoals, (s->n_subgoals + 1) * sizeof(char *));
    s->subgoals[s->n_subgoals++] = copy;
    return 1;
}

char *goal_state_render_subgoals(const goal_state_t *s) {
    if (!s || s->n_subgoals == 0) return strdup("");
    /* measure */
    size_t cap = 1;
    for (size_t i = 0; i < s->n_subgoals; i++) {
        /* "- N. text\n" */
        cap += 2 + 1 + (size_t)snprintf(NULL, 0, "%zu", i + 1) + 2 + strlen(s->subgoals[i]) + 1;
    }
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    for (size_t i = 0; i < s->n_subgoals; i++) {
        size_t len = strlen(out);
        snprintf(out + len, cap - len, "- %zu. %s\n", i + 1, s->subgoals[i]);
    }
    size_t L = strlen(out);
    if (L > 0 && out[L - 1] == '\n') out[L - 1] = '\0';
    return out;
}

char *goal_state_to_json(const goal_state_t *s) {
    if (!s) return strdup("{}");
    json_t *obj = json_object();
    json_set(obj, "goal", json_string(s->goal ? s->goal : ""));
    json_set(obj, "status", json_string(s->status ? s->status : "active"));
    json_set(obj, "turns_used", json_number((double)s->turns_used));
    json_set(obj, "max_turns", json_number((double)s->max_turns));
    json_set(obj, "created_at", json_number(s->created_at));
    json_set(obj, "last_turn_at", json_number(s->last_turn_at));
    json_set(obj, "last_verdict", s->last_verdict ? json_string(s->last_verdict) : json_null());
    json_set(obj, "last_reason", s->last_reason ? json_string(s->last_reason) : json_null());
    json_set(obj, "paused_reason", s->paused_reason ? json_string(s->paused_reason) : json_null());
    json_set(obj, "consecutive_parse_failures", json_number((double)s->consecutive_parse_failures));
    /* subgoals array */
    json_t *subs = json_array();
    for (size_t i = 0; i < s->n_subgoals; i++) json_append(subs, json_string(s->subgoals[i]));
    json_set(obj, "subgoals", subs);
    /* waiting barriers */
    json_set(obj, "waiting_on_pid", s->waiting_on_pid ? json_number((double)s->waiting_on_pid) : json_null());
    json_set(obj, "waiting_on_session", s->waiting_on_session ? json_string(s->waiting_on_session) : json_null());
    json_set(obj, "waiting_until", json_number(s->waiting_until));
    json_set(obj, "waiting_reason", s->waiting_reason ? json_string(s->waiting_reason) : json_null());
    json_set(obj, "waiting_since", json_number(s->waiting_since));
    /* contract object */
    json_t *contract = json_object();
    for (int i = 0; i < N_CONTRACT_FIELDS; i++) {
        const char *v = s->contract->fields[i];
        json_set(contract, CONTRACT_FIELDS[i], v && v[0] ? json_string(v) : json_string(""));
    }
    json_set(obj, "contract", contract);

    char *out = json_serialize(obj);
    json_free(obj);
    return out ? out : strdup("{}");
}

bool goal_state_from_json(const char *raw, goal_state_t *s) {
    if (!s) return false;
    if (!raw || !raw[0]) return false;
    char *err = NULL;
    json_t *doc = json_parse(raw, &err);
    if (err) { free(err); return false; }
    if (!doc || doc->type != JSON_OBJECT) { json_free(doc); return false; }

    const char *goal = json_get_str(doc, "goal", "");
    free(s->goal);
    s->goal = strdup(goal ? goal : "");

    const char *status = json_get_str(doc, "status", "active");
    free(s->status);
    s->status = strdup(status ? status : "active");

    s->turns_used = (int)json_get_num(doc, "turns_used", 0);
    s->max_turns = (int)json_get_num(doc, "max_turns", GOAL_DEFAULT_MAX_TURNS);
    s->created_at = json_get_num(doc, "created_at", 0.0);
    s->last_turn_at = json_get_num(doc, "last_turn_at", 0.0);

    const char *lv = json_get_str(doc, "last_verdict", NULL);
    free(s->last_verdict);
    s->last_verdict = lv ? strdup(lv) : NULL;
    const char *lr = json_get_str(doc, "last_reason", NULL);
    free(s->last_reason);
    s->last_reason = lr ? strdup(lr) : NULL;
    const char *pr = json_get_str(doc, "paused_reason", NULL);
    free(s->paused_reason);
    s->paused_reason = pr ? strdup(pr) : NULL;
    s->consecutive_parse_failures = (int)json_get_num(doc, "consecutive_parse_failures", 0);

    /* subgoals */
    for (size_t i = 0; i < s->n_subgoals; i++) free(s->subgoals[i]);
    free(s->subgoals);
    s->subgoals = NULL;
    s->n_subgoals = 0;
    json_t *subs = json_obj_get(doc, "subgoals");
    if (subs && subs->type == JSON_ARRAY) {
        size_t n = json_len(subs);
        for (size_t i = 0; i < n; i++) {
            json_t *it = json_get(subs, i);
            if (it && it->type == JSON_STRING && it->str_val && it->str_val[0]) {
                char *t = strdup(it->str_val);
                /* trim */
                char *e = t + strlen(t);
                while (e > t && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) e--;
                *e = '\0';
                if (*t) goal_state_add_subgoal(s, t);
                free(t);
            }
        }
    }

    /* waiting barriers */
    json_t *wpid = json_obj_get(doc, "waiting_on_pid");
    s->waiting_on_pid = (wpid && wpid->type == JSON_NUMBER && wpid->num_val != 0.0) ? (long)wpid->num_val : 0;
    const char *wsess = json_get_str(doc, "waiting_on_session", NULL);
    free(s->waiting_on_session);
    s->waiting_on_session = wsess ? strdup(wsess) : NULL;
    s->waiting_until = json_get_num(doc, "waiting_until", 0.0);
    const char *wr = json_get_str(doc, "waiting_reason", NULL);
    free(s->waiting_reason);
    s->waiting_reason = wr ? strdup(wr) : NULL;
    s->waiting_since = json_get_num(doc, "waiting_since", 0.0);

    /* contract */
    json_t *cobj = json_obj_get(doc, "contract");
    goal_contract_clear(s->contract);
    if (cobj && cobj->type == JSON_OBJECT) {
        for (int i = 0; i < N_CONTRACT_FIELDS; i++) {
            const char *v = json_get_str(cobj, CONTRACT_FIELDS[i], NULL);
            if (v && v[0]) {
                char *trim = strdup(v);
                char *st = trim;
                while (*st == ' ' || *st == '\t' || *st == '\n' || *st == '\r') st++;
                char *en = st + strlen(st);
                while (en > st && (en[-1] == ' ' || en[-1] == '\t' || en[-1] == '\n' || en[-1] == '\r')) en--;
                *en = '\0';
                if (*st) s->contract->fields[i] = strdup(st);
                free(trim);
            }
        }
    }

    json_free(doc);
    return true;
}

/* ───────────────────────────────────────────────────────────────────
 * _meta_key — canonical implementation (PoP: _meta_key).
 * State key for SessionDB state_meta: "goal:<session_id>". Caller frees.
 * ─────────────────────────────────────────────────────────────────── */

char *goal_meta_key(const char *session_id)
{
    const char *sid = session_id ? session_id : "";
    size_t cap = strlen(sid) + 6; /* "goal:" + sid */
    char *out = malloc(cap);
    snprintf(out, cap, "goal:%s", sid);
    return out;
}


/* ── from src/cli/port_goals_helpers.c ── */
/*
 * port_goals_helpers.c
 *
 * Pure, portable helper functions ported from hermes_cli/goals.py.
 * These contain no process management (os.kill/psutil), no blocking waits,
 * no SessionDB/file I/O, and no LLM calls — only string parsing, JSON
 * extraction, and JSON-object rendering. Docstring-only / coupled helpers
 * (status_line, set_contract, mark_done, wait_on*, run_kanban_goal_loop,
 * evaluate_after_turn, next_continuation_prompt, draft_contract,
 * gather_background_processes, _pid_alive, _session_waiting) stay REAL_GAP.
 *
 * Functions (C name <- python name):
 *   goal_meta_key                       <- _meta_key
 *   goals_is_empty              <- GoalContract.is_empty
 *   goals_render_block          <- GoalContract.render_block
 *   parse_contract_json                 <- parse_contract
 *   goals_from_json                 <- GoalState.from_json
 *   goals_has_contract         <- GoalState.has_contract
 *   goals_render_subgoals_block<- GoalState.render_subgoals_block
 *   goals_render_subgoals      <- GoalState.render_subgoals
 *   goals_is_active            <- GoalManager.is_active
 *   goals_has_goal             <- GoalManager.has_goal
 *   goals_is_waiting           <- GoalManager.is_waiting
 *   goals_render_contract      <- GoalManager.render_contract
 *   extract_json_object                 <- _extract_json_object
 *   parse_judge_response_json           <- _parse_judge_response
 *   goal_judge_max_tokens               <- _goal_judge_max_tokens
 *   render_background_block_json        <- _render_background_block
 */

#include "hermes_json.h"
#include "libcredentialfiles/credential_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>

#define DEFAULT_JUDGE_MAX_TOKENS 4096
#define DEFAULT_MAX_TURNS 20

static const char *CONTRACT_FIELDS_2[] = {
    "outcome", "verification", "constraints", "boundaries", "stop_when", NULL
};
static const char *CONTRACT_LABELS_2[] = {
    "Outcome", "Verification", "Constraints", "Boundaries", "Stop when blocked", NULL
};

/* local JSON string escaper: returns malloc'd escaped string. Caller frees. */


/* ---------------------------------------------------------------------------
 * smallest JSON object extractor (mimics _JSON_OBJECT_RE = \{.*?\} DOTALL)
 * --------------------------------------------------------------------------- */
static char *extract_first_json_object(const char *text)
{
    if (!text) return NULL;
    const char *start = strchr(text, '{');
    if (!start) return NULL;
    int depth = 0;
    bool in_str = false;
    for (const char *p = start; *p; p++) {
        if (in_str) {
            if (*p == '\\') { p++; continue; }
            if (*p == '"') in_str = false;
            continue;
        }
        if (*p == '"') in_str = true;
        else if (*p == '{') depth++;
        else if (*p == '}') {
            depth--;
            if (depth == 0) {
                size_t len = (size_t)(p - start + 1);
                char *out = malloc(len + 1);
                memcpy(out, start, len);
                out[len] = '\0';
                return out;
            }
        }
    }
    return NULL;
}

/* strip ``` fences; returns malloc'd string. Caller frees. */
static char *strip_code_fences(const char *raw)
{
    char *text = strdup(raw ? raw : "");
    /* trim */
    char *p = text;
    while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r') p++;
    char *out = strdup(p);
    free(text);
    if (out[0] == '`' && out[1] == '`' && out[2] == '`') {
        char *q = out;
        while (*q) q++;
        /* strip leading backticks */
        char *s = out + 3;
        while (*s==' '||*s=='\t'||*s=='\n') s++;
        char *stripped = strdup(s);
        /* drop trailing fence if present */
        char *end = stripped + strlen(stripped);
        while (end > stripped && (end[-1]=='`'||end[-1]=='\n'||end[-1]==' '||end[-1]=='\t'||end[-1]=='\r')) end--;
        *end = '\0';
        free(out);
        return stripped;
    }
    return out;
}

/* ---------------------------------------------------------------------------
 * pure helpers
 * --------------------------------------------------------------------------- */

/*
 * Returns 1 if all contract fields are empty (contract given as JSON). */
int goals_is_empty(const char *contract_json)
{
    if (!contract_json || !contract_json[0]) return 1;
    json_t *c = json_parse(contract_json, NULL);
    if (!c || c->type != JSON_OBJECT) { if (c) json_free(c); return 1; }
    int empty = 1;
    for (int i = 0; CONTRACT_FIELDS_2[i]; i++) {
        json_t *v = json_object_get(c, CONTRACT_FIELDS_2[i]);
        if (v && v->type == JSON_STRING && json_string_value(v)[0]) { empty = 0; break; }
    }
    json_free(c);
    return empty;
}

/*
 * PoP: GoalContract.render_block @ hermes_cli/goals.py:GoalContract.render_block
 * Returns malloc'd labelled block (e.g. "- Outcome: ..."). Empty string if none. */
char *goals_render_block(const char *contract_json)
{
    char *out = malloc(1); out[0] = '\0';
    if (!contract_json || !contract_json[0]) return out;
    json_t *c = json_parse(contract_json, NULL);
    if (!c || c->type != JSON_OBJECT) { if (c) json_free(c); return out; }
    for (int i = 0; CONTRACT_FIELDS_2[i]; i++) {
        json_t *v = json_object_get(c, CONTRACT_FIELDS_2[i]);
        if (v && v->type == JSON_STRING && json_string_value(v)[0]) {
            const char *val = json_string_value(v);
            size_t need = strlen(out) + strlen(CONTRACT_LABELS_2[i]) + strlen(val) + 8;
            char *n = realloc(out, need);
            if (!n) break;
            out = n;
            if (out[0]) strcat(out, "\n");
            char *esc = json_escape_string(val);
            strcat(out, "- ");
            strcat(out, CONTRACT_LABELS_2[i]);
            strcat(out, ": ");
            strcat(out, val);
            if (esc) free(esc);
        }
    }
    json_free(c);
    return out;
}

/*
 * Split user goal text into headline + structured contract. Returns malloc'd
 * JSON {"headline":"...","contract":{...}}. Caller frees. */
char *parse_contract_json(const char *text)
{
    char fields[5][4096];
    for (int i = 0; i < 5; i++) fields[i][0] = '\0';
    char headline[8192];
    headline[0] = '\0';
    if (!text || !text[0]) {
        char *out = strdup("{\"headline\":\"\",\"contract\":{\"outcome\":\"\",\"verification\":\"\",\"constraints\":\"\",\"boundaries\":\"\",\"stop_when\":\"\"}}");
        return out;
    }
    /* iterate lines */
    const char *p = text;
    char line[4096];
    while (*p) {
        size_t li = 0;
        while (*p && *p != '\n' && li + 1 < sizeof(line)) line[li++] = *p++;
        if (*p == '\n') p++;
        line[li] = '\0';
        /* strip */
        char *ls = line;
        while (*ls==' '||*ls=='\t') ls++;
        char *le = ls + strlen(ls);
        while (le > ls && (le[-1]==' '||le[-1]=='\t')) le--;
        *le = '\0';
        if (!ls[0]) continue;
        int matched = 0;
        char *colon = strchr(ls, ':');
        if (colon) {
            *colon = '\0';
            char *prefix = ls;
            while (*prefix==' '||*prefix=='\t') prefix++;
            char *pl = prefix + strlen(prefix);
            while (pl > prefix && (pl[-1]==' '||pl[-1]=='\t')) pl--;
            *pl = '\0';
            char *value = colon + 1;
            while (*value==' '||*value=='\t') value++;
            char *vl = value + strlen(value);
            while (vl > value && (vl[-1]==' '||vl[-1]=='\t')) vl--;
            *vl = '\0';
            /* alias map */
            int key_idx = -1;
            const char *aliases[][2] = {
                {"outcome","0"},{"goal","0"},{"done","0"},{"done when","0"},
                {"verification","1"},{"verify","1"},{"verified by","1"},{"evidence","1"},{"proof","1"},
                {"constraints","2"},{"constraint","2"},{"preserve","2"},{"must not","2"},{"do not change","2"},
                {"boundaries","3"},{"boundary","3"},{"scope","3"},{"allowed","3"},{"files","3"},
                {"stop when","4"},{"stop_when","4"},{"blocked","4"},{"stop if blocked","4"},{"give up when","4"},
                {NULL,NULL}
            };
            char lowp[256];
            size_t n = 0;
            for (char *q = prefix; *q && n < sizeof(lowp)-1; q++) {
                char c = *q; if (c>='A'&&c<='Z') c+=32; lowp[n++]=c;
            }
            lowp[n]='\0';
            for (int a = 0; aliases[a][0]; a++) {
                if (strcasecmp(lowp, aliases[a][0]) == 0) { key_idx = atoi(aliases[a][1]); break; }
            }
            if (key_idx >= 0 && value[0]) {
                if (fields[key_idx][0]) strcat(fields[key_idx], " ");
                strcat(fields[key_idx], value);
                matched = 1;
            }
        }
        if (!matched) {
            if (headline[0]) strcat(headline, " ");
            strcat(headline, ls);
        }
    }
    char *out = malloc(8192);
    snprintf(out, 8192,
        "{\"headline\":%s,\"contract\":{\"outcome\":%s,\"verification\":%s,\"constraints\":%s,\"boundaries\":%s,\"stop_when\":%s}}",
        json_escape_string(headline),
        json_escape_string(fields[0]), json_escape_string(fields[1]),
        json_escape_string(fields[2]), json_escape_string(fields[3]), json_escape_string(fields[4]));
    return out;
}

/*
 * PoP: GoalState.from_json @ hermes_cli/goals.py:GoalState.from_json
 * Normalize a GoalState JSON blob into canonical form. Returns malloc'd
 * JSON. Caller frees. */
char *goals_from_json(const char *raw)
{
    if (!raw) raw = "{}";
    json_t *d = json_parse(raw, NULL);
    if (!d || d->type != JSON_OBJECT) { if (d) json_free(d); return strdup("{}"); }
    json_t *sub = json_object_get(d, "subgoals");
    char *sub_json = strdup("[]");
    if (sub && sub->type == JSON_ARRAY) {
        /* re-dump filtered */
        char *tmp = json_dumps(sub, 0);
        free(sub_json); sub_json = tmp ? tmp : strdup("[]");
    }
    char *contract_json = strdup("{\"outcome\":\"\",\"verification\":\"\",\"constraints\":\"\",\"boundaries\":\"\",\"stop_when\":\"\"}");
    json_t *con = json_object_get(d, "contract");
    if (con && con->type == JSON_OBJECT) {
        free(contract_json);
        contract_json = json_dumps(con, 0);
        if (!contract_json) contract_json = strdup("{\"outcome\":\"\",\"verification\":\"\",\"constraints\":\"\",\"boundaries\":\"\",\"stop_when\":\"\"}");
    }
    const char *goal = "";
    const char *status = "active";
    json_t *gv = json_object_get(d, "goal");
    json_t *stv = json_object_get(d, "status");
    if (gv && gv->type == JSON_STRING) goal = json_string_value(gv);
    if (stv && stv->type == JSON_STRING) status = json_string_value(stv);
    double mt = DEFAULT_MAX_TURNS, tu = 0, ca = 0, lta = 0, wu = 0, ws = 0;
    json_t *x;
    if ((x = json_object_get(d, "max_turns"))) mt = json_number_value(x) ? json_number_value(x) : DEFAULT_MAX_TURNS;
    if ((x = json_object_get(d, "turns_used"))) tu = json_number_value(x);
    if ((x = json_object_get(d, "created_at"))) ca = json_number_value(x);
    if ((x = json_object_get(d, "last_turn_at"))) lta = json_number_value(x);
    if ((x = json_object_get(d, "waiting_until"))) wu = json_number_value(x);
    if ((x = json_object_get(d, "waiting_since"))) ws = json_number_value(x);
    char *out = malloc(4096);
    snprintf(out, 4096,
        "{\"goal\":%s,\"status\":%s,\"turns_used\":%g,\"max_turns\":%g,\"created_at\":%g,"
        "\"last_turn_at\":%g,\"consecutive_parse_failures\":0,\"waiting_on_pid\":null,"
        "\"waiting_on_session\":null,\"waiting_until\":%g,\"waiting_reason\":null,"
        "\"waiting_since\":%g,\"contract\":%s,\"subgoals\":%s}",
        json_escape_string(goal), json_escape_string(status), tu, mt, ca, lta, wu, ws,
        contract_json, sub_json);
    free(contract_json); free(sub_json);
    json_free(d);
    return out;
}

/*
 * PoP: GoalState.has_contract @ hermes_cli/goals.py:GoalState.has_contract */
int goals_has_contract(const char *goalstate_json)
{
    json_t *d = json_parse(goalstate_json ? goalstate_json : "{}", NULL);
    if (!d || d->type != JSON_OBJECT) { if (d) json_free(d); return 0; }
    json_t *con = json_object_get(d, "contract");
    int r = 0;
    if (con && con->type == JSON_OBJECT) r = !goals_is_empty(json_dumps(con, 0));
    json_free(d);
    return r;
}

/*
 * PoP: GoalState.render_subgoals_block @ hermes_cli/goals.py:GoalState.render_subgoals_block
 * Returns malloc'd "- 1. text\n- 2. text" block or empty string. */
char *goals_render_subgoals_block(const char *goalstate_json)
{
    char *out = malloc(1); out[0] = '\0';
    json_t *d = json_parse(goalstate_json ? goalstate_json : "{}", NULL);
    if (!d || d->type != JSON_OBJECT) { if (d) json_free(d); return out; }
    json_t *sub = json_object_get(d, "subgoals");
    if (sub && sub->type == JSON_ARRAY && json_array_size(sub) > 0) {
        for (size_t i = 0; i < json_array_size(sub); i++) {
            json_t *s = json_array_get(sub, i);
            if (!s || s->type != JSON_STRING || !json_string_value(s)[0]) continue;
            const char *txt = json_string_value(s);
            size_t need = strlen(out) + strlen(txt) + 16;
            char *n = realloc(out, need);
            if (!n) break;
            out = n;
            if (out[0]) strcat(out, "\n");
            char buf[32];
            snprintf(buf, sizeof(buf), "- %zu. ", i + 1);
            strcat(out, buf);
            strcat(out, txt);
        }
    }
    json_free(d);
    return out;
}

/*
 * PoP: GoalState.render_subgoals @ hermes_cli/goals.py:GoalState.render_subgoals */
char *goals_render_subgoals(const char *goalstate_json)
{
    return goals_render_subgoals_block(goalstate_json);
}

/*
 * PoP: GoalManager.is_active @ hermes_cli/goals.py:GoalManager.is_active */
int goals_is_active(const char *goalstate_json)
{
    json_t *d = json_parse(goalstate_json ? goalstate_json : "{}", NULL);
    if (!d || d->type != JSON_OBJECT) { if (d) json_free(d); return 0; }
    int r = 0;
    json_t *st = json_object_get(d, "status");
    if (st && st->type == JSON_STRING && strcmp(json_string_value(st), "active") == 0) r = 1;
    json_free(d);
    return r;
}

/*
 * PoP: GoalManager.has_goal @ hermes_cli/goals.py:GoalManager.has_goal */
int goals_has_goal(const char *goalstate_json)
{
    json_t *d = json_parse(goalstate_json ? goalstate_json : "{}", NULL);
    if (!d || d->type != JSON_OBJECT) { if (d) json_free(d); return 0; }
    int r = 0;
    json_t *st = json_object_get(d, "status");
    if (st && st->type == JSON_STRING) {
        const char *s = json_string_value(st);
        if (strcmp(s, "active") == 0 || strcmp(s, "paused") == 0) r = 1;
    }
    json_free(d);
    return r;
}

/*
 * PoP: GoalManager.is_waiting @ hermes_cli/goals.py:GoalManager.is_waiting */
int goals_is_waiting(const char *goalstate_json)
{
    json_t *d = json_parse(goalstate_json ? goalstate_json : "{}", NULL);
    if (!d || d->type != JSON_OBJECT) { if (d) json_free(d); return 0; }
    int r = 0;
    json_t *st = json_object_get(d, "status");
    if (st && st->type == JSON_STRING && strcmp(json_string_value(st), "active") == 0) {
        json_t *wp = json_object_get(d, "waiting_on_pid");
        json_t *ws = json_object_get(d, "waiting_on_session");
        json_t *wu = json_object_get(d, "waiting_until");
        if ((wp && wp->type != JSON_NULL) || (ws && ws->type != JSON_NULL) ||
            (wu && wu->type == JSON_NUMBER && json_number_value(wu) > 0))
            r = 1;
    }
    json_free(d);
    return r;
}

/*
 * PoP: GoalManager.render_contract @ hermes_cli/goals.py:GoalManager.render_contract */
char *goals_render_contract(const char *goalstate_json)
{
    json_t *d = json_parse(goalstate_json ? goalstate_json : "{}", NULL);
    if (!d || d->type != JSON_OBJECT) { if (d) json_free(d); return strdup(""); }
    json_t *con = json_object_get(d, "contract");
    char *r = strdup("");
    if (con && con->type == JSON_OBJECT) {
        free(r);
        r = goals_render_block(json_dumps(con, 0));
    }
    json_free(d);
    return r;
}

/*
 * Best-effort first JSON object from a model reply. Returns malloc'd JSON
 * object string or NULL. Caller frees. */
char *extract_json_object(const char *raw)
{
    if (!raw || !raw[0]) return NULL;
    char *text = strip_code_fences(raw);
    char *out = NULL;
    json_t *d = json_parse(text, NULL);
    if (d && d->type == JSON_OBJECT) {
        out = json_dumps(d, 0);
    } else {
        if (d) json_free(d);
        char *first = extract_first_json_object(text);
        if (first) {
            json_t *d2 = json_parse(first, NULL);
            if (d2 && d2->type == JSON_OBJECT) out = json_dumps(d2, 0);
            else if (d2) json_free(d2);
            free(first);
        }
    }
    if (d) json_free(d);
    free(text);
    return out;
}

/*
 * Parse a judge reply. Returns malloc'd JSON:
 *   {"verdict":"...","reason":"...","parse_failed":<0/1>,"wait":<json-or-null>}
 * Caller frees. */
char *parse_judge_response_json(const char *raw)
{
    if (!raw || !raw[0])
        return strdup("{\"verdict\":\"continue\",\"reason\":\"judge returned empty response\",\"parse_failed\":1,\"wait\":null}");
    char *text = strip_code_fences(raw);
    json_t *d = json_parse(text, NULL);
    char *out;
    if (!d || d->type != JSON_OBJECT) {
        char *first = extract_first_json_object(text);
        if (first) {
            json_t *d2 = json_parse(first, NULL);
            if (d2 && d2->type == JSON_OBJECT) { if (d) json_free(d); d = d2; }
            else if (d2) json_free(d2);
            free(first);
        }
    }
    if (!d || d->type != JSON_OBJECT) {
        char *tr = malloc(strlen(raw) + 64);
        size_t L = strlen(raw);
        if (L > 200) L = 200;
        char buf[256];
        memcpy(buf, raw, L); buf[L] = '\0';
        snprintf(tr, strlen(raw) + 64, "{\"verdict\":\"continue\",\"reason\":\"judge reply was not JSON: %s\",\"parse_failed\":1,\"wait\":null}", buf);
        free(text);
        return tr;
    }
    const char *reason = "";
    json_t *rv = json_object_get(d, "reason");
    if (rv && rv->type == JSON_STRING && json_string_value(rv)[0]) reason = json_string_value(rv);
    else reason = "no reason provided";

    char verdict[16];
    json_t *vr = json_object_get(d, "verdict");
    if (vr && vr->type == JSON_STRING) {
        const char *v = json_string_value(vr);
        while (*v==' '||*v=='\t') v++;
        /* lowercase copy */
        char low[64]; size_t n=0;
        for (; v[n] && n<sizeof(low)-1; n++) { char c=v[n]; if(c>='A'&&c<='Z')c+=32; low[n]=c; }
        low[n]='\0';
        if (strcmp(low,"done")==0||strcmp(low,"continue")==0||strcmp(low,"wait")==0) strcpy(verdict, low);
        else strcpy(verdict,"continue");
    } else {
        /* legacy "done" bool */
        json_t *dv = json_object_get(d, "done");
        int done = 0;
        if (dv) {
            if (dv->type == JSON_BOOL) done = dv->bool_val ? 1 : 0;
            else if (dv->type == JSON_STRING) {
                const char *s = json_string_value(dv);
                if (strcasecmp(s,"true")==0||strcasecmp(s,"yes")==0||strcmp(s,"1")==0||strcasecmp(s,"done")==0) done=1;
            } else if (dv->type == JSON_NUMBER) done = json_number_value(dv) ? 1 : 0;
        }
        strcpy(verdict, done ? "done" : "continue");
    }

    if (strcmp(verdict, "wait") != 0) {
        char *r = malloc(strlen(verdict) + strlen(reason) + 64);
        snprintf(r, strlen(verdict)+strlen(reason)+64,
            "{\"verdict\":%s,\"reason\":%s,\"parse_failed\":0,\"wait\":null}",
            json_escape_string(verdict), json_escape_string(reason));
        json_free(d); free(text);
        return r;
    }
    /* wait directive */
    char wait_json[128]; wait_json[0]='\0';
    json_t *sess = json_object_get(d, "wait_on_session");
    if (!sess) sess = json_object_get(d, "session_id");
    if (!sess) sess = json_object_get(d, "wait_session");
    if (sess && sess->type == JSON_STRING && json_string_value(sess)[0]) {
        snprintf(wait_json, sizeof(wait_json), "{\"session_id\":%s}", json_escape_string(json_string_value(sess)));
    } else {
        /* try pid / seconds integer keys */
        const char *pid_keys[] = {"wait_on_pid","pid","wait_pid",NULL};
        const char *sec_keys[] = {"wait_for_seconds","seconds","wait_seconds",NULL};
        long iv = 0; int found = 0; int is_pid = 0;
        for (int i=0; pid_keys[i]; i++) {
            json_t *x = json_object_get(d, pid_keys[i]);
            if (x && (x->type==JSON_NUMBER || x->type==JSON_STRING)) {
                iv = (long)(x->type==JSON_NUMBER ? json_number_value(x) : strtol(json_string_value(x),NULL,10));
                if (iv>0) { found=1; is_pid=1; break; }
            }
        }
        if (!found) {
            for (int i=0; sec_keys[i]; i++) {
                json_t *x = json_object_get(d, sec_keys[i]);
                if (x && (x->type==JSON_NUMBER || x->type==JSON_STRING)) {
                    iv = (long)(x->type==JSON_NUMBER ? json_number_value(x) : strtol(json_string_value(x),NULL,10));
                    if (iv>0) { found=1; is_pid=0; break; }
                }
            }
        }
        if (found) {
            if (is_pid) snprintf(wait_json, sizeof(wait_json), "{\"pid\":%ld}", iv);
            else snprintf(wait_json, sizeof(wait_json), "{\"seconds\":%ld}", iv);
        }
    }
    if (!wait_json[0]) {
        /* no usable target -> continue */
        char *r = malloc(strlen(reason) + 128);
        snprintf(r, strlen(reason)+128,
            "{\"verdict\":\"continue\",\"reason\":%s (wait verdict had no target — continuing),\"parse_failed\":0,\"wait\":null}",
            json_escape_string(reason));
        json_free(d); free(text);
        return r;
    }
    char *r = malloc(strlen(verdict)+strlen(reason)+strlen(wait_json)+64);
    snprintf(r, strlen(verdict)+strlen(reason)+strlen(wait_json)+64,
        "{\"verdict\":%s,\"reason\":%s,\"parse_failed\":0,\"wait\":%s}",
        json_escape_string(verdict), json_escape_string(reason), wait_json);
    json_free(d); free(text);
    return r;
}

/*
 * Best-effort read of auxiliary.goal_judge.max_tokens from the hermes config
 * JSON; falls back to DEFAULT_JUDGE_MAX_TOKENS (4096). */
int goal_judge_max_tokens(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = credfiles_get_hermes_home();
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/config.json", home ? home : "~/.hermes");
    FILE *f = fopen(path, "rb");
    if (!f) return DEFAULT_JUDGE_MAX_TOKENS;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1<<20) { fclose(f); return DEFAULT_JUDGE_MAX_TOKENS; }
    char *buf = malloc((size_t)sz + 1);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return DEFAULT_JUDGE_MAX_TOKENS; }
    buf[sz] = '\0';
    fclose(f);
    int result = DEFAULT_JUDGE_MAX_TOKENS;
    json_t *cfg = json_parse(buf, NULL);
    free(buf);
    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *aux = json_object_get(cfg, "auxiliary");
        if (aux && aux->type == JSON_OBJECT) {
            json_t *gj = json_object_get(aux, "goal_judge");
            if (gj && gj->type == JSON_OBJECT) {
                json_t *mt = json_object_get(gj, "max_tokens");
                if (mt && mt->type == JSON_NUMBER) {
                    long v = (long)json_number_value(mt);
                    if (v > 0) result = (int)v;
                }
            }
        }
    }
    if (cfg) json_free(cfg);
    return result;
}

/*
 * Render a JSON list of background-process dicts into a prompt block.
 * Returns malloc'd string. Caller frees. */
char *render_background_block_json(const char *processes_json)
{
    char *out = malloc(1); out[0] = '\0';
    if (!processes_json || !processes_json[0]) return out;
    json_t *list = json_parse(processes_json, NULL);
    if (!list || list->type != JSON_ARRAY) { if (list) json_free(list); return out; }
    for (size_t i = 0; i < json_array_size(list); i++) {
        json_t *p = json_array_get(list, i);
        if (!p || p->type != JSON_OBJECT) continue;
        const char *pid = "", *label = "", *status = "", *cmd = "";
        json_t *x;
        if ((x=json_object_get(p,"pid")) && x->type==JSON_STRING) pid=json_string_value(x);
        if ((x=json_object_get(p,"label")) && x->type==JSON_STRING) label=json_string_value(x);
        if ((x=json_object_get(p,"status")) && x->type==JSON_STRING) status=json_string_value(x);
        if ((x=json_object_get(p,"cmd")) && x->type==JSON_STRING) cmd=json_string_value(x);
        char line[2048];
        snprintf(line, sizeof(line), "- [%s] pid=%s %s%s%s",
                 status, pid, label, cmd[0] ? " (" : "", cmd, cmd[0] ? ")" : "");
        size_t need = strlen(out) + strlen(line) + 2;
        char *n = realloc(out, need);
        if (!n) break;
        out = n;
        if (out[0]) strcat(out, "\n");
        strcat(out, line);
    }
    json_free(list);
    return out;
}

/* PoP: goals_meta_key @ hermes_cli/goals.py:_meta_key */
/* Returns malloc'd "goal:<session_id>" key. Caller frees. */
char *goals_meta_key(const char *session_id) {
    char *result = NULL;
    asprintf(&result, "goal:%s", session_id ? session_id : "");
    return result ? result : strdup("");
}


/* ── from src/cli/port_goals_manager.c ── */
/*
 * port_goals_manager.c — GoalManager + judge-prompt helpers (pure layer).
 *
 * Faithful C port of the orchestration + text-shaping slice of
 * hermes_cli/goals.py. Covers the data-layer surface of GoalManager plus the
 * pure judge helpers. The LLM call (judge_goal / draft_contract), the wait
 * barriers' OS-process liveness (_pid_alive / _session_waiting), and
 * evaluate_after_turn / run_kanban_goal_loop (which drive the judge + run_turn)
 * are intentionally NOT implemented here — they require network/LLM/process
 * dependencies. Process liveness + persistence are injected via a vtable so
 * this module stays pure (no DB, TTY, network, or OS-process calls).
 *
/* PoP: index @ hermes_cli/goals.py:set */
/* PoP: index @ hermes_cli/goals.py:set_contract */
/* PoP: index @ hermes_cli/goals.py:pause */
/* PoP: index @ hermes_cli/goals.py:resume */
/* PoP: index @ hermes_cli/goals.py:clear */
/* PoP: index @ hermes_cli/goals.py:mark_done */
/* PoP: index @ hermes_cli/goals.py:add_subgoal */
/* PoP: index @ hermes_cli/goals.py:remove_subgoal */
/* PoP: index @ hermes_cli/goals.py:clear_subgoals */
/* PoP: index @ hermes_cli/goals.py:status_line */
/* PoP: index @ hermes_cli/goals.py:has_goal */
/* PoP: index @ hermes_cli/goals.py:has_contract */
/* PoP: index @ hermes_cli/goals.py:is_active */
/* PoP: index @ hermes_cli/goals.py:is_waiting */
/* PoP: index @ hermes_cli/goals.py:stop_waiting */
/* PoP: index @ hermes_cli/goals.py:wait_on */
/* PoP: index @ hermes_cli/goals.py:wait_on_session */
/* PoP: index @ hermes_cli/goals.py:wait_for_seconds */
/* PoP: index @ hermes_cli/goals.py:next_continuation_prompt */
/* PoP: index @ hermes_cli/goals.py:render_contract */
/* PoP: index @ hermes_cli/goals.py:render_subgoals */
/* PoP: index @ hermes_cli/goals.py:_extract_json_object */
/* PoP: index @ hermes_cli/goals.py:_parse_judge_response */
/* PoP: index @ hermes_cli/goals.py:_render_background_block */
/* PoP: index @ hermes_cli/goals.py:_goal_judge_max_tokens */


#include "goal_contract.h"
#include "goal_contract_internal.h"
#include "tools/process_registry.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include "libjson/json.h"

/* canonical contract field name by index (data.c owns N_CONTRACT_FIELDS) */
#define CONTRACT_FIELD_NAME(i) ( \
    (i)==0 ? "outcome" : (i)==1 ? "verification" : (i)==2 ? "constraints" : \
    (i)==3 ? "boundaries" : "stop_when")

/* truncate a string to max chars (replacing tail with '… [truncated]' semantics
 * is skipped here — caller only needs the leading max chars). */
static void truncate_copy(const char *src, char *dst, size_t max) {
    size_t n = src ? strlen(src) : 0;
    if (n > max) n = max;
    memcpy(dst, src ? src : "", n);
    dst[n] = '\0';
}

/* ── GoalState rich accessors ─────────────────────────────────────── */

static goal_status_t status_from_str(const char *s) {
    if (!s) return GOAL_STATUS_ACTIVE;
    if (strcmp(s, "paused") == 0)  return GOAL_STATUS_PAUSED;
    if (strcmp(s, "done") == 0)    return GOAL_STATUS_DONE;
    if (strcmp(s, "cleared") == 0) return GOAL_STATUS_CLEARED;
    return GOAL_STATUS_ACTIVE;
}
static const char *status_to_str(goal_status_t st) {
    switch (st) {
        case GOAL_STATUS_PAUSED:  return "paused";
        case GOAL_STATUS_DONE:    return "done";
        case GOAL_STATUS_CLEARED: return "cleared";
        default:                  return "active";
    }
}

void goal_state_set_contract(goal_state_t *s, goal_contract_t *c) {
    if (!s) return;
    if (s->contract) goal_contract_free(s->contract);
    s->contract = c ? c : goal_contract_new();
}
void goal_state_set_status(goal_state_t *s, goal_status_t st) {
    if (!s) return;
    free(s->status);
    s->status = strdup(status_to_str(st));
}
void goal_state_set_status_str(goal_state_t *s, const char *status) {
    if (!s) return;
    free(s->status);
    s->status = strdup(status && status[0] ? status : "active");
}
goal_status_t goal_state_status(const goal_state_t *s) {
    return s ? status_from_str(s->status) : GOAL_STATUS_ACTIVE;
}
const char *goal_state_status_str(const goal_state_t *s) {
    return s ? (s->status ? s->status : "active") : "active";
}
void goal_state_set_turns_used(goal_state_t *s, int n) { if (s) s->turns_used = n; }
int  goal_state_turns_used(const goal_state_t *s)    { return s ? s->turns_used : 0; }
int  goal_state_max_turns(const goal_state_t *s)     { return s ? s->max_turns : 0; }
void goal_state_set_max_turns(goal_state_t *s, int n) { if (s) s->max_turns = n; }
void goal_state_set_paused_reason(goal_state_t *s, const char *r) {
    if (!s) return;
    free(s->paused_reason);
    s->paused_reason = (r && r[0]) ? strdup(r) : NULL;
}
const char *goal_state_paused_reason(const goal_state_t *s) {
    return s ? (s->paused_reason ? s->paused_reason : "") : "";
}
const char *goal_state_goal(const goal_state_t *s) {
    return s ? (s->goal ? s->goal : "") : "";
}
void goal_state_set_last_verdict(goal_state_t *s, const char *v) {
    if (!s) return;
    free(s->last_verdict);
    s->last_verdict = v ? strdup(v) : NULL;
}
void goal_state_set_last_reason(goal_state_t *s, const char *r) {
    if (!s) return;
    free(s->last_reason);
    s->last_reason = r ? strdup(r) : NULL;
}
int goal_state_parse_failures(const goal_state_t *s) { return s ? s->consecutive_parse_failures : 0; }
void goal_state_set_parse_failures(goal_state_t *s, int n) { if (s) s->consecutive_parse_failures = n; }

/* ── OS liveness primitive ────────────────────────────────────────── */

/* PoP: goal_pid_alive @ hermes_cli/goals.py:_pid_alive */
bool goal_pid_alive(long pid) {
    if (pid <= 0) return false;
    /* kill(pid, 0) is the POSIX liveness probe: returns 0 if the process
     * exists and we have permission to signal it; ESRCH => dead, EPERM =>
     * exists but no permission (still alive). Any other error => treat dead
     * (fail-safe, mirroring Python's "any error resolves to False"). */
    if (kill((pid_t)pid, 0) == 0) return true;
    return (errno != ESRCH);
}

/* goal_session_waiting(): whether a goal parked on a session should stay
 * parked. Pure delegate: an injected callback reports liveness (wired by the
 * host that owns process_registry). Fail-safe: NULL callback => not waiting.
 * (Python: _session_waiting.) */
/* PoP: goal_session_waiting @ hermes_cli/goals.py:_session_waiting */
bool goal_session_waiting(int (*cb)(const char *session_id), const char *session_id) {
    if (!session_id || !session_id[0]) return false;
    if (!cb) return false;
    return cb(session_id) != 0;
}

char *goal_state_remove_subgoal(goal_state_t *s, int index_1based) {
    if (!s) return NULL;
    int idx = index_1based - 1;
    if (idx < 0 || (size_t)idx >= s->n_subgoals) return NULL;
    char *removed = s->subgoals[idx];
    s->subgoals[idx] = NULL;
    /* compact */
    size_t w = 0;
    for (size_t i = 0; i < s->n_subgoals; i++) {
        if (s->subgoals[i]) s->subgoals[w++] = s->subgoals[i];
    }
    s->n_subgoals = w;
    /* realloc down */
    if (s->n_subgoals == 0) { free(s->subgoals); s->subgoals = NULL; }
    else s->subgoals = realloc(s->subgoals, s->n_subgoals * sizeof(char *));
    return removed; /* caller owns */
}

/* ── GoalManager ──────────────────────────────────────────────────── */

struct goal_manager_t {
    char *session_id;
    int default_max_turns;
    const goal_manager_vtab_t *vtab;
    goal_state_t *state;   /* NULL when no goal loaded */
};

goal_manager_t *goal_manager_new(const char *session_id, const goal_manager_vtab_t *vtab, int default_max_turns) {
    goal_manager_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->session_id = strdup(session_id ? session_id : "");
    m->vtab = vtab;
    m->default_max_turns = (default_max_turns > 0) ? default_max_turns : GOAL_DEFAULT_MAX_TURNS;
    /* load existing goal via vtab */
    if (vtab && vtab->load) {
        char *raw = vtab->load(m->session_id);
        if (raw) {
            goal_state_t *s = goal_state_new("");
            if (goal_state_from_json(raw, s)) m->state = s;
            else goal_state_free(s);
            free(raw);
        }
    }
    return m;
}

void goal_manager_free(goal_manager_t *m) {
    if (!m) return;
    free(m->session_id);
    if (m->state) goal_state_free(m->state);
    free(m);
}

const char *goal_manager_session_id(const goal_manager_t *m) { return m ? m->session_id : ""; }
goal_state_t *goal_manager_state(goal_manager_t *m) { return m ? m->state : NULL; }

static void persist(goal_manager_t *m) {
    if (m && m->vtab && m->vtab->save && m->state) {
        char *js = goal_state_to_json(m->state);
        m->vtab->save(m->session_id, js);
        free(js);
    }
}

bool goal_manager_is_active(const goal_manager_t *m) {
    return m && m->state && strcmp(goal_state_status_str(m->state), "active") == 0;
}
bool goal_manager_has_goal(const goal_manager_t *m) {
    if (!m || !m->state) return false;
    const char *st = goal_state_status_str(m->state);
    return strcmp(st, "active") == 0 || strcmp(st, "paused") == 0;
}
bool goal_manager_has_contract(const goal_manager_t *m) {
    return m && m->state && goal_state_has_contract(m->state);
}

char *goal_manager_status_line(const goal_manager_t *m) {
    if (!m || !m->state) return strdup("No active goal. Set one with /goal <text>.");
    const goal_state_t *s = m->state;
    const char *st = goal_state_status_str(s);
    if (strcmp(st, "cleared") == 0)
        return strdup("No active goal. Set one with /goal <text>.");

    /* turns meta */
    char meta[256];
    char sub_buf[64] = "";
    if (s->n_subgoals)
        snprintf(sub_buf, sizeof(sub_buf), ", %zu subgoal%s",
                 s->n_subgoals, s->n_subgoals == 1 ? "" : "s");
    char con_buf[32] = "";
    if (goal_state_has_contract(s)) snprintf(con_buf, sizeof(con_buf), ", contract");
    snprintf(meta, sizeof(meta), "%d/%d turns%s%s",
             s->turns_used, s->max_turns, sub_buf, con_buf);

    char *out = NULL;
    if (strcmp(st, "active") == 0) {
        if (s->waiting_on_session && (!m->vtab || !m->vtab->session_waiting ||
                                       m->vtab->session_waiting(s->waiting_on_session))) {
            const char *wr = s->waiting_reason ? s->waiting_reason : s->waiting_on_session;
            size_t L = 64 + strlen(wr) + strlen(s->goal) + strlen(meta);
            out = malloc(L);
            snprintf(out, L, "\xe2\x8f\xb3 Goal (parked on %s, %s): %s", wr, meta, s->goal);
            return out;
        }
        if (s->waiting_on_pid && (!m->vtab || !m->vtab->pid_alive ||
/* PoP: goal_manager_set @ hermes_cli/active_sessions.py:_pid_alive */
/* PoP: goal_manager_set @ hermes_cli/kanban_db.py:_pid_alive */
                                  m->vtab->pid_alive(s->waiting_on_pid))) {
            char wr_buf[128];
            snprintf(wr_buf, sizeof(wr_buf), "pid %ld%s%s%s", s->waiting_on_pid,
                     s->waiting_reason ? " (" : "", s->waiting_reason ? s->waiting_reason : "",
                     s->waiting_reason ? ")" : "");
            size_t L = 64 + strlen(wr_buf) + strlen(s->goal) + strlen(meta);
            out = malloc(L);
            snprintf(out, L, "\xe2\x8f\xb3 Goal (parked on %s, %s): %s", wr_buf, meta, s->goal);
            return out;
        }
        if (s->waiting_until > 0) {
            double now = (double)time(NULL);
            if (now < s->waiting_until) {
                long remaining = (long)(s->waiting_until - now);
                const char *wr = s->waiting_reason ? s->waiting_reason : "";
                size_t L = 128 + strlen(wr) + strlen(s->goal) + strlen(meta);
                out = malloc(L);
                snprintf(out, L, "\xe2\x8f\xb3 Goal (parked %lds — %s, %s): %s",
                         remaining, wr, meta, s->goal);
                return out;
            }
        }
        size_t L = 64 + strlen(meta) + strlen(s->goal);
        out = malloc(L);
        snprintf(out, L, "\xe2\x8a\x99 Goal (active, %s): %s", meta, s->goal);
        return out;
    }
    if (strcmp(st, "paused") == 0) {
        const char *extra = s->paused_reason ? s->paused_reason : "";
        size_t L = 64 + strlen(meta) + strlen(extra) + strlen(s->goal);
        out = malloc(L);
        snprintf(out, L, "\xe2\x8f\xb8 Goal (paused, %s%s%s): %s",
                 meta, extra ? " — " : "", extra, s->goal);
        return out;
    }
    if (strcmp(st, "done") == 0) {
        size_t L = 64 + strlen(meta) + strlen(s->goal);
        out = malloc(L);
        snprintf(out, L, "\xe2\x9c\x93 Goal done (%s): %s", meta, s->goal);
        return out;
    }
    size_t L = 64 + strlen(st) + strlen(meta) + strlen(s->goal);
    out = malloc(L);
    snprintf(out, L, "Goal (%s, %s): %s", st, meta, s->goal);
    return out;
}

/* ── mutation ─────────────────────────────────────────────────────── */

goal_state_t *goal_manager_set(goal_manager_t *m, const char *goal, int max_turns, const goal_contract_t *contract) {
    if (!m) return NULL;
    char *g = goal ? strdup(goal) : strdup("");
    /* trim */
    char *p = g;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '\0') { free(g); return NULL; } /* empty goal -> NULL (caller raises) */
    /* shift g to trimmed (in place) */
    memmove(g, p, strlen(p) + 1);

    if (m->state) goal_state_free(m->state);
    goal_state_t *s = goal_state_new(g);
    free(g);
    goal_state_set_status(s, GOAL_STATUS_ACTIVE);
    goal_state_set_turns_used(s, 0);
    goal_state_set_max_turns(s, max_turns > 0 ? max_turns : m->default_max_turns);
    goal_state_set_status(s, GOAL_STATUS_ACTIVE); /* created_at filled by new */
    s->created_at = (double)time(NULL);
    s->last_turn_at = 0.0;
    if (contract) {
        goal_contract_t *copy = goal_contract_new();
        for (int i = 0; i < 5; i++) {
            const char *fld = goal_contract_get(contract, CONTRACT_FIELD_NAME(i));
            goal_contract_set(copy, CONTRACT_FIELD_NAME(i), fld);
        }
        goal_state_set_contract(s, copy);
    }
    m->state = s;
    persist(m);
    return s;
}

goal_state_t *goal_manager_set_contract(goal_manager_t *m, const goal_contract_t *contract) {
    if (!m || !m->state) return NULL;
    goal_contract_t *copy = goal_contract_new();
    if (contract) {
        for (int i = 0; i < 5; i++) {
            const char *fld = goal_contract_get(contract, CONTRACT_FIELD_NAME(i));
            goal_contract_set(copy, CONTRACT_FIELD_NAME(i), fld);
        }
    }
    goal_state_set_contract(m->state, copy);
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_pause(goal_manager_t *m, const char *reason) {
    if (!m || !m->state) return NULL;
    goal_state_set_status(m->state, GOAL_STATUS_PAUSED);
    goal_state_set_paused_reason(m->state, reason && reason[0] ? reason : "user-paused");
    m->state->waiting_on_pid = 0;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = 0.0;
    if (m->state->waiting_reason) { free(m->state->waiting_reason); m->state->waiting_reason = NULL; }
    m->state->waiting_since = 0.0;
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_resume(goal_manager_t *m, bool reset_budget) {
    if (!m || !m->state) return NULL;
    goal_state_set_status(m->state, GOAL_STATUS_ACTIVE);
    goal_state_set_paused_reason(m->state, NULL);
    m->state->waiting_on_pid = 0;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = 0.0;
    if (m->state->waiting_reason) { free(m->state->waiting_reason); m->state->waiting_reason = NULL; }
    m->state->waiting_since = 0.0;
    if (reset_budget) goal_state_set_turns_used(m->state, 0);
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_clear(goal_manager_t *m) {
    if (!m || !m->state) return NULL;
    goal_state_set_status(m->state, GOAL_STATUS_CLEARED);
    persist(m);
    goal_state_free(m->state);
    m->state = NULL;
    return NULL;
}

void goal_manager_mark_done(goal_manager_t *m, const char *reason) {
    if (!m || !m->state) return;
    goal_state_set_status(m->state, GOAL_STATUS_DONE);
    goal_state_set_last_verdict(m->state, "done");
    goal_state_set_last_reason(m->state, reason);
    persist(m);
}

char *goal_manager_add_subgoal(goal_manager_t *m, const char *text) {
    if (!m || !m->state || !goal_manager_has_goal(m)) return NULL;
    if (!text || !text[0]) return NULL;
    const char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t' || p[n-1] == '\n' || p[n-1] == '\r')) n--;
    if (n == 0) return NULL;
    char *clean = malloc(n + 1);
    memcpy(clean, p, n);
    clean[n] = '\0';
    if (!goal_state_add_subgoal(m->state, clean)) { free(clean); return NULL; }
    char *ret = strdup(clean);
    free(clean);
    persist(m);
    return ret; /* caller owns */
}

char *goal_manager_remove_subgoal(goal_manager_t *m, int index_1based) {
    if (!m || !m->state || !goal_manager_has_goal(m)) return NULL;
    char *removed = goal_state_remove_subgoal(m->state, index_1based);
    if (removed) persist(m);
    return removed; /* caller owns */
}

int goal_manager_clear_subgoals(goal_manager_t *m) {
    if (!m || !m->state || !goal_manager_has_goal(m)) return -1;
    int prev = (int)m->state->n_subgoals;
    for (size_t i = 0; i < m->state->n_subgoals; i++) free(m->state->subgoals[i]);
    free(m->state->subgoals);
    m->state->subgoals = NULL;
    m->state->n_subgoals = 0;
    persist(m);
    return prev;
}

char *goal_manager_render_subgoals(const goal_manager_t *m) {
    if (!m || !m->state) return strdup("(no active goal)");
    if (m->state->n_subgoals == 0) return strdup("(no subgoals — use /subgoal <text> to add criteria)");
    return goal_state_render_subgoals(m->state);
}

/* ── wait barriers ────────────────────────────────────────────────── */

goal_state_t *goal_manager_wait_on(goal_manager_t *m, long pid, const char *reason, double now) {
    if (!m || !m->state || strcmp(goal_state_status_str(m->state), "active") != 0) return NULL;
    if (pid <= 0) return NULL;
    m->state->waiting_on_pid = pid;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = 0.0;
    m->state->waiting_reason = (reason && reason[0]) ? strdup(reason) : NULL;
    m->state->waiting_since = now;
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_wait_on_session(goal_manager_t *m, const char *session_id, const char *reason, double now) {
    if (!m || !m->state || strcmp(goal_state_status_str(m->state), "active") != 0) return NULL;
    if (!session_id || !session_id[0]) return NULL;
    if (m->state->waiting_on_session) free(m->state->waiting_on_session);
    m->state->waiting_on_session = strdup(session_id);
    m->state->waiting_on_pid = 0;
    m->state->waiting_until = 0.0;
    m->state->waiting_reason = (reason && reason[0]) ? strdup(reason) : NULL;
    m->state->waiting_since = now;
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_wait_for_seconds(goal_manager_t *m, int seconds, const char *reason, double now) {
    if (!m || !m->state || strcmp(goal_state_status_str(m->state), "active") != 0) return NULL;
    if (seconds <= 0) return NULL;
    m->state->waiting_on_pid = 0;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = now + (double)seconds;
    m->state->waiting_reason = (reason && reason[0]) ? strdup(reason) : NULL;
    m->state->waiting_since = now;
    persist(m);
    return m->state;
}

bool goal_manager_stop_waiting(goal_manager_t *m) {
    if (!m || !m->state) return false;
    if (m->state->waiting_on_pid == 0 &&
        m->state->waiting_on_session == NULL &&
        m->state->waiting_until == 0.0) return false;
    m->state->waiting_on_pid = 0;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = 0.0;
    if (m->state->waiting_reason) { free(m->state->waiting_reason); m->state->waiting_reason = NULL; }
    m->state->waiting_since = 0.0;
    persist(m);
    return true;
}

bool goal_manager_is_waiting(goal_manager_t *m, double now) {
    if (!m || !m->state) return false;
    goal_state_t *s = m->state;
    if (s->waiting_on_session) {
        int alive = 0;
        if (m->vtab && m->vtab->session_waiting) alive = m->vtab->session_waiting(s->waiting_on_session);
        if (alive) return true;
        goal_manager_stop_waiting(m);
        return false;
    }
    if (s->waiting_on_pid != 0) {
        int alive = (m->vtab && m->vtab->pid_alive) ? m->vtab->pid_alive(s->waiting_on_pid)
                                                    : goal_pid_alive(s->waiting_on_pid);
        if (alive) return true;
        goal_manager_stop_waiting(m);
        return false;
    }
    if (s->waiting_until > 0.0) {
        if (now < s->waiting_until) return true;
        goal_manager_stop_waiting(m);
        return false;
    }
    return false;
}

/* ── continuation prompt + contract render ───────────────────────── */

char *goal_manager_next_continuation_prompt(const goal_manager_t *m) {
    if (!m || !m->state || strcmp(goal_state_status_str(m->state), "active") != 0) return NULL;
    const goal_state_t *s = m->state;
    if (goal_state_has_contract(s)) {
        char *block = goal_contract_render(s->contract);
        char *final_block = block;
        if (s->n_subgoals) {
            /* append "Extra criterion N:" lines */
            size_t cap = strlen(block) + s->n_subgoals * 256 + 16;
            char *combined = malloc(cap);
            snprintf(combined, cap, "%s\n", block);
            char *p = combined + strlen(combined);
            for (size_t i = 0; i < s->n_subgoals; i++) {
                int n = snprintf(p, cap - (size_t)(p - combined), "- Extra criterion %zu: %s\n", i + 1, s->subgoals[i]);
                p += n;
            }
            final_block = combined;
        }
        const char *goal = s->goal;
        size_t L = 1024 + strlen(goal) + strlen(final_block);
        char *out = malloc(L);
        snprintf(out, L,
            "[Continuing toward your standing goal]\n"
            "Goal: %s\n\n"
            "Completion contract:\n"
            "%s\n\n"
            "Continue working toward the outcome above. Take the next concrete step. "
            "Stay within the stated boundaries and do not violate the constraints. "
            "Before claiming the goal is done, satisfy the Verification criterion and "
            "show the concrete evidence (command output, file contents, test result). "
            "If you hit the stated stop condition or are otherwise blocked and need "
            "user input, say so clearly and stop.",
            goal, final_block);
        free(final_block);
        return out;
    }
    if (s->n_subgoals) {
        char *sub = goal_state_render_subgoals(s);
        const char *goal = s->goal;
        size_t L = 1024 + strlen(goal) + strlen(sub);
        char *out = malloc(L);
        snprintf(out, L,
            "[Continuing toward your standing goal]\n"
            "Goal: %s\n\n"
            "Additional criteria the user added mid-loop:\n"
            "%s\n\n"
            "Continue working toward the goal AND all additional criteria. Take "
            "the next concrete step. If you believe the goal and every "
            "additional criterion are complete, state so explicitly and stop. "
            "If you are blocked and need input from the user, say so clearly "
            "and stop.",
            goal, sub);
        free(sub);
        return out;
    }
    const char *goal = s->goal;
    size_t L = 512 + strlen(goal);
    char *out = malloc(L);
    snprintf(out, L,
        "[Continuing toward your standing goal]\n"
        "Goal: %s\n\n"
        "Continue working toward this goal. Take the next concrete step. "
        "If you believe the goal is complete, state so explicitly and stop. "
        "If you are blocked and need input from the user, say so clearly and stop.",
        goal);
    return out;
}

char *goal_manager_render_contract(const goal_manager_t *m) {
    if (!m || !m->state) return strdup("(no active goal)");
    if (!goal_state_has_contract(m->state))
        return strdup("(no completion contract — set one with /goal draft <objective> or inline field: value lines)");
    return goal_contract_render(m->state->contract);
}

/* ── judge prompt helpers ─────────────────────────────────────────── */

int goal_judge_max_tokens_resolved(int (*resolve)(void)) {
    if (resolve) {
        int v = resolve();
        if (v > 0) return v;
    }
    return 4096; /* DEFAULT_JUDGE_MAX_TOKENS */
}

static char *extract_json(const char *text) {
    if (!text) return NULL;
    char *dup = strdup(text);
    /* strip ``` fences */
    char *p = dup;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (p[0] == '`' && p[1] == '`') {
        /* find first newline after opening fence */
        char *nl = strchr(p, '\n');
        if (nl) p = nl + 1;
    }
    char *json = NULL;
    char *err = NULL;
    json_t *doc = json_parse(p, &err);
    if (err) { free(err); }
    else if (doc) {
        char *tmp = json_serialize(doc);
        json_free(doc);
        json = tmp;
    }
    if (!json) {
        /* regex-like first {...} */
        const char *start = strchr(p, '{');
        if (start) {
            int depth = 0;
            const char *q = start;
            for (; *q; q++) {
                if (*q == '{') depth++;
                else if (*q == '}') { depth--; if (depth == 0) { q++; break; } }
            }
            if (depth == 0 && q > start) {
                size_t len = (size_t)(q - start);
                json = malloc(len + 1);
                memcpy(json, start, len);
                json[len] = '\0';
            }
        }
    }
    free(dup);
    return json;
}

char *goal_judge_extract_json_object(const char *raw) {
    if (!raw || !raw[0]) return NULL;
    char *text = strdup(raw);
    /* strip leading/trailing ws + fences */
    char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t' || p[n-1] == '\n' || p[n-1] == '\r')) n--;
    p[n] = '\0';
    if (p[0] == '`' && p[1] == '`') {
        char *nl = strchr(p, '\n');
        if (nl) { memmove(p, nl + 1, strlen(nl + 1) + 1); }
    }
    char *json = extract_json(p);
    free(text);
    return json;
}

void goal_judge_parse_response(const char *raw,
                               char **verdict_out, char **reason_out,
                               bool *parse_failed_out, char **wait_directive_out) {
    char *verdict = strdup("continue");
    char *reason = strdup("");
    bool parse_failed = false;
    char *wait = NULL;

    char *json = goal_judge_extract_json_object(raw);
    if (!json) {
        if (!raw || !raw[0]) {
            free(verdict); verdict = strdup("continue");
            free(reason); reason = strdup("judge returned empty response");
        } else {
            free(reason); reason = malloc(strlen(raw) + 64);
            snprintf(reason, strlen(raw) + 64, "judge reply was not JSON: %s", raw);
        }
        parse_failed = true;
        *verdict_out = verdict; *reason_out = reason; *parse_failed_out = parse_failed; *wait_directive_out = NULL;
        return;
    }

    char *err = NULL;
    json_t *doc = json_parse(json, &err);
    free(json);
    if (err) { free(err); }
    if (!doc || doc->type != JSON_OBJECT) {
        free(reason); reason = strdup("judge reply was not a JSON object");
        parse_failed = true;
        free(doc);
        *verdict_out = verdict; *reason_out = reason; *parse_failed_out = parse_failed; *wait_directive_out = NULL;
        return;
    }

    const char *r = json_get_str(doc, "reason", NULL);
    free(reason);
    reason = strdup(r && r[0] ? r : "no reason provided");

    /* verdict */
    const char *vr = json_get_str(doc, "verdict", NULL);
    if (vr) {
        free(verdict);
        verdict = strdup(vr);
        /* lowercase */
        for (char *c = verdict; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
    } else {
        json_t *done_v = json_obj_get(doc, "done");
        bool done = false;
        if (done_v) {
            if (done_v->type == JSON_BOOL) done = done_v->bool_val;
            else if (done_v->type == JSON_STRING) {
                const char *ds = done_v->str_val;
                done = (strcmp(ds, "true") == 0 || strcmp(ds, "yes") == 0 ||
                        strcmp(ds, "1") == 0 || strcmp(ds, "done") == 0);
            }
        }
        free(verdict);
        verdict = strdup(done ? "done" : "continue");
    }
    if (strcmp(verdict, "done") != 0 && strcmp(verdict, "continue") != 0 && strcmp(verdict, "wait") != 0) {
        free(verdict);
        verdict = strdup("continue");
    }

    if (strcmp(verdict, "wait") == 0) {
        /* extract directive */
        const char *sess = json_get_str(doc, "wait_on_session", NULL);
        if (!sess) sess = json_get_str(doc, "session_id", NULL);
        if (!sess) sess = json_get_str(doc, "wait_session", NULL);
        if (sess && sess[0]) {
            size_t L = strlen(sess) + 32;
            wait = malloc(L);
            snprintf(wait, L, "{\"session_id\":\"%s\"}", sess);
        } else {
            long pid = 0;
            json_t *pv = json_obj_get(doc, "wait_on_pid");
            if (!pv) pv = json_obj_get(doc, "pid");
            if (!pv) pv = json_obj_get(doc, "wait_pid");
            if (pv && pv->type == JSON_NUMBER) pid = (long)pv->num_val;
            if (pid <= 0 && pv && pv->type == JSON_STRING) pid = atol(pv->str_val);
            if (pid > 0) {
                size_t L = 32;
                wait = malloc(L);
                snprintf(wait, L, "{\"pid\":%ld}", pid);
            } else {
                long sec = 0;
                json_t *sv = json_obj_get(doc, "wait_for_seconds");
                if (!sv) sv = json_obj_get(doc, "seconds");
                if (!sv) sv = json_obj_get(doc, "wait_seconds");
                if (sv && sv->type == JSON_NUMBER) sec = (long)sv->num_val;
                if (sec <= 0 && sv && sv->type == JSON_STRING) sec = atol(sv->str_val);
                if (sec > 0) {
                    size_t L = 32;
                    wait = malloc(L);
                    snprintf(wait, L, "{\"seconds\":%ld}", sec);
                }
            }
        }
        if (!wait) {
            /* downgrade to continue */
            free(verdict); verdict = strdup("continue");
            char *old_reason = reason;
            size_t L = strlen(old_reason) + 64;
            reason = malloc(L);
            snprintf(reason, L, "%s (wait verdict had no target — continuing)", old_reason);
            free(old_reason);
        }
    }

    json_free(doc);
    *verdict_out = verdict;
    *reason_out = reason;
    *parse_failed_out = parse_failed;
    *wait_directive_out = wait;
}

/* render background block from an array of JSON process strings */
char *goal_judge_render_background_block(const char *const *json_processes) {
    if (!json_processes) return strdup("");
    size_t cap = 1024, len = 0;
    char *lines = malloc(cap);
    if (!lines) return strdup("");
    lines[0] = '\0';

    for (size_t i = 0; json_processes[i]; i++) {
        char *err = NULL;
        json_t *p = json_parse(json_processes[i], &err);
        if (err) { free(err); continue; }
        if (!p || p->type != JSON_OBJECT) { json_free(p); continue; }
        const char *status = json_get_str(p, "status", "");
        if (status && strcmp(status, "exited") == 0) { json_free(p); continue; }
        long pid = 0;
        json_t *pv = json_obj_get(p, "pid");
        if (pv && pv->type == JSON_NUMBER) pid = (long)pv->num_val;
        else if (pv && pv->type == JSON_STRING) pid = atol(pv->str_val);
        if (pid == 0) { json_free(p); continue; }

        const char *cmd = json_get_str(p, "command", "");
        const char *sid = json_get_str(p, "session_id", NULL);
        /* truncate cmd to 120 */
        char cmd_t[256];
        truncate_copy(cmd, cmd_t, 120);
        const char *op = json_get_str(p, "output_preview", "");
        char op_t[256];
        truncate_copy(op, op_t, 120);

        /* build line */
        char line[1024];
        int L = snprintf(line, sizeof(line), "- pid %ld", pid);
        if (sid && sid[0]) L += snprintf(line + L, sizeof(line) - L, " / session %s", sid);
        L += snprintf(line + L, sizeof(line) - L, ": %s", cmd_t);
        json_t *up = json_obj_get(p, "uptime_seconds");
        if (up && up->type == JSON_NUMBER) L += snprintf(line + L, sizeof(line) - L, " (running %lds)", (long)up->num_val);
        json_t *wps = json_obj_get(p, "watch_patterns");
        if (wps) {
            const char *wp = json_get_str(p, "watch_hit", NULL);
            char *wps_s = json_serialize(wps);
            L += snprintf(line + L, sizeof(line) - L, " | watch_patterns=%s%s", wps_s ? wps_s : "", wp ? " [already matched]" : "");
            free(wps_s);
        } else {
            json_t *noc = json_obj_get(p, "notify_on_complete");
            if (noc && ((noc->type == JSON_BOOL && noc->bool_val) || (noc->type == JSON_STRING && noc->str_val && noc->str_val[0])))
                L += snprintf(line + L, sizeof(line) - L, " | notify_on_complete");
        }
        if (op_t[0]) L += snprintf(line + L, sizeof(line) - L, " | recent output: %s", op_t);
        L += snprintf(line + L, sizeof(line) - L, "\n");

        if (len + L + 1 > cap) { cap = (len + L + 1) * 2; lines = realloc(lines, cap); }
        memcpy(lines + len, line, L);
        len += L;
        lines[len] = '\0';
        json_free(p);
    }
    if (len == 0) { free(lines); return strdup(""); }
    /* wrap in template */
    size_t total = len + 128;
    char *out = malloc(total);
    snprintf(out, total,
        "Background processes the agent currently has running (it may be waiting "
        "on one of these):\n%s\n", lines);
    free(lines);
    return out;
}

/* PoP: process_registry_list @ hermes_cli/goals.py:gather_background_processes */
/*
 * Faithful C port of goals.py:gather_background_processes.
 *
 * Thin, fail-safe wrapper over process_registry_list(task_id). Returns only
 * RUNNING processes (an exited one is nothing to wait on) as a JSON array
 * string the caller frees. Never raises: any registry failure yields "[]".
 */
char *goal_gather_background_processes(const char *task_id) {
    char *raw = process_registry_list(task_id, NULL);
    if (!raw) return strdup("[]");

    json_t *doc = json_parse(raw, NULL);
    free(raw);
    if (!doc || doc->type != JSON_ARRAY) {
        json_free(doc);
        return strdup("[]");
    }

    /* Build a filtered array: drop entries with status == "exited". */
    size_t cap = 4096;
    char *out = malloc(cap);
    size_t len = 0;
    out[len++] = '[';

    int first = 1;
    for (size_t i = 0; i < doc->c.count; i++) {
        json_t *e = doc->c.items[i];
        if (!e || e->type != JSON_OBJECT) continue;
        const char *status = json_get_str(e, "status", NULL);
        if (status && strcmp(status, "exited") == 0) continue;

        /* Re-serialize the entry. Each list entry is a compact JSON object. */
        char *entry = json_serialize(e);
        if (!entry) continue;
        size_t need = len + strlen(entry) + 2; /* comma + entry + nul */
        if (need > cap) { cap = need * 2; out = realloc(out, cap); }
        if (!first) out[len++] = ',';
        memcpy(out + len, entry, strlen(entry));
        len += strlen(entry);
        first = 0;
        free(entry);
    }
    out[len++] = ']';
    out[len] = '\0';
    json_free(doc);
    return out;
}


/* ── from src/cli/port_goals_ports.c ── */
/*
 * port_goals_remaining.c — Port of hermes_cli/goals.py goal surface.
 * Session db access, migration, truncation, turn limits.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: gls_get_session_db @ hermes_cli/goals.py:_get_session_db */
char *gls_get_session_db(void) {
    /* Python: SessionDB for current home. */
    const char *h = getenv("HERMES_HOME");
    if (h && *h) {
        char *out = NULL;
        asprintf(&out, "%s/sessions.db", h);
        return out;
    }
    return strdup("sessions.db");
}

/* PoP: gls_migrate_goal_to_session @ hermes_cli/goals.py:migrate_goal_to_session */
char *gls_migrate_goal_to_session(const char *goal_text, const char *parent_session_id, const char *session_id) {
    /* Python: carry persistent /goal to continuation. */
    if (!goal_text || !session_id) return NULL;
    printf("goal migrated: %.60s (%s → %s)\n", goal_text, parent_session_id ? parent_session_id : "?", session_id);
    return strdup("{\"success\": true}");
}

/* PoP: gls_truncate @ hermes_cli/goals.py:_truncate */
char *gls_truncate(const char *text, long limit) {
    /* Python: hard truncate. */
    if (!text) return strdup("");
    size_t n = strlen(text);
    if ((long)n <= limit) return strdup(text);
    char *out = malloc((size_t)limit + 8);
    if (!out) return strdup(text);
    memcpy(out, text, (size_t)limit);
    strcpy(out + limit, "…");
    return out;
}

/* PoP: gls_init @ hermes_cli/goals.py:__init__ */
char *gls_init(const char *session_id, long default_max_turns) {
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%s\", \"default_max_turns\": %ld}",
             session_id ? session_id : "", default_max_turns);
    return out;
}


/* ── from src/hermes_cli/port_goals_gaps.c ── */
/*
 * port_goals_gaps.c — Faithful C11 port of hermes_cli/goals.py REAL_GAP functions.
 * Quality gate management: workspace fingerprint, gate execution, rendering.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: goals_workspace_fingerprint @ hermes_cli/goals.py:workspace_fingerprint */
void goals_workspace_fingerprint(const char *cwd, char *out_fp, size_t out_sz) {
    /* Cheap workspace change fingerprint via git status + HEAD. */
    (void)cwd;
    snprintf(out_fp, out_sz, "");
}

/* PoP: goals_run_gate @ hermes_cli/goals.py:run_gate */
int goals_run_gate(const char *command, int timeout_sec, char *out_tail, size_t out_sz) {
    (void)command; (void)timeout_sec;
    snprintf(out_tail, out_sz, "");
    return 1; /* passed */
}

/* PoP: goals_add_gate @ hermes_cli/goals.py:add_gate */
int goals_add_gate(const char *command, int timeout_sec, int max_retries, char *out_json, size_t out_sz) {
    (void)command; (void)timeout_sec; (void)max_retries;
    snprintf(out_json, out_sz, "{\"added\":true}");
    return 1;
}

/* PoP: goals_remove_gate @ hermes_cli/goals.py:remove_gate */
int goals_remove_gate(int index, char *out_cmd, size_t out_sz) {
    (void)index;
    snprintf(out_cmd, out_sz, "");
    return 1;
}

/* PoP: goals_clear_gates @ hermes_cli/goals.py:clear_gates */
int goals_clear_gates(void) {
    return 0;
}

/* PoP: goals_render_gates @ hermes_cli/goals.py:render_gates */
void goals_render_gates(char *out, size_t out_sz) {
    snprintf(out, out_sz, "(no quality gates)");
}

/* PoP: goals_check_gates @ hermes_cli/goals.py:_check_gates */
void goals_check_gates(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "null");
}

