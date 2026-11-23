/*
 * terminal_tool_config.c — C port of upstream Hermes tools/terminal_tool_config.py.
 * Gate: w14_terminalto_e936fa
 *
 * PoP: w14_terminalto_e936fa_profile_container_key @ tools/terminal_tool_config.py:profile_container_key
 * PoP: w14_terminalto_e936fa_is_host_cwd @ tools/terminal_tool_config.py:_is_host_cwd
 * PoP: w14_terminalto_e936fa_is_windows_drive_path @ tools/terminal_tool_config.py:_is_windows_drive_path
 * PoP: w14_terminalto_e936fa_host_path_key @ tools/terminal_tool_config.py:_host_path_key
 * PoP: w14_terminalto_e936fa_translate_mounted_host_path @ tools/terminal_tool_config.py:translate_mounted_host_path
 * PoP: w14_terminalto_e936fa_is_unusable_container_cwd @ tools/terminal_tool_config.py:_is_unusable_container_cwd
 * PoP: w14_terminalto_e936fa_cwd_follows_host_mount @ tools/terminal_tool_config.py:cwd_follows_host_mount
 * PoP: w14_terminalto_e936fa_tenv_bool @ tools/terminal_tool_config.py:_tenv_bool
 *
 * The scope-aware _tenv() read is injected from the fixture as `value`
 * (Windows CRT setenv("") unsets; Python os.environ[name]="" keeps it).
 * _is_unusable_container_cwd is ported with mounted_host=None, the live default;
 * os.path.isabs() is mirrored as ntpath.isabs() because the oracle runs on Windows
 * (a bare leading '/' is NOT absolute there).
 */
#include "noc_w14_terminalto_e936fa.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

/* Python str() coercion for the value kinds the fixtures use. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    return dupstr("");
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : NULL;
}

static int is_windows_drive(const char *p) {
    return p && p[0] && p[1] == ':' && p[2] &&
           ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) &&
           (p[2] == '\\' || p[2] == '/');
}

static int is_host_cwd(const char *p) {
    if (!p) return 0;
    return !strncmp(p, "/Users/", 7) || !strncmp(p, "/home/", 6) || is_windows_drive(p);
}

/* Python: (path or "").replace("\\","/"); drive-letter lowercased; trailing '/' stripped
 * only when len(text) > 3 (never collapse a drive root "C:/" into "C:"). */
static char *host_path_key(const char *path) {
    const char *in = path ? path : "";
    size_t n = strlen(in);
    char *t = (char *)malloc(n + 1);
    if (!t) return dupstr("");
    for (size_t i = 0; i < n; i++) t[i] = (in[i] == '\\') ? '/' : in[i];
    t[n] = '\0';
    if (n >= 2 && t[1] == ':') t[0] = (char)tolower((unsigned char)t[0]);
    size_t m = strlen(t);
    if (m > 3) while (m > 0 && t[m - 1] == '/') t[--m] = '\0';
    return t;
}

static char *rstrip_slashes(const char *s) {
    size_t n = s ? strlen(s) : 0;
    while (n > 0 && s[n - 1] == '/') n--;
    if (n == 0) return dupstr("/");
    char *r = (char *)malloc(n + 1);
    if (r) { memcpy(r, s, n); r[n] = '\0'; }
    return r;
}

json_t *w14_terminalto_e936fa_profile_container_key(const struct json_t *profile) {
    char *p = py_str(profile);
    json_t *out;
    if (p && !strcmp(p, "default")) out = json_string("default");
    else {
        size_t n = p ? strlen(p) : 0;
        char *buf = (char *)malloc(n + 9);
        if (buf) { memcpy(buf, "profile:", 8); if (n) memcpy(buf + 8, p, n); buf[n + 8] = '\0'; }
        out = json_string(buf ? buf : "profile:");
        free(buf);
    }
    free(p);
    return out;
}

