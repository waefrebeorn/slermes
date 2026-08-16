/*
 * port_secret_sources_helpers.c — C port of agent/secret_sources/
 * bitwarden.py + onepassword.py path/cache/fingerprint helpers.
 * Self-contained; PoP-annotated per function.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <openssl/sha.h>
#include "hermes_json.h"

#define BWS_VERSION "2.0.0"

static void ssh_get_hermes_home(char *out, size_t outsz) {
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) snprintf(out, outsz, "%s", hh);
    else snprintf(out, outsz, "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
}

/* resolve_cache_home: home_path -> HERMES_HOME -> ~/.hermes */
static char *ssh_resolve_cache_home(const char *home_path) {
    char home[1024];
    if (home_path && *home_path) snprintf(home, sizeof(home), "%s", home_path);
    else ssh_get_hermes_home(home, sizeof(home));
    return strdup(home);
}

/* PoP: bw_cache_key_str @ agent/secret_sources/bitwarden.py:_cache_key_str */
char *bw_cache_key_str(const char *token_fp, const char *project_id, const char *server_url) {
    /* "fp|project|url" — stable key for JSON storage. */
    char *out = malloc(strlen(token_fp ? token_fp : "") +
                       strlen(project_id ? project_id : "") +
                       strlen(server_url ? server_url : "") + 8);
    sprintf(out, "%s|%s|%s", token_fp ? token_fp : "",
            project_id ? project_id : "", server_url ? server_url : "");
    return out;
}

/* PoP: bw_disk_cache_path @ agent/secret_sources/bitwarden.py:_disk_cache_path */
char *bw_disk_cache_path(const char *home_path) {
    /* resolve_cache_home(home)/cache/bws_cache.json */
    char *home = ssh_resolve_cache_home(home_path);
    char *out = malloc(strlen(home) + 32);
    sprintf(out, "%s/cache/bws_cache.json", home);
    free(home);
    return out;
}

/* PoP: bw_encrypted_disk_cache_path @ agent/secret_sources/bitwarden.py:_encrypted_disk_cache_path */
char *bw_encrypted_disk_cache_path(const char *home_path) {
    /* resolve_cache_home(home)/cache/bws_cache.enc.json */
    char *home = ssh_resolve_cache_home(home_path);
    char *out = malloc(strlen(home) + 40);
    sprintf(out, "%s/cache/bws_cache.enc.json", home);
    free(home);
    return out;
}

/* PoP: bw_platform_binary_name @ agent/secret_sources/bitwarden.py:_platform_binary_name */
const char *bw_platform_binary_name(void) {
#ifdef _WIN32
    return "bws.exe";
#else
    return "bws";
#endif
}

/* PoP: bw_platform_asset_name @ agent/secret_sources/bitwarden.py:_platform_asset_name */
char *bw_platform_asset_name(void) {
#ifdef __APPLE__
    char *out = malloc(96);
    snprintf(out, 96, "bws-macos-universal-%s.zip", BWS_VERSION);
    return out;
#elif defined(_WIN32)
    return NULL; /* Windows arch variant handled by caller if ever needed */
#else
    /* Linux: gnu by default; musl when ldd says so. */
    const char *machine = getenv("HOSTTYPE");
    const char *m = machine ? machine : "x86_64";
    const char *arch = (strcmp(m, "arm64") == 0 || strcmp(m, "aarch64") == 0)
        ? "aarch64" : "x86_64";
    const char *libc = "gnu";
    FILE *fp = popen("ldd --version 2>&1", "r");
    if (fp) {
        char buf[256];
        if (fgets(buf, sizeof(buf), fp) && strstr(buf, "musl")) libc = "musl";
        pclose(fp);
    }
    char *out = malloc(128);
    snprintf(out, 128, "bws-%s-unknown-linux-%s-%s.zip", arch, libc, BWS_VERSION);
    return out;
#endif
}

/* PoP: bw_token_fingerprint @ agent/secret_sources/bitwarden.py:_token_fingerprint */
char *bw_token_fingerprint(const char *token) {
    /* SHA-256 hex prefix, 16 chars — cache key, never logged/displayed. */
    if (!token) return strdup("");
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)token, strlen(token), digest);
    char *out = malloc(33);
    for (int i = 0; i < 16; i++) sprintf(out + i * 2, "%02x", digest[i]);
    out[32] = '\0';
    return out;
}

/* ── 1Password helpers (agent/secret_sources/onepassword.py) ───────────── */

static char *op_sha256_prefix(const char *material, int prefix_chars) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)material, strlen(material), digest);
    char *out = malloc((size_t)prefix_chars * 2 + 1);
    for (int i = 0; i < prefix_chars; i++) sprintf(out + i * 2, "%02x", digest[i]);
    out[prefix_chars * 2] = '\0';
    return out;
}

/* PoP: op_disk_key_str @ agent/secret_sources/onepassword.py:_disk_key_str */
char *op_disk_key_str(const char *auth_fp, const char *account, const char *refs_fp) {
    /* "auth_fp|account|refs_fp" — home already partitions the disk file. */
    char *out = malloc(strlen(auth_fp ? auth_fp : "") + strlen(account ? account : "") +
                       strlen(refs_fp ? refs_fp : "") + 8);
    sprintf(out, "%s|%s|%s", auth_fp ? auth_fp : "", account ? account : "",
            refs_fp ? refs_fp : "");
    return out;
}

/* PoP: op_disk_cache_path @ agent/secret_sources/onepassword.py:_disk_cache_path */
char *op_disk_cache_path(const char *home_path) {
    char *home = ssh_resolve_cache_home(home_path);
    char *out = malloc(strlen(home) + 32);
    sprintf(out, "%s/cache/op_cache.json", home);
    free(home);
    return out;
}

