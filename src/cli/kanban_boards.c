/*
 * kanban_boards.c — board management for hermes_cli/kanban_db.py
 *
 * Concern: multi-board support — slug validation, board directory layout,
 * board.json metadata read/write, board creation/listing/removal, and the
 * "default" board special-case. Pure filesystem + JSON; reuses the path
 * helpers (board_dir / boards_root / board_metadata_path / kanban_home /
 * normalize_board_slug / get_current_board) from port_kanban_db.c.
 *
 * Faithful to:
 *   create_board, list_boards, read_board_metadata, write_board_metadata,
 *   remove_board, board_exists, scoped_current_board (no-op),
 *   _normalize_board_slug, board_dir, boards_root, board_metadata_path.
 *
 * Opaque structs + minimal includes. C11.
 */

#include "kanban_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

/* ------------------------------------------------------------------ */
/* small JSON helpers (board.json is flat; we hand-roll to avoid a dep) */

static int path_exists(const char *p)
{
    struct stat st;
    return (stat(p, &st) == 0);
}

static int ensure_dir(const char *p)
{
    if (path_exists(p)) return 1;
    /* create parent chain recursively (mirrors Python Path.mkdir(parents=True)) */
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", p);
    for (size_t i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
    return (mkdir(tmp, 0755) == 0) || path_exists(tmp);
}

/* Append a JSON string (with escaping) to a growable buffer. */
static void js_str(char **buf, size_t *len, size_t *cap, const char *s)
{
    if (!s) s = "";
    size_t need = *len + strlen(s) * 2 + 8;
    if (need > *cap) { while (need > *cap) *cap *= 2; *buf = realloc(*buf, *cap); }
    (*buf)[(*len)++] = '"';
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') { (*buf)[(*len)++] = '\\'; }
        (*buf)[(*len)++] = *p;
    }
    (*buf)[(*len)++] = '"';
}

/* Turn a slug into a presentable default display name (mirrors Python's
 * _default_board_display_name): title-case on '-'/'_' boundaries. */
static void default_display_name(const char *slug, char *out, size_t outsz)
{
    size_t j = 0; int cap_next = 1;
    for (size_t i = 0; slug[i] && j + 1 < outsz; i++) {
        char c = slug[i];
        if (c == '-' || c == '_') {
            if (j && j + 1 < outsz) out[j++] = ' ';
            cap_next = 1;
            continue;
        }
        if (cap_next) { c = (char)toupper((unsigned char)c); cap_next = 0; }
        out[j++] = c;
    }
    out[j] = '\0';
}

/* Build a metadata JSON object. Fields may be NULL to omit (use defaults). */
static char *build_metadata(const char *slug, const char *name,
                            const char *description, const char *icon,
                            const char *color, const char *default_workdir,
                            int has_archived, int archived,
                            const char *db_path, long created_at)
{
    char *buf = malloc(1024); size_t len = 0, cap = 1024;
    len += (size_t)snprintf(buf + len, cap - len, "{\"slug\":");
    js_str(&buf, &len, &cap, slug);
    buf[len++] = ',';
    char disp[128];
    if (name && *name) snprintf(disp, sizeof(disp), "%s", name);
    else default_display_name(slug, disp, sizeof(disp));
    len += (size_t)snprintf(buf + len, cap - len, "\"name\":");
    js_str(&buf, &len, &cap, disp);
    buf[len++] = ',';
    len += (size_t)snprintf(buf + len, cap - len, "\"description\":");
    js_str(&buf, &len, &cap, description ? description : "");
    buf[len++] = ',';
    len += (size_t)snprintf(buf + len, cap - len, "\"icon\":");
    js_str(&buf, &len, &cap, icon ? icon : "");
    buf[len++] = ',';
    len += (size_t)snprintf(buf + len, cap - len, "\"color\":");
    js_str(&buf, &len, &cap, color ? color : "");
    buf[len++] = ',';
    len += (size_t)snprintf(buf + len, cap - len, "\"default_workdir\":");
    if (default_workdir && *default_workdir) js_str(&buf, &len, &cap, default_workdir);
    else len += (size_t)snprintf(buf + len, cap - len, "null");
    buf[len++] = ',';
    len += (size_t)snprintf(buf + len, cap - len, "\"created_at\":");
    if (created_at > 0) len += (size_t)snprintf(buf + len, cap - len, "%ld", created_at);
    else len += (size_t)snprintf(buf + len, cap - len, "null");
    buf[len++] = ',';
    len += (size_t)snprintf(buf + len, cap - len, "\"archived\":%s", has_archived ? (archived ? "true" : "false") : "false");
    if (db_path) {
        buf[len++] = ',';
        len += (size_t)snprintf(buf + len, cap - len, "\"db_path\":");
        js_str(&buf, &len, &cap, db_path);
    }
    buf[len++] = '}';
    buf[len] = '\0';
    return buf;
}

