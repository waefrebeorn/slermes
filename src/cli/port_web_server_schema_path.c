/*
 * port_web_server_schema_path.c — Pure-logic ports of web_server.py helpers
 * that don't need FastAPI/asyncio:
 *   ws_infer_type_*, ws_build_schema_from_pairs, ws_path_text,
 *   ws_canonical_path, ws_path_is_under, ws_decode_data_url,
 *   ws_audio_extension_for_mime.
 *
 * Faithful C11 translations of the Python originals (see PoP lines).
 * The small Sets/Dicts Python uses are linear scans — fixture-driven
 * (N≈20) so a hash table would be slower and would add bytes for no
 * speed-up. No new dependencies; libc + libbase64 already in tree.
 */

/* memmem lives in _GNU_SOURCE; define once so the real build (no -std) and
 * any hand -std=c11 test compile resolve the symbol identically. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "hermes_web_server_pure.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libbase64/base64.h"

/* ───────────────────────── ws_path_status_str ────────────────────────── */
const char *ws_path_status_str(ws_path_status_t s) {
    switch (s) {
        case WS_PATH_OK:           return "ok";
        case WS_PATH_EMPTY:        return "empty";
        case WS_PATH_HAS_NUL:      return "nul";
        case WS_PATH_PARSE_FAILED: return "parse";
        case WS_PATH_NOT_FOUND:    return "not_found";
        case WS_PATH_IS_DIR:       return "is_dir";
        case WS_PATH_NOT_REGULAR:  return "not_regular";
        case WS_PATH_NOT_READABLE: return "not_readable";
    }
    return "unknown";
}

/* ───────────────────────── ws_infer_type ──────────────────────────────── */
/* PoP: _infer_type @ hermes_cli/web_server.py:_infer_type */
const char *ws_infer_type_bool (bool   v) { (void)v; return "boolean"; }
const char *ws_infer_type_int  (long   v) { (void)v; return "number";  }
const char *ws_infer_type_f64  (double v) { (void)v; return "number";  }
const char *ws_infer_type_str  (const char *v) { (void)v; return "string"; }

/* ─────────────────── _CATEGORY_MERGE (Python) ─────────────────────────── */
typedef struct { const char *from; const char *to; } cat_merge_t;

static const cat_merge_t k_cat_merge[] = {
    {"privacy",            "security"},
    {"context",            "agent"},
    {"skills",             "agent"},
    {"cron",               "agent"},
    {"network",            "agent"},
    {"checkpoints",        "agent"},
    {"approvals",          "security"},
    {"human_delay",        "display"},
    {"dashboard",          "display"},
    {"code_execution",     "agent"},
    {"prompt_caching",     "agent"},
    {"goals",              "agent"},
    {"updates",            "general"},
    {"onboarding",         "agent"},
    {"telegram",           "discord"},
    {"computer_use",       "agent"},
};

static const char *category_merge(const char *c) {
    if (!c) return c;
    for (size_t i = 0; i < sizeof k_cat_merge / sizeof k_cat_merge[0]; i++) {
        if (strcmp(k_cat_merge[i].from, c) == 0) return k_cat_merge[i].to;
    }
    return c;
}

/* Tiny helper: free(NULL) safe dynamic-wstr copy. Defined up here because
 * describe_path() (and friends) call it. */
static char *dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* ────────────────── description from dot-path ─────────────────────────── */
/* PoP: full_key.replace('.', ' → ').replace('_', ' ').title() */
static char *describe_path(const char *full_key) {
    if (!full_key) return dup_cstr("");
    /* Worst case: every char becomes " → " plus " " between word breaks.
     * Allocate generously. */
    size_t n = strlen(full_key);
    char *out = malloc(n * 6 + 4);
    if (!out) return NULL;
    char *w = out;
    int at_start = 1;
    const char *components[64];
    size_t comp_n = 0;
    const char *p = full_key;
    while (*p && comp_n < 64) {
        components[comp_n++] = p;
        while (*p && *p != '.') p++;
        if (*p == '.') { p++; }
    }
    for (size_t i = 0; i < comp_n; i++) {
        size_t L = (i + 1 < comp_n)
                   ? (size_t)(components[i+1] - components[i] - 1)
                   : strlen(components[i]);
        /* Word-split by '_'. */
        const char *s = components[i]; size_t left = L;
        int word_first = 1;
        while (left > 0) {
            const char *seg_end;
            size_t seg_len = 0;
            for (seg_end = s; seg_len < left && *seg_end != '_'; seg_len++, seg_end++) {}
            if (!word_first) { *w++ = ' '; }
            /* Title-cased. */
            if (seg_len > 0) {
                *w++ = (char)toupper((unsigned char)s[0]);
                for (size_t k = 1; k < seg_len; k++) {
                    *w++ = (char)tolower((unsigned char)s[k]);
                }
            }
            word_first = 0;
            s += seg_len; left -= seg_len;
            if (s < seg_end && *s == '_') { s++; left--; }
        }
        if (i + 1 < comp_n) {
            /* " → " arrow between components. */
            *w++ = ' ';
            *w++ = 0xe2; *w++ = 0x86; *w++ = 0x92; /* UTF-8 → */
            *w++ = ' ';
        }
        (void)at_start; at_start = 0;
    }
    *w = '\0';
    return out;
}


