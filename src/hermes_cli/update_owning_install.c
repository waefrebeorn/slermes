/*
 * update_owning_install.c — C home for upstream Hermes hermes_cli/update_owning_install.py.
 * STATUS: PORTED (gate w25_updateowni_0f1bd1) — differential oracle green.
 * PoP: w25_updateowni_0f1bd1_install_key @ pm/environments.py:install_key
 * PoP: w25_updateowni_0f1bd1_owning_root @ hermes_cli/update_owning_install.py:owning_install_root
 * PoP: w25_updateowni_0f1bd1_pm_owner @ hermes_cli/update_owning_install.py:_pm_environment_owner
 * PoP: w25_updateowni_0f1bd1_retarget @ hermes_cli/update_owning_install.py:retarget_to_owning_install
 * PoP: w25_updateowni_0f1bd1_constants @ hermes_cli/update_owning_install.py:<module constants>
 *
 * ``hermes update`` must be sent back to the install whose interpreter it runs
 * on: a redirected install never moves and the app keeps relaunching its stale
 * build.  Resolution, the filesystem reads (``hermes_cli/main.py``, the PM
 * record), ``PYTHONPATH`` and the child process are seams; ported here is the
 * decision, the install key that proves a PM generation's owner, and the exact
 * retarget text/argv.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "noc_w25_updateowni_0f1bd1.h"

/* === SHA-256 (FIPS 180-4), for pm.environments.install_key === */
typedef struct {
    uint32_t h[8];
    uint64_t total;
    unsigned char buf[64];
    size_t n;
} ui_sha;