/* Parse a known string field from a board.json buffer. Returns malloc'd
 * value or NULL. Very small parser: searches for  "key":  then a quoted
 * string. Used only for reading existing board.json. */
static char *meta_get_str(const char *json, const char *key)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p && *p != ':') p++;
    if (!*p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return NULL;
    p++;
    size_t n = 0; const char *q = p;
    while (*q && *q != '"') { if (*q == '\\') q++; n++; q++; }
    char *out = malloc(n + 1); size_t k = 0;
    while (*p && *p != '"') {
        if (*p == '\\') p++;
        out[k++] = *p++;
    }
    out[k] = '\0';
    return out;
}

static long meta_get_long(const char *json, const char *key, long def)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    while (*p && *p != ':') p++;
    if (!*p) return def;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == 'n') return def; /* null */
    return (long)strtol(p, NULL, 10);
}

/* PoP: kdb_board_exists @ hermes_cli/kanban_db.py:board_exists */
int kdb_board_exists(const char *board)
{
    char *slug = normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    int exists;
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) {
        /* default always exists once a DB is reachable; treat slug as exists */
        exists = 1;
    } else {
        char *bd = board_dir(slug);
        char dbp[PATH_MAX]; char jp[PATH_MAX];
        snprintf(dbp, sizeof(dbp), "%s/kanban.db", bd);
        snprintf(jp, sizeof(jp), "%s/board.json", bd);
        exists = path_exists(dbp) || path_exists(jp);
        free(bd);
    }
    free(slug);
    return exists;
}

/* PoP: kdb_read_board_metadata @ hermes_cli/kanban_db.py:read_board_metadata */
char *kdb_read_board_metadata(const char *board)
{
    char *slug = normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    char *jp = board_metadata_path(slug);

    char *name = NULL, *description = NULL, *icon = NULL, *color = NULL,
         *default_workdir = NULL;
    int has_archived = 0, archived = 0; long created_at = 0;
    int has_dw = 0;

    if (jp && path_exists(jp)) {
        FILE *f = fopen(jp, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            char *raw = malloc((size_t)sz + 1);
            if (fread(raw, 1, (size_t)sz, f) == (size_t)sz) {
                raw[sz] = '\0';
                name = meta_get_str(raw, "name");
                description = meta_get_str(raw, "description");
                icon = meta_get_str(raw, "icon");
                color = meta_get_str(raw, "color");
                default_workdir = meta_get_str(raw, "default_workdir");
                has_dw = (default_workdir != NULL);
                created_at = meta_get_long(raw, "created_at", 0);
                const char *a = strstr(raw, "\"archived\"");
                if (a) { has_archived = 1; archived = (strncmp(a + 10, ":true", 5) == 0); }
            }
            free(raw);
            fclose(f);
        }
    }
    free(jp);

    /* db_path */
    char *dbp = kanban_db_path(slug);

    char *out = build_metadata(slug, name, description, icon, color,
                               has_dw ? default_workdir : NULL,
                               has_archived, archived, dbp, created_at);
    free(name); free(description); free(icon); free(color); free(default_workdir);
    free(dbp); free(slug);
    return out;
}

