/*
 * holders.c - C11 port of upstream persistence/sqlite/_state/holders.py
 * (gate w34_holders_028cc0).
 *
 * PoP: w34_holders_028cc0_looks_like_python_executable @ persistence/sqlite/_state/holders.py:_looks_like_python_executable
 * PoP: w34_holders_028cc0_python_execution_target @ persistence/sqlite/_state/holders.py:_python_execution_target
 * PoP: w34_holders_028cc0_looks_like_hermes @ persistence/sqlite/_state/holders.py:_looks_like_hermes
 * PoP: w34_holders_028cc0_argv_flag_value @ persistence/sqlite/_state/holders.py:_argv_flag_value
 * PoP: w34_holders_028cc0_argv_env_home @ persistence/sqlite/_state/holders.py:_argv_env_home
 * PoP: w34_holders_028cc0_sqlite_family @ persistence/sqlite/_state/holders.py:_sqlite_family
 * PoP: w34_holders_028cc0_norm_path @ persistence/sqlite/_state/holders.py:_norm_path
 * PoP: w34_holders_028cc0_canonical_sqlite_path @ persistence/sqlite/_state/holders.py:canonical_sqlite_path
 * PoP: w34_holders_028cc0_argv_path_tokens @ persistence/sqlite/_state/holders.py:_argv_path_tokens
 * PoP: w34_holders_028cc0_names_other_profile @ persistence/sqlite/_state/holders.py:_names_other_profile
 * PoP: w34_holders_028cc0_argv_home_selection @ persistence/sqlite/_state/holders.py:_argv_home_selection
 * PoP: w34_holders_028cc0_argv_scoped_to_other_home @ persistence/sqlite/_state/holders.py:_argv_scoped_to_other_home
 * PoP: w34_holders_028cc0_describe_holder_pid @ persistence/sqlite/_state/holders.py:describe_holder_pid
 * PoP: w34_holders_028cc0_held_store_refusal @ persistence/sqlite/_state/holders.py:held_store_refusal
 *
 * Seams kept in the caller: the argv source (/proc/<pid>/cmdline, psutil,
 * Windows Restart Manager), the sqlite lock probe, hermes_constants'
 * install-layout resolution (arrives here as install_root / our_profile),
 * profile_cli_selector(), the recovery-docs URL and the holder scan itself
 * (holders arrive as values).  Ported is what those values decide, byte for
 * byte: the argv predicates, OS path identity, the other-instance proof
 * (#92401) and the operator-facing refusal block.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noc_w34_holders_028cc0.h"

#if defined(_WIN32)
#define HS_WIN 1
#else
#define HS_WIN 0
#endif

/* _HERMES_EXECUTABLES / _HERMES_PYTHON_MODULES / _HERMES_PYTHON_SCRIPTS */
static const char *const HS_EXECUTABLES[] = {"hermes", "hermes-agent", "hermes-acp"};
static const char *const HS_MODULES[] = {"acp_adapter", "hermes_cli.main"};
static const char *const HS_SCRIPTS[] = {"hermes_cli/main.py", "run_agent.py"};
/* _HOME_FLAGS / _PROFILE_FLAGS / _STATE_DB_NAMES */
static const char *const HS_HOME_FLAGS[] = {"--hermes-home"};
static const char *const HS_PROFILE_FLAGS[] = {"--profile", "-p"};
static const char *const HS_STATE_DB_NAMES[] = {"state.db", "state.db-wal", "state.db-shm"};

#define HS_COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* ---------------------------------------------------------------- strings */

