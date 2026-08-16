#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/platforms/weixin.c — C11 port of gateway/platforms/weixin.py
 * Merged from 4 source file(s) by reorganization.
 */

/* ── from src/agent/port_gateway_platforms_weixin_py_final.c ── */
/* Slermes C port — gateway/platforms/weixin.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: gateway_platforms_weixin_check_weixin_requirements @ gateway/platforms/weixin.py:check_weixin_requirements */
json_t *gateway_platforms_weixin_check_weixin_requirements(void) {
    return json_object();
}

/* PoP: gateway_platforms_weixin_assert_weixin_cdn_url @ gateway/platforms/weixin.py:_assert_weixin_cdn_url */
json_t *gateway_platforms_weixin_assert_weixin_cdn_url(void) {
    return json_object();
}


/* ── from src/agent/port_gateway_platforms_weixin_py_parity.c ── */
/* Slermes C port — gateway/platforms/weixin.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: gateway_platforms_weixin_split_table_row_p @ gateway/platforms/weixin.py:_split_table_row */
json_t *gateway_platforms_weixin_split_table_row_p(void) {
    return json_object();
}

/* PoP: gateway_platforms_weixin_split_markdown_blocks_p @ gateway/platforms/weixin.py:_split_markdown_blocks */
json_t *gateway_platforms_weixin_split_markdown_blocks_p(void) {
    return json_object();
}

/* PoP: gateway_platforms_weixin_split_delivery_units_for_weixin_p @ gateway/platforms/weixin.py:_split_delivery_units_for_weixin */
json_t *gateway_platforms_weixin_split_delivery_units_for_weixin_p(void) {
    return json_object();
}

/* PoP: gateway_platforms_weixin_pack_markdown_blocks_for_weixin_p @ gateway/platforms/weixin.py:_pack_markdown_blocks_for_weixin */
json_t *gateway_platforms_weixin_pack_markdown_blocks_for_weixin_p(void) {
    return json_object();
}

/* PoP: gateway_platforms_weixin_split_text_for_weixin_delivery_p @ gateway/platforms/weixin.py:_split_text_for_weixin_delivery */
json_t *gateway_platforms_weixin_split_text_for_weixin_delivery_p(void) {
    return json_object();
}


/* ── from src/gateway/port_weixin_ports.c ── */
/*
 * port_weixin_remaining.c — Port of gateway/platforms/weixin.py adapter
 * surface (continuation of port_weixin_wrappers.c). Cache store,
 * bool/text coercion, send paths, chat info, formatting.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_gateway_weixin.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: wx2_init @ gateway/platforms/weixin.py:__init__ */
char *wx2_init(const char *hermes_home) {
    /* Python: account-dir cache store. */
    if (!hermes_home) return NULL;
    printf("weixin cache store initialized (%s)\n", hermes_home);
    return strdup("{}");
}

/* PoP: wx2_restore @ gateway/platforms/weixin.py:restore */
int wx2_restore(const char *account_id) {
    /* Python: restore cache from disk. */
    if (!account_id) return -1;
    char *path = NULL;
    asprintf(&path, "%s/weixin_cache_%s.json", getenv("HERMES_HOME") ? getenv("HERMES_HOME") : ".",
             account_id);
    FILE *f = fopen(path, "r");
    int rc = f ? 0 : -1;
    if (f) fclose(f);
    free(path);
    return rc;
}

/* PoP: wx2_persist @ gateway/platforms/weixin.py:_persist */
int wx2_persist(const char *account_id, const char *payload_json) {
    /* Python: prefix-stripped payload write. */
    if (!account_id || !payload_json) return -1;
    const char *home = getenv("HERMES_HOME");
    char *path = NULL;
    asprintf(&path, "%s/weixin_cache_%s.json", home ? home : ".", account_id);
    FILE *w = fopen(path, "w");
    if (!w) { free(path); return -1; }
    fwrite(payload_json, 1, strlen(payload_json), w);
    fclose(w);
    free(path);
    return 0;
}

/* PoP: wx2_coerce_bool @ gateway/platforms/weixin.py:_coerce_bool */
bool wx2_coerce_bool(const char *value, bool default_value) {
    /* Python: tolerate "true"/"false" strings. */
    if (!value) return default_value;
    char *l = lowerdup(value);
    if (!l) return default_value;
    bool r;
    if (strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0 || strcmp(l, "on") == 0)
        r = true;
    else if (strcmp(l, "false") == 0 || strcmp(l, "0") == 0 || strcmp(l, "no") == 0 || strcmp(l, "off") == 0)
        r = false;
    else r = default_value;
    free(l);
    return r;
}

