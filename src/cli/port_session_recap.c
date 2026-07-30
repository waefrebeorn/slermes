/*
 * port_session_recap.c — Faithful C11 port of hermes_cli/session_recap.py
 *
 * Pure session-recap computation from in-memory conversation history.
 * Messages modeled as libjson arrays/objects.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "json.h"
#include "session_recap.h"

/* PoP: session_coerce_text @ hermes_cli/session_recap.py:_coerce_text */
void session_coerce_text(const json_t *value, char *out, size_t out_cap) {
    out[0] = '\0';
    if (!value) return;
    if (value->type == JSON_STRING) {
        if (value->str_val) { strncpy(out, value->str_val, out_cap-1); out[out_cap-1]='\0'; }
        return;
    }
    if (value->type == JSON_ARRAY) {
        size_t n = json_len(value);
        size_t off = 0;
        for (size_t i = 0; i < n; i++) {
            json_t *block = json_get(value, i);
            if (!block) continue;
            const char *text = NULL;
            if (block->type == JSON_STRING) text = block->str_val;
            else if (block->type == JSON_OBJECT) {
                const json_t *t = json_obj_get(block, "text");
                if (t && t->type == JSON_STRING) text = t->str_val;
            }
            if (text && text[0]) {
                size_t tl = strlen(text);
                if (off + tl + 1 < out_cap) {
                    if (off > 0) { out[off++] = '\n'; }
                    memcpy(out + off, text, tl);
                    off += tl;
                    out[off] = '\0';
                }
            }
        }
        return;
    }
    /* str(value) — for non-string/non-list, Python does str(value) */
    snprintf(out, out_cap, "%s", value->type == JSON_NUMBER ? "number" :
             value->type == JSON_BOOL ? "bool" :
             value->type == JSON_NULL ? "null" : "object");
}

/* PoP: session_tool_call_name_and_args @ hermes_cli/session_recap.py:_tool_call_name_and_args */
void session_tool_call_name_and_args(const json_t *tool_call, char *name_out, size_t name_cap, json_t **args_out) {
    name_out[0] = '\0';
    *args_out = json_object();
    if (!tool_call || tool_call->type != JSON_OBJECT) return;
    const json_t *fn = json_obj_get(tool_call, "function");
    if (!fn || fn->type != JSON_OBJECT) return;
    const json_t *name = json_obj_get(fn, "name");
    if (name && name->type == JSON_STRING && name->str_val) {
        strncpy(name_out, name->str_val, name_cap-1);
        name_out[name_cap-1] = '\0';
    }
    const json_t *raw_args = json_obj_get(fn, "arguments");
    if (raw_args) {
        if (raw_args->type == JSON_OBJECT) {
            json_free(*args_out);
            *args_out = json_copy(raw_args);
            return;
        }
        if (raw_args->type == JSON_STRING && raw_args->str_val && raw_args->str_val[0]) {
            json_t *parsed = json_parse(raw_args->str_val, NULL);
            if (parsed && parsed->type == JSON_OBJECT) {
                json_free(*args_out);
                *args_out = parsed;
            } else {
                if (parsed) json_free(parsed);
            }
            return;
        }
    }
}

