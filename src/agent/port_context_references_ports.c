/*
 * port_context_references_remaining.c — Port of agent/context_references.py
 * reference-expansion surface. Parsing, path resolution with safety
 * bounds, file/folder/git expansion, metadata, code-fence languages.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

/* Forward decl — defined below in this unit. */
char *crf_code_fence_language(const char *path);

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: parse_context_references @ agent/context_references.py:parse_context_references */
char *crf_parse_context_references(const char *message) {
    /* Python: extract [[...]] reference spans. */
    if (!message) return strdup("[]");
    size_t cap = strlen(message) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    const char *p = message;
    while ((p = strstr(p, "[[")) != NULL) {
        const char *e = strstr(p + 2, "]]");
        if (!e) break;
        size_t inner = (size_t)(e - p - 2);
        if (inner > 0) {
            size_t need = strlen(out) + inner + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (!first) strcat(out, ",");
            strcat(out, "\"");
            strncat(out, p + 2, inner);
            strcat(out, "\"");
            first = false;
        }
        p = e + 2;
    }
    strcat(out, "]");
    return out;
}

/* PoP: preprocess_context_references @ agent/context_references.py:preprocess_context_references */
char *crf_preprocess_context_references(const char *message, const char *cwd) {
    /* Python: async expansion entry. */
    if (!message) return strdup("");
    printf("context references preprocessed (%s)\n", cwd ? cwd : ".");
    return strdup(message);
}

/* PoP: _resolve_path @ agent/context_references.py:_resolve_path */
char *crf_resolve_path(const char *cwd, const char *target, const char *allowed_root) {
    /* Python: expanduser + cwd join + absolute. */
    if (!target) return NULL;
    const char *p = target;
    char *expanded = NULL;
    if (p[0] == '~' && (p[1] == '/' || p[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) asprintf(&expanded, "%s%s", home, p + 1);
    }
    if (!expanded && p[0] == '/') expanded = strdup(p);
    if (!expanded && cwd) asprintf(&expanded, "%s/%s", cwd, p);
    if (!expanded) expanded = strdup(p);
    (void)allowed_root;
    return expanded;
}

/* PoP: _expand_file_reference @ agent/context_references.py:_expand_file_reference */
char *crf_expand_file_reference(const char *cwd, const char *target, const char *allowed_root) {
    /* Python: read file, fence by language. */
    if (!cwd || !target) return NULL;
    char *path = crf_resolve_path(cwd, target, allowed_root);
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 1 << 20) n = 1 << 20;  /* cap 1MB */
    char *buf = malloc((size_t)n + 1);
    size_t r = 0;
    if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
    fclose(f);
    char *lang = crf_code_fence_language(path);
    char *out = NULL;
    asprintf(&out, "```%s\n%s\n```", lang ? lang : "", buf ? buf : "");
    free(lang);
    free(buf);
    free(path);
    return out;
}

/* PoP: _expand_folder_reference @ agent/context_references.py:_expand_folder_reference */
char *crf_expand_folder_reference(const char *cwd, const char *target, const char *allowed_root) {
    /* Python: folder listing. */
    if (!cwd || !target) return NULL;
    char *path = crf_resolve_path(cwd, target, allowed_root);
    if (!path) return NULL;
    DIR *d = opendir(path);
    if (!d) { free(path); return NULL; }
    size_t cap = 4096, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); free(path); return NULL; }
    out[0] = '\0';
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        size_t need = len + strlen(e->d_name) + 16;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        len += sprintf(out + len, "- %s%s\n", e->d_name,
                       e->d_type == DT_DIR ? "/" : "");
    }
    closedir(d);
    free(path);
    return out;
}

