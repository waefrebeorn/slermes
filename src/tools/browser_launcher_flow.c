/*
 * browser_launcher_flow.c — launch-decision FLOW cores of
 * tools/browser_launcher.py. See include/browser_launcher_flow.h for the
 * universal-seam contract and the seam name table.
 *
 * Decision sequences only: every live OS/process I/O (file reads, readlink,
 * mkdir/rmtree/unlink, pid/host facts, clock, sleeps, HTTP probes, config
 * reads, sibling launch steps) enters through blf_io_t.call. Python
 * truthiness, or-chain short-circuits, `is None` vs truthy tests, exception
 * catch scopes (OSError only where the live code catches OSError) and call
 * ORDER are transcribed per call site.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). SHA-256
 * is implemented inline so _default_user_data_dir's profile digest needs no
 * libhash object. The harness #includes this .c directly.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "browser_launcher_flow.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── module constants (browser_launcher.py) ── */
#define BLF_DEVTOOLS_FILE "DevToolsActivePort"
#define BLF_LAUNCH_LOG "hermes-chromium.log"
#define BLF_MIN_FIRST_LAUNCH_TIMEOUT_S 120.0
#define BLF_POLL_INTERVAL_S 0.1
#define BLF_STDERR_TAIL_CHARS 1500

/* ── JSON / Python-value helpers ─────────────────────────────────────────── */

static bool blf_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool blf_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool blf_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool blf_is_str(const json_t *v) { return v && v->type == JSON_STRING; }

static const char *blf_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Python truthiness (NULL pointer == None). */
static bool blf_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static double blf_num(const json_t *v, double def) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

/* Python str() of an integral number (f"{pid}", f"{rc}"); floats outside the
 * integral fixture scope use %.17g. */
static void blf_num_str(char *buf, size_t cap, double d) {
    if (d == (double)(long long)d && d >= -9.2e18 && d <= 9.2e18)
        snprintf(buf, cap, "%lld", (long long)d);
    else
        snprintf(buf, cap, "%.17g", d);
}

/* ── args builders (None-valued keys dropped on both sides) ── */

static void blf_arg_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void blf_arg_j(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void blf_arg_b(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
static void blf_arg_n(json_t *a, const char *k, double v) {
    json_set(a, k, json_number(v));
}

/* ── exception specs ─────────────────────────────────────────────────────── */

/* isinstance(exc, cls) for the classes the live catch scopes name. */
static bool blf_exc_is(const json_t *exc, const char *cls) {
    const char *n = json_get_str(exc, "__raise__", "");
    if (strcmp(cls, "OSError") == 0) {
        static const char *const oserr[] = {
            "OSError", "IOError", "FileNotFoundError", "FileExistsError",
            "PermissionError", "IsADirectoryError", "NotADirectoryError",
            "InterruptedError", "BlockingIOError", "ChildProcessError",
            "ProcessLookupError", "TimeoutError", "ConnectionError",
            "ConnectionAbortedError", "ConnectionRefusedError",
            "ConnectionResetError", "BrokenPipeError",
        };
        for (size_t i = 0; i < sizeof(oserr) / sizeof(oserr[0]); i++)
            if (strcmp(n, oserr[i]) == 0) return true;
        return false;
    }
    return strcmp(n, cls) == 0;
}

/* Universal seam call: frees args, returns the NEW scripted result (NULL ==
 * None) or transfers a raise-spec into *exc. */
static json_t *blf_call(const blf_io_t *io, const char *name, json_t *args,
                        json_t **exc) {
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__")) {
        *exc = r;
        return NULL;
    }
    return r;
}

/* Python == for the scalar domain the resolution seams return (str|None). */
static bool blf_json_eq(const json_t *a, const json_t *b) {
    bool an = blf_is_null(a), bn = blf_is_null(b);
    if (an || bn) return an && bn;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "",
                          b->str_val ? b->str_val : "") == 0;
        default: {
            char *sa = json_serialize(a), *sb = json_serialize(b);
            bool eq = sa && sb && strcmp(sa, sb) == 0;
            free(sa); free(sb);
            return eq;
        }
    }
}

/* ── string helpers ──────────────────────────────────────────────────────── */

/* str.strip(): ASCII whitespace + U+0085 / U+00A0 (the fixture whitespace
 * domain; other unicode spaces are out of scope). malloc'd copy. */
static char *blf_strip(const char *s) {
    if (!s) s = "";
    const char *p = s;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) { p++; continue; }
        if (c == 0xC2 && ((unsigned char)p[1] == 0x85 || (unsigned char)p[1] == 0xA0)) { p += 2; continue; }
        break;
    }
    const char *end = p + strlen(p);
    while (end > p) {
        unsigned char c = (unsigned char)end[-1];
        if (c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) { end--; continue; }
        if (end - p >= 2 && (unsigned char)end[-2] == 0xC2
            && (c == 0x85 || c == 0xA0)) { end -= 2; continue; }
        break;
    }
    size_t n = (size_t)(end - p);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

/* str.isdigit() for the ASCII digit fixture domain (non-empty, all 0-9). */
static bool blf_isdigit(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++)
        if (*p < '0' || *p > '9') return false;
    return true;
}

/* ASCII lower copy (the "sandbox" probe substring is ASCII). */
static char *blf_lower_dup(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

/* Text-mode universal newlines: \r\n -> \n, lone \r -> \n. malloc'd. */
static char *blf_universal_newlines(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\r') {
            out[o++] = '\n';
            if (i + 1 < n && s[i + 1] == '\n') i++;
        } else {
            out[o++] = s[i];
        }
    }
    out[o] = '\0';
    return out;
}

/* fh.readline() over translated content: keeps the trailing \n, "" at EOF. */
static char *blf_readline(const char *content, size_t *pos) {
    size_t n = strlen(content);
    if (*pos >= n) return strdup("");
    const char *nl = strchr(content + *pos, '\n');
    char *out;
    if (nl) {
        size_t len = (size_t)(nl - (content + *pos)) + 1;
        out = (char *)malloc(len + 1);
        if (out) { memcpy(out, content + *pos, len); out[len] = '\0'; }
        *pos += len;
    } else {
        out = strdup(content + *pos);
        *pos = n;
    }
    return out;
}

