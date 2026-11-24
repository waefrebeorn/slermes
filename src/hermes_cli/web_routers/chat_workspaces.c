/*
 * chat_workspaces.c - C11 port of upstream hermes_cli/web_routers/chat_workspaces.py
 * (gate w25_chatworksp_6d19f3).
 *
 * PoP: w25_chatworksp_6d19f3_resolve_chat_cwd @ hermes_cli/web_routers/chat_workspaces.py:resolve_chat_cwd
 * PoP: w25_chatworksp_6d19f3_payload @ hermes_cli/web_routers/chat_workspaces.py:_collect_workspaces
 * PoP: w25_chatworksp_6d19f3_constants @ hermes_cli/web_routers/chat_workspaces.py:<literal tables>
 *
 * The filesystem, the process cwd, PATH.home(), projects.db, repo discovery and
 * FastAPI's HTTPException are seams.  What is ported here is the part that is a
 * pure function of values: the strip -> expanduser -> abspath -> isdir decision
 * that turns a ``?cwd=`` into a directory or a 400, and the payload keys the
 * picker answers with.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_chatworksp_6d19f3.h"

static char *cw_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s) + 1;
    out = (char *)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

/* Python truthiness of a JSON value. */
static int cw_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str.strip() -- the whitespace Python's str.strip() takes by default. */
static int cw_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *cw_strip(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *out;
    while (*b && cw_space(*b)) b++;
    e = b + strlen(b);
    while (e > b && cw_space(e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* posixpath.expanduser: only the bare "~" form resolves here; "~user" needs
 * the password database and is a seam (an unknown user is left untouched). */
static char *cw_expanduser(const char *path, const char *home) {
    const char *rest;
    size_t hl, rl;
    char *out;
    if (!path || path[0] != '~') return cw_dup(path);
    rest = strchr(path + 1, '/');
    if (rest == NULL) {
        if (path[1] != '\0') return cw_dup(path); /* "~user": passwd lookup */
        rest = path + 1;                        /* "~" */
    } else if (rest != path + 1) {
        return cw_dup(path);                    /* "~user/...": passwd lookup */
    }
    if (!home) return cw_dup(path);
    hl = strlen(home);
    rl = strlen(rest);
    out = (char *)malloc(hl + rl + 1);
    if (!out) return NULL;
    memcpy(out, home, hl);
    memcpy(out + hl, rest, rl + 1);
    return out;
}

/* posixpath.normpath */
static char *cw_normpath(const char *path) {
    char *buf, *p, *tok, *out;
    const char **parts;
    size_t n = 0, i, cap, need;
    int slashes = 0;

    if (!path || !*path) path = ".";
    buf = cw_dup(path);
    if (!buf) return NULL;
    if (buf[0] == '/') {
        slashes = 1;
        if (buf[1] == '/' && buf[2] != '/') slashes = 2;
    }
    cap = strlen(buf) / 2 + 3;
    parts = (const char **)calloc(cap, sizeof(char *));
    if (!parts) { free(buf); return NULL; }

    p = buf;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        tok = p;
        while (*p && *p != '/') p++;
        if (*p) { *p = '\0'; p++; }
        if (!strcmp(tok, ".")) continue;
        if (!strcmp(tok, "..")) {
            if (!slashes && n == 0) { parts[n++] = tok; }
            else if (n > 0 && strcmp(parts[n - 1], "..") != 0) { n--; }
            else if (n > 0) { parts[n++] = tok; }
            continue;
        }
        parts[n++] = tok;
    }

    need = (size_t)slashes + 2;
    for (i = 0; i < n; i++) need += strlen(parts[i]) + 1;
    out = (char *)malloc(need);
    if (!out) { free(parts); free(buf); return NULL; }
    p = out;
    for (i = 0; i < (size_t)slashes; i++) *p++ = '/';
    for (i = 0; i < n; i++) {
        if (i) *p++ = '/';
        memcpy(p, parts[i], strlen(parts[i]));
        p += strlen(parts[i]);
    }
    *p = '\0';
    if (out[0] == '\0') { out[0] = '.'; out[1] = '\0'; }
    free(parts);
    free(buf);
    return out;
}

/* posixpath.join */
static char *cw_join(const char *a, const char *b) {
    size_t al, bl;
    char *out;
    if (b && b[0] == '/') return cw_dup(b);
    if (!a || !a[0]) return cw_dup(b);
    al = strlen(a);
    bl = strlen(b ? b : "");
    out = (char *)malloc(al + bl + 2);
    if (!out) return NULL;
    memcpy(out, a, al);
    if (a[al - 1] == '/') { memcpy(out + al, b ? b : "", bl + 1); }
    else {
        out[al] = '/';
        memcpy(out + al + 1, b ? b : "", bl + 1);
    }
    return out;
}

json_t *w25_chatworksp_6d19f3_resolve_chat_cwd(const json_t *req) {
    const json_t *v_raw, *v_home, *v_cwd, *v_isdir;
    const char *home, *cwd;
    char *text, *expanded, *joined, *resolved;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) return json_null();
    v_raw = json_obj_get(req, "raw");
    text = cw_strip(v_raw && v_raw->type == JSON_STRING ? v_raw->str_val : "");
    if (!text) return json_null();
    if (!text[0]) { free(text); return json_null(); }

    v_home = json_obj_get(req, "home");
    home = (v_home && v_home->type == JSON_STRING) ? v_home->str_val : NULL;
    v_cwd = json_obj_get(req, "cwd");
    cwd = (v_cwd && v_cwd->type == JSON_STRING) ? v_cwd->str_val : NULL;

    expanded = cw_expanduser(text, home);
    if (expanded && expanded[0] == '/') joined = cw_dup(expanded);
    else joined = cw_join(cwd, expanded ? expanded : "");
    resolved = cw_normpath(joined ? joined : "");

    v_isdir = json_obj_get(req, "isdir");
    if (!cw_truthy(v_isdir)) {
        size_t need = strlen("Working directory does not exist: ") + strlen(text) + 1;
        char *detail = (char *)malloc(need);
        out = json_object();
        if (detail) {
            snprintf(detail, need, "Working directory does not exist: %s", text);
            json_set(out, "status", json_number((double)W25_CHATWORKSP_6D19F3_BAD_CWD_STATUS));
            json_set(out, "detail", json_string(detail));
            free(detail);
        }
        free(text); free(expanded); free(joined); free(resolved);
        return out;
    }
    out = json_string(resolved ? resolved : "");
    free(text); free(expanded); free(joined); free(resolved);
    return out;
}

json_t *w25_chatworksp_6d19f3_payload(const json_t *req) {
    const json_t *projects, *repos, *default_cwd, *home, *enabled;
    json_t *out;
    if (!req || req->type != JSON_OBJECT) return json_null();
    projects = json_obj_get(req, "projects");
    repos = json_obj_get(req, "repos");
    default_cwd = json_obj_get(req, "default_cwd");
    home = json_obj_get(req, "home");
    enabled = json_obj_get(req, "enabled");

    out = json_object();
    json_set(out, "projects",
             projects && projects->type == JSON_ARRAY ? json_copy(projects) : json_array());
    json_set(out, "repos",
             repos && repos->type == JSON_ARRAY ? json_copy(repos) : json_array());
    json_set(out, "default_cwd", default_cwd ? json_copy(default_cwd) : json_null());
    json_set(out, "home",
             home && home->type == JSON_STRING ? json_copy(home) : json_string(""));
    json_set(out, "scan_enabled", json_bool(cw_truthy(enabled) ? true : false));
    return out;
}

json_t *w25_chatworksp_6d19f3_constants(void) {
    static const char *const KEYS[] = {
        "projects", "repos", "default_cwd", "home", "scan_enabled",
    };
    json_t *o = json_object(), *keys = json_array();
    size_t i;
    for (i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) json_append(keys, json_string(KEYS[i]));
    json_set(o, "payload_keys", keys);
    json_set(o, "bad_cwd_status", json_number((double)W25_CHATWORKSP_6D19F3_BAD_CWD_STATUS));
    json_set(o, "error_template", json_string("Working directory does not exist: {text}"));
    return o;
}
