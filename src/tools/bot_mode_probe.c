#define _POSIX_C_SOURCE 200809L
/* bot_mode_probe.c - Port of Python tools/bot_mode_probe.py
 *
 * Bot Mode roster probe - canonical Bot Chat system prompt section.
 * Pure filesystem + YAML + SHA-256 logic (no network). Faithful to the
 * Python source: same silent-failure contract, same cached-probe entry
 * point, same capability-epoch fingerprint.
 *
 * PoP (Python -> C):
 *   _hermes_root                       -> bmp_hermes_root
 *   _profile_name                      -> bmp_profile_name
 *   _is_bot_managed                    -> bmp_is_bot_managed
 *   _roster                            -> bmp_roster / bmp_roster_free
 *   _soul_has_protocol                 -> bmp_soul_has_protocol
 *   _handle                            -> bmp_handle
 *   _peers                             -> bmp_peers / bmp_peers_free
 *   _peer_paragraph                    -> bmp_peer_paragraph
 *   _build_section                     -> bmp_build_section
 *   get_bot_mode_protocol_section      -> get_bot_mode_protocol_section
 *   capability_fingerprint             -> capability_fingerprint
 *   epoch_line                         -> epoch_line
 *   stored_prompt_capability_stale     -> stored_prompt_capability_stale
 *   stored_bot_chat_prompt_needs_upgrade -> stored_bot_chat_prompt_needs_upgrade
 *   _reset_cache_for_tests             -> bot_mode_probe_reset_cache_for_tests
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "libyaml/yaml.h"
#include "libcrypto/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

/* --- protocol constants ------------------------------------------- */
static const char *BMP_PROTOCOL_HEADING = "## Messaging other agents";
static const char *BMP_EPOCH_PREFIX = "Capability epoch: ";

/* --- string builder (append + real newline via char 10) ----------- */
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} bmp_sb;

static void sb_init(bmp_sb *sb) {
    sb->cap = 256;
    sb->len = 0;
    sb->buf = (char *)malloc(sb->cap);
    if (sb->buf) sb->buf[0] = '\0';
}
static void sb_reserve(bmp_sb *sb, size_t extra) {
    if (!sb->buf) sb_init(sb);
    if (!sb->buf) return;
    if (sb->len + extra + 1 > sb->cap) {
        while (sb->len + extra + 1 > sb->cap) sb->cap *= 2;
        char *nb = (char *)realloc(sb->buf, sb->cap);
        if (nb) sb->buf = nb;
    }
}
static void sb_append(bmp_sb *sb, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    sb_reserve(sb, n);
    if (!sb->buf) return;
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}
static void sb_append_c(bmp_sb *sb, char c) {
    sb_reserve(sb, 1);
    if (!sb->buf) return;
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
}
static void sb_append_nl(bmp_sb *sb, const char *s) {
    sb_append(sb, s);
    sb_append_c(sb, 10); /* newline */
}
static void sb_free(bmp_sb *sb) {
    if (sb->buf) free(sb->buf);
    sb->buf = NULL;
    sb->len = sb->cap = 0;
}

/* --- path helpers -------------------------------------------------- */
static void path_join(char *out, size_t n, const char *a, const char *b) {
    if (!a || !b) { if (out) out[0] = '\0'; return; }
    size_t la = strlen(a);
    int have_slash = (la > 0 && a[la - 1] == '/');
    if (have_slash)
        snprintf(out, n, "%s%s", a, b);
    else
        snprintf(out, n, "%s/%s", a, b);
    out[n - 1] = '\0';
}

/* dirname: parent path of p (without trailing slash). */
static void dir_name(const char *p, char *out, size_t n) {
    size_t L = strlen(p);
    while (L > 0 && p[L - 1] == '/') L--;
    long i = (long)L - 1;
    while (i >= 0 && p[i] != '/') i--;
    if (i < 0) { snprintf(out, n, "."); out[n - 1] = '\0'; return; }
    if ((size_t)i >= n) i = (long)n - 1;
    memcpy(out, p, (size_t)i);
    out[i] = '\0';
}