/* ── POSIX path math (Path.parent / os.path.join on posix-spelled inputs) ── */

static char *blf_posix_join(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (b[0] == '/') return strdup(b);
    if (!a[0]) return strdup(b);
    size_t la = strlen(a), lb = strlen(b);
    int sep = (a[la - 1] != '/');
    char *out = (char *)malloc(la + (size_t)sep + lb + 1);
    if (!out) return NULL;
    memcpy(out, a, la);
    size_t o = la;
    if (sep) out[o++] = '/';
    memcpy(out + o, b, lb);
    out[o + lb] = '\0';
    return out;
}

static char *blf_posix_parent(const char *p) {
    if (!p) p = "";
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') n--;
    if (n == 0) return strdup(".");
    size_t k = n;
    while (k > 0 && p[k - 1] != '/') k--;
    if (k == 0) return strdup(".");
    if (k == 1) return strdup("/");
    char *out = (char *)malloc(k);
    if (!out) return NULL;
    memcpy(out, p, k - 1);
    out[k - 1] = '\0';
    return out;
}

/* ── base64 (read_bytes seam scripts carry {"b64": ...}) ── */

static unsigned char *blf_b64_decode(const char *s, size_t *out_len) {
    static const signed char tab[256] = {
        ['A'] = 0, ['B'] = 1, ['C'] = 2, ['D'] = 3, ['E'] = 4, ['F'] = 5,
        ['G'] = 6, ['H'] = 7, ['I'] = 8, ['J'] = 9, ['K'] = 10, ['L'] = 11,
        ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15, ['Q'] = 16, ['R'] = 17,
        ['S'] = 18, ['T'] = 19, ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
        ['Y'] = 24, ['Z'] = 25, ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29,
        ['e'] = 30, ['f'] = 31, ['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35,
        ['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39, ['o'] = 40, ['p'] = 41,
        ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45, ['u'] = 46, ['v'] = 47,
        ['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51, ['0'] = 52, ['1'] = 53,
        ['2'] = 54, ['3'] = 55, ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59,
        ['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63,
    };
    size_t n = strlen(s ? s : "");
    unsigned char *out = (unsigned char *)malloc(n / 4 * 3 + 4);
    if (!out) { *out_len = 0; return NULL; }
    size_t o = 0;
    unsigned int acc = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '=' || c == '\n' || c == '\r') continue;
        signed char v = tab[c];
        if (v == 0 && c != 'A') continue;
        acc = (acc << 6) | (unsigned int)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (unsigned char)((acc >> bits) & 0xFF);
        }
    }
    *out_len = o;
    return out;
}

/* ── UTF-8 decode with errors="replace" (CPython maximal-subpart rules) ────
 * Invalid lead -> one U+FFFD, advance 1. Valid lead + invalid continuation ->
 * one U+FFFD for the bytes consumed so far, restart AT the offending byte.
 * Truncated-at-end sequence -> one U+FFFD for the whole partial sequence. */
static char *blf_utf8_replace(const unsigned char *b, size_t n) {
    char *out = (char *)malloc(n * 3 + 1);
    if (!out) return NULL;
    size_t o = 0, i = 0;
    while (i < n) {
        unsigned char c = b[i];
        if (c < 0x80) { out[o++] = (char)c; i++; continue; }
        int need;
        unsigned char lo = 0x80, hi = 0xBF;
        unsigned int cp;
        if (c >= 0xC2 && c <= 0xDF) { need = 1; cp = c & 0x1Fu; }
        else if (c >= 0xE0 && c <= 0xEF) {
            need = 2; cp = c & 0x0Fu;
            if (c == 0xE0) lo = 0xA0;
            if (c == 0xED) hi = 0x9F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            need = 3; cp = c & 0x07u;
            if (c == 0xF0) lo = 0x90;
            if (c == 0xF4) hi = 0x8F;
        } else {
            out[o++] = (char)0xEF; out[o++] = (char)0xBF; out[o++] = (char)0xBD;
            i++;
            continue;
        }
        size_t j = i + 1;
        bool ok = true;
        for (int k = 0; k < need; k++) {
            if (j >= n) { ok = false; break; }
            unsigned char cc = b[j];
            unsigned char l = (k == 0) ? lo : 0x80;
            unsigned char h = (k == 0) ? hi : 0xBF;
            if (cc < l || cc > h) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3Fu);
            j++;
        }
        if (!ok) {
            out[o++] = (char)0xEF; out[o++] = (char)0xBF; out[o++] = (char)0xBD;
            i = (j >= n) ? n : j;
            continue;
        }
        if (cp < 0x800) {
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out[o++] = (char)(0xE0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else {
            out[o++] = (char)(0xF0 | (cp >> 18));
            out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        }
        i = j;
    }
    out[o] = '\0';
    return out;
}

/* ── SHA-256 (inline; _default_user_data_dir's digest, no libhash object) ── */

static uint32_t blf_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void blf_sha256_hex(const unsigned char *data, size_t len, char out[65]) {
    static const uint32_t K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };
    uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    size_t total = len + 1 + 8;
    size_t padded = ((total + 63) / 64) * 64;
    unsigned char *msg = (unsigned char *)calloc(padded, 1);
    if (!msg) { out[0] = '\0'; return; }
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bitlen = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        msg[padded - 1 - (size_t)i] = (unsigned char)(bitlen >> (8 * i));
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[off + (size_t)i * 4] << 24)
                 | ((uint32_t)msg[off + (size_t)i * 4 + 1] << 16)
                 | ((uint32_t)msg[off + (size_t)i * 4 + 2] << 8)
                 | (uint32_t)msg[off + (size_t)i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = blf_rotr(w[i - 15], 7) ^ blf_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = blf_rotr(w[i - 2], 17) ^ blf_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = blf_rotr(e, 6) ^ blf_rotr(e, 11) ^ blf_rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = blf_rotr(a, 2) ^ blf_rotr(a, 13) ^ blf_rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    free(msg);
    static const char hexd[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++) {
            unsigned char byte = (unsigned char)(h[i] >> (24 - 8 * j));
            out[i * 8 + j * 2] = hexd[byte >> 4];
            out[i * 8 + j * 2 + 1] = hexd[byte & 0x0F];
        }
    out[64] = '\0';
}

/* ── 1. running_in_docker ──────────────────────────────────────────────────
 *   if os.path.exists("/.dockerenv"): return True
 *   try: open("/proc/1/cgroup", "rt", encoding="utf-8"): return "docker" in read()
 *   except OSError: return False                                          */

/* PoP: blf_running_in_docker @ tools/browser_launcher.py:running_in_docker */
json_t *blf_running_in_docker(const json_t *in, const blf_io_t *io,
                              json_t *state, json_t **exc) {
    (void)in; (void)state;
    json_t *a = json_object();
    blf_arg_s(a, "path", "/.dockerenv");
    json_t *r = blf_call(io, "exists", a, exc);
    if (*exc) return NULL;
    bool dockerenv = blf_truth(r);
    json_free(r);
    if (dockerenv) return json_bool(true);

    a = json_object();
    blf_arg_s(a, "path", "/proc/1/cgroup");
    blf_arg_s(a, "mode", "rt");
    blf_arg_s(a, "encoding", "utf-8");
    r = blf_call(io, "open", a, exc);
    if (*exc) {
        if (blf_exc_is(*exc, "OSError")) {
            json_free(*exc); *exc = NULL;
            return json_bool(false);
        }
        return NULL;
    }
    const char *raw = blf_str(r);
    char *text = blf_universal_newlines(raw ? raw : "");
    json_free(r);
    bool found = text && strstr(text, "docker") != NULL;
    free(text);
    return json_bool(found);
}

/* ── 2. apparmor_restricts_unprivileged_userns ─────────────────────────────
 *   try: open(...apparmor_restrict_unprivileged_userns, encoding="utf-8"):
 *        return f.read().strip() == "1"
 *   except OSError: return False                                          */

/* PoP: blf_apparmor_restricts @ tools/browser_launcher.py:apparmor_restricts_unprivileged_userns */
json_t *blf_apparmor_restricts(const json_t *in, const blf_io_t *io,
                               json_t *state, json_t **exc) {
    (void)in; (void)state;
    json_t *a = json_object();
    blf_arg_s(a, "path", "/proc/sys/kernel/apparmor_restrict_unprivileged_userns");
    blf_arg_s(a, "encoding", "utf-8");
    json_t *r = blf_call(io, "open", a, exc);
    if (*exc) {
        if (blf_exc_is(*exc, "OSError")) {
            json_free(*exc); *exc = NULL;
            return json_bool(false);
        }
        return NULL;
    }
    const char *raw = blf_str(r);
    char *text = blf_universal_newlines(raw ? raw : "");
    json_free(r);
    char *stripped = blf_strip(text ? text : "");
    free(text);
    bool yes = stripped && strcmp(stripped, "1") == 0;
    free(stripped);
    return json_bool(yes);
}

/* ── 3. needs_chromium_sandbox_bypass ──────────────────────────────────────
 *   if hasattr(os, "geteuid") and os.geteuid() == 0: return True
 *   if running_in_docker(): return True
 *   return apparmor_restricts_unprivileged_userns()                       */

/* PoP: blf_needs_sandbox_bypass @ tools/browser_launcher.py:needs_chromium_sandbox_bypass */
json_t *blf_needs_sandbox_bypass(const json_t *in, const blf_io_t *io,
                                 json_t *state, json_t **exc) {
    (void)state;
    if (json_get_bool(in, "has_geteuid", true)) {
        json_t *r = blf_call(io, "geteuid", json_object(), exc);
        if (*exc) return NULL;
        double euid = blf_num(r, -1);
        json_free(r);
        if (euid == 0) return json_bool(true);
    }
    json_t *r = blf_call(io, "running_in_docker", json_object(), exc);
    if (*exc) return NULL;
    bool docker = blf_truth(r);
    json_free(r);
    if (docker) return json_bool(true);
    r = blf_call(io, "apparmor_restricts_unprivileged_userns", json_object(), exc);
    if (*exc) return NULL;
    bool apparmor = blf_truth(r);
    json_free(r);
    return json_bool(apparmor);
}

/* ── 4. _explicit_executable_configured ────────────────────────────────────
 *   return chromium_executable(allow_override=True) !=
 *          chromium_executable(allow_override=False)
 * Both calls always run (no short-circuit); the callable is the seam.       */

/* PoP: blf_explicit_executable_configured @ tools/browser_launcher.py:_explicit_executable_configured */
json_t *blf_explicit_executable_configured(const json_t *in, const blf_io_t *io,
                                           json_t *state, json_t **exc) {
    (void)in; (void)state;
    json_t *a = json_object();
    blf_arg_b(a, "allow_override", true);
    json_t *r1 = blf_call(io, "chromium_executable", a, exc);
    if (*exc) return NULL;
    a = json_object();
    blf_arg_b(a, "allow_override", false);
    json_t *r2 = blf_call(io, "chromium_executable", a, exc);
    if (*exc) { json_free(r1); return NULL; }
    bool neq = !blf_json_eq(r1, r2);
    json_free(r1); json_free(r2);
    return json_bool(neq);
}

/* ── 5. _resolve_executable ────────────────────────────────────────────────
 *   exe = chromium_executable()
 *   if not exe: return None
 *   if os.path.isfile(exe): return exe
 *   found = locate_command(exe).command
 *   return found[0] if found else None                                    */

/* PoP: blf_resolve_executable @ tools/browser_launcher.py:_resolve_executable */
json_t *blf_resolve_executable(const json_t *in, const blf_io_t *io,
                               json_t *state, json_t **exc) {
    (void)in; (void)state;
    json_t *exe = blf_call(io, "chromium_executable", json_object(), exc);
    if (*exc) return NULL;
    if (!blf_truth(exe)) { json_free(exe); return json_null(); }
    const char *exe_s = blf_str(exe);
    if (!exe_s) { json_free(exe); return json_null(); }  /* non-str out of scope */

    json_t *a = json_object();
    blf_arg_s(a, "path", exe_s);
    json_t *f = blf_call(io, "isfile", a, exc);
    if (*exc) { json_free(exe); return NULL; }
    bool isfile = blf_truth(f);
    json_free(f);
    if (isfile) return exe;  /* ownership transfers */

    a = json_object();
    blf_arg_s(a, "name", exe_s);
    json_free(exe);
    json_t *loc = blf_call(io, "locate_command", a, exc);
    if (*exc) return NULL;
    const json_t *cmd = blf_is_obj(loc) ? json_obj_get(loc, "command") : NULL;
    json_t *out = json_null();
    if (blf_truth(cmd) && blf_is_arr(cmd) && cmd->c.count > 0) {
        json_free(out);
        out = json_copy(cmd->c.items[0]);
    }
    json_free(loc);
    return out;
}

/* ── 6. chromium_ready ─────────────────────────────────────────────────────
 *   if _resolve_executable(): return True, ""
 *   if _maybe_autoinstall_chromium() and _resolve_executable(): return True, ""
 *   return False, _chromium_missing_hint()                                */

/* PoP: blf_chromium_ready @ tools/browser_launcher.py:chromium_ready */
json_t *blf_chromium_ready(const json_t *in, const blf_io_t *io,
                           json_t *state, json_t **exc) {
    (void)in; (void)state;
    json_t *r = blf_call(io, "_resolve_executable", json_object(), exc);
    if (*exc) return NULL;
    bool ok = blf_truth(r);
    json_free(r);
    if (ok) {
        json_t *out = json_array();
        json_append(out, json_bool(true));
        json_append(out, json_string(""));
        return out;
    }
    r = blf_call(io, "_maybe_autoinstall_chromium", json_object(), exc);
    if (*exc) return NULL;
    bool installed = blf_truth(r);
    json_free(r);
    if (installed) {
        r = blf_call(io, "_resolve_executable", json_object(), exc);
        if (*exc) return NULL;
        ok = blf_truth(r);
        json_free(r);
        if (ok) {
            json_t *out = json_array();
            json_append(out, json_bool(true));
            json_append(out, json_string(""));
            return out;
        }
    }
    json_t *hint = blf_call(io, "_chromium_missing_hint", json_object(), exc);
    if (*exc) return NULL;
    json_t *out = json_array();
    json_append(out, json_bool(false));
    json_append(out, hint ? hint : json_null());
    return out;
}

/* ── 7. _launch_timeout ────────────────────────────────────────────────────
 *   if timeout is not None: return float(timeout)
 *   return float(max(command_timeout(), _MIN_FIRST_LAUNCH_TIMEOUT_S))       */

/* PoP: blf_launch_timeout @ tools/browser_launcher.py:_launch_timeout */
json_t *blf_launch_timeout(const json_t *in, const blf_io_t *io,
                           json_t *state, json_t **exc) {
    (void)state;
    const json_t *t = json_obj_get(in, "timeout");
    if (!blf_is_null(t)) return json_number(blf_num(t, 0));
    json_t *r = blf_call(io, "command_timeout", json_object(), exc);
    if (*exc) return NULL;
    double ct = blf_num(r, 0);
    json_free(r);
    return json_number(ct > BLF_MIN_FIRST_LAUNCH_TIMEOUT_S
                       ? ct : BLF_MIN_FIRST_LAUNCH_TIMEOUT_S);
}

/* ── 8. _resolve_headed ────────────────────────────────────────────────────
 *   if headed is None: headed = is_headed_mode()
 *   if not headed: return False
 *   return _has_display(env) or not sys.platform.startswith("linux")        */

/* PoP: blf_resolve_headed @ tools/browser_launcher.py:_resolve_headed */
json_t *blf_resolve_headed(const json_t *in, const blf_io_t *io,
                           json_t *state, json_t **exc) {
    (void)state;
    const json_t *h = json_obj_get(in, "headed");
    bool headed;
    if (blf_is_null(h)) {
        json_t *r = blf_call(io, "is_headed_mode", json_object(), exc);
        if (*exc) return NULL;
        headed = blf_truth(r);
        json_free(r);
    } else {
        headed = blf_truth(h);
    }
    if (!headed) return json_bool(false);

    const json_t *env = json_obj_get(in, "env");
    json_t *a = json_object();
    json_set(a, "env", (env && env->type != JSON_NULL) ? json_copy(env) : json_object());
    json_t *r = blf_call(io, "_has_display", a, exc);
    if (*exc) return NULL;
    bool display = blf_truth(r);
    json_free(r);
    if (display) return json_bool(true);
    const char *platform = json_get_str(in, "platform", "linux");
    return json_bool(strncmp(platform, "linux", 5) != 0);
}

/* ── 9. _spawn_kwargs ──────────────────────────────────────────────────────
 *   if os.name == "nt":
 *       return {"creationflags": windows_hide_flags() |
 *               getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)}
 *   return {"start_new_session": True}                                    */

/* PoP: blf_spawn_kwargs @ tools/browser_launcher.py:_spawn_kwargs */
json_t *blf_spawn_kwargs(const json_t *in, const blf_io_t *io,
                         json_t *state, json_t **exc) {
    (void)state;
    const char *os_name = json_get_str(in, "os_name", "posix");
    json_t *out = json_object();
    if (strcmp(os_name, "nt") == 0) {
        json_t *r = blf_call(io, "windows_hide_flags", json_object(), exc);
        if (*exc) { json_free(out); return NULL; }
        long long hide = (long long)blf_num(r, 0);
        json_free(r);
        const json_t *grp = json_obj_get(in, "create_new_process_group");
        long long group = (grp && grp->type == JSON_NUMBER) ? (long long)grp->num_val : 0;
        json_set(out, "creationflags", json_number((double)(hide | group)));
    } else {
        json_set(out, "start_new_session", json_bool(true));
    }
    return out;
}/* ── 10. _read_devtools_file ───────────────────────────────────────────────
 *   try: open(join(udd, "DevToolsActivePort"), encoding="utf-8-sig"):
 *        port, path = fh.readline().strip(), fh.readline().strip()
 *   except OSError: return None
 *   if not port.isdigit() or not path.startswith("/devtools/browser/"):
 *       return None
 *   return int(port), path
 * The open seam returns the file TEXT; BOM strip (utf-8-sig), universal
 * newlines and readline semantics are modeled here.                       */

/* PoP: blf_read_devtools_file @ tools/browser_launcher.py:_read_devtools_file */
json_t *blf_read_devtools_file(const json_t *in, const blf_io_t *io,
                               json_t *state, json_t **exc) {
    (void)state;
    const char *udd = json_get_str(in, "user_data_dir", "");
    char *path = blf_posix_join(udd, BLF_DEVTOOLS_FILE);
    json_t *a = json_object();
    blf_arg_s(a, "path", path ? path : "");
    blf_arg_s(a, "encoding", "utf-8-sig");
    free(path);
    json_t *r = blf_call(io, "open", a, exc);
    if (*exc) {
        if (blf_exc_is(*exc, "OSError")) {
            json_free(*exc); *exc = NULL;
            return json_null();
        }
        return NULL;
    }
    const char *raw = blf_str(r);
    if (!raw) raw = "";
    /* utf-8-sig: strip a leading BOM before any read. */
    if ((unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB
        && (unsigned char)raw[2] == 0xBF) raw += 3;
    char *text = blf_universal_newlines(raw);
    json_free(r);
    size_t pos = 0;
    char *l1 = blf_readline(text ? text : "", &pos);
    char *l2 = blf_readline(text ? text : "", &pos);
    free(text);
    char *port_s = blf_strip(l1 ? l1 : "");
    char *path_s = blf_strip(l2 ? l2 : "");
    free(l1); free(l2);
    json_t *out = json_null();
    if (blf_isdigit(port_s) && path_s
        && strncmp(path_s, "/devtools/browser/", 18) == 0) {
        json_free(out);
        out = json_array();
        json_append(out, json_number((double)strtoll(port_s, NULL, 10)));
        json_append(out, json_string(path_s));
    }
    free(port_s); free(path_s);
    return out;
}

/* ── 11. _log_tail ─────────────────────────────────────────────────────────
 *   try: raw = Path(udd, "hermes-chromium.log").read_bytes()[-1500:]
 *   except OSError: return ""
 *   return raw.decode("utf-8", errors="replace").strip()
 * read_bytes seam returns {"b64": ...}; the slice/decode/strip are real.    */

/* PoP: blf_log_tail @ tools/browser_launcher.py:_log_tail */
json_t *blf_log_tail(const json_t *in, const blf_io_t *io,
                     json_t *state, json_t **exc) {
    (void)state;
    const char *udd = json_get_str(in, "user_data_dir", "");
    char *path = blf_posix_join(udd, BLF_LAUNCH_LOG);
    json_t *a = json_object();
    blf_arg_s(a, "path", path ? path : "");
    free(path);
    json_t *r = blf_call(io, "read_bytes", a, exc);
    if (*exc) {
        if (blf_exc_is(*exc, "OSError")) {
            json_free(*exc); *exc = NULL;
            return json_string("");
        }
        return NULL;
    }
    const char *b64 = blf_is_obj(r) ? json_get_str(r, "b64", "") : "";
    size_t blen = b64 ? strlen(b64) : 0;
    size_t data_chars = 0;
    int b64_bad = 0;
    for (size_t i = 0; b64 && i < blen && !b64_bad; i++) {
        unsigned char c = (unsigned char)b64[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=' ||
                 c == ' ' || c == '\t' || c == '\n' || c == '\r';
        if (!ok) b64_bad = 1;
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') data_chars++;
    }
    if (!b64_bad && data_chars % 4 != 0) b64_bad = 1;
    if (b64_bad) {
        json_free(r);
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("Error"));
        json_set(e, "msg", json_string("Incorrect padding"));
        *exc = e;
        return NULL;
    }
    size_t n = 0;
    unsigned char *bytes = blf_b64_decode(b64 ? b64 : "", &n);
    json_free(r);
    size_t take = n > BLF_STDERR_TAIL_CHARS ? BLF_STDERR_TAIL_CHARS : n;
    char *decoded = blf_utf8_replace(bytes ? bytes + (n - take) : NULL, take);
    free(bytes);
    char *stripped = blf_strip(decoded ? decoded : "");
    free(decoded);
    json_t *out = json_string(stripped ? stripped : "");
    free(stripped);
    return out;
}

/* ── 12. _wait_for_devtools ────────────────────────────────────────────────
 *   while True:
 *       found = _read_devtools_file(udd)
 *       if found: return found, None
 *       rc = proc.poll()
 *       if rc is not None:
 *           tail = _log_tail(udd)
 *           hint = " Chromium's sandbox..." if "sandbox" in tail.lower() else ""
 *           return None, f"Chromium exited with code {rc} during startup.{hint}"
 *                        + (f"\n{tail}" if tail else "")
 *       if time.monotonic() >= deadline:
 *           return None, "Chromium did not expose a DevTools endpoint in time."
 *       time.sleep(0.1)                                                   */

/* PoP: blf_wait_for_devtools @ tools/browser_launcher.py:_wait_for_devtools */
json_t *blf_wait_for_devtools(const json_t *in, const blf_io_t *io,
                              json_t *state, json_t **exc) {
    (void)state;
    const char *udd = json_get_str(in, "user_data_dir", "");
    double deadline = json_get_num(in, "deadline", 0);
    for (;;) {
        json_t *a = json_object();
        blf_arg_s(a, "user_data_dir", udd);
        json_t *found = blf_call(io, "_read_devtools_file", a, exc);
        if (*exc) return NULL;
        if (blf_truth(found)) {
            json_t *out = json_array();
            json_append(out, found);
            json_append(out, json_null());
            return out;
        }
        json_free(found);

        json_t *rc = blf_call(io, "proc.poll", json_object(), exc);
        if (*exc) return NULL;
        if (!blf_is_null(rc)) {
            a = json_object();
            blf_arg_s(a, "user_data_dir", udd);
            json_t *tail = blf_call(io, "_log_tail", a, exc);
            if (*exc) { json_free(rc); return NULL; }
            const char *tail_s = blf_str(tail);
            if (!tail_s) tail_s = "";
            char *lower = blf_lower_dup(tail_s);
            const char *hint = (lower && strstr(lower, "sandbox"))
                ? " Chromium's sandbox could not start; add --no-sandbox to browser.chromium_args."
                : "";
            free(lower);
            char rcbuf[64];
            blf_num_str(rcbuf, sizeof(rcbuf), blf_num(rc, 0));
            size_t cap = strlen(rcbuf) + strlen(hint) + strlen(tail_s) + 64;
            char *msg = (char *)malloc(cap);
            if (msg)
                snprintf(msg, cap, "Chromium exited with code %s during startup.%s%s%s",
                         rcbuf, hint, tail_s[0] ? "\n" : "", tail_s);
            json_free(rc); json_free(tail);
            json_t *out = json_array();
            json_append(out, json_null());
            json_append(out, json_string(msg ? msg : ""));
            free(msg);
            return out;
        }
        json_free(rc);

        json_t *m = blf_call(io, "monotonic", json_object(), exc);
        if (*exc) return NULL;
        double now = blf_num(m, 0);
        json_free(m);
        if (now >= deadline) {
            json_t *out = json_array();
            json_append(out, json_null());
            json_append(out, json_string("Chromium did not expose a DevTools endpoint in time."));
            return out;
        }
        a = json_object();
        blf_arg_n(a, "seconds", BLF_POLL_INTERVAL_S);
        json_t *s = blf_call(io, "sleep", a, exc);
        json_free(s);
        if (*exc) return NULL;
    }
}

/* ── 13. _singleton_owner ──────────────────────────────────────────────────
 *   try: target = os.readlink(join(udd, "SingletonLock"))
 *   except OSError: return None
 *   host, _, pid_text = target.rpartition("-")
 *   return (host, int(pid_text)) if pid_text.isdigit() else None          */

/* PoP: blf_singleton_owner @ tools/browser_launcher.py:_singleton_owner */
json_t *blf_singleton_owner(const json_t *in, const blf_io_t *io,
                            json_t *state, json_t **exc) {
    (void)state;
    const char *udd = json_get_str(in, "user_data_dir", "");
    char *path = blf_posix_join(udd, "SingletonLock");
    json_t *a = json_object();
    blf_arg_s(a, "path", path ? path : "");
    free(path);
    json_t *r = blf_call(io, "readlink", a, exc);
    if (*exc) {
        if (blf_exc_is(*exc, "OSError")) {
            json_free(*exc); *exc = NULL;
            return json_null();
        }
        return NULL;
    }
    const char *target = blf_str(r);
    if (!target) target = "";
    const char *dash = strrchr(target, '-');
    char *host;
    const char *pid_text;
    if (dash) {
        size_t hn = (size_t)(dash - target);
        host = (char *)malloc(hn + 1);
        if (host) { memcpy(host, target, hn); host[hn] = '\0'; }
        pid_text = dash + 1;
    } else {
        host = strdup("");
        pid_text = target;
    }
    json_t *out = json_null();
    if (blf_isdigit(pid_text)) {
        json_free(out);
        out = json_array();
        json_append(out, json_string(host ? host : ""));
        json_append(out, json_number((double)strtoll(pid_text, NULL, 10)));
    }
    free(host);
    json_free(r);
    return out;
}

/* ── 14. _singleton_pid ────────────────────────────────────────────────────
 *   owner = _singleton_owner(udd)
 *   if owner is None:
 *       return _pid_naming_dir(udd) if os.name == "nt" else None
 *   return owner[1]                                                       */

/* PoP: blf_singleton_pid @ tools/browser_launcher.py:_singleton_pid */
json_t *blf_singleton_pid(const json_t *in, const blf_io_t *io,
                          json_t *state, json_t **exc) {
    (void)state;
    const char *udd = json_get_str(in, "user_data_dir", "");
    json_t *a = json_object();
    blf_arg_s(a, "user_data_dir", udd);
    json_t *owner = blf_call(io, "_singleton_owner", a, exc);
    if (*exc) return NULL;
    if (blf_is_null(owner)) {
        json_free(owner);
        const char *os_name = json_get_str(in, "os_name", "posix");
        if (strcmp(os_name, "nt") != 0) return json_null();
        a = json_object();
        blf_arg_s(a, "user_data_dir", udd);
        json_t *r = blf_call(io, "_pid_naming_dir", a, exc);
        if (*exc) return NULL;
        return r ? r : json_null();
    }
    json_t *out = json_null();
    if (blf_is_arr(owner) && owner->c.count >= 2) {
        json_free(out);
        out = json_copy(owner->c.items[1]);
    }
    json_free(owner);
    return out;
}

/* ── 15. running_instance ──────────────────────────────────────────────────
 *   pid = _singleton_pid(udd)
 *   found = _read_devtools_file(udd)
 *   if pid is None or found is None or not _pid_alive(pid): return None
 *   port, path = found
 *   return (port, path, pid) if _devtools_endpoint_live(port, path) else None
 * Both probes run before the or-chain; _pid_alive only when both present.  */

/* PoP: blf_running_instance @ tools/browser_launcher.py:running_instance */
json_t *blf_running_instance(const json_t *in, const blf_io_t *io,
                             json_t *state, json_t **exc) {
    (void)state;
    const char *udd = json_get_str(in, "user_data_dir", "");
    json_t *a = json_object();
    blf_arg_s(a, "user_data_dir", udd);
    json_t *pid = blf_call(io, "_singleton_pid", a, exc);
    if (*exc) return NULL;
    a = json_object();
    blf_arg_s(a, "user_data_dir", udd);
    json_t *found = blf_call(io, "_read_devtools_file", a, exc);
    if (*exc) { json_free(pid); return NULL; }
    if (blf_is_null(pid) || blf_is_null(found)) {
        json_free(pid); json_free(found);
        return json_null();
    }
    a = json_object();
    blf_arg_j(a, "pid", pid);
    json_t *alive = blf_call(io, "_pid_alive", a, exc);
    if (*exc) { json_free(pid); json_free(found); return NULL; }
    bool is_alive = blf_truth(alive);
    json_free(alive);
    if (!is_alive) {
        json_free(pid); json_free(found);
        return json_null();
    }
    a = json_object();
    if (blf_is_arr(found) && found->c.count >= 2) {
        blf_arg_j(a, "port", found->c.items[0]);
        blf_arg_j(a, "path", found->c.items[1]);
    }
    json_t *live = blf_call(io, "_devtools_endpoint_live", a, exc);
    if (*exc) { json_free(pid); json_free(found); return NULL; }
    bool endpoint = blf_truth(live);
    json_free(live);
    json_t *out = json_null();
    if (endpoint && blf_is_arr(found) && found->c.count >= 2) {
        json_free(out);
        out = json_array();
        json_append(out, json_copy(found->c.items[0]));
        json_append(out, json_copy(found->c.items[1]));
        json_append(out, json_copy(pid));
    }
    json_free(pid); json_free(found);
    return out;
}

/* ── 16. _default_user_data_dir ────────────────────────────────────────────
 *   digest = sha256(f"{home_key}\0{key}".encode()).hexdigest()[:16]
 *   pid = os.getpid(); started = _start_time(pid)
 *   owner = f"{pid}-{started}" if started is not None else str(pid)
 *   return str(_ephemeral_root(home_key) / f"{digest}-{owner}{ns_tag()}")
 * _ephemeral_root = Path(home)/"cache"/"browser"/"chromium" (posix join;
 * Path("") == Path(".") drops the prefix). SHA-256 is the inline core.     */

/* PoP: blf_default_user_data_dir @ tools/browser_launcher.py:_default_user_data_dir */
json_t *blf_default_user_data_dir(const json_t *in, const blf_io_t *io,
                                  json_t *state, json_t **exc) {
    (void)state;
    const char *key = json_get_str(in, "key", "");
    const char *home_key = json_get_str(in, "home_key", "");
    size_t lh = strlen(home_key), lk = strlen(key);
    unsigned char *buf = (unsigned char *)malloc(lh + 1 + lk);
    if (!buf) return json_null();
    memcpy(buf, home_key, lh);
    buf[lh] = 0;
    memcpy(buf + lh + 1, key, lk);
    char hex[65];
    blf_sha256_hex(buf, lh + 1 + lk, hex);
    free(buf);
    char digest[17];
    memcpy(digest, hex, 16);
    digest[16] = '\0';

    json_t *pid = blf_call(io, "getpid", json_object(), exc);
    if (*exc) return NULL;
    char pidbuf[64];
    blf_num_str(pidbuf, sizeof(pidbuf), blf_num(pid, 0));
    json_t *a = json_object();
    blf_arg_j(a, "pid", pid);
    json_t *started = blf_call(io, "_start_time", a, exc);
    json_free(pid);
    if (*exc) return NULL;
    char owner[160];
    if (!blf_is_null(started)) {
        char stbuf[64];
        blf_num_str(stbuf, sizeof(stbuf), blf_num(started, 0));
        snprintf(owner, sizeof(owner), "%s-%s", pidbuf, stbuf);
    } else {
        snprintf(owner, sizeof(owner), "%s", pidbuf);
    }
    json_free(started);

    json_t *tag = blf_call(io, "ns_tag", json_object(), exc);
    if (*exc) return NULL;
    const char *tag_s = blf_str(tag);
    if (!tag_s) tag_s = "None";  /* f-string of a scripted None */

    size_t tl = strlen(tag_s);
    char *name = malloc(strlen(digest) + strlen(owner) + tl + 2);
    if (!name) { json_free(tag); return json_null(); }
    char *np = name;
    np += sprintf(np, "%s-%s", digest, owner);
    for (size_t i = 0; i < tl; i++) *np++ = tag_s[i] == '\\' ? '/' : tag_s[i];
    *np = '\0';
    json_free(tag);

    /* _ephemeral_root(home_key) / name */
    char *root;
    if (!home_key[0] || strcmp(home_key, ".") == 0) {
        root = strdup("cache/browser/chromium");
    } else {
        char *t1 = blf_posix_join(home_key, "cache");
        char *t2 = blf_posix_join(t1 ? t1 : "", "browser");
        root = blf_posix_join(t2 ? t2 : "", "chromium");
        free(t1); free(t2);
    }
    char *full = blf_posix_join(root ? root : "", name);
    free(name);
    free(root);
    json_t *out = json_string(full ? full : "");
    free(full);
    return out;
}

/* ── 17. _prepare_dir ──────────────────────────────────────────────────────
 *   if ephemeral:
 *       Path(udd).parent.mkdir(mode=0o700, parents=True, exist_ok=True)
 *       shutil.rmtree(udd, ignore_errors=True)
 *   os.makedirs(udd, mode=0o700, exist_ok=True)
 *   try: os.unlink(join(udd, "DevToolsActivePort"))
 *   except OSError: pass
 * mkdir/makedirs raises propagate; rmtree's ignore_errors swallows ANY
 * scripted raise (after recording); only unlink's OSError is caught.       */

/* PoP: blf_prepare_dir @ tools/browser_launcher.py:_prepare_dir */
json_t *blf_prepare_dir(const json_t *in, const blf_io_t *io,
                        json_t *state, json_t **exc) {
    (void)state;
    const char *udd = json_get_str(in, "user_data_dir", "");
    bool ephemeral = json_get_bool(in, "ephemeral", true);
    if (ephemeral) {
        char *parent = blf_posix_parent(udd);
        json_t *a = json_object();
        blf_arg_s(a, "path", parent ? parent : ".");
        blf_arg_n(a, "mode", 448);  /* 0o700 */
        blf_arg_b(a, "parents", true);
        blf_arg_b(a, "exist_ok", true);
        free(parent);
        json_t *r = blf_call(io, "mkdir", a, exc);
        json_free(r);
        if (*exc) return NULL;

        a = json_object();
        blf_arg_s(a, "path", udd);
        blf_arg_b(a, "ignore_errors", true);
        r = blf_call(io, "rmtree", a, exc);
        json_free(r);
        if (*exc) { json_free(*exc); *exc = NULL; }  /* ignore_errors=True */
    }
    json_t *a = json_object();
    blf_arg_s(a, "path", udd);
    blf_arg_n(a, "mode", 448);
    blf_arg_b(a, "exist_ok", true);
    json_t *r = blf_call(io, "makedirs", a, exc);
    json_free(r);
    if (*exc) return NULL;

    char *path = blf_posix_join(udd, BLF_DEVTOOLS_FILE);
    a = json_object();
    blf_arg_s(a, "path", path ? path : "");
    free(path);
    r = blf_call(io, "unlink", a, exc);
    json_free(r);
    if (*exc) {
        if (blf_exc_is(*exc, "OSError")) {
            json_free(*exc); *exc = NULL;
        } else {
            return NULL;
        }
    }
    return json_null();
}

/* ── 18. launch_chromium ───────────────────────────────────────────────────
 *   import tools.process_registry            (no observable event)
 *   with _key_lock(key):
 *       existing = get_browser(key)
 *       if existing is not None: return existing, None
 *       home_key = hermes_home_key()
 *       with _lock: _served_homes.add(home_key)
 *       packaged = executable is None
 *       if packaged:
 *           ok, hint = chromium_ready()
 *           if not ok: return None, hint
 *           executable = _resolve_executable()
 *       udd = user_data_dir or _default_user_data_dir(key, home_key)
 *       launch = dict(packaged=..., headed=..., extra_args=..., env=...,
 *                     timeout=..., sandbox_bypass=...)
 *       if ephemeral:
 *           return _launch_owned(key, executable, udd, ephemeral=True,
 *                                home_key=home_key, **launch)
 *       return _start_shared(key, executable, udd, home_key, prepare, **launch)
 * The `or` on user_data_dir is TRUTHINESS ("" takes the default); the
 * existing-browser test is IDENTITY (an empty dict is still a browser).
 * None-valued launch kwargs are dropped from the recorded args.           */

static void blf_add_launch_kwargs(json_t *a, const json_t *in, bool packaged) {
    blf_arg_b(a, "packaged", packaged);
    blf_arg_j(a, "headed", json_obj_get(in, "headed"));
    const json_t *xa = json_obj_get(in, "extra_args");
    if (!xa) json_set(a, "extra_args", json_array());
    else blf_arg_j(a, "extra_args", xa);
    blf_arg_j(a, "env", json_obj_get(in, "env"));
    blf_arg_j(a, "timeout", json_obj_get(in, "timeout"));
    blf_arg_b(a, "sandbox_bypass", json_get_bool(in, "sandbox_bypass", true));
}

static void blf_state_add_home(json_t *state, const char *home) {
    if (!state || !home) return;
    json_t *homes = json_obj_get(state, "served_homes");
    if (!homes || homes->type != JSON_ARRAY) return;
    for (size_t i = 0; i < homes->c.count; i++) {
        const json_t *h = homes->c.items[i];
        if (h && h->type == JSON_STRING && strcmp(h->str_val, home) == 0) return;
    }
    json_append(homes, json_string(home));
}

/* PoP: blf_launch_chromium @ tools/browser_launcher.py:launch_chromium */
json_t *blf_launch_chromium(const json_t *in, const blf_io_t *io,
                            json_t *state, json_t **exc) {
    const char *key = json_get_str(in, "key", "");
    /* with _key_lock(key): — one recorded seam call, lock value ignored. */
    json_t *a = json_object();
    blf_arg_s(a, "key", key);
    json_t *lock = blf_call(io, "_key_lock", a, exc);
    json_free(lock);
    if (*exc) return NULL;

    a = json_object();
    blf_arg_s(a, "key", key);
    json_t *existing = blf_call(io, "get_browser", a, exc);
    if (*exc) return NULL;
    if (!blf_is_null(existing)) {
        json_t *out = json_array();
        json_append(out, existing);
        json_append(out, json_null());
        return out;
    }
    json_free(existing);

    json_t *hk = blf_call(io, "hermes_home_key", json_object(), exc);
    if (*exc) return NULL;
    const char *home_key = blf_str(hk);
    if (!home_key) home_key = "";
    blf_state_add_home(state, home_key);
    char *home_dup = strdup(home_key);
    json_free(hk);

    const json_t *exec_v = json_obj_get(in, "executable");
    bool packaged = blf_is_null(exec_v);
    json_t *exec_owned = NULL;   /* resolved exe when packaged */
    const char *executable = packaged ? NULL : blf_str(exec_v);
    if (packaged) {
        json_t *ready = blf_call(io, "chromium_ready", json_object(), exc);
        if (*exc) { free(home_dup); return NULL; }
        bool ok = false;
        const json_t *hint = NULL;
        if (blf_is_arr(ready) && ready->c.count >= 2) {
            ok = blf_truth(ready->c.items[0]);
            hint = ready->c.items[1];
        }
        if (!ok) {
            json_t *out = json_array();
            json_append(out, json_null());
            json_append(out, hint ? json_copy(hint) : json_null());
            json_free(ready);
            free(home_dup);
            return out;
        }
        json_free(ready);
        exec_owned = blf_call(io, "_resolve_executable", json_object(), exc);
        if (*exc) { free(home_dup); return NULL; }
        executable = blf_str(exec_owned);
    }

    /* udd = user_data_dir or _default_user_data_dir(key, home_key) — the
     * or-chain yields the default seam's result AS IS (None stays None and
     * is dropped from the recorded launch args). */
    const json_t *udd_v = json_obj_get(in, "user_data_dir");
    char *udd = NULL;
    if (blf_truth(udd_v)) {
        udd = strdup(blf_str(udd_v) ? blf_str(udd_v) : "");
    } else {
        a = json_object();
        blf_arg_s(a, "key", key);
        blf_arg_s(a, "home_key", home_dup ? home_dup : "");
        json_t *d = blf_call(io, "_default_user_data_dir", a, exc);
        if (*exc) { json_free(exec_owned); free(home_dup); return NULL; }
        const char *ds = blf_str(d);
        if (ds) udd = strdup(ds);
        json_free(d);
    }

    bool ephemeral = json_get_bool(in, "ephemeral", true);
    json_t *r;
    if (ephemeral) {
        a = json_object();
        blf_arg_s(a, "key", key);
        blf_arg_s(a, "executable", executable);
        blf_arg_s(a, "user_data_dir", udd);
        blf_arg_b(a, "ephemeral", true);
        blf_arg_s(a, "home_key", home_dup ? home_dup : "");
        blf_add_launch_kwargs(a, in, packaged);
        r = blf_call(io, "_launch_owned", a, exc);
    } else {
        a = json_object();
        blf_arg_s(a, "key", key);
        blf_arg_s(a, "executable", executable);
        blf_arg_s(a, "user_data_dir", udd);
        blf_arg_s(a, "home_key", home_dup ? home_dup : "");
        blf_add_launch_kwargs(a, in, packaged);
        const json_t *prep = json_obj_get(in, "prepare");
        if (!blf_is_null(prep)) blf_arg_s(a, "prepare", "<callable>");
        r = blf_call(io, "_start_shared", a, exc);
    }
    free(udd);
    free(home_dup);
    json_free(exec_owned);
    if (*exc) return NULL;
    return r ? r : json_null();
}
