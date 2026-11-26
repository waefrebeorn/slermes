/*
 * file_search.c — `search_files`: content search and file find, in-process.
 *
 * The previous implementation built a shell command string and ran it through popen():
 *
 *     grep -rn '<pattern>' '<path>' 2>/dev/null | head -50
 *
 * A native Windows binary has no POSIX PATH, so `grep` was not found; `2>/dev/null` swallowed the
 * shell's complaint, the output was empty, and the tool answered "no matches, match_count: 0" for a
 * pattern that is all over the tree. A search that cannot run must not look like a search that found
 * nothing -- that is the difference between a model concluding "this code does not exist" and the truth.
 *
 * It also ignored two arguments its own description advertised (target=content|files and
 * output_mode=content|files_only|count) and reported match_count as 0 or 1 rather than a count.
 *
 * This version walks the tree itself: POSIX regex for content, fnmatch for names, no subprocess, no
 * shell quoting, and a count that is a count.
 */
#include "file_search.h"

#include "hermes_json.h"

#include "hermes_regex.h"   /* the project's regex wrapper; a raw <regex.h> here resolves to a
                             * compat header whose struct does not match the linked implementation */

#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FS_MAX_FILE_BYTES (4 * 1024 * 1024)
#define FS_MAX_LINE       4096
#define FS_MAX_DEPTH      32

typedef struct {
    char  *buf;
    size_t len, cap;
    int    oom;
} fs_sb;

static void sb_add(fs_sb *b, const char *s, size_t n) {
    if (b->oom || !s) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        while (cap < b->len + n + 1) cap *= 2;
        char *p = (char *)realloc(b->buf, cap);
        if (!p) { b->oom = 1; return; }
        b->buf = p;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

/* A directory we never want to descend into: version control, build output, package caches. */
static int skip_dir(const char *name) {
    static const char *skip[] = {
        ".git", "node_modules", "__pycache__", ".venv", "venv", ".mypy_cache",
        ".pytest_cache", ".testbin", "build", ".cache", "dist", ".idea", NULL,
    };
    for (size_t i = 0; skip[i]; i++)
        if (strcmp(name, skip[i]) == 0) return 1;
    return 0;
}

static int looks_binary(const char *buf, size_t n) {
    size_t probe = n < 512 ? n : 512;
    for (size_t i = 0; i < probe; i++)
        if ((unsigned char)buf[i] == 0) return 1;
    return 0;
}

static char *read_whole(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size < 0 || size > FS_MAX_FILE_BYTES) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';
    *out_len = got;
    return buf;
}

typedef struct {
    hregex_t     *re;
    int           regex_valid;
    int           content_target;  /* target == "content": walk decides by THIS, not by output_mode */
    const char   *glob;
    int           want_content;   /* output_mode == "content" */
    int           want_count;     /* output_mode == "count" */
    long          limit;          /* max matches (content) or max paths (files) */
    fs_sb        *out;
    long          count;
    long          files;
    int           truncated;
} fs_ctx;

/* One file: match its lines. Emits "path:line: text" or counts. */
static void search_file(fs_ctx *c, const char *path, const char *label) {
    size_t n = 0;
    char *buf = read_whole(path, &n);
    if (!buf) return;
    if (looks_binary(buf, n)) { free(buf); return; }

    int file_hits = 0;
    size_t pos = 0;
    long lineno = 0;
    char line[FS_MAX_LINE];

    while (pos < n) {
        size_t start = pos;
        while (pos < n && buf[pos] != '\n') pos++;
        size_t len = pos - start;
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, buf + start, len);
        line[len] = '\0';
        lineno++;
        if (pos < n) pos++;   /* skip the newline */

        if (!c->regex_valid) continue;
        regex_match_t *m = regex_search(c->re, line);
        int hit = (m && m->matched);
        if (m) regex_match_free(m);
        if (!hit) continue;

        /* In content mode the limit is on matches REPORTED, so the check comes before the count: a
         * rejected match must not make match_count disagree with the lines actually returned. */
        if (c->want_content && c->count >= c->limit) { c->truncated = 1; break; }

        file_hits++;
        c->count++;
        if (c->want_count) { if (c->count >= c->limit * 1000) break; continue; }

        if (c->want_content) {
            char head[64];
            snprintf(head, sizeof(head), "%ld:", lineno);
            sb_add(c->out, label, strlen(label));
            sb_add(c->out, ":", 1);
            sb_add(c->out, head, strlen(head));
            sb_add(c->out, line, strlen(line));
            sb_add(c->out, "\n", 1);
        }
        if (c->count >= c->limit && !c->want_content) break;
    }

    if (file_hits && !c->want_content && !c->want_count) {
        sb_add(c->out, label, strlen(label));
        sb_add(c->out, "\n", 1);
    }
    if (file_hits) c->files++;
    free(buf);
}

