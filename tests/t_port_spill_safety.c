/*
 * t_port_spill_safety.c — parity harness for src/tools/spill_safety.c
 * (tools/spill_safety.py). Reads a JSON fixture with "ops" and emits one JSON
 * line per op plus a final filesystem listing, mirroring
 * tests/sta_oracle_spill_safety.py so outputs can be diffed.
 *
 * Sandbox: both sides wipe and recreate
 *   tests/oracle/build/spill_safety_ws/<case>/  before running ops,
 * so whichever side runs second starts from the same empty state.
 */
#include "hermes_json.h"
#include "spill_safety.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>

/* Recursive delete, in-process. The harness used to shell out:
 *   system("sh -c \"rm -rf '<root>' && mkdir -p '<root>'\"")
 * On Windows system() goes through cmd.exe, whose PATH resolves `sh` to a
 * denied WindowsApps alias stub -- the command died with "Access is denied"
 * and the port flaked to NO-OUTPUT. Doing it here removes the shell (and the
 * stub) from the equation and behaves identically on MSYS2/Linux/macOS. */
static int rm_rf(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;   /* already gone: nothing to do */
    if (!S_ISDIR(st.st_mode)) return unlink(path) == 0 ? 0 : -1;
    DIR *d = opendir(path);
    if (!d) return -1;
    int rc = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char child[4096];
        snprintf(child, sizeof child, "%s/%s", path, e->d_name);
        if (rm_rf(child) != 0) rc = -1;
    }
    closedir(d);
    return rmdir(path) == 0 ? rc : -1;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

static const char *status_name(spill_status_t st) {
    switch (st) {
    case SPILL_OK: return "ok";
    case SPILL_ERR_EXISTS: return "exists";
    case SPILL_ERR_NOT_DIR: return "not_dir";
    case SPILL_ERR_REFUSE_DIR: return "refuse_dir";
    default: return "io_error";
    }
}