/* PoP: kdb_write_board_metadata @ hermes_cli/kanban_db.py:write_board_metadata */
char *kdb_write_board_metadata(const char *board,
                               const char *name, const char *description,
                               const char *icon, const char *color,
                               int archived_set, int archived,
                               const char *default_workdir)
{
    char *slug = normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);

    /* Start from existing metadata, then overlay provided fields. */
    char *cur = kdb_read_board_metadata(slug);
    char *c_name = NULL, *c_desc = NULL, *c_icon = NULL, *c_color = NULL,
         *c_dw = NULL; int has_dw = 0;
    int has_arch = 0, arch = 0; long created = 0;
    if (cur) {
        c_name = meta_get_str(cur, "name");
        c_desc = meta_get_str(cur, "description");
        c_icon = meta_get_str(cur, "icon");
        c_color = meta_get_str(cur, "color");
        c_dw = meta_get_str(cur, "default_workdir"); has_dw = (c_dw != NULL);
        created = meta_get_long(cur, "created_at", 0);
        const char *a = strstr(cur, "\"archived\"");
        if (a) { has_arch = 1; arch = (strncmp(a + 10, ":true", 5) == 0); }
        free(cur);
    }

    const char *use_name = name ? name : (c_name ? c_name : slug);
    const char *use_desc = description ? description : c_desc;
    const char *use_icon = icon ? icon : c_icon;
    const char *use_color = color ? color : c_color;
    const char *use_dw = default_workdir ? default_workdir : (has_dw ? c_dw : NULL);
    int use_arch = archived_set ? archived : has_arch ? arch : 0;
    if (!created) created = kdb_now();

    char *jp = board_metadata_path(slug);
    char *bd = board_dir(slug);
    if (bd) { ensure_dir(bd); free(bd); }
    char *out = build_metadata(slug, use_name, use_desc, use_icon, use_color,
                               use_dw, 1, use_arch, NULL, created);
    if (jp) {
        FILE *f = fopen(jp, "wb");
        if (f) {
            fprintf(f, "%s\n", out);
            fclose(f);
        }
        free(jp);
    }
    free(c_name); free(c_desc); free(c_icon); free(c_color); free(c_dw);
    free(slug);
    return out; /* db_path omitted from written file; matches Python */
}

/* PoP: kdb_create_board @ hermes_cli/kanban_db.py:create_board */
char *kdb_create_board(const char *slug,
                       const char *name, const char *description,
                       const char *icon, const char *color,
                       const char *default_workdir)
{
    char *normed = normalize_board_slug(slug);
    if (!normed) return NULL; /* bad slug -> error (NULL) */

    char *meta = kdb_write_board_metadata(normed, name, description, icon,
                                          color, 0, 0, default_workdir);
    /* Touch the DB so list_boards() sees it immediately. */
    sqlite3 *c = kdb_connect(normed);
    if (c) kdb_close(c);
    free(normed);
    return meta;
}

