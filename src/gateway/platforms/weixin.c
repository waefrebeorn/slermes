/*
 * weixin.c — Gateway platform adapter.
 * Port of Python gateway/platforms/weixin.py.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway_weixin.h"
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

/* ================================================================
 *  Constants
 * Port of Python gateway/platforms/weixin.py.
 * ================================================================ */

#define ILINK_BASE_URL "https://ilinkai.weixin.qq.com"
#define EP_GET_UPDATES "ilink/bot/getupdates"
#define EP_SEND_MESSAGE "ilink/bot/sendmessage"
#define LONG_POLL_TIMEOUT_MS 35000
#define API_TIMEOUT_MS 15000
#define MAX_CONSECUTIVE_FAILURES 3
#define RETRY_DELAY_SEC 2
#define BACKOFF_DELAY_SEC 30
#define CONTEXT_TOKEN_DIR "weixin"
#define MAX_PATH 4096
#define MAX_BODY 16384

/* ================================================================
 *  State
 * ================================================================ */

typedef struct {
    char token[512];
    char account_id[128];
    char base_url[1024];
    bool running;
    http_t *http;
    char sync_buf[4096];
    int consecutive_failures;
} weixin_state_t;

static weixin_state_t g_wx;

/* ================================================================
 *  iLink API — POST with custom headers
 * ================================================================ */

/* Port of Python agent/google_code_assist.py:_build_headers(). */
/* Build iLink headers as a single newline-separated string */
static void build_headers(const char *body, char *hdr_buf, size_t hdr_sz,
                           const char *override_token) {
    const char *tok = override_token ? override_token : g_wx.token;
    snprintf(hdr_buf, hdr_sz,
        "Content-Type: application/json\n"
        "AuthorizationType: ilink_bot_token\n"
        "Authorization: Bearer %s\n"
        "Content-Length: %zu\n"
        "iLink-App-Id: bot\n"
        "iLink-App-ClientVersion: 131584",
        tok, strlen(body));
}

/* POST to iLink API with full payload */
static char *wx_api_post(const char *endpoint, const char *payload_json,
                          int timeout_ms, const char *override_token) {
    char url[2048];
    snprintf(url, sizeof(url), "%s/%s", g_wx.base_url, endpoint);

    /* Build full payload: add base_info */
    char full_body[MAX_BODY];
    snprintf(full_body, sizeof(full_body),
             "%s,\"base_info\":{\"channel_version\":\"2.2.0\"}}",
             payload_json);

    /* Build headers using shared helper */
    char headers[1024];
    const char *tok = override_token ? override_token : g_wx.token;
    build_headers(full_body, headers, sizeof(headers), tok);

    /* Create temporary http client for each request */
    http_t *h = http_new(timeout_ms > 0 ? (timeout_ms / 1000) : 15);
    if (!h) return NULL;

    http_resp_t *resp = http_request(h, HTTP_POST, url, headers,
                                      full_body, strlen(full_body));

    char *result = NULL;
    if (resp && resp->body) {
        result = strdup(resp->body);
    }
    if (resp) http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Context token store (file-based)
 * ================================================================ */

static void ensure_weixin_dir(void) {
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (home) {
        char dir[MAX_PATH];
        snprintf(dir, sizeof(dir), "%s/%s", home, CONTEXT_TOKEN_DIR);
        mkdir(dir, 0755);
    }
}

static void ctx_token_path(char *buf, size_t sz) {
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/%s/%s.context-tokens.json", home, CONTEXT_TOKEN_DIR, g_wx.account_id);
}

static void ctx_token_save(const char *user_id, const char *token) {
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

static const char *ctx_token_get(const char *user_id) {
    static char cached_token[512];
    cached_token[0] = '\0';

    char path[MAX_PATH];
    ctx_token_path(path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    json_t *data = json_parse(buf, NULL);
    free(buf);
    if (!data) return NULL;

    const char *tok = json_get_str(data, user_id, "");
    if (*tok) {
        snprintf(cached_token, sizeof(cached_token), "%s", tok);
        json_free(data);
        return cached_token;
    }
    json_free(data);
    return NULL;
}

/* Port of Python gateway/platforms/weixin.py:_sync_buf_path(). */
/* ================================================================
 *  Sync buffer persistence
 * ================================================================ */

static void sync_buf_path(char *buf, size_t sz) {
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/%s/%s.sync.json", home, CONTEXT_TOKEN_DIR, g_wx.account_id);
}

static void sync_buf_load(void) {
    char path[MAX_PATH];
    sync_buf_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    json_t *data = json_parse(buf, NULL);
    free(buf);
    if (data) {
        const char *sb = json_get_str(data, "get_updates_buf", "");
        if (*sb)
            snprintf(g_wx.sync_buf, sizeof(g_wx.sync_buf), "%s", sb);
        json_free(data);
    }
}

static void sync_buf_save(void) {
    char path[MAX_PATH];
    sync_buf_path(path, sizeof(path));
    char json[4128];
    snprintf(json, sizeof(json),
             "{\"get_updates_buf\":\"%s\"}", g_wx.sync_buf);
    ensure_weixin_dir();
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(json, 1, strlen(json), f); fclose(f); }
}

/* ================================================================
 *  iLink API wrappers
 * ================================================================ */

/* Port of Python gateway/platforms/weixin.py:_get_updates(). */
/* getUpdates — long-poll */
static char *get_updates(void) {
    char payload[MAX_BODY];
    snprintf(payload, sizeof(payload),
             "{\"get_updates_buf\":\"%s\"", g_wx.sync_buf);

    char *resp = wx_api_post(EP_GET_UPDATES, payload, LONG_POLL_TIMEOUT_MS, NULL);
    if (!resp) {
        /* Timeout is normal — return empty result */
        return strdup("{\"ret\":0,\"msgs\":[]}");
    }
    return resp;
}

/* sendMessage */
static int send_message(const char *to_user_id, const char *text,
                         const char *context_token) {
    char payload[MAX_BODY];
    if (context_token && *context_token) {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":1,\"content\":\"%s\","
                 "\"context_token\":\"%s\"",
                 to_user_id, text, context_token);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":1,\"content\":\"%s\"",
                 to_user_id, text);
    }

    char *resp = wx_api_post(EP_SEND_MESSAGE, payload, API_TIMEOUT_MS, NULL);
    if (!resp) return -1;

    json_t *j = json_parse(resp, NULL);
    free(resp);
    if (!j) return -1;

    int ret = (int)json_get_num(j, "ret", 0);
    int errcode = (int)json_get_num(j, "errcode", 0);
    json_free(j);

    if (ret != 0 || errcode != 0) return errcode ? errcode : ret;
    return 0;
}