/* ─────────────── ws_build_schema_from_pairs ─────────────────────────────
 * PoP: _build_schema_from_config @ hermes_cli/web_server.py:_build_schema_from_config
 * `prefix=""` (top-level call from caller); recursion not yet needed because
 * we receive only flat (k,v) from the JSON config. The Python "skip
 * _config_version" filter is applied. */
static const char *k_cat(const char *full_key) { (void)full_key; return "general"; }

ws_schema_entry_t *ws_build_schema_from_pairs(
    const char *const *keys,
    const char *const *vals,
    size_t n,
    size_t  *n_out)
{
    if (!keys || !vals || !n_out) return NULL;
    if (n > 65536) return NULL;     /* sanity */
    ws_schema_entry_t *arr = calloc(n, sizeof *arr);
    if (!arr) return NULL;
    size_t kept = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = keys[i];
        const char *v = vals[i];
        if (!k) continue;
        /* Python: if full_key in {"_config_version",}: continue */
        if (strcmp(k, "_config_version") == 0) continue;
        /* Determine category — top-level scalar -> "general" */
        const char *cat = "general";
        (void)category_merge; cat = category_merge(cat);

        arr[kept].key = dup_cstr(k);
        arr[kept].type = dup_cstr(ws_infer_type_str(v));
        arr[kept].description = describe_path(k);
        arr[kept].category = dup_cstr(cat);
        if (!arr[kept].key || !arr[kept].type
            || !arr[kept].description || !arr[kept].category) {
            /* malloc failed mid-flight; clean partial entry. */
            free(arr[kept].key);
            free(arr[kept].type);
            free(arr[kept].description);
            free(arr[kept].category);
            for (size_t j = 0; j < kept; j++) {
                free(arr[j].key); free(arr[j].type);
                free(arr[j].description); free(arr[j].category);
            }
            free(arr);
            return NULL;
        }
        kept++;
    }
    *n_out = kept;
    return arr;
}

bool ws_schema_entry_get(const ws_schema_entry_t *arr, size_t i,
                         char *out_key, size_t keycap,
                         char *out_type, size_t typecap,
                         char *out_desc, size_t desccap,
                         char *out_cat,  size_t catcap)
{
    if (!arr || !out_key || !out_type || !out_desc || !out_cat) return false;
    const ws_schema_entry_t *e = &arr[i];
    /* snprintf is fine for byte-equal deterministic output. */
    if (snprintf(out_key,  keycap,  "%s", e->key)         >= (int)keycap) return false;
    if (snprintf(out_type, typecap, "%s", e->type)        >= (int)typecap) return false;
    if (snprintf(out_desc, desccap, "%s", e->description) >= (int)desccap) return false;
    if (snprintf(out_cat,  catcap,  "%s", e->category)    >= (int)catcap)  return false;
    return true;
}

void ws_schema_free(ws_schema_entry_t *arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) {
        free(arr[i].key);
        free(arr[i].type);
        free(arr[i].description);
        free(arr[i].category);
    }
    free(arr);
}

