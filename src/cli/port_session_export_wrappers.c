/*
 * port_session_export_wrappers.c — C port of hermes_cli/session_export.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

#include <time.h>

/* helpers */
static char *sexp_collapse(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    size_t o = 0, sp = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\n' || s[i] == '\r' || s[i] == '\t') { sp = 1; continue; }
        if (sp && o > 0) out[o++] = ' ';
        sp = 0;
        out[o++] = s[i];
    }
    while (o > 0 && out[o-1] == ' ') o--;
    out[o] = '\0';
    return out;
}
static const char *sexp_json_get_str(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && json_is_string(v)) ? json_string_value(v) : NULL;
}
static char *sexp_msg_text(json_t *msg) {
    /* _message_text: content may be a string or a list of content parts. */
    json_t *c = json_obj_get(msg, "content");
    if (!c) return strdup("");
    if (c->type == JSON_STRING) return strdup(json_string_value(c));
    if (c->type == JSON_ARRAY) {
        size_t cap = 256, len = 0;
        char *buf = malloc(cap);
        buf[0] = '\0';
        for (size_t i = 0; i < json_len(c); i++) {
            json_t *part = json_get(c, i);
            if (!part || part->type != JSON_OBJECT) continue;
            json_t *pt = json_obj_get(part, "text");
            if (pt && json_is_string(pt)) {
                const char *s = json_string_value(pt);
                size_t need = len + strlen(s) + 2;
                if (need > cap) { cap = need * 2; buf = realloc(buf, cap); }
                strcat(buf, s);
                len += strlen(s);
            }
        }
        return buf;
    }
    return strdup("");
}
static void sexp_format_timestamp(json_t *msg, char *out, size_t outsz) {
    /* _format_timestamp: ISO "YYYY-MM-DD HH:MM:SS" when a timestamp int/str
     * is present, else empty. */
    json_t *ts = json_obj_get(msg, "timestamp");
    out[0] = '\0';
    if (!ts) return;
    if (json_is_string(ts)) { snprintf(out, outsz, "%s", json_string_value(ts)); return; }
    if (json_is_number(ts)) {
        long long v = (long long)json_number_value(ts);
        if (v <= 0) return;
        time_t t = (time_t)v;
        struct tm tmv;
        localtime_r(&t, &tmv);
        strftime(out, outsz, "%Y-%m-%d %H:%M:%S", &tmv);
    }
}

/* forward decls: the dispatcher precedes the renderers in file order. */
int sexp_u_render_user_prompts_markdown(const char *arg);
int sexp_u_render_full_markdown(const char *arg);

/* PoP: normalize_export_format @ hermes_cli/session_export.py:normalize_export_format */
int sexp_normalize_export_format(const char *arg) {
    /* Python: strip/lower; md -> markdown; jsonl/markdown only. Arg = fmt
     * (default jsonl). */
    const char *p = (arg && *arg) ? arg : "jsonl";
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) n--;
    if (n == 2 && strncasecmp(p, "md", 2) == 0) { printf("markdown\n"); return 0; }
    if (n == 5 && strncasecmp(p, "jsonl", 5) == 0) { printf("jsonl\n"); return 0; }
    if (n == 8 && strncasecmp(p, "markdown", 8) == 0) { printf("markdown\n"); return 0; }
    printf("unsupported: %.*s\n", (int)n, p);
    return 1;
}