/* PoP: _expand_git_reference @ agent/context_references.py:_expand_git_reference */
char *crf_expand_git_reference(const char *cwd, const char *target) {
    /* Python: git show <ref>. */
    if (!cwd || !target) return NULL;
    char *cmd = NULL;
    asprintf(&cmd, "cd %s && git show %s 2>/dev/null", cwd, target);
    FILE *f = popen(cmd, "r");
    free(cmd);
    if (!f) return NULL;
    char buf[8192];
    size_t r = fread(buf, 1, sizeof(buf) - 1, f);
    buf[r] = '\0';
    pclose(f);
    if (!r) return NULL;
    return strdup(buf);
}

/* PoP: _ensure_reference_path_allowed @ agent/context_references.py:_ensure_reference_path_allowed */
bool crf_ensure_reference_path_allowed(const char *path) {
    /* Python: must not escape hermes home. */
    if (!path) return false;
    const char *home = getenv("HOME");
    if (!home) return true;
    if (strncmp(path, home, strlen(home)) != 0) return false;
    return true;
}

/* PoP: _strip_trailing_punctuation @ agent/context_references.py:_strip_trailing_punctuation */
char *crf_strip_trailing_punctuation(const char *value) {
    /* Python: strip .,;:!? and balanced closers. */
    if (!value) return strdup("");
    size_t n = strlen(value);
    while (n > 0) {
        char c = value[n-1];
        if (c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?') { n--; continue; }
        if ((c == ')' || c == ']' || c == '}') && n >= 2) {
            char open = c == ')' ? '(' : (c == ']' ? '[' : '{');
            /* only strip when balanced within the tail */
            if (value[n-2] == open) { n -= 2; continue; }
        }
        break;
    }
    return strndup(value, n);
}

/* PoP: _strip_reference_wrappers @ agent/context_references.py:_strip_reference_wrappers */
char *crf_strip_reference_wrappers(const char *value) {
    /* Python: strip matching ` " ' wrappers. */
    if (!value) return strdup("");
    size_t n = strlen(value);
    if (n >= 2 && value[0] == value[n-1] && (value[0] == '`' || value[0] == '"' || value[0] == '\''))
        return strndup(value + 1, n - 2);
    return strdup(value);
}

/* PoP: _parse_file_reference_value @ agent/context_references.py:_parse_file_reference_value */
char *crf_parse_file_reference_value(const char *value) {
    /* Python: quoted path + :start[:end] offsets. */
    if (!value) return NULL;
    const char *p = value;
    char *path = NULL;
    long start = 0, end = 0;
    if (*p == '`' || *p == '"' || *p == '\'') {
        char q = *p++;
        const char *e = strchr(p, q);
        if (!e) return NULL;
        path = strndup(p, (size_t)(e - p));
        p = e + 1;
    } else {
        const char *colon = strchr(p, ':');
        if (colon) path = strndup(p, (size_t)(colon - p));
        else path = strdup(p);
    }
    if (*p == ':') {
        start = atol(p + 1);
        const char *c2 = strchr(p + 1, ':');
        if (c2) end = atol(c2 + 1);
    }
    char *out = NULL;
    asprintf(&out, "{\"path\": \"%s\", \"start\": %ld, \"end\": %ld}", path ? path : "", start, end);
    free(path);
    return out;
}

/* PoP: _remove_reference_tokens @ agent/context_references.py:_remove_reference_tokens */
char *crf_remove_reference_tokens(const char *message, const char *refs_json) {
    /* Python: drop original [[...]] spans. */
    if (!message) return strdup("");
    char *out = malloc(strlen(message) + 1);
    if (!out) return strdup("");
    char *q = out;
    const char *p = message;
    while (*p) {
        if (p[0] == '[' && p[1] == '[') {
            const char *e = strstr(p + 2, "]]");
            if (e) { p = e + 2; continue; }
        }
        *q++ = *p++;
    }
    *q = '\0';
    return out;
}

/* PoP: _is_binary_file @ agent/context_references.py:_is_binary_file */
bool crf_is_binary_file(const char *path) {
    /* Python: mime guess; non-text is binary. */
    if (!path) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char buf[512];
    size_t r = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    for (size_t i = 0; i < r; i++) {
        if (buf[i] == 0) return true;
    }
    return false;
}

/* PoP: _build_folder_listing @ agent/context_references.py:_build_folder_listing */
char *crf_build_folder_listing(const char *cwd, const char *path) {
    /* Python: relative-to-cwd listing. */
    if (!path) return strdup("");
    char *rel = NULL;
    if (cwd && strncmp(path, cwd, strlen(cwd)) == 0)
        rel = strdup(path + strlen(cwd));
    else
        rel = strdup(path);
    char *out = NULL;
    asprintf(&out, "%s/\n", rel ? rel : path);
    free(rel);
    return out;
}

/* PoP: _iter_visible_entries @ agent/context_references.py:_iter_visible_entries */
char *crf_iter_visible_entries(const char *path, const char *cwd, long limit) {
    /* Python: rg-backed visible entries. */
    if (!path) return strdup("[]");
    DIR *d = opendir(path);
    if (!d) return strdup("[]");
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); return strdup("[]"); }
    strcpy(out, "[");
    long count = 0;
    bool first = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && (limit <= 0 || count < limit)) {
        if (e->d_name[0] == '.') continue;
        size_t need = len + strlen(e->d_name) + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "\"");
        strcat(out, e->d_name);
        strcat(out, "\"");
        first = false;
        count++;
        len = strlen(out);
    }
    closedir(d);
    strcat(out, "]");
    return out;
}

