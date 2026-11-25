/*
 * files.c — C11 home for upstream Hermes hermes_cli/web_routers/files.py.
 *
 * GATES: w28_files_ee43ed_p1, w28_files_ee43ed_p2, w28_files_ee43ed_p3
 * (one port file, three differential partitions of the same pure core).
 *
 * WHAT THIS FILE OWNS
 * -------------------
 * Only the PURE decisions of the managed-files / media / fs router surface:
 * basename + component denylists, write-scope containment, filename
 * sanitisation, ``file:`` URL decoding, listing/entry shaping, size and MIME
 * derivation, pagination-free ordering, and the request-payload coercions
 * (data-URL decoding, image magic sniffing, proxy host/content-type policy).
 *
 * WHAT STAYS A SEAM (never ported here)
 * ------------------------------------
 *   * HTTP: FastAPI routers, Request/UploadFile, JSONResponse/FileResponse,
 *     status codes as exceptions -> every function below RETURNS the
 *     (status, detail) pair the route would have raised, as data.
 *   * Filesystem: stat/scandir/read/unlink/realpath/mkdir/os.replace. Fixture
 *     flags (``exists``, ``kind``, ``size``, ``mtime``, ``under_roots``)
 *     stand in for the stat result; ``real`` stands in for realpath().
 *   * Host identity: ``os.name`` / ``os.sep`` arrive as an explicit ``nt``
 *     flag (they pick the casefold and the separator the Python code branches
 *     on), so the same fixture set can be replayed on a POSIX gateway.
 *   * Environment: HERMES_HOME / HERMES_DASHBOARD_FILES_ROOT / TERMINAL_CWD
 *     are passed as strings, never read.
 *   * The system MIME database (``mimetypes.guess_type``). Only the module's
 *     own ``_FS_MIME_TYPES`` / ``_MEDIA_CONTENT_TYPES`` tables are ported; any
 *     other extension falls back to ``application/octet-stream`` exactly as
 *     ``guess_type`` does for an unknown type.
 *   * The clock, secrets.token_hex, subprocess (git), SSH workspace adapters,
 *     SQLite live-connection registry, tickets.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "hermes_json.h"

#define W28_MAX_PART 256
#define W28_MAX_PARTS 96
#define W28_BIG 4096

/* ------------------------------------------------------------------ */
/* shared helpers                                                      */
/* ------------------------------------------------------------------ */

/* PoP: w28_files_ee43ed_p1.ascii_lower @ helper (str.lower() on ASCII) */
static void ascii_lower(char *s) {
    for (; s && *s; s++) {
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
    }
}

static bool is_sep(char c, int nt) { return c == '/' || (nt && c == '\\'); }

/* Trailing + leading ASCII whitespace strip, in place. */
static void strip_ws(char *s) {
    if (!s) return;
    size_t n = strlen(s), i = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' ||
                     s[i] == '\f' || s[i] == '\v')) i++;
    size_t j = n;
    while (j > i && (s[j - 1] == ' ' || s[j - 1] == '\t' || s[j - 1] == '\n' ||
                     s[j - 1] == '\r' || s[j - 1] == '\f' || s[j - 1] == '\v')) j--;
    memmove(s, s + i, j - i);
    s[j - i] = '\0';
}

static void strip_chars(char *s, const char *set) {
    if (!s) return;
    size_t n = strlen(s), i = 0;
    while (i < n && s[i] && strchr(set, s[i])) i++;
    size_t j = n;
    while (j > i && strchr(set, s[j - 1])) j--;
    memmove(s, s + i, j - i);
    s[j - i] = '\0';
}

typedef struct {
    char part[W28_MAX_PARTS][W28_MAX_PART];
    int n;
} parts_t;

/* PoP: w28_files_ee43ed_p1.path_parts @ helper (PurePath.parts) */
static void path_parts(const char *path, int nt, parts_t *out) {
    out->n = 0;
    if (!path) return;
    const char *p = path;
    while (*p) {
        while (*p && is_sep(*p, nt)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !is_sep(*p, nt)) p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;   /* Path drops '.' */
        if (len >= W28_MAX_PART) len = W28_MAX_PART - 1;
        if (out->n >= W28_MAX_PARTS) break;
        memcpy(out->part[out->n], start, len);
        out->part[out->n][len] = '\0';
        out->n++;
    }
}

/* PoP: w28_files_ee43ed_p1.path_name @ helper (PurePath.name) */
static void path_name(const char *path, int nt, char *out, size_t cap) {
    out[0] = '\0';
    if (!path) return;
    char buf[W28_BIG];
    snprintf(buf, sizeof(buf), "%s", path);
    size_t n = strlen(buf);
    while (n > 0 && is_sep(buf[n - 1], nt)) buf[--n] = '\0';
    if (n == 0) { out[0] = '\0'; return; }
    size_t i = n;
    while (i > 0 && !is_sep(buf[i - 1], nt)) i--;
    const char *name = buf + i;
    if (!strcmp(name, ".") || !strcmp(name, "..")) { out[0] = '\0'; return; }
    snprintf(out, cap, "%s", name);
}

/* PoP: w28_files_ee43ed_p1.path_suffix @ helper (PurePath.suffix, lowercased) */
static void path_suffix_lower(const char *path, int nt, char *out, size_t cap) {
    char name[W28_MAX_PART];
    out[0] = '\0';
    path_name(path, nt, name, sizeof(name));
    size_t n = strlen(name);
    if (!n) return;
    size_t dot = (size_t)-1;
    for (size_t i = n; i > 0; i--) {
        if (name[i - 1] == '.') { dot = i - 1; break; }
    }
    if (dot == (size_t)-1) return;
    if (dot == 0 || dot == n - 1) return; /* '.hidden', 'a.' -> '' */
    snprintf(out, cap, "%s", name + dot);
    ascii_lower(out);
}

/* PoP: w28_files_ee43ed_p1.path_stem @ helper (PurePath.stem) */
static void path_stem(const char *name, char *out, size_t cap) {
    size_t n = strlen(name);
    size_t dot = (size_t)-1;
    for (size_t i = n; i > 0; i--) {
        if (name[i - 1] == '.') { dot = i - 1; break; }
    }
    if (dot != (size_t)-1 && dot != 0 && dot != n - 1) n = dot;
    if (n >= cap) n = cap - 1;
    memcpy(out, name, n);
    out[n] = '\0';
}

/* Collapse separators and '.' segments; keeps '..' (callers reject those
 * earlier). Folded when nt, because PureWindowsPath compares case-blind. */
static void norm_path(const char *in, int nt, char *out, size_t cap) {
    parts_t p;
    path_parts(in, nt, &p);
    size_t used = 0;
    out[0] = '\0';
    for (int i = 0; i < p.n; i++) {
        if (!strcmp(p.part[i], "..")) continue; /* lexical fold: prefix-only checks */
        int w = snprintf(out + used, cap > used ? cap - used : 0, "%s%s", used ? "/" : "", p.part[i]);
        if (w < 0) break;
        used += (size_t)w;
        if (used >= cap) { used = cap - 1; break; }
    }
    if (nt) ascii_lower(out);
}

static json_t *verdict_obj(bool ok, int status, const char *detail) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok));
    json_set(o, "status", json_number(status));
    if (detail) json_set(o, "detail", json_string(detail));
    else json_set(o, "detail", json_null());
    return o;
}

/* ================================================================== */
/* GATE p1 — path scoping & sanitisation                               */
/* ================================================================== */

/* The credential-store basenames the managed tree must never expose.
 * Mirrors _SENSITIVE_MANAGED_FILE_BASENAMES (module table + HOME_ENV_FILES
 * + SECRET_CACHE_STORES names + HOME_CREDENTIAL_FILES). */
static const char *const P1_SENSITIVE_BASENAMES[] = {
    "auth.json", "auth.lock", "credentials", "config.yaml", ".anthropic_oauth.json",
    "google_token.json", "google_client_secret.json", "google_oauth_pending.json",
    "google_oauth.json", "webhook_subscriptions.json", ".env", ".op.env",
    "vault.key", "vault.json.enc", "bws_cache.json", "bws_cache.enc.json",
    "op_cache.json", "photon-sidecar.json", ".netrc", "_netrc", ".pgpass",
    ".npmrc", ".pypirc", ".git-credentials",
};

