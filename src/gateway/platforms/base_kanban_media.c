/*
 * base_kanban_media.c - C11 port of upstream
 * gateway/platforms/base_kanban_media.py (gate w24_basekanban_865998).
 *
 * PoP: w24_basekanban_865998_board_name_ok @ gateway/platforms/base_kanban_media.py:_kanban_attachment_roots
 * PoP: w24_basekanban_865998_attachment_roots @ gateway/platforms/base_kanban_media.py:_kanban_attachment_roots
 * PoP: w24_basekanban_865998_board_db_paths @ gateway/platforms/base_kanban_media.py:_kanban_board_db_paths
 * PoP: w24_basekanban_865998_constants @ gateway/platforms/base_kanban_media.py:<module>
 *
 * Seams: the filesystem listing (``iterdir`` / ``is_dir`` / ``is_symlink`` /
 * ``is_file``), the environment override and ``expanduser`` all arrive as
 * values.  Ported here: the ``[a-z0-9][a-z0-9_-]{0,63}`` fullmatch gate, the
 * root order (shared root first, then per-board roots that pass the gate), and
 * the sidecar fan-out for a named board's store.  Path joining follows pathlib:
 * components are collapsed, ``.`` dropped, and the result rendered POSIX-style.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_basekanban_865998.h"

#define NAMEDB "kanban.db"
static const char *const KP_SQLITE_FILES[W24_BASEKANBAN_865998_SQLITE_FILES] = {
    NAMEDB, NAMEDB "-wal", NAMEDB "-shm", NAMEDB "-journal",
};
#define KP_PATTERN "[a-z0-9][a-z0-9_-]{0,63}"

/* ---- small string buffer ------------------------------------------------- */

typedef struct {
    char *p;
    size_t n, cap;
} kp_buf;

static int kp_reserve(kp_buf *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return 1;
    {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (cap < b->n + extra + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return 0;
        b->p = np;
        b->cap = cap;
    }
    return 1;
}

static int kp_add(kp_buf *b, const char *s, size_t n) {
    if (!kp_reserve(b, n)) return 0;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
    return 1;
}

static int kp_puts(kp_buf *b, const char *s) { return kp_add(b, s, strlen(s)); }

/* pathlib rendering: keep a leading "/", drop empty and "." components,
 * re-join with a single "/".  An empty result is "." (Path("")). */
static char *kp_norm(const char *raw) {
    kp_buf out = {0};
    int absolute = raw && raw[0] == '/';
    const char *p = raw ? raw : "";
    int wrote = 0;

    if (absolute) {
        if (!kp_add(&out, "/", 1)) return NULL;
    }
    while (*p) {
        const char *seg;
        size_t len;
        while (*p == '/') p++;
        seg = p;
        while (*p && *p != '/') p++;
        len = (size_t)(p - seg);
        if (len == 0 || (len == 1 && seg[0] == '.')) continue;
        if (out.n > (absolute ? 1u : 0u)) {
            if (!kp_add(&out, "/", 1)) { free(out.p); return NULL; }
        }
        if (!kp_add(&out, seg, len)) { free(out.p); return NULL; }
        wrote = 1;
    }
    if (!wrote && !absolute) {
        if (!kp_puts(&out, ".")) { free(out.p); return NULL; }
    }
    return out.p ? out.p : strdup(".");
}

/* Path(root) / seg... , normalised. */
static char *kp_join(const char *root, const char *const *segs, size_t nseg) {
    kp_buf raw = {0};
    size_t i;
    char *out;

    if (!kp_puts(&raw, root ? root : "")) { free(raw.p); return NULL; }
    for (i = 0; i < nseg; i++) {
        if (raw.n > 0 && raw.p[raw.n - 1] != '/') {
            if (!kp_add(&raw, "/", 1)) { free(raw.p); return NULL; }
        }
        if (!kp_puts(&raw, segs[i] ? segs[i] : "")) { free(raw.p); return NULL; }
    }
    out = kp_norm(raw.p ? raw.p : "");
    free(raw.p);
    return out;
}

/* Python str.strip() over ASCII whitespace. */
static char *kp_strip(const char *s) {
    size_t n, i = 0;
    char *out;
    if (!s) return strdup("");
    n = strlen(s);
    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' ||
                     s[i] == '\r' || s[i] == '\f' || s[i] == '\v')) i++;
    while (n > i && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' ||
                     s[n - 1] == '\r' || s[n - 1] == '\f' || s[n - 1] == '\v')) n--;
    out = (char *)malloc(n - i + 1);
    if (!out) return NULL;
    memcpy(out, s + i, n - i);
    out[n - i] = '\0';
    return out;
}

