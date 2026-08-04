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

/* secret_scope_* helpers live in src/agent/port_agent_secret_scope.c
 * (port of agent/secret_scope.py). They are exported there but not yet
 * gathered into a public header; declare what we need here. */
extern json_t *secret_scope_current_secret_scope(void);
extern bool secret_scope_is_multiplex_active(void);
extern bool secret_scope_is_global_env_fn(const char *name);

/* PoP: _wx_secret @ gateway/platforms/weixin.py:_wx_secret */
/* Scope-aware WEIXIN_* read with the default-profile startup fallback.
 * Mirrors Python: try get_secret(name, default); on UnscopedSecretError
 * (multiplex active, no profile scope installed) fall back to
 * os.getenv(name, default). */
const char *wx_secret(const char *name, const char *default_val)
{
    if (!name) return default_val;

    /* 1. Genuinely-global vars always read os.environ */
    if (secret_scope_is_global_env_fn(name)) {
        const char *val = getenv(name);
        return val ? val : default_val;
    }

    /* 2. Secret scope installed (multiplexed turn): scope is authoritative */
    json_t *scope = secret_scope_current_secret_scope();
    if (scope && scope->type == JSON_OBJECT) {
        json_t *val_node = json_object_get(scope, name);
        if (val_node && val_node->type == JSON_STRING) {
            return json_node_get_string(val_node);
        }
        /* Absent key: under multiplexing return default (no cross-profile
         * borrow). Multiplex off: scope is an overlay; fall through. */
        if (secret_scope_is_multiplex_active()) return default_val;
        const char *val = getenv(name);
        return val ? val : default_val;
    }

    /* 3. No scope installed: Python raises UnscopedSecretError when
     * multiplexing is on, and _wx_secret catches it and falls back to
     * os.getenv(name, default). Multiplex off reads os.environ directly. */
    const char *val = getenv(name);
    return val ? val : default_val;
}

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
/* PoP: _api_post @ gateway/platforms/weixin.py:_api_post */
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

/* PoP: ctx_token_path @ hermes_cli/windows_ssh_runtime.py:_token_path */
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
/* PoP: set @ gateway/platforms/weixin.py:set */
    json_set(data, user_id, json_string(token));

    char *json_str = json_serialize(data);
    if (json_str) {
        FILE *out = fopen(path, "wb");
        if (out) { fwrite(json_str, 1, strlen(json_str), out); fclose(out); }
        free(json_str);
    }
    json_free(data);
}
/* PoP: get @ gateway/platforms/weixin.py:get */

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

/* PoP: api_post @ gateway/platforms/weixin.py:api_post */
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
/* PoP: send_video @ gateway/platforms/weixin.py:send_video */
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

/* PoP: send_image @ gateway/platforms/weixin.py:send_image */
/* Port of Python gateway/platforms/weixin.py:send_image(). */
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

/* Local helper: join an array of lines with '\n', strip leading/trailing
 * whitespace, and return a malloc'd string (caller frees). */
static char *weixin__join_strip(char **lines, int n)
{
    size_t total = 1; /* NUL */
    for (int i = 0; i < n; i++) total += strlen(lines[i]) + 1; /* + '\n' */
    char *buf = malloc(total);
    if (!buf) return NULL;
    buf[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i) strcat(buf, "\n");
        strcat(buf, lines[i]);
    }
    /* strip trailing whitespace */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t' ||
                       buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';
    /* strip leading whitespace */
    const char *p = buf;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (p != buf) memmove(buf, p, strlen(p) + 1);
    return buf;
}
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

/* ═══════════════════════════════════════════════════════════════════════
 *  Pure helpers ported from gateway/platforms/weixin.py
 *  (faithful, no async/HTTP — these are synchronous utilities)
 * ═══════════════════════════════════════════════════════════════════════ */

#define WEIXIN_RATE_LIMIT_ERRCODE (-2)

/* PoP: _is_stale_session_ret @ gateway/platforms/weixin.py:_is_stale_session_ret */
/* True when iLink returns ret=-2 / errcode=-2 with 'unknown error' — stale-session signal. */
bool weixin_is_stale_session_ret(int ret, int errcode, const char *errmsg)
{
    if (ret != WEIXIN_RATE_LIMIT_ERRCODE && errcode != WEIXIN_RATE_LIMIT_ERRCODE)
        return false;
    return errmsg != NULL && strcasecmp(errmsg, "unknown error") == 0;
}