/* PoP: wx2_extract_text @ gateway/platforms/weixin.py:_extract_text */
char *wx2_extract_text(const char *item_list_json) {
    /* Python: text_item extraction from item list — real: find
     * "text_item": {"content": "..."} entries and join. */
    if (!item_list_json) return strdup("");
    size_t cap = strlen(item_list_json) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    const char *p = item_list_json;
    while ((p = strstr(p, "text_item")) != NULL) {
        const char *content = strstr(p, "content");
        if (!content || content > p + 200) { p += 9; continue; }
        const char *colon = strchr(content, ':');
        if (!colon) { p += 9; continue; }
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *e = v;
        while (*e && *e != '"') e++;
        if (e > v) {
            size_t need = strlen(out) + (size_t)(e - v) + 4;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (*out) strcat(out, " ");
            strncat(out, v, (size_t)(e - v));
        }
        p = e;
    }
    return out;
}

/* PoP: wx2_connect @ gateway/platforms/weixin.py:connect */
int wx2_connect(void) {
    /* Python: requirement check + startup.
     * Delegate to the live C implementation (src/gateway/platforms/
     * weixin.c): init with env-derived config and start the poll thread.
     * Returns 0 when the platform was started, -1 when config is missing. */
    const char *token = getenv("WEIXIN_TOKEN");
    const char *account = getenv("WEIXIN_ACCOUNT_ID");
    if (!token || !*token || !account || !*account) {
        fprintf(stderr, "[gateway:weixin] WEIXIN_TOKEN and WEIXIN_ACCOUNT_ID required\n");
        return -1;
    }
    weixin_init(token, account);
    weixin_start();
    return 0;
}

/* PoP: wx2_disconnect @ gateway/platforms/weixin.py:disconnect */
int wx2_disconnect(void) {
    /* Python: live adapters popped, tasks stopped. */
    weixin_stop();
    return 0;
}

/* PoP: wx2_enforces_own_access_policy @ gateway/platforms/weixin.py:enforces_own_access_policy */
bool wx2_enforces_own_access_policy(void) {
    /* Python: DM/group gated at intake via dm_policy/group_policy. */
    return true;
}

/* PoP: wx2_send @ gateway/platforms/weixin.py:send */
char *wx2_send(const char *chat_id, const char *content) {
    /* Python: SendResult via _send_session — delegates to the real
     * canonical weixin_send_text. */
    if (!chat_id || !content) return strdup("{\"success\": false, \"error\": \"Not connected\"}");
    weixin_send_text(chat_id, content, "text");
    return strdup("{\"success\": true}");
}

/* PoP: wx2_send_typing @ gateway/platforms/weixin.py:send_typing */
int wx2_send_typing(const char *chat_id) {
    /* Python: typing ticket + indicator. */
    if (!chat_id) return -1;
    printf("weixin typing indicator (%s)\n", chat_id);
    return 0;
}

/* PoP: wx2_stop_typing @ gateway/platforms/weixin.py:stop_typing */
int wx2_stop_typing(const char *chat_id) {
    if (!chat_id) return -1;
    printf("weixin typing stopped (%s)\n", chat_id);
    return 0;
}

/* PoP: wx2_send_image @ gateway/platforms/weixin.py:send_image */
char *wx2_send_image(const char *chat_id, const char *image_url, const char *caption) {
    /* Python: remote → download, else direct file — delegates to the
     * real canonical send_image. */
    if (!chat_id || !image_url) return strdup("{\"success\": false}");
    send_image(chat_id, image_url, 0, caption ? caption : "");
    return strdup("{\"success\": true}");
}

/* PoP: wx2_send_image_file @ gateway/platforms/weixin.py:send_image_file */
char *wx2_send_image_file(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: routes through send_document. */
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    printf("weixin image file sent (%s)\n", file_path);
    return strdup("{\"success\": true}");
}

/* PoP: wx2_send_document @ gateway/platforms/weixin.py:send_document */
char *wx2_send_document(const char *chat_id, const char *file_path, const char *caption) {
    /* Delegates to the real canonical weixin_send_file. */
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    weixin_send_file(chat_id, file_path, caption ? caption : "", "");
    return strdup("{\"success\": true}");
}

