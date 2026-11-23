/*
 * media.c — faithful C11 port of session_rules/media.py (gate w7_media_2c731e).
 *
 * PoP: w7_media_hosted_document_references @ session_rules/media.py:hosted_document_references
 * PoP: w7_media_admission_media_references @ session_rules/media.py:admission_media_references
 * PoP: w7_media_archive_refs_in_text @ session_rules/media.py:archive_refs_in_text
 * PoP: w7_media_retirement_candidates_in_text @ session_rules/media.py:retirement_candidates_in_text
 * PoP: w7_media_spillover_names_in_text @ session_rules/media.py:spillover_names_in_text
 *
 * Standard-library-only Python logic: re patterns + pathlib PurePath rules.
 * Set results are emitted sorted (Python sorted(set)).
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- small string/path helpers ---------------- */

static int w7_media_is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

static int w7_media_is_hex_n(const char *s, size_t n) {
    if (!s || strlen(s) < n) return 0;
    for (size_t i = 0; i < n; i++) if (!w7_media_is_hex(s[i])) return 0;
    return 1;
}

/* re.fullmatch(r'[0-9a-f]{64}') */
static int w7_media_sha256_name(const char *s) {
    return s && strlen(s) == 64 && w7_media_is_hex_n(s, 64);
}

/* re.fullmatch(r'api_[0-9a-f]{32}\.(png|jpg|gif|webp)') — the dot is escaped. */
static int w7_media_api_image_name(const char *s) {
    if (!s || strncmp(s, "api_", 4)) return 0;
    if (!w7_media_is_hex_n(s + 4, 32)) return 0;
    const char *dot = s + 36;
    if (*dot != '.') return 0;
    const char *ext = dot + 1;
    return !strcmp(ext, "png") || !strcmp(ext, "jpg") ||
           !strcmp(ext, "gif") || !strcmp(ext, "webp");
}

static char *w7_media_strdup_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Normalize a Windows path for PureWindowsPath equality: backslashes -> '/',
 * trailing separators removed (except a bare root), comparison case-folded. */
static char *w7_media_norm_path(const char *p) {
    if (!p) return w7_media_strdup_n("", 0);
    size_t n = strlen(p);
    char *out = (char *)malloc(n + 2);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = p[i] == '\\' ? '/' : p[i];
    out[n] = '\0';
    size_t e = strlen(out);
    while (e > 0 && out[e - 1] == '/' && e > 3) out[--e] = '\0';
    return out;
}

static int w7_media_path_eq(const char *a, const char *b) {
    char *na = w7_media_norm_path(a), *nb = w7_media_norm_path(b);
    int eq = na && nb && !strcasecmp(na, nb);
    free(na); free(nb);
    return eq;
}

/* PureWindowsPath(value).is_absolute(): drive-absolute or UNC/rooted. */
static int w7_media_is_absolute(const char *p) {
    if (!p || !*p) return 0;
    if (p[0] == '\\' || p[0] == '/') return 1;
    return isalpha((unsigned char)p[0]) && p[1] == ':' &&
           (p[2] == '/' || p[2] == '\\');
}

/* Last path segment (handles both separators). */
static const char *w7_media_basename(const char *p) {
    if (!p || !*p) return "";
    const char *last = p;
    for (const char *c = p; *c; c++) if (*c == '/' || *c == '\\') last = c + 1;
    return last;
}

/* Parent directory of p, NUL-terminated copy (NULL when none). */
static char *w7_media_parent_of(const char *p) {
    if (!p || !*p) return NULL;
    const char *last = NULL;
    for (const char *c = p; *c; c++) if (*c == '/' || *c == '\\') last = c;
    if (!last) return NULL;
    if (last == p) return w7_media_strdup_n(p, 1);   /* rooted path -> "/" */
    return w7_media_strdup_n(p, (size_t)(last - p));
}

/* ---------------- sorted string set ---------------- */

typedef struct { char **v; size_t n, cap; } w7_media_set;

static void w7_media_set_add(w7_media_set *s, const char *value) {
    if (!value) return;
    for (size_t i = 0; i < s->n; i++) if (!strcmp(s->v[i], value)) return;
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8;
        char **nv = (char **)realloc(s->v, nc * sizeof(char *));
        if (!nv) return;
        s->v = nv; s->cap = nc;
    }
    s->v[s->n++] = _strdup(value);
}

static int w7_media_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *w7_media_set_json(w7_media_set *s) {
    qsort(s->v, s->n, sizeof(char *), w7_media_cmp_str);
    json_t *arr = json_array();
    for (size_t i = 0; i < s->n; i++) {
        json_append(arr, json_string(s->v[i]));
        free(s->v[i]);
    }
    free(s->v);
    s->v = NULL; s->n = s->cap = 0;
    return arr;
}

/* ---------------- hosted_document_references ---------------- */

