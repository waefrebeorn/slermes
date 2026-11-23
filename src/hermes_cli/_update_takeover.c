/*
 * _update_takeover.c — real C11 port of upstream Hermes hermes_cli/_update_takeover.py.
 * PoP: w14_updatetak_ce4155_sha_valid @ hermes_cli/_update_takeover.py:_SHA
 * PoP: w14_updatetak_ce4155_head_sha @ hermes_cli/_update_takeover.py:_head_sha
 * Ported: the SHA-shape predicate and the git/ref parsing decision core of
 * _head_sha, with subprocess/filesystem results injected as fixture facts.
 * The update orchestration (prepare/main/receipt/fleet obligations) is
 * side-effecting and intentionally excluded.
 */
#include "hermes_json.h"
#include "noc_w14_updatetak_ce4155.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n; char *p;
    if (!s) return NULL;
    n = strlen(s) + 1; p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

int w14_updatetak_ce4155_sha_valid(const char *s) {
    size_t n, i;
    if (!s) return 0;
    n = strlen(s);
    if (n != 40 && n != 64) return 0;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

static char *astrim(const char *s) {
    size_t n; char *o;
    if (!s) return dupstr("");
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n) memcpy(o, s, n);
    o[n] = 0;
    return o;
}

static int starts_with(const char *s, const char *prefix) {
    return s && !strncmp(s, prefix, strlen(prefix));
}

/* First packed-refs line ending in " <ref>": its first whitespace-free token. */
static char *packed_ref_lookup(const char *packed, const char *ref) {
    size_t suffix_len = strlen(ref) + 1;
    char *suffix = (char *)malloc(suffix_len + 1);
    const char *p = packed ? packed : "";
    char *found = dupstr("");
    if (suffix) { suffix[0] = ' '; memcpy(suffix + 1, ref, strlen(ref) + 1); }
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char *line = (char *)malloc(len + 1);
        size_t sl;
        if (!line) break;
        memcpy(line, p, len); line[len] = 0;
        if (len && line[len - 1] == '\r') line[--len] = 0;
        sl = strlen(line);
        if (suffix && sl >= suffix_len && !strcmp(line + sl - suffix_len, suffix)) {
            char *sp = strchr(line, ' ');
            if (sp) *sp = 0;
            free(found);
            found = astrim(line);
            free(line);
            break;
        }
        free(line);
        if (!e) break;
        p = e + 1;
    }
    free(suffix);
    return found ? found : dupstr("");
}

char *w14_updatetak_ce4155_head_sha(const struct json_t *facts) {
    double rc = facts ? json_get_num(facts, "git_rc", 1.0) : 1.0;
    const char *gso = facts ? json_get_str(facts, "git_stdout", "") : "";
    const char *headraw = facts ? json_get_str(facts, "head", "") : "";
    char *head, *result = NULL;
    if (rc == 0.0) {
        char *t = astrim(gso);
        if (w14_updatetak_ce4155_sha_valid(t)) return t;
        free(t);
    }
    head = astrim(headraw);
    if (starts_with(head, "ref:")) {
        char *ref = astrim(head + 4);
        const json_t *loose = facts ? json_obj_get(facts, "loose") : NULL;
        const json_t *packed = facts ? json_obj_get(facts, "packed") : NULL;
        if (loose && loose->type == JSON_STRING) result = astrim(loose->str_val);
        else if (packed && packed->type == JSON_STRING) result = packed_ref_lookup(packed->str_val, ref);
        else result = dupstr("");
        free(ref);
    } else {
        result = head;
        head = NULL;
    }
    free(head);
    if (!result) result = dupstr("");
    if (!w14_updatetak_ce4155_sha_valid(result)) { free(result); result = dupstr(""); }
    return result;
}