/* re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", name) */
static int kp_name_matches(const char *s) {
    size_t i, n;
    if (!s) return 0;
    n = strlen(s);
    if (n < 1 || n > 64) return 0;
    for (i = 0; i < n; i++) {
        char c = s[i];
        int alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (i == 0) {
            if (!alnum) return 0;
        } else if (!alnum && c != '_' && c != '-') {
            return 0;
        }
    }
    return 1;
}

/* Python truthiness of a JSON value. */
static int kp_truthy(const json_t *v) {
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

json_t *w24_basekanban_865998_board_name_ok(const json_t *name) {
    if (!name || name->type == JSON_NULL) return json_null();      /* bare "" is not None */
    if (name->type != JSON_STRING) return json_null();             /* re TypeError seam */
    return json_bool(kp_name_matches(name->str_val) ? true : false);
}

json_t *w24_basekanban_865998_attachment_roots(const json_t *req) {
    const json_t *boards, *override;
    const char *root;
    char *ov = NULL;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) return json_array();
    root = json_get_str(req, "root", "");
    override = json_obj_get(req, "override");
    if (override && override->type == JSON_STRING) {
        ov = kp_strip(override->str_val);
    } else if (kp_truthy(override)) {
        return json_null();                       /* .strip() AttributeError seam */
    }

    out = json_array();
    if (ov && ov[0]) {
        char *only = kp_norm(ov);
        if (only) { json_append(out, json_string(only)); free(only); }
        free(ov);
        return out;
    }
    free(ov);

    {
        static const char *const shared[2] = { "kanban", "attachments" };
        char *p = kp_join(root, shared, 2);
        if (p) { json_append(out, json_string(p)); free(p); }
    }

    boards = json_obj_get(req, "boards");
    if (boards && boards->type == JSON_ARRAY) {
        size_t i, n = json_len(boards);
        for (i = 0; i < n; i++) {
            const json_t *b = json_get(boards, i);
            const json_t *nm;
            const char *name;
            if (!b || b->type != JSON_OBJECT) continue;
            if (json_get_bool(b, "is_symlink", false)) continue;
            if (!json_get_bool(b, "has_db", false)) continue;
            nm = json_obj_get(b, "name");
            if (!nm || nm->type != JSON_STRING) continue;   /* path.name is always str */
            name = nm->str_val ? nm->str_val : "";
            if (!kp_name_matches(name)) continue;
            {
                const char *segs[4] = { "kanban", "boards", name, "attachments" };
                char *p = kp_join(root, segs, 4);
                if (p) { json_append(out, json_string(p)); free(p); }
            }
        }
    }
    return out;
}

json_t *w24_basekanban_865998_board_db_paths(const json_t *req) {
    const json_t *boards;
    const char *root;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) return json_array();
    root = json_get_str(req, "root", "");
    out = json_array();
    boards = json_obj_get(req, "boards");
    if (!boards || boards->type != JSON_ARRAY) return out;
    {
        size_t i, j, n = json_len(boards);
        for (i = 0; i < n; i++) {
            const json_t *b = json_get(boards, i);
            const char *name;
            if (!b || b->type != JSON_OBJECT) continue;
            name = json_get_str(b, "name", NULL);
            if (!name) continue;                     /* path.name is always str */
            for (j = 0; j < W24_BASEKANBAN_865998_SQLITE_FILES; j++) {
                const char *segs[4] = { "kanban", "boards", name, KP_SQLITE_FILES[j] };
                char *p = kp_join(root, segs, 4);
                if (p) { json_append(out, json_string(p)); free(p); }
            }
        }
    }
    return out;
}

json_t *w24_basekanban_865998_constants(void) {
    json_t *o = json_object();
    json_t *files = json_array();
    size_t i;
    for (i = 0; i < W24_BASEKANBAN_865998_SQLITE_FILES; i++)
        json_append(files, json_string(KP_SQLITE_FILES[i]));
    json_set(o, "board_name_pattern", json_string(KP_PATTERN));
    json_set(o, "board_db_name", json_string(NAMEDB));
    json_set(o, "sqlite_files", files);
    json_set(o, "kanban_dir", json_string("kanban"));
    json_set(o, "boards_subdir", json_string("boards"));
    json_set(o, "attachments_subdir", json_string("attachments"));
    return o;
}