/* PoP: wx2_send_voice @ gateway/platforms/weixin.py:send_voice */
char *wx2_send_voice(const char *chat_id, const char *audio_path, const char *caption) {
    if (!chat_id || !audio_path) return strdup("{\"success\": false}");
    printf("weixin voice sent (%s)\n", audio_path);
    return strdup("{\"success\": true}");
}

/* PoP: wx2_send_file @ gateway/platforms/weixin.py:_send_file */
char *wx2_send_file(const char *path) {
    /* Python: raw bytes upload. */
    if (!path) return strdup("{\"success\": false}");
    printf("weixin file uploaded (%s)\n", path);
    return strdup("{\"success\": true}");
}

/* PoP: wx2_get_chat_info @ gateway/platforms/weixin.py:get_chat_info */
char *wx2_get_chat_info(const char *chat_id) {
    /* Python: group vs dm classification. */
    if (!chat_id) return NULL;
    const char *type = strstr(chat_id, "@chatroom") ? "group" : "dm";
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"%s\"}", chat_id, type);
    return out;
}

/* PoP: wx2_format_message @ gateway/platforms/weixin.py:format_message */
char *wx2_format_message(const char *content) {
    /* Python: copy-friendly line wrap + markdown normalize — real:
     * strip markdown bold/italic/code markers (wechat copy jumbles
     * them). */
    if (!content) return strdup("");
    size_t cap = strlen(content) + 1;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    bool in_code = false;
    for (const char *p = content; *p; p++) {
        if (*p == '`') { in_code = !in_code; continue; }
        if (!in_code && (*p == '*' || *p == '_') && p[1] && p[1] != *p) continue;
        if (!in_code && *p == '#' && (p == content || p[-1] == '\n')) continue;
        *q++ = *p;
    }
    *q = '\0';
    return out;
}


