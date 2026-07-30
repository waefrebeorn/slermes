/* Slermes C port — tools/patch_parser.py (V4A patch format parser).
 *
 * Faithful port of parse_v4a_patch() only. The apply/validate phases
 * exercise file I/O and the live tool registry, so they are NA in C.
 * parse_v4a_patch is a pure string->structure transform, ideal to port.
 *
 * Behavior matches LIVE Python (see tests/sta_oracle_patch_parser.py):
 *  - boundary detection (*** Begin/End Patch, optional markers)
 *  - Update/Add/Delete/Move file operations
 *  - @@ context-hint hunks, + / - / ' ' / implicit-space / "\ skip lines
 *  - finalize-on-new-op and finalize-at-end
 *  - validation: empty path, UPDATE-with-no-hunks, MOVE-with-no-dst
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef enum { OP_ADD, OP_UPDATE, OP_DELETE, OP_MOVE } op_type_t;

typedef struct {
    char prefix;        /* ' ', '-', '+' */
    char *content;      /* malloc'd; never NULL */
} hunk_line_t;

typedef struct {
    char *context_hint; /* malloc'd or NULL */
    hunk_line_t *lines;
    size_t n_lines;
    size_t cap_lines;
} hunk_t;

typedef struct {
    op_type_t op;
    char *file_path;    /* malloc'd */
    char *new_path;     /* malloc'd or NULL (move dst) */
    hunk_t *hunks;
    size_t n_hunks;
    size_t cap_hunks;
} patch_op_t;

typedef struct {
    patch_op_t *ops;
    size_t n_ops;
    size_t cap_ops;
    char *error;        /* malloc'd or NULL */
} parse_result_t;

/* ---------- small helpers ---------- */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    memcpy(p, s, n);
    return p;
}