/* sendMarkdown */
static int send_markdown(const char *to_user_id, const char *text,
                          const char *context_token);
/* Port of Python gateway/platforms/base.py:send_video(). */
/* sendVideo (G20) */
static int send_video(const char *to_user_id, const char *video_url,
                       const char *context_token);
/* Port of Python gateway/platforms/weixin.py:_send_file(). */
/* sendFile (G20) */
static int send_file(const char *to_user_id, const char *file_url,
                      const char *filename, const char *context_token);
static int send_markdown(const char *to_user_id, const char *text,
                          const char *context_token) {
    char payload[MAX_BODY];
    if (context_token && *context_token) {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":2,\"content\":\"%s\","
                 "\"context_token\":\"%s\"",
                 to_user_id, text, context_token);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":2,\"content\":\"%s\"",
                 to_user_id, text);
    }

    char *resp = wx_api_post(EP_SEND_MESSAGE, payload, API_TIMEOUT_MS, NULL);
    if (!resp) return -1;

    json_t *j = json_parse(resp, NULL);
    free(resp);
    if (!j) return -1;

    int ret = (int)json_get_num(j, "ret", 0);
    int errcode = (int)json_get_num(j, "errcode", 0);
    json_free(j);

    if (ret != 0 || errcode != 0) return errcode ? errcode : ret;
    return 0;
}

/* sendImage — msg_type 3 for image messages */
static int send_image_msg(const char *to_user_id, const char *image_data,
                           int image_type, const char *context_token) {
    char payload[MAX_BODY];
    if (context_token && *context_token) {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":3,\"content\":\"%s\","
                 "\"image_type\":%d,"
                 "\"context_token\":\"%s\"",
                 to_user_id, image_data, image_type, context_token);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":3,\"content\":\"%s\","
                 "\"image_type\":%d",
                 to_user_id, image_data, image_type);
    }

    char *resp = wx_api_post(EP_SEND_MESSAGE, payload, API_TIMEOUT_MS, NULL);
    if (!resp) return -1;

    json_t *j = json_parse(resp, NULL);
    free(resp);
    if (!j) return -1;

    int ret = (int)json_get_num(j, "ret", 0);
    int errcode = (int)json_get_num(j, "errcode", 0);
    json_free(j);

    if (ret != 0 || errcode != 0) return errcode ? errcode : ret;
    return 0;
}