json_t *w7_media_hosted_document_references(const char *request_id,
        const json_t *payload, const char *media_root) {
    json_t *out = json_array();
    if (!request_id || strncmp(request_id, "hosted:", 7)) return out;
    json_t *text = payload ? json_obj_get(payload, "text") : NULL;
    if (!text || text->type != JSON_STRING || !text->str_val) return out;
    const char *marker = "\n[Shared attachment] file: ";
    size_t mlen = strlen(marker);
    const char *cursor = text->str_val;
    while ((cursor = strstr(cursor, marker)) != NULL) {
        const char *start = cursor + mlen;
        const char *nl = strchr(start, '\n');
        if (!nl) { cursor++; continue; }
        char *value = w7_media_strdup_n(start, (size_t)(nl - start));
        if (value) {
            char *norm = w7_media_norm_path(value);
            const char *name = w7_media_basename(norm ? norm : value);
            char *parent = w7_media_parent_of(norm ? norm : value);
            char *grand = parent ? w7_media_parent_of(parent) : NULL;
            if (w7_media_is_absolute(value) && grand &&
                    w7_media_path_eq(grand, media_root) &&
                    w7_media_sha256_name(w7_media_basename(parent)) &&
                    !w7_media_api_image_name(name)) {
                json_t *item = json_object();
                json_set(item, "path", json_string(value));
                json_set(item, "sha256", json_string(w7_media_basename(parent)));
                json_append(out, item);
            }
            free(grand); free(parent); free(norm); free(value);
        }
        cursor = nl + 1;  /* re.findall continues after the whole match */
    }
    return out;
}

/* ---------------- admission_media_references ---------------- */

json_t *w7_media_admission_media_references(const json_t *payload) {
    json_t *out = json_array();
    const char *sections[] = {"attachments_v1", "native_text_v1"};
    for (size_t i = 0; i < 2; i++) {
        json_t *section = payload ? json_obj_get(payload, sections[i]) : NULL;
        if (!section || section->type != JSON_OBJECT) continue;
        json_t *media = json_obj_get(section, "media");
        if (!media || media->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < media->c.count; j++)
            json_append(out, json_copy(media->c.items[j]));
    }
    return out;
}

/* ---------------- archive reference scanning ---------------- */

static int w7_media_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-';
}

/* Collect "cache/spillover/<name>" and "cache/pruned/<name>" keys. */
static void w7_media_collect_archive_refs(const json_t *texts, w7_media_set *s) {
    if (!texts || texts->type != JSON_ARRAY) return;
    for (size_t t = 0; t < texts->c.count; t++) {
        json_t *node = texts->c.items[t];
        if (!node || node->type != JSON_STRING || !node->str_val) continue;
        const char *p = node->str_val;

        /* re.findall(r"spillover(?:/|\\+)([A-Za-z0-9_.-]+)") */
        while ((p = strstr(p, "spillover")) != NULL) {
            const char *q = p + 9;
            const char *base = NULL;
            if (*q == '/') { q++; base = q; }
            else if (*q == '\\') {
                while (*q == '\\') q++;
                if (*q == '/') q++;
                base = q;
            }
            if (base) {
                while (w7_media_name_char(*q)) q++;
                if (q > base) {
                    char buf[512];
                    snprintf(buf, sizeof(buf), "cache/spillover/%.*s", (int)(q - base), base);
                    w7_media_set_add(s, buf);
                }
                p = q > p + 9 ? q : p + 9;
            } else {
                p += 1;
            }
        }

        /* re.findall(r"cache/pruned/([A-Za-z0-9_.-]+)") */
        p = node->str_val;
        while ((p = strstr(p, "cache/pruned/")) != NULL) {
            const char *base = p + 13;
            const char *q = base;
            while (w7_media_name_char(*q)) q++;
            if (q > base) {
                char buf[512];
                snprintf(buf, sizeof(buf), "cache/pruned/%.*s", (int)(q - base), base);
                w7_media_set_add(s, buf);
            }
            p = q > base ? q : p + 13;
        }
    }
}

json_t *w7_media_archive_refs_in_text(const json_t *texts) {
    w7_media_set s = {0};
    w7_media_collect_archive_refs(texts, &s);
    return w7_media_set_json(&s);
}

json_t *w7_media_retirement_candidates_in_text(const json_t *texts) {
    w7_media_set refs = {0}, out = {0};
    w7_media_collect_archive_refs(texts, &refs);
    const char *prefix = "cache/spillover/";
    for (size_t i = 0; i < refs.n; i++) {
        const char *v = refs.v[i];
        w7_media_set_add(&out, !strncmp(v, prefix, strlen(prefix)) ? v + strlen(prefix) : v);
    }
    for (size_t i = 0; i < refs.n; i++) free(refs.v[i]);
    free(refs.v);
    return w7_media_set_json(&out);
}

json_t *w7_media_spillover_names_in_text(const json_t *texts) {
    w7_media_set refs = {0}, out = {0};
    w7_media_collect_archive_refs(texts, &refs);
    const char *prefix = "cache/spillover/";
    for (size_t i = 0; i < refs.n; i++) {
        if (!strncmp(refs.v[i], prefix, strlen(prefix)))
            w7_media_set_add(&out, refs.v[i] + strlen(prefix));
    }
    for (size_t i = 0; i < refs.n; i++) free(refs.v[i]);
    free(refs.v);
    return w7_media_set_json(&out);
}