static char *xstrndup(const char *s, size_t n) {
    char *p = malloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Trim leading/trailing ASCII whitespace (in place, returns start). */
static char *trim(char *s) {
    if (!s) return s;
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = '\0';
    return s;
}

/* PCRE-style simple regex helpers (no full regex needed for markers). */

/* Match "*** <OP> File: <rest>" -> returns malloc'd rest (trimmed) or NULL.
 * opkw is e.g. "Update", "Add", "Delete". Spaces around keywords tolerated. */
static char *match_file_marker(const char *line, const char *opkw) {
    /* line must start with "***" */
    if (strncmp(line, "***", 3) != 0) return NULL;
    const char *p = line + 3;
    while (*p == ' ') p++;
    size_t kwl = strlen(opkw);
    if (strncmp(p, opkw, kwl) != 0) return NULL;
    p += kwl;
    while (*p == ' ') p++;
    if (strncmp(p, "File:", 5) != 0) return NULL;
    p += 5;
    while (*p == ' ') p++;
    char *rest = xstrdup(p);
    return xstrdup(trim(rest)); /* trimmed copy */
}

/* Match "*** Move File: <src> -> <dst>". Returns src,dst malloc'd or NULL. */
static bool match_move_marker(const char *line, char **src, char **dst) {
    if (strncmp(line, "***", 3) != 0) return false;
    const char *p = line + 3;
    while (*p == ' ') p++;
    if (strncmp(p, "Move", 4) != 0) return false;
    p += 4;
    while (*p == ' ') p++;
    if (strncmp(p, "File:", 5) != 0) return false;
    p += 5;
    while (*p == ' ') p++;
    /* src = up to " -> " */
    const char *arrow = strstr(p, "->");
    if (!arrow) return false;
    size_t srclen = (size_t)(arrow - p);
    /* trim trailing ws before arrow */
    while (srclen > 0 && (p[srclen-1] == ' ' || p[srclen-1] == '\t')) srclen--;
    *src = xstrndup(p, srclen);
    const char *d = arrow + 2;
    while (*d == ' ' || *d == '\t') d++;
    char *d2 = xstrdup(d);
    *dst = xstrdup(trim(d2));
    free(d2);
    return true;
}

/* ---------- structural mutation ---------- */

static hunk_line_t *hunk_push_line(hunk_t *h, char prefix, const char *content) {
    if (h->n_lines + 1 > h->cap_lines) {
        h->cap_lines = h->cap_lines ? h->cap_lines * 2 : 4;
        h->lines = realloc(h->lines, h->cap_lines * sizeof(hunk_line_t));
    }
    hunk_line_t *l = &h->lines[h->n_lines++];
    l->prefix = prefix;
    l->content = xstrdup(content ? content : "");
    return l;
}

static void hunk_free(hunk_t *h) {
    free(h->context_hint);
    for (size_t i = 0; i < h->n_lines; i++) free(h->lines[i].content);
    free(h->lines);
    h->lines = NULL; h->n_lines = h->cap_lines = 0; h->context_hint = NULL;
}

static void op_free(patch_op_t *o) {
    free(o->file_path);
    free(o->new_path);
    for (size_t i = 0; i < o->n_hunks; i++) hunk_free(&o->hunks[i]);
    free(o->hunks);
    o->file_path = o->new_path = NULL;
    o->hunks = NULL; o->n_hunks = o->cap_hunks = 0;
}

static void result_free(parse_result_t *r) {
    for (size_t i = 0; i < r->n_ops; i++) op_free(&r->ops[i]);
    free(r->ops);
    free(r->error);
    r->ops = NULL; r->n_ops = r->cap_ops = 0; r->error = NULL;
}

static patch_op_t *result_push_op(parse_result_t *r) {
    if (r->n_ops + 1 > r->cap_ops) {
        r->cap_ops = r->cap_ops ? r->cap_ops * 2 : 4;
        r->ops = realloc(r->ops, r->cap_ops * sizeof(patch_op_t));
    }
    patch_op_t *o = &r->ops[r->n_ops++];
    memset(o, 0, sizeof(*o));
    return o;
}

/* Finalize current_hunk into current_op (append if it has lines). */
static void finalize_hunk(patch_op_t *cur, hunk_t *cur_hunk) {
    if (cur && cur_hunk && cur_hunk->n_lines > 0) {
        if (cur->n_hunks + 1 > cur->cap_hunks) {
            cur->cap_hunks = cur->cap_hunks ? cur->cap_hunks * 2 : 2;
            cur->hunks = realloc(cur->hunks, cur->cap_hunks * sizeof(hunk_t));
        }
        cur->hunks[cur->n_hunks++] = *cur_hunk; /* move */
        /* reset the local so free() doesn't double-free moved contents */
        cur_hunk->lines = NULL; cur_hunk->n_lines = 0; cur_hunk->cap_lines = 0;
        cur_hunk->context_hint = NULL;
    }
}

/* Close the current op: finalize its pending hunk (if any) into cur.
 * cur already points at a live slot in r->ops, so nothing is copied or freed. */
static void close_cur_op(patch_op_t *cur, hunk_t *cur_hunk) {
    if (cur) finalize_hunk(cur, cur_hunk);
}

/* PoP: parse_v4a_patch @ tools/patch_parser.py:parse_v4a_patch */
parse_result_t patch_parser_parse_v4a(const char *patch_content) {
    parse_result_t r;
    memset(&r, 0, sizeof(r));

    /* split into lines on '\n' (keep empty lines, like Python split) */
    size_t len = strlen(patch_content);
    /* count lines */
    size_t nlines = 1;
    for (size_t i = 0; i < len; i++) if (patch_content[i] == '\n') nlines++;
    char **lines = malloc(nlines * sizeof(char *));
    size_t li = 0;
    const char *start = patch_content;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || patch_content[i] == '\n') {
            size_t l = (size_t)(&patch_content[i] - start);
            char *cpy = xstrndup(start, l);
            /* strip a trailing '\r' (common in CRLF) — Python keeps '\r' in
             * content, but our markers are whitespace-tolerant; keep '\r' off
             * the prefix check by trimming only trailing '\r' on whole-line ops. */
            if (l > 0 && cpy[l-1] == '\r') cpy[l-1] = '\0';
            lines[li++] = cpy;
            start = patch_content + i + 1;
        }
    }
    nlines = li;

    long start_idx = -2; /* -2 = not found */
    long end_idx = (long)nlines;
    for (size_t i = 0; i < nlines; i++) {
        if (strstr(lines[i], "*** Begin Patch") || strstr(lines[i], "***Begin Patch")) {
            start_idx = (long)i;
        } else if (strstr(lines[i], "*** End Patch") || strstr(lines[i], "***End Patch")) {
            end_idx = (long)i;
            break;
        }
    }
    if (start_idx == -2) start_idx = -1; /* try without explicit begin */

    patch_op_t *cur = NULL;
    hunk_t cur_hunk;
    memset(&cur_hunk, 0, sizeof(cur_hunk));

    for (long i = start_idx + 1; i < end_idx; i++) {
        char *line = lines[i];

        char *upd = match_file_marker(line, "Update");
        char *add = match_file_marker(line, "Add");
        char *del = match_file_marker(line, "Delete");
        char *mv_src = NULL, *mv_dst = NULL;
        bool is_move = match_move_marker(line, &mv_src, &mv_dst);

        if (upd) {
            close_cur_op(cur, &cur_hunk);
            cur = result_push_op(&r);
            cur->op = OP_UPDATE;
            cur->file_path = upd;
            memset(&cur_hunk, 0, sizeof(cur_hunk));
        } else if (add) {
            close_cur_op(cur, &cur_hunk);
            cur = result_push_op(&r);
            cur->op = OP_ADD;
            cur->file_path = add;
            memset(&cur_hunk, 0, sizeof(cur_hunk));
        } else if (del) {
            close_cur_op(cur, &cur_hunk);
            cur = result_push_op(&r);
            cur->op = OP_DELETE;
            cur->file_path = del;
            close_cur_op(cur, &cur_hunk);
            cur = NULL;
        } else if (is_move) {
            close_cur_op(cur, &cur_hunk);
            cur = result_push_op(&r);
            cur->op = OP_MOVE;
            cur->file_path = mv_src;
            cur->new_path = mv_dst;
            close_cur_op(cur, &cur_hunk);
            cur = NULL;
        } else if (line[0] == '@' && line[1] == '@') {
            finalize_hunk(cur, &cur_hunk);
            char *a = line + 2;
            while (*a == ' ') a++;
            char *b = strstr(a, "@@");
            char *hint = NULL;
            if (b) {
                size_t hl = (size_t)(b - a);
                while (hl > 0 && (a[hl-1] == ' ' || a[hl-1] == '\t')) hl--;
                hint = xstrndup(a, hl);
            }
            memset(&cur_hunk, 0, sizeof(cur_hunk));
            cur_hunk.context_hint = hint;
        } else if (cur && line[0] != '\0') {
            if (line[0] == '+') {
                hunk_push_line(&cur_hunk, '+', line + 1);
            } else if (line[0] == '-') {
                hunk_push_line(&cur_hunk, '-', line + 1);
            } else if (line[0] == ' ') {
                hunk_push_line(&cur_hunk, ' ', line + 1);
            } else if (line[0] == '\\') {
                /* "\ No newline at end of file" — skip */
            } else {
                hunk_push_line(&cur_hunk, ' ', line);
            }
        }
        /* upd/add/del/mv pointers are now owned by the live op slot (cur or
         * an already-closed slot). Do NOT free them here. */
    }

    /* finalize last op (if still open) */
    if (cur) { close_cur_op(cur, &cur_hunk); cur = NULL; }

    for (size_t i = 0; i < nlines; i++) free(lines[i]);
    free(lines);

    if (r.n_ops == 0) {
        /* empty patch is not an error */
        return r;
    }

    /* validation */
    char errbuf[4096];
    errbuf[0] = '\0';
    for (size_t i = 0; i < r.n_ops; i++) {
        patch_op_t *o = &r.ops[i];
        if (!o->file_path || o->file_path[0] == '\0') {
            strncat(errbuf, "Operation with empty file path; ", sizeof(errbuf)-1);
        }
        if (o->op == OP_UPDATE && o->n_hunks == 0) {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "UPDATE '%s': no hunks found; ",
                     o->file_path ? o->file_path : "");
            strncat(errbuf, tmp, sizeof(errbuf)-1);
        }
        if (o->op == OP_MOVE && (!o->new_path || o->new_path[0] == '\0')) {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "MOVE '%s': missing destination path (expected 'src -> dst'); ",
                     o->file_path ? o->file_path : "");
            strncat(errbuf, tmp, sizeof(errbuf)-1);
        }
    }
    if (errbuf[0] != '\0') {
        /* drop trailing "; " */
        size_t el = strlen(errbuf);
        if (el >= 2) { errbuf[el-1] = '\0'; errbuf[el-2] = '\0'; }
        char *msg = malloc(strlen("Parse error: ") + strlen(errbuf) + 1);
        strcpy(msg, "Parse error: ");
        strcat(msg, errbuf);
        /* free parsed ops on error (Python returns ([], error)) */
        for (size_t i = 0; i < r.n_ops; i++) op_free(&r.ops[i]);
        free(r.ops);
        r.ops = NULL; r.n_ops = 0; r.cap_ops = 0;
        r.error = msg;
    }

    return r;
}