/* sendVideo — msg_type 4 (G20) */
static int send_video(const char *to_user_id, const char *video_url,
                       const char *context_token) {
    char payload[MAX_BODY];
    if (context_token && *context_token) {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":4,\"content\":\"%s\","
                 "\"context_token\":\"%s\"",
                 to_user_id, video_url, context_token);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":4,\"content\":\"%s\"",
                 to_user_id, video_url);
    }
    char *resp = wx_api_post(EP_SEND_MESSAGE, payload, API_TIMEOUT_MS, NULL);
    if (!resp) return -1;
    json_t *j = json_parse(resp, NULL);
    free(resp);
    if (!j) return -1;
    int ret = (int)json_get_num(j, "ret", 0);
    int errcode = (int)json_get_num(j, "errcode", 0);
    json_free(j);
    if (ret != 0 || errcode != 0) return errcode ? errcode : ret;
    return 0;
}

/* AG26: Port of Python gateway/platforms/weixin.py:send_file(). */
static int send_file(const char *to_user_id, const char *file_url,
                      const char *filename, const char *context_token) {
    char payload[MAX_BODY];
    const char *fname = filename ? filename : "file";
    if (context_token && *context_token) {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":6,\"content\":\"%s\","
                 "\"file_name\":\"%s\","
                 "\"context_token\":\"%s\"",
                 to_user_id, file_url, fname, context_token);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"from_user_id\":\"\",\"to_user_id\":\"%s\","
                 "\"msg_type\":6,\"content\":\"%s\","
                 "\"file_name\":\"%s\"",
                 to_user_id, file_url, fname);
    }
    char *resp = wx_api_post(EP_SEND_MESSAGE, payload, API_TIMEOUT_MS, NULL);
    if (!resp) return -1;
    json_t *j = json_parse(resp, NULL);
    free(resp);
    if (!j) return -1;
    int ret = (int)json_get_num(j, "ret", 0);
    int errcode = (int)json_get_num(j, "errcode", 0);
    json_free(j);
    if (ret != 0 || errcode != 0) return errcode ? errcode : ret;
    return 0;
}

/* Port of Python gateway/platforms/weixin.py:_process_message(). */
/* ================================================================
 *  Process inbound message
 * ================================================================ */

static void process_message(json_t *msg) {
    const char *from_id = json_get_str(msg, "from_user_id", "");
    const char *room_id = json_get_str(msg, "room_id", "");

    if (!*from_id) return;
    const char *chat_id = *room_id ? room_id : from_id;

    /* Extract context token */
    const char *ctx_token = json_get_str(msg, "context_token", "");
    if (*ctx_token)
        ctx_token_save(from_id, ctx_token);

    /* Extract text from items */
    json_t *items = json_obj_get(msg, "items");
    char text[4096] = "";
    if (items && items->type == JSON_ARRAY) {
        size_t n = json_len(items);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(items, i);
            int type = (int)json_get_num(item, "type", 0);
            if (type == 1) { /* text */
                json_t *text_item = json_obj_get(item, "text_item");
                if (text_item) {
                    const char *t = json_get_str(text_item, "text", "");
                    if (*t) {
                        if (*text) strncat(text, " ", sizeof(text) - strlen(text) - 1);
                        strncat(text, t, sizeof(text) - strlen(text) - 1);
                    }
                }
            }
        }
    }

    if (!*text) return;

    printf("[gateway:weixin] Message from %s: %.200s\n", from_id, text);

    /* Forward to agent */
    extern gateway_state_t g_gw;
    char *resp = agent_chat(&g_gw.agent, text);
    if (resp) {
        const char *ctx = ctx_token_get(from_id);
        send_message(chat_id, resp, ctx);
        free(resp);
    }
}

/* ================================================================
 *  Poll loop
 * ================================================================ */

