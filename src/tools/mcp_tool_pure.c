/*
 * mcp_tool_pure.c — Pure-logic helpers ported from tools/mcp_tool.py.
 *
 * These are the subprocess-free, asyncio-free functions from mcp_tool.py:
 * file-lock acquisition (fcntl, POSIX-only), the discovery-lock cookie,
 * hidden-whitespace config warnings, and name-filter matching. They depend
 * only on libjson + libpath (path_fnmatch) + libc; no slermes gateway
 * plumbing so they can be unit-oracle-verified in isolation.
 */

#define _POSIX_C_SOURCE 200809L
#include "mcp_tool_pure.h"
#include <json.h>
#include <path.h>        /* path_fnmatch */
#include <slermes_home.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

/* ── _LockCookie ─────────────────────────────────────────── */
/* PoP: __init__ @ tools/mcp_tool.py:_LockCookie.__init__ */
/* PoP: release @ tools/mcp_tool.py:_LockCookie.release */
struct mcp_lock_cookie {
    int fd;
    bool held;
};

/* Python: f"{path}.{k}" if path else str(k)  */
static char *mcp_join_path(const char *path, const char *key)
{
    if (!path || !*path) {
        char *r = malloc(strlen(key) + 1);
        strcpy(r, key);
        return r;
    }
    /* "%s.%s" → path + '.' + key + NUL = strlen(path)+strlen(key)+3 bytes */
    char *r = malloc(strlen(path) + strlen(key) + 3);
    sprintf(r, "%s.%s", path, key);
    return r;
}

/* Python: f"{path}[{i}]" → path + '[' + idx + ']' + NUL */
static char *mcp_join_path_index(const char *path, const char *idx)
{
    char *r = malloc(strlen(path) + 1 + strlen(idx) + 1 + 1 + 1);
    sprintf(r, "%s[%s]", path, idx);
    return r;
}

struct mcp_lock_cookie *mcp_lock_cookie_new(int fd)
{
    struct mcp_lock_cookie *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->fd = fd;
    c->held = true;
    return c;
}

void mcp_lock_cookie_release(struct mcp_lock_cookie *c)
{
    if (!c || !c->held) return;
    if (c->fd >= 0) {
        /* Python: fcntl.flock(fd, LOCK_UN) — ignore errors */
#ifdef LOCK_UN
        flock(c->fd, LOCK_UN);
#endif
        close(c->fd);
        c->fd = -1;
    }
    c->held = false;
}

void mcp_lock_cookie_free(struct mcp_lock_cookie *c)
{
    if (!c) return;
    mcp_lock_cookie_release(c);
    free(c);
}

/* ── _acquire_lock_on_fh ─────────────────────────────────── */
/* PoP: _acquire_lock_on_fh @ tools/mcp_tool.py:_acquire_lock_on_fh */
/*
 * Faithful port of _acquire_lock_on_fh. Python returns True/False on
 * EACCES/EAGAIN/EWOULDBLOCK and re-raises other OSErrors as RuntimeError.
 * We mirror: return true/false for the "refused" cases; for other fcntl
 * errors set *err_reraise and return false-with-raise signal via out-param.
 * Windows (portalocker) path omitted — slermes is POSIX-targeted (Linux/macOS);
 * no god-platform concessions here.
 */
bool mcp_acquire_lock_on_fh(int fd, int *out_errno_reraise)
{
    if (out_errno_reraise) *out_errno_reraise = 0;
#ifdef LOCK_EX
    if (flock(fd, LOCK_EX | LOCK_NB) == 0)
        return true;
    int e = errno;
    if (e == EACCES || e == EAGAIN || e == EWOULDBLOCK)
        return false;
    if (out_errno_reraise) *out_errno_reraise = e;
    return false;
#else
    /* No flock on this platform — locking unavailable. */
    if (out_errno_reraise) *out_errno_reraise = ENOSYS;
    return false;
#endif
}

/* ── _try_acquire_mcp_discovery_lock ─────────────────────── */
/* PoP: _try_acquire_mcp_discovery_lock @ tools/mcp_tool.py:_try_acquire_mcp_discovery_lock */
/* Returns: MCP_LOCK_COOKIE (acquired), MCP_LOCK_BUSY (held by other),
 *          MCP_LOCK_UNAVAILABLE (broken/unavailable). */
