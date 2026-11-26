/* tool_result_summary.c — one-line summaries of tool calls and results.
 *
 * Port of agent/context_compressor.py:_summarize_tool_result. Its own translation unit so a differential
 * harness (tests/unit/tool_summary_diff.c) can link it against libjson alone and compare the text with
 * the Python function instead of asserting the format from memory:
 *
 *     python tests/oracle/tool_summary_oracle.py
 *
 * Every branch below was written against a disagreement the oracle reported. The branches the oracle
 * does not grade yet (clarify, cronjob_manage, process_manage) are marked BEST-EFFORT: they keep the
 * shape this port had, not a verified match.
 */
#include "hermes_json.h"
#include "context_compressor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUM_FAILED_LABEL "FAILED"
#define SUM_MAX_SUMMARY 4096

/* ── small helpers ─────────────────────────────────────────────────────────────────────────── */

static const char *jstr(const json_t *obj, const char *key, const char *def) {
    if (!obj || obj->type != JSON_OBJECT) return def;
    const json_t *v = json_obj_get(obj, key);
    if (!v) return def;
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : def;
    return def;                                  /* _str_arg: non-strings are not used verbatim */
}

static double jnum(const json_t *obj, const char *key, double def) {
    if (!obj || obj->type != JSON_OBJECT) return def;
    const json_t *v = json_obj_get(obj, key);
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING && v->str_val) return atof(v->str_val);
    return def;
}

