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

static const char *CONTRACT_FIELDS[] = {
    "outcome", "verification", "constraints", "boundaries", "stop_when", NULL
};
static const char *CONTRACT_LABELS[] = {
    "Outcome", "Verification", "Constraints", "Boundaries", "Stop when blocked", NULL
};

/* local JSON string escaper: returns malloc'd escaped string. Caller frees. */
static char *json_escape(const char *s)
{
    if (!s) s = "";
    size_t need = 1;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') need += 2;
        else if (c == '\n') need += 2;
        else if (c == '\r') need += 2;
        else if (c == '\t') need += 2;
        else if (c < 0x20) need += 6;
        else need += 1;
    }
    char *out = malloc(need + 1);
    char *q = out;
    *q++ = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') { *q++='\\'; *q++='"'; }
        else if (c == '\\') { *q++='\\'; *q++='\\'; }
        else if (c == '\n') { *q++='\\'; *q++='n'; }
        else if (c == '\r') { *q++='\\'; *q++='r'; }
        else if (c == '\t') { *q++='\\'; *q++='t'; }
        else if (c < 0x20) { sprintf(q, "\\u%04x", c); q += 6; }
        else *q++ = (char)c;
    }
    *q++ = '"';
    *q = '\0';
    return out;
}

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
 * PoP: GoalContract.is_empty @ hermes_cli/goals.py:GoalContract.is_empty
 * Returns 1 if all contract fields are empty (contract given as JSON). */
int goals_is_empty(const char *contract_json)
{
    if (!contract_json || !contract_json[0]) return 1;
    json_t *c = json_parse(contract_json, NULL);
    if (!c || c->type != JSON_OBJECT) { if (c) json_free(c); return 1; }
    int empty = 1;
    for (int i = 0; CONTRACT_FIELDS[i]; i++) {
        json_t *v = json_object_get(c, CONTRACT_FIELDS[i]);
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
    for (int i = 0; CONTRACT_FIELDS[i]; i++) {
        json_t *v = json_object_get(c, CONTRACT_FIELDS[i]);
        if (v && v->type == JSON_STRING && json_string_value(v)[0]) {
            const char *val = json_string_value(v);
            size_t need = strlen(out) + strlen(CONTRACT_LABELS[i]) + strlen(val) + 8;
            char *n = realloc(out, need);
            if (!n) break;
            out = n;
            if (out[0]) strcat(out, "\n");
            char *esc = json_escape(val);
            strcat(out, "- ");
            strcat(out, CONTRACT_LABELS[i]);
            strcat(out, ": ");
            strcat(out, val);
            if (esc) free(esc);
        }
    }
    json_free(c);
    return out;
}

/*
 * PoP: parse_contract @ hermes_cli/goals.py:parse_contract
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
        json_escape(headline),
        json_escape(fields[0]), json_escape(fields[1]),
        json_escape(fields[2]), json_escape(fields[3]), json_escape(fields[4]));
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
        json_escape(goal), json_escape(status), tu, mt, ca, lta, wu, ws,
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
 * PoP: _extract_json_object @ hermes_cli/goals.py:_extract_json_object
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
 * PoP: _parse_judge_response @ hermes_cli/goals.py:_parse_judge_response
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
            json_escape(verdict), json_escape(reason));
        json_free(d); free(text);
        return r;
    }
    /* wait directive */
    char wait_json[128]; wait_json[0]='\0';
    json_t *sess = json_object_get(d, "wait_on_session");
    if (!sess) sess = json_object_get(d, "session_id");
    if (!sess) sess = json_object_get(d, "wait_session");
    if (sess && sess->type == JSON_STRING && json_string_value(sess)[0]) {
        snprintf(wait_json, sizeof(wait_json), "{\"session_id\":%s}", json_escape(json_string_value(sess)));
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
            json_escape(reason));
        json_free(d); free(text);
        return r;
    }
    char *r = malloc(strlen(verdict)+strlen(reason)+strlen(wait_json)+64);
    snprintf(r, strlen(verdict)+strlen(reason)+strlen(wait_json)+64,
        "{\"verdict\":%s,\"reason\":%s,\"parse_failed\":0,\"wait\":%s}",
        json_escape(verdict), json_escape(reason), wait_json);
    json_free(d); free(text);
    return r;
}

/*
 * PoP: _goal_judge_max_tokens @ hermes_cli/goals.py:_goal_judge_max_tokens
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
 * PoP: _render_background_block @ hermes_cli/goals.py:_render_background_block
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
