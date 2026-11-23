/*
 * approval_pipe_launchers.c — faithful C11 port of tools/approval_pipe_launchers.py.
 * Gate: apppipe.
 * PoP: apppipe_pipe_launcher_re @ tools/approval_pipe_launchers.py:_pipe_launcher_re
 * PoP: apppipe_path_prefix_re @ tools/approval_pipe_launchers.py:_PIPE_PATH_PREFIX_RE
 * PoP: apppipe_wrappers_re @ tools/approval_pipe_launchers.py:_PIPE_WRAPPERS_RE
 *
 * The module builds regex SOURCE strings (it does not evaluate them). The
 * builders reproduce the exact f-string concatenation of the Python source.
 */
#include "noc_apppipe.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t len, cap; } sbuf;
static void sb_init(sbuf *b) { b->cap = 512; b->len = 0; b->p = (char *)malloc(b->cap); if (b->p) b->p[0] = '\0'; }
static void sb_grow(sbuf *b, size_t need) {
    if (b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap;
    while (nc < b->len + need + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc);
    if (np) { b->p = np; b->cap = nc; }
}
static void sb_addn(sbuf *b, const char *s, size_t n) {
    if (!b->p || !s) return;
    sb_grow(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void sb_add(sbuf *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static char *sb_take(sbuf *b) { char *p = b->p; b->p = NULL; b->len = b->cap = 0; return p; }

char *apppipe_pipe_launcher_re(const char *name, const char *short_opts,
                               const char *const *long_opts, size_t n_long,
                               const char *ws) {
    sbuf b; sb_init(&b);
    if (!ws) ws = "\\s";
    sb_add(&b, name);
    sb_add(&b, "(?:");
    sb_add(&b, ws);
    sb_add(&b, "++(?>");
    int first = 1;
    if (short_opts && short_opts[0]) {
        if (!first) sb_add(&b, "|"); first = 0;
        sb_add(&b, "(?-i:-[^\\W"); sb_add(&b, short_opts);
        sb_add(&b, "\\d-]*+["); sb_add(&b, short_opts); sb_add(&b, "])");
        sb_add(&b, ws); sb_add(&b, "++[^\\s|;&<>]++");
    }
    if (long_opts && n_long) {
        if (!first) sb_add(&b, "|"); first = 0;
        sb_add(&b, "--(?:");
        for (size_t i = 0; i < n_long; i++) {
            if (i) sb_add(&b, "|");
            sb_add(&b, long_opts[i]);
        }
        sb_add(&b, ")");
        sb_add(&b, ws); sb_add(&b, "++[^\\s|;&<>]++");
    }
    if (!first) sb_add(&b, "|");
    sb_add(&b, "-[^\\s|;&<>]++");
    sb_add(&b, "|");
    sb_add(&b, "\\w++=[^\\s|;&<>]*+");
    sb_add(&b, "|");
    sb_add(&b, "(?:infinity|\\d++(?:\\.\\d++)?(?:e\\d++)?|\\.\\d++)[smhd]?(?=\\s)");
    sb_add(&b, "))");
    sb_add(&b, "*+");
    return sb_take(&b);
}

const char *apppipe_path_prefix_re(void) {
    return "(?>(?:[a-zA-Z]:)?(?:[.\\w~-]*+[/\\\\])*+)";
}

typedef struct {
    const char *name;
    const char *short_o;
    const char *const *long_o;
    size_t n_long;
} launcher_t;

static const char *const sudo_l[] = {
    "user","group","other-user","close-from","chdir","host","prompt","role",
    "type","command-timeout","chroot"};
static const char *const env_l[]  = {"unset","chdir","argv0"};
static const char *const nice_l[] = {"adjustment"};
static const char *const time_l[] = {"format","output"};
static const char *const timeout_l[] = {"signal","kill-after"};
static const char *const stdbuf_l[] = {"input","output","error"};
static const char *const ionice_l[] = {"class","classdata","pid","pgid","uid"};

static const launcher_t LAUNCHERS[] = {
    {"sudo",   "ugUCDhpRrtT", sudo_l,    11},
    {"doas",   "uC",          NULL,       0},
    {"env",    "uCa",         env_l,      3},
    {"exec",   "a",           NULL,       0},
    {"command","",            NULL,       0},
    {"nohup",  "",            NULL,       0},
    {"nice",   "n",           nice_l,     1},
    {"time",   "fo",          time_l,     2},
    {"timeout","sk",          timeout_l,  2},
    {"stdbuf", "ioe",         stdbuf_l,   3},
    {"busybox","",            NULL,       0},
    {"setsid", "",            NULL,       0},
    {"ionice", "cnpPu",       ionice_l,   5},
};
#define NLAUNCH ((size_t)(sizeof(LAUNCHERS) / sizeof(LAUNCHERS[0])))

char *apppipe_wrappers_re(void) {
    sbuf b; sb_init(&b);
    sb_add(&b, "(?>");
    sb_add(&b, apppipe_path_prefix_re());
    sb_add(&b, "(?:");
    for (size_t i = 0; i < NLAUNCH; i++) {
        if (i) sb_add(&b, "|");
        char *r = apppipe_pipe_launcher_re(LAUNCHERS[i].name, LAUNCHERS[i].short_o,
                                           LAUNCHERS[i].long_o, LAUNCHERS[i].n_long, "\\s");
        sb_add(&b, r);
        free(r);
    }
    sb_add(&b, ")\\s++)*+");
    return sb_take(&b);
}
