/*
 * media.c -- C11 port of the retirement-candidate rows of upstream Hermes
 *   persistence/sqlite/conversation/_store/media.py.
 *
 * PoP: w22_media_6187d8_prefix @ persistence/sqlite/conversation/_store/media.py:PREFIX
 * PoP: w22_media_6187d8_spillover_prefix @ persistence/sqlite/conversation/_store/media.py:SPILLOVER_PREFIX
 * PoP: w22_media_6187d8_retire_media @ persistence/sqlite/conversation/_store/media.py:retire_media
 * PoP: w22_media_6187d8_spillover_records @ persistence/sqlite/conversation/_store/media.py:retire_spillover
 *
 * The sqlite handle (`conn.execute` / `conn.executemany`), the `_id_chunks`
 * paging and `SPILLOVER_SQL_FILTER` are the DB seam and stay upstream: the C
 * answers the exact (key, value) rows those statements would insert. The rules
 * themselves (admission_media_references, hosted_document_references,
 * retirement_candidates_in_text) are ported here.
 *
 * Pure C11, ASCII, no VLA.
 */
#include "hermes_json.h"
#include "noc_w22_media_6187d8.h"
#include "crypto.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define WM6187_PREFIX "gateway.media.retired.v1."
#define WM6187_SPILL  "gateway.spillover.retired.v1."

static char *wm6187_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *wm6187_dup_n(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* `hashlib.sha256(text.encode()).hexdigest()` */
static void wm6187_sha_hex(const char *text, char out[65]) {
    static const char HEX[] = "0123456789abcdef";
    unsigned char digest[CRYPTO_SHA256_LEN];
    size_t i;
    crypto_sha256((const unsigned char *)text, strlen(text), digest);
    for (i = 0; i < CRYPTO_SHA256_LEN; i++) {
        out[i * 2] = HEX[digest[i] >> 4];
        out[i * 2 + 1] = HEX[digest[i] & 0x0f];
    }
    out[64] = '\0';
}

/* --- path flavour: PureWindowsPath on this host ---------------------------- */

/* os.sep-normalised, dot components dropped, trailing separator dropped. */
static char *wm6187_clean(const char *path) {
    size_t n, i, j = 0;
    char *out;
    if (!path) return NULL;
    n = strlen(path);
    out = (char *)malloc(n + 2);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = path[i];
        if (c == '\\') c = '/';
        if (c == '/' && j > 0 && out[j - 1] == '/') continue;
        if (c == '/' && j == 0 && n > 1) continue;   /* keep a single leading slash only */
        out[j++] = c;
    }
    /* drop a lone "." component and any "./" pair produced above */
    if (j == 2 && out[0] == '.' && out[1] == '/') { out[0] = '/'; j = 1; }
    while (j > 1 && out[j - 1] == '/') j--;
    /* drop a trailing "/." */
    if (j >= 2 && out[j - 1] == '.' && out[j - 2] == '/') j -= 2;
    while (j > 1 && out[j - 1] == '/') j--;
    out[j] = '\0';
    return out;
}

/* PureWindowsPath.is_absolute(): a drive root ("C:/") or a UNC share. */
static int wm6187_absolute(const char *clean) {
    size_t n;
    if (!clean) return 0;
    n = strlen(clean);
    if (n >= 3 && ((clean[0] >= 'A' && clean[0] <= 'Z') || (clean[0] >= 'a' && clean[0] <= 'z'))
        && clean[1] == ':' && clean[2] == '/') return 1;
    if (n >= 3 && clean[0] == '/' && clean[1] == '/') return 1;
    return 0;
}