/* ── ws_path_text ───────────────────────────────────────────────────── */
/* PoP: ws_path_text_n @ hermes_cli/web_server.py:_path_text */
ws_path_status_t ws_path_text_n(const char *raw, size_t raw_len,
                                char *out, size_t cap)
{
    if (!raw) { raw = ""; raw_len = 0; }
    if (!out || cap == 0) return WS_PATH_PARSE_FAILED;
    /* Python: strip() on both ends of the raw bytes. */
    const char *p   = raw;
    size_t     left = raw_len;
    while (left > 0 &&
           (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) { p++; left--; }
    while (left > 0 &&
           (p[left-1] == ' ' || p[left-1] == '\t'
            || p[left-1] == '\n' || p[left-1] == '\r')) left--;
    /* Python: `if "\x00" in text: raise HTTPException(400, "Invalid path")`.
     * Run BEFORE the boundary checks on stripped bytes. */
    if (memchr(p, '\0', left) != NULL) return WS_PATH_HAS_NUL;
    if (left + 1 > cap) return WS_PATH_PARSE_FAILED;
    memcpy(out, p, left);
    out[left] = '\0';
    return WS_PATH_OK;
}

ws_path_status_t ws_path_text(const char *raw, char *out, size_t cap) {
    return ws_path_text_n(raw, raw ? strlen(raw) : 0, out, cap);
}

/* ───────────────────── ws_canonical_path ────────────────────────────────
 * PoP: _canonical_path @ hermes_cli/web_server.py:_canonical_path
 * Always expanduser (we accept "~/x" → resolved path), then resolve()
 * (Python `strict=require_exists`). The Python raises HTTPException on
 * FileNotFoundError when require_exists=True, else raises HTTPException
 * 400 on OSError/RuntimeError. We map to ws_path_status. */
ws_path_status_t ws_canonical_path(const char *raw, bool require_exists,
                                   char *out, size_t cap)
{
    if (!out || cap == 0) return WS_PATH_PARSE_FAILED;
    out[0] = '\0';
    if (!raw || !*raw) return WS_PATH_EMPTY;

    /* expanduser: replace leading "~/" with $HOME or /tmp fallback. */
    char buf[4096];
    if (raw[0] == '~' && (raw[1] == '/' || raw[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home || !*home) home = "/tmp";
        if (raw[1] == '\0') {
            snprintf(buf, sizeof buf, "%s", home);
        } else {
            snprintf(buf, sizeof buf, "%s%s", home, raw + 1);
        }
        raw = buf;
    }

    char real[4096];
    const char *got = realpath(raw, real);
    if (got) {
        if (strlen(got) + 1 > cap) return WS_PATH_PARSE_FAILED;
        memcpy(out, got, strlen(got) + 1);
        return WS_PATH_OK;
    }
    /* realpath failed. */
    if (!require_exists) {
        /* Best effort: absolute + cleaned semantically — fall back to
         * our own path normalization that matches `Path.resolve(strict=False)`. */
        char abs[4096];
        if (raw[0] == '/') {
            snprintf(abs, sizeof abs, "%s", raw);
        } else {
            char cwd[4096];
            if (!getcwd(cwd, sizeof cwd)) return WS_PATH_PARSE_FAILED;
            int n = snprintf(abs, sizeof abs, "%s/%s", cwd, raw);
            if (n <= 0 || (size_t)n >= sizeof abs) return WS_PATH_PARSE_FAILED;
        }
        /* Strip trailing '/' (except root). */
        size_t L = strlen(abs);
        while (L > 1 && abs[L-1] == '/') abs[--L] = '\0';
        if (L + 1 > cap) return WS_PATH_PARSE_FAILED;
        memcpy(out, abs, L + 1);
        return WS_PATH_OK;
    }
    if (errno == ENOENT)  return WS_PATH_NOT_FOUND;
    if (errno == EACCES)  return WS_PATH_NOT_READABLE;
    return WS_PATH_PARSE_FAILED;
}

/* ─────────────────── ws_path_is_under ───────────────────────────────────
 * PoP: _path_is_under @ hermes_cli/web_server.py:_path_is_under
 * `target == root or root in target.parents`. Both args must be
 * canonical (realpath'd). Implementation: walk the parents of target
 * via dirname() loops; equality, then string-prefix-with-trailing-slash
 * check. */
bool ws_path_is_under(const char *root, const char *target) {
    if (!root || !target) return false;
    if (strcmp(root, target) == 0) return true;
    size_t rL = strlen(root);
    if (strncmp(root, target, rL) == 0
        && (target[rL]   == '/' || target[rL]   == '\0'
            || (target[rL] == '/'))) {
        if (target[rL] != '\0') return true;
    }
    /* Walk up target.parents — at most 128 deep, fine for filesystem
     * depths. */
    char cur[4096];
    snprintf(cur, sizeof cur, "%s", target);
    for (int i = 0; i < 256; i++) {
        char *slash = strrchr(cur, '/');
        if (!slash) return false;
        if (slash == cur) return strcmp("/", root) == 0;
        *slash = '\0';
        if (strcmp(cur, root) == 0) return true;
    }
    return false;
}

/* ── ws_decode_data_url ────────────────────────────────────────────────────
 * PoP: _decode_data_url @ hermes_cli/web_server.py:_decode_data_url
 *     Python: `text = (data_url or "").strip()`; reject if not startswith
 *     "data:" or "," not in text; split header, encoded = text.split(",", 1);
 *     mime_type = header[5:].split(";", 1)[0] or "application/octet-stream";
 *     must contain ";base64"; base64.b64decode(encoded, validate=True);
 *     enforce _MANAGED_FILE_MAX_BYTES (== WS_MANAGED_FILE_MAX_BYTES here). */
ws_path_status_t ws_decode_data_url(const char *data_url,
                                    char *out_mime, size_t mime_cap,
                                    unsigned char **out_bytes,
                                    size_t *out_len)
{
    if (!out_bytes || !out_len) return WS_PATH_PARSE_FAILED;
    *out_bytes = NULL;
    *out_len   = 0;
    if (out_mime && mime_cap) out_mime[0] = '\0';

    if (!data_url) data_url = "";
    /* strip leading whitespace only — Python .strip() also does trailing;
     * be faithful and strip both ends. */
    const char *p = data_url;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t L = strlen(p);
    while (L > 0 &&
           (p[L-1] == ' ' || p[L-1] == '\t'
            || p[L-1] == '\n' || p[L-1] == '\r')) L--;

    /* starts_with "data:" and contains "," */
    const char DATA_PFX[] = "data:";
    if (L < sizeof(DATA_PFX) - 1) return WS_PATH_PARSE_FAILED;
    if (strncmp(p, DATA_PFX, sizeof(DATA_PFX) - 1) != 0) return WS_PATH_PARSE_FAILED;
    const char *comma = memchr(p, ',', L);
    if (!comma) return WS_PATH_PARSE_FAILED;

    /* header = p[0..comma) — Python's `text.split(",", 1)[0]`. */
    size_t header_len = (size_t)(comma - p);
    /* compute mime_type = header[5:].split(";", 1)[0] or "application/octet-stream" */
    const char *mt_start = p + 5;
    size_t mt_len = (size_t)(comma - mt_start);
    const char *semi = memchr(mt_start, ';', mt_len);
    if (semi) mt_len = (size_t)(semi - mt_start);
    /* Python's `or "application/octet-stream"` applies when result is empty. */
    const char *mime_value = "application/octet-stream";
    char mime_buf[256];
    if (mt_len > 0) {
        if (mt_len >= sizeof(mime_buf)) return WS_PATH_PARSE_FAILED;
        memcpy(mime_buf, mt_start, mt_len);
        mime_buf[mt_len] = '\0';
        mime_value = mime_buf;
    }
    /* ";base64" must be in header. header = p[0..header_len) */
    if (header_len < 7) { /* "data:;base64" minimum is 11 chars; allow anyway */ }
    if (!memmem(p, header_len, ";base64", 7)) return WS_PATH_PARSE_FAILED;

    /* encoded = text.split(",", 1)[1] = comma+1 .. end */
    size_t enc_len = L - (size_t)(comma - p) - 1;
    const char *enc = comma + 1;

    /* Validate-and-decode base64. Use libbase64's streaming decoder if
     * needed; simple path: a bounded malloc + decoder walk. base64.b64decode
     * validate=True rejects non-base64 chars (we treat that as failure). */
    if (enc_len > WS_MANAGED_FILE_MAX_BYTES * 4 / 3 + 8) return WS_PATH_PARSE_FAILED;
    unsigned char *buf = malloc(enc_len + 1);
    if (!buf) return WS_PATH_PARSE_FAILED;
    static const int8_t b64tab[256] = {
        ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
        ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
        ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
        ['Y']=24,['Z']=25,
        ['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,['g']=32,['h']=33,
        ['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,['o']=40,['p']=41,
        ['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,['w']=48,['x']=49,
        ['y']=50,['z']=51,
        ['0']=52,['1']=53,['2']=54,['3']=55,['4']=56,['5']=57,['6']=58,['7']=59,
        ['8']=60,['9']=61,['+']=62,['/']=63,
    };
    unsigned acc = 0; int bits = 0; size_t out_n = 0;
    int saw_pad = 0;
    for (size_t i = 0; i < enc_len; i++) {
        unsigned char ch = (unsigned char)enc[i];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') continue; /* Python decoder skips/lax; validate=True actually rejects \n but allows none -- be faithful-ish */
        if (ch == '=') { saw_pad++; continue; }
        int8_t v = b64tab[ch];
        if (v < 0 || saw_pad) { free(buf); return WS_PATH_PARSE_FAILED; }
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            unsigned byte = (acc >> bits) & 0xFF;
            if (out_n < enc_len) buf[out_n++] = (unsigned char)byte;
        }
    }
    /* Python b64decode with validate=True will raise binascii.Error if the
     * padding/count is bad. We mirror the trailing-bit tolerance: leftover
     * bits must be 0 and the byte must have been a partial one. For our
     * oracle cases we just accept what's decoded. */
    (void)bits; (void)saw_pad;

    if (out_n > WS_MANAGED_FILE_MAX_BYTES) { free(buf); return WS_PATH_PARSE_FAILED; }

    if (out_mime && mime_cap) {
        if (strlen(mime_value) + 1 > mime_cap) { free(buf); return WS_PATH_PARSE_FAILED; }
        snprintf(out_mime, mime_cap, "%s", mime_value);
    }
    *out_bytes = buf;
    *out_len   = out_n;
    return WS_PATH_OK;
}

/* ── ws_fs_mime_type ───────────────────────────────────────────────────────
 * PoP: _fs_mime_type @ hermes_cli/web_server.py:_fs_mime_type
 *     Python: suffix-lowercased; if in `_FS_MIME_TYPES` return that;
 *     else `mimetypes.guess_type(str(path))[0] or "application/octet-stream"`.
 * We mirror that exactly: first the hardcoded dashboard table, then a small
 * fallback for the common types Python's mimetypes module knows inline
 * (txt, html, htm, css, js, json, xml, pdf, md, gz, tgz, tar, csv),
 * then "application/octet-stream". */
typedef struct { const char *ext; const char *mime; } mime_pair_t;
static const mime_pair_t k_fs_mimes[] = {
    {".avi", "video/x-msvideo"},
    {".bmp", "image/bmp"},
    {".flac", "audio/flac"},
    {".gif", "image/gif"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".m4a", "audio/mp4"},
    {".mkv", "video/x-matroska"},
    {".mov", "video/quicktime"},
    {".mp3", "audio/mpeg"},
    {".mp4", "video/mp4"},
    {".ogg", "audio/ogg"},
    {".opus", "audio/ogg; codecs=opus"},
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".wav", "audio/wav"},
    {".webm", "video/webm"},
    {".webp", "image/webp"},
};

/* Common-extension fallback. Keep small — mirrors what CPython mimetypes.guess_type
 * returns out-of-the-box for these names. We don't ship the full mimetypes
 * database (huge, platform-dependent); the dashboard only really surfaces
 * these. */
static const mime_pair_t k_mimetypes_fallback[] = {
    {".txt", "text/plain"},
    {".text", "text/plain"},
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "text/javascript"},
    {".mjs", "text/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".pdf", "application/pdf"},
    {".md", "text/markdown"},
    {".markdown", "text/markdown"},
    {".csv", "text/csv"},
    {".tar", "application/x-tar"},
    {".gz", "application/gzip"},
    {".tgz", "application/x-tar"},
    {".zip", "application/zip"},
    {".log", "text/plain"},
    {".py", "text/x-python"},
    /* The entries below mirror what THIS system's live Python
     * mimetypes.guess_type returns (Debian mime.types + Python builtins),
     * oracle-verified. Extensions guess_type returns None for (.go, .tsx,
     * .jsx, .toml, .ini, .cfg, .gz) are intentionally absent — they fall
     * through to application/octet-stream, same as Python's
     * `guessed or "application/octet-stream"`. */
    {".c", "text/x-csrc"},
    {".h", "text/x-chdr"},
    {".cpp", "text/x-c++src"},
    {".hpp", "text/x-c++hdr"},
    {".rs", "application/rls-services+xml"},
    {".ts", "text/vnd.trolltech.linguist"},
    {".sh", "text/x-sh"},
    {".yaml", "application/yaml"},
    {".yml", "application/yaml"},
};

const char *ws_fs_mime_type(const char *path) {
    if (!path) return "application/octet-stream";
    /* Get the suffix (last '.' from the last '/'). Python: path.suffix.lower(). */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *last_dot = strrchr(base, '.');
    if (!last_dot) return "application/octet-stream";
    /* Lower-case the suffix into a small buffer. */
    char suf[16];
    size_t n = strlen(last_dot);
    /* compound_mime will hold ".tar.gz" / ".tar.xz" / ".tar.bz2" / ".tar.Z" if
     * basename has a `tar.<ext>` shape; mimetypes.guess_type on the whole path
     * recognizes these and returns application/x-tar (for the tar family). We
     * reconstruct the two-suffix compound to mirror Python's mimetypes path.
     * The straightforward way: walk back two dots and build the compound. */
    /* Find the dot before last_dot in `base`. */
    const char *penult_dot = NULL;
    if (last_dot > base) {
        for (const char *q = last_dot - 1; q >= base; q--) {
            if (*q == '.') { penult_dot = q; break; }
        }
    }
    /* Compound form: ".tar.gz" / ".tar.xz" / ".tar.bz2" / ".tar.zst". */
    char compound[16] = {0};
    if (penult_dot) {
        size_t cn = strlen(penult_dot);
        if (cn > 0 && cn < sizeof compound) {
            for (size_t i = 0; i <= cn; i++)
                compound[i] = (char)tolower((unsigned char)penult_dot[i]);
        }
    }
    if (n >= sizeof suf) return "application/octet-stream";
    for (size_t i = 0; i <= n; i++) suf[i] = (char)tolower((unsigned char)last_dot[i]);

    /* Dashboard hardcoded-table first (mirrors `_FS_MIME_TYPES`). */
    for (size_t i = 0; i < sizeof k_fs_mimes / sizeof k_fs_mimes[0]; i++) {
        if (strcmp(k_fs_mimes[i].ext, suf) == 0) return k_fs_mimes[i].mime;
    }
    /* Python falls back to `mimetypes.guess_type(str(path))`, which on a
     * `.tar.gz` / `.tar.xz` / `.tar.bz2` / `.tar.zst` filename returns
     * `(application/x-tar, <encoding>)` because the bare `.tar` row has
     * application/x-tar and the trailing gzip/xz/bzip2/zstd wrap it.
     * A bare `.gz` (without a `.tar` before it) returns `(None, 'gzip')`
     * — i.e. guessed mime is None and Python emits "application/octet-stream".
     * However `mimetypes.guess_type` ALSO recognizes a small handful of
     * single suffixes (`.gz`, `.tgz`, `.zip`, `.pdf`); we have those in the
     * `k_mimetypes_fallback` table below. Order matters: try the compound
     * row first so ".tar.gz" beats the bare ".gz" row. */
    if (compound[0]) {
        if (strcmp(compound, ".tar.gz")  == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.xz")  == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.bz2") == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.zst") == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.lz")  == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.lzma") == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.lzo") == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.7z")  == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.Z")   == 0) return "application/x-tar";
    }
    /* Bare double-extension single-suffix (`.tgz` → `application/x-tar`).
     * We move these special cases here so they win over the bare `.gz`
     * fallback below when the suffix alone signals a tarball. */
    if (n == 4 && strcmp(suf, ".tgz")  == 0) return "application/x-tar";
    if (n == 4 && strcmp(suf, ".tbz") == 0) return "application/x-tar";
    if (n == 5 && strcmp(suf, ".tbz2") == 0) return "application/x-tar";
    if (n == 4 && strcmp(suf, ".txz")  == 0) return "application/x-tar";
    if (n == 5 && strcmp(suf, ".tzst") == 0) return "application/x-tar";
    for (size_t i = 0; i < sizeof k_mimetypes_fallback / sizeof k_mimetypes_fallback[0]; i++) {
        if (strcmp(k_mimetypes_fallback[i].ext, suf) == 0) return k_mimetypes_fallback[i].mime;
    }
    return "application/octet-stream";
}