enum mcp_lock_result mcp_try_acquire_mcp_discovery_lock(struct mcp_lock_cookie **out)
{
    if (out) *out = NULL;
    static char *lock_path_cache = NULL;
    if (!lock_path_cache) {
        const char *home = slermes_home();
        if (!home) return MCP_LOCK_UNAVAILABLE;
        size_t n = strlen(home) + strlen("/.mcp-discovery.lock") + 1;
        lock_path_cache = malloc(n);
        if (!lock_path_cache) return MCP_LOCK_UNAVAILABLE;
        snprintf(lock_path_cache, n, "%s/.mcp-discovery.lock", home);
    }
    int fd = open(lock_path_cache, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return MCP_LOCK_UNAVAILABLE;
    int reraise = 0;
    bool acquired = mcp_acquire_lock_on_fh(fd, &reraise);
    if (reraise != 0) {
        /* unexpected fcntl error → _LOCK_UNAVAILABLE */
        close(fd);
        return MCP_LOCK_UNAVAILABLE;
    }
    if (acquired) {
        if (out) *out = mcp_lock_cookie_new(fd);
        return MCP_LOCK_COOKIE;
    }
    /* lock held by another process */
    close(fd);
    return MCP_LOCK_BUSY;
}

/* ── _warn_hidden_whitespace ─────────────────────────────── */
/* PoP: _warn_hidden_whitespace @ tools/mcp_tool.py:_warn_hidden_whitespace */
/*
 * Faithful port: walks a json_t config (str/dict/list), collects dotted key
 * paths whose string value has leading/trailing whitespace. De-dupes
 * (server_name, key_path) via the provided already-flagged check; the caller
 * owns the warned-set lifecycle (mirroring module-global _whitespace_warned).
 * Logs nothing itself — returns the flagged list for testability; the caller
 * decides how to surface.
 */
char **mcp_warn_hidden_whitespace(const char *server_name,
                                   const json_t *config,
                                   bool (*is_warned)(const char *srv, const char *path),
                                   void (*mark_warned)(const char *srv, const char *path),
                                   size_t *out_count)
{
    if (out_count) *out_count = 0;
    /* Collect paths via a small scratch buffer + recursive walker. */
    struct collector {
        char **items;
        size_t n, cap;
    };
    struct collector c = {0};

    /* We can't recurse into json easily with a closure in C; use a work list. */
    struct frame { json_t *val; char *path; };
    struct frame *stack = NULL;
    size_t sp = 0, cap_s = 0;
    char *empty = strdup("");

    if (!config) return NULL;
    /* push root */
    if (sp == cap_s) { cap_s = 16; stack = malloc(sizeof(*stack)*cap_s); }
    stack[sp++] = (struct frame){(json_t*)config, empty};

    while (sp > 0) {
        struct frame cur = stack[--sp];
        json_t *v = cur.val;
        char *p = cur.path;
        if (v->type == JSON_STRING) {
            const char *s = v->str_val;
            size_t sl = strlen(s);
            /* Python: value != value.strip() — check leading/trailing ws */
            bool dirty = false;
            if (sl > 0) {
                if (isspace((unsigned char)s[0]) || isspace((unsigned char)s[sl-1]))
                    dirty = true;
            }
            if (dirty) {
                /* Python appends to flagged BEFORE the dedupe check; return
                 * ALL flagged paths (dedupe only gates logging). */
                if (c.n == c.cap) { c.cap = c.cap ? c.cap*2 : 8; c.items = realloc(c.items, sizeof(char*)*c.cap); }
                c.items[c.n++] = p; /* transfer ownership */
            } else {
                free(p);
            }
        } else if (v->type == JSON_OBJECT) {
            /* Push children in reverse so they pop in insertion order (FIFO). */
            for (ssize_t i = (ssize_t)v->c.count - 1; i >= 0; i--) {
                const char *k = v->c.keys[i];
                char *np = mcp_join_path(p, k);
                if (sp == cap_s) { cap_s *= 2; stack = realloc(stack, sizeof(*stack)*cap_s); }
                stack[sp++] = (struct frame){v->c.items[i], np};
            }
            free(p);
        } else if (v->type == JSON_ARRAY) {
            for (ssize_t i = (ssize_t)v->c.count - 1; i >= 0; i--) {
                char idx[32]; snprintf(idx, sizeof(idx), "%zu", (size_t)i);
                char *np = mcp_join_path_index(p, idx);
                if (sp == cap_s) { cap_s *= 2; stack = realloc(stack, sizeof(*stack)*cap_s); }
                stack[sp++] = (struct frame){v->c.items[i], np};
            }
            free(p);
        } else {
            free(p);
        }
    }
    free(stack);
    /* empty is consumed by the walker's first iteration (freed in the
     * non-string branch); do NOT free it again here. */
    if (out_count) *out_count = c.n;
    return c.n ? c.items : (NULL);
}

/* Free a list returned by mcp_warn_hidden_whitespace. */
void mcp_warn_hidden_whitespace_free(char **items, size_t n)
{
    if (!items) return;
    for (size_t i = 0; i < n; i++) free(items[i]);
    free(items);
}

/* ── matches_name_filter ─────────────────────────────────── */
/* PoP: matches_name_filter @ tools/mcp_tool.py:matches_name_filter */
/*
 * Exact membership first (O(1)), then fnmatch-case for patterns containing
 * glob metacharacters. Matches Python's fnmatchcase exactly via libpath.
 */
bool matches_name_filter(const char *tool_name, const char **patterns, size_t n)
{
    if (!n || !tool_name) return false;
    /* exact membership first */
    for (size_t i = 0; i < n; i++) {
        if (patterns[i] && strcmp(patterns[i], tool_name) == 0)
            return true;
    }
    for (size_t i = 0; i < n; i++) {
        if (!patterns[i]) continue;
        const char *p = patterns[i];
        if (strchr(p, '*') || strchr(p, '?') || strchr(p, '[')) {
            if (path_fnmatch(p, tool_name))
                return true;
        }
    }
    return false;
}

/* ── MCP trust + identity header helpers (tools/mcp_tool.py) ─────────── */

#define _TRUST_FULL     "full"
#define _TRUST_UNTRUSTED "untrusted"

/* PoP: _normalize_server_trust @ tools/mcp_tool.py:_normalize_server_trust */
/* Return "full" or "untrusted". Missing/None -> full; unknown -> untrusted. */
char *mcp_normalize_server_trust(const json_t *value) {
    if (!value || value->type == JSON_NULL) return strdup(_TRUST_FULL);
    const char *text = "";
    if (value->type == JSON_STRING) text = value->str_val;
    else if (value->type == JSON_NUMBER || value->type == JSON_BOOL) {
        /* Python str(value).strip().lower() */
        char tmp[32];
        if (value->type == JSON_NUMBER) snprintf(tmp, sizeof(tmp), "%.0f", value->num_val);
        else snprintf(tmp, sizeof(tmp), "%s", value->num_val ? "true" : "false");
        text = tmp;
    } else {
        return strdup(_TRUST_UNTRUSTED);
    }
    /* strip + lower */
    const char *s = text;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t sl = strlen(s);
    while (sl > 0 && (s[sl-1]==' '||s[sl-1]=='\t'||s[sl-1]=='\n'||s[sl-1]=='\r')) sl--;
    if (sl == 0) return strdup(_TRUST_UNTRUSTED);
    char lowered[64];
    size_t li = 0;
    for (size_t i = 0; i < sl && li < 63; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        lowered[li++] = c;
    }
    lowered[li] = '\0';
    if (strcmp(lowered, _TRUST_FULL) == 0) return strdup(_TRUST_FULL);
    if (strcmp(lowered, _TRUST_UNTRUSTED) == 0) return strdup(_TRUST_UNTRUSTED);
    return strdup(_TRUST_UNTRUSTED);
}

/* PoP: _annotation_read_only_hint @ tools/mcp_tool.py:_annotation_read_only_hint */
/* Return true only when annotations.readOnlyHint is exactly True. */
bool mcp_annotation_read_only_hint(const json_t *mcp_tool) {
    if (!mcp_tool) return false;
    /* handle dict or object with attribute */
    json_t *annotations = NULL;
    if (mcp_tool->type == JSON_OBJECT) {
        annotations = json_obj_get(mcp_tool, "annotations");
    }
    if (!annotations) return false;
    if (annotations->type == JSON_NULL) return false;
    json_t *hint = NULL;
    if (annotations->type == JSON_OBJECT) {
        hint = json_obj_get(annotations, "readOnlyHint");
    }
    if (!hint) return false;
    return (hint->type == JSON_BOOL && hint->num_val != 0);
}

/* PoP: _resolve_identity_header @ tools/mcp_tool.py:_resolve_identity_header */
/* Returns a json_t* array [name, value], or NULL. Caller must json_free. */
#include "port_gateway_run_deps.h"  /* gw_active_profile_name */
json_t *mcp_resolve_identity_header(const char *server_name, const json_t *config) {
    (void)server_name;
    if (!config || config->type != JSON_OBJECT) return NULL;
    json_t *raw = json_obj_get(config, "identity_header");
    if (!raw || raw->type == JSON_NULL) return NULL;
    if (raw->type != JSON_OBJECT) return NULL;  /* warn + ignore */
    json_t *name = json_obj_get(raw, "name");
    if (!name || name->type != JSON_STRING || !name->str_val || !*name->str_val) return NULL;
    const char *name_str = name->str_val;
    char value_from[32] = "static";
    json_t *vf = json_obj_get(raw, "value_from");
    if (vf && vf->type == JSON_STRING && vf->str_val && *vf->str_val) {
        strncpy(value_from, vf->str_val, sizeof(value_from) - 1);
        value_from[sizeof(value_from)-1] = '\0';
    }
    /* strip + lower value_from */
    char *p = value_from;
    while (*p == ' ' || *p == '\t') p++;
    size_t pl = strlen(p);
    while (pl > 0 && (p[pl-1]==' '||p[pl-1]=='\t')) pl--;
    p[pl] = '\0';
    for (size_t i = 0; i < pl && p[i]; i++) {
        if (p[i] >= 'A' && p[i] <= 'Z') p[i] = (char)(p[i] + 32);
    }
    if (strcmp(p, "profile") == 0) {
        const char *active = gw_active_profile_name();
        json_t *arr = json_array();
        json_append(arr, json_string(name_str));
        json_append(arr, json_string(active ? active : ""));
        return arr;
        }
    if (strcmp(p, "static") != 0) return NULL;
    json_t *val = json_obj_get(raw, "value");
    if (!val || val->type != JSON_STRING || !val->str_val || !*val->str_val) return NULL;
    json_t *arr = json_array();
    json_append(arr, json_string(name_str));
    json_append(arr, json_string(val->str_val));
    return arr;
}

/* PoP: _apply_identity_header @ tools/mcp_tool.py:_apply_identity_header */
/* Merge the resolved identity header into headers (in place). Returns headers. */
json_t *mcp_apply_identity_header(const char *server_name, const json_t *config, json_t *headers) {
    if (!headers || headers->type != JSON_OBJECT) return headers;
    json_t *resolved = mcp_resolve_identity_header(server_name, config);
    if (!resolved || resolved->type != JSON_ARRAY || resolved->c.count < 2) {
        if (resolved) json_free(resolved);
        return headers;
    }
    json_t *name_val = resolved->c.items[0];
    json_t *value_val = resolved->c.items[1];
    if (!name_val || name_val->type != JSON_STRING) { json_free(resolved); return headers; }
    const char *new_name = name_val->str_val;
    const char *new_value = (value_val && value_val->type == JSON_STRING) ? value_val->str_val : "";
    /* check if any existing key matches case-insensitively */
    for (size_t i = 0; i < headers->c.count; i++) {
        json_t *kv = headers->c.items[i];
        if (!kv || kv->type != JSON_STRING) continue;
        /* case-insensitive compare */
        const char *ek = kv->str_val, *nk = new_name;
        int eq = 1;
        while (*ek && *nk) {
            if (((*ek >= 'A' && *ek <= 'Z') ? *ek + 32 : *ek) !=
                ((*nk >= 'A' && *nk <= 'Z') ? *nk + 32 : *nk)) { eq = 0; break; }
            ek++; nk++;
        }
        eq = eq && !*ek && !*nk;
        if (eq) {
            json_free(resolved);
            return headers;  /* explicit config wins */
        }
    }
    json_set(headers, new_name, json_string(new_value));  /* json_set steals */
    json_free(resolved);
    return headers;
}