/* join sandbox root + rel path */
static void ws_path(char *out, size_t outsz, const char *root, const char *rel) {
    snprintf(out, outsz, "%s/%s", root, rel);
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* recursive listing: append "rel<TAB>type<TAB>mode" lines to a growing array */
static char **g_lines; static size_t g_n, g_cap;
static void add_line(const char *s) {
    if (g_n == g_cap) { g_cap = g_cap ? g_cap * 2 : 64;
        g_lines = realloc(g_lines, g_cap * sizeof(char *)); }
    g_lines[g_n++] = strdup(s);
}
static mode_t g_root_mode;
static void list_dir(const char *base, const char *prefix) {
    DIR *d = opendir(base);
    if (!d) return;
    struct dirent *e;
    char *names[1024]; size_t nn = 0;
    while ((e = readdir(d)) && nn < 1024) {
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
            names[nn++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, nn, sizeof(char *), cmp_str);
    for (size_t i = 0; i < nn; i++) {
        char ppath[4096], rel[4096];
        snprintf(ppath, sizeof ppath, "%s/%s", base, names[i]);
        /* The listing is filesystem evidence and must read like the Python
         * module's, whose `str(Path.relative_to(root))` uses the PLATFORM
         * separator: "\" on Windows, "/" elsewhere. Syscalls below keep "/",
         * which Windows accepts; only the emitted text is platform-shaped. */
        const char *sep = "/";
#ifdef _WIN32
        sep = "\\";
#endif
        if (prefix[0]) snprintf(rel, sizeof rel, "%s%s%s", prefix, sep, names[i]);
        else snprintf(rel, sizeof rel, "%s", names[i]);
        struct stat st;
        if (lstat(ppath, &st) != 0) continue;
        const char *ty = S_ISLNK(st.st_mode) ? "link"
                        : S_ISDIR(st.st_mode) ? "dir"
                        : S_ISREG(st.st_mode) ? "file" : "other";
        char line[4352];
        /* link targets are recorded as "link"; content only for regular files */
        if (S_ISREG(st.st_mode)) {
            FILE *f = fopen(ppath, "rb");
            char content[8192] = "";
            if (f) {
                size_t r = fread(content, 1, sizeof content - 1, f);
                content[r] = '\0'; fclose(f);
            }
            snprintf(line, sizeof line, "%s\tfile\t%o\t%s", rel,
                     (unsigned)(st.st_mode & 07777), content);
        } else {
            snprintf(line, sizeof line, "%s\t%s\t%o", rel, ty,
                     (unsigned)(st.st_mode & 07777));
        }
        add_line(line);
        if (S_ISDIR(st.st_mode)) list_dir(ppath, rel);
        free(names[i]);
    }
}

/* escape a string for embedding in a JSON string literal */
static char *json_escape(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(6 * n + 3);
    char *w = out;
    *w++ = '"';
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '"' || ch == '\\') { *w++ = '\\'; *w++ = (char)ch; }
        else if (ch == '\t') { *w++ = '\\'; *w++ = 't'; }
        else if (ch == '\n') { *w++ = '\\'; *w++ = 'n'; }
        else if (ch == '\r') { *w++ = '\\'; *w++ = 'r'; }
        else *w++ = (char)ch;
    }
    *w++ = '"';
    *w = '\0';
    return out;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *rootj = json_parse(raw, NULL);
    if (!rootj) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }

    /* sandbox root derived from the fixture name */
    char fixcopy[4096]; snprintf(fixcopy, sizeof fixcopy, "%s", argv[1]);
    char *casebase = basename(fixcopy);
    char *dot = strrchr(casebase, '.'); if (dot) *dot = '\0';
    char root[4096];
    snprintf(root, sizeof root, "tests/oracle/build/spill_safety_ws/%s", casebase);

    /* wipe + recreate the sandbox.
     * `system()` on Windows goes through cmd.exe, where `rm` does not exist and
     * single quotes are not quoting -- the command died with "The syntax of the
     * command is incorrect." and the port reported "C produced no output".
     * `sh -c` is the portable form: same command on MSYS2, Linux and macOS. */
    /* wipe + recreate the sandbox, in-process (see rm_rf). */
    if (rm_rf(root) != 0 || mkdir(root, 0755) != 0) {
        fprintf(stderr, "sandbox reset failed\n");
        return 1;
    }

    json_t *ops = json_object_get(rootj, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            json_object_set(o, "kind", json_string(kind));
            char p[4096];
            if (strcmp(kind, "ensure_dir") == 0) {
                ws_path(p, sizeof p, root, json_get_str(c, "path", ""));
                spill_status_t st = spill_safety_ensure_spill_dir(
                    p, json_get_bool(c, "private", 1) ? 1 : 0);
                json_object_set(o, "status", json_string(status_name(st)));
            } else if (strcmp(kind, "write_text") == 0) {
                ws_path(p, sizeof p, root, json_get_str(c, "path", ""));
                spill_status_t st = spill_safety_write_text_exclusive(
                    p, json_get_str(c, "text", ""),
                    json_get_bool(c, "private", 1) ? 1 : 0,
                    json_get_bool(c, "overwrite", 0) ? 1 : 0);
                json_object_set(o, "status", json_string(status_name(st)));
            } else if (strcmp(kind, "open_exclusive") == 0) {
                ws_path(p, sizeof p, root, json_get_str(c, "path", ""));
                spill_status_t st;
                FILE *fh = spill_safety_open_exclusive(
                    p, json_get_bool(c, "private", 1) ? 1 : 0,
                    json_get_bool(c, "overwrite", 0) ? 1 : 0, &st);
                json_object_set(o, "status", json_string(status_name(st)));
                if (fh) {
                    const char *w = json_get_str(c, "write", "");
                    fwrite(w, 1, strlen(w), fh);
                    fclose(fh);
                }
            } else if (strcmp(kind, "symlink") == 0) {
                /* plant a symlink: link -> target (relative to sandbox) */
                char lp[4096], tp[4096];
                ws_path(lp, sizeof lp, root, json_get_str(c, "link", ""));
                ws_path(tp, sizeof tp, root, json_get_str(c, "target", ""));
                int okc = symlink(tp, lp) == 0;
                json_object_set(o, "status", json_string(okc ? "ok" : "io_error"));
            } else if (strcmp(kind, "write_raw") == 0) {
                /* plant a plain file via normal open (fixture setup helper) */
                ws_path(p, sizeof p, root, json_get_str(c, "path", ""));
                FILE *f = fopen(p, "w");
                int okc = f != NULL;
                if (f) { fwrite(json_get_str(c, "text", ""), 1,
                                strlen(json_get_str(c, "text", "")), f); fclose(f); }
                json_object_set(o, "status", json_string(okc ? "ok" : "io_error"));
            } else {
                json_object_set(o, "status", json_string("unknown"));
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }

    /* final listing — deterministic filesystem evidence */
    add_line("--- listing ---");
    struct stat rst;
    if (lstat(root, &rst) == 0) {
        char lm[256];
        snprintf(lm, sizeof lm, ".\tdir\t%o", (unsigned)(rst.st_mode & 07777));
        add_line(lm);
    }
    list_dir(root, "");
    qsort(g_lines + 1, g_n > 1 ? g_n - 1 : 0, sizeof(char *), cmp_str);
    for (size_t i = 0; i < g_n; i++) {
        char *esc = json_escape(g_lines[i]);
        printf("{\"listing\": %s}\n", esc);
        free(esc);
    }
    (void)g_root_mode;
    json_free(rootj);
    free(raw);
    return 0;
}
