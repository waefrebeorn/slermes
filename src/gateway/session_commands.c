/*
 * session_commands.c — C home for upstream Hermes gateway/session_commands.py.
 *
 * PoP: w10_sessioncom_c9451d_first_word @ gateway/session_commands.py:_first_word
 * PoP: w10_sessioncom_c9451d_first_token @ gateway/session_commands.py:_first_token
 * PoP: w10_sessioncom_c9451d_command_verdict @ gateway/session_commands.py:command_verdict
 * PoP: w10_sessioncom_c9451d_refusal_hint @ gateway/session_commands.py:REFUSAL_HINTS
 * PoP: w10_sessioncom_c9451d_text @ gateway/session_commands.py:_text
 *
 * Pure command-classification helpers.  _first_token uses POSIX shlex splitting
 * (quotes concatenate, backslash escapes) exactly as shlex.split does; a parse
 * error is reported as {"__raise__":"AuthorityRefusal","msg":"invalid_params"}.
 */
#include "noc_w10_sessioncom_c9451d.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *xstrdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static void lower_ascii(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }
static int in_set(const char *s, const char *const *set, size_t n) {
    if (!s) return 0;
    for (size_t i = 0; i < n; i++) if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}
static int is_blank(const char *s) {
    if (!s) return 1;
    for (; *s; s++) if (!isspace((unsigned char)*s)) return 0;
    return 1;
}

/* POSIX-ish shlex.split. Returns NULL on an unclosed quote (error flag set). */
static json_t *shlex_split(const char *s, int *err) {
    json_t *arr = json_array();
    if (!s) s = "";
    const char *p = s;
    *err = 0;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char *tok = (char *)malloc(strlen(p) + 1);
        size_t ti = 0;
        int done = 0;
        while (*p && !done) {
            if (isspace((unsigned char)*p)) { done = 1; break; }
            if (*p == '\'') {
                p++;
                while (*p && *p != '\'') tok[ti++] = *p++;
                if (!*p) { *err = 1; break; }
                p++;
            } else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && p[1] && strchr("\\\"$`", p[1])) { p++; tok[ti++] = *p++; }
                    else tok[ti++] = *p++;
                }
                if (!*p) { *err = 1; break; }
                p++;
            } else if (*p == '\\') {
                p++;
                if (*p) tok[ti++] = *p++;
            } else tok[ti++] = *p++;
        }
        tok[ti] = '\0';
        if (*err) { free(tok); json_free(arr); return NULL; }
        json_append(arr, json_string(tok));
        free(tok);
    }
    return arr;
}

static json_t *raise_invalid(void) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string("AuthorityRefusal"));
    json_set(o, "msg", json_string("invalid_params"));
    return o;
}

/* arg.split(None,1)[0].lower() else '' */
struct json_t *w10_sessioncom_c9451d_first_word(const char *arg) {
    if (!arg) arg = "";
    const char *p = arg;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *b = p;
    while (*p && !isspace((unsigned char)*p)) p++;
    if (p == b) return json_string("");
    char *w = (char *)malloc((size_t)(p - b) + 1);
    if (!w) return json_string("");
    memcpy(w, b, (size_t)(p - b));
    w[p - b] = '\0';
    lower_ascii(w);
    json_t *out = json_string(w);
    free(w);
    return out;
}

struct json_t *w10_sessioncom_c9451d_first_token(const char *arg) {
    int err = 0;
    json_t *toks = shlex_split(arg, &err);
    if (err) return raise_invalid();
    const char *res = "";
    for (size_t i = 0; i < json_len(toks); i++) {
        const json_t *t = json_get(toks, i);
        const char *v = (t && t->type == JSON_STRING) ? t->str_val : "";
        if (strcmp(v, "--board") == 0) { i++; continue; }
        if (strncmp(v, "--board=", 8) != 0) { res = v; break; }
    }
    char *cp = xstrdup(res);
    lower_ascii(cp);
    json_free(toks);
    json_t *out = json_string(cp);
    free(cp);
    return out;
}

static const char *const CONTROL_COMMANDS[] = {"title"};
static const char *const HANDLER_CONTROLS[] = {"goal","subgoal","loop","heartbeat","review","refine","debug","curator","login","autoprune"};
static const char *const PROMPT_COMMANDS[] = {"plan","learn","init","moa","blueprint","egress"};
static const char *const SETTING_COMMANDS[] = {"reasoning","voice","approvals","codex-runtime","footer","busy","verbose","yolo","fast","personality"};
static const char *const APPROVAL_COMMANDS[] = {"approve","deny"};
static const char *const INFO_READS[] = {"toolsets","platforms"};
static const char *const READ_COMMANDS[] = {"help","commands","status","context","version","whoami","usage","insights","profile","diff","memory","kanban","topup","suggestions","bundles","skills"};
static const char *const KANBAN_READS[] = {"","help","--help","-h","?","list","ls","show","stats","assignees","context","runs","log","diagnostics","diag"};
static const char *const CONTROL_ARGS[] = {"approve","apply","reject","deny","drop","approval","mode"};

static json_t *first_word_j(const char *arg) { return w10_sessioncom_c9451d_first_word(arg); }
static const char *fw_str(const char *arg, char *buf, size_t n) {
    json_t *w = first_word_j(arg);
    const char *v = (w && w->type == JSON_STRING) ? w->str_val : "";
    snprintf(buf, n, "%s", v);
    json_free(w);
    return buf;
}

