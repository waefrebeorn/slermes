/*
 * port_web_server_fs.c — faithful C11 port of the /api/fs endpoint cluster
 * in hermes_cli/web_server.py (fs_list, fs_read_text, fs_write_text,
 * fs_read_data_url, fs_default_cwd, _fs_git_branch + the preview-language
 * and readdir-hidden tables).
 *
 * Reuses (no duplication): ws_fs_path / ws_fs_regular_file / ws_fs_mime_type /
 * ws_fs_looks_binary / ws_fs_find_git_root from port_web_server_schema_path.c
 * via hermes_web_server_pure.h, and libbase64.
 */

#include "web_server_fs.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "hermes_web_server_pure.h"
#include "base64.h"

/* ── Python _FS_READDIR_HIDDEN ───────────────────────────────────────────── */
/* PoP: ws_fs_readdir_hidden @ hermes_cli/web_server.py:_FS_READDIR_HIDDEN */
bool ws_fs_readdir_hidden(const char *name) {
    static const char *hidden[] = {
        ".git", ".hg", ".svn", ".cache", ".next", ".turbo", ".venv",
        "__pycache__", "build", "dist", "node_modules", "target", "venv",
        NULL,
    };
    if (!name) return false;
    for (int i = 0; hidden[i]; i++)
        if (strcmp(name, hidden[i]) == 0) return true;
    return false;
}

/* ── Python _FS_PREVIEW_LANGUAGE_BY_EXT ─────────────────────────────────── */
/* PoP: ws_fs_preview_language @ hermes_cli/web_server.py:_FS_PREVIEW_LANGUAGE_BY_EXT */
const char *ws_fs_preview_language(const char *path) {
    static const struct { const char *ext; const char *lang; } t[] = {
        {".c","c"},{".conf","ini"},{".cpp","cpp"},{".css","css"},{".csv","csv"},
        {".go","go"},{".graphql","graphql"},{".h","c"},{".hpp","cpp"},
        {".html","html"},{".java","java"},{".js","javascript"},{".json","json"},
        {".jsx","jsx"},{".kt","kotlin"},{".lua","lua"},{".md","markdown"},
        {".mjs","javascript"},{".py","python"},{".rb","ruby"},{".rs","rust"},
        {".sh","shell"},{".sql","sql"},{".svg","xml"},{".toml","toml"},
        {".ts","typescript"},{".tsx","tsx"},{".txt","text"},{".xml","xml"},
        {".yaml","yaml"},{".yml","yaml"},{".zsh","shell"},{NULL,NULL},
    };
    const char *dot = path ? strrchr(path, '.') : NULL;
    if (dot) {
        /* suffix.lower() */
        char low[16];
        size_t dl = strlen(dot);
        if (dl < sizeof(low)) {
            for (size_t i = 0; i <= dl; i++)
                low[i] = (char)tolower((unsigned char)dot[i]);
            for (int i = 0; t[i].ext; i++)
                if (strcmp(low, t[i].ext) == 0) return t[i].lang;
        }
    }
    return "text";
}

/* ── fs_list ─────────────────────────────────────────────────────────────── */
static int fs_entry_cmp(const void *a, const void *b) {
    const ws_fs_entry_t *ea = a, *eb = b;
    /* Python key: (not isDirectory, name.lower(), name) */
    if (ea->is_directory != eb->is_directory)
        return ea->is_directory ? -1 : 1;
    int c = strcasecmp(ea->name, eb->name);
    if (c) return c;
    return strcmp(ea->name, eb->name);
}

