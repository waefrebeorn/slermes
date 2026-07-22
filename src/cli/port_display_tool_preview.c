/*
 * port_display_tool_preview.c — C port of agent/display.py tool preview/label
 * builders (build_tool_preview, build_tool_label, redact_tool_args_for_display)
 * plus their pure helpers (_truncate_preview, _read_file_line_label).
 *
 * Faithful port of the pure-transform display functions. Terminal/code
 * previews reuse cli_agent_display__summarize_shell_command; whitespace
 * collapsing uses cli_agent_display__oneline; browser_type text redaction
 * uses the browser_redact port of redact_sensitive_text.
 */

#include "hermes_json.h"
#include "cli/port_agent_display.h"
#include "tools/browser_redact.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* ---- _truncate_preview ---- */

/* PoP: cli_agent_display__truncate_preview @ agent/display.py:_truncate_preview */
char *cli_agent_display__truncate_preview(const char *text, int max_len)
{
    if (!text) return strdup("");
    size_t len = strlen(text);
    if (max_len > 0 && len > (size_t)max_len) {
        if (max_len <= 3) {
            char *r = malloc((size_t)max_len + 1);
            for (int i = 0; i < max_len; i++) r[i] = '.';
            r[max_len] = '\0';
            return r;
        }
        size_t cut = (size_t)max_len - 3;
        char *r = malloc(cut + 4);
        memcpy(r, text, cut);
        r[cut] = '\0';
        strcat(r, "...");
        return r;
    }
    return strdup(text);
}

/* ---- _read_file_line_label ---- */

/* PoP: cli_agent_display__read_file_line_label @ agent/display.py:_read_file_line_label */
char *cli_agent_display__read_file_line_label(const json_t *args)
{
    if (!args || args->type != JSON_OBJECT) return strdup("");
    json_t *off = json_obj_get(args, "offset");
    json_t *lim = json_obj_get(args, "limit");
    int offset = (off && off->type == JSON_NUMBER) ? json_node_get_int(off) : 0;
    int limit = (lim && lim->type == JSON_NUMBER) ? json_node_get_int(lim) : 0;
    if (offset <= 0) return strdup("");
    if (limit <= 1) {
        char buf[32]; snprintf(buf, sizeof(buf), "L%d", offset); return strdup(buf);
    }
    char buf[64]; snprintf(buf, sizeof(buf), "L%d-%d", offset, offset + limit - 1); return strdup(buf);
}

/* ---- redact_tool_args_for_display ---- */

/* PoP: cli_agent_display__redact_tool_args_for_display @ agent/display.py:redact_tool_args_for_display */
/* Returns a freshly-allocated JSON string with browser_type "text" redacted.
 * For all other tools returns an unmodified copy of args_json. Caller frees. */
char *cli_agent_display__redact_tool_args_for_display(const char *tool_name, const char *args_json)
{
    if (!args_json) return strdup("{}");
    if (strcmp(tool_name ? tool_name : "", "browser_type") != 0)
        return strdup(args_json);
    char *err = NULL;
    json_t *args = json_parse(args_json, &err);
    if (!args) { if (err) free(err); return strdup(args_json); }
    json_t *text = json_obj_get(args, "text");
    if (text && text->type == JSON_STRING && text->str_val) {
        char *red = browser_redact_sensitive_text(text->str_val);
        json_set(args, "text", json_string(red ? red : ""));
        free(red);
    }
    char *out = json_serialize(args);
    json_free(args);
    return out ? out : strdup(args_json);
}

/* ---- build_tool_preview ---- */

