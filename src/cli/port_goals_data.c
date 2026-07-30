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
/* PoP: goal_contract_from_dict @ hermes_cli/goals.py:from_dict */
/* PoP: goal_contract_render    @ hermes_cli/goals.py:render_block */
/* PoP: parse_contract          @ hermes_cli/goals.py:parse_contract */
/* PoP: goal_state_to_json      @ hermes_cli/goals.py:to_json */
/* PoP: goal_state_from_json    @ hermes_cli/goals.py:from_json */
/* PoP: goal_state_has_contract @ hermes_cli/goals.py:has_contract */
/* PoP: goal_state_render_subgoals @ hermes_cli/goals.py:render_subgoals_block */

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
