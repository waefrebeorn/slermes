/*
 * kanban_db_workspace.c — faithful C11 port of hermes_cli/kanban_db_workspace.py
 * (gate w18_kanbandbw_de7a45).
 *
 * PoP: w18_kanbandbw_de7a45_path_key @ hermes_cli/kanban_db_workspace.py:_path_key
 * PoP: w18_kanbandbw_de7a45_constants @ hermes_cli/kanban_db_workspace.py:_TERMINAL_STATUSES_SQL
 */
#include "noc_w18_kanbandbw_de7a45.h"

#include <stdlib.h>
#include <string.h>

/* ---- minimal UTF-8 codec + NFC canonical composition for path identity ---- */

typedef unsigned int cp_t;

static size_t decode_utf8(const char *s, cp_t *out, size_t cap) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = 0;
    while (*p && n < cap) {
        unsigned char b = *p;
        cp_t cp;
        size_t len;
        if (b < 0x80) { cp = b; len = 1; }
        else if ((b & 0xE0) == 0xC0) { cp = ((cp_t)(b & 0x1F) << 6) | (p[1] & 0x3F); len = 2; }
        else if ((b & 0xF0) == 0xE0) { cp = ((cp_t)(b & 0x0F) << 12) | ((cp_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); len = 3; }
        else if ((b & 0xF8) == 0xF0) { cp = ((cp_t)(b & 0x07) << 18) | ((cp_t)(p[1] & 0x3F) << 12) | ((cp_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); len = 4; }
        else { cp = b; len = 1; }
        out[n++] = cp;
        p += len;
    }
    return n;
}

static size_t encode_utf8(cp_t cp, unsigned char *out) {
    if (cp < 0x80) { out[0] = (unsigned char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (unsigned char)(0xC0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (unsigned char)(0xE0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (unsigned char)(0xF0 | (cp >> 18));
    out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (unsigned char)(0x80 | (cp & 0x3F));
    return 4;
}

struct compose { cp_t base, mark, out; };
static const struct compose COMPOSE_TABLE[] = {
    /* grave */
    {'A',0x300,0xC0},{'E',0x300,0xC8},{'I',0x300,0xCC},{'O',0x300,0xD2},{'U',0x300,0xD9},
    {'a',0x300,0xE0},{'e',0x300,0xE8},{'i',0x300,0xEC},{'o',0x300,0xF2},{'u',0x300,0xF9},
    /* acute */
    {'A',0x301,0xC1},{'E',0x301,0xC9},{'I',0x301,0xCD},{'O',0x301,0xD3},{'U',0x301,0xDA},{'Y',0x301,0xDD},
    {'a',0x301,0xE1},{'e',0x301,0xE9},{'i',0x301,0xED},{'o',0x301,0xF3},{'u',0x301,0xFA},{'y',0x301,0xFD},
    /* circumflex */
    {'A',0x302,0xC2},{'E',0x302,0xCA},{'I',0x302,0xCE},{'O',0x302,0xD4},{'U',0x302,0xDB},
    {'a',0x302,0xE2},{'e',0x302,0xEA},{'i',0x302,0xEE},{'o',0x302,0xF4},{'u',0x302,0xFB},
    /* tilde */
    {'A',0x303,0xC3},{'N',0x303,0xD1},{'O',0x303,0xD5},
    {'a',0x303,0xE3},{'n',0x303,0xF1},{'o',0x303,0xF5},
    /* diaeresis */
    {'A',0x308,0xC4},{'E',0x308,0xCB},{'I',0x308,0xCF},{'O',0x308,0xD6},{'U',0x308,0xDC},{'Y',0x308,0x178},
    {'a',0x308,0xE4},{'e',0x308,0xEB},{'i',0x308,0xEF},{'o',0x308,0xF6},{'u',0x308,0xFC},{'y',0x308,0xFF},
    /* ring above */
    {'A',0x30A,0xC5},{'a',0x30A,0xE5},
    /* caron (Latin Extended-A) */
    {'C',0x30C,0x10C},{'S',0x30C,0x160},{'Z',0x30C,0x17D},
    {'c',0x30C,0x10D},{'s',0x30C,0x161},{'z',0x30C,0x17E},
    /* cedilla */
    {'C',0x327,0xC7},{'c',0x327,0xE7},
};

static cp_t compose_pair(cp_t base, cp_t mark) {
    size_t i;
    for (i = 0; i < sizeof(COMPOSE_TABLE) / sizeof(COMPOSE_TABLE[0]); i++)
        if (COMPOSE_TABLE[i].base == base && COMPOSE_TABLE[i].mark == mark)
            return COMPOSE_TABLE[i].out;
    return 0;
}

/* str(path) if path is not None; unicodedata.normalize("NFC", ...) on it. */
json_t *w18_kanbandbw_de7a45_path_key(const char *path) {
    cp_t cps[16384], out[16384];
    size_t n, i, m = 0;
    char buf[65536];
    size_t used = 0;
    if (!path) return json_string("");
    n = decode_utf8(path, cps, 16384);
    for (i = 0; i < n; i++) {
        if (m > 0 && cps[i] >= 0x300 && cps[i] <= 0x36F) {
            cp_t composed = compose_pair(out[m - 1], cps[i]);
            if (composed) { out[m - 1] = composed; continue; }
        }
        out[m++] = cps[i];
    }
    for (i = 0; i < m; i++) {
        size_t k = encode_utf8(out[i], (unsigned char *)buf + used);
        used += k;
    }
    buf[used] = '\0';
    { json_t *r = json_string(buf); return r; }
}

json_t *w18_kanbandbw_de7a45_constants(void) {
    json_t *out = json_object();
    json_set(out, "_TERMINAL_STATUSES_SQL", json_string("('done', 'archived', 'failed', 'cancelled')"));
    json_set(out, "_WORKSPACE_ROW_SQL",
             json_string("SELECT workspace_kind, workspace_path, branch_name FROM tasks WHERE id = ?"));
    json_set(out, "_ACTIVE_CHILDREN_SQL",
             json_string("SELECT 1 FROM task_links l JOIN tasks t ON t.id = l.child_id "
                         "WHERE l.parent_id = ? AND t.status NOT IN ('done', 'archived', 'failed', 'cancelled') LIMIT 1"));
    json_set(out, "_TERMINAL_TASK_SQL",
             json_string("SELECT 1 FROM tasks WHERE id = ? AND status IN ('done', 'archived', 'failed', 'cancelled')"));
    json_set(out, "_OTHER_LIVE_PATHS_SQL",
             json_string("SELECT workspace_path FROM tasks WHERE id != ? AND workspace_path IS NOT NULL "
                         "AND status NOT IN ('done', 'archived', 'failed', 'cancelled')"));
    json_set(out, "_ANY_LIVE_PATHS_SQL",
             json_string("SELECT workspace_path FROM tasks WHERE workspace_path IS NOT NULL "
                         "AND status NOT IN ('done', 'archived', 'failed', 'cancelled')"));
    {
        json_t *kinds = json_array();
        json_append(kinds, json_string("scratch"));
        json_append(kinds, json_string("worktree"));
        json_set(out, "_REMOVABLE_KINDS", kinds);
    }
    json_set(out, "_SCRATCH_TIP_SENTINEL_NAME", json_string(".scratch_tip_shown"));
    json_set(out, "_SCRATCH_TIP_MESSAGE",
             json_string("scratch workspaces are ephemeral \xe2\x80\x94 they're deleted when the task "
                         "completes. Use --workspace worktree: (git worktree) or "
                         "--workspace dir:/abs/path (existing dir) to preserve worker output."));
    return out;
}