/* A tiny growable string builder */
typedef struct { char *buf; size_t cap; size_t len; } sb_t;
static void sb_init(sb_t *s) { s->cap = 64; s->buf = malloc(s->cap); s->buf[0] = '\0'; s->len = 0; }
static void sb_append_str(sb_t *s, const char *v)
{
    if (!v) return;
    size_t add = strlen(v);
    if (s->len + add + 1 > s->cap) { while (s->len + add + 1 > s->cap) s->cap *= 2; s->buf = realloc(s->buf, s->cap); }
    memcpy(s->buf + s->len, v, add + 1); s->len += add;
}
static void sb_appendf(sb_t *s, const char *fmt, ...)
{
    char tmp[1024]; va_list ap; va_start(ap, fmt); vsnprintf(tmp, sizeof(tmp), fmt, ap); va_end(ap);
    sb_append_str(s, tmp);
}

/* primary arg key per tool (from Python primary_args) */
static const char *primary_arg_key(const char *tool)
{
    if (!tool) return NULL;
    if (strcmp(tool,"terminal")==0) return "command";
    if (strcmp(tool,"web_search")==0) return "query";
    if (strcmp(tool,"web_extract")==0) return "urls";
    if (strcmp(tool,"read_file")==0) return "path";
    if (strcmp(tool,"write_file")==0) return "path";
    if (strcmp(tool,"patch")==0) return "path";
    if (strcmp(tool,"search_files")==0) return "pattern";
    if (strcmp(tool,"browser_navigate")==0) return "url";
    if (strcmp(tool,"browser_click")==0) return "ref";
    if (strcmp(tool,"browser_type")==0) return "text";
    if (strcmp(tool,"image_generate")==0) return "prompt";
    if (strcmp(tool,"text_to_speech")==0) return "text";
    if (strcmp(tool,"vision_analyze")==0) return "question";
    if (strcmp(tool,"skill_view")==0) return "name";
    if (strcmp(tool,"skills_list")==0) return "category";
    if (strcmp(tool,"cronjob")==0) return "action";
    if (strcmp(tool,"execute_code")==0) return "code";
    if (strcmp(tool,"delegate_task")==0) return "goal";
    if (strcmp(tool,"clarify")==0) return "question";
    if (strcmp(tool,"skill_manage")==0) return "name";
    return NULL;
}

/* PoP: cli_agent_display__build_tool_preview @ agent/display.py:build_tool_preview */
/* tool_name + args_json (object) + max_len (0 = unlimited). Returns a malloc'd
 * preview string (caller frees) or strdup("") on empty/None. */
