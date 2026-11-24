/*
 * run_heartbeat_restore.c — C port of upstream Hermes gateway/run_heartbeat_restore.py.
 *
 * PoP: w17_runheartbe_1de6c4_watched_homes @ gateway/run_heartbeat_restore.py:_watched_homes
 *
 * The oracle drives str(Path(...)) under the Windows interpreter; pathnorm
 * below reproduces that lexical normalization (forward slashes -> backslashes,
 * duplicate separators collapsed, "." dropped, trailing separator removed,
 * empty -> ".").
 */
#include "noc_w17_runheartbe_1de6c4.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int is_sep(char c) { return c == '/' || c == '\\'; }

/* Windows pathlib lexical str(); caller frees the result. */
static char *hb_pathnorm(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 4);
    if (!buf) return NULL;
    size_t o = 0, i = 0;
    int drive = 0, abs = 0, sep_seen = 0, had_seg = 0;

    if (len >= 2 && s[1] == ':' && isalpha((unsigned char)s[0])) {
        buf[o++] = s[0]; buf[o++] = ':'; drive = 1; i = 2;
    }
    while (i < len && is_sep(s[i])) { abs = 1; sep_seen = 1; i++; }
    while (i < len) {
        while (i < len && is_sep(s[i])) { sep_seen = 1; i++; }
        if (i >= len) break;
        size_t st = i;
        while (i < len && !is_sep(s[i])) i++;
        size_t sl = i - st;
        if (sl == 1 && s[st] == '.') continue;   /* Path drops '.' components */
        if (had_seg || abs || drive) buf[o++] = '\\';
        memcpy(buf + o, s + st, sl);
        o += sl;
        had_seg = 1;
    }
    if (!had_seg) {
        if (drive && sep_seen) buf[o++] = '\\';       /* C:/ -> C:\ */
        else if (abs && !drive) buf[o++] = '\\';       /* / -> \ */
        else if (!drive && !abs) buf[o++] = '.';      /* '' -> . */
    }
    buf[o] = '\0';  /* NUL at the new length after shortening */
    return buf;
}

struct json_t *w17_runheartbe_1de6c4_watched_homes(const struct json_t *default_home,
                                        const struct json_t *multiplex_homes,
                                        int multiplex_profiles) {
    json_t *out = json_array();
    const char *dstr = (default_home && default_home->type == JSON_STRING) ? default_home->str_val : "";

    /* homes = [default_home]; if multiplex: homes += [home for name, home in ...] */
    size_t n = 1 + (multiplex_profiles && json_is_array(multiplex_homes) ? json_len(multiplex_homes) : 0);
    char **raw = (char **)calloc(n ? n : 1, sizeof(char *));
    if (!raw) return out;
    size_t cnt = 0;
    raw[cnt++] = hb_pathnorm(dstr);
    if (multiplex_profiles && json_is_array(multiplex_homes)) {
        for (size_t i = 0; i < json_len(multiplex_homes); i++) {
            const json_t *pair = json_get(multiplex_homes, i);
            const json_t *h = json_is_array(pair) ? json_get(pair, 1) : NULL;
            const char *hs = (h && h->type == JSON_STRING) ? h->str_val : "";
            raw[cnt++] = hb_pathnorm(hs);
        }
    }

    /* list(dict.fromkeys(...)): dedup by normalized string, first occurrence wins. */
    for (size_t i = 0; i < cnt; i++) {
        int seen = 0;
        for (size_t j = 0; j < json_len(out); j++) {
            const json_t *v = json_get(out, j);
            if (v && v->type == JSON_STRING && raw[i] && strcmp(v->str_val, raw[i]) == 0) { seen = 1; break; }
        }
        if (!seen) json_append(out, json_string(raw[i] ? raw[i] : ""));
    }
    for (size_t i = 0; i < cnt; i++) free(raw[i]);
    free(raw);
    return out;
}