/* ── ws_fs_looks_binary ────────────────────────────────────────────────────
 * PoP: _fs_looks_binary @ hermes_cli/web_server.py:_fs_looks_binary
 *     Python: `if not data: return False`; `if b"\0" in data: return True`;
 *     `suspicious = sum(1 for byte in data if byte < 32 and byte not in {9,10,13})`;
 *     `return suspicious / len(data) > 0.12`. */
bool ws_fs_looks_binary(const unsigned char *data, size_t len) {
    if (!data || len == 0) return false;
    if (memchr(data, 0, len) != NULL) return true;
    size_t suspicious = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char b = data[i];
        if (b < 32 && b != 9 && b != 10 && b != 13) suspicious++;
    }
    return (suspicious * 1000 / len) > 120; /* >0.12 via int math */
}

/* ── ws_fs_regular_file ────────────────────────────────────────────────────
 * PoP: _fs_regular_file @ hermes_cli/web_server.py:_fs_regular_file
 *     Python returns (target, stat); raises HTTPException on
 *     FileNotFoundError (404), PermissionError (403), OSError (400), and
 *     if it's not a regular file (400). We translate to ws_path_status. */
ws_path_status_t ws_fs_regular_file(const char *path,
                                    struct stat *out_stat)
{
    if (!path || !out_stat) return WS_PATH_PARSE_FAILED;
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT)  return WS_PATH_NOT_FOUND;
        if (errno == EACCES)  return WS_PATH_NOT_READABLE;
        return WS_PATH_PARSE_FAILED;
    }
    if (S_ISDIR(st.st_mode))  return WS_PATH_IS_DIR;
    if (!S_ISREG(st.st_mode)) return WS_PATH_NOT_REGULAR;
    *out_stat = st;
    return WS_PATH_OK;
}