/* PoP: kdb_list_boards @ hermes_cli/kanban_db.py:list_boards */
char *kdb_list_boards(int include_archived)
{
    /* default first */
    char *def = kdb_read_board_metadata(KB_DEFAULT_BOARD);
    char **others = NULL; int n_others = 0, ocap = 8;
    others = malloc(sizeof(char*) * ocap);

    char *root = kanban_boards_root();
    if (root && path_exists(root)) {
        DIR *d = opendir(root);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.') continue;
#ifdef _DIRENT_HAVE_D_TYPE
                if (e->d_type != DT_UNKNOWN && e->d_type != DT_DIR) continue;
#endif
                char *slug = normalize_board_slug(e->d_name);
                if (!slug) continue;
                if (strcmp(slug, KB_DEFAULT_BOARD) == 0) { free(slug); continue; }
                char *bd = board_dir(slug);
                char dbp[PATH_MAX]; char jp[PATH_MAX];
                snprintf(dbp, sizeof(dbp), "%s/kanban.db", bd);
                snprintf(jp, sizeof(jp), "%s/board.json", bd);
                int ok = path_exists(dbp) || path_exists(jp);
                free(bd);
                if (!ok) { free(slug); continue; }
                char *m = kdb_read_board_metadata(slug);
                /* skip archived unless requested */
                if (m && !include_archived) {
                    const char *a = strstr(m, "\"archived\":true");
                    if (a) { free(m); free(slug); continue; }
                }
                if (m) {
                    if (n_others >= ocap) { ocap *= 2; others = realloc(others, sizeof(char*)*ocap); }
                    others[n_others++] = m;
                }
                free(slug);
            }
            closedir(d);
        }
    }

    /* sort others alphabetically by slug (simple insertion sort on JSON) */
    for (int i = 1; i < n_others; i++) {
        char *key = others[i]; int j = i - 1;
        while (j >= 0) {
            char *sj = strstr(others[j], "\"slug\":");
            char *sk = strstr(key, "\"slug\":");
            int cmp = sj && sk ? strcmp(sj, sk) : 0;
            if (cmp > 0) { others[j+1] = others[j]; j--; } else break;
        }
        others[j+1] = key;
    }

    size_t cap = 8192; char *out = malloc(cap); size_t len = 0;
    len += (size_t)snprintf(out + len, cap - len, "[");
    int first = 1;
    if (def) {
        len += (size_t)snprintf(out + len, cap - len, "%s", def);
        first = 0; free(def);
    }
    for (int i = 0; i < n_others; i++) {
        if (!first) len += (size_t)snprintf(out + len, cap - len, ",");
        first = 0;
        len += (size_t)snprintf(out + len, cap - len, "%s", others[i]);
        free(others[i]);
    }
    len += (size_t)snprintf(out + len, cap - len, "]");
    out[len] = '\0';
    free(others); free(root);
    return out;
}

/* Best-effort recursive directory deletion (used for non-archived removal). */
static void remove_dir_recursive(const char *path)
{
    DIR *d = opendir(path);
    if (!d) { remove(path); return; }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char child[PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
#ifdef _DIRENT_HAVE_D_TYPE
        if (e->d_type == DT_DIR) { remove_dir_recursive(child); }
        else { remove(child); }
#else
        struct stat st;
        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode)) remove_dir_recursive(child);
        else remove(child);
#endif
    }
    closedir(d);
    remove(path);
}

/* PoP: kdb_remove_board @ hermes_cli/kanban_db.py:remove_board */
char *kdb_remove_board(const char *slug, int archive)
{
    char *normed = normalize_board_slug(slug);
    if (!normed) { return NULL; }
    if (strcmp(normed, KB_DEFAULT_BOARD) == 0) { free(normed); return NULL; }
    char *bd = board_dir(normed);
    if (!bd || !path_exists(bd)) { free(bd); free(normed); return NULL; }

    char *home = kanban_home();
    size_t need = strlen(home) + strlen(normed) + 64;
    char *new_path = malloc(need);
    if (archive) {
        long ts = kdb_now();
        snprintf(new_path, need, "%s/kanban/boards/_archived/%s-%ld",
                 home, normed, ts);
        char archd[PATH_MAX];
        snprintf(archd, sizeof(archd), "%s/kanban/boards/_archived", home);
        ensure_dir(archd);
        rename(bd, new_path);
    } else {
        snprintf(new_path, need, "");
        remove_dir_recursive(bd);
    }
    size_t oc = 256; char *out = malloc(oc); size_t ol = 0;
    ol += (size_t)snprintf(out + ol, oc - ol, "{\"slug\":");
    {
        const char *p = normed;
        out[ol++] = '"';
        for (; *p; p++) { if (*p=='"'||*p=='\\') out[ol++]='\\'; out[ol++]=*p; }
        out[ol++] = '"';
    }
    ol += (size_t)snprintf(out + ol, oc - ol, ",\"action\":\"%s\",\"new_path\":",
                           archive ? "archived" : "delete");
    {
        const char *p = archive ? new_path : "";
        out[ol++] = '"';
        for (; *p; p++) { if (*p=='"'||*p=='\\') out[ol++]='\\'; out[ol++]=*p; }
        out[ol++] = '"';
    }
    ol += (size_t)snprintf(out + ol, oc - ol, "}");
    out[ol] = '\0';

    free(bd); free(home); free(new_path); free(normed);
    return out;
}