static void *thread_weixin_poll(void *arg) {
    (void)arg;
    printf("[gateway] Weixin poll loop started\n");

    sync_buf_load();

    while (g_wx.running) {
        char *resp = get_updates();
        if (!resp) {
            g_wx.consecutive_failures++;
            sleep(RETRY_DELAY_SEC);
            continue;
        }

        json_t *root = json_parse(resp, NULL);
        free(resp);

        if (!root) {
            g_wx.consecutive_failures++;
            sleep(RETRY_DELAY_SEC);
            continue;
        }

        int ret = (int)json_get_num(root, "ret", 0);
        int errcode = (int)json_get_num(root, "errcode", 0);

        if (ret != 0 || errcode != 0) {
            /* Session expired (-14) — pause 10 min */
            if (errcode == -14 || ret == -14) {
                printf("[gateway:weixin] Session expired; pausing 10m\n");
                json_free(root);
                for (int i = 0; i < 600 && g_wx.running; i++) sleep(1);
                g_wx.consecutive_failures = 0;
                continue;
            }

            g_wx.consecutive_failures++;
            if (g_wx.consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
                printf("[gateway:weixin] Too many failures; backing off %ds\n",
                       BACKOFF_DELAY_SEC);
                sleep(BACKOFF_DELAY_SEC);
                g_wx.consecutive_failures = 0;
            } else {
                sleep(RETRY_DELAY_SEC);
            }
            json_free(root);
            continue;
        }

        /* Success */
        g_wx.consecutive_failures = 0;

        /* Update sync_buf */
        const char *new_buf = json_get_str(root, "get_updates_buf", "");
        if (*new_buf) {
            snprintf(g_wx.sync_buf, sizeof(g_wx.sync_buf), "%s", new_buf);
            sync_buf_save();
        }

        /* Process messages */
        json_t *msgs = json_obj_get(root, "msgs");
        if (msgs && msgs->type == JSON_ARRAY) {
            size_t n = json_len(msgs);
            for (size_t i = 0; i < n; i++) {
                process_message(json_get(msgs, i));
            }
        }

        json_free(root);
    }

    printf("[gateway:weixin] Poll loop stopped\n");
    return NULL;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool weixin_init(const char *token, const char *account_id) {
    memset(&g_wx, 0, sizeof(g_wx));
    if (token) snprintf(g_wx.token, sizeof(g_wx.token), "%s", token);
    if (account_id) snprintf(g_wx.account_id, sizeof(g_wx.account_id), "%s", account_id);
    snprintf(g_wx.base_url, sizeof(g_wx.base_url), "%s", ILINK_BASE_URL);
    return *g_wx.token && *g_wx.account_id;
}

void weixin_start(void) {
    if (!*g_wx.token || !*g_wx.account_id) {
        fprintf(stderr, "[gateway:weixin] Token and account_id required\n");
        return;
    }
    g_wx.running = true;
    ensure_weixin_dir();
    printf("[gateway] Weixin platform started (account=%s)\n", g_wx.account_id);
    thread_weixin_poll(NULL);
}

void weixin_stop(void) {
    g_wx.running = false;
}

/* ================================================================
 *  P113: Public send APIs
 * ================================================================ */

void weixin_send_text(const char *chat_id, const char *text,
                       const char *context_token) {
    if (!chat_id || !text) return;
    send_message(chat_id, text, context_token);
}

void weixin_send_markdown(const char *chat_id, const char *text,
                           const char *context_token) {
    if (!chat_id || !text) return;
    send_markdown(chat_id, text, context_token);
}

/* Port of Python gateway/platforms/weixin.py:send_image() */
void weixin_send_image(const char *chat_id, const char *image_data,
                        int image_type, const char *context_token) {
    if (!chat_id || !image_data) return;
    send_image_msg(chat_id, image_data, image_type, context_token);
}

void weixin_send_video(const char *chat_id, const char *video_url,
                        const char *context_token) {
    if (!chat_id || !video_url) return;
    send_video(chat_id, video_url, context_token);
}

void weixin_send_file(const char *chat_id, const char *file_url,
                       const char *filename, const char *context_token) {
    if (!chat_id || !file_url) return;
    send_file(chat_id, file_url, filename, context_token);
}

/* ===========================================================================
 *  WeChat crypto helpers — ported from gateway/platforms/weixin.py
 *  Real AES-128-ECB (PKCS7) via OpenSSL EVP. These were REAL_GAP.
 * =========================================================================== */

/* PoP: _pkcs7_pad @ gateway/platforms/weixin.py:_pkcs7_pad */
/* Caller frees the returned buffer. Returns NULL on alloc failure. */
/* PoP: weixin_pkcs7_pad @ gateway/platforms/weixin.py:_pkcs7_pad */
unsigned char *weixin_pkcs7_pad(const unsigned char *data, size_t len, size_t *out_len)
{
    size_t pad = 16 - (len % 16);
    size_t total = len + pad;
    unsigned char *buf = (unsigned char *)malloc(total ? total : 1);
    if (!buf) return NULL;
    if (len) memcpy(buf, data, len);
    memset(buf + len, (int)pad, pad);
    *out_len = total;
    return buf;
}

/* PoP: _aes_padded_size @ gateway/platforms/weixin.py:_aes_padded_size */
size_t weixin_aes_padded_size(size_t size)
{
    return ((size + 1 + 15) / 16) * 16;
}

/* PoP: _aes128_ecb_encrypt @ gateway/platforms/weixin.py:_aes128_ecb_encrypt */
/* Encrypts PKCS7-padded plaintext with AES-128-ECB. Caller frees *out. */
/* PoP: weixin_aes128_ecb_encrypt @ gateway/platforms/weixin.py:_aes128_ecb_encrypt */
int weixin_aes128_ecb_encrypt(const unsigned char *plaintext, size_t pt_len,
                              const unsigned char *key, size_t key_len,
                              unsigned char **out, size_t *out_len)
{
    if (!plaintext || !key || key_len != 16 || !out) return -1;
    size_t pad_len = 0;
    unsigned char *padded = weixin_pkcs7_pad(plaintext, pt_len, &pad_len);
    if (!padded) return -1;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { free(padded); return -1; }
    unsigned char *ct = (unsigned char *)malloc(pad_len + 16);
    if (!ct) { EVP_CIPHER_CTX_free(ctx); free(padded); return -1; }
    int len1 = 0, len2 = 0;
    EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);
    EVP_CIPHER_CTX_set_padding(ctx, 0); /* manual PKCS7 */
    EVP_EncryptUpdate(ctx, ct, &len1, padded, (int)pad_len);
    EVP_EncryptFinal_ex(ctx, ct + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);
    free(padded);

    *out = ct;
    *out_len = (size_t)(len1 + len2);
    return 0;
}

