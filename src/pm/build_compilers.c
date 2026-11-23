/*
 * build_compilers.c — C port of pm/build_compilers.py (gate w6_buildcompil_8eb661).
 * PoP: w6_buildcompil_8eb661_missing_compiler_remedy @ pm/build_compilers.py:missing_compiler_remedy
 * PoP: w6_buildcompil_8eb661_linux_defaults @ pm/build_compilers.py:_linux_defaults
 * PoP: w6_buildcompil_8eb661_pinned_build_defaults @ pm/build_compilers.py:pinned_build_defaults
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_buildcompil_8eb661.h"

#define REMEDY_TAIL ": install a C/C++ compiler (Debian/Ubuntu/WSL: `sudo apt install build-essential`; Fedora: `sudo dnf install gcc gcc-c++`; macOS: `xcode-select --install`) or point CC/CXX at one, then retry"

static char *dupstr(const char *s) { return s ? strdup(s) : NULL; }
static char *remedy(const char *what) {
    size_t n = strlen(what) + strlen(REMEDY_TAIL) + 1;
    char *r = (char *)malloc(n);
    if (r) snprintf(r, n, "%s%s", what, REMEDY_TAIL);
    return r;
}
/* f"the build needs `{name}`, which is not on PATH" */
static char *need_remedy(const char *name) {
    size_t n = strlen(name) + strlen("the build needs `") + strlen("`, which is not on PATH") + 1;
    char *what = (char *)malloc(n);
    if (!what) return NULL;
    snprintf(what, n, "the build needs `%s`, which is not on PATH", name);
    char *r = remedy(what);
    free(what);
    return r;
}

/* Path(match.group(1)).name — split on either separator (Windows pathlib). */
static const char *path_name(const char *p) {
    const char *b = p;
    for (const char *q = p; *q; q++) if (*q == '/' || *q == '\\') b = q + 1;
    return b;
}
/* _COMPILER_NAME.fullmatch(name): clang|clang++|cc|gcc|g++|c++ plus -[0-9.]+ */
static int compiler_name(const char *name) {
    char base[256];
    const char *dash = strchr(name, '-');
    if (dash) {
        size_t n = (size_t)(dash - name);
        if (!n || n >= sizeof(base)) return 0;
        memcpy(base, name, n); base[n] = 0;
        const char *suf = dash + 1;
        if (!*suf) return 0;
        for (const char *q = suf; *q; q++)
            if (!((*q >= '0' && *q <= '9') || *q == '.')) return 0;
    } else {
        if (strlen(name) >= sizeof(base)) return 0;
        strcpy(base, name);
    }
    return !strcmp(base, "clang") || !strcmp(base, "clang++") || !strcmp(base, "cc") ||
           !strcmp(base, "gcc") || !strcmp(base, "g++") || !strcmp(base, "c++");
}

char *w6_buildcompil_8eb661_missing_compiler_remedy(const char *output) {
    if (!output) output = "";
    char cap[1024];
    /* No such file or directory: '([^']+)' */
    const char *p = output;
    while ((p = strstr(p, "No such file or directory: '"))) {
        const char *start = p + strlen("No such file or directory: '");
        const char *end = strchr(start, '\'');
        if (end && end > start) {
            size_t n = (size_t)(end - start);
            if (n >= sizeof(cap)) n = sizeof(cap) - 1;
            memcpy(cap, start, n); cap[n] = 0;
            if (compiler_name(path_name(cap)))
                return need_remedy(cap);
        }
        p++;
    }
    /* command '([^']+)' failed: No such file or directory */
    p = output;
    while ((p = strstr(p, "command '"))) {
        const char *start = p + strlen("command '");
        const char *end = strchr(start, '\'');
        if (end && end > start && !strncmp(end + 1, " failed: No such file or directory",
                                          strlen(" failed: No such file or directory"))) {
            size_t n = (size_t)(end - start);
            if (n >= sizeof(cap)) n = sizeof(cap) - 1;
            memcpy(cap, start, n); cap[n] = 0;
            if (compiler_name(path_name(cap))) return need_remedy(cap);
        }
        p++;
    }
    /* No CMAKE_(?:C|CXX)_COMPILER could be found (no capture -> generic) */
    if (strstr(output, "No CMAKE_C_COMPILER could be found") ||
        strstr(output, "No CMAKE_CXX_COMPILER could be found"))
        return remedy("this package compiles native code and no compiler was found");
    return dupstr("");
}