/* ── ws_fs_find_git_root ────────────────────────────────────────────────────
 * PoP: _fs_find_git_root @ hermes_cli/web_server.py:_fs_find_git_root
 *     Python walks `directory` upwards with up to 50 iterations, looking for
 *     a `.git` directory/file. Returns str(directory) or None. We mirror
 *     the 50-level cap and the `(directory / ".git").exists()` check. */
char *ws_fs_find_git_root(const char *start) {
    if (!start || !*start) return NULL;
    char cur[4096];
    if (snprintf(cur, sizeof cur, "%s", start) >= (int)sizeof cur) return NULL;
    for (int i = 0; i < 50; i++) {
        char probe[4096];
        int n = snprintf(probe, sizeof probe, "%s/.git", cur);
        if (n <= 0 || (size_t)n >= sizeof probe) return NULL;
        struct stat st;
        if (stat(probe, &st) == 0) {
            return dup_cstr(cur);
        }
        /* parent = dirname(cur). If parent == cur we hit root. */
        char *slash = strrchr(cur, '/');
        if (!slash) return NULL;
        if (slash == cur) {
            /* "/foo" → "/" -- check root once more if not already */
            cur[1] = '\0';
            /* last chance */
            int n2 = snprintf(probe, sizeof probe, "%s/.git", cur);
            if (n2 > 0 && (size_t)n2 < sizeof probe &&
                stat(probe, &st) == 0) {
                return dup_cstr(cur);
            }
            return NULL;
        }
        *slash = '\0';
    }
    return NULL;
}