/* PoP: ws_fs_list @ hermes_cli/web_server.py:fs_list */
ws_fs_entry_t *ws_fs_list(const char *path, size_t *out_n,
                          const char **out_err, bool *out_err_malloced) {
    *out_n = 0;
    *out_err = NULL;
    *out_err_malloced = false;

    char *target = ws_fs_path(path);
    if (!target) { *out_err = "read-error"; return NULL; }

    DIR *d = opendir(target);
    if (!d) {
        switch (errno) {
        case ENOENT:  *out_err = "ENOENT"; break;
        case ENOTDIR: *out_err = "ENOTDIR"; break;
        case EACCES:  *out_err = "EACCES"; break;
        default: {
            const char *se = strerror(errno);
            *out_err = se && se[0] ? strdup(se) : "read-error";
            *out_err_malloced = (se && se[0]);
            break;
        }
        }
        free(target);
        return NULL;
    }

    size_t cap = 64, n = 0;
    ws_fs_entry_t *arr = calloc(cap, sizeof *arr);
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (ws_fs_readdir_hidden(de->d_name))
            continue;
        if (n == cap) {
            cap *= 2;
            arr = realloc(arr, cap * sizeof *arr);
        }
        ws_fs_entry_t *e = &arr[n];
        memset(e, 0, sizeof *e);
        snprintf(e->name, sizeof(e->name), "%s", de->d_name);
        snprintf(e->path, sizeof(e->path), "%s/%s", target, de->d_name);
        /* entry.is_dir(follow_symlinks=False) → lstat S_ISDIR */
        struct stat st;
        e->is_directory = (lstat(e->path, &st) == 0) && S_ISDIR(st.st_mode);
        n++;
    }
    closedir(d);
    free(target);

    qsort(arr, n, sizeof *arr, fs_entry_cmp);
    *out_n = n;
    return arr;
}

/* ── UTF-8 errors="replace" decode ──────────────────────────────────────── */
/* Python bytes.decode("utf-8", errors="replace"): invalid sequences become
 * U+FFFD (EF BF BD) following the maximal-subpart policy. */
static char *utf8_decode_replace(const unsigned char *in, size_t len,
                                 size_t *out_len) {
    /* worst case: every byte → 3-byte U+FFFD */
    char *out = malloc(len * 3 + 1);
    size_t o = 0, i = 0;
    while (i < len) {
        unsigned char b = in[i];
        size_t need = 0;
        unsigned int cp = 0;
        if (b < 0x80) { out[o++] = (char)b; i++; continue; }
        else if ((b & 0xE0) == 0xC0 && b >= 0xC2) { need = 1; cp = b & 0x1F; }
        else if ((b & 0xF0) == 0xE0) { need = 2; cp = b & 0x0F; }
        else if ((b & 0xF8) == 0xF0 && b <= 0xF4) { need = 3; cp = b & 0x07; }
        else { /* invalid lead */ memcpy(out + o, "\xEF\xBF\xBD", 3); o += 3; i++; continue; }

        size_t j = 1;
        bool valid = true;
        for (; j <= need; j++) {
            if (i + j >= len) { valid = false; break; }
            unsigned char c = in[i + j];
            if ((c & 0xC0) != 0x80) { valid = false; break; }
            /* range guards (overlong/surrogate/max) */
            if (j == 1) {
                if (b == 0xE0 && c < 0xA0) { valid = false; break; }
                if (b == 0xED && c > 0x9F) { valid = false; break; }
                if (b == 0xF0 && c < 0x90) { valid = false; break; }
                if (b == 0xF4 && c > 0x8F) { valid = false; break; }
            }
            cp = (cp << 6) | (c & 0x3F);
        }
        if (valid) {
            memcpy(out + o, in + i, need + 1);
            o += need + 1;
            i += need + 1;
        } else {
            /* replace the maximal subpart (lead + valid continuations seen) */
            memcpy(out + o, "\xEF\xBF\xBD", 3);
            o += 3;
            i += j; /* consumed lead + (j-1) continuations */
        }
    }
    out[o] = '\0';
    if (out_len) *out_len = o;
    return out;
}

/* ── fs_read_text ───────────────────────────────────────────────────────── */
static int status_from_path(ws_path_status_t s) {
    switch (s) {
    case WS_PATH_OK:           return 0;
    case WS_PATH_NOT_FOUND:    return 404;
    case WS_PATH_IS_DIR:       return 400;
    case WS_PATH_NOT_REGULAR:  return 400;
    case WS_PATH_NOT_READABLE: return 403;
    default:                   return 400;
    }
}