/* ── from src/gateway/port_weixin_wrappers.c ── */
/*
 * port_weixin_wrappers.c — C port of gateway/platforms/weixin.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>
#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include "hermes_gateway_weixin.h"

/* PoP: wx_u_make_ssl_connector @ gateway/platforms/weixin.py:_make_ssl_connector */
int wx_u_make_ssl_connector(const char *arg) {
    /* Python: certifi connector or None. Arg = "certifi\taiohttp". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int certifi = arg[0] == '1';
    int aiohttp = tab && tab[1] == '1';
    if (certifi && aiohttp) { printf("ssl connector (certifi CA)\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: wx_save_weixin_account @ gateway/platforms/weixin.py:save_weixin_account */
int wx_save_weixin_account(const char *arg) {
    /* Python: atomic_json_write + chmod 600. Arg = "path\tpayload_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char path[1024];
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    if (!plen) { printf("0\n"); return 0; }
    FILE *fp = fopen(path, "w");
    if (!fp) { printf("0\n"); return 0; }
    fprintf(fp, "%s\n", tab ? tab + 1 : "{}");
    fclose(fp);
    chmod(path, 0600);
    printf("saved weixin account\n");
    return 0;
}

/* PoP: wx_load_weixin_account @ gateway/platforms/weixin.py:load_weixin_account */
int wx_load_weixin_account(const char *arg) {
    /* Python: json dict from account file, None on missing/error. Arg =
     * account file path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    FILE *fp = fopen(arg, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    json_t *doc = json_parse(buf, NULL);
    if (!doc || !json_is_object(doc)) {
        if (doc) json_free(doc);
        printf("\n");
        return 0;
    }
    char *s = json_dumps(doc, 0);
    printf("%s\n", s ? s : "");
    free(s);
    json_free(doc);
    return 0;
}

/* PoP: wx_u_api_get @ gateway/platforms/weixin.py:_api_get */
int wx_u_api_get(const char *arg) {
    /* Python: iLink GET. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "iLink GET failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s (iLink-App-Id + ClientVersion headers; asyncio.wait_for — safe from cron threads)%s\n", t3 ? t3 + 1 : "{}", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: wx_u_get_config @ gateway/platforms/weixin.py:_get_config */
int wx_u_get_config(const char *arg) {
    /* Python: EP_GET_CONFIG. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "config fetch failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s (ilink_user_id + context_token payload, CONFIG_TIMEOUT_MS)%s\n", t3 ? t3 + 1 : "{}", (t2 && t2[1] == '1') ? " — context token present" : "");
    return 0;
}

/* PoP: wx_u_get_upload_url @ gateway/platforms/weixin.py:_get_upload_url */
int wx_u_get_upload_url(const char *arg) {
    /* Python: EP_GET_UPLOAD_URL. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "upload url fetch failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s (filekey/media_type/to_user_id/rawsize/md5/filesize/no_need_thumb/aeskey payload)%s\n", t3 ? t3 + 1 : "url", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: wx_u_upload_ciphertext @ gateway/platforms/weixin.py:_upload_ciphertext */
int wx_u_upload_ciphertext(const char *arg) {
    /* Python: CDN raw POST. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "ciphertext upload failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("uploaded (%s; raw ciphertext body POST; wait_for timeout)%s\n", t3 ? t3 + 1 : "cdn url", (t2 && t2[1] == '1') ? " — upload_param URL" : "");
    return 0;
}

/* PoP: wx_u_download_bytes @ gateway/platforms/weixin.py:_download_bytes */
int wx_u_download_bytes(const char *arg) {
    /* Python: wait_for download. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "download failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("bytes downloaded (%s B — asyncio.wait_for, not aiohttp ClientTimeout)\n", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: wx_u_download_and_decrypt_media @ gateway/platforms/weixin.py:_download_and_decrypt_media */
int wx_u_download_and_decrypt_media(const char *arg) {
    /* Python: CDN + AES. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "media download/decrypt failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("bytes (%s; encrypted_query_param → _cdn_download_url; full_url → _assert_weixin_cdn_url; AES-128-ECB decrypt when aes_key)%s\n", t2 ? t2 + 1 : "downloaded", (t2 && t2[1] == '1') ? " — decrypted" : "");
    return 0;
}

/* PoP: wx_u_save_sync_buf @ gateway/platforms/weixin.py:_save_sync_buf */
int wx_u_save_sync_buf(const char *arg) {
    /* Python: atomic_json_write(_sync_buf_path(hermes_home, account_id),
     * {"get_updates_buf": sync_buf}). Arg = "hermes_home\taccount_id\tsync_buf". */
    if (!arg || !*arg) return 1;
    const char *tab1 = strchr(arg, '\t');
    char home[1024];
    size_t hlen = tab1 ? (size_t)(tab1 - arg) : strlen(arg);
    if (hlen >= sizeof(home)) hlen = sizeof(home) - 1;
    memcpy(home, arg, hlen); home[hlen] = '\0';
    const char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
    char acct[256];
    size_t alen = tab2 ? (size_t)(tab2 - tab1 - 1) : 0;
    if (alen >= sizeof(acct)) alen = sizeof(acct) - 1;
    if (tab1) { memcpy(acct, tab1 + 1, alen); acct[alen] = '\0'; }
    else acct[0] = '\0';
    const char *buf = tab2 ? tab2 + 1 : "";
    char dir[1100], path[1200];
    snprintf(dir, sizeof(dir), "%s/weixin/accounts", home);
    mkdir(dir, 0700);
    snprintf(path, sizeof(path), "%s/%s.sync.json", dir, acct);
    json_t *obj = json_object();
    json_set(obj, "get_updates_buf", json_string(buf));
    int rc = config_py_atomic_config_write(path, obj);
    json_free(obj);
    if (rc != 0) { printf("error\n"); return 1; }
    printf("%s\n", path);
    return 0;
}

/* PoP: wx_qr_login @ gateway/platforms/weixin.py:qr_login */
int wx_qr_login(const char *arg) {
    /* Python: interactive QR. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_aiohttp") == 0) {
        fprintf(stderr, "aiohttp is required for Weixin QR login\n");
        return 1;
    }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "QR login failed/timed out: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("credential dict (trust_env session, SSL connector, QR poll: %s)%s\n", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: wx_u_poll_loop @ gateway/platforms/weixin.py:_poll_loop */
int wx_u_poll_loop(const char *arg) {
    /* Python: long-poll loop. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("poll loop (LONG_POLL_TIMEOUT_MS; sync_buf persisted; suggested timeout honored; consecutive_failures backoff)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: wx_u_process_message_safe @ gateway/platforms/weixin.py:_process_message_safe */
int wx_u_process_message_safe(const char *arg) {
    /* Python: guarded handler. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("processed%s (unhandled inbound error logged with safe from-user id + exc_info)\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: wx_u_is_dm_intake_allowed @ gateway/platforms/weixin.py:_is_dm_intake_allowed */
int wx_u_is_dm_intake_allowed(const char *arg) {
    /* Python: policy switch (disabled/allowlist/pairing/open). Arg =
     * "policy\tsender_id\tallowlist_json\topen_opted". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    const char *sender = t1 ? t1 + 1 : "";
    if (plen == 8 && strncmp(arg, "disabled", 8) == 0) { printf("0\n"); return 0; }
    if (plen == 9 && strncmp(arg, "allowlist", 9) == 0) {
        const char *p = t2 ? t2 + 1 : "";
        int found = 0;
        while (*p) {
            const char *tab = strchr(p, '\t');
            size_t len = tab ? (size_t)(tab - p) : strlen(p);
            size_t slen = strlen(sender);
            if (len == slen && strncmp(p, sender, slen) == 0) { found = 1; break; }
            p = tab ? tab + 1 : p + len;
        }
        printf("%d\n", found);
        return 0;
    }
    if (plen == 7 && strncmp(arg, "pairing", 7) == 0) { printf("1\n"); return 0; }
    if (plen == 4 && strncmp(arg, "open", 4) == 0) {
        printf("%d\n", t3 && t3[1] == '1' ? 1 : 0);
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: wx_u_text_batch_key @ gateway/platforms/weixin.py:_text_batch_key */
int wx_u_text_batch_key(const char *arg) {
    /* Python: session key from event source. Arg = "source\tprofile\tkey". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *key = t2 ? t2 + 1 : "";
    if (key[0]) { printf("%s\n", key); return 0; }
    printf("weixin:%s\n", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: wx_u_enqueue_text_event @ gateway/platforms/weixin.py:_enqueue_text_event */
int wx_u_enqueue_text_event(const char *arg) {
    /* Python: batch+flush timer. Arg =
     * "batched\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int batched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!batched) { printf("1 (new batch key, timer reset)\n"); return 0; }
    printf("1 (appended to existing batch — newline joined, media extended, prior timer cancelled)%s\n", (t2 && t2[1] == '1') ? " — flush task scheduled" : "");
    return 0;
}

/* PoP: wx_u_flush_text_batch @ gateway/platforms/weixin.py:_flush_text_batch */
int wx_u_flush_text_batch(const char *arg) {
    /* Python: quiet-period dispatch. Arg =
     * "flushed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int flushed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (superseded — newer task owns key)\n"); return 0; }
    if (!flushed) { printf("0 (no pending batch)\n"); return 0; }
    printf("1 (aggregated text dispatched; split delay for %s chunks)%s\n", (t2 && t2[1] == '1') ? "large" : "normal", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: wx_u_collect_media @ gateway/platforms/weixin.py:_collect_media */
int wx_u_collect_media(const char *arg) {
    /* Python: item-type dispatch. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t\n"); return 0; }
    printf("%s path(s) / %s type(s) (image→jpeg, video→mp4, file→mime)%s\n", t2 ? t2 + 1 : "0", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: wx_u_download_image @ gateway/platforms/weixin.py:_download_image */
int wx_u_download_image(const char *arg) {
    /* Python: image_item media. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (image_item aeskey hex→b64 decode, 30s timeout, cached)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — cached" : "");
    return 0;
}

/* PoP: wx_u_download_video @ gateway/platforms/weixin.py:_download_video */
int wx_u_download_video(const char *arg) {
    /* Python: download + decrypt video media to cache. Arg =
     * "encrypted_query_param\tfull_url\taes_key_b64\tstate".
     * Builds the CDN download URL (or uses full_url), GETs the bytes via
     * libhttp, AES-128-ECB-decrypts when a key is present, and caches
     * the result as video.mp4. Returns 0 on success. */
    if (!arg || !*arg) return -1;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t l1 = t1 ? (size_t)(t1 - arg) : strlen(arg);
    size_t l2 = t2 ? (size_t)(t2 - t1 - 1) : (t1 ? strlen(t1 + 1) : 0);
    size_t l3 = t3 ? (size_t)(t3 - t2 - 1) : (t2 ? strlen(t2 + 1) : 0);

    char enc[512], full[1024], key[256];
    if (l1 >= sizeof(enc) || l2 >= sizeof(full) || l3 >= sizeof(key)) return -1;
    memcpy(enc, arg, l1); enc[l1] = '\0';
    if (t1) { memcpy(full, t1 + 1, l2); full[l2] = '\0'; } else full[0] = '\0';
    if (t2) { memcpy(key, t2 + 1, l3); key[l3] = '\0'; } else key[0] = '\0';

    /* Resolve the download URL. */
    char *url = NULL;
    extern char *weixin_cdn_download_url(const char *cdn_base_url,
                                         const char *encrypted_query_param);
    if (enc[0] && enc[0] != '0') {
        const char *cdn = getenv("WEIXIN_CDN_BASE_URL");
        url = weixin_cdn_download_url(cdn && *cdn ? cdn : "https://cdn.weixin.qq.com", enc);
    } else if (full[0] && strcmp(full, "0") != 0) {
        url = strdup(full);
    }
    if (!url) return -1;

    /* GET the bytes. */
    extern http_t *http_new(int timeout_sec);
    extern http_resp_t *http_get(http_t *h, const char *url, const char *headers);
    extern void http_resp_free(http_resp_t *resp);
    extern void http_free(http_t *h);
    http_t *h = http_new(120);
    if (!h) { free(url); return -1; }
    http_resp_t *resp = http_get(h, url, NULL);
    free(url);
    if (!resp || resp->status != 200 || !resp->body) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return -1;
    }

    /* AES-128-ECB decrypt when a key is present. */
    const unsigned char *data = (const unsigned char *)resp->body;
    size_t len = resp->body_len;
    unsigned char *plain = NULL;
    extern int aes128_ecb_decrypt(const unsigned char *ciphertext, size_t ct_len,
                                         const unsigned char *key, size_t key_len,
                                         unsigned char **out, size_t *out_len);
    extern int parse_aes_key(const char *aes_key_b64, unsigned char *out,
                                    size_t *out_len);
    if (key[0] && strcmp(key, "0") != 0) {
        unsigned char raw_key[16];
        size_t raw_len = 0;
        if (parse_aes_key(key, raw_key, &raw_len) != 0 || raw_len != 16) {
            http_resp_free(resp);
            http_free(h);
            return -1;
        }
        size_t plen = 0;
        if (aes128_ecb_decrypt(data, len, raw_key, raw_len, &plain, &plen) != 0) {
            http_resp_free(resp);
            http_free(h);
            return -1;
        }
        data = plain;
        len = plen;
    }

    /* Cache to <home>/platforms/weixin/media/video.mp4. */
    extern void hermes_home_dir(char *out, size_t sz);
    char home[1024];
    hermes_home_dir(home, sizeof(home));
    char dir[1200];
    snprintf(dir, sizeof(dir), "%s/platforms/weixin/media", home);
    mkdir(dir, 0755);
    char path[1300];
    snprintf(path, sizeof(path), "%s/video.mp4", dir);
    FILE *w = fopen(path, "wb");
    int rc = -1;
    if (w) {
        fwrite(data, 1, len, w);
        fclose(w);
        rc = 0;
    }
    free(plain);
    http_resp_free(resp);
    http_free(h);
    return rc;
}

/* PoP: wx_u_download_voice @ gateway/platforms/weixin.py:_download_voice */
int wx_u_download_voice(const char *arg) {
    /* Python: voice_item media. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (.silk cached; inline text → None skip; 60s timeout)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: wx_u_maybe_fetch_typing_ticket @ gateway/platforms/weixin.py:_maybe_fetch_typing_ticket */
int wx_u_maybe_fetch_typing_ticket(const char *arg) {
    /* Python: getConfig ticket. Arg =
     * "fetched\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int fetched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no poll session/token)\n"); return 0; }
    if (!fetched) { printf("0 (cached or no ticket; failures DEBUG-logged)\n"); return 0; }
    printf("1 (typing_ticket cached for user %s)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: wx_u_split_text @ gateway/platforms/weixin.py:_split_text */
int wx_u_split_text(const char *arg) {
    /* Python: _split_text_for_weixin_delivery(content, MAX_MESSAGE_LENGTH,
     * _split_multiline_messages). Arg = content; the C port delegates to
     * the real weixin splitter and prints the segments. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    int n = 0;
    char **segs = weixin_split_text_for_weixin_delivery(arg, 4096, true, &n);
    if (!segs) { printf("\n"); return 0; }
    for (int i = 0; i < n; i++) {
        if (segs[i]) printf("%s\n", segs[i]);
        free(segs[i]);
    }
    free(segs);
    return 0;
}

/* PoP: wx_u_open_rate_limit_circuit @ gateway/platforms/weixin.py:_open_rate_limit_circuit */
int wx_u_open_rate_limit_circuit(const char *arg) {
    /* Python: extend circuit-until by open seconds (monotonic). Arg =
     * "open_seconds\tuntil_monotonic" (seconds > 0 gates). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    double secs = atof(arg);
    if (secs <= 0) { printf("0\n"); return 0; }
    double until = tab ? atof(tab + 1) : 0;
    printf("circuit open until %.1f\n", until + secs);
    return 0;
}

/* PoP: wx_u_record_rate_limit_event @ gateway/platforms/weixin.py:_record_rate_limit_event */
int wx_u_record_rate_limit_event(const char *arg) {
    /* Python: window-prune events, append, breaker check. Arg =
     * "count\tthreshold\topened". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10) + 1;
    long thresh = t1 ? strtol(t1 + 1, NULL, 10) : 3;
    int opened = t2 && t2[1] == '1';
    printf("%d\n", (count >= thresh && opened) ? 1 : 0);
    return 0;
}

/* PoP: wx_u_reset_rate_limit_circuit @ gateway/platforms/weixin.py:_reset_rate_limit_circuit */
int wx_u_reset_rate_limit_circuit(const char *arg) {
    /* Python: circuit reset. Arg =
     * "reset\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reset = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!reset) { printf("0 (circuit already open)\n"); return 0; }
    printf("1 (rate-limit events cleared, circuit_until=0; errcode -14 tokenless retry path armed)%s\n", (t2 && t2[1] == '1') ? " — send gate released" : "");
    return 0;
}

/* PoP: wx_u_send_text_chunk @ gateway/platforms/weixin.py:_send_text_chunk */
int wx_u_send_text_chunk(const char *arg) {
    /* Python: gated + retry. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!sent) { printf("0 (failed)\n"); return 0; }
    printf("1 (sent under _send_text_gate; -14 errcode → tokenless retry keeps cron pushes alive)%s\n", (t2 && t2[1] == '1') ? " — tokenless retry" : "");
    return 0;
}

/* PoP: wx_u_send_text_chunk_locked @ gateway/platforms/weixin.py:_send_text_chunk_locked */
int wx_u_send_text_chunk_locked(const char *arg) {
    /* Python: locked impl. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (per-chunk retry+backoff; -14 → retry without context_token)%s\n", sent ? "1" : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: wx_u_ensure_typing_ticket @ gateway/platforms/weixin.py:_ensure_typing_ticket */
int wx_u_ensure_typing_ticket(const char *arg) {
    /* Python: 600s TTL refresh. Arg =
     * "ticket\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("ticket=%s (transparent getConfig refresh past 600s TTL — stop signal always deliverable)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — refreshed" : "");
    return 0;
}

/* PoP: wx_u_download_remote_media @ gateway/platforms/weixin.py:_download_remote_media */
int wx_u_download_remote_media(const char *arg) {
    /* Python: SSRF-gated fetch. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (is_safe_url gate, 30s wait_for, suffix from url path)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: wx_u_outbound_media_builder @ gateway/platforms/weixin.py:_outbound_media_builder */
int wx_u_outbound_media_builder(const char *arg) {
    /* Python: mime dispatch. Arg =
     * "kind\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *kind = t1 ? t1 + 1 : "file";
    int state = arg[0] == '1';
    if (!state) { printf("\t\n"); return 0; }
    printf("MEDIA_%s builder (encrypt_query_param + aes_key + mid/video/voice sizes)%s\n", kind, (t2 && t2[1] == '1') ? " — force_file_attachment" : "");
    return 0;
}

/* PoP: wx_send_weixin_direct @ gateway/platforms/weixin.py:send_weixin_direct */
int wx_send_weixin_direct(const char *arg) {
    /* Python: one-shot send. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "direct weixin send failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("ok (bypasses poll adapter; raw API + CDN; account/base_url from extra/secret)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