/* Credential store trees as component runs (_SENSITIVE_MANAGED_DIR_PARTS). */
static const char *const P1_SENSITIVE_DIR_RUNS[][4] = {
    {"mcp-tokens", NULL}, {"pairing", NULL}, {"platforms", "pairing", NULL},
    {"whatsapp", "session", NULL}, {"platforms", "whatsapp", "session", NULL},
    {"matrix", "store", NULL}, {"platforms", "matrix", "store", NULL},
};
static const int P1_SENSITIVE_DIR_RUN_LEN[] = {1, 1, 2, 2, 3, 2, 3};

/* OS/CLI credential trees (_SENSITIVE_MANAGED_DIR_SEQUENCES, already lower). */
static const char *const P1_SENSITIVE_OS_RUNS[][3] = {
    {".ssh", NULL}, {".aws", NULL}, {".gnupg", NULL}, {".kube", NULL},
    {".docker", NULL}, {".azure", NULL}, {".config", "gh", NULL},
    {".config", "gcloud", NULL},
};
static const int P1_SENSITIVE_OS_RUN_LEN[] = {1, 1, 1, 1, 1, 1, 2, 2};

/* PoP: w28_files_ee43ed_p1.is_sensitive_filename @ hermes_cli/web_routers/files.py:_is_sensitive_filename */
bool w28_files_ee43ed_p1_is_sensitive_filename(const char *name) {
    char low[W28_MAX_PART];
    if (!name) name = "";
    snprintf(low, sizeof(low), "%s", name);
    ascii_lower(low);
    if (!strcmp(low, ".env") || !strcmp(low, ".envrc")) return true;
    if (!strncmp(low, ".env.", 5)) return true;
    for (size_t i = 0; i < sizeof(P1_SENSITIVE_BASENAMES) / sizeof(*P1_SENSITIVE_BASENAMES); i++) {
        if (!strcmp(low, P1_SENSITIVE_BASENAMES[i])) return true;
    }
    return false;
}

static bool run_matches(parts_t *p, int at, const char *const *run, int runlen) {
    if (at + runlen > p->n) return false;
    for (int k = 0; k < runlen; k++) {
        if (strcmp(p->part[at + k], run[k])) return false;
    }
    return true;
}

/* PoP: w28_files_ee43ed_p1.is_sensitive_path @ hermes_cli/web_routers/files.py:_is_sensitive_path */
bool w28_files_ee43ed_p1_is_sensitive_path(const char *path) {
    int nt = 1;
    char name[W28_MAX_PART];
    path_name(path ? path : "", nt, name, sizeof(name));
    if (w28_files_ee43ed_p1_is_sensitive_filename(name)) return true;
    parts_t p;
    path_parts(path ? path : "", nt, &p);
    for (int i = 0; i < p.n; i++) ascii_lower(p.part[i]);
    for (size_t r = 0; r < sizeof(P1_SENSITIVE_DIR_RUNS) / sizeof(*P1_SENSITIVE_DIR_RUNS); r++) {
        for (int i = 0; i < p.n; i++) {
            if (run_matches(&p, i, P1_SENSITIVE_DIR_RUNS[r], P1_SENSITIVE_DIR_RUN_LEN[r])) return true;
        }
    }
    for (size_t r = 0; r < sizeof(P1_SENSITIVE_OS_RUNS) / sizeof(*P1_SENSITIVE_OS_RUNS); r++) {
        for (int i = 0; i < p.n; i++) {
            if (run_matches(&p, i, P1_SENSITIVE_OS_RUNS[r], P1_SENSITIVE_OS_RUN_LEN[r])) return true;
        }
    }
    return false;
}

static bool deny_eq(const char *a, const char *b, int nt) {
    if (!nt) return !strcmp(a, b);
    char x[W28_BIG], y[W28_BIG];
    snprintf(x, sizeof(x), "%s", a);
    snprintf(y, sizeof(y), "%s", b);
    ascii_lower(x);
    ascii_lower(y);
    return !strcmp(x, y);
}

/* PoP: w28_files_ee43ed_p1.deny_path_under @ agent/file_safety.py:deny_path_under */
bool w28_files_ee43ed_p1_deny_path_under(const char *resolved, const char *base, int nt) {
    const char sep = nt ? '\\' : '/';
    char r[W28_BIG], b[W28_BIG];
    snprintf(r, sizeof(r), "%s", resolved ? resolved : "");
    snprintf(b, sizeof(b), "%s", base ? base : "");
    if (deny_eq(r, b, nt)) return true;
    size_t n = strlen(b);
    while (n > 0 && b[n - 1] == sep) b[--n] = '\0';
    size_t rl = strlen(r);
    if (rl <= n) {
        char prefix[W28_BIG];
        snprintf(prefix, sizeof(prefix), "%s%c", b, sep);
        size_t pl = strlen(prefix);
        if (rl < pl) return false;
        char head[W28_BIG];
        memcpy(head, r, pl);
        head[pl] = '\0';
        return deny_eq(head, prefix, nt);
    }
    return false;
}

/* PoP: w28_files_ee43ed_p1.is_local_sensitive_path @ hermes_cli/web_routers/files.py:_is_local_sensitive_path */
bool w28_files_ee43ed_p1_is_local_sensitive_path(const json_t *req) {
    const char *path = json_get_str(req, "path", "");
    const char *real = json_get_str(req, "real", "");
    int nt = json_get_bool(req, "nt", true) ? 1 : 0;
    if (w28_files_ee43ed_p1_is_sensitive_path(path)) return true;
    json_t *files = json_obj_get(req, "files");
    json_t *dirs = json_obj_get(req, "dirs");
    for (size_t i = 0; i < json_len(files); i++) {
        json_t *v = json_get(files, i);
        if (v && v->type == JSON_STRING && deny_eq(real, v->str_val, nt)) return true;
    }
    for (size_t i = 0; i < json_len(dirs); i++) {
        json_t *v = json_get(dirs, i);
        if (v && v->type == JSON_STRING && w28_files_ee43ed_p1_deny_path_under(real, v->str_val, nt)) return true;
    }
    return false;
}

/* PoP: w28_files_ee43ed_p1.sanitize_filename @ hermes_cli/web_routers/files.py:_sanitize_chat_image_filename */
void w28_files_ee43ed_p1_sanitize_filename(const char *filename, int nt, char *out, size_t cap) {
    char work[W28_BIG], name[W28_MAX_PART], cleaned[W28_MAX_PART];
    snprintf(work, sizeof(work), "%s", filename ? filename : "");
    strip_ws(work);
    path_name(work, nt, name, sizeof(name));
    /* re.sub(r"[\x00-\x1f]+", "_", candidate) */
    size_t w = 0;
    for (size_t i = 0; name[i] && w + 1 < sizeof(cleaned); ) {
        unsigned char c = (unsigned char)name[i];
        if (c < 0x20) {
            cleaned[w++] = '_';
            while (name[i] && (unsigned char)name[i] < 0x20) i++;
        } else {
            cleaned[w++] = name[i++];
        }
    }
    cleaned[w] = '\0';
    strip_ws(cleaned);
    strip_chars(cleaned, ".");
    snprintf(out, cap, "%s", cleaned[0] ? cleaned : "pasted-image");
}