/* PoP: op_auth_fingerprint @ agent/secret_sources/onepassword.py:_auth_fingerprint */
char *op_auth_fingerprint(const char *token) {
    /* SHA-256 prefix over token + OP_ACCOUNT + OP_CONNECT_HOST/TOKEN +
     * all sorted OP_SESSION_* env vars. */
    char *parts[512];
    int n = 0;
    char tokbuf[512], accbuf[512], chbuf[512], ctbuf[512];
    snprintf(tokbuf, sizeof(tokbuf), "token=%s", token ? token : "");
    parts[n++] = tokbuf;
    const char *acc = getenv("OP_ACCOUNT");
    snprintf(accbuf, sizeof(accbuf), "account=%s", acc ? acc : "");
    parts[n++] = accbuf;
    const char *ch = getenv("OP_CONNECT_HOST");
    snprintf(chbuf, sizeof(chbuf), "connect_host=%s", ch ? ch : "");
    parts[n++] = chbuf;
    const char *ct = getenv("OP_CONNECT_TOKEN");
    snprintf(ctbuf, sizeof(ctbuf), "connect_token=%s", ct ? ct : "");
    parts[n++] = ctbuf;
    /* all OP_SESSION_* vars, sorted */
    extern char **environ;
    char sess[128][512];
    int ns = 0;
    for (char **e = environ; *e && ns < 128; e++) {
        if (strncmp(*e, "OP_SESSION_", 11) == 0) {
            snprintf(sess[ns], 512, "%s", *e);
            /* strip the VAR= prefix is NOT what Python does — it keeps key=value */
            ns++;
        }
    }
    for (int a = 0; a < ns - 1; a++)
        for (int b = a + 1; b < ns; b++)
            if (strcmp(sess[b], sess[a]) < 0) {
                char tmp[512]; strcpy(tmp, sess[a]); strcpy(sess[a], sess[b]); strcpy(sess[b], tmp);
            }
    /* build material */
    size_t total = 0;
    for (int i = 0; i < n; i++) total += strlen(parts[i]) + 1;
    for (int i = 0; i < ns; i++) total += strlen(sess[i]) + 1;
    char *material = malloc(total + 1);
    material[0] = '\0';
    for (int i = 0; i < n; i++) {
        strcat(material, parts[i]);
        strcat(material, "\n");
    }
    for (int i = 0; i < ns; i++) {
        strcat(material, sess[i]);
        strcat(material, "\n");
    }
    /* Python joins with \n — trailing newline present; hash includes it */
    char *out = op_sha256_prefix(material, 16);
    free(material);
    return out;
}

/* PoP: op_refs_fingerprint @ agent/secret_sources/onepassword.py:_refs_fingerprint */
char *op_refs_fingerprint(const char *refs_arg) {
    /* SHA-256 prefix over sorted "name=reference" lines joined by \n.
     * Arg = tab-separated "name=reference" pairs (caller-side material). */
    if (!refs_arg || !*refs_arg) return op_sha256_prefix("", 16);
    char *copy = strdup(refs_arg);
    char *lines[512];
    int n = 0;
    char *save = NULL;
    for (char *tok = strtok_r(copy, "\t", &save); tok && n < 512; tok = strtok_r(NULL, "\t", &save)) {
        if (!*tok) continue;
        lines[n++] = tok;
    }
    for (int a = 0; a < n - 1; a++)
        for (int b = a + 1; b < n; b++)
            if (strcmp(lines[b], lines[a]) < 0) {
                char *tmp = lines[a]; lines[a] = lines[b]; lines[b] = tmp;
            }
    size_t total = 1;
    for (int i = 0; i < n; i++) total += strlen(lines[i]) + 1;
    char *material = malloc(total);
    material[0] = '\0';
    for (int i = 0; i < n; i++) {
        strcat(material, lines[i]);
        if (i + 1 < n) strcat(material, "\n");
    }
    free(copy);
    char *out = op_sha256_prefix(material, 16);
    free(material);
    return out;
}
/* PoP: op_scrub @ agent/secret_sources/onepassword.py:_scrub */
char *op_scrub(const char *text) {
    /* Remove ANSI CSI sequences (ESC [ ... letter) and bare ESC; trim. */
    if (!text) return strdup("");
    size_t n = strlen(text);
    char *out = malloc(n + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if ((unsigned char)text[i] == 0x1b) {
            if (i + 1 < n && text[i+1] == '[') {
                i += 2;
                while (i < n && !(text[i] >= 0x40 && text[i] <= 0x7e)) i++;
                continue;
            }
            continue;
        }
        out[o++] = text[i];
    }
    out[o] = '\0';
    char *start = out;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    size_t len = strlen(start);
    while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t' || start[len-1] == '\n' || start[len-1] == '\r')) len--;
    if (start != out) memmove(out, start, len);
    out[len] = '\0';
    return out;
}

/* PoP: op_protected_env_vars @ agent/secret_sources/onepassword.py:protected_env_vars */
char *op_protected_env_vars(const char *cfg_json) {
    /* Python: frozenset({cfg.service_account_token_env or
     * OP_SERVICE_ACCOUNT_TOKEN}). Prints the single env name. */
    const char *token_env = "OP_SERVICE_ACCOUNT_TOKEN";
    if (cfg_json && *cfg_json) {
        json_t *cfg = json_parse(cfg_json, NULL);
        if (cfg && cfg->type == JSON_OBJECT) {
            json_t *te = json_obj_get(cfg, "service_account_token_env");
            if (te && json_is_string(te) && json_string_value(te)[0])
                token_env = json_string_value(te);
        }
        json_free(cfg);
    }
    printf("%s\n", token_env);
    return NULL;
}