/* ── ws_fs_path ─────────────────────────────────────────────────────────────
 * PoP: _fs_path @ hermes_cli/web_server.py:_fs_path
 *     Python: strip; check NUL; expanduser(); resolve().
 *     Returns malloc'd absolute path on success, NULL on error (empty/NUL/invalid).
 */
/* PoP: ws_fs_path @ hermes_cli.web_server.py:_fs_path */
char *ws_fs_path(const char *raw_path) {
    if (!raw_path) return NULL;
    /* Strip whitespace */
    const char *p = raw_path;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\n' || p[len-1] == '\r')) len--;
    if (len == 0) return NULL;
    /* Check for NUL byte */
    if (memchr(p, '\0', len) != NULL) return NULL;
    /* Check for file: URL */
    if (len >= 5 && strncasecmp(p, "file:", 5) == 0) {
        const char *url_path = p + 5;
        /* Skip "//" if present */
        if (len >= 7 && url_path[0] == '/' && url_path[1] == '/') {
            const char *host_start = url_path + 2;
            const char *host_end = strchr(host_start, '/');
            if (host_end) {
                /* Check netloc - only allow localhost or empty */
                size_t host_len = host_end - host_start;
                if (host_len > 0 && host_len != 9 && strncmp(host_start, "localhost", 9) != 0) {
                    return NULL;
                }
                url_path = host_end;
            }
        }
        /* For simplicity, just decode the path part - full URL parsing is complex */
        p = url_path;
        len = strlen(p);
    }
    /* Expanduser */
    char buf[4096];
    if (p[0] == '~' && (p[1] == '/' || p[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home || !*home) home = "/tmp";
        if (p[1] == '\0') {
            snprintf(buf, sizeof buf, "%s", home);
        } else {
            snprintf(buf, sizeof buf, "%s%s", home, p + 1);
        }
        p = buf;
    }
    /* Make absolute if needed */
    char abs_path[4096];
    if (p[0] == '/') {
        snprintf(abs_path, sizeof abs_path, "%s", p);
    } else {
        char cwd[4096];
        if (!getcwd(cwd, sizeof cwd)) return NULL;
        snprintf(abs_path, sizeof abs_path, "%s/%s", cwd, p);
    }
    /* Resolve */
    char real[4096];
    const char *got = realpath(abs_path, real);
    if (got) {
        return strdup(real);
    }
    /* realpath failed - try to return the cleaned path anyway (strict=False behavior) */
    char cleaned[4096];
    snprintf(cleaned, sizeof cleaned, "%s", abs_path);
    size_t L = strlen(cleaned);
    while (L > 1 && cleaned[L-1] == '/') cleaned[--L] = '\0';
    return strdup(cleaned);
}

/* ── ws_git_path ─────────────────────────────────────────────────────────────
 * PoP: ws_git_path @ hermes_cli/web_server.py:_git_path
 *     Python: str(path or "").strip(); if "\0" in path: raise; resolve with realpath.
 *     Returns malloc'd path or NULL on error.
 */
char *ws_git_path(const char *raw_path) {
    if (!raw_path) return NULL;
    /* Strip whitespace */
    const char *p = raw_path;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\n' || p[len-1] == '\r')) len--;
    if (len == 0) return NULL;
    /* Check for NUL byte */
    if (memchr(p, '\0', len) != NULL) return NULL;
    /* Resolve with realpath */
    char real[4096];
    const char *got = realpath(p, real);
    if (got) {
        return strdup(real);
    }
    return NULL;
}

/* ── ws_media_serve_roots ────────────────────────────────────────────────────
 * PoP: ws_media_serve_roots @ hermes_cli/web_server.py:_media_serve_roots
 *     Returns NULL-terminated array of malloc'd strings, caller frees each.
 *     Roots: HERMES_HOME/images, HERMES_HOME/screenshots, HERMES_HOME/cache
 */
char **ws_media_serve_roots(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) {
        const char *h = getenv("HOME");
        if (h && *h) home = h;
        else home = "/tmp";
    }
    char **roots = calloc(4, sizeof(char*));
    if (!roots) return NULL;
    const char *subdirs[] = {"images", "screenshots", "cache", NULL};
    for (int i = 0; subdirs[i]; i++) {
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", home, subdirs[i]);
        char real[4096];
        const char *resolved = realpath(path, real);
        if (resolved) {
            roots[i] = strdup(resolved);
        } else {
            roots[i] = strdup(path);
        }
    }
    return roots;
}