struct json_t *w10_sessioncom_c9451d_command_verdict(const char *name, const char *arg) {
    if (!name) name = "";
    if (!arg) arg = "";
    const char *verdict = "refused";
    /* _REFUSED_ARGUMENTS: voice tts */
    int is_tts = 0;
    if (strcmp(name, "voice") == 0) {
        char *t = xstrdup(arg); lower_ascii(t);
        char *st = t; while (*st && isspace((unsigned char)*st)) st++;
        size_t l = strlen(st); while (l && isspace((unsigned char)st[l-1])) st[--l] = '\0';
        is_tts = (strcmp(st, "tts") == 0); free(t);
    }
    if (is_tts) return json_string("refused");

    if (strcmp(name, "cron") == 0) {
        int err = 0; json_t *toks = shlex_split(arg, &err);
        int is_read = 1;
        if (err) is_read = 0;
        else {
            if (json_len(toks) == 0) is_read = 1; /* cron_read([]) -> 'overview' */
            else {
                const json_t *t0 = json_get(toks, 0);
                char first[64] = "";
                if (t0 && t0->type == JSON_STRING) snprintf(first, sizeof(first), "%s", t0->str_val);
                lower_ascii(first);
                int ok = (strcmp(first, "list") == 0);
                for (size_t i = 1; ok && i < json_len(toks); i++) {
                    const json_t *ti = json_get(toks, i);
                    const char *v = (ti && ti->type == JSON_STRING) ? ti->str_val : "";
                    if (!(strcmp(v, "--all") == 0 || strcmp(v, "-a") == 0)) ok = 0;
                }
                is_read = ok; /* cron_read -> 'list'/'list-all' (non-None) */
            }
        }
        json_free(toks);
        return json_string(is_read ? "read" : "control");
    }

    char fwbuf[1024];
    const char *fw = fw_str(arg, fwbuf, sizeof(fwbuf));
    int control = 0;
    if (in_set(name, CONTROL_COMMANDS, sizeof(CONTROL_COMMANDS)/sizeof(*CONTROL_COMMANDS))) control = 1;
    if (in_set(name, HANDLER_CONTROLS, sizeof(HANDLER_CONTROLS)/sizeof(*HANDLER_CONTROLS))) control = 1;
    if (in_set(name, PROMPT_COMMANDS, sizeof(PROMPT_COMMANDS)/sizeof(*PROMPT_COMMANDS))) control = 1;
    if (in_set(name, SETTING_COMMANDS, sizeof(SETTING_COMMANDS)/sizeof(*SETTING_COMMANDS))) control = 1;
    if (in_set(name, APPROVAL_COMMANDS, sizeof(APPROVAL_COMMANDS)/sizeof(*APPROVAL_COMMANDS))) control = 1;
    if ((strcmp(name, "skills") == 0 || strcmp(name, "memory") == 0)
        && in_set(fw, CONTROL_ARGS, sizeof(CONTROL_ARGS)/sizeof(*CONTROL_ARGS))) control = 1;
    if (control) return json_string("control");

    int read = 0;
    if (in_set(name, INFO_READS, sizeof(INFO_READS)/sizeof(*INFO_READS))) read = 1;
    if (in_set(name, READ_COMMANDS, sizeof(READ_COMMANDS)/sizeof(*READ_COMMANDS))) {
        if (strcmp(name, "skills") == 0) read = (strcmp(fw,"")==0 || strcmp(fw,"pending")==0 || strcmp(fw,"diff")==0);
        else if (strcmp(name, "usage") == 0) read = is_blank(arg);
        else if (strcmp(name, "memory") == 0) read = (strcmp(fw,"")==0 || strcmp(fw,"pending")==0);
        else if (strcmp(name, "suggestions") == 0) read = is_blank(arg);
        else if (strcmp(name, "kanban") == 0) {
            json_t *ft = w10_sessioncom_c9451d_first_token(arg);
            const char *v = (ft && ft->type == JSON_STRING) ? ft->str_val : "";
            read = in_set(v, KANBAN_READS, sizeof(KANBAN_READS)/sizeof(*KANBAN_READS));
            json_free(ft);
        } else read = 1; /* _READ_ARGUMENTS.get(name, lambda _: True) */
    }
    verdict = read ? "read" : "refused";
    return json_string(verdict);
}

struct json_t *w10_sessioncom_c9451d_refusal_hint(const char *name) {
    if (!name) return json_null();
    if (!strcmp(name,"export")) return json_string("Run `hermes profile export [name] -o <file>` in a terminal.");
    if (!strcmp(name,"import")) return json_string("Run `hermes profile import <archive> [--name <name>]` in a terminal.");
    if (!strcmp(name,"snapshot")) return json_string("Run `hermes backup --quick` in a terminal (restore: `hermes --cli`, then /snapshot restore).");
    if (!strcmp(name,"handoff")) return json_string("Hand off from the classic CLI: `hermes --cli --resume <session>`, then /handoff <platform>.");
    if (!strcmp(name,"hatch")) return json_string("Generate a pet from the classic CLI (`hermes --cli`, then /hatch), or use /pet to pick one.");
    if (!strcmp(name,"worktree")) return json_string("Start in a fresh worktree with `hermes -w`, or use the classic CLI (`hermes --cli`).");
    if (!strcmp(name,"palette")) return json_string("Type / to search commands (the composer completes and filters them).");
    return json_null();
}

struct json_t *w10_sessioncom_c9451d_text(const struct json_t *output) {
    const json_t *v = output;
    if (output && output->type == JSON_OBJECT) {
        const json_t *t = json_obj_get(output, "text");
        if (t) v = t; else v = NULL;
    }
    if (!v) return json_string("");
    if (v->type == JSON_NULL) return json_string("");
    if (v->type == JSON_STRING) return json_string(v->str_val);
    if (v->type == JSON_BOOL) return json_string(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) { char b[64]; snprintf(b,sizeof(b),"%.17g",v->num_val); return json_string(b); }
    /* list/dict: str() of the JSON value — serialize compactly */
    char *s = json_dumps(v, 0);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}