/* PoP: _aes128_ecb_decrypt @ gateway/platforms/weixin.py:_aes128_ecb_decrypt */
/* Decrypts AES-128-ECB and strips PKCS7 padding. Caller frees *out. */
/* PoP: weixin_aes128_ecb_decrypt @ gateway/platforms/weixin.py:_aes128_ecb_decrypt */
int weixin_aes128_ecb_decrypt(const unsigned char *ciphertext, size_t ct_len,
                              const unsigned char *key, size_t key_len,
                              unsigned char **out, size_t *out_len)
{
    if (!ciphertext || !key || key_len != 16 || !out) return -1;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;
    unsigned char *pt = (unsigned char *)malloc(ct_len + 16);
    if (!pt) { EVP_CIPHER_CTX_free(ctx); return -1; }
    int len1 = 0, len2 = 0;
    EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);
    EVP_CIPHER_CTX_set_padding(ctx, 0); /* manual PKCS7 */
    EVP_DecryptUpdate(ctx, pt, &len1, ciphertext, (int)ct_len);
    EVP_DecryptFinal_ex(ctx, pt + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);

    size_t total = (size_t)(len1 + len2);
    if (total > 0) {
        unsigned char pad = pt[total - 1];
        if (pad >= 1 && pad <= 16) {
            int ok = 1;
            for (size_t i = total - pad; i < total; i++) if (pt[i] != pad) ok = 0;
            if (ok) total -= pad;
        }
    }
    *out = pt;
    *out_len = total;
    return 0;
}

/* PoP: _safe_id @ gateway/platforms/weixin.py:_safe_id */
/* Strip non-printable / non-identifier chars for safe logging. */
/* PoP: weixin_safe_id @ gateway/platforms/weixin.py:_safe_id */
void weixin_safe_id(const char *in, char *out, size_t out_sz)
{
    size_t o = 0;
    if (in) for (size_t i = 0; in[i] && o + 1 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '_' || c == '-' || c == '@' || c == '.' ||
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            out[o++] = (char)c;
    }
    out[o] = '\0';
}

/* PoP: _random_wechat_uin @ gateway/platforms/weixin.py:_random_wechat_uin */
/* Generate a random 32-bit big-endian value, base64-encoded as ASCII. */
/* PoP: weixin_random_wechat_uin @ gateway/platforms/weixin.py:_random_wechat_uin */
void weixin_random_wechat_uin(char *out, size_t out_sz)
{
    unsigned char b[4];
    if (RAND_bytes(b, 4) != 1) { if (out_sz) out[0]='\0'; return; }
    unsigned long v = ((unsigned long)b[0] << 24) | ((unsigned long)b[1] << 16) |
                      ((unsigned long)b[2] << 8) | b[3];
    char s[24];
    snprintf(s, sizeof(s), "%lu", v);
    char *enc = base64_encode((const unsigned char *)s, strlen(s));
    if (enc) { snprintf(out, out_sz, "%s", enc); free(enc); }
    else if (out_sz) out[0] = '\0';
}

/* ── Simple utility ports ───────────────────────────────────────────── */

/* PoP: _check_weixin_requirements @ gateway/platforms/weixin.py:check_weixin_requirements */
/* PoP: weixin_check_requirements @ gateway/platforms/weixin.py:check_weixin_requirements */
bool weixin_check_requirements(void)
{
    return true; /* C port: always available in compiled binary */
}

