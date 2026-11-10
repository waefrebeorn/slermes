/*
 * port_auth_qwen.c — Port of Python: hermes_cli/auth_qwen.py (file/env half).
 */

#include "port_auth_qwen.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static char *dup_str(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *str_strip_dup(const char *s)
{
    if (!s) return dup_str("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* PoP: qwen_auth_path @ hermes_cli/auth_qwen.py:_qwen_cli_auth_path */
char *qwen_auth_path(const char *home)
{
    if (!home || !*home) return NULL;
    size_t cap = strlen(home) + 32;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    snprintf(out, cap, "%s/.qwen/oauth_creds.json", home);
    return out;
}

static char *read_file(const char *path, size_t *len_out)
{
    FILE *fh = fopen(path, "rb");
    if (!fh) return NULL;
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { fclose(fh); return NULL; }
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, fh)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) { free(buf); fclose(fh); return NULL; }
            buf = tmp;
        }
    }
    fclose(fh);
    buf[len] = '\0';
    if (len_out) *len_out = len;
    return buf;
}

/* PoP: qwen_read_tokens @ hermes_cli/auth_qwen.py:_read_qwen_cli_tokens */
json_t *qwen_read_tokens(const char *path, char **err)
{
    if (err) *err = NULL;
    char msg[1024];

    struct stat sb;
    if (!path || stat(path, &sb) != 0) {
        if (err) *err = dup_str("Qwen CLI credentials not found. Run 'qwen auth qwen-oauth' first.");
        return NULL;
    }

    size_t len = 0;
    char *raw = read_file(path, &len);
    if (!raw) {
        snprintf(msg, sizeof(msg), "Failed to read Qwen CLI credentials from %s: cannot open file",
                 path);
        if (err) *err = dup_str(msg);
        return NULL;
    }

    /* encoding="utf-8-sig": a leading BOM is not part of the document */
    const char *text = raw;
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        text += 3;

    char *parse_err = NULL;
    json_t *doc = json_parse(text, &parse_err);
    free(raw);
    if (!doc) {
        snprintf(msg, sizeof(msg), "Failed to read Qwen CLI credentials from %s: %s",
                 path, parse_err ? parse_err : "invalid JSON");
        if (err) *err = dup_str(msg);
        if (parse_err) free(parse_err);
        return NULL;
    }
    if (doc->type != JSON_OBJECT) {
        snprintf(msg, sizeof(msg), "Invalid Qwen CLI credentials in %s.", path);
        if (err) *err = dup_str(msg);
        json_free(doc);
        return NULL;
    }
    return doc;
}

static int mkdir_p(const char *dir)
{
    char tmp[1024];
    size_t n = strlen(dir);
    if (n >= sizeof(tmp)) return 0;
    memcpy(tmp, dir, n + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            mkdir(tmp);
            *p = saved;
        }
    }
    mkdir(tmp);
    return 1;
}

static void parent_dir(const char *path, char *out, size_t cap)
{
    const char *slash = strrchr(path, '/');
    if (!slash) { out[0] = '\0'; return; }
    size_t n = (size_t)(slash - path);
    if (n >= cap) n = cap - 1;
    memcpy(out, path, n);
    out[n] = '\0';
}

/* PoP: qwen_save_tokens @ hermes_cli/auth_qwen.py:_save_qwen_cli_tokens */
int qwen_save_tokens(const char *path, const char *json_text)
{
    if (!path || !json_text) return 0;
    char dir[1024];
    parent_dir(path, dir, sizeof(dir));
    if (dir[0]) mkdir_p(dir);

    size_t cap = strlen(path) + 16;
    char *tmp_path = (char *)malloc(cap);
    if (!tmp_path) return 0;
    snprintf(tmp_path, cap, "%s.tmp%ld", path, (long)getpid());

    FILE *fh = fopen(tmp_path, "wb");
    if (!fh) { free(tmp_path); return 0; }
    size_t len = strlen(json_text);
    int ok = fwrite(json_text, 1, len, fh) == len;
    if (fflush(fh) != 0) ok = 0;
    fclose(fh);
    if (!ok) { remove(tmp_path); free(tmp_path); return 0; }

    /* owner-only before any byte is visible under the real name */
    chmod(tmp_path, 0600);
    remove(path);
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        free(tmp_path);
        return 0;
    }
    chmod(path, 0600);
    free(tmp_path);
    return 1;
}

/* PoP: qwen_token_is_expiring @ hermes_cli/auth_qwen.py:_qwen_access_token_is_expiring */
int qwen_token_is_expiring(const char *expiry_date_ms, int skew_seconds,
                           double now_seconds)
{
    if (!expiry_date_ms) return 1;
    /* Python int(): a decimal integer only. "1e12" and "12.5" raise, and the
     * caller treats any conversion failure as "expiring". */
    const char *p = expiry_date_ms;
    while (*p && isspace((unsigned char)*p)) p++;
    int negative = 0;
    if (*p == '+' || *p == '-') { negative = (*p == '-'); p++; }
    if (!isdigit((unsigned char)*p)) return 1;
    char *end = NULL;
    long long value = strtoll(p, &end, 10);
    if (!end || end == p) return 1;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 1;
    double expiry = (double)(negative ? -value : value);

    int skew = skew_seconds < 0 ? 0 : skew_seconds;
    return (now_seconds + (double)skew) * 1000.0 >= expiry;
}

/* PoP: qwen_base_url @ hermes_cli/auth_qwen.py:_qwen_base_url */
char *qwen_base_url(const char *env_value)
{
    char *value = str_strip_dup(env_value ? env_value : "");
    if (!value) return NULL;
    size_t n = strlen(value);
    while (n > 0 && value[n - 1] == '/') value[--n] = '\0';
    if (n == 0) {
        free(value);
        return dup_str(QWEN_DEFAULT_BASE_URL);
    }
    return value;
}