/* Are the first `want` chars lowercase hex? */
static int wm6187_hex_n(const char *s, size_t want) {
    size_t i;
    for (i = 0; i < want; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

/* `_API_IMAGE_NAME.fullmatch(name)`: api_<32 lowercase hex>.(png|jpg|gif|webp) */
static int wm6187_api_image(const char *name) {
    size_t n = strlen(name);
    const char *dot;
    if (strncmp(name, "api_", 4) != 0) return 0;
    if (n < 4 + 32 + 1 + 3) return 0;
    if (!wm6187_hex_n(name + 4, 32)) return 0;
    if (name[4 + 32] != '.') return 0;
    dot = name + 4 + 32 + 1;
    if (!strcmp(dot, "png") || !strcmp(dot, "jpg") || !strcmp(dot, "gif") || !strcmp(dot, "webp"))
        return 1;
    return 0;
}

/* "<root>/<64 lowercase hex>/<name>", compared the way Python compares the two
 * PureWindowsPath objects: case-insensitive, separators normalised. Answers the
 * hex component (the reference's "sha256"). */
static int wm6187_hosted_match(const char *value, const char *root_clean, char hex_out[65]) {
    char *clean = wm6187_clean(value);
    const char *rest, *slash;
    size_t rl, hex_len;
    int ok = 0;
    if (!clean || !root_clean) { free(clean); return 0; }
    if (!wm6187_absolute(clean)) { free(clean); return 0; }
    rl = strlen(root_clean);
    if (strncasecmp(clean, root_clean, rl) != 0 || clean[rl] != '/') { free(clean); return 0; }
    rest = clean + rl + 1;
    slash = strchr(rest, '/');
    if (!slash || slash == rest || slash[1] == '\0') { free(clean); return 0; }
    if (strchr(slash + 1, '/') != NULL) { free(clean); return 0; }
    hex_len = (size_t)(slash - rest);
    if (hex_len == 64 && wm6187_hex_n(rest, 64) && !wm6187_api_image(slash + 1)) {
        memcpy(hex_out, rest, hex_len);
        hex_out[hex_len] = '\0';
        ok = 1;
    }
    free(clean);
    return ok;
}

/* --- reference rows -------------------------------------------------------- */

/* One INSERT row for one reference; -1 when the reference has no string path. */
static int wm6187_push_ref(json_t *out, const json_t *ref) {
    const json_t *path;
    char hex[65], key[128];
    json_t *item;
    if (!ref || ref->type != JSON_OBJECT) return -1;
    path = json_obj_get(ref, "path");
    if (!path || path->type != JSON_STRING) return -1;
    wm6187_sha_hex(path->str_val ? path->str_val : "", hex);
    memcpy(key, WM6187_PREFIX, sizeof(WM6187_PREFIX) - 1);
    memcpy(key + sizeof(WM6187_PREFIX) - 1, hex, 65);
    item = json_object();
    if (!item) return -1;
    json_set(item, "key", json_string(key));
    json_set(item, "value", json_copy(ref));
    json_append(out, item);
    return 0;
}

/* list(x[key]['media']) for one payload key; -1 on a non-mapping holder. */
static int wm6187_push_media_list(json_t *out, const json_t *payload, const char *key) {
    const json_t *holder = json_obj_get(payload, key);
    const json_t *media;
    size_t i;
    if (!holder) return 0;
    if (holder->type != JSON_OBJECT) return -1;
    media = json_obj_get(holder, "media");
    if (!media) return 0;
    if (media->type != JSON_ARRAY) return -1;
    for (i = 0; i < media->c.count; i++) {
        if (wm6187_push_ref(out, media->c.items[i]) != 0) return -1;
    }
    return 0;
}

static int wm6187_push_api_turn(json_t *out, const json_t *payload) {
    const json_t *holder = json_obj_get(payload, "api_turn_v1");
    const json_t *media;
    size_t i;
    if (!holder) return 0;
    if (holder->type != JSON_OBJECT) return -1;
    media = json_obj_get(holder, "media");
    if (!media) return 0;
    if (media->type != JSON_ARRAY) return -1;
    for (i = 0; i < media->c.count; i++) {
        if (wm6187_push_ref(out, media->c.items[i]) != 0) return -1;
    }
    return 0;
}

#define WM6187_HOSTED_NEEDLE "\n[Shared attachment] file: "

static int wm6187_push_hosted(json_t *out, const json_t *payload, const char *request_id,
                              const char *media_root) {
    const json_t *text;
    char *root_clean;
    const char *p, *needle = WM6187_HOSTED_NEEDLE;
    size_t needle_len = sizeof(WM6187_HOSTED_NEEDLE) - 1;
    if (!request_id || strncmp(request_id, "hosted:", 7) != 0) return 0;
    text = json_obj_get(payload, "text");
    if (!text || text->type != JSON_STRING) return 0;
    root_clean = wm6187_clean(media_root);
    if (!root_clean) return -1;
    if (!wm6187_absolute(root_clean)) { free(root_clean); return 0; }
    p = text->str_val ? text->str_val : "";
    for (;;) {
        const char *hit = strstr(p, needle);
        const char *vstart, *nl;
        char hex[65];
        if (!hit) break;
        vstart = hit + needle_len;
        nl = strchr(vstart, '\n');
        if (!nl) break;
        if (nl > vstart) {
            char *value = wm6187_dup_n(vstart, (size_t)(nl - vstart));
            if (!value) { free(root_clean); return -1; }
            if (wm6187_hosted_match(value, root_clean, hex)) {
                json_t *ref = json_object();
                if (!ref) { free(value); free(root_clean); return -1; }
                json_set(ref, "path", json_string(value));
                json_set(ref, "sha256", json_string(hex));
                if (wm6187_push_ref(out, ref) != 0) {
                    json_free(ref);
                    free(value);
                    free(root_clean);
                    return -1;
                }
                json_free(ref);
            }
            free(value);
        }
        p = nl + 1;
    }
    free(root_clean);
    return 0;
}

json_t *w22_media_6187d8_retire_media(const json_t *payload, const char *request_id,
                                      const char *media_root) {
    json_t *out;
    if (!payload || payload->type != JSON_OBJECT) return NULL;
    out = json_array();
    if (!out) return NULL;
    if (wm6187_push_media_list(out, payload, "attachments_v1") != 0 ||
        wm6187_push_media_list(out, payload, "native_text_v1") != 0 ||
        (media_root != NULL && wm6187_push_hosted(out, payload, request_id, media_root) != 0) ||
        wm6187_push_api_turn(out, payload) != 0) {
        json_free(out);
        return NULL;
    }
    return out;
}

/* --- spillover archive names ---------------------------------------------- */

typedef struct {
    char **items;
    size_t n, cap;
} wm6187_names;

static int wm6187_name_allowed(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '.' || c == '-';
}

static void wm6187_names_free(wm6187_names *list) {
    size_t i;
    for (i = 0; i < list->n; i++) free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->n = list->cap = 0;
}

static int wm6187_names_add(wm6187_names *list, const char *s, size_t n) {
    size_t i;
    char *copy;
    for (i = 0; i < list->n; i++) {
        if (strlen(list->items[i]) == n && memcmp(list->items[i], s, n) == 0) return 0;
    }
    if (list->n == list->cap) {
        size_t cap = list->cap ? list->cap * 2 : 8;
        char **grown = (char **)realloc(list->items, cap * sizeof(char *));
        if (!grown) return -1;
        list->items = grown;
        list->cap = cap;
    }
    copy = wm6187_dup_n(s, n);
    if (!copy) return -1;
    list->items[list->n++] = copy;
    return 0;
}

/* `spillover(?:/|\\+)([A-Za-z0-9_.-]+)` -- no word boundary, so "xspillover/a" counts. */
static int wm6187_scan_spill(const char *text, wm6187_names *list) {
    const char *p = text;
    while ((p = strstr(p, "spillover")) != NULL) {
        const char *q = p + strlen("spillover");
        const char *start;
        size_t n = 0;
        if (*q == '/') {
            q++;
        } else if (*q == '\\') {
            while (*q == '\\') q++;
        } else {
            p++;
            continue;
        }
        start = q;
        while (wm6187_name_allowed(*q)) { q++; n++; }
        if (n > 0 && wm6187_names_add(list, start, n) != 0) return -1;
        p = q > p ? q : p + 1;
    }
    return 0;
}

/* `cache/pruned/([A-Za-z0-9_.-]+)` */
static int wm6187_scan_pruned(const char *text, wm6187_names *list) {
    const char *p = text;
    const char *needle = "cache/pruned/";
    size_t len = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) {
        const char *start = p + len, *q = start;
        size_t n = 0;
        while (wm6187_name_allowed(*q)) { q++; n++; }
        if (n > 0) {
            char *joined = (char *)malloc(len + n + 1);
            int rc;
            if (!joined) return -1;
            memcpy(joined, needle, len);
            memcpy(joined + len, start, n);
            joined[len + n] = '\0';
            rc = wm6187_names_add(list, joined, len + n);
            free(joined);
            if (rc != 0) return -1;
        }
        p = q > p ? q : p + 1;
    }
    return 0;
}

static int wm6187_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w22_media_6187d8_spillover_records(const json_t *rows) {
    json_t *out;
    wm6187_names list = {NULL, 0, 0};
    size_t i, j;
    if (rows && rows->type != JSON_ARRAY) return NULL;
    out = json_array();
    if (!out) return NULL;
    if (!rows) return out;   /* no rows selected: no INSERT batch */
    for (i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        if (!row || row->type != JSON_OBJECT) { json_free(out); wm6187_names_free(&list); return NULL; }
        for (j = 0; j < 2; j++) {
            const json_t *text = json_obj_get(row, j == 0 ? "content" : "api_content");
            if (!text || text->type != JSON_STRING) continue;
            if (wm6187_scan_spill(text->str_val ? text->str_val : "", &list) != 0 ||
                wm6187_scan_pruned(text->str_val ? text->str_val : "", &list) != 0) {
                json_free(out);
                wm6187_names_free(&list);
                return NULL;
            }
        }
    }
    if (list.n > 1) qsort(list.items, list.n, sizeof(char *), wm6187_cmp);
    for (i = 0; i < list.n; i++) {
        json_t *item = json_object();
        char *key;
        size_t klen = strlen(WM6187_SPILL) + strlen(list.items[i]);
        if (!item) { json_free(out); wm6187_names_free(&list); return NULL; }
        key = (char *)malloc(klen + 1);
        if (!key) { json_free(item); json_free(out); wm6187_names_free(&list); return NULL; }
        strcpy(key, WM6187_SPILL);
        strcat(key, list.items[i]);
        json_set(item, "key", json_string(key));
        json_set(item, "value", json_string(list.items[i]));
        json_append(out, item);
        free(key);
    }
    wm6187_names_free(&list);
    return out;
}

const char *w22_media_6187d8_prefix(void) { return WM6187_PREFIX; }
const char *w22_media_6187d8_spillover_prefix(void) { return WM6187_SPILL; }