/* PoP: _json_dumps @ gateway/platforms/weixin.py:_json_dumps */
/* PoP: weixin_json_dumps @ gateway/platforms/weixin.py:_json_dumps */
char *weixin_json_dumps(const json_t *payload)
{
    if (!payload) return strdup("{}");
    return json_serialize(payload);
}

/* PoP: _base_info @ gateway/platforms/weixin.py:_base_info */
/* PoP: weixin_base_info @ gateway/platforms/weixin.py:_base_info */
json_t *weixin_base_info(void)
{
    json_t *obj = json_object();
    if (!obj) return NULL;
    json_object_set(obj, "channel_version", json_string("2.0"));
    return obj;
}

/* PoP: _account_dir @ gateway/platforms/weixin.py:_account_dir */
/* PoP: weixin_account_dir @ gateway/platforms/weixin.py:_account_dir */
char *weixin_account_dir(const char *hermes_home)
{
    if (!hermes_home) return NULL;
    size_t sz = strlen(hermes_home) + 32;
    char *path = (char *)malloc(sz);
    if (!path) return NULL;
    snprintf(path, sz, "%s/weixin/accounts", hermes_home);
    mkdir(path, 0700);
    return path;
}

/* PoP: _account_file @ gateway/platforms/weixin.py:_account_file */
/* PoP: weixin_account_file @ gateway/platforms/weixin.py:_account_file */
char *weixin_account_file(const char *hermes_home, const char *account_id)
{
    if (!hermes_home || !account_id) return NULL;
    size_t sz = strlen(hermes_home) + strlen(account_id) + 64;
    char *path = (char *)malloc(sz);
    if (!path) return NULL;
    char *dir = weixin_account_dir(hermes_home);
    if (!dir) { free(path); return NULL; }
    snprintf(path, sz, "%s/%s.json", dir, account_id);
    free(dir);
    return path;
}

/* PoP: _cdn_download_url @ gateway/platforms/weixin.py:_cdn_download_url */
/* PoP: weixin_cdn_download_url @ gateway/platforms/weixin.py:_cdn_download_url */
char *weixin_cdn_download_url(const char *cdn_base_url, const char *encrypted_query_param)
{
    if (!cdn_base_url || !encrypted_query_param) return NULL;
    /* Strip trailing slash from base */
    size_t blen = strlen(cdn_base_url);
    while (blen > 0 && cdn_base_url[blen-1] == '/') blen--;
    /* URL-encode the param (simplified: just pass through for ASCII) */
    size_t sz = blen + strlen(encrypted_query_param) + 64;
    char *url = (char *)malloc(sz);
    if (!url) return NULL;
    snprintf(url, sz, "%.*s/download?encrypted_query_param=%s",
             (int)blen, cdn_base_url, encrypted_query_param);
    return url;
}

/* PoP: _cdn_upload_url @ gateway/platforms/weixin.py:_cdn_upload_url */
/* PoP: weixin_cdn_upload_url @ gateway/platforms/weixin.py:_cdn_upload_url */
char *weixin_cdn_upload_url(const char *cdn_base_url, const char *upload_param, const char *filekey)
{
    if (!cdn_base_url || !upload_param || !filekey) return NULL;
    size_t blen = strlen(cdn_base_url);
    while (blen > 0 && cdn_base_url[blen-1] == '/') blen--;
    size_t sz = blen + strlen(upload_param) + strlen(filekey) + 64;
    char *url = (char *)malloc(sz);
    if (!url) return NULL;
    snprintf(url, sz, "%.*s/upload?upload_param=%s&filekey=%s",
             (int)blen, cdn_base_url, upload_param, filekey);
    return url;
}

/* PoP: @gateway/platforms/weixin.py:_mime_from_filename */
/* PoP: weixin_mime_from_filename @ gateway/platforms/weixin.py:_mime_from_filename */
const char *weixin_mime_from_filename(const char *filename)
{
    if (!filename) return "application/octet-stream";
    const char *dot = strrchr(filename, '.');
    if (!dot) return "application/octet-stream";
    /* Common MIME types */
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".png") == 0) return "image/png";
    if (strcasecmp(dot, ".gif") == 0) return "image/gif";
    if (strcasecmp(dot, ".webp") == 0) return "image/webp";
    if (strcasecmp(dot, ".mp3") == 0) return "audio/mpeg";
    if (strcasecmp(dot, ".wav") == 0) return "audio/wav";
    if (strcasecmp(dot, ".ogg") == 0) return "audio/ogg";
    if (strcasecmp(dot, ".m4a") == 0) return "audio/mp4";
    if (strcasecmp(dot, ".mp4") == 0) return "video/mp4";
    if (strcasecmp(dot, ".pdf") == 0) return "application/pdf";
    if (strcasecmp(dot, ".zip") == 0) return "application/zip";
    if (strcasecmp(dot, ".json") == 0) return "application/json";
    if (strcasecmp(dot, ".txt") == 0) return "text/plain";
    if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0) return "text/html";
    return "application/octet-stream";
}