/* basename: final path component. */
static void base_name(const char *p, char *out, size_t n) {
    size_t L = strlen(p);
    while (L > 0 && p[L - 1] == '/') L--;
    long i = (long)L - 1;
    while (i >= 0 && p[i] != '/') i--;
    size_t start = (size_t)(i + 1);
    size_t blen = L - start;
    if (blen >= n) blen = n - 1;
    memcpy(out, p + start, blen);
    out[blen] = '\0';
}

/* --- file read ---------------------------------------------------- */
static char *read_text(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static int path_is_dir(const char *p) {
    struct stat st;
    if (stat(p, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}
static int path_is_file(const char *p) {
    struct stat st;
    if (stat(p, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

/* --- home resolution ---------------------------------------------- */
const char *bot_mode_probe_resolve_home(const char *home, char *out, size_t n) {
    if (home && *home) {
        snprintf(out, n, "%s", home);
        out[n - 1] = '\0';
        return out;
    }
    const char *e = getenv("HERMES_HOME");
    if (e && *e) {
        snprintf(out, n, "%s", e);
        out[n - 1] = '\0';
        return out;
    }
    const char *h = getenv("HOME");
    if (h && *h) {
        snprintf(out, n, "%s/.hermes", h);
        out[n - 1] = '\0';
        return out;
    }
    snprintf(out, n, "~/.hermes");
    out[n - 1] = '\0';
    return out;
}

/* --- Python _hermes_root / _profile_name -------------------------- */
static void bmp_hermes_root(const char *home, char *out, size_t n) {
    char parent[4096];
    dir_name(home, parent, sizeof(parent));
    char pb[1024];
    base_name(parent, pb, sizeof(pb));
    if (strcmp(pb, "profiles") == 0) {
        dir_name(parent, out, n);
    } else {
        snprintf(out, n, "%s", home);
    }
    out[n - 1] = '\0';
}
static void bmp_profile_name(const char *home, char *out, size_t n) {
    char parent[4096];
    dir_name(home, parent, sizeof(parent));
    char pb[1024];
    base_name(parent, pb, sizeof(pb));
    if (strcmp(pb, "profiles") == 0) {
        base_name(home, out, n);
    } else {
        snprintf(out, n, "default");
    }
    out[n - 1] = '\0';
}

/* --- _is_bot_managed ---------------------------------------------- */
static int bmp_is_bot_managed(const char *profile_dir) {
    char meta[4608];
    path_join(meta, sizeof(meta), profile_dir, "profile.yaml");
    char *raw = read_text(meta);
    if (!raw) return 0;
    int found = 0;
    if (strstr(raw, "hermes-bots") != NULL) {
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(raw, &err);
        if (err) free(err);
        if (doc) {
            char *ui_meta = yaml_to_json_string(doc, "ui_meta");
            if (ui_meta && ui_meta[0] == '{') {
                char *hb = yaml_to_json_string(doc, "ui_meta.hermes-bots");
                if (hb && hb[0] == '{') found = 1;
                if (hb) free(hb);
            }
            if (ui_meta) free(ui_meta);
            yaml_free(doc);
        }
    }
    free(raw);
    return found;
}

/* --- roster ------------------------------------------------------- */
typedef struct { char *name; char *dir; } bmp_roster_entry;

static int bmp_roster_cmp(const void *a, const void *b) {
    const bmp_roster_entry *ea = (const bmp_roster_entry *)a;
    const bmp_roster_entry *eb = (const bmp_roster_entry *)b;
    return strcmp(ea->name, eb->name);
}
static bmp_roster_entry *bmp_roster(const char *root, size_t *count) {
    bmp_roster_entry *arr = (bmp_roster_entry *)malloc(sizeof(bmp_roster_entry) * 8);
    size_t cap = 8, n = 0;
    if (!arr) { *count = 0; return NULL; }
    arr[n].name = strdup("default");
    arr[n].dir = strdup(root);
    n++;
    char profiles[4608];
    path_join(profiles, sizeof(profiles), root, "profiles");
    if (path_is_dir(profiles)) {
        DIR *d = opendir(profiles);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                char child[4608];
                path_join(child, sizeof(child), profiles, e->d_name);
                if (!path_is_dir(child)) continue;
                if (n == cap) {
                    cap *= 2;
                    bmp_roster_entry *na = (bmp_roster_entry *)realloc(arr, sizeof(bmp_roster_entry) * cap);
                    if (!na) continue;
                    arr = na;
                }
                arr[n].name = strdup(e->d_name);
                arr[n].dir = strdup(child);
                n++;
            }
            closedir(d);
        }
    }
    qsort(arr, n, sizeof(bmp_roster_entry), bmp_roster_cmp);
    *count = n;
    return arr;
}
static void bmp_roster_free(bmp_roster_entry *arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) {
        if (arr[i].name) free(arr[i].name);
        if (arr[i].dir) free(arr[i].dir);
    }
    free(arr);
}

/* --- _soul_has_protocol ------------------------------------------- */
static int bmp_soul_has_protocol(const char *profile_dir) {
    char soul[4608];
    path_join(soul, sizeof(soul), profile_dir, "SOUL.md");
    char *raw = read_text(soul);
    if (!raw) return 0;
    int found = (strstr(raw, BMP_PROTOCOL_HEADING) != NULL);
    free(raw);
    return found;
}

/* --- _handle ------------------------------------------------------ */
static void bmp_handle(const char *name, char *out, size_t n) {
    if (strcmp(name, "default") == 0) snprintf(out, n, "hermes");
    else snprintf(out, n, "%s", name);
    out[n - 1] = '\0';
}

/* --- _peers (bot_peers keys, non-empty, sorted) ------------------- */
static char **bmp_peers(const char *root, size_t *count) {
    *count = 0;
    char cfg[4608];
    path_join(cfg, sizeof(cfg), root, "config.yaml");
    char *raw = read_text(cfg);
    if (!raw) return NULL;
    char **names = NULL;
    if (strstr(raw, "bot_peers") != NULL) {
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(raw, &err);
        if (err) free(err);
        if (doc) {
            size_t kc = 0;
            char **keys = yaml_map_keys(doc, "bot_peers", &kc);
            if (keys) {
                size_t nc = 0;
                char **tmp = (char **)malloc(sizeof(char *) * (kc + 1));
                for (size_t i = 0; i < kc; i++) {
                    if (keys[i] && keys[i][0] != '\0') {
                        char *s = strdup(keys[i]);
                        size_t L = strlen(s);
                        while (L > 0 && isspace((unsigned char)s[L - 1])) s[--L] = '\0';
                        if (s[0] != '\0') tmp[nc++] = s;
                        else free(s);
                    }
                }
                qsort(tmp, nc, sizeof(char *), bmp_roster_cmp);
                names = tmp;
                *count = nc;
                for (size_t i = 0; i < kc; i++) free(keys[i]);
                free(keys);
            }
            yaml_free(doc);
        }
    }
    free(raw);
    return names;
}
static void bmp_peers_free(char **names, size_t count) {
    if (!names) return;
    for (size_t i = 0; i < count; i++) free(names[i]);
    free(names);
}

/* --- _peer_paragraph ---------------------------------------------- */
static void bmp_peer_paragraph(const char *root, bmp_sb *sb) {
    size_t pc = 0;
    char **peers = bmp_peers(root, &pc);
    if (!peers || pc == 0) {
        if (peers) bmp_peers_free(peers, pc);
        return;
    }
    bmp_sb listed;
    sb_init(&listed);
    for (size_t i = 0; i < pc; i++) {
        if (i) sb_append(&listed, ", ");
        sb_append_c(&listed, '`');
        sb_append(&listed, peers[i]);
        sb_append_c(&listed, '`');
    }
    sb_append(sb, "\n\n");
    sb_append(sb, "Teammates on OTHER machines: this install also has peer gateways ");
    sb_append(sb, "registered (");
    sb_append(sb, listed.buf ? listed.buf : "");
    sb_append(sb, "). Message an agent on a peer the same way, via ");
    sb_append(sb, "`hermes peer dm` (same terminal-tool pattern: background=true, ");
    sb_append(sb, "notify_on_complete=true; the reply prints on stdout when it completes):");
    sb_append_nl(sb, "```");
    sb_append_nl(sb, "hermes peer dm <peer>/<agent-name> \"Message from \xf0\x9f\xa4\x96 <you> (@<you>): your message\"");
    sb_append_nl(sb, "```");
    sb_append(sb, "Use `<peer>` alone for the peer's main agent. Run `hermes peer list` ");
    sb_append(sb, "for the live peer list.");
    sb_free(&listed);
    bmp_peers_free(peers, pc);
}

/* --- _build_section ----------------------------------------------- */
static char *bmp_build_section(const char *home) {
    char root[4608];
    bmp_hermes_root(home, root, sizeof(root));
    char me[1024];
    bmp_profile_name(home, me, sizeof(me));

    size_t rc = 0;
    bmp_roster_entry *roster = bmp_roster(root, &rc);
    int any_managed = 0;
    for (size_t i = 0; i < rc; i++) {
        if (bmp_is_bot_managed(roster[i].dir)) { any_managed = 1; break; }
    }
    if (!any_managed) {
        bmp_roster_free(roster, rc);
        return strdup("");
    }

    char my_dir[4608];
    if (strcmp(me, "default") == 0) snprintf(my_dir, sizeof(my_dir), "%s", root);
    else {
        char profs[4608];
        path_join(profs, sizeof(profs), root, "profiles");
        path_join(my_dir, sizeof(my_dir), profs, me);
    }
    if (bmp_soul_has_protocol(my_dir)) {
        bmp_roster_free(roster, rc);
        return strdup("");
    }

    char handle[1024];
    bmp_handle(me, handle, sizeof(handle));

    bmp_sb teammates;
    sb_init(&teammates);
    int first = 1;
    for (size_t i = 0; i < rc; i++) {
        if (strcmp(roster[i].name, me) == 0) continue;
        if (!first) sb_append(&teammates, ", ");
        first = 0;
        sb_append_c(&teammates, '`');
        sb_append(&teammates, roster[i].name);
        sb_append_c(&teammates, '`');
    }
    if (first) sb_append(&teammates, "(none yet)");

    bmp_sb bs;
    sb_init(&bs);
    sb_append_nl(&bs, BMP_PROTOCOL_HEADING);
    sb_append(&bs, "This install runs Bot Mode: each Hermes profile is an agent teammate with ");
    sb_append(&bs, "one canonical \"Bot Chat\" conversation. To message a teammate, run on the ");
    sb_append(&bs, "terminal tool (background=true, notify_on_complete=true), then finish your ");
    sb_append(&bs, "turn \xe2\x80\x94 the reply arrives later as a new message:");
    sb_append_nl(&bs, "```");
    {
        bmp_sb cmd;
        sb_init(&cmd);
        sb_append(&cmd, "hermes -p <agent-name> chat --in ~ -c \"Bot Chat\" --create-if-missing -Q -q \"Message from \xf0\x9f\xa4\x96 ");
        sb_append(&cmd, handle);
        sb_append(&cmd, " (@@");
        sb_append(&cmd, handle);
        sb_append(&cmd, "): your message\"");
        sb_append_nl(&bs, cmd.buf ? cmd.buf : "");
        sb_free(&cmd);
    }
    sb_append_nl(&bs, "```");
    sb_append(&bs, "Always open with the \"Message from \xf0\x9f\xa4\x96 ");
    sb_append(&bs, handle);
    sb_append(&bs, " (@@");
    sb_append(&bs, handle);
    sb_append(&bs, "):\" prefix so they ");
    sb_append(&bs, "know who is talking. When YOU receive a message with that prefix, you are ");
    sb_append(&bs, "being messaged by a teammate agent \xe2\x80\x94 address them (not the user) and reply ");
    sb_append(&bs, "concisely. When the user says \"ask <name>\" or \"tell <name> ...\", that is a ");
    sb_append(&bs, "handoff: message that agent, wait for the reply, and report back, saying ");
    sb_append(&bs, "which agent it came from. Run `hermes profile list` for the LIVE teammate ");
    sb_append(&bs, "list before a handoff. Teammates at session start: ");
    sb_append(&bs, teammates.buf ? teammates.buf : "");
    sb_append(&bs, ".");
    bmp_peer_paragraph(root, &bs);

    char *result = strdup(bs.buf ? bs.buf : "");
    sb_free(&teammates);
    sb_free(&bs);
    bmp_roster_free(roster, rc);
    return result;
}

/* --- cached probe entry point ------------------------------------- */
#define BMP_CACHE_MAX 64
typedef struct { char *key; char *val; } bmp_cache_entry;
static bmp_cache_entry g_cache[BMP_CACHE_MAX];
static size_t g_cache_n = 0;

static const char *bmp_cache_get(const char *key) {
    for (size_t i = 0; i < g_cache_n; i++)
        if (strcmp(g_cache[i].key, key) == 0) return g_cache[i].val;
    return NULL;
}
static void bmp_cache_put(const char *key, char *val) {
    for (size_t i = 0; i < g_cache_n; i++) {
        if (strcmp(g_cache[i].key, key) == 0) {
            free(g_cache[i].val);
            g_cache[i].val = val;
            return;
        }
    }
    if (g_cache_n < BMP_CACHE_MAX) {
        g_cache[g_cache_n].key = strdup(key);
        g_cache[g_cache_n].val = val;
        g_cache_n++;
    } else {
        free(g_cache[0].key);
        free(g_cache[0].val);
        memmove(&g_cache[0], &g_cache[1], sizeof(bmp_cache_entry) * (BMP_CACHE_MAX - 1));
        g_cache[BMP_CACHE_MAX - 1].key = strdup(key);
        g_cache[BMP_CACHE_MAX - 1].val = val;
    }
}

/* PoP: get_bot_mode_protocol_section @ tools/bot_mode_probe.py:get_bot_mode_protocol_section */
const char *get_bot_mode_protocol_section(const char *home, int force_refresh) {
    char resolved[4608];
    bot_mode_probe_resolve_home(home, resolved, sizeof(resolved));
    const char *cached = bmp_cache_get(resolved);
    if (!force_refresh && cached) return cached;
    char *section = bmp_build_section(resolved);
    if (!section) section = strdup("");
    bmp_cache_put(resolved, section);
    return section;
}

void bot_mode_probe_reset_cache_for_tests(void) {
    for (size_t i = 0; i < g_cache_n; i++) {
        free(g_cache[i].key);
        free(g_cache[i].val);
    }
    g_cache_n = 0;
}

/* --- capability fingerprint --------------------------------------- */
static void json_escape_str(const char *s, bmp_sb *sb) {
    sb_append_c(sb, '"');
    if (s) {
        for (size_t i = 0; s[i]; i++) {
            unsigned char c = (unsigned char)s[i];
            switch (c) {
                case '"':  sb_append(sb, "\\\""); break;
                case '\\': sb_append(sb, "\\\\"); break;
                case '\n': sb_append(sb, "\\n"); break;
                case '\r': sb_append(sb, "\\r"); break;
                case '\t': sb_append(sb, "\\t"); break;
                case '\b': sb_append(sb, "\\b"); break;
                case '\f': sb_append(sb, "\\f"); break;
                default:
                    if (c < 0x20) {
                        char hex[8];
                        snprintf(hex, sizeof(hex), "\\u%04x", c);
                        sb_append(sb, hex);
                    } else {
                        sb_append_c(sb, (char)c);
                    }
            }
        }
    }
    sb_append_c(sb, '"');
}
static void json_escape_strlist(char **list, size_t n, bmp_sb *sb) {
    sb_append_c(sb, '[');
    for (size_t i = 0; i < n; i++) {
        if (i) sb_append(sb, ", ");
        json_escape_str(list[i], sb);
    }
    sb_append_c(sb, ']');
}

static char *bmp_sha256_hex(const unsigned char *data, size_t len) {
    unsigned char h[CRYPTO_SHA256_LEN];
    crypto_sha256(data, len, h);
    char *out = (char *)malloc(CRYPTO_SHA256_LEN * 2 + 1);
    static const char *hexd = "0123456789abcdef";
    for (int i = 0; i < CRYPTO_SHA256_LEN; i++) {
        out[i * 2] = hexd[h[i] >> 4];
        out[i * 2 + 1] = hexd[h[i] & 0xf];
    }
    out[CRYPTO_SHA256_LEN * 2] = '\0';
    return out;
}

/* PoP: capability_fingerprint @ tools/bot_mode_probe.py:capability_fingerprint */
char *capability_fingerprint(const char *home) {
    char resolved[4608];
    bot_mode_probe_resolve_home(home, resolved, sizeof(resolved));

    char **disabled = NULL; size_t disabled_n = 0;
    char **toolsets = NULL; size_t toolsets_n = 0;
    char *mcp_json = strdup("");

    char cfg[4608];
    path_join(cfg, sizeof(cfg), resolved, "config.yaml");
    char *raw = read_text(cfg);
    if (raw) {
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(raw, &err);
        if (err) free(err);
        if (doc) {
            size_t lc = 0;
            lc = yaml_list_count(doc, "skills.disabled");
            if (lc) {
                disabled = (char **)malloc(sizeof(char *) * (lc + 1));
                for (size_t i = 0; i < lc; i++) {
                    const char *v = yaml_list_get(doc, "skills.disabled", i);
                    char *s = strdup(v ? v : "");
                    for (size_t j = 0; s[j]; j++) s[j] = (char)tolower((unsigned char)s[j]);
                    disabled[disabled_n++] = s;
                }
                qsort(disabled, disabled_n, sizeof(char *), bmp_roster_cmp);
            }
            lc = yaml_list_count(doc, "tools.enabled_toolsets");
            if (lc) {
                toolsets = (char **)malloc(sizeof(char *) * (lc + 1));
                for (size_t i = 0; i < lc; i++) {
                    const char *v = yaml_list_get(doc, "tools.enabled_toolsets", i);
                    toolsets[toolsets_n++] = strdup(v ? v : "");
                }
                qsort(toolsets, toolsets_n, sizeof(char *), bmp_roster_cmp);
            }
            char *mj = yaml_to_json_string(doc, "mcp_servers");
            if (mj) {
                char *esc = (char *)malloc(strlen(mj) * 2 + 4);
                size_t o = 0;
                esc[o++] = '"';
                for (size_t i = 0; mj[i]; i++) {
                    if (mj[i] == '"' || mj[i] == '\\') esc[o++] = '\\';
                    esc[o++] = mj[i];
                }
                esc[o++] = '"';
                esc[o] = '\0';
                free(mcp_json);
                mcp_json = esc;
                free(mj);
            }
            yaml_free(doc);
        }
        free(raw);
    }

    char soul_hex[CRYPTO_SHA256_LEN * 2 + 1];
    soul_hex[0] = '\0';
    char soul[4608];
    path_join(soul, sizeof(soul), resolved, "SOUL.md");
    {
        FILE *f = fopen(soul, "rb");
        if (f) {
            if (fseek(f, 0, SEEK_END) == 0) {
                long sz = ftell(f);
                if (sz >= 0) {
                    rewind(f);
                    unsigned char *data = (unsigned char *)malloc((size_t)sz + 1);
                    if (data) {
                        size_t got = fread(data, 1, (size_t)sz, f);
                        unsigned char h[CRYPTO_SHA256_LEN];
                        crypto_sha256(data, got, h);
                        static const char *hexd = "0123456789abcdef";
                        for (int i = 0; i < CRYPTO_SHA256_LEN; i++) {
                            soul_hex[i * 2] = hexd[h[i] >> 4];
                            soul_hex[i * 2 + 1] = hexd[h[i] & 0xf];
                        }
                        soul_hex[CRYPTO_SHA256_LEN * 2] = '\0';
                        free(data);
                    }
                }
            }
            fclose(f);
        }
    }

    char **skills = NULL; size_t skills_n = 0;
    {
        char sroot[4608];
        path_join(sroot, sizeof(sroot), resolved, "skills");
        if (path_is_dir(sroot)) {
            DIR *d = opendir(sroot);
            if (d) {
                struct dirent *e;
                while ((e = readdir(d)) != NULL) {
                    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                    char sub[4608];
                    path_join(sub, sizeof(sub), sroot, e->d_name);
                    if (!path_is_dir(sub)) continue;
                    char sk[4608];
                    path_join(sk, sizeof(sk), sub, "SKILL.md");
                    if (path_is_file(sk)) {
                        char *nm = strdup(e->d_name);
                        skills = (char **)realloc(skills, sizeof(char *) * (skills_n + 1));
                        skills[skills_n++] = nm;
                    }
                }
                closedir(d);
            }
            qsort(skills, skills_n, sizeof(char *), bmp_roster_cmp);
        }
    }

    char root[4608];
    bmp_hermes_root(resolved, root, sizeof(root));
    char **roster_names = NULL; size_t roster_n = 0;
    {
        size_t rc = 0;
        bmp_roster_entry *roster = bmp_roster(root, &rc);
        for (size_t i = 0; i < rc; i++) {
            if (bmp_is_bot_managed(roster[i].dir)) {
                roster_names = (char **)realloc(roster_names, sizeof(char *) * (roster_n + 1));
                roster_names[roster_n++] = strdup(roster[i].name);
            }
        }
        bmp_roster_free(roster, rc);
        qsort(roster_names, roster_n, sizeof(char *), bmp_roster_cmp);
    }

    char **peers = NULL; size_t peers_n = 0;
    peers = bmp_peers(root, &peers_n);

    bmp_sb blob;
    sb_init(&blob);
    sb_append_c(&blob, '{');
    sb_append(&blob, "\"disabled_skills\": ");
    json_escape_strlist(disabled, disabled_n, &blob);
    sb_append(&blob, ", \"enabled_toolsets\": ");
    json_escape_strlist(toolsets, toolsets_n, &blob);
    sb_append(&blob, ", \"mcp\": ");
    sb_append(&blob, mcp_json);
    sb_append(&blob, ", \"peers\": ");
    json_escape_strlist(peers, peers_n, &blob);
    sb_append(&blob, ", \"roster\": ");
    json_escape_strlist(roster_names, roster_n, &blob);
    sb_append(&blob, ", \"skills\": ");
    json_escape_strlist(skills, skills_n, &blob);
    sb_append(&blob, ", \"soul\": ");
    json_escape_str(soul_hex, &blob);
    sb_append_c(&blob, '}');

    char *fp = NULL;
    if (blob.buf) {
        char *full = bmp_sha256_hex((const unsigned char *)blob.buf, strlen(blob.buf));
        size_t L = strlen(full);
        if (L > 12) { full[12] = '\0'; }
        fp = strdup(full);
        free(full);
    } else {
        fp = strdup("unavailable");
    }

    for (size_t i = 0; i < disabled_n; i++) free(disabled[i]);
    free(disabled);
    for (size_t i = 0; i < toolsets_n; i++) free(toolsets[i]);
    free(toolsets);
    free(mcp_json);
    for (size_t i = 0; i < skills_n; i++) free(skills[i]);
    free(skills);
    for (size_t i = 0; i < roster_n; i++) free(roster_names[i]);
    free(roster_names);
    bmp_peers_free(peers, peers_n);
    sb_free(&blob);
    return fp;
}

/* PoP: epoch_line @ tools/bot_mode_probe.py:epoch_line */
char *epoch_line(const char *home) {
    char *fp = capability_fingerprint(home);
    size_t L = strlen(BMP_EPOCH_PREFIX) + strlen(fp) + 1;
    char *line = (char *)malloc(L + 1);
    snprintf(line, L + 1, "%s%s", BMP_EPOCH_PREFIX, fp);
    free(fp);
    return line;
}

/* PoP: stored_prompt_capability_stale @ tools/bot_mode_probe.py:stored_prompt_capability_stale */
int stored_prompt_capability_stale(const char *stored_prompt, const char *home) {
    if (!stored_prompt) return 0;
    const char *p = strstr(stored_prompt, BMP_EPOCH_PREFIX);
    if (!p) return 0;
    const char *digits = p + strlen(BMP_EPOCH_PREFIX);
    char cur[13];
    size_t i = 0;
    while (i < 12 && digits[i] && ((digits[i] >= '0' && digits[i] <= '9') ||
                                   (digits[i] >= 'a' && digits[i] <= 'f'))) {
        cur[i] = digits[i];
        i++;
    }
    if (i < 12) return 0;
    cur[12] = '\0';
    char *current = capability_fingerprint(home);
    if (strcmp(current, "unavailable") == 0) { free(current); return 0; }
    int stale = (strcmp(cur, current) != 0);
    free(current);
    return stale;
}

/* PoP: stored_bot_chat_prompt_needs_upgrade @ tools/bot_mode_probe.py:stored_bot_chat_prompt_needs_upgrade */
int stored_bot_chat_prompt_needs_upgrade(const char *stored_prompt, const char *home) {
    if (!stored_prompt) return 0;
    if (strstr(stored_prompt, BMP_EPOCH_PREFIX) != NULL) return 0;
    if (strstr(stored_prompt, BMP_PROTOCOL_HEADING) != NULL) return 0;
    const char *section = get_bot_mode_protocol_section(home, 0);
    return (section && section[0] != '\0') ? 1 : 0;
}