/* PoP: session_count_visible_turns @ hermes_cli/session_recap.py:_count_visible_turns */
void session_count_visible_turns(const json_t *messages, int *users, int *assistants, int *tools) {
    *users = *assistants = *tools = 0;
    if (!messages || messages->type != JSON_ARRAY) return;
    size_t n = json_len(messages);
    for (size_t i = 0; i < n; i++) {
        json_t *msg = json_get(messages, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const json_t *role = json_obj_get(msg, "role");
        if (!role || role->type != JSON_STRING) continue;
        if (strcmp(role->str_val, "user") == 0) (*users)++;
        else if (strcmp(role->str_val, "assistant") == 0) (*assistants)++;
        else if (strcmp(role->str_val, "tool") == 0) (*tools)++;
    }
}

/* PoP: session_latest_user_prompt @ hermes_cli/session_recap.py:_latest_user_prompt */
int session_latest_user_prompt(const json_t *messages, char *out, size_t out_cap) {
    out[0] = '\0';
    if (!messages || messages->type != JSON_ARRAY) return 0;
    size_t n = json_len(messages);
    for (size_t i = n; i > 0; i--) {
        json_t *msg = json_get(messages, i-1);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const json_t *role = json_obj_get(msg, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val, "user") != 0) continue;
        char text[8192];
        session_coerce_text(json_obj_get(msg, "content"), text, sizeof(text));
        /* strip */
        char *s = text;
        while (*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++;
        size_t l = strlen(s);
        while (l > 0 && (s[l-1]==' '||s[l-1]=='\t'||s[l-1]=='\r'||s[l-1]=='\n')) s[--l]='\0';
        if (l > 0) { strncpy(out, s, out_cap-1); out[out_cap-1]='\0'; return 1; }
    }
    return 0;
}

/* PoP: session_latest_assistant_text @ hermes_cli/session_recap.py:_latest_assistant_text */
int session_latest_assistant_text(const json_t *messages, char *out, size_t out_cap) {
    out[0] = '\0';
    if (!messages || messages->type != JSON_ARRAY) return 0;
    size_t n = json_len(messages);
    for (size_t i = n; i > 0; i--) {
        json_t *msg = json_get(messages, i-1);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const json_t *role = json_obj_get(msg, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val, "assistant") != 0) continue;
        char text[8192];
        session_coerce_text(json_obj_get(msg, "content"), text, sizeof(text));
        char *s = text;
        while (*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++;
        size_t l = strlen(s);
        while (l > 0 && (s[l-1]==' '||s[l-1]=='\t'||s[l-1]=='\r'||s[l-1]=='\n')) s[--l]='\0';
        if (l > 0) { strncpy(out, s, out_cap-1); out[out_cap-1]='\0'; return 1; }
    }
    return 0;
}

/* PoP: session_recent_window @ hermes_cli/session_recap.py:_recent_window */
json_t *session_recent_window(const json_t *messages, int window) {
    if (!messages || messages->type != JSON_ARRAY) return json_array();
    size_t n = json_len(messages);
    int count = 0;
    size_t cut = 0;
    int found = 0;
    for (size_t i = n; i > 0; i--) {
        json_t *msg = json_get(messages, i-1);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const json_t *role = json_obj_get(msg, "role");
        if (role && role->type == JSON_STRING &&
            (strcmp(role->str_val, "user") == 0 || strcmp(role->str_val, "assistant") == 0)) {
            count++;
            if (count >= window) { cut = i-1; found = 1; break; }
        }
    }
    if (!found) return json_copy(messages);
    json_t *out = json_array();
    for (size_t i = cut; i < n; i++) {
        json_t *msg = json_get(messages, i);
        if (msg) json_append(out, json_copy(msg));
    }
    return out;
}

/* PoP: session_shortened_path @ hermes_cli/session_recap.py:_shortened_path */
/* cwd and home passed in (deterministic, no IO). */
void session_shortened_path(const char *path, const char *cwd, const char *home, char *out, size_t out_cap) {
    if (!path || !*path) { out[0]='\0'; return; }
    /* abspath: if relative, prepend cwd */
    char abs_path[4096];
    if (path[0] == '/') {
        strncpy(abs_path, path, sizeof(abs_path)-1);
    } else {
        snprintf(abs_path, sizeof(abs_path), "%s/%s", cwd ? cwd : "", path);
    }
    abs_path[sizeof(abs_path)-1] = '\0';
    /* expanduser ~ */
    if (strncmp(abs_path, "~/", 2) == 0 && home) {
        char tmp[4096];
        snprintf(tmp, sizeof(tmp), "%s%s", home, abs_path+1);
        strncpy(abs_path, tmp, sizeof(abs_path)-1);
    }
    if (cwd && strcmp(abs_path, cwd) == 0) { strncpy(out, ".", out_cap); return; }
    if (cwd) {
        size_t cl = strlen(cwd);
        if (strncmp(abs_path, cwd, cl) == 0 && abs_path[cl] == '/') {
            strncpy(out, abs_path + cl + 1, out_cap-1);
            out[out_cap-1] = '\0';
            return;
        }
    }
    if (home) {
        size_t hl = strlen(home);
        if (strncmp(abs_path, home, hl) == 0 && abs_path[hl] == '/') {
            snprintf(out, out_cap, "~/%s", abs_path + hl + 1);
            return;
        }
    }
    strncpy(out, abs_path, out_cap-1);
    out[out_cap-1] = '\0';
}

/* PoP: session_truncate @ hermes_cli/session_recap.py:_truncate */
void session_truncate(const char *text, int limit, char *out, size_t out_cap) {
    /* collapse whitespace */
    char collapsed[8192];
    size_t o = 0;
    int in_space = 0;
    for (const char *p = text ? text : ""; *p && o < sizeof(collapsed)-1; p++) {
        if (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') {
            if (!in_space && o > 0) { collapsed[o++] = ' '; in_space = 1; }
        } else { collapsed[o++] = *p; in_space = 0; }
    }
    collapsed[o] = '\0';
    size_t len = strlen(collapsed);
    if ((int)len <= limit) {
        strncpy(out, collapsed, out_cap-1);
        out[out_cap-1] = '\0';
        return;
    }
    /* text[:limit-1].rstrip() + "…" */
    size_t cut = limit - 1;
    if (cut > len) cut = len;
    while (cut > 0 && (collapsed[cut-1]==' '||collapsed[cut-1]=='\t')) cut--;
    if (cut >= out_cap - 1) cut = out_cap - 2;
    memcpy(out, collapsed, cut);
    out[cut] = 0xE2;  /* … UTF-8 */
    out[cut+1] = 0x80;
    out[cut+2] = 0xA6;
    out[cut+3] = '\0';
}

/* PoP: session_summarise_tool_activity @ hermes_cli/session_recap.py:_summarise_tool_activity */
void session_summarise_tool_activity(const json_t *tool_calls, json_t **counts_out, json_t **files_out) {
    *counts_out = json_array();
    *files_out = json_array();
    if (!tool_calls || tool_calls->type != JSON_ARRAY) return;
    /* count + collect files (reverse order) */
    json_t *counter = json_object();
    json_t *files_seen = json_array();
    json_t *files_set = json_object();
    size_t n = json_len(tool_calls);
    for (size_t i = n; i > 0; i--) {
        json_t *tc = json_get(tool_calls, i-1);
        if (!tc || tc->type != JSON_ARRAY || json_len(tc) < 2) continue;
        char name[256] = "";
        json_t *args = NULL;
        /* tc is [name, args] tuple */
        json_t *n = json_get(tc, 0);
        json_t *a = json_get(tc, 1);
        if (n && n->type == JSON_STRING) strncpy(name, n->str_val, sizeof(name)-1);
        if (!a || a->type != JSON_OBJECT) continue;
        (void)args;
        const json_t *existing = json_obj_get(counter, name);
        long cnt = existing ? (existing->type == JSON_NUMBER ? (long)existing->num_val : 0) : 0;
        cnt++;
        json_set(counter, name, json_number((double)cnt));
        /* file edit tools */
        const char *arg_key = NULL;
        if (strcmp(name, "write_file") == 0) arg_key = "path";
        else if (strcmp(name, "patch") == 0) arg_key = "path";
        else if (strcmp(name, "read_file") == 0) arg_key = "path";
        else if (strcmp(name, "skill_manage") == 0) arg_key = "file_path";
        else if (strcmp(name, "skill_view") == 0) arg_key = "file_path";
        if (arg_key) {
            const json_t *path = json_obj_get(a, arg_key);
            if (path && path->type == JSON_STRING && path->str_val && path->str_val[0]) {
                if (!json_obj_get(files_set, path->str_val)) {
                    json_set(files_set, path->str_val, json_bool(1));
                    char shortened[4096];
                    session_shortened_path(path->str_val, NULL, getenv("HOME"), shortened, sizeof(shortened));
                    json_append(files_seen, json_string(shortened));
                }
            }
        }
    }
    json_free(files_set);
    /* sort counts by (-count, name) */
    /* simple insertion sort */
    /* sort counts by (-count, name) */
    /* build array of (name, count) */
    char names[256][256];
    long counts[256];
    size_t ci = 0;
    for (size_t i = 0; i < counter->c.count && ci < 256; i++) {
        strncpy(names[ci], counter->c.keys[i], 255);
        names[ci][255] = '\0';
        counts[ci] = counter->c.items[i]->type == JSON_NUMBER ? (long)counter->c.items[i]->num_val : 0;
        ci++;
    }
    for (size_t i = 0; i < ci; i++) {
        for (size_t j = i+1; j < ci; j++) {
            if (counts[j] > counts[i] || (counts[j] == counts[i] && strcmp(names[j], names[i]) < 0)) {
                long tc = counts[i]; counts[i] = counts[j]; counts[j] = tc;
                char tn[256]; strcpy(tn, names[i]); strcpy(names[i], names[j]); strcpy(names[j], tn);
            }
        }
    }
    for (size_t i = 0; i < ci; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(names[i]));
        json_append(pair, json_number((double)counts[i]));
        json_append(*counts_out, pair);
    }
    *files_out = files_seen;
    json_free(counter);
}