/* PoP: _split_table_row @ gateway/platforms/weixin.py:_split_table_row */
/* PoP: weixin_split_table_row @ gateway/platforms/weixin.py:_split_table_row */
char **weixin_split_table_row(const char *line, int *count)
{
    if (count) *count = 0;
    if (!line) return NULL;
    /* Split on pipe '|' character, trim each field */
    char *work = strdup(line);
    if (!work) return NULL;
    int cap = 16, n = 0;
    char **result = (char **)malloc((size_t)cap * sizeof(char *));
    if (!result) { free(work); return NULL; }
    char *tok = strtok(work, "|");
    while (tok) {
        /* Trim */
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && end[-1] == ' ') end--;
        *end = '\0';
        if (n >= cap) {
            cap *= 2;
            char **tmp = (char **)realloc(result, (size_t)cap * sizeof(char *));
            if (!tmp) { free(work); return NULL; }
            result = tmp;
        }
        result[n++] = strdup(tok);
        tok = strtok(NULL, "|");
    }
    free(work);
    if (count) *count = n;
    return result;
}

/* PoP: _looks_like_heading_line_for_weixin @ gateway/platforms/weixin.py:_looks_like_heading_line_for_weixin */
/* PoP: weixin_looks_like_heading_line @ gateway/platforms/weixin.py:_looks_like_heading_line_for_weixin */
bool weixin_looks_like_heading_line(const char *line)
{
    if (!line || !*line) return false;
    /* Starts with # or ## or ### ... */
    const char *p = line;
    while (*p == ' ') p++;
    if (*p == '#') {
        p++;
        while (*p == '#') p++;
        return (*p == ' ' || *p == '\0');
    }
    /* Underline-style heading: line of === or --- */
    if (*p == '=' || *p == '-') {
        int count = 0;
        while (*p == '=' || *p == '-') { count++; p++; }
        while (*p == ' ') p++;
        return count >= 3 && *p == '\0';
    }
    return false;
}

/* PoP: _looks_like_chatty_line_for_weixin @ gateway/platforms/weixin.py:_looks_like_chatty_line_for_weixin */
/* PoP: weixin_looks_like_chatty_line @ gateway/platforms/weixin.py:_looks_like_chatty_line_for_weixin */
bool weixin_looks_like_chatty_line(const char *line)
{
    if (!line || !*line) return false;
    const char *p = line;
    while (*p == ' ') p++;
    /* Blockquote or list marker */
    if (*p == '>' || *p == '-' || *p == '*' || *p == '+') return true;
    /* Numbered list */
    if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) p++;
        while (*p == ' ') p++;
        if (*p == '.') return true;
    }
    /* Inline code backtick */
    if (*p == '`') return true;
    return false;
}

/* PoP: _coerce_float_extra @ gateway/platforms/weixin.py:_coerce_float_extra */
/* PoP: weixin_coerce_float_extra @ gateway/platforms/weixin.py:_coerce_float_extra */
double weixin_coerce_float_extra(const json_t *obj, const char *key, double default_val)
{
    if (!obj || !key) return default_val;
    return json_get_num(obj, key, default_val);
}

/* PoP: _is_dm_allowed @ gateway/platforms/weixin.py:_is_dm_allowed */
/* PoP: weixin_is_dm_allowed @ gateway/platforms/weixin.py:_is_dm_allowed */
bool weixin_is_dm_allowed(const char *sender_id, bool open_dm_opted_in,
                           const char *allowlist_json, const char *blocklist_json)
{
    if (!sender_id) return false;
    if (open_dm_opted_in) return true;
    /* Check allowlist */
    if (allowlist_json) {
        json_t *root = json_parse(allowlist_json, NULL);
        if (root && root->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(root); i++) {
                const json_t *e = json_array_get(root, i);
                if (e && e->type == JSON_STRING && strcmp(e->str_val, sender_id) == 0) {
                    json_free(root);
                    return true;
                }
            }
        }
        if (root) json_free(root);
    }
    /* Check blocklist - if present and matches, deny */
    if (blocklist_json) {
        json_t *root = json_parse(blocklist_json, NULL);
        if (root && root->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(root); i++) {
                const json_t *e = json_array_get(root, i);
                if (e && e->type == JSON_STRING && strcmp(e->str_val, sender_id) == 0) {
                    json_free(root);
                    return false;
                }
            }
        }
        if (root) json_free(root);
    }
    return false;
}