static char *hs_dup(const char *s) {
    size_t n;
    char *o;
    if (!s) return NULL;
    n = strlen(s);
    o = (char *)malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static int hs_starts(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int hs_ends(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static void hs_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* str.removesuffix: strip once, only at the end. */
static char *hs_removesuffix(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    char *o;
    if (n >= m && strcmp(s + n - m, suffix) == 0) n -= m;
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* Python slice [:limit] counted in code points, never splitting UTF-8. */
static void hs_copy_chars(const char *s, size_t limit, char *out, size_t cap) {
    size_t used = 0, chars = 0;
    if (cap == 0) return;
    while (s && s[used] && chars < limit) {
        size_t step = 1;
        unsigned char c = (unsigned char)s[used];
        if ((c & 0xE0) == 0xC0) step = 2;
        else if ((c & 0xF0) == 0xE0) step = 3;
        else if ((c & 0xF8) == 0xF0) step = 4;
        if (used + step > cap - 1) break;
        used += step;
        chars++;
    }
    if (!s) { out[0] = '\0'; return; }
    memcpy(out, s, used);
    out[used] = '\0';
}

/* ------------------------------------------------------------ grow buffer */

typedef struct {
    char *p;
    size_t n, cap;
    int ok;
} hs_sb;

static void sb_init(hs_sb *b) {
    b->p = (char *)malloc(256);
    b->n = 0;
    b->cap = b->p ? 256 : 0;
    b->ok = b->p != NULL;
    if (b->p) b->p[0] = '\0';
}

static void sb_addn(hs_sb *b, const char *s, size_t n) {
    if (!b->ok || !s) return;
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *grown;
        while (cap < b->n + n + 1) cap *= 2;
        grown = (char *)realloc(b->p, cap);
        if (!grown) { b->ok = 0; return; }
        b->p = grown;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_add(hs_sb *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static void sb_addc(hs_sb *b, char c) { sb_addn(b, &c, 1); }

static void sb_free(hs_sb *b) {
    free(b->p);
    b->p = NULL;
    b->n = b->cap = 0;
    b->ok = 0;
}

/* str.split() + " ".join(): whitespace runs collapse to one space, trimmed. */
static void hs_words_join(hs_sb *out, const char *text) {
    size_t i = 0;
    int first = 1;
    if (!text) return;
    while (text[i]) {
        size_t start;
        while (text[i] && isspace((unsigned char)text[i])) i++;
        if (!text[i]) break;
        start = i;
        while (text[i] && !isspace((unsigned char)text[i])) i++;
        if (!first) sb_addc(out, ' ');
        sb_addn(out, text + start, i - start);
        first = 0;
    }
}

/* ---------------------------------------------------------------- paths */

static int hs_is_sep(int c) {
#if HS_WIN
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

static char hs_sep(void) {
#if HS_WIN
    return '\\';
#else
    return '/';
#endif
}

/* os.path.isabs (ntpath: a bare rooted path is NOT absolute without a drive). */
static int hs_isabs(const char *p) {
#if HS_WIN
    if (isalpha((unsigned char)p[0]) && p[1] == ':' &&
        (p[2] == '\\' || p[2] == '/'))
        return 1;
    return 0;
#else
    return p[0] == '/';
#endif
}

static int hs_rooted(const char *p) {
#if HS_WIN
    return p[0] == '\\' || p[0] == '/';
#else
    return p[0] == '/';
#endif
}

/* os.path.normpath.  A ".." at a root is dropped; a drive prefix is kept. */
static char *hs_normpath(const char *in) {
    size_t n = strlen(in), i = 0, o = 0, k, outn = 0;
    char *work, *comp, *out;
    size_t *starts;
    size_t ncomp = 0;
    int absolute = 0, has_drive = 0;
    char drive[2] = {0, 0};

    if (n == 0) return hs_dup(".");
    work = (char *)malloc(n + 1);
    comp = (char *)malloc(n + 2);
    starts = (size_t *)malloc(sizeof(size_t) * (n + 2));
    out = (char *)malloc(n + 8);
    if (!work || !comp || !starts || !out) {
        free(work); free(comp); free(starts); free(out);
        return hs_dup(in);
    }
    for (i = 0; i < n; i++)
        work[i] = hs_is_sep((unsigned char)in[i]) ? hs_sep() : in[i];
    work[n] = '\0';

    i = 0;
#if HS_WIN
    if (n >= 2 && isalpha((unsigned char)work[0]) && work[1] == ':') {
        has_drive = 1;
        drive[0] = work[0];
        drive[1] = ':';
        i = 2;
    }
#endif
    if (i < n && hs_is_sep((unsigned char)work[i])) {
        absolute = 1;
        while (i < n && hs_is_sep((unsigned char)work[i])) i++;
    }
    while (i < n) {
        size_t start, len;
        while (i < n && hs_is_sep((unsigned char)work[i])) i++;
        if (i >= n) break;
        start = i;
        while (i < n && !hs_is_sep((unsigned char)work[i])) i++;
        len = i - start;
        if (len == 1 && work[start] == '.') continue;
        if (len == 2 && work[start] == '.' && work[start + 1] == '.') {
            if (ncomp > 0) {
                size_t prev = starts[ncomp - 1];
                size_t plen = strlen(comp + prev);
                if (!(plen == 2 && comp[prev] == '.' && comp[prev + 1] == '.')) {
                    ncomp--;
                    continue;
                }
            }
            if (absolute) continue; /* ".." at a root is dropped */
        }
        starts[ncomp++] = o;
        memcpy(comp + o, work + start, len);
        o += len;
        comp[o++] = '\0';
    }

    if (has_drive) {
        out[outn++] = drive[0];
        out[outn++] = drive[1];
    }
    if (absolute) out[outn++] = hs_sep();
    for (k = 0; k < ncomp; k++) {
        size_t l = strlen(comp + starts[k]);
        if (k) out[outn++] = hs_sep();
        memcpy(out + outn, comp + starts[k], l);
        outn += l;
    }
    if (outn == 0) out[outn++] = '.';
    out[outn] = '\0';

    free(work);
    free(comp);
    free(starts);
    return out;
}

/* os.path.normcase: '/' -> '\\' + lowercase under Windows, identity else. */
static char *hs_normcase(const char *s) {
    char *o = hs_dup(s);
    if (!o) return NULL;
#if HS_WIN
    {
        char *p;
        for (p = o; *p; p++) {
            if (*p == '/') *p = '\\';
            else *p = (char)tolower((unsigned char)*p);
        }
    }
#endif
    return o;
}

/* os.path.join(dir, name). */
static char *hs_join(const char *dir, const char *name) {
    size_t dl, nl;
    char *o;
    if (hs_rooted(name) || hs_isabs(name)) return hs_dup(name);
    dl = strlen(dir);
    nl = strlen(name);
    if (dl == 0) return hs_dup(name);
    o = (char *)malloc(dl + nl + 2);
    if (!o) return NULL;
    memcpy(o, dir, dl);
    if (!hs_is_sep((unsigned char)dir[dl - 1])) o[dl++] = hs_sep();
    memcpy(o + dl, name, nl + 1);
    return o;
}

/* os.path.abspath: GetFullPathName resolves a bare root against the cwd DRIVE
 * (and uppercases it) on Windows; POSIX just prefixes the cwd. */
static char *hs_abspath(const char *p) {
    char cwd[4096];
    char *joined, *out;

    if (hs_isabs(p)) {
        out = hs_normpath(p);
    } else if (hs_rooted(p)) {
#if HS_WIN
        size_t pl = strlen(p);
        if (!getcwd(cwd, sizeof cwd)) snprintf(cwd, sizeof cwd, "C:\\");
        /* GetFullPathName anchors a bare root on the cwd's DRIVE, not the cwd */
        joined = (char *)malloc(pl + 3);
        if (joined) {
            joined[0] = (char)toupper((unsigned char)cwd[0]);
            joined[1] = ':';
            memcpy(joined + 2, p, pl + 1);
        }
        out = joined ? hs_normpath(joined) : hs_dup(p);
        free(joined);
#else
        out = hs_normpath(p);
#endif
    } else {
        if (!getcwd(cwd, sizeof cwd)) snprintf(cwd, sizeof cwd, ".");
        joined = hs_join(cwd, p);
        out = joined ? hs_normpath(joined) : hs_dup(p);
        free(joined);
    }
#if HS_WIN
    if (out && isalpha((unsigned char)out[0]) && out[1] == ':')
        out[0] = (char)toupper((unsigned char)out[0]);
#endif
    return out;
}

/* os.path.dirname of a normalized path. */
static char *hs_dirname(const char *p) {
    size_t n = strlen(p), k;
    char *o;
    while (n > 1 && hs_is_sep((unsigned char)p[n - 1])) n--;
    k = n;
    while (k > 0 && !hs_is_sep((unsigned char)p[k - 1])) k--;
    if (k == 0) {
#if HS_WIN
        if (n >= 2 && p[1] == ':') { /* bare drive: "C:" */
            o = (char *)malloc(3);
            if (o) { o[0] = p[0]; o[1] = ':'; o[2] = '\0'; }
            return o;
        }
#endif
        return hs_dup("");
    }
    while (k > 1 && hs_is_sep((unsigned char)p[k - 1])) k--;
    o = (char *)malloc(k + 1);
    if (!o) return NULL;
    memcpy(o, p, k);
    o[k] = '\0';
    return o;
}

/* os.path.basename: text after the last separator (trailing sep -> ""). */
static char *hs_basename(const char *p) {
    size_t n = strlen(p), k;
    const char *last = NULL;
    for (k = 0; k < n; k++)
        if (hs_is_sep((unsigned char)p[k])) last = p + k;
    return hs_dup(last ? last + 1 : p);
}

/* pathlib.Path(p).name: a trailing separator is not a component. */
static char *hs_path_name(const char *p) {
    size_t n = strlen(p);
    char *trimmed, *out;
    while (n > 1 && hs_is_sep((unsigned char)p[n - 1])) n--;
    trimmed = (char *)malloc(n + 1);
    if (!trimmed) return NULL;
    memcpy(trimmed, p, n);
    trimmed[n] = '\0';
    out = hs_basename(trimmed);
    free(trimmed);
    return out;
}

/* --------------------------------------------------------------- json args */

static int hs_argv_len(const json_t *argv) {
    return argv && argv->type == JSON_ARRAY ? (int)json_len(argv) : 0;
}

static const char *hs_argv_at(const json_t *argv, size_t index) {
    const json_t *v = json_get(argv, index);
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    return v->str_val;
}

static json_t *hs_target(const char *kind, const char *value) {
    json_t *o = json_object();
    json_set(o, "kind", json_string(kind));
    json_set(o, "value", json_string(value ? value : ""));
    return o;
}

/* ------------------------------------------------------------- predicates */

int w34_holders_028cc0_looks_like_python_executable(const char *program) {
    char *name;
    size_t i;
    int out = 0;

    if (!program) return 0;
    name = hs_basename(program);
    if (!name) return 0;
    hs_lower(name);
    if (hs_ends(name, ".exe")) name[strlen(name) - 4] = '\0';
    for (i = 0; i < 2 && !out; i++) {
        const char *prefix = i ? "pypy" : "python";
        size_t pl = strlen(prefix);
        if (strncmp(name, prefix, pl) == 0) {
            const char *suffix = name + pl;
            out = 1;
            for (; *suffix; suffix++)
                if (!(isdigit((unsigned char)*suffix) || *suffix == '.')) { out = 0; break; }
        }
    }
    free(name);
    return out;
}

json_t *w34_holders_028cc0_python_execution_target(const json_t *argv) {
    int count = hs_argv_len(argv);
    size_t index = 1;

    while (index < (size_t)count) {
        const char *arg = hs_argv_at(argv, index);
        if (!arg) return json_null(); /* upstream raises on a non-str token */
        if (strcmp(arg, "--") == 0) {
            const char *v;
            index++;
            v = index < (size_t)count ? hs_argv_at(argv, index) : NULL;
            return v ? hs_target("script", v) : json_null();
        }
        if (strcmp(arg, "--check-hash-based-pycs") == 0 || strcmp(arg, "--jit") == 0) {
            index += 2;
            continue;
        }
        if (hs_starts(arg, "--check-hash-based-pycs=") || hs_starts(arg, "--jit=")) {
            index += 1;
            continue;
        }
        if (hs_starts(arg, "--")) {
            index += 1;
            continue;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            const char *options = arg + 1;
            size_t oi = 0, ol = strlen(options);
            int consumed_next = 0;
            while (oi < ol) {
                char option = options[oi];
                const char *attached = options + oi + 1;
                if (option == 'c') return json_null();
                if (option == 'm') {
                    const char *v;
                    if (*attached) return hs_target("module", attached);
                    index++;
                    v = index < (size_t)count ? hs_argv_at(argv, index) : NULL;
                    return v ? hs_target("module", v) : json_null();
                }
                /* _PYTHON_SHORT_OPTIONS_WITH_OPERANDS */
                if (option == 'Q' || option == 'W' || option == 'X') {
                    consumed_next = (*attached == '\0');
                    break;
                }
                oi++;
            }
            index += consumed_next ? 2 : 1;
            continue;
        }
        return hs_target("script", arg);
    }
    return json_null();
}

int w34_holders_028cc0_looks_like_hermes(const json_t *argv) {
    const char *arg0;
    char *program, *normalized;
    json_t *target;
    int out = 0;
    size_t i;

    if (hs_argv_len(argv) == 0) return 0;
    arg0 = hs_argv_at(argv, 0);
    if (!arg0) return 0;
    program = hs_basename(arg0);
    if (!program) return 0;
    hs_lower(program);
    if (hs_ends(program, ".exe")) program[strlen(program) - 4] = '\0';
    for (i = 0; i < HS_COUNT(HS_EXECUTABLES); i++)
        if (strcmp(program, HS_EXECUTABLES[i]) == 0) { free(program); return 1; }
    if (!w34_holders_028cc0_looks_like_python_executable(program)) {
        free(program);
        return 0;
    }
    free(program);

    target = w34_holders_028cc0_python_execution_target(argv);
    if (!target || target->type != JSON_OBJECT) { json_free(target); return 0; }
    normalized = hs_dup(json_get_str(target, "value", ""));
    if (!normalized) { json_free(target); return 0; }
    hs_lower(normalized);
    for (i = 0; normalized[i]; i++)
        if (normalized[i] == '\\') normalized[i] = '/';

    if (strcmp(json_get_str(target, "kind", ""), "module") == 0) {
        for (i = 0; i < HS_COUNT(HS_MODULES); i++)
            if (strcmp(normalized, HS_MODULES[i]) == 0) { out = 1; break; }
    } else {
        for (i = 0; i < HS_COUNT(HS_SCRIPTS) && !out; i++) {
            hs_sb suffix;
            sb_init(&suffix);
            sb_addc(&suffix, '/');
            sb_add(&suffix, HS_SCRIPTS[i]);
            if (strcmp(normalized, HS_SCRIPTS[i]) == 0 ||
                (suffix.ok && hs_ends(normalized, suffix.p)))
                out = 1;
            sb_free(&suffix);
        }
    }
    free(normalized);
    json_free(target);
    return out;
}

/* ------------------------------------------------------------- argv reads */

static char *hs_flag_lookup(const json_t *argv, const char *const *flags, size_t nflags) {
    int count = hs_argv_len(argv), index = 0;
    char *value = NULL;

    while (index < count) {
        const char *token = hs_argv_at(argv, index);
        if (token) {
            size_t f;
            int exact = 0;
            for (f = 0; f < nflags; f++)
                if (strcmp(token, flags[f]) == 0) { exact = 1; break; }
            if (exact && index + 1 < count) {
                const char *next = hs_argv_at(argv, index + 1);
                if (next) {
                    free(value);
                    value = hs_dup(next);
                    index += 2;
                    continue;
                }
            }
            for (f = 0; f < nflags && !exact; f++) {
                size_t fl = strlen(flags[f]);
                if (strncmp(token, flags[f], fl) == 0 && token[fl] == '=') {
                    free(value);
                    value = hs_dup(token + fl + 1);
                    break;
                }
            }
        }
        index += 1;
    }
    return value;
}

static char *hs_env_home(const json_t *argv) {
    int count = hs_argv_len(argv), index;
    for (index = count - 1; index >= 0; index--) {
        const char *token = hs_argv_at(argv, index);
        if (token && hs_starts(token, "HERMES_HOME="))
            return hs_dup(token + strlen("HERMES_HOME="));
    }
    return NULL;
}

json_t *w34_holders_028cc0_argv_flag_value(const json_t *argv, const json_t *flags) {
    const char **names;
    size_t i, n = flags && flags->type == JSON_ARRAY ? json_len(flags) : 0, used = 0;
    char *value;
    json_t *out;

    names = (const char **)calloc(n ? n : 1, sizeof(char *));
    if (!names) return json_null();
    for (i = 0; i < n; i++) {
        const char *f = hs_argv_at(flags, i);
        if (f) names[used++] = f;
    }
    value = hs_flag_lookup(argv, names, used);
    free(names);
    if (!value) return json_null();
    out = json_string(value);
    free(value);
    return out;
}

json_t *w34_holders_028cc0_argv_env_home(const json_t *argv) {
    char *value = hs_env_home(argv);
    json_t *out;
    if (!value) return json_null();
    out = json_string(value);
    free(value);
    return out;
}

json_t *w34_holders_028cc0_sqlite_family(const char *base) {
    json_t *out = json_array();
    const char *b = base ? base : "";
    size_t n = strlen(b), k;
    static const char *const tails[2] = {"-wal", "-shm"};

    json_append(out, json_string(b));
    for (k = 0; k < 2; k++) {
        char *side = (char *)malloc(n + 5);
        if (!side) continue;
        memcpy(side, b, n);
        memcpy(side + n, tails[k], 5);
        json_append(out, json_string(side));
        free(side);
    }
    return out;
}

json_t *w34_holders_028cc0_norm_path(const char *value) {
    char *normed, *cased;
    json_t *out;
    if (!value) return json_null();
    normed = hs_normpath(value);
    if (!normed) return json_null();
    cased = hs_normcase(normed);
    free(normed);
    if (!cased) return json_null();
    out = json_string(cased);
    free(cased);
    return out;
}

json_t *w34_holders_028cc0_canonical_sqlite_path(const char *path) {
    char *stripped, *absolute, *cased;
    json_t *out;
    if (!path) return json_null();
    stripped = hs_removesuffix(path, " (deleted)");
    if (!stripped) return json_null();
    absolute = hs_abspath(stripped);
    free(stripped);
    if (!absolute) return json_null();
    cased = hs_normcase(absolute);
    free(absolute);
    if (!cased) return json_null();
    out = json_string(cased);
    free(cased);
    return out;
}

json_t *w34_holders_028cc0_argv_path_tokens(const json_t *argv) {
    int count = hs_argv_len(argv), index;
    json_t *out = json_array();

    for (index = 0; index < count; index++) {
        const char *token = hs_argv_at(argv, index);
        const char *path_token = NULL;
        char *normed;
        json_t *pair;
        if (!token) continue;
        if (token[0] == '/') {
            path_token = token;
        } else if (token[0] == '-' && strchr(token, '=')) {
            const char *value = strchr(token, '=') + 1;
            if (value[0] == '/') path_token = value;
        }
        if (!path_token) continue;
        normed = hs_normpath(path_token);
        if (!normed) continue;
        pair = json_array();
        json_append(pair, json_number((double)index));
        json_append(pair, json_string(normed));
        json_append(out, pair);
        free(normed);
    }
    return out;
}

/* --------------------------------------------------------------- scoping */

int w34_holders_028cc0_names_other_profile(const char *normalized,
                                           const char *install_root,
                                           const char *our_profile) {
    char *profiles, *cased, *prefix, *name, *tail;
    size_t pl, k;
    int out = 0;

    if (!normalized || !install_root) return 0;
    profiles = hs_join(install_root, "profiles");
    if (!profiles) return 0;
    cased = hs_normcase(profiles);
    free(profiles);
    if (!cased) return 0;
    pl = strlen(cased);
    prefix = (char *)malloc(pl + 2);
    if (!prefix) { free(cased); return 0; }
    memcpy(prefix, cased, pl);
    prefix[pl] = hs_sep();
    prefix[pl + 1] = '\0';
    free(cased);

    if (strncmp(normalized, prefix, pl + 1) != 0) { free(prefix); return 0; }
    tail = hs_dup(normalized + pl + 1);
    free(prefix);
    if (!tail) return 0;
    k = 0;
    while (tail[k] && !hs_is_sep((unsigned char)tail[k])) k++;
    name = (char *)malloc(k + 1);
    if (!name) { free(tail); return 0; }
    memcpy(name, tail, k);
    name[k] = '\0';
    free(tail);

    if (name[0] != '\0') {
        if (our_profile && our_profile[0] != '\0') {
            char *ours = hs_normcase(our_profile);
            if (ours) {
                out = strcmp(name, ours) != 0;
                free(ours);
            }
        } else {
            out = 1; /* name != None */
        }
    }
    free(name);
    return out;
}

json_t *w34_holders_028cc0_argv_home_selection(const json_t *argv,
                                               const char *this_home,
                                               const char *install_root,
                                               const char *our_profile) {
    char *home_value, *profile_value;

    home_value = hs_flag_lookup(argv, HS_HOME_FLAGS, HS_COUNT(HS_HOME_FLAGS));
    if (!home_value || home_value[0] == '\0') {
        char *env = hs_env_home(argv);
        free(home_value);
        home_value = env;
    }
    if (home_value && home_value[0] != '\0') {
        char *a = hs_normpath(home_value);
        char *b = hs_normpath(this_home ? this_home : "");
        json_t *out = json_string(a && b && strcmp(a, b) == 0 ? "ours" : "other");
        free(a);
        free(b);
        free(home_value);
        return out;
    }
    free(home_value);

    profile_value = hs_flag_lookup(argv, HS_PROFILE_FLAGS, HS_COUNT(HS_PROFILE_FLAGS));
    if (profile_value && profile_value[0] != '\0') {
        json_t *out;
        if (our_profile != NULL)
            out = json_string(strcmp(profile_value, our_profile) == 0 ? "ours" : "other");
        else
            out = json_string((install_root != NULL && strcmp(profile_value, "default") == 0)
                                  ? "ours"
                                  : "other");
        free(profile_value);
        return out;
    }
    free(profile_value);
    return json_null();
}

int w34_holders_028cc0_argv_scoped_to_other_home(const json_t *argv,
                                                 const char *db_path,
                                                 const char *install_root,
                                                 const char *our_profile) {
    char *db_abspath, *this_home, *this_home_norm, *root_norm = NULL;
    char *sidecars[3];
    json_t *tokens, *selection_json;
    const char *selection;
    int i, out;
    int argv0_locates_home;
    int other_home_seen;
    size_t t, tn, hn, rn;

    if (!db_path) return 0;
    db_abspath = hs_abspath(db_path);
    if (!db_abspath) return 0;
    this_home = hs_dirname(db_abspath);
    this_home_norm = this_home ? hs_normcase(this_home) : NULL;
    if (!this_home_norm) { free(db_abspath); free(this_home); return 0; }
    if (install_root && install_root[0] != '\0') root_norm = hs_normcase(install_root);

    sidecars[0] = hs_normcase(db_abspath);
    sidecars[1] = (char *)malloc(strlen(db_abspath) + 5);
    sidecars[2] = (char *)malloc(strlen(db_abspath) + 5);
    if (sidecars[1]) { strcpy(sidecars[1], db_abspath); strcat(sidecars[1], "-wal"); }
    if (sidecars[2]) { strcpy(sidecars[2], db_abspath); strcat(sidecars[2], "-shm"); }
    for (i = 1; i < 3; i++) {
        if (sidecars[i]) {
            char *cased = hs_normcase(sidecars[i]);
            free(sidecars[i]);
            sidecars[i] = cased;
        }
    }

    tokens = w34_holders_028cc0_argv_path_tokens(argv);
    tn = json_len(tokens);

    /* 1. a token naming our state.db or a sidecar exactly: definitively ours */
    for (t = 0; t < tn; t++) {
        const json_t *pair = json_get(tokens, t);
        const json_t *p = pair ? json_get(pair, 1) : NULL;
        if (!p || p->type != JSON_STRING || !p->str_val) continue;
        for (i = 0; i < 3; i++) {
            if (sidecars[i] && strcmp(p->str_val, sidecars[i]) == 0) {
                out = 0;
                goto done;
            }
        }
    }

    /* 2. the process's own home/profile selection decides a multiplexer. */
    selection_json = w34_holders_028cc0_argv_home_selection(argv, this_home, install_root, our_profile);
    selection = selection_json && selection_json->type == JSON_STRING ? selection_json->str_val : NULL;
    if (selection && strcmp(selection, "ours") == 0) {
        json_free(selection_json);
        out = 0;
        goto done;
    }
    argv0_locates_home = install_root != NULL;
    other_home_seen = selection && strcmp(selection, "other") == 0;
    json_free(selection_json);

    /* 3. path tokens: ours, the neutral shared root, another profile, another home. */
    hn = strlen(this_home_norm);
    rn = root_norm ? strlen(root_norm) : 0;
    for (t = 0; t < tn; t++) {
        const json_t *pair = json_get(tokens, t);
        const json_t *idx = pair ? json_get(pair, 0) : NULL;
        const json_t *p = pair ? json_get(pair, 1) : NULL;
        const char *normalized;
        long index;
        char *base;
        int named = 0;

        if (!idx || idx->type != JSON_NUMBER || !p || p->type != JSON_STRING || !p->str_val)
            continue;
        normalized = p->str_val;
        index = (long)idx->num_val;

        if (w34_holders_028cc0_names_other_profile(normalized, install_root, our_profile)) {
            other_home_seen = 1;
            continue;
        }
        if (strcmp(normalized, this_home_norm) == 0 ||
            (strncmp(normalized, this_home_norm, hn) == 0 && normalized[hn] == hs_sep())) {
            out = 0;
            goto done;
        }
        if (root_norm && (strcmp(normalized, root_norm) == 0 ||
                          (strncmp(normalized, root_norm, rn) == 0 && normalized[rn] == hs_sep())))
            continue; /* shared install root: neutral */
        if (index == 0 && !argv0_locates_home) continue;
        base = hs_basename(normalized);
        if (strstr(normalized, "/.hermes") || hs_ends(normalized, "/.hermes")) {
            named = 1;
        } else if (base) {
            for (i = 0; i < (int)HS_COUNT(HS_STATE_DB_NAMES); i++)
                if (strcmp(base, HS_STATE_DB_NAMES[i]) == 0) { named = 1; break; }
        }
        free(base);
        if (named) other_home_seen = 1;
    }
    out = other_home_seen;

done:
    json_free(tokens);
    free(db_abspath);
    free(this_home);
    free(this_home_norm);
    free(root_norm);
    for (i = 0; i < 3; i++) free(sidecars[i]);
    return out;
}

/* ----------------------------------------------------------- row shaping */

json_t *w34_holders_028cc0_describe_holder_pid(long pid, const json_t *argv) {
    hs_sb raw, who;
    char *text, *clipped;
    json_t *out;
    int i, count;

    count = argv && argv->type == JSON_ARRAY ? hs_argv_len(argv) : 0;
    if (count == 0) {
        text = (char *)malloc(64);
        if (!text) return json_null();
        snprintf(text, 64, "PID %ld (command line unavailable)", pid);
        out = json_string(text);
        free(text);
        return out;
    }

    sb_init(&raw);
    for (i = 0; i < count; i++) {
        const char *token = hs_argv_at(argv, i);
        char *piece;
        if (!token) continue;
        piece = i == 0 ? hs_basename(token) : hs_dup(token);
        if (!piece) continue;
        if (raw.n > 0) sb_addc(&raw, ' ');
        sb_add(&raw, piece);
        free(piece);
    }
    sb_init(&who);
    hs_words_join(&who, raw.ok ? raw.p : "");
    sb_free(&raw);

    clipped = (char *)malloc((who.ok ? who.n : 0) + 1);
    if (!clipped) { sb_free(&who); return json_null(); }
    hs_copy_chars(who.ok ? who.p : "", 80, clipped, who.n + 1);
    sb_free(&who);

    text = (char *)malloc(strlen(clipped) + 64);
    if (!text) { free(clipped); return json_null(); }
    snprintf(text, strlen(clipped) + 64, "PID %ld (%s)", pid, clipped);
    free(clipped);
    out = json_string(text);
    free(text);
    return out;
}

typedef struct {
    long pid;
    char **names;
    size_t n, cap;
} hs_pidrow;

static void pidrow_add(hs_pidrow *row, const char *name) {
    size_t i;
    char **grown;
    for (i = 0; i < row->n; i++)
        if (strcmp(row->names[i], name) == 0) return; /* set() */
    if (row->n == row->cap) {
        size_t cap = row->cap ? row->cap * 2 : 4;
        grown = (char **)realloc(row->names, cap * sizeof(char *));
        if (!grown) return;
        row->names = grown;
        row->cap = cap;
    }
    row->names[row->n] = hs_dup(name);
    if (row->names[row->n]) row->n++;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w34_holders_028cc0_held_store_refusal(const char *db_path,
                                              const json_t *holders,
                                              const char *command,
                                              const char *profile_arg,
                                              const char *docs_url,
                                              const char *force_hint) {
    hs_pidrow *rows = NULL;
    char **unknown = NULL;
    size_t nrows = 0, nunknown = 0, i, j;
    size_t count = holders && holders->type == JSON_ARRAY ? json_len(holders) : 0;
    hs_sb b;
    json_t *out;

    if (count == 0) return json_null();
    rows = (hs_pidrow *)calloc(count, sizeof(hs_pidrow));
    unknown = (char **)calloc(count, sizeof(char *));
    if (!rows || !unknown) { free(rows); free(unknown); return json_null(); }

    for (i = 0; i < count; i++) {
        const json_t *entry = json_get(holders, i);
        const json_t *pid_v = entry ? json_get(entry, 0) : NULL;
        const json_t *target_v = entry ? json_get(entry, 1) : NULL;
        long pid;
        const char *target;

        if (!pid_v || pid_v->type != JSON_NUMBER || !target_v || target_v->type != JSON_STRING)
            continue;
        pid = (long)pid_v->num_val;
        target = target_v->str_val ? target_v->str_val : "";
        if (pid <= 0 || hs_starts(target, "uninspectable")) {
            unknown[nunknown++] = hs_dup(target);
            continue;
        }
        {
            char *stripped = hs_removesuffix(target, " (deleted)");
            char *name = stripped ? hs_path_name(stripped) : NULL;
            size_t r = 0;
            free(stripped);
            if (!name) continue;
            while (r < nrows && rows[r].pid != pid) r++;
            if (r == nrows) {
                rows[nrows].pid = pid;
                rows[nrows].names = NULL;
                rows[nrows].n = rows[nrows].cap = 0;
                nrows++;
            }
            pidrow_add(&rows[r], name);
            free(name);
        }
    }

    for (i = 1; i < nrows; i++) { /* sorted(by_pid) */
        hs_pidrow key = rows[i];
        j = i;
        while (j > 0 && rows[j - 1].pid > key.pid) { rows[j] = rows[j - 1]; j--; }
        rows[j] = key;
    }

    sb_init(&b);
    sb_add(&b, "Refusing `hermes sessions ");
    sb_add(&b, command ? command : "");
    sb_add(&b, "`: another process is using ");
    sb_add(&b, db_path ? db_path : "");
    sb_add(&b, ".");
    for (i = 0; i < nrows; i++) {
        json_t *label = w34_holders_028cc0_describe_holder_pid(rows[i].pid, NULL);
        sb_add(&b, "\n  ");
        if (label && label->type == JSON_STRING && label->str_val) sb_add(&b, label->str_val);
        sb_add(&b, ": ");
        if (rows[i].n > 1) qsort(rows[i].names, rows[i].n, sizeof(char *), cmp_str);
        for (j = 0; j < rows[i].n; j++) {
            if (j) sb_add(&b, ", ");
            sb_add(&b, rows[i].names[j]);
        }
        json_free(label);
    }
    if (nunknown > 0) {
        char clipped[1024];
        hs_copy_chars(unknown[0] ? unknown[0] : "", 120, clipped, sizeof clipped);
        sb_add(&b, "\n  cannot prove the database is quiet (holder scan incomplete: ");
        sb_add(&b, clipped);
        sb_add(&b, ")");
    }
    sb_add(&b, "\nRewriting the database under a live writer is how every agent ends up refusing turns with the ");
    sb_add(&b, "retired state.db-wal error. Nothing is lost.");
    sb_add(&b, "\nStop them first (`hermes ");
    sb_add(&b, profile_arg ? profile_arg : "");
    sb_add(&b, "gateway stop`, quit the Desktop app, pause cron), then re-run.");
    if (force_hint && force_hint[0] != '\0') {
        sb_add(&b, "\nOverride with ");
        sb_add(&b, force_hint);
        sb_add(&b, " if you accept the risk.");
    }
    sb_add(&b, "\nRecovery guide: ");
    sb_add(&b, docs_url ? docs_url : "");

    out = b.ok ? json_string(b.p) : json_null();
    sb_free(&b);
    for (i = 0; i < nrows; i++) {
        for (j = 0; j < rows[i].n; j++) free(rows[i].names[j]);
        free(rows[i].names);
    }
    for (i = 0; i < nunknown; i++) free(unknown[i]);
    free(rows);
    free(unknown);
    return out;
}
