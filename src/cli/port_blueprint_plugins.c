/*
 * port_blueprint_plugins.c — pure validators from cron/blueprint_plugins.py.
 *
 * _SLOT_NAME_RE = ^[A-Za-z_]\w{0,63}$  (first char letter/underscore, then
 * word chars, 1..64 long).
 */

#include "port_blueprint_plugins.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bp_valid_slot_name(const char *s)
{
    size_t i, len = strlen(s);
    if (len == 0 || len > 64)
        return 0;
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return 0;
    for (i = 1; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!(isalnum(c) || c == '_'))
            return 0;
    }
    return 1;
}

int bp_text(const char *name, const char *value, char **out)
{
    const char *v = value ? value : "";
    size_t s = 0, e;
    char *o;
    if (out) *out = NULL;
    e = strlen(v);
    while (s < e && isspace((unsigned char)v[s]))
        s++;
    if (s == e) /* empty (or not a string) -> ValueError */
        return -1;
    while (e > s && isspace((unsigned char)v[e - 1]))
        e--;
    o = (char *)malloc(e - s + 1);
    if (!o) return -1;
    memcpy(o, v + s, e - s);
    o[e - s] = '\0';
    if (out) *out = o; else free(o);
    return 0;
}

int bp_strings(const char *name, const char *value_json)
{
    json_t *arr = value_json ? json_parse(value_json, NULL) : NULL;
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) { /* not an iterable of strings */
        if (arr) json_free(arr);
        return -1;
    }
    for (i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (!it || it->type != JSON_STRING || !it->str_val) {
            json_free(arr);
            return -1;
        }
        /* non-empty after strip */
        size_t n = strlen(it->str_val);
        while (n > 0 && isspace((unsigned char)it->str_val[n - 1])) n--;
        if (n == 0) {
            json_free(arr);
            return -1;
        }
    }
    json_free(arr);
    return 0;
}

/* Find the matching closing brace for the field that opens at t[i]=='{',
 * brace-depth aware (nested {..} inside a format spec). Returns the index of
 * the closing '}', or (size_t)-1 when unbalanced. */
static size_t bp_field_end(const char *t, size_t i, size_t len)
{
    int depth = 1;
    size_t j = i + 1;
    while (j < len && depth > 0) {
        if (t[j] == '{') depth++;
        else if (t[j] == '}') depth--;
        j++;
    }
    return depth == 0 ? j - 1 : (size_t)-1;
}

int bp_placeholders(const char *template, const char *what, char **out)
{
    const char *t = template ? template : "";
    size_t len = strlen(t), i = 0;
    const char *wname = what ? what : "";
    /* collect names into a growable list */
    char names[64][96];
    size_t nnames = 0;
    size_t cap = 256, used = 0;
    char *buf;

    if (out) *out = NULL;
    while (i < len) {
        if (t[i] == '{') {
            if (i + 1 < len && t[i + 1] == '{') { i += 2; continue; } /* {{ */
            {
                size_t end = bp_field_end(t, i, len);
                size_t k, nl;
                if (end == (size_t)-1) { /* unbalanced -> invalid field */
                    return -1;
                }
                /* field content t[i+1 .. end-1]; name is leading token before
                 * '!' or ':'. */
                k = i + 1;
                while (k < end && t[k] != '!' && t[k] != ':') k++;
                nl = k - (i + 1);
                {
                    char nb[96];
                    if (nl >= sizeof(nb)) return -1;
                    memcpy(nb, t + i + 1, nl);
                    nb[nl] = '\0';
                    /* "" (a lone "{}") fails just like a bad name. */
                    if (!bp_valid_slot_name(nb))
                        return -1; /* ValueError: "{what} placeholder {nb}..." */
                    if (nnames < 64) {
                        memcpy(names[nnames], nb, nl + 1);
                        nnames++;
                    }
                }
                i = end + 1;
            }
        } else if (t[i] == '}') {
            if (i + 1 < len && t[i + 1] == '}') { i += 2; continue; } /* }} */
            return -1; /* stray '}' -> Python parse raises */
        } else {
            i++;
        }
    }

    /* Build a JSON array of the names. */
    used = 2; /* "[]" */
    for (size_t j = 0; j < nnames; j++)
        used += strlen(names[j]) + 4;
    if (used > cap) cap = used;
    buf = (char *)malloc(cap);
    if (!buf) return -1;
    {
        char *p = buf;
        *p++ = '[';
        for (size_t j = 0; j < nnames; j++) {
            if (j) *p++ = ',';
            *p++ = '"';
            size_t l = strlen(names[j]);
            memcpy(p, names[j], l);
            p += l;
            *p++ = '"';
        }
        *p++ = ']';
        *p = '\0';
    }
    if (out) *out = buf; else free(buf);
    (void)wname;
    return 0;
}