/* PoP: _parse_aes_key @ gateway/platforms/weixin.py:_parse_aes_key */
/* Decode a base64 AES key; accept 16 raw bytes or a 32-char hex string. */
/* PoP: _key @ gateway/platforms/weixin.py:_key */
int weixin_parse_aes_key(const char *aes_key_b64, unsigned char *out, size_t *out_len)
{
    if (!aes_key_b64 || !out || !out_len) return -1;
    size_t dlen = 0;
    unsigned char *dec = base64_decode(aes_key_b64, &dlen);
    if (!dec) return -1;
    if (dlen == 16) {
        memcpy(out, dec, 16);
        *out_len = 16;
        free(dec);
        return 0;
    }
    if (dlen == 32) {
        /* If it decodes to a pure hex string, interpret as hex bytes. */
        bool all_hex = true;
        for (size_t i = 0; i < 32; i++) {
            char c = (char)dec[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                all_hex = false; break;
            }
        }
        if (all_hex) {
            for (size_t i = 0; i < 16; i++) {
                unsigned int b = 0;
                sscanf((char *)dec + i * 2, "%2x", &b);
                out[i] = (unsigned char)b;
            }
            *out_len = 16;
            free(dec);
            return 0;
        }
    }
    free(dec);
    return -1; /* unexpected aes_key format */
}

/* PoP: _guess_chat_type @ gateway/platforms/weixin.py:_guess_chat_type */
/* Derive (chat_type, peer_id) from a raw inbound message dict. */
void weixin_guess_chat_type(json_t *message, const char *account_id,
                             char *out_type, size_t type_cap,
                             char *out_peer, size_t peer_cap)
{
    const char *room = json_get_str(message, "room_id", "");
    if (!room || !*room) room = json_get_str(message, "chat_room_id", "");
    const char *to = json_get_str(message, "to_user_id", "");
    const char *from = json_get_str(message, "from_user_id", "");
    int msg_type = (int)json_get_num(message, "msg_type", 0);

    bool is_group = (room && *room) ||
                    (to && account_id && strcmp(to, account_id) != 0 && msg_type == 1);
    if (is_group) {
        snprintf(out_type, type_cap, "group");
        const char *peer = room && *room ? room : (to && *to ? to : (from ? from : ""));
        snprintf(out_peer, peer_cap, "%s", peer);
    } else {
        snprintf(out_type, type_cap, "dm");
        snprintf(out_peer, peer_cap, "%s", from ? from : "");
    }
}

/* PoP: _message_type_from_media @ gateway/platforms/weixin.py:_message_type_from_media */
typedef enum {
    WEIXIN_MSG_TEXT = 0,
    WEIXIN_MSG_PHOTO,
    WEIXIN_MSG_VIDEO,
    WEIXIN_MSG_VOICE,
    WEIXIN_MSG_DOCUMENT,
    WEIXIN_MSG_COMMAND,
} weixin_msg_type_t;

weixin_msg_type_t weixin_message_type_from_media(const char **media_types, int n_media, const char *text)
{
    for (int i = 0; i < n_media; i++) {
        if (media_types[i] && strncmp(media_types[i], "image/", 6) == 0) return WEIXIN_MSG_PHOTO;
    }
    for (int i = 0; i < n_media; i++) {
        if (media_types[i] && strncmp(media_types[i], "video/", 6) == 0) return WEIXIN_MSG_VIDEO;
    }
    for (int i = 0; i < n_media; i++) {
        if (media_types[i] && strncmp(media_types[i], "audio/", 6) == 0) return WEIXIN_MSG_VOICE;
    }
    if (n_media > 0) return WEIXIN_MSG_DOCUMENT;
    if (text && text[0] == '/') return WEIXIN_MSG_COMMAND;
    return WEIXIN_MSG_TEXT;
}