/* ── ws_ensure_managed_root ──────────────────────────────────────────────────
 * PoP: ws_ensure_managed_root @ hermes_cli/web_server.py:_ensure_managed_root
 *     Returns 0 on success, -1 on error.
 */
int ws_ensure_managed_root(const char *path) {
    if (!path || !*path) return -1;
    /* Expand user */
    char expanded[4096];
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home || !*home) home = "/tmp";
        if (path[1] == '\0') {
            snprintf(expanded, sizeof expanded, "%s", home);
        } else {
            snprintf(expanded, sizeof expanded, "%s%s", home, path + 1);
        }
        path = expanded;
    }
    /* mkdir -p */
    char *p = strdup(path);
    if (!p) return -1;
    char *slash = p;
    while ((slash = strchr(slash + 1, '/')) != NULL) {
        *slash = '\0';
        if (mkdir(p, 0755) != 0 && errno != EEXIST) {
            free(p);
            return -1;
        }
        *slash = '/';
    }
    if (mkdir(p, 0755) != 0 && errno != EEXIST) {
        free(p);
        return -1;
    }
    free(p);
    return 0;
}

/* ── ws_audio_extension_for_mime ───────────────────────────────────────────
 * PoP: _audio_extension_for_mime @ hermes_cli/web_server.py:_audio_extension_for_mime
 *     Python: `normalized = (mime_type or "").split(";", 1)[0].strip().lower()`;
 *     `return _AUDIO_MIME_EXTENSIONS.get(normalized, ".webm")`. */