/* PoP: session_build_recap @ hermes_cli/session_recap.py:build_recap */
char *session_build_recap(const json_t *messages, const char *session_title,
                           const char *session_id, const char *platform) {
    (void)platform;
    char *lines[64];
    int nlines = 0;

    /* header */
    char header[512];
    if (session_title && session_title[0]) {
        snprintf(header, sizeof(header), "Session recap — %s", session_title);
    } else if (session_id && session_id[0]) {
        char short_id[9];
        strncpy(short_id, session_id, 8);
        short_id[8] = '\0';
        snprintf(header, sizeof(header), "Session recap — %s", short_id);
    } else {
        strcpy(header, "Session recap");
    }
    lines[nlines++] = strdup(header);

    if (!messages || messages->type != JSON_ARRAY || json_len(messages) == 0) {
        lines[nlines++] = strdup("  (nothing to recap — no messages yet)");
        goto done;
    }

    int users, assistants, tool_msgs;
    session_count_visible_turns(messages, &users, &assistants, &tool_msgs);
    json_t *window = session_recent_window(messages, 20);
    int win_users, win_assistants, _;
    session_count_visible_turns(window, &win_users, &win_assistants, &_);

    char scope[256];
    snprintf(scope, sizeof(scope), "  Recent: %d user turn%s / %d assistant repl%s",
             win_users, win_users != 1 ? "s" : "",
             win_assistants, win_assistants != 1 ? "ies" : "y");
    if (users != win_users || assistants != win_assistants) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), " (of %d/%d total)", users, assistants);
        strcat(scope, tmp);
    }
    {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), ", %d tool result%s", tool_msgs, tool_msgs != 1 ? "s" : "");
        strcat(scope, tmp);
    }
    lines[nlines++] = strdup(scope);

    /* tool calls */
    json_t *tool_calls = json_array();
    size_t wn = json_len(window);
    for (size_t i = 0; i < wn; i++) {
        json_t *msg = json_get(window, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const json_t *role = json_obj_get(msg, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val, "assistant") != 0) continue;
        const json_t *tcs = json_obj_get(msg, "tool_calls");
        if (!tcs || tcs->type != JSON_ARRAY) continue;
        size_t tn = json_len(tcs);
        for (size_t j = 0; j < tn; j++) {
            char name[256] = "";
            json_t *args = NULL;
            session_tool_call_name_and_args(json_get(tcs, j), name, sizeof(name), &args);
            if (name[0]) {
                json_t *pair = json_array();
                json_append(pair, json_string(name));
                json_append(pair, args);
                json_append(tool_calls, pair);
            } else if (args) {
                json_free(args);
            }
        }
    }

    json_t *counts = NULL, *files = NULL;
    session_summarise_tool_activity(tool_calls, &counts, &files);

    if (json_len(counts) > 0) {
        char tools_line[1024] = "  Tools used: ";
        size_t off = strlen(tools_line);
        size_t cn = json_len(counts);
        for (size_t i = 0; i < cn && i < 5; i++) {
            json_t *pair = json_get(counts, i);
            const char *name = json_get(pair, 0)->str_val;
            long cnt = (long)json_get(pair, 1)->num_val;
            int w = snprintf(tools_line + off, sizeof(tools_line) - off, "%s×%ld", name, cnt);
            off += w;
            if (i < 4 && i < cn - 1) { tools_line[off++] = ','; tools_line[off++] = ' '; }
        }
        if (cn > 5) {
            int w = snprintf(tools_line + off, sizeof(tools_line) - off, " (+%zu more)", cn - 5);
            off += w;
        }
        lines[nlines++] = strdup(tools_line);
    }

    if (json_len(files) > 0) {
        char files_line[1024] = "  Files touched: ";
        size_t off = strlen(files_line);
        size_t fn = json_len(files);
        for (size_t i = 0; i < fn && i < 5; i++) {
            const char *f = json_get(files, i)->str_val;
            int w = snprintf(files_line + off, sizeof(files_line) - off, "%s", f);
            off += w;
            if (i < 4 && i < fn - 1) { files_line[off++] = ','; files_line[off++] = ' '; }
        }
        if (fn > 5) {
            int w = snprintf(files_line + off, sizeof(files_line) - off, " (+%zu more)", fn - 5);
            off += w;
        }
        lines[nlines++] = strdup(files_line);
    }

    char latest_user[256];
    if (session_latest_user_prompt(window, latest_user, sizeof(latest_user))) {
        char truncated[256];
        session_truncate(latest_user, 140, truncated, sizeof(truncated));
        char line[512];
        snprintf(line, sizeof(line), "  Last ask: %s", truncated);
        lines[nlines++] = strdup(line);
    }

    char latest_reply[256];
    if (session_latest_assistant_text(window, latest_reply, sizeof(latest_reply))) {
        char truncated[256];
        session_truncate(latest_reply, 200, truncated, sizeof(truncated));
        char line[512];
        snprintf(line, sizeof(line), "  Last reply: %s", truncated);
        lines[nlines++] = strdup(line);
    }

    if (nlines == 2) {
        lines[nlines++] = strdup("  (no assistant activity yet in this window)");
    }

    json_free(window);
    json_free(tool_calls);
    json_free(counts);
    json_free(files);

done:
    /* join with \n */
    size_t total = 1;
    for (int i = 0; i < nlines; i++) total += strlen(lines[i]) + 1;
    char *result = malloc(total);
    result[0] = '\0';
    for (int i = 0; i < nlines; i++) {
        if (i > 0) strcat(result, "\n");
        strcat(result, lines[i]);
        free(lines[i]);
    }
    return result;
}