static bool jhas(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

/* Python's {:,} — 1234567 -> "1,234,567". */
static void fmt_count(char *out, size_t cap, size_t n) {
    char raw[32];
    snprintf(raw, sizeof(raw), "%zu", n);
    size_t len = strlen(raw), o = 0;
    for (size_t i = 0; i < len && o + 1 < cap; i++) {
        if (i && (len - i) % 3 == 0) out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = '\0';
}

/* content.count("\n") + 1 if content.strip() else 0 */
static int line_count_of(const char *content) {
    if (!content) return 0;
    const char *p = content;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (!*p) return 0;
    int n = 1;
    for (const char *q = content; *q; q++) if (*q == '\n') n++;
    return n;
}

/* " FAILED: <one-line reason, 80 chars>" or " FAILED" or "". */
static void failure_suffix(char *out, size_t cap, const char *reason) {
    out[0] = '\0';
    if (!reason || !*reason) {
        snprintf(out, cap, " %s", SUM_FAILED_LABEL);
        return;
    }
    char preview[128];
    size_t o = 0;
    bool space = false;
    for (const char *p = reason; *p && o + 1 < sizeof(preview); p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            if (o && !space) preview[o++] = ' ', space = true;
            continue;
        }
        preview[o++] = (char)c;
        space = false;
    }
    while (o && preview[o - 1] == ' ') o--;
    preview[o] = '\0';
    if (o > 80) { preview[80] = '\0'; }
    snprintf(out, cap, " %s: %s", SUM_FAILED_LABEL, preview);
}

/* Suffix from a parsed payload: top-level error, or success:false. */
static void payload_failure_suffix(const json_t *payload, char *out, size_t cap) {
    out[0] = '\0';
    if (!payload || payload->type != JSON_OBJECT) return;
    const json_t *error = json_obj_get(payload, "error");
    const json_t *success = json_obj_get(payload, "success");
    bool failed = (error && error->type != JSON_NULL) ||
                  (success && success->type == JSON_BOOL && !success->bool_val);
    if (!failed) return;
    const char *reason = NULL;
    if (error && error->type == JSON_STRING) reason = error->str_val;
    else if (error && error->type == JSON_NUMBER) { static char nb[32]; snprintf(nb, sizeof(nb), "%.0f", error->num_val); reason = nb; }
    failure_suffix(out, cap, reason);
}

/* Parse a tool payload; returns NULL for empty/invalid/non-object, like _json_dict. */
static json_t *parse_object(const char *text) {
    if (!text || !*text) return NULL;
    json_t *v = json_parse(text, NULL);
    if (v && v->type != JSON_OBJECT) { json_free(v); return NULL; }
    return v;
}

static void result_failure_suffix(const char *content, char *out, size_t cap) {
    json_t *payload = parse_object(content);
    payload_failure_suffix(payload, out, cap);
    if (payload) json_free(payload);
}

/* "{k}={v}" for the first two args, value truncated to 40 chars: Python's fallback detail. */
static void first_args_detail(const json_t *args, char *out, size_t cap) {
    out[0] = '\0';
    if (!args || args->type != JSON_OBJECT) return;
    size_t n = json_object_size(args), o = 0;
    for (size_t i = 0; i < n && i < 2; i++) {
        const char *k = json_object_get_key_at(args, i);
        const json_t *v = json_object_get_at(args, i);
        if (!k) continue;
        char val[64] = "";
        if (v) {
            if (v->type == JSON_STRING) snprintf(val, sizeof(val), "%s", v->str_val ? v->str_val : "");
            else if (v->type == JSON_NUMBER) snprintf(val, sizeof(val), "%s%.15g", v->num_val == (double)(long long)v->num_val ? "" : "", v->num_val);
            else if (v->type == JSON_BOOL) snprintf(val, sizeof(val), "%s", v->bool_val ? "True" : "False");
            else { char *s = json_serialize(v); snprintf(val, sizeof(val), "%s", s ? s : ""); free(s); }
        }
        if (strlen(val) > 40) val[40] = '\0';
        int w = snprintf(out + o, cap > o ? cap - o : 0, " %s=%s", k, val);
        if (w < 0) break;
        o += (size_t)w;
        if (o >= cap) break;
    }
}

static bool is_browser_tool(const char *name) {
    static const char *names[] = {"browser_navigate", "browser_click", "browser_snapshot",
                                  "browser_type", "browser_scroll", "browser_vision", NULL};
    for (size_t i = 0; names[i]; i++) if (strcmp(name, names[i]) == 0) return true;
    return false;
}

/* ── the summarizer ────────────────────────────────────────────────────────────────────────── */

void compress_summarize_tool(const char *tool_name, const char *tool_args,
                             const char *tool_content, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!tool_name) tool_name = "";

    json_t *args = parse_object(tool_args);
    const char *content = tool_content ? tool_content : "";
    size_t content_len = strlen(content);
    int line_count = line_count_of(content);

    char chars[32], fail[160], buf[SUM_MAX_SUMMARY];
    fmt_count(chars, sizeof(chars), content_len);

    /* terminal */
    if (strcmp(tool_name, "terminal") == 0) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "%s", jstr(args, "command", ""));
        if (strlen(jstr(args, "command", "")) > 80) {
            snprintf(cmd, sizeof(cmd), "%.77s...", jstr(args, "command", ""));
        }
        char exit_code[32] = "?";
        const char *m = strstr(content, "\"exit_code\"");
        if (m) {
            m = strchr(m, ':');
            if (m) {
                m++;
                while (*m == ' ') m++;
                size_t i = 0;
                if (*m == '-') exit_code[i++] = *m++;
                while (*m >= '0' && *m <= '9' && i < sizeof(exit_code) - 1) exit_code[i++] = *m++;
                exit_code[i] = '\0';
            }
        }
        snprintf(out, out_sz, "[terminal] ran `%s` -> exit %s, %d lines output", cmd, exit_code, line_count);
        goto done;
    }

    /* read_file */
    if (strcmp(tool_name, "read_file") == 0) {
        snprintf(out, out_sz, "[read_file] read %s from line %.0f (%s chars)",
                 jstr(args, "path", "?"), jnum(args, "offset", 1), chars);
        goto done;
    }

    /* write_file */
    if (strcmp(tool_name, "write_file") == 0) {
        result_failure_suffix(content, fail, sizeof(fail));
        if (fail[0]) {
            snprintf(out, out_sz, "[write_file] %s%s", jstr(args, "path", "?"), fail);
            goto done;
        }
        const char *written = jstr(args, "content", NULL);
        if (written) {
            int lines = 1;
            for (const char *p = written; *p; p++) if (*p == '\n') lines++;
            snprintf(out, out_sz, "[write_file] wrote to %s (%d lines)", jstr(args, "path", "?"), lines);
        } else {
            snprintf(out, out_sz, "[write_file] wrote to %s (? lines)", jstr(args, "path", "?"));
        }
        goto done;
    }

    /* search_files */
    if (strcmp(tool_name, "search_files") == 0) {
        char count[32] = "?";
        const char *m = strstr(content, "\"total_count\"");
        if (m) {
            m = strchr(m, ':');
            if (m) {
                m++;
                while (*m == ' ') m++;
                size_t i = 0;
                while (*m >= '0' && *m <= '9' && i < sizeof(count) - 1) count[i++] = *m++;
                count[i] = '\0';
            }
        }
        snprintf(out, out_sz, "[search_files] %s search for '%s' in %s -> %s matches",
                 jstr(args, "target", "content"), jstr(args, "pattern", "?"),
                 jstr(args, "path", "."), count);
        goto done;
    }

    /* patch */
    if (strcmp(tool_name, "patch") == 0) {
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[patch] %s in %s (%s chars result)%s",
                 jstr(args, "mode", "replace"), jstr(args, "path", "?"), chars, fail);
        goto done;
    }

    /* browser_* */
    if (is_browser_tool(tool_name)) {
        const char *url = jstr(args, "url", "");
        const char *ref = jstr(args, "ref", "");
        char detail[512] = "";
        if (url && *url) snprintf(detail, sizeof(detail), " %s", url);
        else if (ref && *ref) snprintf(detail, sizeof(detail), " ref=%s", ref);
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[%s]%s (%s chars)%s", tool_name, detail, chars, fail);
        goto done;
    }

    /* web_search */
    if (strcmp(tool_name, "web_search") == 0) {
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[web_search] query='%s' (%s chars result)%s",
                 jstr(args, "query", "?"), chars, fail);
        goto done;
    }

    /* web_extract */
    if (strcmp(tool_name, "web_extract") == 0) {
        const char *first = "?";
        const json_t *urls = args ? json_obj_get(args, "urls") : NULL;
        if (urls && urls->type == JSON_ARRAY && json_len(urls) > 0) {
            const json_t *u0 = json_get(urls, 0);
            if (u0 && u0->type == JSON_STRING) first = u0->str_val ? u0->str_val : "?";
        }
        char more[32] = "";
        if (urls && urls->type == JSON_ARRAY && json_len(urls) > 1) {
            snprintf(more, sizeof(more), " (+%zu more)", json_len(urls) - 1);
        }
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[web_extract] %s%s (%s chars)%s", first, more, chars, fail);
        goto done;
    }

    /* delegate_task */
    if (strcmp(tool_name, "delegate_task") == 0) {
        const char *goal = jstr(args, "goal", "");
        char trimmed[128];
        if (strlen(goal) > 60) snprintf(trimmed, sizeof(trimmed), "%.57s...", goal);
        else snprintf(trimmed, sizeof(trimmed), "%s", goal);
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[delegate_task] '%s' (%s chars result)%s", trimmed, chars, fail);
        goto done;
    }

    /* execute_code */
    if (strcmp(tool_name, "execute_code") == 0) {
        const char *code = jstr(args, "code", "");
        char preview[96];
        size_t o = 0;
        for (const char *p = code; *p && o < 60; p++) preview[o++] = (*p == '\n' ? ' ' : *p);
        preview[o] = '\0';
        snprintf(out, out_sz, "[execute_code] `%s%s` (%d lines output)",
                 preview, strlen(code) > 60 ? "..." : "", line_count);
        goto done;
    }

    /* skill_view */
    if (strcmp(tool_name, "skill_view") == 0) {
        const char *name = jstr(args, "name", "?");
        result_failure_suffix(content, fail, sizeof(fail));
        char marker[256] = "";
        /* Python: the summary replaces the body, so a healthy load still says "gone, reload" — only a
         * failed load (which already carries its own FAILED reason) drops the marker. */
        if (strcmp(name, "?") != 0 && (strstr(content, "SKILL_PRUNED") || !fail[0]))
            snprintf(marker, sizeof(marker),
                     " [SKILL_PRUNED: content lost in compression; reload with skill_view(name='%s')]", name);
        snprintf(out, out_sz, "[skill_view] name=%s%s (%s chars)%s", name, fail, chars, marker);
        goto done;
    }

    /* skills_list */
    if (strcmp(tool_name, "skills_list") == 0) {
        const char *category = jstr(args, "category", "");
        char scope[160] = "", listed[64] = "";
        if (category && *category) snprintf(scope, sizeof(scope), " category=%s", category);
        const json_t *count = args ? json_obj_get(args, "count") : NULL;   /* payload, not args */
        json_t *payload = parse_object(content);
        const json_t *pcount = payload ? json_obj_get(payload, "count") : NULL;
        (void)count;
        if (pcount && pcount->type == JSON_NUMBER)
            snprintf(listed, sizeof(listed), " %.0f skills", pcount->num_val);
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[skills_list]%s%s%s (%s chars)", scope, listed, fail, chars);
        if (payload) json_free(payload);
        goto done;
    }

    /* skill_manage */
    if (strcmp(tool_name, "skill_manage") == 0) {
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[skill_manage] %s %s%s (%s chars)",
                 jstr(args, "action", "?"), jstr(args, "name", "?"), fail, chars);
        goto done;
    }

    /* vision_analyze */
    if (strcmp(tool_name, "vision_analyze") == 0) {
        const char *q = jstr(args, "question", "");
        char trimmed[64];
        snprintf(trimmed, sizeof(trimmed), "%.50s", q);
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[vision_analyze] '%s' (%s chars)%s", trimmed, chars, fail);
        goto done;
    }

    /* memory */
    if (strcmp(tool_name, "memory") == 0) {
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[memory] %s on %s%s",
                 jstr(args, "action", "?"), jstr(args, "target", "?"), fail);
        goto done;
    }

    /* todo_list — Python returns a fixed line, no counts, no outcome. */
    if (strcmp(tool_name, "todo_list") == 0) {
        snprintf(out, out_sz, "[todo] updated task list");
        goto done;
    }

    /* text_to_speech */
    if (strcmp(tool_name, "text_to_speech") == 0) {
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[text_to_speech] generated audio (%s chars)%s", chars, fail);
        goto done;
    }

    /* BEST-EFFORT (not graded yet): clarify / cronjob_manage / process_manage */
    if (strcmp(tool_name, "clarify") == 0) {
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[clarify]%s", fail[0] ? fail : " asked the user");
        goto done;
    }
    if (strcmp(tool_name, "cronjob_manage") == 0 || strcmp(tool_name, "cronjob") == 0) {
        json_t *payload = parse_object(content);
        char jfail[160] = "";
        payload_failure_suffix(payload, jfail, sizeof(jfail));
        if (!jfail[0] && payload) {
            const json_t *job = json_obj_get(payload, "job");
            if (job && job->type == JSON_OBJECT) {
                const json_t *err = json_obj_get(job, "execution_error");
                const json_t *ok = json_obj_get(job, "execution_success");
                if (err && err->type != JSON_NULL) failure_suffix(jfail, sizeof(jfail), err->str_val);
                else if (ok && ok->type == JSON_BOOL && !ok->bool_val) failure_suffix(jfail, sizeof(jfail), NULL);
            }
        }
        snprintf(out, out_sz, "[cronjob] %s%s", jstr(args, "action", "?"), jfail);
        if (payload) json_free(payload);
        goto done;
    }
    if (strcmp(tool_name, "process_manage") == 0 || strcmp(tool_name, "process") == 0) {
        json_t *payload = parse_object(content);
        char jfail[160] = "";
        payload_failure_suffix(payload, jfail, sizeof(jfail));
        if (!jfail[0] && payload) {
            const json_t *ec = json_obj_get(payload, "exit_code");
            if (ec && ec->type == JSON_NUMBER && ec->num_val != 0) {
                char reason[64];
                snprintf(reason, sizeof(reason), "exit code %.0f", ec->num_val);
                failure_suffix(jfail, sizeof(jfail), reason);
            }
        }
        snprintf(out, out_sz, "[process] %s session=%s%s", jstr(args, "action", "?"),
                 jstr(args, "session_id", "?"), jfail);
        if (payload) json_free(payload);
        goto done;
    }

    /* Fallback: "[name]{ k=v for the first two args} (N chars result)" + outcome. */
    {
        char detail[256];
        first_args_detail(args, detail, sizeof(detail));
        result_failure_suffix(content, fail, sizeof(fail));
        snprintf(out, out_sz, "[%s]%s (%s chars result)%s", tool_name, detail, chars, fail);
    }

done:
    if (args) json_free(args);
    (void)buf;
}
