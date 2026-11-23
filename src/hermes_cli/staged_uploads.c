/*
 * staged_uploads.c — C home for upstream Hermes hermes_cli/staged_uploads.py.
 * PoP: w12_stageduplo_df301a_safe_filename @ hermes_cli/staged_uploads.py:_safe_filename
 * PoP: w12_stageduplo_df301a_parse_staged_upload @ hermes_cli/staged_uploads.py:parse_staged_upload
 * PoP: w12_stageduplo_df301a_staged_upload_descriptor @ hermes_cli/staged_uploads.py:staged_upload_descriptor
 *
 * The layout rule lives on both sides of a staged upload; these three seams
 * are the parts that do not touch the filesystem.
 */
#include "hermes_json.h"
#include "noc_w12_stageduplo_df301a.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------- _safe_filename */
static size_t su_cp_len(const unsigned char *p)
{
    if (p[0] < 0x80) return 1;
    if ((p[0] & 0xE0) == 0xC0) return 2;
    if ((p[0] & 0xF0) == 0xE0) return 3;
    if ((p[0] & 0xF8) == 0xF0) return 4;
    return 1;
}

/* ASCII-safe classes only; every non-ASCII codepoint is "unsafe". */
static int su_cp_safe(const unsigned char *p, size_t n)
{
    unsigned char c;
    if (n != 1) return 0;
    c = p[0];
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

/* re.sub(r"[^A-Za-z0-9._-]+", "-", s) */
static void su_sub_unsafe(const char *src, char *out, size_t cap)
{
    const unsigned char *p = (const unsigned char *)src;
    size_t o = 0;
    int in_run = 0;
    if (!cap) return;
    out[0] = '\0';
    while (*p) {
        size_t n = su_cp_len(p);
        int safe = su_cp_safe(p, n);
        if (!safe) {
            if (!in_run && o + 1 < cap) { out[o++] = '-'; out[o] = '\0'; in_run = 1; }
        } else {
            if (o + n >= cap) break;
            memcpy(out + o, p, n);
            o += n;
            out[o] = '\0';
            in_run = 0;
        }
        p += n;
    }
    out[o] = '\0';  /* NUL at the final (possibly shortened) length */
}

static void su_copy_span(const char *s, size_t n, char *out, size_t cap)
{
    size_t k = n;
    if (!cap) return;
    if (k >= cap) k = cap - 1;
    memcpy(out, s, k);
    out[k] = '\0';
}

void w12_stageduplo_df301a_safe_filename(const char *value, char *out, size_t cap)
{
    const char *name;
    char nb[2048], stem[2048], sf[2048], st[2048];
    size_t L, blen, a, b, slen, suflen, room;
    const char *base;
    size_t i;

    if (!out || !cap) return;
    out[0] = '\0';
    name = (value && *value) ? value : "attachment";

    L = strlen(name);
    while (L > 0 && (name[L - 1] == '/' || name[L - 1] == '\\')) L--;
    base = name;
    for (i = 0; i < L; i++)
        if (name[i] == '/' || name[i] == '\\') base = name + i + 1;
    blen = (size_t)(name + L - base);
    su_copy_span(base, blen, nb, sizeof(nb));

    /* pathlib stem/suffix: last '.' strictly after position 0; "."/".." have none. */
    stem[0] = '\0';
    sf[0] = '\0';
    if (!((blen == 1 && nb[0] == '.') || (blen == 2 && nb[0] == '.' && nb[1] == '.'))) {
        for (i = blen; i > 1; i--) {
            if (nb[i - 1] == '.') {
                su_copy_span(nb, i - 1, stem, sizeof(stem));
                su_copy_span(nb + (i - 1), blen - (i - 1), sf, sizeof(sf));
                break;
            }
        }
    }
    if (!stem[0]) su_copy_span(nb, blen, stem, sizeof(stem));

    su_sub_unsafe(stem, st, sizeof(st));
    slen = strlen(st);
    a = 0;
    b = slen;
    while (a < b && (st[a] == '.' || st[a] == '-')) a++;
    while (b > a && (st[b - 1] == '.' || st[b - 1] == '-')) b--;
    memmove(st, st + a, b - a);
    st[b - a] = '\0';  /* NUL at the shortened length */
    if (!st[0]) { strcpy(st, "attachment"); }

    if (sf[0]) {
        su_sub_unsafe(sf, stem, sizeof(stem));  /* reuse stem as scratch */
        suflen = strlen(stem);
        while (suflen > 0 && (stem[suflen - 1] == '.' || stem[suflen - 1] == '-'))
            stem[--suflen] = '\0';
        if (suflen > 119) { stem[119] = '\0'; suflen = 119; }
    } else {
        stem[0] = '\0';
        suflen = 0;
    }

    room = 120 - suflen;
    slen = strlen(st);
    if (slen > room) { st[room] = '\0'; slen = room; }
    snprintf(out, cap, "%s%s", st, stem);
}

/* -------------------------------------------------- descriptor seams */
struct json_t *w12_stageduplo_df301a_parse_staged_upload(
    const char *install_id, const struct json_t *descriptor)
{
    json_t *out = json_object();
    json_t *iv, *hv, *pv;
    const char *ids, *home, *path;

    if (!descriptor || descriptor->type != JSON_OBJECT) {
        json_set(out, "error", json_string("invalid staged upload"));
        return out;
    }
    if (!install_id || !*install_id) {
        json_set(out, "error", json_string(
            "Staged attachment belongs to another Hermes backend; select the file again"));
        return out;
    }
    iv = json_obj_get(descriptor, "install_id");
    ids = (iv && iv->type == JSON_STRING) ? iv->str_val : NULL;
    if (!ids || strcmp(ids, install_id)) {
        json_set(out, "error", json_string(
            "Staged attachment belongs to another Hermes backend; select the file again"));
        return out;
    }
    hv = json_obj_get(descriptor, "profile_home");
    pv = json_obj_get(descriptor, "path");
    home = (hv && hv->type == JSON_STRING) ? hv->str_val : NULL;
    path = (pv && pv->type == JSON_STRING) ? pv->str_val : NULL;
    if (!home || !*home || !path || !*path) {
        json_set(out, "error", json_string("invalid staged upload source"));
        return out;
    }
    json_set(out, "home", json_string(home));
    json_set(out, "path", json_string(path));
    return out;
}

struct json_t *w12_stageduplo_df301a_staged_upload_descriptor(
    const char *install_id, const char *path, const char *profile_home)
{
    json_t *out;
    if (!install_id || !*install_id) return NULL;
    out = json_object();
    json_set(out, "install_id", json_string(install_id));
    json_set(out, "path", json_string(path ? path : ""));
    json_set(out, "profile_home", json_string(profile_home ? profile_home : ""));
    return out;
}