/* PoP: w28_files_ee43ed_p1.filename_stem @ hermes_cli/web_routers/files.py:upload_chat_image */
void w28_files_ee43ed_p1_filename_stem(const char *filename, int nt, char *out, size_t cap) {
    char sane[W28_MAX_PART], stem[W28_MAX_PART], cleaned[W28_MAX_PART];
    w28_files_ee43ed_p1_sanitize_filename(filename, nt, sane, sizeof(sane));
    path_stem(sane, stem, sizeof(stem));
    if (!stem[0]) snprintf(stem, sizeof(stem), "pasted-image");
    /* re.sub(r"[^A-Za-z0-9_.-]+", "_", stem).strip("._-") */
    size_t w = 0;
    for (size_t i = 0; stem[i] && w + 1 < sizeof(cleaned); ) {
        char c = stem[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '.' || c == '-';
        if (ok) {
            cleaned[w++] = c;
            i++;
        } else {
            cleaned[w++] = '_';
            while (stem[i]) {
                char d = stem[i];
                bool dok = (d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9') ||
                           d == '_' || d == '.' || d == '-';
                if (dok) break;
                i++;
            }
        }
    }
    cleaned[w] = '\0';
    strip_chars(cleaned, "._-");
    snprintf(out, cap, "%s", cleaned[0] ? cleaned : "pasted-image");
}

/* PoP: w28_files_ee43ed_p1.path_text @ hermes_cli/web_server_files.py:_path_text */
json_t *w28_files_ee43ed_p1_path_text(const json_t *req) {
    char text[W28_BIG];
    json_t *raw = json_obj_get(req, "raw");
    snprintf(text, sizeof(text), "%s", (raw && raw->type == JSON_STRING) ? raw->str_val : "");
    strip_ws(text);
    /* JSON cannot carry U+0000 through a C string: the flag stands in for
     * `if "\x00" in text`, which is what the live helper tests. */
    if (json_get_bool(req, "nul", false)) return verdict_obj(false, 400, "Invalid path");
    json_t *o = verdict_obj(true, 200, NULL);
    json_set(o, "text", json_string(text));
    return o;
}

/* PoP: w28_files_ee43ed_p1.path_is_under @ hermes_cli/web_server_files.py:_path_is_under */
bool w28_files_ee43ed_p1_path_is_under(const char *root, const char *target, int nt) {
    char r[W28_BIG], t[W28_BIG];
    norm_path(root ? root : "", nt, r, sizeof(r));
    norm_path(target ? target : "", nt, t, sizeof(t));
    if (!strcmp(r, t)) return true;
    size_t rl = strlen(r), tl = strlen(t);
    if (tl <= rl) return false;
    if (strncmp(r, t, rl)) return false;
    return t[rl] == '/';
}

/* PoP: w28_files_ee43ed_p1.file_url_path @ hermes_cli/web_server_files.py:_file_url_path */
json_t *w28_files_ee43ed_p1_file_url_path(const json_t *req) {
    char url[W28_BIG], path[W28_BIG], host[W28_MAX_PART], out[W28_BIG];
    int nt = json_get_bool(req, "nt", true) ? 1 : 0;
    snprintf(url, sizeof(url), "%s", json_get_str(req, "url", ""));
    /* An EMPTY url is not "another scheme": urlparse("") has no scheme and no path,
     * and _file_url_path answers the empty path for it. Only a url that actually
     * names a scheme the helper refuses is rejected. */
    {
        /* urlsplit() strips leading C0 controls and spaces before it reads the
         * scheme, so " file:///a%2fb" is still a file: URL (and is still refused for
         * encoding a separator). Testing the raw first byte called it a foreign
         * scheme instead. */
        const char *p = url;
        while (*p && (unsigned char)*p <= 0x20) p++;
        if (*p && strncasecmp(p, "file:", 5)) return verdict_obj(false, 400, "not a file: URL");
    }
    const char *rest = url + 5;
    /* authority: urlparse reads netloc up to the next '/', '?' or '#' */
    host[0] = '\0';
    if (rest[0] == '/' && rest[1] == '/') {
        rest += 2;
        size_t hl = strcspn(rest, "/?#");
        size_t n = hl < sizeof(host) - 1 ? hl : sizeof(host) - 1;
        memcpy(host, rest, n);
        host[n] = '\0';
        rest += hl;
    }
    if (!strcasecmp(host, "localhost")) host[0] = '\0';
    /* drop query/fragment the way urlparse.path does */
    snprintf(path, sizeof(path), "%s", rest);
    path[strcspn(path, "?#")] = '\0';
    /* unquote once, for the drive and separator probes */
    char decoded[W28_BIG];
    size_t w = 0;
    for (size_t i = 0; path[i] && w + 1 < sizeof(decoded); i++) {
        if (path[i] == '%' && path[i + 1] && path[i + 2]) {
            unsigned hi = (unsigned)path[i + 1], lo = (unsigned)path[i + 2];
            int hv = (hi >= '0' && hi <= '9') ? hi - '0' : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10
                     : (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : -1;
            int lv = (lo >= '0' && lo <= '9') ? lo - '0' : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10
                     : (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : -1;
            if (hv >= 0 && lv >= 0) {
                decoded[w++] = (char)(hv * 16 + lv);
                i += 2;
                continue;
            }
        }
        decoded[w++] = path[i];
    }
    decoded[w] = '\0';
    /* a URL that encodes a separator is refused (adds a segment when decoded) */
    for (const char *q = path; *q; q++) {
        if (q[0] == '%' && (q[1] == '2' || q[1] == '3') && (q[2] == 'f' || q[2] == 'F')) {
            return verdict_obj(false, 400, "file: URL encodes a path separator");
        }
    }
    bool windows = nt || host[0] || (decoded[0] == '/' && decoded[2] == ':' &&
                                     ((decoded[1] >= 'a' && decoded[1] <= 'z') ||
                                      (decoded[1] >= 'A' && decoded[1] <= 'Z')));
    if (windows) {
        for (const char *q = path; *q; q++) {
            if (q[0] == '%' && q[1] == '5' && (q[2] == 'c' || q[2] == 'C')) {
                return verdict_obj(false, 400, "file: URL encodes a path separator");
            }
        }
    }
    if (host[0] && !nt) return verdict_obj(false, 400, "file: URL names another host");
    char joined[W28_BIG];
    if (host[0]) snprintf(joined, sizeof(joined), "//%s%s", host, path);
    else snprintf(joined, sizeof(joined), "%s", path);
    if (nt) {
        /* nturl2path.url2pathname: '///' collapses, a leading '/X:' loses the slash */
        char tmp[W28_BIG];
        snprintf(tmp, sizeof(tmp), "%s", joined);
        if (!strncmp(tmp, "///", 3)) memmove(tmp, tmp + 2, strlen(tmp + 2) + 1);
        else if (!strncmp(tmp, "//localhost/", 12)) memmove(tmp, tmp + 11, strlen(tmp + 11) + 1);
        if (!strncmp(tmp, "///", 3)) memmove(tmp, tmp + 1, strlen(tmp + 1) + 1);
        else if (tmp[0] == '/' && tmp[2] == ':') memmove(tmp, tmp + 1, strlen(tmp + 1) + 1);
        for (char *q = tmp; *q; q++) {
            if (*q == '/') *q = '\\';
        }
        snprintf(joined, sizeof(joined), "%s", tmp);
    }
    /* url2pathname unquotes once more */
    w = 0;
    for (size_t i = 0; joined[i] && w + 1 < sizeof(out); i++) {
        if (joined[i] == '%' && joined[i + 1] && joined[i + 2]) {
            unsigned hi = (unsigned)joined[i + 1], lo = (unsigned)joined[i + 2];
            int hv = (hi >= '0' && hi <= '9') ? hi - '0' : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10
                     : (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : -1;
            int lv = (lo >= '0' && lo <= '9') ? lo - '0' : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10
                     : (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : -1;
            if (hv >= 0 && lv >= 0) {
                out[w++] = (char)(hv * 16 + lv);
                i += 2;
                continue;
            }
        }
        out[w++] = joined[i];
    }
    out[w] = '\0';
    json_t *o = verdict_obj(true, 200, NULL);
    json_set(o, "path", json_string(out));
    return o;
}

/* PoP: w28_files_ee43ed_p1.agent_container_translatable @ hermes_cli/web_server_files.py:_agent_container_path */
bool w28_files_ee43ed_p1_agent_container_translatable(const char *raw) {
    if (!raw || raw[0] != '/') return false;
    if (raw[1] == '/') return false;
    const char *p = raw;
    while (*p) {
        while (*p == '/') p++;
        const char *s = p;
        while (*p && *p != '/') p++;
        if ((size_t)(p - s) == 2 && s[0] == '.' && s[1] == '.') return false;
    }
    return true;
}

/* PoP: w28_files_ee43ed_p1.under_root_text @ helper ("raw == r or raw.startswith(r + '/')") */
static bool under_root_text(const char *raw, const char *root) {
    size_t rl = strlen(root ? root : "");
    while (rl > 0 && root[rl - 1] == '/') rl--;
    if (strncmp(raw, root, rl)) return false;
    if (!raw[rl]) return true;
    return raw[rl] == '/' && rl > 0;
}

/* PoP: w28_files_ee43ed_p1.unmapped_container_path @ hermes_cli/web_server_files.py:_is_unmapped_container_path */
bool w28_files_ee43ed_p1_unmapped_container_path(const json_t *req) {
    const char *raw = json_get_str(req, "raw", "");
    bool docker = json_get_bool(req, "docker_active", false);
    bool msys_same = json_get_bool(req, "msys_same", true);
    if (raw[0] != '/' || raw[1] == '/') return false;
    if (!docker || !msys_same) return false;
    /* roots = {"/workspace", "/root", *configured volume targets} */
    static const char *const base_roots[] = {"/workspace", "/root"};
    char root[W28_BIG];
    for (size_t i = 0; i < sizeof(base_roots) / sizeof(*base_roots); i++) {
        if (under_root_text(raw, base_roots[i])) return true;
    }
    json_t *roots = json_obj_get(req, "roots");
    for (size_t i = 0; i < json_len(roots); i++) {
        json_t *v = json_get(roots, i);
        if (!v || v->type != JSON_STRING) continue;
        snprintf(root, sizeof(root), "%s", v->str_val);
        if (under_root_text(raw, root)) return true;
    }
    return false;
}

/* PoP: w28_files_ee43ed_p1.managed_path_verdict @ hermes_cli/web_server_files.py:_resolve_managed_path */
json_t *w28_files_ee43ed_p1_managed_path_verdict(const json_t *req) {
    char text[W28_BIG], cand[W28_BIG], resolved[W28_BIG], root[W28_BIG];
    bool locked = json_get_bool(req, "locked", false);
    bool for_write = json_get_bool(req, "for_write", false);
    bool exists = json_get_bool(req, "exists", true);
    int nt = json_get_bool(req, "nt", true) ? 1 : 0;
    snprintf(root, sizeof(root), "%s", json_get_str(req, "root", ""));
    json_t *raw = json_obj_get(req, "text");
    snprintf(text, sizeof(text), "%s", (raw && raw->type == JSON_STRING) ? raw->str_val : "");
    strip_ws(text);

    if (locked && (!text[0] || !strcmp(text, ".") || !strcmp(text, "/"))) {
        snprintf(cand, sizeof(cand), "%s", root);
    } else if (!text[0]) {
        snprintf(cand, sizeof(cand), "%s", json_get_str(req, "default_path", ""));
    } else {
        parts_t p;
        path_parts(text, nt, &p);
        bool absolute = text[0] == '/' || (nt && (text[0] == '\\' || (p.n && p.part[0][1] == ':')));
        bool dotdot = false;
        for (int i = 0; i < p.n; i++) {
            if (!strcmp(p.part[i], "..")) dotdot = true;
        }
        if (locked && !absolute) {
            if (dotdot) return verdict_obj(false, 400, "Path cannot contain '..'");
            snprintf(cand, sizeof(cand), "%s/%s", root, text);
        } else if (!absolute) {
            return verdict_obj(false, 400, "Path must be absolute");
        } else {
            snprintf(cand, sizeof(cand), "%s", text);
        }
    }
    parts_t cp;
    path_parts(cand, nt, &cp);
    for (int i = 0; i < cp.n; i++) {
        if (!strcmp(cp.part[i], "..")) return verdict_obj(false, 400, "Path cannot contain '..'");
    }
    if (!for_write && !exists) return verdict_obj(false, 404, "Path not found");
    norm_path(cand, nt, resolved, sizeof(resolved));
    bool under = w28_files_ee43ed_p1_path_is_under(root, resolved, nt);
    if (locked && !under) return verdict_obj(false, 403, "Path outside managed files root");
    json_t *o = verdict_obj(true, 200, NULL);
    json_set(o, "under_root", json_bool(under));
    return o;
}

/* PoP: w28_files_ee43ed_p1.if_none_match @ hermes_cli/web_routers/files.py:_if_none_match */
json_t *w28_files_ee43ed_p1_if_none_match(const char *header) {
    json_t *arr = json_array();
    char buf[W28_BIG];
    snprintf(buf, sizeof(buf), "%s", header ? header : "");
    /* str.split(",") keeps empty fields: "".split(",") == [""]. */
    char *field = buf;
    for (;;) {
        char *comma = strchr(field, ',');
        if (comma) *comma = '\0';
        char tag[W28_MAX_PART];
        snprintf(tag, sizeof(tag), "%s", field);
        strip_ws(tag);
        const char *v = tag;
        if (v[0] == 'W' && v[1] == '/') v += 2;
        bool dup = false;
        for (size_t i = 0; i < json_len(arr); i++) {
            json_t *e = json_get(arr, i);
            if (e && e->type == JSON_STRING && !strcmp(e->str_val, v)) dup = true;
        }
        if (!dup) json_append(arr, json_string(v));
        if (!comma) break;
        field = comma + 1;
    }
    /* set semantics: emit sorted so Python's sorted(set) compares directly */
    for (size_t i = 0; i < json_len(arr); i++) {
        for (size_t j = i + 1; j < json_len(arr); j++) {
            json_t *a = json_get(arr, i), *b = json_get(arr, j);
            if (strcmp(a->str_val, b->str_val) > 0) {
                arr->c.items[i] = b;
                arr->c.items[j] = a;
            }
        }
    }
    return arr;
}

/* PoP: w28_files_ee43ed_p1.if_none_match_hit @ hermes_cli/web_routers/files.py:fs_read_data_url */
bool w28_files_ee43ed_p1_if_none_match_hit(const char *header, const char *etag) {
    json_t *tags = w28_files_ee43ed_p1_if_none_match(header);
    bool hit = false;
    for (size_t i = 0; i < json_len(tags); i++) {
        json_t *e = json_get(tags, i);
        if (!e || e->type != JSON_STRING) continue;
        if (!strcmp(e->str_val, etag ? etag : "") || !strcmp(e->str_val, "*")) hit = true;
    }
    json_free(tags);
    return hit;
}

/* ================================================================== */
/* GATE p2 — listing / entry shaping / size & MIME derivation          */
/* ================================================================== */

static const struct {
    const char *ext;
    const char *mime;
} P2_MIME[] = {
    {".avi", "video/x-msvideo"}, {".bmp", "image/bmp"}, {".flac", "audio/flac"},
    {".gif", "image/gif"}, {".jpeg", "image/jpeg"}, {".jpg", "image/jpeg"},
    {".m4a", "audio/mp4"}, {".mkv", "video/x-matroska"}, {".mov", "video/quicktime"},
    {".mp3", "audio/mpeg"}, {".mp4", "video/mp4"}, {".ogg", "audio/ogg"},
    {".opus", "audio/ogg; codecs=opus"}, {".png", "image/png"}, {".svg", "image/svg+xml"},
    {".wav", "audio/wav"}, {".webm", "video/webm"}, {".webp", "image/webp"},
};

static const char *p2_mime_lookup(const char *ext) {
    for (size_t i = 0; i < sizeof(P2_MIME) / sizeof(*P2_MIME); i++) {
        if (!strcmp(ext, P2_MIME[i].ext)) return P2_MIME[i].mime;
    }
    return NULL;
}

/* PoP: w28_files_ee43ed_p2.fs_mime_type @ hermes_cli/web_routers/files.py:_fs_mime_type */
void w28_files_ee43ed_p2_fs_mime_type(const char *path, char *out, size_t cap) {
    char ext[32];
    path_suffix_lower(path ? path : "", 1, ext, sizeof(ext));
    const char *hit = ext[0] ? p2_mime_lookup(ext) : NULL;
    snprintf(out, cap, "%s", hit ? hit : "application/octet-stream");
}

static const struct {
    const char *ext;
    const char *lang;
} P2_LANG[] = {
    {".c", "c"}, {".conf", "ini"}, {".cpp", "cpp"}, {".css", "css"}, {".csv", "csv"},
    {".go", "go"}, {".graphql", "graphql"}, {".h", "c"}, {".hpp", "cpp"}, {".html", "html"},
    {".java", "java"}, {".js", "javascript"}, {".json", "json"}, {".jsx", "jsx"},
    {".kt", "kotlin"}, {".lua", "lua"}, {".md", "markdown"}, {".mjs", "javascript"},
    {".py", "python"}, {".rb", "ruby"}, {".rs", "rust"}, {".sh", "shell"}, {".sql", "sql"},
    {".svg", "xml"}, {".toml", "toml"}, {".ts", "typescript"}, {".tsx", "tsx"},
    {".txt", "text"}, {".xml", "xml"}, {".yaml", "yaml"}, {".yml", "yaml"}, {".zsh", "shell"},
};

static const char *p2_lang_find(const char *ext) {
    for (size_t i = 0; i < sizeof(P2_LANG) / sizeof(*P2_LANG); i++) {
        if (!strcmp(ext, P2_LANG[i].ext)) return P2_LANG[i].lang;
    }
    return NULL;
}

static const char *p2_lang_lookup(const char *ext) {
    const char *hit = p2_lang_find(ext);
    return hit ? hit : "text";
}

/* PoP: w28_files_ee43ed_p2.preview_governing @ hermes_cli/web_routers/files.py:_fs_preview_type_path */
json_t *w28_files_ee43ed_p2_preview_governing(const json_t *req) {
    char requested[W28_BIG], target[W28_BIG], ext[32], mime[64];
    snprintf(requested, sizeof(requested), "%s", json_get_str(req, "requested", ""));
    snprintf(target, sizeof(target), "%s", json_get_str(req, "target", ""));
    strip_ws(requested);
    bool use_requested = true;
    if (!strncasecmp(requested, "file:", 5)) {
        json_t *sub = json_object();
        json_set(sub, "url", json_string(requested));
        json_set(sub, "nt", json_bool(json_get_bool(req, "nt", true)));
        json_t *dec = w28_files_ee43ed_p1_file_url_path(sub);
        if (json_get_bool(dec, "ok", false)) {
            snprintf(requested, sizeof(requested), "%s", json_get_str(dec, "path", ""));
        } else {
            use_requested = false; /* malformed file: URL -> the resolved target */
        }
        json_free(dec);
        json_free(sub);
    }
    if (use_requested) {
        path_suffix_lower(requested, 1, ext, sizeof(ext));
        bool known = false;
        if (p2_lang_find(ext)) known = true;
        w28_files_ee43ed_p2_fs_mime_type(requested, mime, sizeof(mime));
        if (strcmp(mime, "application/octet-stream")) known = true;
        use_requested = known;
    }
    const char *governing = use_requested ? requested : target;
    path_suffix_lower(governing, 1, ext, sizeof(ext));
    w28_files_ee43ed_p2_fs_mime_type(governing, mime, sizeof(mime));
    (void)mime; /* the MIME itself is fs_mime_type's answer; the db is a seam */
    json_t *o = json_object();
    json_set(o, "source", json_string(use_requested ? "requested" : "target"));
    json_set(o, "suffix", json_string(ext));
    json_set(o, "language", json_string(p2_lang_lookup(ext)));
    return o;
}

/* PoP: w28_files_ee43ed_p2.looks_binary @ hermes_cli/web_routers/files.py:_fs_looks_binary */
bool w28_files_ee43ed_p2_looks_binary(const json_t *bytes) {
    size_t n = json_len(bytes);
    if (!n) return false;
    size_t suspicious = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(bytes, i);
        int b = v ? (int)v->num_val : 0;
        if (b == 0) return true;
        if (b < 32 && b != 9 && b != 10 && b != 13) suspicious++;
    }
    return ((double)suspicious / (double)n) > 0.12;
}

/* PoP: w28_files_ee43ed_p2.managed_response_meta @ hermes_cli/web_server_files.py:_managed_response_meta */
json_t *w28_files_ee43ed_p2_managed_response_meta(const json_t *req) {
    bool locked = json_get_bool(req, "locked", false);
    json_t *o = json_object();
    json_set(o, "root_locked", json_bool(locked));
    json_set(o, "locked_root_present", json_bool(locked));
    json_set(o, "can_change_path", json_bool(json_get_bool(req, "can_change_path", true)));
    return o;
}

/* PoP: w28_files_ee43ed_p2.managed_files_policy @ hermes_cli/web_server_files.py:_managed_files_policy */
json_t *w28_files_ee43ed_p2_managed_files_policy(const json_t *req) {
    char forced[W28_BIG], hermes_home[W28_BIG], dflt[W28_BIG], opt[W28_BIG];
    snprintf(forced, sizeof(forced), "%s", json_get_str(req, "forced_root", ""));
    strip_ws(forced);
    const char *kind = "home";
    bool locked = false, can_change = true;
    if (forced[0]) {
        kind = "forced";
        locked = true;
        can_change = false;
    } else {
        snprintf(hermes_home, sizeof(hermes_home), "%s", json_get_str(req, "hermes_home", ""));
        strip_ws(hermes_home);
        if (hermes_home[0]) {
            snprintf(dflt, sizeof(dflt), "%s", json_get_str(req, "default_root", hermes_home));
            norm_path(dflt, 1, opt, sizeof(opt));
            if (!strcmp(opt, "opt/data")) {
                kind = "opt_data";
                locked = true;
                can_change = false;
            }
        }
    }
    json_t *o = json_object();
    json_set(o, "root_kind", json_string(kind));
    json_set(o, "locked", json_bool(locked));
    json_set(o, "can_change_path", json_bool(can_change));
    return o;
}

/* PoP: w28_files_ee43ed_p2.managed_file_size @ hermes_cli/web_routers/files.py:_managed_file_size */
json_t *w28_files_ee43ed_p2_managed_file_size(const json_t *req) {
    double size = json_get_num(req, "size", 0);
    double max_bytes = json_get_num(req, "max_bytes", 0);
    if (size > max_bytes) return verdict_obj(false, 413, "File is too large");
    json_t *o = verdict_obj(true, 200, NULL);
    json_set(o, "size", json_number(size));
    return o;
}

/* PoP: w28_files_ee43ed_p2.managed_file_entry @ hermes_cli/web_server_files.py:_managed_file_entry */
json_t *w28_files_ee43ed_p2_managed_file_entry(const json_t *req) {
    const char *kind = json_get_str(req, "kind", "file");
    char name[W28_MAX_PART], mime[64];
    snprintf(name, sizeof(name), "%s", json_get_str(req, "name", ""));
    bool locked = json_get_bool(req, "locked", false);
    if (!strcmp(kind, "outside")) return verdict_obj(false, 403, "Path outside managed files root");
    if (!strcmp(kind, "missing")) return json_null(); /* skip_missing=True in a listing */
    if (!strcmp(kind, "dir")) {
        json_t *o = json_object();
        json_set(o, "name", json_string(name));
        json_set(o, "is_directory", json_bool(true));
        json_set(o, "size", json_null());
        json_set(o, "mtime", json_number(json_get_num(req, "mtime", 0)));
        json_set(o, "mime_type", json_null());
        return o;
    }
    if (!strcmp(kind, "broken")) {
        bool inside = json_get_bool(req, "inside", true);
        if (locked && !inside) return verdict_obj(false, 403, "Path outside managed files root");
        json_t *o = json_object();
        json_set(o, "name", json_string(name));
        json_set(o, "is_directory", json_bool(false));
        json_set(o, "broken_link", json_bool(true));
        json_set(o, "size", json_null());
        json_set(o, "mtime", json_null());
        json_set(o, "mime_type", json_null());
        return o;
    }
    w28_files_ee43ed_p2_fs_mime_type(name, mime, sizeof(mime));
    json_t *o = json_object();
    json_set(o, "name", json_string(name));
    json_set(o, "is_directory", json_bool(false));
    json_set(o, "size", json_number(json_get_num(req, "size", 0)));
    json_set(o, "mtime", json_number(json_get_num(req, "mtime", 0)));
    json_set(o, "mime_type", json_string(mime));
    return o;
}

static void sort_rows(json_t *arr, int key_kind) {
    /* key_kind 0: (not isDirectory, name.lower(), name) — _fs_list_entries
     * key_kind 1: (not is_directory, name.lower())      — list_managed_files
     * index tiebreak reproduces Python's stable sort. */
    size_t n = json_len(arr);
    for (size_t i = 1; i < n; i++) {
        json_t *cur = json_copy(json_get(arr, i));
        size_t j = i;
        while (j > 0) {
            json_t *prev = json_get(arr, j - 1);
            const char *pd = key_kind == 0 ? "isDirectory" : "is_directory";
            bool cdir = !json_get_bool(cur, pd, false), pdir = !json_get_bool(prev, pd, false);
            int cmp = (cdir == pdir) ? 0 : (cdir ? 1 : -1);
            if (!cmp) {
                char cn[W28_MAX_PART], pn[W28_MAX_PART];
                snprintf(cn, sizeof(cn), "%s", json_get_str(cur, "name", ""));
                snprintf(pn, sizeof(pn), "%s", json_get_str(prev, "name", ""));
                ascii_lower(cn);
                ascii_lower(pn);
                cmp = strcmp(cn, pn);
            }
            if (!cmp && key_kind == 0) {
                cmp = strcmp(json_get_str(cur, "name", ""), json_get_str(prev, "name", ""));
            }
            if (cmp >= 0) break;
            json_free(arr->c.items[j]);
            arr->c.items[j] = json_copy(prev);
            j--;
        }
        json_free(arr->c.items[j]);
        arr->c.items[j] = cur;
    }
}

/* PoP: w28_files_ee43ed_p2.fs_list_sort @ hermes_cli/web_routers/files.py:_fs_list_entries */
json_t *w28_files_ee43ed_p2_fs_list_sort(const json_t *entries) {
    json_t *arr = json_copy(entries);
    if (!arr || arr->type != JSON_ARRAY) {
        json_free(arr);
        return json_array();
    }
    sort_rows(arr, 0);
    return arr;
}

/* PoP: w28_files_ee43ed_p2.managed_list_sort @ hermes_cli/web_routers/files.py:list_managed_files */
json_t *w28_files_ee43ed_p2_managed_list_sort(const json_t *entries) {
    json_t *arr = json_copy(entries);
    if (!arr || arr->type != JSON_ARRAY) {
        json_free(arr);
        return json_array();
    }
    sort_rows(arr, 1);
    return arr;
}

/* PoP: w28_files_ee43ed_p2.list_has_parent @ hermes_cli/web_routers/files.py:list_managed_files */
bool w28_files_ee43ed_p2_list_has_parent(const char *target, const char *locked_root, int nt) {
    parts_t p;
    path_parts(target ? target : "", nt, &p);
    if (p.n == 0) return false; /* target.parent == target: a filesystem root */
    if (nt && p.n == 1 && strlen(p.part[0]) == 2 && p.part[0][1] == ':') return false; /* 'C:/' */
    if (locked_root && locked_root[0]) {
        char t[W28_BIG], r[W28_BIG];
        norm_path(target, nt, t, sizeof(t));
        norm_path(locked_root, nt, r, sizeof(r));
        if (!strcmp(t, r)) return false;
    }
    return true;
}

/* PoP: w28_files_ee43ed_p2.is_readdir_hidden @ hermes_cli/web_routers/files.py:_FS_READDIR_HIDDEN */
bool w28_files_ee43ed_p2_is_readdir_hidden(const char *name) {
    static const char *const hidden[] = {
        ".git", ".hg", ".svn", ".cache", ".next", ".turbo", ".venv", "__pycache__",
        "build", "dist", "node_modules", "target", "venv",
    };
    for (size_t i = 0; i < sizeof(hidden) / sizeof(*hidden); i++) {
        if (!strcmp(name ? name : "", hidden[i])) return true;
    }
    return false;
}

/* PoP: w28_files_ee43ed_p2.backend_error @ hermes_cli/web_routers/files.py:_raise_fs_backend_error */
json_t *w28_files_ee43ed_p2_backend_error(const char *code) {
    static const struct { const char *code; int status; const char *detail; } map[] = {
        {"EACCES", 403, "File is not readable"},
        {"ECONN", 503, "SSH workspace is unavailable"},
        {"EFBIG", 413, "File too large"},
        {"EINVAL", 400, "Invalid path"},
        {"EIO", 502, "SSH workspace request failed"},
        {"ENOENT", 404, "File not found"},
        {"ENOTREG", 400, "Only regular files can be read"},
    };
    const char *c = code ? code : "";
    for (size_t i = 0; i < sizeof(map) / sizeof(*map); i++) {
        if (!strcmp(c, map[i].code)) return verdict_obj(false, map[i].status, map[i].detail);
    }
    return verdict_obj(false, 400, c[0] ? c : "Filesystem request failed");
}

/* PoP: w28_files_ee43ed_p2.media_extension_verdict @ hermes_cli/web_routers/files.py:_managed_file_response */
json_t *w28_files_ee43ed_p2_media_extension_verdict(const json_t *req) {
    static const char *const streamable[] = {
        ".avi", ".flac", ".m4a", ".mkv", ".mov", ".mp3", ".mp4", ".ogg", ".opus", ".wav", ".webm",
    };
    bool media_only = json_get_bool(req, "media_only", false);
    char ext[32];
    snprintf(ext, sizeof(ext), "%s", json_get_str(req, "suffix", ""));
    ascii_lower(ext);
    if (!media_only) return verdict_obj(true, 200, NULL);
    for (size_t i = 0; i < sizeof(streamable) / sizeof(*streamable); i++) {
        if (!strcmp(ext, streamable[i])) return verdict_obj(true, 200, NULL);
    }
    return verdict_obj(false, 415, "Unsupported media type");
}

/* PoP: w28_files_ee43ed_p2.sandbox_not_found_detail @ hermes_cli/web_routers/files.py:_fs_sandbox_not_found_detail */
const char *w28_files_ee43ed_p2_sandbox_not_found_detail(bool unmapped) {
    return unmapped
        ? "File not found on the gateway host: the path may live inside the agent sandbox"
        : "File not found";
}

/* ================================================================== */
/* GATE p3 — query parsing, coercion, payload decoding                 */
/* ================================================================== */

static int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* base64.b64decode(s, validate=True): alphabet-only, 4-aligned, padding trailing. */
static int b64_decode_strict(const char *s, size_t n, unsigned char *out, size_t cap, size_t *outlen) {
    *outlen = 0;
    if (n % 4) return -1;
    size_t pad = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool is_pad = c == '=';
        if (is_pad) {
            pad++;
            if (i < n - 2) return -1; /* '=' may only occupy the last two slots */
            continue;
        }
        if (b64val(c) < 0) return -1;
    }
    if (pad > 2) return -1;
    if (n == 0) return 0;
    for (size_t i = 0; i < n; i += 4) {
        int v[4];
        for (int k = 0; k < 4; k++) {
            char c = s[i + k];
            v[k] = (c == '=') ? 0 : b64val(c);
        }
        unsigned int quad = ((unsigned)v[0] << 18) | ((unsigned)v[1] << 12) | ((unsigned)v[2] << 6) | (unsigned)v[3];
        size_t emit = 3;
        if (s[i + 3] == '=') emit = (s[i + 2] == '=') ? 1 : 2;
        for (size_t k = 0; k < emit; k++) {
            if (*outlen < cap) out[*outlen] = (unsigned char)((quad >> (16 - 8 * k)) & 0xff);
            (*outlen)++;
        }
    }
    return 0;
}

static json_t *bytes_to_json(const unsigned char *data, size_t n) {
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) json_append(arr, json_number((double)data[i]));
    return arr;
}

/* PoP: w28_files_ee43ed_p3.decode_data_url @ hermes_cli/web_routers/files.py:_decode_data_url */
json_t *w28_files_ee43ed_p3_decode_data_url(const json_t *req) {
    char text[W28_BIG], header[W28_BIG], mime[W28_MAX_PART];
    const char *data_url = json_get_str(req, "data_url", "");
    double max_bytes = json_get_num(req, "max_bytes", 0);
    snprintf(text, sizeof(text), "%s", data_url ? data_url : "");
    strip_ws(text);
    if (strncmp(text, "data:", 5) || !strchr(text, ',')) {
        return verdict_obj(false, 400, "Upload payload must be a data URL");
    }
    char *comma = strchr(text, ',');
    size_t hl = (size_t)(comma - text);
    if (hl >= sizeof(header)) hl = sizeof(header) - 1;
    memcpy(header, text, hl);
    header[hl] = '\0';
    const char *encoded = comma + 1;
    /* mime_type = header[5:].split(";", 1)[0] or "application/octet-stream" */
    snprintf(mime, sizeof(mime), "%s", header + 5);
    mime[strcspn(mime, ";")] = '\0';
    if (!mime[0]) snprintf(mime, sizeof(mime), "application/octet-stream");
    if (!strstr(header, ";base64")) return verdict_obj(false, 400, "Upload payload must be base64 encoded");
    unsigned char buf[W28_BIG * 4];
    size_t outlen = 0;
    if (b64_decode_strict(encoded, strlen(encoded), buf, sizeof(buf), &outlen) != 0 || outlen > sizeof(buf)) {
        return verdict_obj(false, 400, "Upload payload is not valid base64");
    }
    if ((double)outlen > max_bytes) return verdict_obj(false, 413, "File is too large");
    json_t *o = verdict_obj(true, 200, NULL);
    json_set(o, "mime_type", json_string(mime));
    json_set(o, "data", bytes_to_json(buf, outlen));
    return o;
}

/* PoP: w28_files_ee43ed_p3.chat_image_extension @ hermes_cli/web_routers/files.py:_chat_image_extension */
json_t *w28_files_ee43ed_p3_chat_image_extension(const json_t *bytes) {
    unsigned char head[16];
    size_t n = json_len(bytes);
    if (n > 16) n = 16;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(bytes, i);
        head[i] = (unsigned char)(v ? (int)v->num_val : 0);
    }
    if (n >= 12 && !memcmp(head, "RIFF", 4) && !memcmp(head + 8, "WEBP", 4)) return json_string(".webp");
    static const struct { const char *sig; size_t len; const char *ext; } magic[] = {
        {"\x89PNG\r\n\x1a\n", 8, ".png"}, {"\xff\xd8\xff", 3, ".jpg"},
        {"GIF87a", 6, ".gif"}, {"GIF89a", 6, ".gif"}, {"BM", 2, ".bmp"},
    };
    for (size_t i = 0; i < sizeof(magic) / sizeof(*magic); i++) {
        if (n >= magic[i].len && !memcmp(head, magic[i].sig, magic[i].len)) return json_string(magic[i].ext);
    }
    return json_null();
}

/* PoP: w28_files_ee43ed_p3.decode_chat_image_upload @ hermes_cli/web_routers/files.py:_decode_chat_image_upload */
json_t *w28_files_ee43ed_p3_decode_chat_image_upload(const json_t *req) {
    static const char *const allowed[] = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp"};
    json_t *dec = w28_files_ee43ed_p3_decode_data_url(req);
    if (!json_get_bool(dec, "ok", false)) return dec;
    char low[W28_MAX_PART], detail[128];
    snprintf(low, sizeof(low), "%s", json_get_str(dec, "mime_type", ""));
    ascii_lower(low);
    if (strncmp(low, "image/", 6)) {
        json_free(dec);
        return verdict_obj(false, 400, "Upload payload must be an image");
    }
    double cap = json_get_num(req, "cap", 0);
    json_t *data = json_obj_get(dec, "data");
    if ((double)json_len(data) > cap) {
        json_free(dec);
        snprintf(detail, sizeof(detail), "Image is too large; cap is %d MB", (int)((long long)cap / (1024 * 1024)));
        return verdict_obj(false, 413, detail);
    }
    json_t *ext = w28_files_ee43ed_p3_chat_image_extension(data);
    const char *e = (ext && ext->type == JSON_STRING) ? ext->str_val : "";
    bool ok = false;
    for (size_t i = 0; i < sizeof(allowed) / sizeof(*allowed); i++) {
        if (e[0] && !strcmp(e, allowed[i])) ok = true;
    }
    if (!ok) {
        json_free(ext);
        json_free(dec);
        return verdict_obj(false, 400, "Unsupported image type");
    }
    json_set(dec, "ext", json_copy(ext));
    json_free(ext);
    return dec;
}

/* PoP: w28_files_ee43ed_p3.media_proxy_host_allowed @ hermes_cli/web_routers/files.py:_media_proxy_host_allowed */
bool w28_files_ee43ed_p3_media_proxy_host_allowed(const char *host) {
    static const char *const allowed[] = {
        "fal.media", "fal.run", "v3.fal.media", "storage.googleapis.com", "*.fal.media", "*.fal.run",
    };
    char h[W28_MAX_PART];
    snprintf(h, sizeof(h), "%s", host ? host : "");
    ascii_lower(h);
    size_t n = strlen(h);
    while (n > 0 && h[n - 1] == '.') h[--n] = '\0';
    for (size_t i = 0; i < sizeof(allowed) / sizeof(*allowed); i++) {
        const char *a = allowed[i];
        if (a[0] == '*' && a[1] == '.') {
            size_t al = strlen(a + 1); /* ".fal.media" */
            size_t hl = strlen(h);
            if (hl >= al && !strcmp(h + hl - al, a + 1)) return true;
        } else if (!strcmp(h, a)) {
            return true;
        }
    }
    return false;
}

/* PoP: w28_files_ee43ed_p3.media_proxy_target @ hermes_cli/web_routers/files.py:proxy_remote_media */
json_t *w28_files_ee43ed_p3_media_proxy_target(const json_t *req) {
    char url[W28_BIG], host[W28_MAX_PART], path[W28_BIG], scheme[16];
    snprintf(url, sizeof(url), "%s", json_get_str(req, "url", ""));
    strip_ws(url);
    scheme[0] = '\0';
    host[0] = '\0';
    path[0] = '\0';
    const char *colon = strstr(url, ":");
    if (colon && colon != url) {
        size_t sl = (size_t)(colon - url);
        if (sl < sizeof(scheme)) {
            memcpy(scheme, url, sl);
            scheme[sl] = '\0';
            ascii_lower(scheme);
            if (colon[1] == '/' && colon[2] == '/') {
                const char *auth = colon + 3;
                size_t al = strcspn(auth, "/?#");
                char authority[W28_MAX_PART];
                size_t an = al < sizeof(authority) - 1 ? al : sizeof(authority) - 1;
                memcpy(authority, auth, an);
                authority[an] = '\0';
                char *at = strrchr(authority, '@');
                char *hstart = at ? at + 1 : authority;
                snprintf(host, sizeof(host), "%s", hstart);
                if (host[0] == '[') {
                    char *close = strchr(host, ']');
                    if (close) close[1] = '\0';
                } else {
                    char *port = strrchr(host, ':');
                    if (port) *port = '\0';
                }
                ascii_lower(host);
                snprintf(path, sizeof(path), "%s", auth + al);
                path[strcspn(path, "?#")] = '\0';
            }
        }
    }
    if (strcmp(scheme, "http") && strcmp(scheme, "https")) {
        return verdict_obj(false, 400, "A remote image URL is required");
    }
    if (!host[0]) return verdict_obj(false, 400, "A remote image URL is required");
    if (!w28_files_ee43ed_p3_media_proxy_host_allowed(host)) return verdict_obj(false, 403, "Image host not allowed");
    char ext[32];
    path_suffix_lower(path, 0, ext, sizeof(ext));
    static const char *const media_ext[] = {
        ".png", ".jpg", ".jpeg", ".gif", ".webp", ".svg", ".bmp", ".ico",
    };
    bool allowed_ext = false;
    for (size_t i = 0; i < sizeof(media_ext) / sizeof(*media_ext); i++) {
        if (ext[0] && !strcmp(ext, media_ext[i])) allowed_ext = true;
    }
    if (!path[0] || !allowed_ext) {
        /* A content-hash CDN path has no extension: only an explicit non-image
         * extension is refused. */
        if (ext[0] && !allowed_ext) return verdict_obj(false, 415, "Unsupported media type");
    }
    json_t *o = verdict_obj(true, 200, NULL);
    json_set(o, "host", json_string(host));
    json_set(o, "suffix", json_string(ext));
    return o;
}

/* PoP: w28_files_ee43ed_p3.media_proxy_content_type_ok @ hermes_cli/web_routers/files.py:proxy_remote_media */
bool w28_files_ee43ed_p3_media_proxy_content_type_ok(const char *content_type) {
    static const char *const values[] = {
        "image/png", "image/jpeg", "image/gif", "image/webp",
        "image/svg+xml", "image/bmp", "image/x-icon",
    };
    char ct[W28_MAX_PART];
    snprintf(ct, sizeof(ct), "%s", content_type ? content_type : "");
    ct[strcspn(ct, ";")] = '\0';
    strip_ws(ct);
    ascii_lower(ct);
    for (size_t i = 0; i < sizeof(values) / sizeof(*values); i++) {
        if (!strcmp(ct, values[i])) return true;
    }
    return false;
}

/* PoP: w28_files_ee43ed_p3.media_proxy_redirect @ hermes_cli/web_routers/files.py:_media_proxy_get */
json_t *w28_files_ee43ed_p3_media_proxy_redirect(const json_t *req) {
    char url[W28_BIG], cur[W28_BIG];
    snprintf(url, sizeof(url), "%s", json_get_str(req, "url", ""));
    snprintf(cur, sizeof(cur), "%s", url);
    json_t *chain = json_obj_get(req, "chain");
    int hops = 0;
    for (size_t i = 0; i < json_len(chain) && hops < 3; i++) {
        json_t *hop = json_get(chain, i);
        int status = hop ? (int)json_get_num(hop, "status", 200) : 200;
        const char *loc = hop ? json_get_str(hop, "location", "") : "";
        bool redirect = status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
        if (!redirect || !loc[0]) break;
        char next[W28_BIG];
        if (!strncmp(loc, "http://", 7) || !strncmp(loc, "https://", 8)) {
            snprintf(next, sizeof(next), "%s", loc);
        } else if (loc[0] == '/') {
            char origin[W28_BIG];
            snprintf(origin, sizeof(origin), "%s", cur);
            char *sep = strstr(origin, "://");
            sep = sep ? strchr(sep + 3, '/') : NULL;
            if (sep) *sep = '\0';
            snprintf(next, sizeof(next), "%s%s", origin, loc);
        } else {
            char base[W28_BIG];
            snprintf(base, sizeof(base), "%s", cur);
            char *slash = strrchr(base, '/');
            if (slash && slash > strstr(base, "://") + 2) *slash = '\0';
            snprintf(next, sizeof(next), "%s/%s", base, loc);
        }
        snprintf(cur, sizeof(cur), "%s", next);
        hops++;
        json_t *probe = json_object();
        json_set(probe, "url", json_string(cur));
        json_t *parsed = w28_files_ee43ed_p3_media_proxy_target(probe);
        bool okhost = false;
        char host[W28_MAX_PART];
        /* scheme + host of the hop, without the extension rules */
        char scheme[16];
        scheme[0] = '\0';
        host[0] = '\0';
        const char *colon = strstr(cur, ":");
        if (colon) {
            size_t sl = (size_t)(colon - cur);
            if (sl < sizeof(scheme) && colon[1] == '/' && colon[2] == '/') {
                memcpy(scheme, cur, sl);
                scheme[sl] = '\0';
                ascii_lower(scheme);
                const char *auth = colon + 3;
                size_t al = strcspn(auth, "/?#");
                size_t an = al < sizeof(host) - 1 ? al : sizeof(host) - 1;
                memcpy(host, auth, an);
                host[an] = '\0';
                char *at = strrchr(host, '@');
                if (at) memmove(host, at + 1, strlen(at + 1) + 1);
                char *port = strrchr(host, ':');
                if (port && host[0] != '[') *port = '\0';
                ascii_lower(host);
                okhost = w28_files_ee43ed_p3_media_proxy_host_allowed(host);
            }
        }
        json_free(parsed);
        json_free(probe);
        if ((strcmp(scheme, "http") && strcmp(scheme, "https")) || !okhost) {
            json_t *o = verdict_obj(false, 403, "Image redirect host not allowed");
            json_set(o, "hops", json_number(hops));
            return o;
        }
    }
    json_t *o = verdict_obj(true, 200, NULL);
    json_set(o, "hops", json_number(hops));
    return o;
}

/* PoP: w28_files_ee43ed_p3.proxy_response_verdict @ hermes_cli/web_routers/files.py:proxy_remote_media */
json_t *w28_files_ee43ed_p3_proxy_response_verdict(const json_t *req) {
    int status = (int)json_get_num(req, "status", 200);
    double size = json_get_num(req, "size", 0);
    char detail[128];
    if (status != 200) {
        snprintf(detail, sizeof(detail), "Image fetch returned HTTP %d", status);
        return verdict_obj(false, 502, detail);
    }
    if (!w28_files_ee43ed_p3_media_proxy_content_type_ok(json_get_str(req, "content_type", ""))) {
        return verdict_obj(false, 415, "Unsupported media type");
    }
    if (size > json_get_num(req, "max_bytes", 0)) return verdict_obj(false, 413, "File too large");
    return verdict_obj(true, 200, NULL);
}

/* PoP: w28_files_ee43ed_p3.profile_key @ hermes_cli/web_routers/files.py:_fs_backend */
void w28_files_ee43ed_p3_profile_key(const char *profile, char *out, size_t cap) {
    char buf[W28_MAX_PART];
    snprintf(buf, sizeof(buf), "%s", profile ? profile : "current");
    strip_ws(buf);
    ascii_lower(buf);
    snprintf(out, cap, "%s", buf[0] ? buf : "current");
}

/* PoP: w28_files_ee43ed_p3.default_cwd_placeholder @ hermes_cli/web_routers/files.py:_fs_default_cwd */
bool w28_files_ee43ed_p3_default_cwd_placeholder(const char *raw) {
    char buf[W28_BIG];
    snprintf(buf, sizeof(buf), "%s", raw ? raw : "");
    strip_ws(buf);
    return !buf[0] || !strcmp(buf, ".") || !strcmp(buf, "auto") || !strcmp(buf, "cwd");
}

/* PoP: w28_files_ee43ed_p3.read_text_meta @ hermes_cli/web_routers/files.py:fs_read_text */
json_t *w28_files_ee43ed_p3_read_text_meta(const json_t *req) {
    char name[W28_BIG], ext[32], mime[64];
    double size = json_get_num(req, "size", 0);
    double preview = json_get_num(req, "preview_max", 0);
    snprintf(name, sizeof(name), "%s", json_get_str(req, "name", ""));
    json_t *head = json_obj_get(req, "data");
    json_t *truncated_head = json_array();
    size_t limit = json_len(head) > 4096 ? 4096 : json_len(head);
    for (size_t i = 0; i < limit; i++) json_append(truncated_head, json_copy(json_get(head, i)));
    bool binary = w28_files_ee43ed_p2_looks_binary(truncated_head);
    json_free(truncated_head);
    path_suffix_lower(name, 1, ext, sizeof(ext));
    w28_files_ee43ed_p2_fs_mime_type(name, mime, sizeof(mime));
    json_t *o = json_object();
    json_set(o, "binary", json_bool(binary));
    json_set(o, "byteSize", json_number(size));
    json_set(o, "truncated", json_bool(size > preview));
    json_set(o, "language", json_string(p2_lang_lookup(ext)));
    json_set(o, "mimeType", json_string(mime));
    return o;
}

/* PoP: w28_files_ee43ed_p3.media_suffix_verdict @ hermes_cli/web_routers/files.py:get_media */
json_t *w28_files_ee43ed_p3_media_suffix_verdict(const json_t *req) {
    static const char *const types[] = {
        ".png", ".jpg", ".jpeg", ".gif", ".webp", ".svg", ".bmp", ".ico",
    };
    static const char *const values[] = {
        "image/png", "image/jpeg", "image/jpeg", "image/gif", "image/webp",
        "image/svg+xml", "image/bmp", "image/x-icon",
    };
    char ext[32];
    snprintf(ext, sizeof(ext), "%s", json_get_str(req, "suffix", ""));
    ascii_lower(ext);
    const char *ct = NULL;
    for (size_t i = 0; i < sizeof(types) / sizeof(*types); i++) {
        if (ext[0] && !strcmp(ext, types[i])) ct = values[i];
    }
    if (!ct) return verdict_obj(false, 415, "Unsupported media type");
    if (!json_get_bool(req, "under_roots", true)) return verdict_obj(false, 403, "Path outside media roots");
    if (!json_get_bool(req, "is_file", true)) return verdict_obj(false, 404, "File not found");
    if (json_get_num(req, "size", 0) > json_get_num(req, "max_bytes", 0)) {
        return verdict_obj(false, 413, "File too large");
    }
    json_t *o = verdict_obj(true, 200, NULL);
    json_set(o, "content_type", json_string(ct));
    return o;
}