/* PoP: ws_fs_read_text @ hermes_cli/web_server.py:fs_read_text */
void ws_fs_read_text(const char *path, ws_fs_read_text_t *out) {
    memset(out, 0, sizeof *out);
    out->mime = "application/octet-stream";
    snprintf(out->language, sizeof(out->language), "text");

    char *target = ws_fs_path(path);
    if (!target) { out->status = 400; return; }

    struct stat st;
    ws_path_status_t ps = ws_fs_regular_file(target, &st);
    if (ps != WS_PATH_OK) { out->status = status_from_path(ps); free(target); return; }

    if ((unsigned long long)st.st_size > WS_FS_TEXT_SOURCE_MAX_BYTES) {
        out->status = 413;
        free(target);
        return;
    }
    size_t to_read = st.st_size < (off_t)WS_FS_TEXT_PREVIEW_MAX_BYTES
                         ? (size_t)st.st_size
                         : WS_FS_TEXT_PREVIEW_MAX_BYTES;

    FILE *f = fopen(target, "rb");
    if (!f) {
        out->status = (errno == EACCES) ? 403 : 400;
        free(target);
        return;
    }
    unsigned char *data = malloc(to_read ? to_read : 1);
    size_t got = fread(data, 1, to_read, f);
    fclose(f);

    out->binary = ws_fs_looks_binary(data, got < 4096 ? got : 4096);
    out->byte_size = (long long)st.st_size;
    snprintf(out->language, sizeof(out->language), "%s",
             ws_fs_preview_language(target));
    out->mime = ws_fs_mime_type(target);
    out->truncated = (unsigned long long)st.st_size > WS_FS_TEXT_PREVIEW_MAX_BYTES;
    out->text = utf8_decode_replace(data, got, &out->text_len);
    out->status = 0;
    free(data);
    free(target);
}