static const char *jstr(const struct json_t *o, const char *key) {
    const struct json_t *v = o ? json_obj_get(o, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static int has_key(const struct json_t *o, const char *key) {
    return o && o->type == JSON_OBJECT && json_obj_get(o, key) != NULL;
}
static int available_has(const struct json_t *av, const char *prog) {
    if (!av || av->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(av); i++) {
        const struct json_t *v = json_get(av, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, prog)) return 1;
    }
    return 0;
}
/* substitute + command[len(program):], where program = command.split(" ",1)[0]. */
static char *rewrite(const char *substitute, const char *command) {
    const char *sp = strchr(command, ' ');
    const char *rest = sp ? sp : command + strlen(command);
    size_t n = strlen(substitute) + strlen(rest) + 1;
    char *r = (char *)malloc(n);
    if (r) snprintf(r, n, "%s%s", substitute, rest);
    return r;
}
static const char *gcc_family(const char *prog) {
    if (!strcmp(prog, "clang")) return "gcc";
    if (!strcmp(prog, "clang++")) return "g++";
    return NULL;
}

struct json_t *w6_buildcompil_8eb661_linux_defaults(const struct json_t *recorded,
        const struct json_t *env, const struct json_t *available) {
    struct json_t *out = json_object();
    static const char *const cvars[] = { "CC", "CXX" };
    static const char *const lvars[] = { "LDSHARED", "LDCXXSHARED" };
    for (int i = 0; i < 2; i++) {
        const char *command = jstr(recorded, cvars[i]);
        char prog[512];
        const char *sp = strchr(command, ' ');
        size_t pn = sp ? (size_t)(sp - command) : strlen(command);
        if (pn >= sizeof(prog)) pn = sizeof(prog) - 1;
        memcpy(prog, command, pn); prog[pn] = 0;
        const char *sub = gcc_family(prog);
        if (has_key(env, cvars[i]) || !sub || available_has(available, prog) ||
            !available_has(available, sub))
            continue;
        char *v = rewrite(sub, command);
        json_set(out, cvars[i], json_string(v ? v : "")); free(v);
        const char *linker = jstr(recorded, lvars[i]);
        /* linker.split(" ",1)[0] == program */
        char lprog[512];
        const char *lsp = strchr(linker, ' ');
        size_t ln = lsp ? (size_t)(lsp - linker) : strlen(linker);
        if (ln >= sizeof(lprog)) ln = sizeof(lprog) - 1;
        memcpy(lprog, linker, ln); lprog[ln] = 0;
        if (!has_key(env, lvars[i]) && !strcmp(lprog, prog)) {
            char *lv = rewrite(sub, linker);
            json_set(out, lvars[i], json_string(lv ? lv : "")); free(lv);
        }
    }
    const char *archiver = jstr(recorded, "AR");
    if (*archiver && !available_has(available, archiver) && available_has(available, "ar"))
        json_set(out, "AR", json_string("ar"));
    return out;
}

struct json_t *w6_buildcompil_8eb661_pinned_build_defaults(const char *platform,
        const struct json_t *recorded, const struct json_t *env, const struct json_t *available) {
    if (!platform) platform = "";
    if (!strcmp(platform, "darwin")) {
        struct json_t *o = json_object();
        json_set(o, "AR", json_string("/usr/bin/ar"));
        return o;
    }
    if (strcmp(platform, "linux")) return json_object();
    return w6_buildcompil_8eb661_linux_defaults(recorded, env, available);
}