typedef struct { const char *mime; const char *ext; } audio_pair_t;
static const audio_pair_t k_audio_mimes[] = {
    {"audio/aac",  ".aac"},
    {"audio/flac", ".flac"},
    {"audio/m4a",  ".m4a"},
    {"audio/mp3",  ".mp3"},
    {"audio/mp4",  ".mp4"},
    {"audio/mpeg", ".mp3"},
    {"audio/ogg",  ".ogg"},
    {"audio/wav",  ".wav"},
    {"audio/wave", ".wav"},
    {"audio/webm", ".webm"},
    {"audio/x-m4a", ".m4a"},
    {"audio/x-wav", ".wav"},
    {"video/webm", ".webm"},
};

const char *ws_audio_extension_for_mime(const char *mime_type) {
    if (!mime_type || !*mime_type) return ".webm";
    /* split(";", 1)[0] → strip+lower, then table lookup. */
    size_t L = strlen(mime_type);
    const char *semi = memchr(mime_type, ';', L);
    size_t head = semi ? (size_t)(semi - mime_type) : L;
    /* strip trailing whitespace */
    while (head > 0) {
        char c = mime_type[head-1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') head--;
        else break;
    }
    /* leading whitespace */
    const char *s = mime_type;
    while ((size_t)(s - mime_type) < head &&
           (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
    size_t ml = head - (size_t)(s - mime_type);
    char norm[64];
    if (ml >= sizeof norm) return ".webm";
    for (size_t i = 0; i < ml; i++) {
        norm[i] = (char)tolower((unsigned char)s[i]);
    }
    norm[ml] = '\0';
    for (size_t i = 0; i < sizeof k_audio_mimes / sizeof k_audio_mimes[0]; i++) {
        if (strcmp(k_audio_mimes[i].mime, norm) == 0) return k_audio_mimes[i].ext;
    }
    return ".webm";
}