json_t *w14_terminalto_e936fa_is_host_cwd(const struct json_t *path) {
    return json_bool(is_host_cwd(jstr(path)));
}

json_t *w14_terminalto_e936fa_is_windows_drive_path(const struct json_t *path) {
    const char *p = jstr(path);
    return json_bool(p && *p && is_windows_drive(p));
}

json_t *w14_terminalto_e936fa_host_path_key(const struct json_t *path) {
    char *k = host_path_key(jstr(path));
    json_t *out = json_string(k ? k : "");
    free(k);
    return out;
}

json_t *w14_terminalto_e936fa_translate_mounted_host_path(const struct json_t *path,
                                                          const struct json_t *host_root,
                                                          const struct json_t *container_root) {
    const char *p = jstr(path), *h = jstr(host_root), *c = jstr(container_root);
    if (!p || !h || !c || !*p || !*h || !*c) return json_null();
    char *key = host_path_key(p), *root = host_path_key(h);
    if (!key || !root || !*key || !*root) { free(key); free(root); return json_null(); }
    char *mount = rstrip_slashes(c);
    json_t *out = json_null();
    if (!strcmp(key, root)) {
        out = json_string(mount ? mount : "/");
    } else {
        size_t rn = strlen(root);
        char *prefix = (char *)malloc(rn + 2);
        if (prefix) {
            memcpy(prefix, root, rn);
            if (rn && root[rn - 1] == '/') { prefix[rn] = '\0'; }
            else { prefix[rn] = '/'; prefix[rn + 1] = '\0'; }
            size_t pn = strlen(prefix);
            if (!strncmp(key, prefix, pn)) {
                const char *rest = key + pn;
                size_t mn = mount ? strlen(mount) : 0;
                size_t total = mn + 1 + strlen(rest);
                char *res = (char *)malloc(total + 1);
                if (res) {
                    if (mn) memcpy(res, mount, mn);
                    res[mn] = '/';
                    strcpy(res + mn + 1, rest);
                    out = json_string(res);
                    free(res);
                }
            }
            free(prefix);
        }
    }
    free(key); free(root); free(mount);
    return out;
}

/* ntpath.isabs() as seen by the Windows oracle: a bare leading '/' is NOT
 * absolute; only a drive-with-separator or a UNC '\\\\server' prefix is. */
static int isabs_nt(const char *p) {
    if (!p || !*p) return 0;
    if (p[0] == '\\' && p[1] == '\\') return 1;
    return is_windows_drive(p);
}

/* mounted_host is None: _is_mounted_host_cwd() is a no-op in the live default. */
static int unusable_cwd(const char *cwd) {
    if (!cwd || !*cwd) return 0;
    if (is_host_cwd(cwd)) return 1;
    return !isabs_nt(cwd);
}

json_t *w14_terminalto_e936fa_is_unusable_container_cwd(const struct json_t *cwd) {
    return json_bool(unusable_cwd(jstr(cwd)));
}

json_t *w14_terminalto_e936fa_cwd_follows_host_mount(const struct json_t *cwd,
                                                    const struct json_t *mount) {
    const char *c = jstr(cwd), *m = jstr(mount);
    if (!c) c = ""; if (!m) m = "";
    if (!*m || !*c || !strcmp(c, m)) return json_bool(false);
    if (unusable_cwd(c)) return json_bool(true);
    return json_bool(!strcmp(c, "/workspace") && strcmp(m, "/workspace"));
}

json_t *w14_terminalto_e936fa_tenv_bool(const struct json_t *env_value) {
    const char *v = jstr(env_value);
    if (!v) return json_bool(false);
    char lower[32];
    size_t n = strlen(v);
    if (n >= sizeof(lower)) n = sizeof(lower) - 1;
    for (size_t i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)v[i]);
    lower[n] = '\0';
    return json_bool(!strcmp(lower, "true") || !strcmp(lower, "1") || !strcmp(lower, "yes"));
}