char *cli_agent_display__build_tool_preview(const char *tool_name, const char *args_json, int max_len)
{
    if (!tool_name) tool_name = "";
    if (!args_json || !args_json[0]) return strdup("");

    char *red = cli_agent_display__redact_tool_args_for_display(tool_name, args_json);
    char *err = NULL;
    json_t *args = json_parse(red, &err);
    free(red);
    if (!args) { if (err) free(err); return strdup(""); }
    if (args->type != JSON_OBJECT) { json_free(args); return strdup(""); }

    sb_t sb; sb_init(&sb);
    char *result = NULL;

    if (strcmp(tool_name, "delegate_task") == 0) {
        json_t *tasks = json_obj_get(args, "tasks");
        if (tasks && tasks->type == JSON_ARRAY) {
            int task_count = 0;
            sb_t goals; sb_init(&goals);
            size_t n = json_len(tasks);
            for (size_t i = 0; i < n; i++) {
                json_t *t = json_get(tasks, i);
                if (!t || t->type != JSON_OBJECT) continue;
                json_t *raw = json_obj_get(t, "goal");
                char *g = cli_agent_display__oneline(raw && raw->type == JSON_STRING ? raw->str_val : "?");
                char *gt = cli_agent_display__truncate_preview(g ? g : "?", 40);
                if (goals.len) sb_append_str(&goals, " | ");
                sb_append_str(&goals, gt);
                free(gt); free(g);
                task_count++;
            }
            if (goals.len)
                sb_appendf(&sb, "%d tasks: %s", task_count, goals.buf);
            else
                sb_appendf(&sb, "%zu parallel tasks", n);
            free(goals.buf);
        } else {
            json_t *goal = json_obj_get(args, "goal");
            if (!goal || (goal->type == JSON_NULL)) { json_free(args); free(sb.buf); return strdup(""); }
            char *gp = cli_agent_display__oneline(goal->type == JSON_STRING ? goal->str_val : "");
            if (gp && gp[0]) {
                char *gt = cli_agent_display__truncate_preview(gp, max_len);
                sb_append_str(&sb, gt); free(gt);
            }
            free(gp);
        }
    } else if (strcmp(tool_name, "process") == 0) {
        json_t *action = json_obj_get(args, "action");
        json_t *sid = json_obj_get(args, "session_id");
        json_t *data = json_obj_get(args, "data");
        json_t *timeout_val = json_obj_get(args, "timeout");
        sb_append_str(&sb, action && action->type == JSON_STRING ? action->str_val : "");
        if (sid && sid->type == JSON_STRING && sid->str_val[0]) {
            size_t sl = strlen(sid->str_val);
            if (sl > 16) { char tmp[17]; memcpy(tmp, sid->str_val, 16); tmp[16]='\0'; sb_appendf(&sb, " %s", tmp); }
            else sb_appendf(&sb, " %s", sid->str_val);
        }
        if (data && data->type == JSON_STRING && data->str_val[0]) {
            char *on = cli_agent_display__oneline(data->str_val);
            size_t dl = strlen(on);
            char *cut = dl > 20 ? strndup(on, 20) : strdup(on);
            sb_appendf(&sb, " \"%s\"", cut); free(cut); free(on);
        }
        if (timeout_val && action && action->type == JSON_STRING && strcmp(action->str_val, "wait") == 0) {
            if (timeout_val->type == JSON_NUMBER) {
                sb_append_str(&sb, " ");
                sb_appendf(&sb, "%d", json_node_get_int(timeout_val));
            }
            sb_append_str(&sb, "s");
        }
    } else if (strcmp(tool_name, "todo") == 0) {
        json_t *todos = json_obj_get(args, "todos");
        json_t *merge = json_obj_get(args, "merge");
        int merge_on = merge && (merge->type == JSON_BOOL ? merge->bool_val : (merge->type == JSON_NUMBER && merge->num_val != 0.0));
        if (todos == NULL || (todos->type == JSON_NULL)) sb_append_str(&sb, "reading task list");
        else if (merge_on) sb_appendf(&sb, "updating %zu task(s)", json_len(todos));
        else sb_appendf(&sb, "planning %zu task(s)", json_len(todos));
    } else if (strcmp(tool_name, "terminal") == 0 || strcmp(tool_name, "execute_code") == 0) {
        const char *key = strcmp(tool_name, "execute_code") == 0 ? "code" : "command";
        json_t *cmd = json_obj_get(args, key);
        if (!cmd || (cmd->type != JSON_STRING)) { json_free(args); free(sb.buf); return strdup(""); }
        char *sum = cli_agent_display__summarize_shell_command(cmd->str_val);
        if (sum && sum[0]) {
            char *t = cli_agent_display__truncate_preview(sum, max_len);
            sb_append_str(&sb, t); free(t);
        }
        free(sum);
    } else if (strcmp(tool_name, "read_file") == 0) {
        json_t *p = json_obj_get(args, "path");
        if (!p || p->type != JSON_STRING) p = json_obj_get(args, "file");
        if (!p || p->type != JSON_STRING) p = json_obj_get(args, "filepath");
        if (!p || p->type != JSON_STRING) { json_free(args); free(sb.buf); return strdup(""); }
        /* label = basename after replacing backslashes with '/' */
        char *norm = strdup(p->str_val);
        for (char *c = norm; *c; c++) if (*c == '\\') *c = '/';
        char *label = strrchr(norm, '/'); label = label ? label + 1 : norm;
        char *line_lbl = cli_agent_display__read_file_line_label(args);
        sb_append_str(&sb, label);
        if (line_lbl && line_lbl[0]) { sb_append_str(&sb, " "); sb_append_str(&sb, line_lbl); }
        free(line_lbl); free(norm);
    } else if (strcmp(tool_name, "session_search") == 0) {
        json_t *q = json_obj_get(args, "query");
        char *qq = cli_agent_display__oneline(q && q->type == JSON_STRING ? q->str_val : "");
        size_t ql = strlen(qq);
        char *cut = ql > 25 ? strndup(qq, 25) : strdup(qq);
        sb_appendf(&sb, "recall: \"%s%s\"", cut, ql > 25 ? "..." : "");
        free(cut); free(qq);
    } else if (strcmp(tool_name, "memory") == 0) {
        json_t *action = json_obj_get(args, "action");
        json_t *target = json_obj_get(args, "target");
        const char *act = action && action->type == JSON_STRING ? action->str_val : "";
        const char *tgt = target && target->type == JSON_STRING ? target->str_val : "";
        if (strcmp(act, "add") == 0) {
            json_t *c = json_obj_get(args, "content");
            char *cc = cli_agent_display__oneline(c && c->type == JSON_STRING ? c->str_val : "");
            size_t cl = strlen(cc); char *cut = cl > 25 ? strndup(cc, 25) : strdup(cc);
            sb_appendf(&sb, "+%s: \"%s%s\"", tgt, cut, cl > 25 ? "..." : "");
            free(cut); free(cc);
        } else if (strcmp(act, "replace") == 0 || strcmp(act, "remove") == 0) {
            json_t *o = json_obj_get(args, "old_text");
            char *oo = cli_agent_display__oneline(o && o->type == JSON_STRING ? o->str_val : "");
            if (!oo || !oo[0]) { free(oo); oo = strdup("<missing old_text>"); }
            size_t ol = strlen(oo); char *cut = ol > 20 ? strndup(oo, 20) : strdup(oo);
            sb_appendf(&sb, "%s%s: \"%s\"", act[0] == 'r' ? "-" : "~", tgt, cut);
            free(cut); free(oo);
        } else sb_append_str(&sb, act);
    } else if (strcmp(tool_name, "send_message") == 0) {
        json_t *target = json_obj_get(args, "target");
        json_t *msg = json_obj_get(args, "message");
        const char *tgt = target && target->type == JSON_STRING ? target->str_val : "?";
        char *mm = cli_agent_display__oneline(msg && msg->type == JSON_STRING ? msg->str_val : "");
        size_t ml = strlen(mm);
        if (ml > 20) { char *cut = strndup(mm, 17); sb_appendf(&sb, "to %s: \"%s...\"", tgt, cut); free(cut); }
        else sb_appendf(&sb, "to %s: \"%s\"", tgt, mm);
        free(mm);
    } else {
        const char *key = primary_arg_key(tool_name);
        if (!key) {
            const char *fb[] = {"query","text","command","path","name","prompt","code","goal",NULL};
            for (int i = 0; fb[i]; i++) {
                if (json_obj_get(args, fb[i])) { key = fb[i]; break; }
            }
        }
        if (!key || !json_obj_get(args, key)) { json_free(args); free(sb.buf); return strdup(""); }
        json_t *value = json_obj_get(args, key);
        char *vs;
        if (value->type == JSON_ARRAY) {
            json_t *first = json_get(value, 0);
            vs = cli_agent_display__oneline(first && first->type == JSON_STRING ? first->str_val : (first ? "" : ""));
        } else if (value->type == JSON_STRING) {
            vs = cli_agent_display__oneline(value->str_val);
        } else {
            char *sv = json_serialize(value);
            vs = cli_agent_display__oneline(sv ? sv : "");
            free(sv);
        }
        if (!vs || !vs[0]) { free(vs); json_free(args); free(sb.buf); return strdup(""); }
        char *t = cli_agent_display__truncate_preview(vs, max_len);
        sb_append_str(&sb, t);
        free(t); free(vs);
    }

    json_free(args);
    result = (sb.len > 0) ? strdup(sb.buf) : strdup("");
    free(sb.buf);
    return result;
}