/* ── fs_write_text ──────────────────────────────────────────────────────── */
/* PoP: ws_fs_write_text @ hermes_cli/web_server.py:fs_write_text */
int ws_fs_write_text(const char *path, const char *content, size_t content_len) {
    if (!content) { content = ""; content_len = 0; }
    if (content_len > WS_FS_TEXT_WRITE_MAX_BYTES) return 413;

    char *target = ws_fs_path(path);
    if (!target) return 400;

    struct stat st;
    bool exists = (stat(target, &st) == 0);
    if (!exists && errno == EACCES) { free(target); return 403; }
    if (exists && S_ISDIR(st.st_mode)) { free(target); return 400; }
    if (exists && !S_ISREG(st.st_mode)) { free(target); return 400; }

    /* parent must already be a directory */
    char parent[2048];
    snprintf(parent, sizeof(parent), "%s", target);
    char *slash = strrchr(parent, '/');
    const char *basename = target;
    if (slash) {
        basename = slash + 1;
        if (slash == parent) parent[1] = '\0';  /* root */
        else *slash = '\0';
    } else {
        snprintf(parent, sizeof(parent), ".");
    }
    struct stat pst;
    if (stat(parent, &pst) != 0 || !S_ISDIR(pst.st_mode)) {
        free(target);
        return 400;
    }

    /* staged sibling tmp + atomic rename */
    char tmp[2304];
    snprintf(tmp, sizeof(tmp), "%s/.%s.hermes-tmp-%d", parent, basename,
             (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        free(target);
        return (errno == EACCES) ? 403 : 500;
    }
    size_t wrote = fwrite(content, 1, content_len, f);
    if (fclose(f) != 0 || wrote != content_len) {
        unlink(tmp);
        free(target);
        return 500;
    }
    if (rename(tmp, target) != 0) {
        int rc = (errno == EACCES) ? 403 : 500;
        unlink(tmp);
        free(target);
        return rc;
    }
    free(target);
    return 0;
}

/* ── fs_read_data_url ───────────────────────────────────────────────────── */
/* PoP: ws_fs_read_data_url @ hermes_cli/web_server.py:fs_read_data_url */
char *ws_fs_read_data_url(const char *path, int *out_status) {
    *out_status = 0;
    char *target = ws_fs_path(path);
    if (!target) { *out_status = 400; return NULL; }

    struct stat st;
    ws_path_status_t ps = ws_fs_regular_file(target, &st);
    if (ps != WS_PATH_OK) { *out_status = status_from_path(ps); free(target); return NULL; }
    if ((unsigned long long)st.st_size > WS_FS_DATA_URL_MAX_BYTES) {
        *out_status = 413;
        free(target);
        return NULL;
    }

    FILE *f = fopen(target, "rb");
    if (!f) {
        *out_status = (errno == EACCES) ? 403 : 400;
        free(target);
        return NULL;
    }
    unsigned char *data = malloc(st.st_size ? (size_t)st.st_size : 1);
    size_t got = fread(data, 1, (size_t)st.st_size, f);
    fclose(f);

    char *b64 = base64_encode(data, got);
    free(data);
    if (!b64) { *out_status = 500; free(target); return NULL; }

    const char *mime = ws_fs_mime_type(target);
    size_t need = strlen("data:;base64,") + strlen(mime) + strlen(b64) + 1;
    char *url = malloc(need);
    snprintf(url, need, "data:%s;base64,%s", mime, b64);
    free(b64);
    free(target);
    return url;
}

/* ── fs_default_cwd + _fs_git_branch ────────────────────────────────────── */
/* PoP: ws_fs_default_cwd @ hermes_cli/web_server.py:_fs_default_cwd */
void ws_fs_default_cwd(char *out, size_t cap) {
    /* Python: load_config().get("terminal").get("cwd") or TERMINAL_CWD env;
     * reject "."/"auto"/"cwd"; must be an existing dir; else Path.cwd(). */
    const char *raw = getenv("TERMINAL_CWD");
    char cfgbuf[1024] = "";
    /* config first (env is the fallback in Python's `or` chain) — the
     * config loader is heavy; honor the same precedence with the config
     * value when available via SLERMES config env bridge. */
    const char *cfg_cwd = getenv("SLERMES_TERMINAL_CWD");
    if (cfg_cwd && cfg_cwd[0]) {
        snprintf(cfgbuf, sizeof(cfgbuf), "%s", cfg_cwd);
        raw = cfgbuf;
    }
    if (raw && raw[0] &&
        strcmp(raw, ".") != 0 && strcmp(raw, "auto") != 0 &&
        strcmp(raw, "cwd") != 0) {
        /* expanduser + resolve + is_dir */
        char expanded[1024];
        if (raw[0] == '~') {
            const char *h = getenv("HOME");
            snprintf(expanded, sizeof(expanded), "%s%s", h ? h : "", raw + 1);
        } else {
            snprintf(expanded, sizeof(expanded), "%s", raw);
        }
        char resolved[2048];
        if (realpath(expanded, resolved)) {
            struct stat st;
            if (stat(resolved, &st) == 0 && S_ISDIR(st.st_mode)) {
                snprintf(out, cap, "%s", resolved);
                return;
            }
        }
    }
    if (!getcwd(out, cap)) snprintf(out, cap, ".");
}

/* PoP: ws_fs_git_branch @ hermes_cli/web_server.py:_fs_git_branch */
void ws_fs_git_branch(const char *cwd, char *out, size_t cap) {
    out[0] = '\0';
    if (!cwd || !cwd[0]) return;
    /* Python: subprocess.run(["git","-C",cwd,"branch","--show-current"],
     * timeout=2, check=False) — argv exec, NO shell. Mirror with fork/exec
     * so a cwd containing quotes can't inject. */
    int pfd[2];
    if (pipe(pfd) != 0) return;
    pid_t pid = fork();
    if (pid < 0) { close(pfd[0]); close(pfd[1]); return; }
    if (pid == 0) {
        /* child */
        close(pfd[0]);
        dup2(pfd[1], 1);
        close(pfd[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 2); close(devnull); }
        /* timeout=2 via alarm in the child */
        alarm(2);
        execlp("git", "git", "-C", cwd, "branch", "--show-current",
               (char *)NULL);
        _exit(127);
    }
    close(pfd[1]);
    char line[512] = "";
    ssize_t got = read(pfd[0], line, sizeof(line) - 1);
    if (got > 0) line[got] = '\0';
    close(pfd[0]);
    int wstatus = 0;
    waitpid(pid, &wstatus, 0);
    if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
        out[0] = '\0';
        return;
    }
    /* strip() */
    size_t len = strlen(line);
    while (len && (line[len-1] == '\n' || line[len-1] == '\r' ||
                   line[len-1] == ' ' || line[len-1] == '\t')) line[--len] = '\0';
    size_t s = 0;
    while (line[s] == ' ' || line[s] == '\t') s++;
    snprintf(out, cap, "%s", line + s);
}