static const uint32_t UI_K[64] = {
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

#define UI_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void ui_sha_init(ui_sha *c) {
    static const uint32_t init[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    memcpy(c->h, init, sizeof(init));
    c->total = 0;
    c->n = 0;
}

static void ui_sha_block(ui_sha *c, const unsigned char *p) {
    uint32_t w[64], a, b, cc, d, e, f, g, h, s0, s1, ch, maj, t1, t2;
    size_t i;

    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        s0 = UI_ROTR(w[i - 15], 7) ^ UI_ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        s1 = UI_ROTR(w[i - 2], 17) ^ UI_ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3];
    e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (i = 0; i < 64; i++) {
        s1 = UI_ROTR(e, 6) ^ UI_ROTR(e, 11) ^ UI_ROTR(e, 25);
        ch = (e & f) ^ ((~e) & g);
        t1 = h + s1 + ch + UI_K[i] + w[i];
        s0 = UI_ROTR(a, 2) ^ UI_ROTR(a, 13) ^ UI_ROTR(a, 22);
        maj = (a & b) ^ (a & cc) ^ (b & cc);
        t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void ui_sha_update(ui_sha *c, const unsigned char *p, size_t len) {
    c->total += (uint64_t)len;
    while (len > 0) {
        size_t take = 64 - c->n;
        if (take > len) take = len;
        memcpy(c->buf + c->n, p, take);
        c->n += take;
        p += take;
        len -= take;
        if (c->n == 64) { ui_sha_block(c, c->buf); c->n = 0; }
    }
}

static void ui_sha_final(ui_sha *c, unsigned char out[32]) {
    uint64_t bits = c->total * 8u;
    unsigned char pad = 0x80;
    unsigned char lenbuf[8];
    size_t i;

    ui_sha_update(c, &pad, 1);
    while (c->n != 56) {
        unsigned char zero = 0;
        ui_sha_update(c, &zero, 1);
    }
    for (i = 0; i < 8; i++) lenbuf[i] = (unsigned char)(bits >> (56 - 8 * i));
    ui_sha_update(c, lenbuf, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)c->h[i];
    }
}

/* sha256(utf8(path)).hexdigest()[:16] into a caller buffer. */
static void ui_sha_hex16(const char *path, char out[W25_UPDATEOWNI_0F1BD1_KEY_CHARS + 1]) {
    static const char digits[] = "0123456789abcdef";
    ui_sha ctx;
    unsigned char digest[32];
    size_t i;

    ui_sha_init(&ctx);
    ui_sha_update(&ctx, (const unsigned char *)(path ? path : ""), path ? strlen(path) : 0);
    ui_sha_final(&ctx, digest);
    for (i = 0; i < W25_UPDATEOWNI_0F1BD1_KEY_CHARS; i++)
        out[i] = digits[(digest[i / 2] >> (i % 2 ? 0 : 4)) & 0x0f];
    out[W25_UPDATEOWNI_0F1BD1_KEY_CHARS] = '\0';
}

json_t *w25_updateowni_0f1bd1_install_key(const char *path) {
    char hex[W25_UPDATEOWNI_0F1BD1_KEY_CHARS + 1];
    ui_sha_hex16(path, hex);
    return json_string(hex);
}

/* === Path helpers (the paths arrive already resolved: resolution is the seam) === */

static int ui_is_sep(char c) { return c == '/' || c == '\\'; }

/* Path(p).name: the last component. */
static const char *ui_basename(const char *path, char *buf, size_t cap) {
    const char *p = path ? path : "";
    size_t len = strlen(p), end = len, start;

    while (end > 1 && ui_is_sep(p[end - 1])) end--;
    start = end;
    while (start > 0 && !ui_is_sep(p[start - 1])) start--;
    if (end - start + 1 > cap) { if (cap) buf[0] = '\0'; return buf; }
    memcpy(buf, p + start, end - start);
    buf[end - start] = '\0';
    return buf;
}

/* Path(p).parent as text ("" when there is none). */
static const char *ui_dirname(const char *path, char *buf, size_t cap) {
    const char *p = path ? path : "";
    size_t len = strlen(p), end = len, start;

    while (end > 1 && ui_is_sep(p[end - 1])) end--;
    start = end;
    while (start > 0 && !ui_is_sep(p[start - 1])) start--;
    if (start == 0) { if (cap) buf[0] = '\0'; return buf; }
    end = start;
    while (end > 1 && ui_is_sep(p[end - 1])) end--;
    if (end + 1 > cap) { if (cap) buf[0] = '\0'; return buf; }
    memcpy(buf, p, end);
    buf[end] = '\0';
    return buf;
}

/* The separator pathlib would print for this path. */
static char ui_sep(const char *path) {
    size_t i, n = path ? strlen(path) : 0;
    if (n >= 2 && path[1] == ':' && ((path[0] >= 'A' && path[0] <= 'Z') ||
                                     (path[0] >= 'a' && path[0] <= 'z'))) return '\\';
    for (i = 0; i < n; i++) if (path[i] == '\\') return '\\';
    return '/';
}

static char *ui_join(const char *base, const char *name) {
    char sep = ui_sep(base);
    size_t bl = strlen(base), nl = strlen(name);
    char *out = (char *)malloc(bl + nl + 2);
    if (!out) return NULL;
    memcpy(out, base, bl);
    out[bl] = sep;
    memcpy(out + bl + 1, name, nl);
    out[bl + 1 + nl] = '\0';
    return out;
}

static int ui_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* text read with utf-8-sig then .strip() */
static char *ui_strip(const char *s) {
    static const char bom[] = "\xEF\xBB\xBF";
    size_t start = 0, end = strlen(s);
    char *out;

    if (strncmp(s, bom, 3) == 0) start = 3;
    while (start < end && ui_is_ws(s[start])) start++;
    while (end > start && ui_is_ws(s[end - 1])) end--;
    out = (char *)malloc(end - start + 1);
    if (!out) return NULL;
    memcpy(out, s + start, end - start);
    out[end - start] = '\0';
    return out;
}

static const char *ui_str(const json_t *v) {
    return v && v->type == JSON_STRING && v->str_val ? v->str_val : NULL;
}

static int ui_bool(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return 0;
}

json_t *w25_updateowni_0f1bd1_pm_owner(const json_t *diag) {
    const char *venv = ui_str(json_obj_get(diag, "venv"));
    const char *record = ui_str(json_obj_get(diag, "record"));
    char one[4096], two[4096], state[4096], name[256], key[W25_UPDATEOWNI_0F1BD1_KEY_CHARS + 1];
    char state_name[256];
    char *owner;
    json_t *out;

    if (!venv || !record) return json_null(); /* missing/unreadable .project-root */
    ui_dirname(venv, one, sizeof(one)); /* .../environments/<gen> */
    ui_dirname(one, two, sizeof(two));  /* .../environments */
    if (strcmp(ui_basename(two, name, sizeof(name)), "environments") != 0) return json_null();
    ui_dirname(two, state, sizeof(state)); /* <installs>/<key> */
    ui_basename(state, state_name, sizeof(state_name));

    owner = ui_strip(record);
    if (!owner || !owner[0]) { free(owner); return json_null(); }
    ui_sha_hex16(owner, key);
    if (strcmp(key, state_name) != 0 || !ui_bool(json_obj_get(diag, "main_py"))) {
        free(owner);
        return json_null();
    }
    out = json_string(owner);
    free(owner);
    return out;
}

json_t *w25_updateowni_0f1bd1_owning_root(const json_t *diag) {
    const char *prefix = ui_str(json_obj_get(diag, "prefix"));
    const char *base = ui_str(json_obj_get(diag, "base_prefix"));
    const char *root = ui_str(json_obj_get(diag, "project_root"));
    const char *pm = ui_str(json_obj_get(diag, "pm_owner"));
    const json_t *pythonpath = json_obj_get(diag, "pythonpath");
    char name[256], parent[4096];
    const char *owner;
    size_t i, n;

    if (!prefix || !base || !root) return json_null();
    if (strcmp(prefix, base) == 0) return json_null(); /* not a venv interpreter */
    if (strcmp(ui_basename(prefix, name, sizeof(name)), "venv") != 0 &&
        strcmp(name, ".venv") != 0) return json_null();
    if (ui_bool(json_obj_get(diag, "main_py"))) {
        owner = ui_dirname(prefix, parent, sizeof(parent));
    } else {
        owner = pm;
    }
    if (!owner || !owner[0]) return json_null();
    if (strcmp(owner, root) == 0) return json_null(); /* already its own checkout */
    if (pythonpath && pythonpath->type == JSON_ARRAY) {
        n = json_len(pythonpath);
        for (i = 0; i < n; i++) {
            const char *entry = ui_str(json_get(pythonpath, i));
            if (entry && entry[0] && strcmp(entry, root) == 0)
                return json_null(); /* the dev tree was put on PYTHONPATH on purpose */
        }
    }
    return json_string(owner);
}

json_t *w25_updateowni_0f1bd1_retarget(const json_t *req) {
    const char *owner = ui_str(json_obj_get(req, "owner"));
    const char *prefix = ui_str(json_obj_get(req, "prefix"));
    const char *root = ui_str(json_obj_get(req, "project_root"));
    const char *exe = ui_str(json_obj_get(req, "executable"));
    const json_t *args = json_obj_get(req, "args");
    char name[256];
    char *venv_path, *line1, *line2;
    size_t need1, need2, i, n;
    json_t *out, *lines, *argv;

    if (!owner) return json_null();
    if (!exe) exe = "";
    ui_basename(prefix ? prefix : "", name, sizeof(name));
    venv_path = ui_join(owner, name);
    if (!venv_path) return json_null();

    need1 = strlen(venv_path) + strlen(root ? root : "") + 96;
    need2 = strlen(owner) + 96;
    line1 = (char *)malloc(need1);
    line2 = (char *)malloc(need2);
    if (!line1 || !line2) { free(line1); free(line2); free(venv_path); return json_null(); }
    /* U+26A0 WARNING SIGN ... U+2192 RIGHTWARDS ARROW */
    snprintf(line1, need1, "\xE2\x9A\xA0 %s is running the checkout at %s, not its own code.",
             venv_path, root ? root : "");
    snprintf(line2, need2,
             "\xE2\x86\x92 Updating %s with its own updater; this also points its venv back at it.",
             owner);

    out = json_object();
    lines = json_array();
    json_append(lines, json_string(line1));
    json_append(lines, json_string(line2));
    json_set(out, "lines", lines);
    json_set(out, "pythonpath", json_string(owner));
    json_set(out, "cwd", json_string(owner));
    argv = json_array();
    json_append(argv, json_string(exe));
    json_append(argv, json_string("-m"));
    json_append(argv, json_string(W25_UPDATEOWNI_0F1BD1_MODULE));
    if (args && args->type == JSON_ARRAY) {
        n = json_len(args);
        for (i = 0; i < n; i++) json_append(argv, json_copy(json_get(args, i)));
    }
    json_set(out, "argv", argv);
    free(line1);
    free(line2);
    free(venv_path);
    return out;
}

json_t *w25_updateowni_0f1bd1_constants(void) {
    json_t *o = json_object();
    json_t *names = json_array();
    json_append(names, json_string("venv"));
    json_append(names, json_string(".venv"));
    json_set(o, "venv_names", names);
    json_set(o, "module", json_string(W25_UPDATEOWNI_0F1BD1_MODULE));
    json_set(o, "key_chars", json_number((double)W25_UPDATEOWNI_0F1BD1_KEY_CHARS));
    json_set(o, "record_path", json_string("inputs/.project-root"));
    return o;
}