/* ---- redact_browser_typed_text_for_display ---- */

/* Recursively replace every occurrence of `needle` with `redacted` in a JSON
 * value (strings replaced in-place substring-wise; dicts/arrays recursed).
 * Mirrors Python's recursive walk over str/dict/list (tuples become arrays in
 * JSON so are covered by the array branch). Mutates `node` in place. */
static char *str_replace_all(const char *hay, const char *needle, const char *repl)
{
    size_t nlen = strlen(needle);
    if (nlen == 0) return strdup(hay);
    size_t rlen = strlen(repl);
    /* count occurrences */
    size_t count = 0;
    for (const char *p = hay; (p = strstr(p, needle)); p += nlen) count++;
    if (count == 0) return strdup(hay);
    size_t outlen = strlen(hay) + count * (rlen > nlen ? rlen - nlen : 0) + 1;
    char *out = malloc(outlen);
    char *w = out;
    const char *p = hay;
    while (1) {
        const char *hit = strstr(p, needle);
        if (!hit) { strcpy(w, p); break; }
        size_t pre = (size_t)(hit - p);
        memcpy(w, p, pre); w += pre;
        memcpy(w, repl, rlen); w += rlen;
        p = hit + nlen;
    }
    return out;
}

static void redact_typed_walk(json_t *node, const char *needle, const char *redacted)
{
    if (!node) return;
    if (node->type == JSON_STRING && node->str_val) {
        char *rep = str_replace_all(node->str_val, needle, redacted);
        free(node->str_val);
        node->str_val = rep;
    } else if (node->type == JSON_OBJECT) {
        size_t n = json_len(node);
        for (size_t i = 0; i < n; i++)
            redact_typed_walk(json_object_get_at(node, i), needle, redacted);
    } else if (node->type == JSON_ARRAY) {
        size_t n = json_len(node);
        for (size_t i = 0; i < n; i++)
            redact_typed_walk(json_get(node, i), needle, redacted);
    }
}

