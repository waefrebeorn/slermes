/*
 * weixin_http.c - weixin sync-buf + token path helpers.
 *
 * Split from weixin.c (soul.md: self-contained modules). Token path/save,
 * weixin dir creation and the sync-buffer load/save path.
 * No file-scope state.
 */
#include "weixin_http.h"
#include "hermes_json.h"   /* json_t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "libbase64/base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>



/* ================================================================
 *  Context token store (file-based)
 * ================================================================ */ void ensure_weixin_dir(void) {
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (home) {
        char dir[MAX_PATH];
        snprintf(dir, sizeof(dir), "%s/%s", home, CONTEXT_TOKEN_DIR);
        mkdir(dir, 0755);
    }
}
 void ctx_token_save(const char *user_id, const char *token) {
    char path[MAX_PATH];
    ctx_token_path(path, sizeof(path));

    json_t *data = NULL;
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            char *buf = malloc((size_t)sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, (size_t)sz, f);
                buf[n] = '\0';
                data = json_parse(buf, NULL);
                free(buf);
            }
        }
        fclose(f);
    }

    if (!data) data = json_object();
    json_set(data, user_id, json_string(token));

    char *json_str = json_serialize(data);
    if (json_str) {
        FILE *out = fopen(path, "wb");
        if (out) { fwrite(json_str, 1, strlen(json_str), out); fclose(out); }
        free(json_str);
    }
    json_free(data);
}


/* PoP: _load_sync_buf @ gateway/platforms/weixin.py:_load_sync_buf */
/* PoP: weixin_load_sync_buf @ gateway/platforms/weixin.py:_load_sync_buf */
char *weixin_load_sync_buf(const char *hermes_home, const char *account_id)
{
    if (!hermes_home || !account_id) return NULL;
    char *path = weixin_account_file(hermes_home, account_id);
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return strdup(""); }
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}