/* Files mode: match the NAME, emit relative paths (the tool doubles as `ls`). */
static void may_match_name(fs_ctx *c, const char *label, const char *name) {
    if (c->glob && *c->glob && fnmatch(c->glob, name, FNM_PATHNAME) != 0) return;
    if (c->count >= c->limit) { c->truncated = 1; return; }
    sb_add(c->out, label, strlen(label));
    sb_add(c->out, "\n", 1);
    c->count++;
    c->files++;
}

static void walk(fs_ctx *c, const char *dir, const char *rel, int depth) {
    if (depth > FS_MAX_DEPTH || c->count >= c->limit * 4) return;
    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        char full[4096], label[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (rel && *rel) snprintf(label, sizeof(label), "%s/%s", rel, name);
        else snprintf(label, sizeof(label), "%s", name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (skip_dir(name)) continue;
            walk(c, full, label, depth + 1);
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;

        if (c->glob && *c->glob) {
            /* A glob with a slash matches against the relative path, otherwise the basename. */
            const char *subject = strchr(c->glob, '/') ? label : name;
            /* FNM_PATHNAME: '*' must not cross a directory boundary, or "sub/*.c" would also match
             * sub/deep/delta.c and the glob would mean something different from every other tool. */
            if (fnmatch(c->glob, subject, FNM_PATHNAME) != 0) continue;
        }

        if (c->content_target) search_file(c, full, label);
        else may_match_name(c, label, name);
    }
    closedir(d);
}

char *file_search_run(const char *args_json) {
    json_t *args = args_json && *args_json ? json_parse(args_json, NULL) : json_object();
    if (!args) return strdup("{\"error\":\"JSON parse\"}");

    const char *pattern = json_string_value_safe(json_obj_get(args, "pattern"));
    const char *path    = json_string_value_safe(json_obj_get(args, "path"));
    const char *glob    = json_string_value_safe(json_obj_get(args, "file_glob"));
    const char *target  = json_string_value_safe(json_obj_get(args, "target"));
    const char *mode    = json_string_value_safe(json_obj_get(args, "output_mode"));
    double limit_n      = json_get_num(args, "limit", 0);

    if (!pattern || !*pattern) {
        json_free(args);
        return strdup("{\"error\":\"Missing pattern\"}");
    }
    if (!path || !*path) path = ".";
    if (!target || !*target) target = "content";
    if (!mode || !*mode) mode = "content";

    long limit = (long)(limit_n > 0 ? limit_n : (strcmp(target, "files") == 0 ? 200 : 50));

    fs_ctx c;
    memset(&c, 0, sizeof(c));
    c.glob = (glob && *glob) ? glob : NULL;
    c.limit = limit;
    c.content_target = (strcmp(target, "content") == 0);
    c.want_content = (strcmp(target, "content") == 0) && (strcmp(mode, "content") == 0);
    c.want_count   = (strcmp(target, "content") == 0) && (strcmp(mode, "count") == 0);

    fs_sb out;
    memset(&out, 0, sizeof(out));
    c.out = &out;   /* the context writes into this buffer; leaving it NULL crashed on the first match */

    int compile_failed = 0;
    if (strcmp(target, "content") == 0) {
        c.re = regex_compile(pattern, 0);
        if (!c.re) compile_failed = 1;
        else c.regex_valid = 1;
    }

    if (compile_failed) {
        /* `pattern` points into args: freeing args first made this the crash path. */
        json_t *err = json_object();
        json_set(err, "error", json_string("invalid regular expression"));
        json_set(err, "pattern", json_string(pattern));
        char *s = json_serialize(err);
        json_free(err);
        json_free(args);
        return s;
    }

    walk(&c, path, "", 0);
    if (c.re) regex_free(c.re);

    json_t *res = json_object();
    json_set(res, "pattern", json_string(pattern));
    json_set(res, "path", json_string(path));
    json_set(res, "target", json_string(target));
    json_set(res, "output_mode", json_string(mode));
    json_set(res, "match_count", json_number((double)c.count));
    json_set(res, "file_count", json_number((double)c.files));
    json_set(res, "truncated", json_bool(c.truncated));
    if (c.want_count) {
        /* count mode: one line per file is what a caller sums; the numeric count is above. */
        json_set(res, "matches", json_string(""));
    } else {
        json_set(res, "matches", json_string(out.buf ? out.buf : ""));
    }
    if (out.oom) json_set(res, "warning", json_string("output truncated by memory"));

    char *s = json_serialize(res);
    json_free(res);
    free(out.buf);
    json_free(args);
    return s;
}