/* PoP: _split_markdown_blocks @ gateway/platforms/weixin.py:_split_markdown_blocks */
/* Split content into blocks at fenced code regions and blank lines, keeping fenced blocks intact. */
char **weixin_split_markdown_blocks(const char *content, int *out_count)
{
    if (out_count) *out_count = 0;
    if (!content || !*content) return NULL;

    /* Count lines first. */
    int nlines = 1;
    for (const char *p = content; *p; p++) if (*p == '\n') nlines++;
    char **lines = malloc(sizeof(char *) * (nlines + 1));
    int nl = 0;
    const char *start = content;
    for (const char *p = content; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = p - start;
            char *line = malloc(len + 1);
            memcpy(line, start, len);
            line[len] = '\0';
            /* strip trailing CR */
            while (len > 0 && (line[len-1] == '\r')) line[--len] = '\0';
            lines[nl++] = line;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    lines[nl] = NULL;

    char **blocks = malloc(sizeof(char *) * (nl + 1));
    int nb = 0;
    char **current = malloc(sizeof(char *) * (nl + 1));
    int nc = 0;
    bool in_code = false;

    for (int i = 0; i < nl; i++) {
        const char *ls = lines[i];
        while (*ls == ' ' || *ls == '\t') ls++;
        bool is_fence = (strncmp(ls, "```", 3) == 0);

        if (is_fence) {
            if (!in_code && nc > 0) {
                blocks[nb++] = weixin__join_strip(current, nc);
                nc = 0;
            }
            current[nc++] = strdup(lines[i]);
            in_code = !in_code;
            if (!in_code) {
                blocks[nb++] = weixin__join_strip(current, nc);
                nc = 0;
            }
            continue;
        }
        if (in_code) {
            current[nc++] = strdup(lines[i]);
            continue;
        }
        if (lines[i][0] == '\0') {
            if (nc > 0) {
                blocks[nb++] = weixin__join_strip(current, nc);
                nc = 0;
            }
            continue;
        }
        current[nc++] = strdup(lines[i]);
    }
    if (nc > 0) blocks[nb++] = weixin__join_strip(current, nc);
    blocks[nb] = NULL;

    /* filter empties */
    int out_n = 0;
    for (int i = 0; i < nb; i++) {
        if (blocks[i] && blocks[i][0]) blocks[out_n++] = blocks[i];
        else free(blocks[i]);
    }
    blocks[out_n] = NULL;

    for (int i = 0; i < nl; i++) free(lines[i]);
    free(lines);
    free(current);
    if (out_count) *out_count = out_n;
    return blocks;
}

/* ================================================================
 *  Markdown / delivery helpers (Port of gateway/platforms/weixin.py)
 *  Pure, headless string transforms used by the Weixin delivery path.
 * ================================================================ */

/* Free a NULL-terminated char** returned by the split helpers below. */
static void weixin_free_strv(char **v) {
    if (!v) return;
    for (int i = 0; v[i]; i++) free(v[i]);
    free(v);
}

/* Return true when `line` (any leading whitespace stripped) is a fenced-code
 * opener/closer (``` or ````). Mirrors weixin._FENCE_RE. */
static bool weixin__is_fence(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    return strncmp(line, "```", 3) == 0;
}

/* Mirrors weixin._TABLE_RULE_RE: a row of dashes/colons/pipes (>=3 dashes, has '|'). */
static bool weixin__is_table_rule(const char *s) {
    bool has_dash = false, has_pipe = false;
    for (const char *p = s; *p; p++) {
        char c = *p;
        if (c == ' ' || c == '\t') continue;
        if (c == '-') has_dash = true;
        else if (c == '|') has_pipe = true;
        else if (c == ':') continue;
        else return false;
    }
    return has_dash && has_pipe;
}

/* Mirrors weixin._HEADER_RE: 1-6 '#' then space then content. */
static bool weixin__is_header_line(const char *s) {
    int n = 0;
    while (s[n] == '#') n++;
    if (n < 1 || n > 6) return false;
    return s[n] == ' ' || s[n] == '\t';
}

/* PoP: _media_reference @ gateway/platforms/weixin.py:_media_reference */
/* Return the "media" sub-dict of item[key], or NULL. Faithful to Python
 * (item.get(key) or {}).get("media") or {}. */
json_t *weixin_media_reference(json_t *item, const char *key) {
    if (!item || !key) return NULL;
    json_t *sub = json_object_get(item, key);
    if (!sub || json_is_null(sub)) sub = json_new_object();
    return json_object_get(sub, "media");
}

/* PoP: _normalize_markdown_blocks @ gateway/platforms/weixin.py:_normalize_markdown_blocks */
/* Collapse runs of >1 blank line to a single blank line; keep fenced blocks
 * intact; strip leading/trailing whitespace of the whole content. */
char *weixin_normalize_markdown_blocks(const char *content) {
    if (!content) return NULL;
    char **lines = NULL; int nl = 0;
    const char *start = content;
    for (const char *p = content; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = p - start;
            char *line = malloc(len + 1);
            memcpy(line, start, len); line[len] = '\0';
            while (len > 0 && (line[len-1] == '\r' || line[len-1] == ' ' || line[len-1] == '\t')) line[--len] = '\0';
            lines = realloc(lines, sizeof(char *) * (nl + 2));
            lines[nl++] = line;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    char **out = malloc(sizeof(char *) * (nl + 1));
    int no = 0, blank_run = 0; bool in_code = false;
    for (int i = 0; i < nl; i++) {
        const char *ls = lines[i];
        while (*ls == ' ' || *ls == '\t') ls++;
        if (weixin__is_fence(ls)) {
            in_code = !in_code;
            out[no++] = strdup(lines[i]);
            blank_run = 0;
            continue;
        }
        if (in_code) { out[no++] = strdup(lines[i]); continue; }
        if (lines[i][0] == '\0') {
            blank_run++;
            if (blank_run <= 1) out[no++] = strdup("");
            continue;
        }
        blank_run = 0;
        out[no++] = strdup(lines[i]);
    }
    out[no] = NULL;
    /* join with '\n', strip trailing newlines, then leading */
    size_t cap = 1;
    for (int i = 0; i < no; i++) cap += strlen(out[i]) + 1;
    char *res = malloc(cap);
    res[0] = '\0';
    for (int i = 0; i < no; i++) {
        if (i) strncat(res, "\n", cap - strlen(res) - 1);
        strncat(res, out[i], cap - strlen(res) - 1);
    }
    /* trim trailing newlines */
    size_t L = strlen(res);
    while (L > 0 && res[L-1] == '\n') res[--L] = '\0';
    for (int i = 0; i < no; i++) free(out[i]);
    free(out); free(lines);
    return res;
}

/* PoP: _looks_like_chatty_line_for_weixin @ gateway/platforms/weixin.py:_looks_like_chatty_line_for_weixin */
static bool weixin_looks_like_chatty_line_for_weixin(const char *line) {
    const char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return false;
    size_t len = strlen(s);
    if (len > 48) return false;
    if (s[0] == ' ' || s[0] == '\t') return false;
    if (s[0] == '>' || s[0] == '-' || s[0] == '*' || s[0] == '【' || s[0] == '#' || s[0] == '|') return false;
    if (weixin__is_table_rule(s)) return false;
    /* ^\*\*[^*]+\*\*$  (bold-only line) */
    if (len >= 4 && s[0] == '*' && s[1] == '*' && s[len-1] == '*' && s[len-2] == '*') {
        bool inner_ok = true;
        for (size_t i = 2; i + 2 < len; i++) if (s[i] == '*') { inner_ok = false; break; }
        if (inner_ok) return false;
    }
    /* ^\d+\.  */
    size_t i = 0;
    while (isdigit((unsigned char)s[i])) i++;
    if (i > 0 && s[i] == '.') return false;
    return true;
}

/* PoP: _looks_like_heading_line_for_weixin @ gateway/platforms/weixin.py:_looks_like_heading_line_for_weixin */
static bool weixin_looks_like_heading_line_for_weixin(const char *line) {
    const char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return false;
    if (weixin__is_header_line(s)) return true;
    size_t len = strlen(s);
    if (len > 24) return false;
    size_t L = len;
    return (L > 0 && (s[L-1] == ':' || s[L-1] == '：'));
}

/* PoP: _should_split_short_chat_block_for_weixin @ gateway/platforms/weixin.py:_should_split_short_chat_block_for_weixin */
static bool weixin_should_split_short_chat_block_for_weixin(const char *block) {
    char **lines = NULL; int nl = 0;
    const char *start = block;
    for (const char *p = block; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = p - start;
            char *line = malloc(len + 1);
            memcpy(line, start, len); line[len] = '\0';
            lines = realloc(lines, sizeof(char *) * (nl + 2));
            lines[nl++] = line;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    int nonblank = 0;
    for (int i = 0; i < nl; i++) if (lines[i][0]) nonblank++;
    bool result = false;
    if (nonblank >= 2 && nonblank <= 6) {
        if (weixin_looks_like_heading_line_for_weixin(lines[0])) {
            result = false;
        } else {
            result = true;
            for (int i = 0; i < nl; i++) {
                if (!lines[i][0]) continue;
                if (!weixin_looks_like_chatty_line_for_weixin(lines[i])) { result = false; break; }
            }
        }
    }
    for (int i = 0; i < nl; i++) free(lines[i]);
    free(lines);
    return result;
}

#define WEIXIN_COPY_LINE_WIDTH 120

/* PoP: _wrap_copy_friendly_lines_for_weixin @ gateway/platforms/weixin.py:_wrap_copy_friendly_lines_for_weixin */
/* Greedy word-wrap long display lines (break_long_words=false): a word longer
 * than the width is kept intact on its own line. Fenced blocks, short lines,
 * table rows, and empty lines pass through unchanged. */
char *weixin_wrap_copy_friendly_lines_for_weixin(const char *content) {
    if (!content || !*content) return strdup(content ? content : "");
    char **lines = NULL; int nl = 0;
    const char *start = content;
    for (const char *p = content; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = p - start;
            char *line = malloc(len + 1);
            memcpy(line, start, len); line[len] = '\0';
            lines = realloc(lines, sizeof(char *) * (nl + 2));
            lines[nl++] = line;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    char **out = malloc(sizeof(char *) * (nl * 4 + 1));
    int no = 0; bool in_code = false;
    for (int i = 0; i < nl; i++) {
        const char *raw = lines[i];
        const char *ls = raw; while (*ls == ' ' || *ls == '\t') ls++;
        if (weixin__is_fence(ls)) { in_code = !in_code; out[no++] = strdup(raw); continue; }
        if (in_code || strlen(raw) <= WEIXIN_COPY_LINE_WIDTH || !ls[0] ||
            ls[0] == '|' || weixin__is_table_rule(ls)) {
            out[no++] = strdup(raw);
            continue;
        }
        /* greedy wrap on spaces */
        const char *cur = raw;
        char *buf = malloc(strlen(raw) + 1); buf[0] = '\0';
        bool has = false;
        const char *w = cur;
        while (*w) {
            while (*w == ' ') w++;
            if (!*w) break;
            const char *we = w;
            while (*we && *we != ' ') we++;
            size_t wlen = we - w;
            if (has && strlen(buf) + 1 + wlen > WEIXIN_COPY_LINE_WIDTH) {
                out[no++] = strdup(buf);
                buf[0] = '\0'; has = false;
            }
            if (has) { strncat(buf, " ", 1); }
            strncat(buf, w, wlen);
            has = true;
            w = we;
        }
        if (has) out[no++] = strdup(buf);
        free(buf);
    }
    out[no] = NULL;
    size_t cap = 1;
    for (int i = 0; i < no; i++) cap += strlen(out[i]) + 1;
    char *res = malloc(cap); res[0] = '\0';
    for (int i = 0; i < no; i++) {
        if (i) strncat(res, "\n", cap - strlen(res) - 1);
        strncat(res, out[i], cap - strlen(res) - 1);
    }
    size_t L = strlen(res);
    while (L > 0 && res[L-1] == '\n') res[--L] = '\0';
    for (int i = 0; i < no; i++) free(out[i]);
    free(out);
    for (int i = 0; i < nl; i++) free(lines[i]);
    free(lines);
    return res;
}

/* PoP: _split_delivery_units_for_weixin @ gateway/platforms/weixin.py:_split_delivery_units_for_weixin */
/* Split formatted content into chat-friendly delivery units: one unit per
 * fenced block, otherwise one unit per top-level (non-indented) line group,
 * attaching indented continuation lines to the preceding unit. */
char **weixin_split_delivery_units_for_weixin(const char *content, int *out_count) {
    if (out_count) *out_count = 0;
    if (!content || !*content) return NULL;
    char **blocks = weixin_split_markdown_blocks(content, NULL);
    if (!blocks) return NULL;
    int nb = 0; while (blocks[nb]) nb++;
    char **units = malloc(sizeof(char *) * (nb * 8 + 1));
    int nu = 0;
    for (int b = 0; b < nb; b++) {
        const char *blk = blocks[b];
        const char *first = blk; while (*first == ' ' || *first == '\t') first++;
        if (weixin__is_fence(first)) { units[nu++] = strdup(blk); continue; }
        char **lines = NULL; int nl = 0;
        const char *start = blk;
        for (const char *p = blk; ; p++) {
            if (*p == '\n' || *p == '\0') {
                size_t len = p - start;
                char *line = malloc(len + 1);
                memcpy(line, start, len); line[len] = '\0';
                lines = realloc(lines, sizeof(char *) * (nl + 2));
                lines[nl++] = line;
                start = p + 1;
                if (*p == '\0') break;
            }
        }
        char **cur = malloc(sizeof(char *) * (nl + 1));
        int nc = 0;
        for (int i = 0; i < nl; i++) {
            const char *rl = lines[i];
            if (rl[0] == '\0') {
                if (nc > 0) { cur[nc] = NULL; units[nu++] = weixin__join_strip(cur, nc); nc = 0; }
                continue;
            }
            bool cont = (nc > 0) && (rl[0] == ' ' || rl[0] == '\t');
            if (cont) { cur[nc++] = strdup(rl); continue; }
            if (nc > 0) { cur[nc] = NULL; units[nu++] = weixin__join_strip(cur, nc); }
            cur[nc++] = strdup(rl);
        }
        if (nc > 0) { cur[nc] = NULL; units[nu++] = weixin__join_strip(cur, nc); }
        for (int i = 0; i < nl; i++) free(lines[i]);
        free(lines); free(cur);
    }
    units[nu] = NULL;
    int out_n = 0;
    for (int i = 0; i < nu; i++) if (units[i] && units[i][0]) units[out_n++] = units[i]; else free(units[i]);
    units[out_n] = NULL;
    for (int i = 0; i < nb; i++) free(blocks[i]);
    free(blocks);
    if (out_count) *out_count = out_n;
    return units;
}

/* PoP: truncate_message @ gateway/platforms/base.py:truncate_message */
/* Faithful port: split long content into <=max_length chunks, preserving
 * fenced-code-block boundaries (close orphan fences, reopen next chunk with the
 * same language tag), and append (i/total) indicators when multiple chunks. */
char **weixin_truncate_message(const char *content, int max_length, int *out_count) {
    if (out_count) *out_count = 0;
    if (!content) return NULL;
    int total = (int)strlen(content);
    if (total <= max_length) {
        char **r = malloc(sizeof(char *) * 2);
        r[0] = strdup(content); r[1] = NULL;
        if (out_count) *out_count = 1;
        return r;
    }
    const int INDICATOR_RESERVE = 10;
    const char *FENCE_CLOSE = "\n```";
    char **chunks = malloc(sizeof(char *) * 64);
    int nch = 0;
    char *remaining = strdup(content);
    char *carry_lang = NULL;
    while (remaining && *remaining) {
        char *prefix = NULL;
        if (carry_lang) {
            size_t pl = strlen(carry_lang) + 5;
            prefix = malloc(pl + 1);
            snprintf(prefix, pl + 1, "```%s\n", carry_lang);
        }
        int pref_len = prefix ? (int)strlen(prefix) : 0;
        int headroom = max_length - INDICATOR_RESERVE - pref_len - (int)strlen(FENCE_CLOSE);
        if (headroom < 1) headroom = max_length / 2;
        int rem_len = (int)strlen(remaining);
        if (pref_len + rem_len <= max_length - INDICATOR_RESERVE) {
            char *full = malloc(pref_len + rem_len + 1);
            full[0] = '\0';
            if (prefix) strcat(full, prefix);
            strcat(full, remaining);
            chunks[nch++] = full;
            break;
        }
        int cp_limit = headroom;
        if (cp_limit > rem_len) cp_limit = rem_len;
        int region_len = cp_limit;
        int split_at = -1;
        for (int i = region_len - 1; i >= 0; i--) {
            if (remaining[i] == '\n') { split_at = i; break; }
        }
        if (split_at < region_len / 2) {
            for (int i = region_len - 1; i >= 0; i--) {
                if (remaining[i] == ' ') { split_at = i; break; }
            }
        }
        if (split_at < 1) split_at = region_len;
        /* avoid splitting inside an inline code span */
        char *candidate = malloc(split_at + 1);
        memcpy(candidate, remaining, split_at); candidate[split_at] = '\0';
        int bt = 0; for (int i = 0; candidate[i]; i++) if (candidate[i] == '`') bt++;
        if (bt % 2 == 1) {
            int last_bt = -1;
            for (int i = (int)strlen(candidate) - 1; i > 0; i--) if (candidate[i] == '`') { last_bt = i; break; }
            if (last_bt > 0) {
                int safe = -1;
                for (int i = last_bt - 1; i >= 0; i--) if (candidate[i] == ' ' || candidate[i] == '\n') { safe = i; break; }
                if (safe > region_len / 4) split_at = safe;
            }
        }
        free(candidate);
        int chunk_body_len = split_at;
        char *chunk_body = malloc(chunk_body_len + 1);
        memcpy(chunk_body, remaining, chunk_body_len); chunk_body[chunk_body_len] = '\0';
        char *rest = strdup(remaining + split_at);
        /* lstrip rest */
        char *p = rest; while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        memmove(rest, p, strlen(p) + 1);
        /* determine if we end inside an open code block */
        bool in_code = (carry_lang != NULL);
        char *lang = carry_lang ? strdup(carry_lang) : strdup("");
        char *cb = chunk_body;
        char *line = NULL; size_t cap = 0;
        /* iterate lines of chunk_body */
        {
            const char *s = chunk_body;
            while (*s) {
                const char *e = s; while (*e && *e != '\n') e++;
                size_t ll = e - s;
                free(line); line = malloc(ll + 1); memcpy(line, s, ll); line[ll] = '\0';
                char *st = line; while (*st == ' ' || *st == '\t') st++;
                if (strncmp(st, "```", 3) == 0) {
                    if (in_code) { in_code = false; free(lang); lang = strdup(""); }
                    else {
                        in_code = true;
                        char *tag = st + 3; while (*tag == ' ') tag++;
                        char *sp = tag; while (*sp && *sp != ' ' && *sp != '\n' && *sp != '\t') sp++;
                        *sp = '\0';
                        free(lang); lang = strdup(tag);
                    }
                }
                s = (*e) ? e + 1 : e;
            }
        }
        free(line);
        char *full_chunk = malloc(pref_len + chunk_body_len + (in_code ? strlen(FENCE_CLOSE) : 0) + 1);
        full_chunk[0] = '\0';
        if (prefix) strcat(full_chunk, prefix);
        strcat(full_chunk, chunk_body);
        if (in_code) { strcat(full_chunk, FENCE_CLOSE); free(carry_lang); carry_lang = strdup(lang); }
        else { free(carry_lang); carry_lang = NULL; }
        chunks[nch++] = full_chunk;
        free(chunk_body); free(remaining); remaining = rest;
        free(prefix); free(lang);
    }
    /* append indicators when multiple chunks */
    if (nch > 1) {
        int total_n = nch;
        for (int i = 0; i < nch; i++) {
            char *with = malloc(strlen(chunks[i]) + 16);
            snprintf(with, strlen(chunks[i]) + 16, "%s (%d/%d)", chunks[i], i + 1, total_n);
            free(chunks[i]); chunks[i] = with;
        }
    }
    chunks[nch] = NULL;
    free(remaining); free(carry_lang);
    if (out_count) *out_count = nch;
    return chunks;
}

/* PoP: _pack_markdown_blocks_for_weixin @ gateway/platforms/weixin.py:_pack_markdown_blocks_for_weixin */
char **weixin_pack_markdown_blocks_for_weixin(const char *content, int max_length, int *out_count) {
    if (out_count) *out_count = 0;
    if (!content) return NULL;
    if ((int)strlen(content) <= max_length) {
        char **r = malloc(sizeof(char *) * 2);
        r[0] = strdup(content); r[1] = NULL;
        if (out_count) *out_count = 1;
        return r;
    }
    char **blocks = weixin_split_markdown_blocks(content, NULL);
    if (!blocks) return NULL;
    int nb = 0; while (blocks[nb]) nb++;
    char **packed = malloc(sizeof(char *) * (nb * 4 + 1));
    int np = 0;
    char *current = strdup("");
    for (int b = 0; b < nb; b++) {
        char *candidate;
        if (!*current) candidate = strdup(blocks[b]);
        else {
            size_t cl = strlen(current), bl = strlen(blocks[b]);
            candidate = malloc(cl + 2 + bl + 1);
            snprintf(candidate, cl + 2 + bl + 1, "%s\n\n%s", current, blocks[b]);
        }
        if ((int)strlen(candidate) <= max_length) {
            free(current); current = candidate;
            continue;
        }
        if (*current) packed[np++] = current; else free(current);
        current = strdup("");
        if ((int)strlen(blocks[b]) <= max_length) {
            current = strdup(blocks[b]);
            continue;
        }
        int tn = 0;
        char **tr = weixin_truncate_message(blocks[b], max_length, &tn);
        for (int i = 0; i < tn; i++) packed[np++] = tr[i];
        free(tr);
        free(candidate);
    }
    if (*current) packed[np++] = current; else free(current);
    packed[np] = NULL;
    int out_n = 0;
    for (int i = 0; i < np; i++) if (packed[i] && packed[i][0]) packed[out_n++] = packed[i]; else free(packed[i]);
    packed[out_n] = NULL;
    for (int i = 0; i < nb; i++) free(blocks[i]);
    free(blocks);
    if (out_count) *out_count = out_n;
    return packed;
}

/* PoP: _split_text_for_weixin_delivery @ gateway/platforms/weixin.py:_split_text_for_weixin_delivery */
char **weixin_split_text_for_weixin_delivery(const char *content, int max_length, bool split_per_line, int *out_count) {
    if (out_count) *out_count = 0;
    if (!content || !*content) return NULL;
    char **result = NULL;
    if (split_per_line) {
        if ((int)strlen(content) <= max_length && strchr(content, '\n') == NULL) {
            result = malloc(sizeof(char *) * 2);
            result[0] = strdup(content); result[1] = NULL;
            if (out_count) *out_count = 1;
            return result;
        }
        char **units = weixin_split_delivery_units_for_weixin(content, NULL);
        int nu = 0; while (units && units[nu]) nu++;
        char **chunks = malloc(sizeof(char *) * (nu * 4 + 1));
        int nc = 0;
        for (int i = 0; i < nu; i++) {
            if ((int)strlen(units[i]) <= max_length) {
                chunks[nc++] = strdup(units[i]);
            } else {
                int pn = 0;
                char **pk = weixin_pack_markdown_blocks_for_weixin(units[i], max_length, &pn);
                for (int j = 0; j < pn; j++) chunks[nc++] = pk[j];
                free(pk);
            }
        }
        chunks[nc] = NULL;
        int out_n = 0;
        for (int i = 0; i < nc; i++) if (chunks[i] && chunks[i][0]) chunks[out_n++] = chunks[i]; else free(chunks[i]);
        chunks[out_n] = NULL;
        for (int i = 0; i < nu; i++) free(units[i]);
        free(units);
        if (out_n == 0) { free(chunks); result = malloc(sizeof(char *) * 2); result[0] = strdup(content); result[1] = NULL; if (out_count) *out_count = 1; return result; }
        if (out_count) *out_count = out_n;
        return chunks;
    }
    /* compact (default): single message when under the limit — unless the
     * content looks like a short chatty exchange, in which case split. */
    if ((int)strlen(content) <= max_length) {
        if (weixin_should_split_short_chat_block_for_weixin(content)) {
            char **units = weixin_split_delivery_units_for_weixin(content, NULL);
            int nu = 0; while (units && units[nu]) nu++;
            char **out = malloc(sizeof(char *) * (nu + 1));
            int on = 0;
            for (int i = 0; i < nu; i++) if (units[i] && units[i][0]) out[on++] = strdup(units[i]);
            out[on] = NULL;
            for (int i = 0; i < nu; i++) free(units[i]);
            free(units);
            if (out_count) *out_count = on;
            return out;
        }
        result = malloc(sizeof(char *) * 2);
        result[0] = strdup(content); result[1] = NULL;
        if (out_count) *out_count = 1;
        return result;
    }
    result = weixin_pack_markdown_blocks_for_weixin(content, max_length, out_count);
    if (!result || !*result) { result = malloc(sizeof(char *) * 2); result[0] = strdup(content); result[1] = NULL; if (out_count) *out_count = 1; }
    return result;
}