/* ---------- public API (header names) ---------- */

typedef op_type_t patch_op_type_t;
typedef hunk_line_t patch_hunk_line_t;
typedef hunk_t patch_hunk_t;
typedef parse_result_t patch_parse_result_t;

void patch_parser_result_free(patch_parse_result_t *r)
{
    result_free(r);
}

/* JSON string escaper for canonical output. */
static void emit_json_str(const char *s)
{
    putchar('"');
    for (const char *p = s; p && *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            default:
                if (c < 0x20) fprintf(stdout, "\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

static const char *op_name(op_type_t t)
{
    switch (t) {
        case OP_ADD: return "add";
        case OP_UPDATE: return "update";
        case OP_DELETE: return "delete";
        case OP_MOVE: return "move";
    }
    return "?";
}

/* Print canonical JSON (one line). Order must match the Python oracle's
 * serializer so the two strings can be diffed directly. */
void patch_parser_print_canonical(const patch_parse_result_t *r)
{
    if (r->error) {
        putchar('{');
        fputs("\"error\":", stdout);
        emit_json_str(r->error);
        putchar('}');
        putchar('\n');
        return;
    }
    putchar('[');
    for (size_t i = 0; i < r->n_ops; i++) {
        const patch_op_t *o = &r->ops[i];
        if (i) putchar(',');
        putchar('{');
        fputs("\"op\":", stdout); emit_json_str(op_name(o->op)); putchar(',');
        fputs("\"path\":", stdout); emit_json_str(o->file_path ? o->file_path : ""); putchar(',');
        fputs("\"new\":", stdout);
        if (o->new_path) emit_json_str(o->new_path); else fputs("null", stdout);
        putchar(',');
        fputs("\"hunks\":[", stdout);
        for (size_t h = 0; h < o->n_hunks; h++) {
            const hunk_t *hk = &o->hunks[h];
            if (h) putchar(',');
            putchar('{');
            fputs("\"hint\":", stdout);
            if (hk->context_hint) emit_json_str(hk->context_hint); else fputs("null", stdout);
            fputs(",\"lines\":[", stdout);
            for (size_t l = 0; l < hk->n_lines; l++) {
                if (l) putchar(',');
                putchar('[');
                putchar('"'); putchar(hk->lines[l].prefix); putchar('"'); putchar(',');
                emit_json_str(hk->lines[l].content);
                putchar(']');
            }
            putchar(']');
            putchar('}');
        }
        putchar(']');
        putchar('}');
    }
    putchar(']');
    putchar('\n');
}