/* PoP: cli_agent_display__redact_browser_typed_text_for_display @ agent/display.py:redact_browser_typed_text_for_display */
/* value_json is a serialized JSON value; typed_text is the raw typed string.
 * Returns a freshly-allocated serialized JSON value with every occurrence of the
 * raw typed secret replaced by its redacted form. When typed_text is NULL/empty
 * or contains nothing secret-looking, returns an unmodified copy. Caller frees. */
char *cli_agent_display__redact_browser_typed_text_for_display(const char *value_json, const char *typed_text)
{
    if (!value_json) return strdup("null");
    if (!typed_text || !typed_text[0]) return strdup(value_json);
    char *redacted = browser_redact_sensitive_text(typed_text);
    if (!redacted || strcmp(redacted, typed_text) == 0) {
        /* Nothing secret-looking; leave payload untouched. */
        free(redacted);
        return strdup(value_json);
    }
    char *err = NULL;
    json_t *node = json_parse(value_json, &err);
    if (!node) { if (err) free(err); free(redacted); return strdup(value_json); }
    redact_typed_walk(node, typed_text, redacted);
    char *out = json_serialize(node);
    json_free(node);
    free(redacted);
    return out ? out : strdup(value_json);
}

/* ---- friendly tool labels global state + accessors ---- */

static int s_friendly_tool_labels = 1;

/* PoP: cli_agent_display__set_friendly_tool_labels @ agent/display.py:set_friendly_tool_labels */
void cli_agent_display__set_friendly_tool_labels(int enabled)
{
    s_friendly_tool_labels = enabled ? 1 : 0;
}