/* PoP: normalize_export_only @ hermes_cli/session_export.py:normalize_export_only */
int sexp_normalize_export_only(const char *arg) {
    /* Python: strip/lower; user* -> "user-prompts"; else ValueError. Arg =
     * only (empty = None). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) n--;
    if (n == 4 && strncasecmp(p, "user", 4) == 0) { printf("user-prompts\n"); return 0; }
    if (n == 7 && strncasecmp(p, "prompts", 7) == 0) { printf("user-prompts\n"); return 0; }
    if (n == 12 && strncasecmp(p, "user-prompts", 12) == 0) { printf("user-prompts\n"); return 0; }
    if (n == 12 && strncasecmp(p, "user_prompts", 12) == 0) { printf("user-prompts\n"); return 0; }
    printf("unsupported: %.*s\n", (int)n, p);
    return 1;
}

/* PoP: render_sessions_export @ hermes_cli/session_export.py:render_sessions_export */
int sexp_render_sessions_export(const char *arg) {
    /* Python: jsonl or markdown render. Arg = "fmt\tonly\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    printf("%s\n", result);
    return 0;
}

/* PoP: export_record_count @ hermes_cli/session_export.py:export_record_count */
int sexp_export_record_count(const char *arg) {
    /* Python: (count, "prompt"|"session"). Arg = "only\tcount". */
    if (!arg || !*arg) { printf("0 session\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long count = tab ? strtol(tab + 1, NULL, 10) : 0;
    if (tab && tab > arg && strncmp(arg, "user", 4) == 0) printf("%ld prompt\n", count);
    else printf("%ld session\n", count);
    return 0;
}

/* PoP: iter_user_prompt_records @ hermes_cli/session_export.py:iter_user_prompt_records */
int sexp_iter_user_prompt_records(const char *arg) {
    /* Python: user-prompt records. Arg = "records_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: _render_jsonl @ hermes_cli/session_export.py:_render_jsonl */
int sexp_u_render_jsonl(const char *arg) {
    /* Python: json.dumps per row joined by \n (+ trailing). Arg = "only" or
     * rows JSON array (each row dumped verbatim). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (strcmp(arg, "user-prompts") == 0) { printf("\n"); return 0; }
    json_t *arr = json_parse(arg, NULL);
    if (!arr || !json_is_array(arr)) {
        if (arr) json_free(arr);
        printf("%s\n", arg);
        return 0;
    }
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_get(arr, i);
        char *s = json_dumps(row, 0);
        printf("%s\n", s ? s : "");
        free(s);
    }
    json_free(arr);
    return 0;
}

/* PoP: _render_markdown @ hermes_cli/session_export.py:_render_markdown */
int sexp_u_render_markdown(const char *arg) {
    /* Python (sessions, only): "user-prompts" -> the prompts renderer,
     * anything else -> the full renderer. Arg = "only\tsessionsJSON". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *only = tab ? arg : "full";
    size_t olen = tab ? (size_t)(tab - arg) : 4;
    const char *sjson = tab ? tab + 1 : arg;
    if (olen == 12 && strncmp(only, "user-prompts", 12) == 0)
        return sexp_u_render_user_prompts_markdown(sjson);
    return sexp_u_render_full_markdown(sjson);
}
/* PoP: _render_user_prompts_markdown @ hermes_cli/session_export.py:_render_user_prompts_markdown */
int sexp_u_render_user_prompts_markdown(const char *arg) {
    /* Python: "# User prompts for session <id>" + metadata; per prompt
     * "{marker} {index}. {timestamp}" + fenced content. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *arr = json_parse(arg, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); printf("\n"); return 0; }
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_t *s = json_get(arr, i);
        const char *sid = sexp_json_get_str(s, "id");
        if (!sid) sid = sexp_json_get_str(s, "session_id");
        if (n == 1)
            printf("# User prompts for session %s\n", sid ? sid : "unknown");
        else
            printf("# User prompts for session %s\n", sid ? sid : "unknown");
        const char *v;
        if ((v = sexp_json_get_str(s, "source"))) printf("- Source: `%s`\n", v);
        if ((v = sexp_json_get_str(s, "model"))) printf("- Model: `%s`\n", v);
        printf("\n");
        int hl = n == 1 ? 2 : 3;
        char marker[16];
        memset(marker, '#', (size_t)hl);
        marker[hl] = '\0';
        json_t *msgs = json_obj_get(s, "messages");
        int shown = 0, idx = 1;
        if (msgs && msgs->type == JSON_ARRAY) {
            for (size_t k = 0; k < json_len(msgs); k++) {
                json_t *msg = json_get(msgs, k);
                if (!msg || msg->type != JSON_OBJECT) continue;
                const char *role = sexp_json_get_str(msg, "role");
                if (!role || strcmp(role, "user") != 0) continue;
                char ts[64];
                sexp_format_timestamp(msg, ts, sizeof(ts));
                printf("%s %d. %s\n", marker, idx++, ts[0] ? ts : "timestamp unavailable");
                char *txt = sexp_msg_text(msg);
                char f[16] = "```";
                while (strstr(txt, f)) strcat(f, "`");
                printf("%s\n%s\n%s\n\n", f, txt, f);
                free(txt);
                shown++;
            }
        }
        if (!shown) printf("_No user prompts found._\n\n");
    }
    json_free(arr);
    return 0;
}

/* PoP: _append_prompt_records @ hermes_cli/session_export.py:_append_prompt_records */
int sexp_u_append_prompt_records(const char *arg) {
    /* Python: prompt record headings. Arg = "records_json\theading_level". */
    if (!arg || !*arg) { printf("_No user prompts found._\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long hl = tab ? strtol(tab + 1, NULL, 10) : 3;
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_array(j) || json_array_size(j) == 0) {
        if (j) json_free(j);
        printf("_No user prompts found._\n\n");
        return 0;
    }
    size_t n = json_array_size(j);
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_array_get(j, i);
        if (!p) continue;
        long idx = (long)json_get_num(p, "index", 0);
        const char *ts = json_get_str(p, "created_at", "timestamp unavailable");
        char marker[16];
        for (long k = 0; k < hl && k < 12; k++) marker[k] = '#';
        marker[hl < 12 ? hl : 12] = '\0';
        printf("%s %ld. %s\n", marker, idx, ts);
        const char *mid = json_get_str(p, "message_id", "");
        if (mid[0]) { printf("Message ID: `%s`\n\n", mid); }
        const char *text = json_get_str(p, "text", "");
        printf("%s\n\n", text);
    }
    json_free(j);
    return 0;
}

/* PoP: _render_full_markdown @ hermes_cli/session_export.py:_render_full_markdown */
int sexp_u_render_full_markdown(const char *arg) {
    /* Python: single session -> "# Session: <title>" + metadata + messages
     * at heading 2; multiple -> "# Hermes sessions export" grouped with
     * heading 2/3. Trailing blank lines trimmed, final newline kept. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *arr = json_parse(arg, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); printf("\n"); return 0; }
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_t *s = json_get(arr, i);
        const char *title = sexp_json_get_str(s, "title");
        char *t2 = title ? sexp_collapse(title) : NULL;
        const char *sid = sexp_json_get_str(s, "id");
        if (!sid) sid = sexp_json_get_str(s, "session_id");
        if (n == 1) {
            printf("# Session: %s\n", (t2 && *t2) ? t2 : (sid ? sid : "unknown"));
        } else if (i == 0) {
            printf("# Hermes sessions export\n\n## Session: %s\n",
                   (t2 && *t2) ? t2 : (sid ? sid : "unknown"));
        } else {
            printf("## Session: %s\n", (t2 && *t2) ? t2 : (sid ? sid : "unknown"));
        }
        free(t2);
        printf("- Session ID: `%s`\n", sid ? sid : "unknown");
        const char *v;
        if ((v = sexp_json_get_str(s, "source"))) printf("- Source: `%s`\n", v);
        if ((v = sexp_json_get_str(s, "model"))) printf("- Model: `%s`\n", v);
        if ((v = sexp_json_get_str(s, "title"))) {
            char *c = sexp_collapse(v);
            printf("- Title: %s\n", c);
            free(c);
        }
        printf("\n");
        int hl = n == 1 ? 2 : 3;
        char marker[16];
        memset(marker, '#', (size_t)hl);
        marker[hl] = '\0';
        json_t *msgs = json_obj_get(s, "messages");
        int visible = 0;
        if (msgs && msgs->type == JSON_ARRAY) {
            for (size_t k = 0; k < json_len(msgs); k++) {
                json_t *msg = json_get(msgs, k);
                if (!msg || msg->type != JSON_OBJECT) continue;
                const char *role = sexp_json_get_str(msg, "role");
                if (role && strcmp(role, "system") == 0) continue;
                visible++;
                char ts[64];
                sexp_format_timestamp(msg, ts, sizeof(ts));
                if (role && strcmp(role, "tool") == 0) {
                    const char *tn = sexp_json_get_str(msg, "tool_name");
                    if (!tn) tn = sexp_json_get_str(msg, "name");
                    if (!tn) tn = "tool";
                    char *ht = sexp_collapse(tn);
                    printf("%s Tool: %s%s%s\n\n", marker, ht, ts[0] ? " - " : "", ts);
                    char *txt = sexp_msg_text(msg);
                    char f[16] = "```";
                    while (strstr(txt, f)) strcat(f, "`");
                    printf("<details><summary>%s</summary>\n\n%s\n%s\n%s\n\n</details>\n\n",
                           ht, f, txt, f);
                    free(txt);
                    free(ht);
                } else {
                    const char *label = "unknown";
                    if (role && strcmp(role, "user") == 0) label = "User";
                    else if (role && strcmp(role, "assistant") == 0) label = "Assistant";
                    else if (role) label = role;
                    printf("%s %s%s%s\n\n", marker, label, ts[0] ? " - " : "", ts);
                    char *txt = sexp_msg_text(msg);
                    printf("%s\n\n", txt);
                    free(txt);
                }
            }
        }
        if (!visible) printf("_No messages found._\n\n");
    }
    json_free(arr);
    return 0;
}
/* PoP: _append_session_messages @ hermes_cli/session_export.py:_append_session_messages */
int sexp_u_append_session_messages(const char *arg) {
    /* Python (lines, session, heading_level): "## Tool: X" fenced details
     * blocks for tool rows; "{marker} {Label}{ - ts}" + content otherwise.
     * Arg = "heading_level\tsessionJSON". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    int hl = tab ? atoi(arg) : 2;
    const char *sjson = tab ? tab + 1 : arg;
    json_t *s = json_parse(sjson, NULL);
    if (!s || s->type != JSON_OBJECT) { if (s) json_free(s); return 0; }
    json_t *msgs = json_obj_get(s, "messages");
    char marker[16];
    memset(marker, '#', (size_t)hl);
    marker[hl] = '\0';
    int visible = 0;
    if (msgs && msgs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(msgs); i++) {
            json_t *msg = json_get(msgs, i);
            if (!msg || msg->type != JSON_OBJECT) continue;
            const char *role = sexp_json_get_str(msg, "role");
            if (role && strcmp(role, "system") == 0) continue;
            visible++;
            char ts[64];
            sexp_format_timestamp(msg, ts, sizeof(ts));
            if (role && strcmp(role, "tool") == 0) {
                const char *tn = sexp_json_get_str(msg, "tool_name");
                if (!tn) tn = sexp_json_get_str(msg, "name");
                if (!tn) tn = "tool";
                char *ht = sexp_collapse(tn);
                printf("%s Tool: %s%s\n\n", marker, ht, ts[0] ? " - " : "");
                if (ts[0]) printf("%s\n", ts);
                char *txt = sexp_msg_text(msg);
                char *fence = malloc(strlen(txt) + 16);
                char f[16] = "```";
                while (strstr(txt, f)) strcat(f, "`");
                snprintf(fence, strlen(txt) + 16, "%s\n%s\n%s", f, txt, f);
                printf("<details><summary>%s</summary>\n\n%s\n\n</details>\n\n", ht, fence);
                free(fence); free(txt); free(ht);
            } else {
                const char *label = "unknown";
                if (role && strcmp(role, "user") == 0) label = "User";
                else if (role && strcmp(role, "assistant") == 0) label = "Assistant";
                else if (role) label = role; /* role.title() */
                if (label == role && role) {
                    char *lc = strdup(role);
                    if (*lc >= 'a' && *lc <= 'z') *lc = (char)(*lc - 32);
                    printf("%s %s%s%s\n\n", marker, lc, ts[0] ? " - " : "", ts);
                    free(lc);
                } else {
                    printf("%s %s%s%s\n\n", marker, label, ts[0] ? " - " : "", ts);
                }
                char *txt = sexp_msg_text(msg);
                printf("%s\n\n", txt);
                free(txt);
            }
        }
    }
    if (!visible) printf("_No messages found._\n\n");
    json_free(s);
    return 0;
}

/* PoP: _messages @ hermes_cli/session_export.py:_messages */
int sexp_u_messages(const char *arg) {
    /* Python: session.get("messages") or []; keep only dict entries.
     * Arg = session JSON. Print the filtered JSON array. */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    json_t *session = json_parse(arg, NULL);
    if (!session || !json_is_object(session)) {
        if (session) json_free(session);
        printf("[]\n");
        return 0;
    }
    json_t *messages = json_obj_get(session, "messages");
    json_t *out = json_array();
    if (messages && json_is_array(messages)) {
        size_t n = json_len(messages);
        for (size_t i = 0; i < n; i++) {
            json_t *m = json_get(messages, i);
            if (m && json_is_object(m)) json_append(out, json_copy(m));
        }
    }
    char *s = json_serialize(out);
    printf("%s\n", s ? s : "[]");
    free(s);
    json_free(out);
    json_free(session);
    return 0;
}

/* PoP: _message_text @ hermes_cli/session_export.py:_message_text */
int sexp_u_message_text(const char *arg) {
    /* Python: str passthrough; list -> join parts; dict text/content. Arg =
     * "type\tvalue" (type: str/list/dict/other). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *typ = arg;
    const char *val = tab ? tab + 1 : "";
    if (strcmp(typ, "str") == 0) { printf("%s\n", val); return 0; }
    if (strcmp(typ, "list") == 0) {
        json_t *j = json_parse(val, NULL);
        if (j && json_is_array(j)) {
            size_t n = json_array_size(j);
            int first = 1;
            for (size_t i = 0; i < n; i++) {
                json_t *it = json_array_get(j, i);
                const char *s = it && json_is_string(it) ? json_string_value(it) : "";
                if (s[0]) {
                    if (!first) printf("\n");
                    printf("%s", s);
                    first = 0;
                }
            }
            printf("\n");
            json_free(j);
            return 0;
        }
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    if (strcmp(typ, "dict") == 0) {
        json_t *j = json_parse(val, NULL);
        if (j && json_is_object(j)) {
            const char *t = json_get_str(j, "text", "");
            if (!t[0]) t = json_get_str(j, "content", "");
            if (t[0]) { printf("%s\n", t); json_free(j); return 0; }
            char *s = json_dumps(j, 0);
            printf("%s\n", s ? s : "");
            free(s);
            json_free(j);
            return 0;
        }
        if (j) json_free(j);
        printf("%s\n", val);
        return 0;
    }
    printf("%s\n", val);
    return 0;
}

/* PoP: _content_part_text @ hermes_cli/session_export.py:_content_part_text */
int sexp_u_content_part_text(const char *arg) {
    /* Python: str part; dict text/content string; else json dump; else str.
     * Arg = part JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *part = json_parse(arg, NULL);
    if (!part) { printf("%s\n", arg); return 0; }
    if (json_is_string(part)) {
        printf("%s\n", json_string_value(part));
        json_free(part);
        return 0;
    }
    if (json_is_object(part)) {
        const char *v = json_get_str(part, "text", "");
        if (*v) { printf("%s\n", v); json_free(part); return 0; }
        v = json_get_str(part, "content", "");
        if (*v) { printf("%s\n", v); json_free(part); return 0; }
    }
    char *s = json_dumps(part, 0);
    printf("%s\n", s ? s : "");
    free(s);
    json_free(part);
    return 0;
}

/* PoP: _session_metadata_lines @ hermes_cli/session_export.py:_session_metadata_lines */
int sexp_u_session_metadata_lines(const char *arg) {
    /* Python: "- Session ID: `<id>`" + Source/Model/Title lines. */
    if (!arg || !*arg) return 0;
    json_t *s = json_parse(arg, NULL);
    if (!s || s->type != JSON_OBJECT) { if (s) json_free(s); return 0; }
    const char *id = sexp_json_get_str(s, "id");
    if (!id) id = sexp_json_get_str(s, "session_id");
    printf("- Session ID: `%s`\n", id ? id : "unknown");
    const char *v;
    if ((v = sexp_json_get_str(s, "source"))) printf("- Source: `%s`\n", v);
    if ((v = sexp_json_get_str(s, "model"))) printf("- Model: `%s`\n", v);
    if ((v = sexp_json_get_str(s, "title"))) {
        char *c = sexp_collapse(v);
        printf("- Title: %s\n", c);
        free(c);
    }
    json_free(s);
    return 0;
}

/* PoP: _session_id @ hermes_cli/session_export.py:_session_id */
int sexp_u_session_id(const char *arg) {
    /* Python: session.get("id") or session.get("session_id") or "unknown". */
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    json_t *s = json_parse(arg, NULL);
    if (!s || s->type != JSON_OBJECT) { if (s) json_free(s); printf("unknown\n"); return 0; }
    const char *id = sexp_json_get_str(s, "id");
    if (!id) id = sexp_json_get_str(s, "session_id");
    printf("%s\n", id ? id : "unknown");
    json_free(s);
    return 0;
}

/* PoP: _session_title_or_id @ hermes_cli/session_export.py:_session_title_or_id */
int sexp_u_session_title_or_id(const char *arg) {
    /* Python: stripped title or _session_id(session). */
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    json_t *s = json_parse(arg, NULL);
    if (!s || s->type != JSON_OBJECT) { if (s) json_free(s); printf("unknown\n"); return 0; }
    const char *title = sexp_json_get_str(s, "title");
    if (title) {
        char *c = sexp_collapse(title);
        if (*c) { printf("%s\n", c); free(c); json_free(s); return 0; }
        free(c);
    }
    const char *id = sexp_json_get_str(s, "id");
    if (!id) id = sexp_json_get_str(s, "session_id");
    printf("%s\n", id ? id : "unknown");
    json_free(s);
    return 0;
}

/* PoP: _heading_text @ hermes_cli/session_export.py:_heading_text */
int sexp_u_heading_text(const char *arg) {
    /* Python: " ".join(str(value).splitlines()).strip() or "unknown". */
    char *c = sexp_collapse(arg ? arg : "");
    if (!*c) { printf("unknown\n"); free(c); return 0; }
    printf("%s\n", c);
    free(c);
    return 0;
}

/* PoP: _inline_text @ hermes_cli/session_export.py:_inline_text */
int sexp_u_inline_text(const char *arg) {
    /* Python: " ".join(value.splitlines()).strip(). */
    char *c = sexp_collapse(arg ? arg : "");
    printf("%s\n", c);
    free(c);
    return 0;
}

/* PoP: _fenced_text @ hermes_cli/session_export.py:_fenced_text */
int sexp_u_fenced_text(const char *arg) {
    /* Python (language, text): fence grows while present in the text. */
    if (!arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *lang = tab ? arg : "";
    size_t llen = tab ? (size_t)(tab - arg) : 0;
    const char *text = tab ? tab + 1 : arg;
    char fence[16] = "```";
    while (strstr(text, fence)) strcat(fence, "`");
    printf("%.*s\n%s\n%s\n", (int)llen, lang, text, fence);
    return 0;
}

/* PoP: _finish_markdown @ hermes_cli/session_export.py:_finish_markdown */
int sexp_u_finish_markdown(const char *arg) {
    /* Python: strip trailing empty lines, join, add one final newline. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    size_t n = strlen(arg);
    while (n > 0 && (arg[n-1] == '\n' || arg[n-1] == '\r')) n--;
    while (n >= 1 && arg[n-1] == '\n') n--;
    /* also drop blank lines at the tail */
    while (n >= 2 && arg[n-1] == '\n' && (arg[n-2] == '\n' || arg[n-2] == '\r')) n--;
    printf("%.*s\n", (int)n, arg);
    return 0;
}