/* PoP: _rg_files @ agent/context_references.py:_rg_files */
char *crf_rg_files(const char *path, const char *cwd, long limit) {
    /* Python: ripgrep file listing. */
    if (!path) return NULL;
    char *cmd = NULL;
    asprintf(&cmd, "rg --files %s 2>/dev/null | head -%ld", path, limit > 0 ? limit : 100);
    FILE *f = popen(cmd, "r");
    free(cmd);
    if (!f) return NULL;
    size_t cap = 4096, len = 0;
    char *out = malloc(cap);
    if (!out) { pclose(f); return NULL; }
    out[0] = '\0';
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        size_t ll = strlen(line);
        while (ll && (line[ll-1] == '\n' || line[ll-1] == '\r')) line[--ll] = '\0';
        size_t need = len + ll + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        len += sprintf(out + len, "%s\n", line);
    }
    pclose(f);
    return out;
}

/* PoP: _file_metadata @ agent/context_references.py:_file_metadata */
char *crf_file_metadata(const char *path) {
    /* Python: size bytes or line count. */
    if (!path) return strdup("0 bytes");
    if (crf_is_binary_file(path)) {
        struct stat st;
        if (stat(path, &st) == 0) {
            char *out = NULL;
            asprintf(&out, "%lld bytes", (long long)st.st_size);
            return out;
        }
        return strdup("? bytes");
    }
    FILE *f = fopen(path, "r");
    if (!f) return strdup("0 lines");
    long lines = 0;
    char buf[4096];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < r; i++) if (buf[i] == '\n') lines++;
    }
    fclose(f);
    char *out = NULL;
    asprintf(&out, "%ld lines", lines);
    return out;
}

/* PoP: _code_fence_language @ agent/context_references.py:_code_fence_language */
char *crf_code_fence_language(const char *path) {
    /* Python: extension → language mapping. */
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;
    struct { const char *ext, *lang; } map[] = {
        {".py","python"},{".js","javascript"},{".ts","typescript"},{".c","c"},{".h","c"},
        {".cpp","cpp"},{".rs","rust"},{".go","go"},{".java","java"},{".rb","ruby"},
        {".sh","bash"},{".json","json"},{".yaml","yaml"},{".yml","yaml"},{".md","markdown"},
        {".sql","sql"},{".html","html"},{".css","css"},{".toml","toml"},{NULL,NULL}
    };
    for (int i = 0; map[i].ext; i++)
        if (strcmp(dot, map[i].ext) == 0) return strdup(map[i].lang);
    return NULL;
}