/* PoP: cli_agent_display__get_friendly_tool_labels @ agent/display.py:get_friendly_tool_labels */
int cli_agent_display__get_friendly_tool_labels(void)
{
    return s_friendly_tool_labels;
}

/* ---- build_tool_label ---- */

/* PoP: cli_agent_display__build_tool_label @ agent/display.py:build_tool_label */

static const char *TOOL_VERBS(const char *tool)
{
    if (!tool) return NULL;
    static const struct { const char *k; const char *v; } m[] = {
        {"web_search","Searching the web"},{"web_extract","Reading"},
        {"browser_navigate","Browsing"},{"browser_click","Clicking"},
        {"browser_type","Typing"},{"read_file","Reading"},
        {"write_file","Writing"},{"patch","Editing"},
        {"search_files","Searching files"},{"terminal","Running"},
        {"execute_code","Running code"},{"image_generate","Generating image"},
        {"video_generate","Generating video"},{"text_to_speech","Generating speech"},
        {"vision_analyze","Looking at the image"},{"session_search","Searching past sessions"},
        {"skill_view","Reading skill"},{"skills_list","Listing skills"},
        {"skill_manage","Updating skill"},{"memory","Updating memory"},
        {"todo","Updating tasks"},{"delegate_task","Delegating"},
        {"cronjob","Scheduling"},{"clarify","Asking"},{NULL,NULL}
    };
    for (int i = 0; m[i].k; i++) if (strcmp(m[i].k, tool) == 0) return m[i].v;
    return NULL;
}
/* PoP: cli_agent_display__get_tool_verb @ agent/display.py:get_tool_verb */
/* Returns the friendly verb for a built-in tool, or NULL when friendly labels
 * are disabled or the tool has no curated verb. Returned pointer is static. */
const char *cli_agent_display__get_tool_verb(const char *tool_name)
{
    if (!s_friendly_tool_labels) return NULL;
    return TOOL_VERBS(tool_name);
}

/* PoP: cli_agent_display__verb_drops_preview @ agent/display.py:verb_drops_preview */
int cli_agent_display__verb_drops_preview(const char *tool_name) {
    if (!tool_name) return 0;
    return strcmp(tool_name,"skills_list")==0 || strcmp(tool_name,"session_search")==0;
}
static int verb_no_preview(const char *tool) {
    return cli_agent_display__verb_drops_preview(tool);
}

/* PoP: cli_agent_display__tool_verb_connector @ agent/display.py:tool_verb_connector */
/* Returns the connector between a verb and its preview (" for " or " "). */
const char *cli_agent_display__tool_verb_connector(const char *tool_name) {
    if (tool_name && (strcmp(tool_name,"web_search")==0 || strcmp(tool_name,"search_files")==0))
        return " for ";
    return " ";
}
static int verb_for_connector(const char *tool) {
    return tool && strcmp(cli_agent_display__tool_verb_connector(tool), " for ") == 0;
}

char *cli_agent_display__build_tool_label(const char *tool_name, const char *args_json, int max_len, int friendly)
{
    if (!friendly)
        return cli_agent_display__build_tool_preview(tool_name, args_json, max_len);
    const char *verb = TOOL_VERBS(tool_name);
    if (!verb)
        return cli_agent_display__build_tool_preview(tool_name, args_json, max_len);
    if (verb_no_preview(tool_name))
        return strdup(verb);
    char *preview = cli_agent_display__build_tool_preview(tool_name, args_json, max_len);
    if (!preview || !preview[0]) { free(preview); return strdup(verb); }
    if (verb_for_connector(tool_name)) {
        char *r = malloc(strlen(verb) + strlen(preview) + 6);
        sprintf(r, "%s for %s", verb, preview);
        free(preview);
        return r;
    }
    char *r = malloc(strlen(verb) + strlen(preview) + 2);
    sprintf(r, "%s %s", verb, preview);
    free(preview);
    return r;
}