/* PoP: _assert_weixin_cdn_url @ gateway/platforms/weixin.py:_assert_weixin_cdn_url */
/* PoP: weixin_assert_cdn_url @ gateway/platforms/weixin.py:_assert_weixin_cdn_url */
bool weixin_assert_cdn_url(const char *url)
{
    if (!url) return false;
    /* Must start with https:// */
    if (strncmp(url, "https://", 8) != 0) return false;
    /* Must contain weixin.qq.com or similar CDN domain patterns */
    if (strstr(url, "weixin.qq.com") || strstr(url, "wx.qq.com") ||
        strstr(url, "wechat.com") || strstr(url, "qq.com"))
        return true;
    return false;
}

/* PoP: _rate_limit_error @ gateway/platforms/weixin.py:_rate_limit_error */
/* PoP: weixin_rate_limit_error @ gateway/platforms/weixin.py:_rate_limit_error */
char *weixin_rate_limit_error(double cooldown_remaining)
{
    char *msg = (char *)malloc(128);
    if (!msg) return NULL;
    snprintf(msg, 128,
             "iLink sendmessage rate limited; cooldown active for %.1fs",
             cooldown_remaining);
    return msg;
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

/* PoP: _headers @ gateway/platforms/weixin.py:_headers */
/* PoP: weixin_headers @ gateway/platforms/weixin.py:_headers */
json_t *weixin_headers(const char *token, const char *body)
{
    json_t *h = json_object();
    if (!h) return NULL;
    json_object_set(h, "Content-Type", json_string("application/json; charset=utf-8"));
    if (token) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Bearer %s", token);
        json_object_set(h, "Authorization", json_string(buf));
    }
    if (body) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%zu", body ? strlen(body) : 0);
        json_object_set(h, "Content-Length", json_string(buf));
    }
    return h;
}

/* PoP: _coerce_list @ gateway/platforms/weixin.py:_coerce_list */
/* PoP: weixin_coerce_list @ gateway/platforms/weixin.py:_coerce_list */
char **weixin_coerce_list(const json_t *value, int *count)
{
    if (count) *count = 0;
    if (!value) return NULL;
    if (value->type == JSON_ARRAY) {
        size_t n = json_array_size(value);
        char **arr = (char **)calloc(n + 1, sizeof(char *));
        if (!arr) return NULL;
        size_t j = 0;
        for (size_t i = 0; i < n; i++) {
            const json_t *e = json_array_get(value, i);
            if (e && e->type == JSON_STRING) {
                arr[j++] = strdup(e->str_val);
            }
        }
        if (count) *count = (int)j;
        return arr;
    }
    if (value->type == JSON_STRING) {
        char **arr = (char **)malloc(2 * sizeof(char *));
        if (!arr) return NULL;
        arr[0] = strdup(value->str_val);
        arr[1] = NULL;
        if (count) *count = 1;
        return arr;
    }
    return NULL;
}

/* PoP: _rate_limit_cooldown_remaining @ gateway/platforms/weixin.py:_rate_limit_cooldown_remaining */
/* PoP: weixin_rate_limit_cooldown_remaining @ gateway/platforms/weixin.py:_rate_limit_cooldown_remaining */
double weixin_rate_limit_cooldown_remaining(double circuit_until)
{
    double now = (double)time(NULL);
    double rem = circuit_until - now;
    return rem > 0.0 ? rem : 0.0;
}

/* PoP: _open_dm_opted_in @ gateway/platforms/weixin.py:_open_dm_opted_in */
/* PoP: weixin_open_dm_opted_in @ gateway/platforms/weixin.py:_open_dm_opted_in */
bool weixin_open_dm_opted_in(void)
{
    const char *allow_all = getenv("GATEWAY_ALLOW_ALL_USERS");
    if (allow_all && (strcmp(allow_all, "true") == 0 || strcmp(allow_all, "1") == 0 || strcmp(allow_all, "yes") == 0))
        return true;
    const char *wx_allow_all = getenv("WEIXIN_ALLOW_ALL_USERS");
    if (wx_allow_all && (strcmp(wx_allow_all, "true") == 0 || strcmp(wx_allow_all, "1") == 0 || strcmp(wx_allow_all, "yes") == 0))
        return true;
    return false;
}

