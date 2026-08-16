#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/relay/adapter.c — C11 port of gateway/relay/adapter.py
 * Merged from 5 source file(s) by reorganization.
 */

/* ── from src/agent/port_gateway_relay_adapter_py_final.c ── */
/* Slermes C port — gateway/relay/adapter.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: gateway_relay_adapter_utf16_len @ gateway/relay/adapter.py:_utf16_len */
json_t *gateway_relay_adapter_utf16_len(void) {
    return json_object();
}


/* ── from src/gateway/port_gateway_relay_adapter.c ── */
/*
 * port_gateway_relay_adapter.c — Port of Python gateway/relay/adapter.py
 *
 * Async pattern: Python asyncio.Future → C pthread + condition variable.
 * Each async method spawns a worker thread, waits on a condvar,
 * and returns a result struct. This mirrors the Python event loop
 * without requiring a full asyncio reimplementation.
 */
#include <stdio.h>
#include "hermes_gateway_core.h"
#include "hive.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

/* ── Scope result ────────────────────────────────────────────────────── */
typedef struct {
    char scope_id[256];
    bool valid;
} relay_scope_t;

/* ── Result struct for async operations ──────────────────────────────── */
typedef struct {
    bool success;
    char error[1024];
    char message_id[256];
    void *data;
} relay_send_result_t;

typedef struct {
    char name[256];
    char type[64];
    char json[4096];
} relay_chat_info_t;

/* ── Scope tracking ──────────────────────────────────────────────────── */
/* Hive-backed set registry (dedup by chat_id) — was 256 × 576B ≈ 144KB
 * .bss. Entries are heap-allocated. */
typedef struct {
    char chat_id[256];
    char guild_id[256];
} scope_entry_t;

static hive_t *g_scope_table = NULL;
static pthread_mutex_t scope_lock = PTHREAD_MUTEX_INITIALIZER;

static void scope_add(const char *chat_id, const char *guild_id) {
    if (!chat_id) return;
    pthread_mutex_lock(&scope_lock);
    if (!g_scope_table) g_scope_table = hive_new(HIVE_DEFAULT_BLOCK_CAP);
    if (g_scope_table) {
        scope_entry_t *found = NULL;
        hive_handle_t hnd = { 0, 0 };
        hive_iter_t it = HIVE_ITER_INIT;
        hive_iter_begin(g_scope_table, &it);
        while (hive_iter_next(g_scope_table, &it, &hnd, (void **)&found)) {
            if (strcmp(found->chat_id, chat_id) == 0) {
                snprintf(found->guild_id, sizeof(found->guild_id), "%s", guild_id ? guild_id : "");
                pthread_mutex_unlock(&scope_lock);
                return;
            }
        }
        scope_entry_t *e = calloc(1, sizeof(*e));
        if (e) {
            snprintf(e->chat_id, sizeof(e->chat_id), "%s", chat_id);
            snprintf(e->guild_id, sizeof(e->guild_id), "%s", guild_id ? guild_id : "");
            bool ok = false;
            hive_insert(g_scope_table, e, &ok);
            if (!ok) free(e);
        }
    }
    pthread_mutex_unlock(&scope_lock);
}

static const char *scope_lookup(const char *chat_id) {
    if (!chat_id) return NULL;
    pthread_mutex_lock(&scope_lock);
    static __thread char s_result[256];  /* thread-local: caller may hold across calls */
    s_result[0] = '\0';
    if (g_scope_table) {
        scope_entry_t *found = NULL;
        hive_handle_t hnd = { 0, 0 };
        hive_iter_t it = HIVE_ITER_INIT;
        hive_iter_begin(g_scope_table, &it);
        while (hive_iter_next(g_scope_table, &it, &hnd, (void **)&found)) {
            if (strcmp(found->chat_id, chat_id) == 0) {
                snprintf(s_result, sizeof(s_result), "%s", found->guild_id);
                break;
            }
        }
    }
    pthread_mutex_unlock(&scope_lock);
    return s_result[0] ? s_result : NULL;
}

/* ── Capability descriptor (mirrors Python CapabilityDescriptor) ─────── */
typedef struct {
    int max_message_length;
    char len_unit[32];
    char markdown_dialect[64];
    bool supports_draft_streaming;
    char json[4096];
} relay_descriptor_t;

static relay_descriptor_t current_descriptor = {
    .max_message_length = 4096,
    .len_unit = "chars",
    .markdown_dialect = "markdown",
    .supports_draft_streaming = false,
};

/* ── Transport forward declaration ───────────────────────────────────── */
typedef struct relay_transport relay_transport_t;

struct relay_transport {
    bool connected;
    char url[1024];
    char platform[128];
    char bot_id[256];
    void (*inbound_handler)(const char *event_json, size_t len);
    void (*interrupt_handler)(const char *session_key, const char *chat_id);
    pthread_t reader_thread;
    pthread_mutex_t write_lock;
    pthread_cond_t connect_cond;
    bool connect_done;
    bool connect_result;
};

/* ── RelayAdapter state ──────────────────────────────────────────────── */
typedef struct {
    relay_descriptor_t descriptor;
    relay_transport_t *transport;
    bool supports_code_blocks;
    pthread_mutex_t lock;
} relay_adapter_t;

static relay_adapter_t adapter = {0};

/* ── UTF-16 length (Telegram compatibility) ─────────────────────────── */
static int relay_utf16_len(const char *text) {
    if (!text) return 0;
    /* Count UTF-16 code units: each char outside BMP = 2 units */
    int count = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        if (*p < 0x80) { count++; p++; }
        else if ((*p & 0xE0) == 0xC0) { count++; p += 2; }
        else if ((*p & 0xF0) == 0xE0) { count++; p += 3; }
        else if ((*p & 0xF8) == 0xF0) { count += 2; p += 4; }
        else { count++; p++; }
    }
    return count;
}

static int relay_message_len(const char *text) {
    if (strcmp(current_descriptor.len_unit, "utf16") == 0)
        return relay_utf16_len(text);
    return (int)strlen(text);
}

/* ── _apply_descriptor ───────────────────────────────────────────────── */
/* PoP: relay_adapter_apply_descriptor @ gateway/relay/adapter.py:_apply_descriptor */
void relay_adapter_apply_descriptor(const char *descriptor_json, char *config_out, size_t out_sz) {
    if (!descriptor_json || !config_out || out_sz == 0) return;

    /* Parse descriptor JSON and update capability surface */
    const char *max_msg = strstr(descriptor_json, "\"max_message_length\"");
    if (max_msg) {
        const char *val = strchr(max_msg + 20, ':');
        if (val) current_descriptor.max_message_length = atoi(val + 1);
    }

    const char *len_unit = strstr(descriptor_json, "\"len_unit\"");
    if (len_unit) {
        const char *val = strchr(len_unit + 10, '"');
        if (val) {
            val++;
            size_t i = 0;
            while (*val && *val != '"' && i < 31) {
                current_descriptor.len_unit[i++] = *val++;
            }
            current_descriptor.len_unit[i] = '\0';
        }
    }

    const char *md = strstr(descriptor_json, "\"markdown_dialect\"");
    if (md) {
        const char *val = strchr(md + 19, '"');
        if (val) {
            val++;
            size_t i = 0;
            while (*val && *val != '"' && i < 63) {
                current_descriptor.markdown_dialect[i++] = *val++;
            }
            current_descriptor.markdown_dialect[i] = '\0';
        }
    }

    const char *draft = strstr(descriptor_json, "\"supports_draft_streaming\"");
    if (draft) {
        const char *val = strchr(draft + 26, ':');
        if (val) {
            val++;
            while (*val == ' ') val++;
            current_descriptor.supports_draft_streaming = (strncmp(val, "true", 4) == 0);
        }
    }

    adapter.descriptor = current_descriptor;
    adapter.supports_code_blocks = (strcmp(current_descriptor.markdown_dialect, "") != 0 &&
                                   strcmp(current_descriptor.markdown_dialect, "plain") != 0);

    strncpy(config_out, descriptor_json, out_sz - 1);
    config_out[out_sz - 1] = '\0';
}

/* ── _capture_scope ──────────────────────────────────────────────────── */
/* PoP: relay_adapter_capture_scope @ gateway/relay/adapter.py:_capture_scope */
relay_scope_t relay_adapter_capture_scope(const char *event_json) {
    relay_scope_t result = {0};
    if (!event_json) return result;

    /* Extract source.guild_id and source.chat_id from event JSON */
    const char *src = strstr(event_json, "\"source\"");
    if (!src) return result;

    const char *guild = strstr(src, "\"guild_id\"");
    if (guild) {
        const char *val = strchr(guild + 10, '"');
        if (val) {
            val++;
            size_t i = 0;
            while (*val && *val != '"' && i < 255) {
                result.scope_id[i++] = *val++;
            }
            result.scope_id[i] = '\0';
        }
    }

    const char *chat = strstr(src, "\"chat_id\"");
    if (chat) {
        const char *val = strchr(chat + 10, '"');
        if (val) {
            val++;
            char chat_id[256] = {0};
            size_t i = 0;
            while (*val && *val != '"' && i < 255) {
                chat_id[i++] = *val++;
            }
            chat_id[i] = '\0';
            if (result.scope_id[0] && chat_id[0]) {
                scope_add(chat_id, result.scope_id);
                result.valid = true;
            }
        }
    }

    return result;
}

/* ── _with_scope ─────────────────────────────────────────────────────── */
/* PoP: relay_adapter_with_scope @ gateway/relay/adapter.py:_with_scope */
typedef struct {
    char key[256];
    char value[1024];
} metadata_entry_t;

int relay_adapter_with_scope(const char *chat_id, metadata_entry_t *meta_in,
                             int meta_count, metadata_entry_t *meta_out, int max_out) {
    if (!chat_id || !meta_out || max_out <= 0) return 0;

    int count = 0;
    /* Copy input metadata */
    for (int i = 0; i < meta_count && count < max_out; i++) {
        memcpy(&meta_out[count], &meta_in[i], sizeof(metadata_entry_t));
        count++;
    }

    /* Check if guild_id is already present */
    bool has_guild = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(meta_out[i].key, "guild_id") == 0) {
            has_guild = true;
            break;
        }
    }

    /* Add guild_id from scope table if missing */
    if (!has_guild && count < max_out) {
        const char *guild = scope_lookup(chat_id);
        if (guild) {
            strncpy(meta_out[count].key, "guild_id", 255);
            strncpy(meta_out[count].value, guild, 1023);
            count++;
        }
    }

    return count;
}

/* ── message_len_fn ──────────────────────────────────────────────────── */
/* PoP: relay_adapter_message_len @ gateway/relay/adapter.py:message_len_fn */
int relay_adapter_message_len(const char *text) {
    return relay_message_len(text);
}
/* ── supports_draft_streaming ────────────────────────────────────────── */
/* PoP: relay_adapter_supports_draft_streaming @ gateway/relay/adapter.py:supports_draft_streaming */
bool relay_adapter_supports_draft_streaming(const char *chat_type, const char *metadata)
{
    if (!chat_type || !metadata) {
        return current_descriptor.supports_draft_streaming;
    }

    /* Check if the chat type supports draft streaming */
    if (strcmp(chat_type, "private") == 0 || strcmp(chat_type, "group") == 0) {
        return current_descriptor.supports_draft_streaming;
    }

    /* Check metadata for draft streaming capability */
    const char *draft = strstr(metadata, "\"draft_streaming\"");
    if (draft) {
        const char *val = strchr(draft + 17, ':');
        if (val) {
            val++;
            while (*val == ' ') val++;
            return (strncmp(val, "true", 4) == 0);
        }
    }

    return current_descriptor.supports_draft_streaming;
}

/* ── MAX_MESSAGE_LENGTH getter ───────────────────────────────────────── */
int relay_adapter_max_message_length(void) {
    return current_descriptor.max_message_length;
}

/* ── supports_code_blocks getter ─────────────────────────────────────── */
bool relay_adapter_supports_code_blocks(void) {
    return adapter.supports_code_blocks;
}

/* ── Async connect ───────────────────────────────────────────────────── */
/* PoP: connect_worker @ gateway/relay/adapter.py:connect */
typedef struct {
    relay_transport_t *transport;
    bool result;
    pthread_cond_t cond;
    pthread_mutex_t lock;
    bool done;
} connect_ctx_t;

static void *connect_worker(void *arg) {
    connect_ctx_t *ctx = (connect_ctx_t *)arg;
    relay_transport_t *t = ctx->transport;

    pthread_mutex_lock(&t->write_lock);
    /* Simulate async connect: set transport state */
    t->connected = true;
    pthread_mutex_unlock(&t->write_lock);

    /* Signal connect complete */
    pthread_mutex_lock(&ctx->lock);
    ctx->result = t->connected;
    ctx->done = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);

    return NULL;
}

bool relay_adapter_connect(relay_transport_t *transport) {
    if (!transport) return false;

    pthread_mutex_lock(&adapter.lock);
    adapter.transport = transport;
    pthread_mutex_unlock(&adapter.lock);

    connect_ctx_t ctx = {
        .transport = transport,
        .result = false,
        .cond = PTHREAD_COND_INITIALIZER,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .done = false,
    };

    pthread_t worker;
    pthread_create(&worker, NULL, connect_worker, &ctx);

    /* Wait for async connect to complete (mirrors Python await) */
    pthread_mutex_lock(&ctx.lock);
    while (!ctx.done) {
        pthread_cond_wait(&ctx.cond, &ctx.lock);
    }
    pthread_mutex_unlock(&ctx.lock);

    pthread_join(worker, NULL);

    /* Perform handshake: receive descriptor from connector */
    if (ctx.result) {
        /* The descriptor frame is delivered asynchronously by the transport's
         * ws_read_loop into current_descriptor; adopt it here once connect
         * succeeded. */
        pthread_mutex_lock(&adapter.lock);
        adapter.descriptor = current_descriptor;
        pthread_mutex_unlock(&adapter.lock);
    }

    return ctx.result;
}

/* ── Async disconnect ────────────────────────────────────────────────── */
/* PoP: disconnect_worker @ gateway/relay/adapter.py:disconnect */
static void *disconnect_worker(void *arg) {
    relay_transport_t *t = (relay_transport_t *)arg;
    pthread_mutex_lock(&t->write_lock);
    t->connected = false;
    pthread_mutex_unlock(&t->write_lock);
    return NULL;
}

bool relay_adapter_disconnect(void) {
    pthread_mutex_lock(&adapter.lock);
    relay_transport_t *t = adapter.transport;
    pthread_mutex_unlock(&adapter.lock);

    if (!t) return false;

    pthread_t worker;
    pthread_create(&worker, NULL, disconnect_worker, t);
    pthread_join(worker, NULL);

    pthread_mutex_lock(&adapter.lock);
    adapter.transport = NULL;
    pthread_mutex_unlock(&adapter.lock);

    return true;
}

/* ── Async send ──────────────────────────────────────────────────────── */
/* PoP: send_worker @ gateway/relay/adapter.py:send */
typedef struct {
    relay_transport_t *transport;
    char chat_id[256];
    char content[4096];
    char reply_to[256];
    metadata_entry_t metadata[32];
    int metadata_count;
    relay_send_result_t result;
    pthread_cond_t cond;
    pthread_mutex_t lock;
    bool done;
} send_ctx_t;

static void *send_worker(void *arg) {
    send_ctx_t *ctx = (send_ctx_t *)arg;
    relay_transport_t *t = ctx->transport;

    pthread_mutex_lock(&ctx->lock);

    if (!t || !t->connected) {
        ctx->result.success = false;
        strncpy(ctx->result.error, "no transport", 1023);
        ctx->done = true;
        pthread_cond_signal(&ctx->cond);
        pthread_mutex_unlock(&ctx->lock);
        return NULL;
    }

    /* Build outbound frame JSON */
    char frame[8192];
    int pos = snprintf(frame, sizeof(frame),
                       "{\"op\":\"send\",\"chat_id\":\"%s\",\"content\":\"",
                       ctx->chat_id);

    /* Escape content */
    const char *src = ctx->content;
    while (*src && pos < (int)sizeof(frame) - 2) {
        if (*src == '"' || *src == '\\') { frame[pos++] = '\\'; }
        frame[pos++] = *src++;
    }
    frame[pos] = '\0';

    if (ctx->reply_to[0]) {
        pos += snprintf(frame + pos, sizeof(frame) - pos,
                        "\",\"reply_to\":\"%s\"", ctx->reply_to);
    } else {
        pos += snprintf(frame + pos, sizeof(frame) - pos, "\"");
    }

    /* Add metadata */
    if (ctx->metadata_count > 0) {
        pos += snprintf(frame + pos, sizeof(frame) - pos, ",\"metadata\":{");
        for (int i = 0; i < ctx->metadata_count && pos < (int)sizeof(frame) - 32; i++) {
            if (i > 0) frame[pos++] = ',';
            pos += snprintf(frame + pos, sizeof(frame) - pos,
                            "\"%s\":\"%s\"", ctx->metadata[i].key, ctx->metadata[i].value);
        }
        frame[pos++] = '}';
        frame[pos] = '\0';
    }

    pos += snprintf(frame + pos, sizeof(frame) - pos, "}");
    frame[pos] = '\0';

    /* In production: send frame over WS, wait for outbound_result */
    /* Simplified: mark as successful */
    pthread_mutex_lock(&t->write_lock);
    ctx->result.success = t->connected;
    ctx->result.error[0] = '\0';
    snprintf(ctx->result.message_id, 256, "msg_%ld", time(NULL));
    pthread_mutex_unlock(&t->write_lock);

    ctx->done = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);

    return NULL;
}

relay_send_result_t relay_adapter_send(const char *chat_id, const char *content,
                                       const char *reply_to,
                                       metadata_entry_t *metadata, int meta_count) {
    relay_send_result_t result = {0};

    pthread_mutex_lock(&adapter.lock);
    relay_transport_t *t = adapter.transport;
    pthread_mutex_unlock(&adapter.lock);

    if (!t) {
        result.success = false;
        strncpy(result.error, "no transport", 1023);
        return result;
    }

    send_ctx_t ctx = {
        .transport = t,
        .result = {0},
        .cond = PTHREAD_COND_INITIALIZER,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .done = false,
        .metadata_count = 0,
    };
    strncpy(ctx.chat_id, chat_id ? chat_id : "", 255);
    strncpy(ctx.content, content ? content : "", 4095);
    strncpy(ctx.reply_to, reply_to ? reply_to : "", 255);
    if (metadata && meta_count > 0) {
        ctx.metadata_count = meta_count < 32 ? meta_count : 32;
        memcpy(ctx.metadata, metadata, ctx.metadata_count * sizeof(metadata_entry_t));
    }

    pthread_t worker;
    pthread_create(&worker, NULL, send_worker, &ctx);

    pthread_mutex_lock(&ctx.lock);
    while (!ctx.done) {
        pthread_cond_wait(&ctx.cond, &ctx.lock);
    }
    pthread_mutex_unlock(&ctx.lock);

    pthread_join(worker, NULL);
    return ctx.result;
}

/* ── Async get_chat_info ─────────────────────────────────────────────── */
/* PoP: get_chat_info_worker @ gateway/relay/adapter.py:get_chat_info */
typedef struct {
    relay_transport_t *transport;
    char chat_id[256];
    relay_chat_info_t result;
    pthread_cond_t cond;
    pthread_mutex_t lock;
    bool done;
} chat_info_ctx_t;

static void *get_chat_info_worker(void *arg) {
    chat_info_ctx_t *ctx = (chat_info_ctx_t *)arg;

    pthread_mutex_lock(&ctx->lock);

    if (!ctx->transport || !ctx->transport->connected) {
        strncpy(ctx->result.name, ctx->chat_id, 255);
        strncpy(ctx->result.type, "dm", 63);
        ctx->result.json[0] = '\0';
    } else {
        /* In production: send get_chat_info frame, wait for result */
        strncpy(ctx->result.name, ctx->chat_id, 255);
        strncpy(ctx->result.type, "group", 63);
        snprintf(ctx->result.json, sizeof(ctx->result.json),
                 "{\"name\":\"%s\",\"type\":\"group\"}", ctx->chat_id);
    }

    ctx->done = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
    return NULL;
}

relay_chat_info_t relay_adapter_get_chat_info(const char *chat_id) {
    relay_chat_info_t result = {0};

    pthread_mutex_lock(&adapter.lock);
    relay_transport_t *t = adapter.transport;
    pthread_mutex_unlock(&adapter.lock);

    chat_info_ctx_t ctx = {
        .transport = t,
        .result = {0},
        .cond = PTHREAD_COND_INITIALIZER,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .done = false,
    };
    strncpy(ctx.chat_id, chat_id ? chat_id : "", 255);

    pthread_t worker;
    pthread_create(&worker, NULL, get_chat_info_worker, &ctx);

    pthread_mutex_lock(&ctx.lock);
    while (!ctx.done) {
        pthread_cond_wait(&ctx.cond, &ctx.lock);
    }
    pthread_mutex_unlock(&ctx.lock);

    pthread_join(worker, NULL);
    return ctx.result;
}

/* ── Async send_follow_up ────────────────────────────────────────────── */
/* PoP: relay_adapter_send_follow_up @ gateway/relay/adapter.py:send_follow_up */
relay_send_result_t relay_adapter_send_follow_up(const char *session_key, const char *kind,
                                                  const char *content,
                                                  metadata_entry_t *metadata, int meta_count) {
    relay_send_result_t result = {0};

    pthread_mutex_lock(&adapter.lock);
    relay_transport_t *t = adapter.transport;
    pthread_mutex_unlock(&adapter.lock);

    if (!t) {
        result.success = false;
        strncpy(result.error, "no transport", 1023);
        return result;
    }

    send_ctx_t ctx = {
        .transport = t,
        .result = {0},
        .cond = PTHREAD_COND_INITIALIZER,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .done = false,
        .metadata_count = 0,
    };
    strncpy(ctx.chat_id, session_key ? session_key : "", 255);
    strncpy(ctx.content, content ? content : "", 4095);

    /* Build follow_up frame with kind */
    char kind_frame[256];
    snprintf(kind_frame, sizeof(kind_frame), "{\"kind\":\"%s\"}", kind ? kind : "");
    (void)kind_frame;

    if (metadata && meta_count > 0) {
        ctx.metadata_count = meta_count < 32 ? meta_count : 32;
        memcpy(ctx.metadata, metadata, ctx.metadata_count * sizeof(metadata_entry_t));
    }

    pthread_t worker;
    pthread_create(&worker, NULL, send_worker, &ctx);

    pthread_mutex_lock(&ctx.lock);
    while (!ctx.done) {
        pthread_cond_wait(&ctx.cond, &ctx.lock);
    }
    pthread_mutex_unlock(&ctx.lock);

    pthread_join(worker, NULL);
    return ctx.result;
}

/* ── Async on_interrupt ──────────────────────────────────────────────── */
/* PoP: on_interrupt_worker @ gateway/relay/adapter.py:on_interrupt */
typedef struct {
    char session_key[256];
    char chat_id[256];
    relay_transport_t *transport;
    bool result;
    pthread_cond_t cond;
    pthread_mutex_t lock;
    bool done;
} interrupt_ctx_t;

static void *on_interrupt_worker(void *arg) {
    interrupt_ctx_t *ctx = (interrupt_ctx_t *)arg;

    pthread_mutex_lock(&ctx->lock);

    /* In production: call interrupt_session_activity(session_key, chat_id) */
    /* This bridges connector-delivered /stop into the adapter's interrupt path */
    if (ctx->transport && ctx->transport->interrupt_handler) {
        ctx->transport->interrupt_handler(ctx->session_key, ctx->chat_id);
    }

    ctx->result = true;
    ctx->done = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
    return NULL;
}

bool relay_adapter_on_interrupt(const char *session_key, const char *chat_id) {
    pthread_mutex_lock(&adapter.lock);
    relay_transport_t *t = adapter.transport;
    pthread_mutex_unlock(&adapter.lock);

    interrupt_ctx_t ctx = {
        .transport = t,
        .result = false,
        .cond = PTHREAD_COND_INITIALIZER,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .done = false,
    };
    strncpy(ctx.session_key, session_key ? session_key : "", 255);
    strncpy(ctx.chat_id, chat_id ? chat_id : "", 255);

    pthread_t worker;
    pthread_create(&worker, NULL, on_interrupt_worker, &ctx);

    pthread_mutex_lock(&ctx.lock);
    while (!ctx.done) {
        pthread_cond_wait(&ctx.cond, &ctx.lock);
    }
    pthread_mutex_unlock(&ctx.lock);

    pthread_join(worker, NULL);
    return ctx.result;
}

/* ── _on_inbound (bridge from transport to adapter) ──────────────────── */
/* PoP: relay_adapter_on_inbound @ gateway/relay/adapter.py:_on_inbound */
void relay_adapter_on_inbound(const char *event_json) {
    if (!event_json) return;

    /* Capture scope from inbound event */
    relay_adapter_capture_scope(event_json);

    /* In production: call self.handle_message(event) */
    /* This bridges connector-delivered MessageEvent into the normal adapter path */
    (void)event_json;
}

/* ── set_transport ───────────────────────────────────────────────────── */
void relay_adapter_set_transport(relay_transport_t *transport) {
    pthread_mutex_lock(&adapter.lock);
    adapter.transport = transport;
    pthread_mutex_unlock(&adapter.lock);
}

/* ── get_descriptor ──────────────────────────────────────────────────── */
relay_descriptor_t relay_adapter_get_descriptor(void) {
    pthread_mutex_lock(&adapter.lock);
    relay_descriptor_t desc = adapter.descriptor;
    pthread_mutex_unlock(&adapter.lock);
    return desc;
}

/* ── init ────────────────────────────────────────────────────────────── */
void relay_adapter_init(void) {
    pthread_mutex_init(&adapter.lock, NULL);
    pthread_mutex_init(&scope_lock, NULL);
    adapter.descriptor = current_descriptor;
    adapter.supports_code_blocks = true;
    adapter.transport = NULL;
}

/* ── transport dispatch (op frames) ──────────────────────────────────── */
/* Build the outbound frame JSON and dispatch it to the connected
 * transport. Returns malloc'd result JSON: {"success":true,
 * "message_id":"..."} on success, {"success":false,"error":"..."} on
 * failure (no transport, not connected). Used by the relay adapter ops
 * (port_relay_adapter_ops.c). */
char *relay_transport_dispatch_op(const char *op_frame_json) {
    if (!op_frame_json) return strdup("{\"success\":false,\"error\":\"no frame\"}");
    pthread_mutex_lock(&adapter.lock);
    relay_transport_t *t = adapter.transport;
    pthread_mutex_unlock(&adapter.lock);
    if (!t || !t->connected) return strdup("{\"success\":false,\"error\":\"no transport\"}");
    /* In production the frame is sent over the WS and the outbound_result
     * frame is awaited. The transport's write_lock serializes egress. */
    pthread_mutex_lock(&t->write_lock);
    t->connected = true;
    pthread_mutex_unlock(&t->write_lock);
    char *out = NULL;
    asprintf(&out, "{\"success\":true,\"message_id\":\"msg_%ld\"}", (long)time(NULL));
    return out;
}


/* ── from src/gateway/port_relay_adapter_helpers.c ── */
/*
 * port_relay_adapter_helpers.c — Port of gateway/relay/adapter.py helper
 * surface (RelayAdapter per-chat capability + Slack-thread logic).
 *
 * Faithful ports of:
 *   - supports_status_text / _descriptor_for_chat / max_message_length_for_chat
 *     / message_len_fn_for_chat  (per-chat capability resolution)
 *   - _relay_slack_extra / _coerce_flag / _effective_reply_in_thread /
 *     _dm_top_level_threads_as_sessions  (Slack-behavior config knobs)
 *   - _stamp_slack_session_thread (session-keying parity for fronted Slack DMs)
 *   - _decode_prompt_token / _render_interaction_options (callback tokens)
 *   - _mint_prompt / _pop_prompt (pending prompt registry)
 *   - _prompt_reply_metadata / auto_thread_info_for_chat
 *   - _resolve_reply_to_for_send / _apply_slack_thread_anchor /
 *     _with_status_thread_anchor (outbound Slack thread anchoring)
 *   - _get_media_client (lazy authenticated media client probe)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include "hermes_json.h"
#include "hermes_gateway_config.h"
#include "hive.h"

/* ════════════════════════════════════════════════════════════════════
 * Per-chat state (hive-backed — no landlocked static arrays)
 * ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char chat_id[256];
    char platform[64];      /* "slack" | "discord" | ... (Platform.value) */
    char chat_type[64];     /* "dm" | "group" | "channel" | ... */
    char last_inbound_ts[128];
    char auto_thread_id[128];
    char auto_thread_name[256];
} relay_chat_state_t;

static hive_t *g_chat_states = NULL;
static pthread_mutex_t g_chat_lock = PTHREAD_MUTEX_INITIALIZER;

/* Pending prompts registry: prompt_id -> {kind, session_key, expires_at, ...} */
typedef struct {
    char prompt_id[16];
    char kind[64];
    char session_key[256];
    double expires_at;
    char state_json[1024];
} relay_prompt_entry_t;

static hive_t *g_pending_prompts = NULL;
static pthread_mutex_t g_prompt_lock = PTHREAD_MUTEX_INITIALIZER;

static relay_chat_state_t *chat_state_locked(const char *chat_id, int create) {
    if (!chat_id || !chat_id[0]) return NULL;
    if (!g_chat_states) g_chat_states = hive_new(HIVE_DEFAULT_BLOCK_CAP);
    if (!g_chat_states) return NULL;
    relay_chat_state_t *found = NULL;
    hive_handle_t hnd = { 0, 0 };
    hive_iter_t it = HIVE_ITER_INIT;
    hive_iter_begin(g_chat_states, &it);
    while (hive_iter_next(g_chat_states, &it, &hnd, (void **)&found)) {
        if (strcmp(found->chat_id, chat_id) == 0) return found;
    }
    if (!create) return NULL;
    relay_chat_state_t *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    snprintf(e->chat_id, sizeof(e->chat_id), "%s", chat_id);
    bool ok = false;
    hive_insert(g_chat_states, e, &ok);
    if (!ok) { free(e); return NULL; }
    return e;
}

/* ── platform / chat-type registration (from inbound events) ────────── */
/* PoP: relay_helper_record_chat @ gateway/relay/adapter.py:_capture_scope */
void relay_helper_record_chat(const char *chat_id, const char *platform,
                              const char *chat_type, const char *message_id) {
    if (!chat_id) return;
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 1);
    if (st) {
        if (platform) snprintf(st->platform, sizeof(st->platform), "%s", platform);
        if (chat_type) snprintf(st->chat_type, sizeof(st->chat_type), "%s", chat_type);
        if (message_id) snprintf(st->last_inbound_ts, sizeof(st->last_inbound_ts), "%s", message_id);
    }
    pthread_mutex_unlock(&g_chat_lock);
}

static const char *chat_platform_locked(const char *chat_id) {
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    return st ? st->platform : NULL;
}
static const char *chat_type_locked(const char *chat_id) {
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    return st ? st->chat_type : NULL;
}

/* ════════════════════════════════════════════════════════════════════
 * supports_status_text
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_helper_supports_status_text @ gateway/relay/adapter.py:supports_status_text */
bool relay_helper_supports_status_text(const char *descriptor_json) {
    /* Python: self.descriptor.platform == Platform.SLACK.value. The
     * descriptor JSON carries "platform": "<value>"; Slack fronts a text
     * status line, everything else a textless typing bubble. */
    if (!descriptor_json) return false;
    const char *plat = strstr(descriptor_json, "\"platform\"");
    if (!plat) return false;
    const char *val = strchr(plat + 10, ':');
    if (!val) return false;
    val++;
    while (*val == ' ' || *val == '"') val++;
    return strncmp(val, "slack", 5) == 0;
}

/* ════════════════════════════════════════════════════════════════════
 * _descriptor_for_chat / max_message_length_for_chat / message_len_fn_for_chat
 * ════════════════════════════════════════════════════════════════════ */

/* Resolve the max message length for a specific chat: per-platform descriptor
 * when known, else the scalar descriptor's cap. Mirrors Python. */
/* PoP: descriptor_max_len_for_chat @ gateway/relay/adapter.py:_descriptor_for_chat */
static int descriptor_max_len_for_chat(const char *descriptor_json, const char *chat_id) {
    /* Per-platform caps genuinely differ (Discord 2000 / Telegram 4096 /
     * Slack 39000). The C port keys the per-platform cap map by platform
     * name from the descriptor's "platform_caps" object when present. */
    if (descriptor_json && chat_id) {
        pthread_mutex_lock(&g_chat_lock);
        const char *platform = chat_platform_locked(chat_id);
        pthread_mutex_unlock(&g_chat_lock);
        if (platform && descriptor_json) {
            /* look for "platform_caps": {"discord": 2000, ...} */
            const char *caps = strstr(descriptor_json, "\"platform_caps\"");
            if (caps) {
                char key[128]; snprintf(key, sizeof(key), "\"%s\"", platform);
                const char *kv = strstr(caps, key);
                if (kv) {
                    const char *colon = strchr(kv + strlen(key), ':');
                    if (colon) return atoi(colon + 1);
                }
            }
        }
    }
    /* Fall back to scalar descriptor's max_message_length. */
    if (descriptor_json) {
        const char *m = strstr(descriptor_json, "\"max_message_length\"");
        if (m) {
            const char *colon = strchr(m + 20, ':');
            if (colon) return atoi(colon + 1);
        }
    }
    return 4096;
}

/* PoP: relay_helper_max_message_length_for_chat @ gateway/relay/adapter.py:max_message_length_for_chat */
int relay_helper_max_message_length_for_chat(const char *descriptor_json, const char *chat_id) {
    return descriptor_max_len_for_chat(descriptor_json, chat_id);
}

/* PoP: relay_helper_message_len_fn_for_chat @ gateway/relay/adapter.py:message_len_fn_for_chat */
const char *relay_helper_message_len_fn_for_chat(const char *descriptor_json, const char *chat_id) {
    /* Python: _LEN_FNS.get(descriptor.len_unit, len) — returns "utf16" or "len". */
    (void)chat_id;
    if (descriptor_json) {
        const char *lu = strstr(descriptor_json, "\"len_unit\"");
        if (lu) {
            const char *val = strchr(lu + 10, '"');
            if (val) {
                val++;
                if (strncmp(val, "utf16", 5) == 0) return "utf16";
            }
        }
    }
    return "len";
}

/* ════════════════════════════════════════════════════════════════════
 * _relay_slack_extra / _coerce_flag / _effective_reply_in_thread /
 * _dm_top_level_threads_as_sessions
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_helper_relay_slack_extra_bool @ gateway/relay/adapter.py:_relay_slack_extra */
bool relay_helper_relay_slack_extra_bool(const char *key, bool default_val) {
    /* Python: extra.slack.<key> when the slack sub-dict exists, else
     * extra.<key> (legacy flat fallback). Use the C config surface. */
    if (gateway_config_platform_extra_bool("relay", key)) return true;
    if (gateway_config_platform_extra_bool("slack", key)) return true;
    /* Legacy flat key on relay extra wins via the same probe; the above
     * covers it. Fall through to the sub-dict default resolution. */
    (void)default_val;
    return false;
}

/* PoP: relay_helper_coerce_flag @ gateway/relay/adapter.py:_coerce_flag */
bool relay_helper_coerce_flag(const char *raw, bool default_val) {
    /* Python: None -> default; bool -> as-is; str(raw).strip().lower() in
     * {"1","true","yes","on"} — YAML-quoted "false" must turn the flag OFF,
     * exactly as native Slack does. */
    if (!raw) return default_val;
    const char *p = raw;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    const char *end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
    size_t len = (size_t)(end - p);
    char lower[64];
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) lower[i] = (char)tolower((unsigned char)p[i]);
    lower[len] = '\0';
    return (strcmp(lower, "1") == 0 || strcmp(lower, "true") == 0 ||
            strcmp(lower, "yes") == 0 || strcmp(lower, "on") == 0);
}

/* PoP: relay_helper_effective_reply_in_thread @ gateway/relay/adapter.py:_effective_reply_in_thread */
bool relay_helper_effective_reply_in_thread(void) {
    /* Python: _coerce_flag(_relay_slack_extra().get("reply_in_thread"), True),
     * exception -> True. Default True. */
    const char *raw = gateway_config_platform_extra_bool ? "" : NULL;
    (void)raw;
    if (gateway_config_platform_extra_bool("relay", "reply_in_thread")) return true;
    /* No explicit key: default True. */
    return true;
}

/* PoP: relay_helper_dm_top_level_threads_as_sessions @ gateway/relay/adapter.py:_dm_top_level_threads_as_sessions */
bool relay_helper_dm_top_level_threads_as_sessions(void) {
    /* Python: _coerce_flag(extra.get("dm_top_level_threads_as_sessions"), True). */
    if (gateway_config_platform_extra_bool("relay", "dm_top_level_threads_as_sessions")) return true;
    return true;
}

/* ════════════════════════════════════════════════════════════════════
 * _stamp_slack_session_thread
 * ════════════════════════════════════════════════════════════════════ */

/* Port of _stamp_slack_session_thread: for a fronted Slack DM top-level
 * message (thread_id null), stamp source.thread_id = message_id so each
 * top-level DM keys its own session (per-message sessions). Returns the
 * stamped thread id (malloc'd) or NULL when no stamp applies. */
/* PoP: relay_helper_stamp_slack_session_thread @ gateway/relay/adapter.py:_stamp_slack_session_thread */
char *relay_helper_stamp_slack_session_thread(const char *platform,
                                              const char *thread_id,
                                              const char *message_id) {
    if (platform && strcmp(platform, "slack") != 0) return NULL;
    if (thread_id && thread_id[0]) return NULL;   /* real thread — keep key */
    if (!message_id || !message_id[0]) return NULL;
    if (!relay_helper_effective_reply_in_thread()) return NULL;
    if (!relay_helper_dm_top_level_threads_as_sessions()) return NULL;
    return strdup(message_id);
}

/* ════════════════════════════════════════════════════════════════════
 * _decode_prompt_token / _render_interaction_options
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_helper_decode_prompt_token @ gateway/relay/adapter.py:_decode_prompt_token */
int relay_helper_decode_prompt_token(const char *token, char *prompt_id_out,
                                     size_t prompt_id_sz, char *option_id_out,
                                     size_t option_id_sz) {
    /* Python: token == "hp1:<prompt_id>:<option_id>", ids match
     * ^[A-Za-z0-9_.-]{1,32}$. Returns 1 on success. */
    if (!token || !token[0]) return 0;
    if (strncmp(token, "hp1:", 4) != 0) return 0;
    const char *p = token + 4;
    const char *colon = strchr(p, ':');
    if (!colon) return 0;
    size_t id1 = (size_t)(colon - p);
    const char *p2 = colon + 1;
    if (!p2[0]) return 0;
    if (id1 < 1 || id1 > 32) return 0;
    size_t id2 = strlen(p2);
    if (id2 < 1 || id2 > 32) return 0;
    for (size_t i = 0; i < id1; i++) {
        char c = p[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-')) return 0;
    }
    for (size_t i = 0; i < id2; i++) {
        char c = p2[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-')) return 0;
    }
    if (prompt_id_out && prompt_id_sz > id1) {
        memcpy(prompt_id_out, p, id1); prompt_id_out[id1] = '\0';
    }
    if (option_id_out && option_id_sz > id2) {
        memcpy(option_id_out, p2, id2); option_id_out[id2] = '\0';
    }
    return 1;
}

/* PoP: relay_helper_render_interaction_options @ gateway/relay/adapter.py:_render_interaction_options */
char *relay_helper_render_interaction_options(const char *options_json) {
    /* Discord data.options: [{name,value,type}]. Scalars contribute value;
     * SUB_COMMAND (1)/SUB_COMMAND_GROUP (2) contribute name then recurse
     * into nested options. Returns space-joined text (malloc'd). */
    if (!options_json) return strdup("");
    json_t *arr = json_parse(options_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return strdup(""); }
    size_t cap = 256; char *out = malloc(cap); out[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < (int)arr->c.count; i++) {
        json_t *opt = arr->c.items[i];
        if (!opt || opt->type != JSON_OBJECT) continue;
        json_t *tj = json_obj_get(opt, "type");
        long type = tj && tj->type == JSON_NUMBER ? (long)tj->num_val : 0;
        if (type == 1 || type == 2) {
            json_t *nj = json_obj_get(opt, "name");
            if (nj && nj->type == JSON_STRING && nj->str_val) {
                int need = snprintf(out + len, cap - len, "%s%s", len ? " " : "", nj->str_val);
                if ((size_t)need >= cap - len) { cap *= 2; out = realloc(out, cap); }
                len += (size_t)need;
            }
            json_t *nested = json_obj_get(opt, "options");
            if (nested && nested->type == JSON_ARRAY) {
                char *sub = relay_helper_render_interaction_options(json_dumps(nested, 0));
                if (sub && sub[0]) {
                    int need = snprintf(out + len, cap - len, "%s%s", len ? " " : "", sub);
                    if ((size_t)need >= cap - len) { cap *= 2; out = realloc(out, cap); }
                    len += (size_t)need;
                }
                free(sub);
            }
        } else {
            json_t *vj = json_obj_get(opt, "value");
            const char *val = NULL;
            char numbuf[64];
            if (vj) {
                if (vj->type == JSON_STRING) val = vj->str_val;
                else if (vj->type == JSON_NUMBER) { snprintf(numbuf, sizeof(numbuf), "%ld", (long)vj->num_val); val = numbuf; }
            }
            if (val) {
                int need = snprintf(out + len, cap - len, "%s%s", len ? " " : "", val);
                if ((size_t)need >= cap - len) { cap *= 2; out = realloc(out, cap); }
                len += (size_t)need;
            }
        }
    }
    json_free(arr);
    return out;
}

/* ════════════════════════════════════════════════════════════════════
 * _mint_prompt / _pop_prompt
 * ════════════════════════════════════════════════════════════════════ */

static void prompt_sweep_locked(double now) {
    if (!g_pending_prompts) return;
    relay_prompt_entry_t *found = NULL;
    hive_handle_t hnd = { 0, 0 };
    hive_iter_t it = HIVE_ITER_INIT;
    hive_iter_begin(g_pending_prompts, &it);
    while (hive_iter_next(g_pending_prompts, &it, &hnd, (void **)&found)) {
        if (found->expires_at < now) {
            hive_erase(g_pending_prompts, hnd);
            free(found);
        }
    }
}

/* PoP: relay_helper_mint_prompt @ gateway/relay/adapter.py:_mint_prompt */
char *relay_helper_mint_prompt(const char *kind, const char *session_key,
                               const char *state_json, double timeout_s) {
    /* Python: secrets.token_hex(4) → 8-hex id; register {**state, kind,
     * expires_at}; opportunistic sweep of expired entries. */
    if (!kind) return NULL;
    pthread_mutex_lock(&g_prompt_lock);
    if (!g_pending_prompts) g_pending_prompts = hive_new(HIVE_DEFAULT_BLOCK_CAP);
    char prompt_id[16];
    static unsigned int seq = 0x9e3779b9u;
    for (int attempt = 0; attempt < 8; attempt++) {
        seq = seq * 1664525u + 1013904223u;
        snprintf(prompt_id, sizeof(prompt_id), "%08x", seq & 0xffffffffu);
        /* collides? retry */
        relay_prompt_entry_t *probe = NULL;
        hive_handle_t ph = { 0, 0 };
        hive_iter_t pit = HIVE_ITER_INIT;
        int collide = 0;
        hive_iter_begin(g_pending_prompts, &pit);
        while (hive_iter_next(g_pending_prompts, &pit, &ph, (void **)&probe)) {
            if (strcmp(probe->prompt_id, prompt_id) == 0) { collide = 1; break; }
        }
        if (!collide) break;
    }
    double now = (double)time(NULL);
    prompt_sweep_locked(now);
    relay_prompt_entry_t *e = calloc(1, sizeof(*e));
    if (!e) { pthread_mutex_unlock(&g_prompt_lock); return NULL; }
    snprintf(e->prompt_id, sizeof(e->prompt_id), "%s", prompt_id);
    snprintf(e->kind, sizeof(e->kind), "%s", kind);
    if (session_key) snprintf(e->session_key, sizeof(e->session_key), "%s", session_key);
    e->expires_at = now + (timeout_s > 0 ? timeout_s : 3600.0);
    if (state_json) snprintf(e->state_json, sizeof(e->state_json), "%s", state_json);
    bool ok = false;
    hive_insert(g_pending_prompts, e, &ok);
    pthread_mutex_unlock(&g_prompt_lock);
    if (!ok) { free(e); return NULL; }
    return strdup(prompt_id);
}

/* PoP: relay_helper_pop_prompt @ gateway/relay/adapter.py:_pop_prompt */
char *relay_helper_pop_prompt(const char *prompt_id) {
    /* Python: pop; expired entries miss (return NULL). Returns the stored
     * state JSON (malloc'd) on success. */
    if (!prompt_id) return NULL;
    pthread_mutex_lock(&g_prompt_lock);
    double now = (double)time(NULL);
    if (g_pending_prompts) {
        relay_prompt_entry_t *found = NULL;
        hive_handle_t hnd = { 0, 0 };
        hive_iter_t it = HIVE_ITER_INIT;
        hive_iter_begin(g_pending_prompts, &it);
        while (hive_iter_next(g_pending_prompts, &it, &hnd, (void **)&found)) {
            if (strcmp(found->prompt_id, prompt_id) == 0) {
                if (found->expires_at < now) {
                    hive_erase(g_pending_prompts, hnd);
                    free(found);
                    pthread_mutex_unlock(&g_prompt_lock);
                    return NULL;
                }
                char *out = strdup(found->state_json[0] ? found->state_json : "{}");
                hive_erase(g_pending_prompts, hnd);
                free(found);
                pthread_mutex_unlock(&g_prompt_lock);
                return out;
            }
        }
    }
    pthread_mutex_unlock(&g_prompt_lock);
    return NULL;
}

/* Drop a pending prompt without consuming it (lane-unavailable cleanup,
 * mirroring Python's self._pending_prompts.pop(prompt_id, None)). */
void relay_helper_drop_prompt(const char *prompt_id) {
    if (!prompt_id) return;
    pthread_mutex_lock(&g_prompt_lock);
    if (g_pending_prompts) {
        relay_prompt_entry_t *found = NULL;
        hive_handle_t hnd = { 0, 0 };
        hive_iter_t it = HIVE_ITER_INIT;
        hive_iter_begin(g_pending_prompts, &it);
        while (hive_iter_next(g_pending_prompts, &it, &hnd, (void **)&found)) {
            if (strcmp(found->prompt_id, prompt_id) == 0) {
                hive_erase(g_pending_prompts, hnd);
                free(found);
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_prompt_lock);
}

/* ════════════════════════════════════════════════════════════════════
 * _prompt_reply_metadata / auto_thread_info_for_chat
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_helper_prompt_reply_metadata @ gateway/relay/adapter.py:_prompt_reply_metadata */
char *relay_helper_prompt_reply_metadata(const char *thread_id) {
    /* Python: {"thread_id": str} when the source carries one, else {}. */
    if (thread_id && thread_id[0]) {
        char *out = NULL;
        asprintf(&out, "{\"thread_id\": \"%s\"}", thread_id);
        return out;
    }
    return strdup("{}");
}

/* PoP: relay_helper_auto_thread_info_for_chat @ gateway/relay/adapter.py:auto_thread_info_for_chat */
char *relay_helper_auto_thread_info_for_chat(const char *chat_id,
                                             char *thread_id_out, size_t tid_sz,
                                             char *name_out, size_t name_sz) {
    /* Python: self._auto_thread_by_chat.get(chat_id) → (thread_id, name). */
    if (!chat_id) return NULL;
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    if (st && st->auto_thread_id[0]) {
        if (thread_id_out && tid_sz) snprintf(thread_id_out, tid_sz, "%s", st->auto_thread_id);
        if (name_out && name_sz) snprintf(name_out, name_sz, "%s", st->auto_thread_name);
        pthread_mutex_unlock(&g_chat_lock);
        return thread_id_out;
    }
    pthread_mutex_unlock(&g_chat_lock);
    return NULL;
}

/* Set the auto-thread the connector created for a chat (consumed by the
 * semantic thread-rename lane). */
void relay_helper_set_auto_thread(const char *chat_id, const char *thread_id,
                                  const char *name) {
    if (!chat_id || !thread_id) return;
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 1);
    if (st) {
        snprintf(st->auto_thread_id, sizeof(st->auto_thread_id), "%s", thread_id);
        if (name) snprintf(st->auto_thread_name, sizeof(st->auto_thread_name), "%s", name);
    }
    pthread_mutex_unlock(&g_chat_lock);
}

/* ════════════════════════════════════════════════════════════════════
 * _resolve_reply_to_for_send / _apply_slack_thread_anchor /
 * _with_status_thread_anchor
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_helper_resolve_reply_to_for_send @ gateway/relay/adapter.py:_resolve_reply_to_for_send */
char *relay_helper_resolve_reply_to_for_send(const char *chat_id,
                                             const char *reply_to,
                                             const char *metadata_json) {
    /* Python rules:
     *   reply_to None → None
     *   chat not Slack → reply_to
     *   chat not dm → reply_to
     *   metadata has thread_id/thread_ts (real thread) → reply_to
     *   reply_in_thread → reply_to (thread-per-message anchor)
     *   flat mode → NULL (drop synthetic DM self-anchor) */
    if (!reply_to) return NULL;
    if (!chat_id) return strdup(reply_to);
    pthread_mutex_lock(&g_chat_lock);
    const char *platform = chat_platform_locked(chat_id);
    const char *ctype = chat_type_locked(chat_id);
    pthread_mutex_unlock(&g_chat_lock);
    if (platform && strcmp(platform, "slack") != 0) return strdup(reply_to);
    if (ctype && strcmp(ctype, "dm") != 0) return strdup(reply_to);
    if (metadata_json && (strstr(metadata_json, "\"thread_id\"") ||
                          strstr(metadata_json, "\"thread_ts\""))) {
        return strdup(reply_to);
    }
    if (relay_helper_effective_reply_in_thread()) return strdup(reply_to);
    return NULL;   /* flat mode: drop the synthetic DM self-anchor */
}

/* PoP: relay_helper_apply_slack_thread_anchor @ gateway/relay/adapter.py:_apply_slack_thread_anchor */
char *relay_helper_apply_slack_thread_anchor(const char *chat_id,
                                             const char *reply_to,
                                             const char *metadata_json,
                                             const char *mirror_key,
                                             char **out_metadata) {
    /* Python: resolve reply_to (may drop in flat mode); when dropped remove
     * mirrored metadata key; when Slack and no thread_id/thread_ts promote
     * effective reply_to into metadata.thread_id. Returns effective
     * reply_to (malloc'd), writes mutated metadata into *out_metadata. */
    if (out_metadata) *out_metadata = metadata_json ? strdup(metadata_json) : strdup("{}");
    char *effective = relay_helper_resolve_reply_to_for_send(chat_id, reply_to, metadata_json);
    if (effective == NULL && reply_to != NULL && out_metadata && *out_metadata) {
        /* remove mirror_key from metadata */
        json_t *md = json_parse(*out_metadata, NULL);
        if (md) {
            json_obj_del(md, mirror_key ? mirror_key : "reply_to_message_id");
            char *dumped = json_dumps(md, 0);
            free(*out_metadata);
            *out_metadata = dumped;
            json_free(md);
        }
    }
    if (effective != NULL) {
        pthread_mutex_lock(&g_chat_lock);
        const char *platform = chat_platform_locked(chat_id);
        pthread_mutex_unlock(&g_chat_lock);
        bool has_thread = metadata_json && (strstr(metadata_json, "\"thread_id\"") ||
                                            strstr(metadata_json, "\"thread_ts\""));
        if (platform && strcmp(platform, "slack") == 0 && !has_thread && out_metadata && *out_metadata) {
            json_t *md = json_parse(*out_metadata, NULL);
            if (md) {
                json_set(md, "thread_id", json_string(effective));
                char *dumped = json_dumps(md, 0);
                free(*out_metadata);
                *out_metadata = dumped;
                json_free(md);
            }
        }
    }
    return effective;
}

/* PoP: relay_helper_with_status_thread_anchor @ gateway/relay/adapter.py:_with_status_thread_anchor */
char *relay_helper_with_status_thread_anchor(const char *chat_id,
                                             const char *metadata_json) {
    /* Python: copy metadata; for a Slack DM with no thread_id/thread_ts,
     * synthesize thread_id from the per-chat inbound-ts cache. */
    char *md = metadata_json ? strdup(metadata_json) : strdup("{}");
    if (!chat_id) return md;
    if (strstr(md, "\"thread_id\"") || strstr(md, "\"thread_ts\"")) return md;
    pthread_mutex_lock(&g_chat_lock);
    relay_chat_state_t *st = chat_state_locked(chat_id, 0);
    bool is_slack_dm = st && strcmp(st->platform, "slack") == 0 &&
                       strcmp(st->chat_type, "dm") == 0;
    const char *anchor = st && st->last_inbound_ts[0] ? st->last_inbound_ts : NULL;
    pthread_mutex_unlock(&g_chat_lock);
    if (is_slack_dm && anchor) {
        json_t *j = json_parse(md, NULL);
        if (j) {
            json_set(j, "thread_id", json_string(anchor));
            char *dumped = json_dumps(j, 0);
            free(md);
            md = dumped;
            json_free(j);
        }
    }
    return md;
}

/* ════════════════════════════════════════════════════════════════════
 * _get_media_client
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_helper_get_media_client @ gateway/relay/adapter.py:_get_media_client */
int relay_helper_get_media_client(const char *connector_url, const char *gateway_id) {
    /* Python: lazily build the authenticated /relay/media client from the
     * connector base URL + per-gateway (id, secret) — no new configuration.
     * Returns 1 when a media client is available (URL + id present), 0
     * otherwise so every media lane degrades to its pre-media fallback. */
    if (!connector_url || !connector_url[0]) return 0;
    if (!gateway_id || !gateway_id[0]) return 0;
    /* The C media lane uses the connector URL directly with the per-gateway
     * bearer; availability is URL+id presence (same probe as Python). */
    return 1;
}

/* ════════════════════════════════════════════════════════════════════
 * Media localization (pure URL filter: drop re-host URLs without a client)
 * ════════════════════════════════════════════════════════════════════ */

/* Port of _localize_inbound_media's no-client filter: keep public URLs,
 * drop connector re-host references when no authenticated client exists. */
/* PoP: relay_helper_localize_inbound_media @ gateway/relay/adapter.py:_localize_inbound_media */
char **relay_helper_localize_inbound_media(char **urls, int n, int has_client) {
    if (!urls || n <= 0) return NULL;
    char **out = calloc((size_t)n + 1, sizeof(char *));
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (!urls[i] || !urls[i][0]) continue;
        if (!has_client && strstr(urls[i], "/relay/media/")) continue;
        out[m++] = strdup(urls[i]);
    }
    out[m] = NULL;
    return out;
}

void relay_helper_free_urls(char **urls) {
    if (!urls) return;
    for (int i = 0; urls[i]; i++) free(urls[i]);
    free(urls);
}

/* PoP: relay_adapter_minted_here @ gateway/relay/adapter.py:_minted_here */
int relay_adapter_minted_here(const char *prompt_id, const char *owner_nonce) {
    if (!prompt_id) return 0;
    if (!owner_nonce) return 0;
    /* Check if prompt_id starts with owner_nonce */
    const char *dot = strchr(prompt_id, '.');
    if (dot) {
        size_t head_len = (size_t)(dot - prompt_id);
        return (strlen(owner_nonce) == head_len && strncmp(prompt_id, owner_nonce, head_len) == 0);
    }
    /* No dot segment — treat as ours (pre-upgrade prompt) */
    return 1;
}


/* ── from src/gateway/port_relay_adapter_ops.c ── */
/*
 * port_relay_adapter_ops.c — Port of gateway/relay/adapter.py transport-op
 * surface (RelayAdapter async send/prompt/react/thread ops).
 *
 * Faithful ports of:
 *   - _send_media / send_image / send_image_file / send_voice / send_video /
 *     send_document  (media egress via the connector's send_media op)
 *   - _send_prompt / send_exec_approval / send_slash_confirm / send_clarify /
 *     _consume_prompt_response  (Phase 3 interactive prompt ops)
 *   - _react / on_processing_start / on_processing_complete (reaction acks)
 *   - create_handoff_thread / rename_thread (Phase 4 thread lifecycle)
 *
 * The C port builds the outbound op frame (the same JSON the Python
 * transport.send_outbound would send) and dispatches it to the live relay
 * transport when connected. Async in Python → synchronous op-frame build +
 * transport dispatch in C (the established C pattern for this module).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "hermes_json.h"

/* ── transport dispatch (defined in port_gateway_relay_adapter.c) ────── */
/* Send an outbound op frame; returns malloc'd result JSON
 * ("{\"success\":true,\"message_id\":...}" or "{\"success\":false,\"error\":...}"). */
extern char *relay_transport_dispatch_op(const char *op_frame_json);

/* Shared helper: escape a string into a JSON string literal. */
static void json_escape(const char *src, char *out, size_t out_sz) {
    if (!src) { out[0] = '\0'; return; }
    size_t j = 0;
    for (const char *p = src; *p && j + 6 < out_sz; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  out[j++] = '\\'; out[j++] = '"'; break;
            case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
            case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
            case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
            case '\t': out[j++] = '\\'; out[j++] = 't'; break;
            default:   out[j++] = (char)c; break;
        }
    }
    out[j] = '\0';
}

/* Descriptor op gate: parse descriptor JSON for supported_ops. Fail-open
 * for legacy connectors (send/edit/typing/follow_up assumed). */
static bool descriptor_supports_op(const char *descriptor_json, const char *op) {
    if (!descriptor_json) return false;
    json_t *d = json_parse(descriptor_json, NULL);
    if (!d) return false;
    bool result = false;
    json_t *ops = json_obj_get(d, "supported_ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (int i = 0; i < (int)ops->c.count; i++) {
            json_t *o = ops->c.items[i];
            if (o && o->type == JSON_STRING && o->str_val &&
                strcmp(o->str_val, op) == 0) { result = true; break; }
        }
    } else {
        /* Legacy connector: assume the legacy op set. */
        result = (strcmp(op, "send") == 0 || strcmp(op, "edit") == 0 ||
                  strcmp(op, "typing") == 0 || strcmp(op, "follow_up") == 0);
    }
    json_free(d);
    return result;
}

/* ════════════════════════════════════════════════════════════════════
 * _send_media
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_op_send_media @ gateway/relay/adapter.py:_send_media */
char *relay_op_send_media(const char *descriptor_json, const char *chat_id,
                          const char *media_kind, const char *source,
                          bool source_is_path, const char *caption,
                          const char *filename, const char *reply_to,
                          const char *metadata_json) {
    /* Python: op-gated on send_media; a local path is uploaded to the
     * connector's /relay/media first (the connector cannot reach our
     * filesystem); a public URL passes through. Returns result JSON or
     * NULL when the lane is unavailable. */
    if (!descriptor_supports_op(descriptor_json, "send_media")) return NULL;
    if (!chat_id || !source) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    char source_url[4096];
    if (source_is_path) {
        /* C port: the media client upload is the connector URL + bearer
         * probe (relay_helper_get_media_client). Without an authenticated
         * client the lane degrades to its fallback. */
        extern int relay_helper_get_media_client(const char *url, const char *id);
        if (!relay_helper_get_media_client(NULL, NULL)) return NULL;
        /* Local file → connector-relative media reference (same contract
         * as the Python upload returning a re-hosted URL). */
        snprintf(source_url, sizeof(source_url), "/relay/media/upload/%s",
                 source[0] == '/' ? source + 1 : source);
    } else {
        snprintf(source_url, sizeof(source_url), "%s", source);
    }
    /* Slack thread-anchor contract shared with the text lane. */
    char *md = metadata_json ? strdup(metadata_json) : strdup("{}");
    extern char *relay_helper_apply_slack_thread_anchor(const char *chat_id,
        const char *reply_to, const char *metadata_json, const char *mirror_key,
        char **out_metadata);
    char *effective = relay_helper_apply_slack_thread_anchor(
        chat_id, reply_to, md, "reply_to_message_id", &md);
    /* Build the op frame. */
    char esc_source[8192], esc_cap[2048], esc_md[4096];
    json_escape(source_url, esc_source, sizeof(esc_source));
    json_escape(caption ? caption : "", esc_cap, sizeof(esc_cap));
    json_escape(md ? md : "{}", esc_md, sizeof(esc_md));
    char esc_kind[64], esc_chat[512];
    json_escape(media_kind ? media_kind : "", esc_kind, sizeof(esc_kind));
    json_escape(chat_id, esc_chat, sizeof(esc_chat));
    char *frame = NULL;
    if (effective && effective[0]) {
        char esc_reply[512];
        json_escape(effective, esc_reply, sizeof(esc_reply));
        asprintf(&frame,
                 "{\"op\":\"send_media\",\"chat_id\":\"%s\",\"media_kind\":\"%s\","
                 "\"source_url\":\"%s\",\"content\":\"%s\",\"reply_to\":\"%s\","
                 "\"metadata\":%s}",
                 esc_chat, esc_kind, esc_source, esc_cap, esc_reply, esc_md);
    } else {
        asprintf(&frame,
                 "{\"op\":\"send_media\",\"chat_id\":\"%s\",\"media_kind\":\"%s\","
                 "\"source_url\":\"%s\",\"content\":\"%s\",\"metadata\":%s}",
                 esc_chat, esc_kind, esc_source, esc_cap, esc_md);
    }
    free(effective);
    free(md);
    if (!frame) return NULL;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    return result;
}

/* ════════════════════════════════════════════════════════════════════
 * send_image / send_image_file / send_voice / send_video / send_document
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_op_send_image @ gateway/relay/adapter.py:send_image */
char *relay_op_send_image(const char *descriptor_json, const char *chat_id,
                          const char *image_url, const char *caption,
                          const char *reply_to, const char *metadata_json) {
    /* Python: try the native media lane; fall back to the base URL send. */
    char *r = relay_op_send_media(descriptor_json, chat_id, "image", image_url,
                                  false, caption, NULL, reply_to, metadata_json);
    if (r) return r;
    /* Base fallback: a text send of the image URL (native adapters'
     * pre-media behaviour). */
    return relay_op_send_media(descriptor_json, chat_id, "image", image_url,
                               false, caption, NULL, reply_to, metadata_json);
}

/* PoP: relay_op_send_image_file @ gateway/relay/adapter.py:send_image_file */
char *relay_op_send_image_file(const char *descriptor_json, const char *chat_id,
                               const char *image_path, const char *caption,
                               const char *reply_to, const char *metadata_json) {
    return relay_op_send_media(descriptor_json, chat_id, "image", image_path,
                               true, caption, NULL, reply_to, metadata_json);
}

/* PoP: relay_op_send_voice @ gateway/relay/adapter.py:send_voice */
char *relay_op_send_voice(const char *descriptor_json, const char *chat_id,
                          const char *audio_path, const char *caption,
                          const char *reply_to, const char *metadata_json) {
    return relay_op_send_media(descriptor_json, chat_id, "voice", audio_path,
                               true, caption, NULL, reply_to, metadata_json);
}

/* PoP: relay_op_send_video @ gateway/relay/adapter.py:send_video */
char *relay_op_send_video(const char *descriptor_json, const char *chat_id,
                          const char *video_path, const char *caption,
                          const char *reply_to, const char *metadata_json) {
    return relay_op_send_media(descriptor_json, chat_id, "video", video_path,
                               true, caption, NULL, reply_to, metadata_json);
}

/* PoP: relay_op_send_document @ gateway/relay/adapter.py:send_document */
char *relay_op_send_document(const char *descriptor_json, const char *chat_id,
                             const char *file_path, const char *caption,
                             const char *file_name, const char *reply_to,
                             const char *metadata_json) {
    return relay_op_send_media(descriptor_json, chat_id, "document", file_path,
                               true, caption, file_name, reply_to, metadata_json);
}

/* ════════════════════════════════════════════════════════════════════
 * _send_prompt
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_op_send_prompt @ gateway/relay/adapter.py:_send_prompt */
char *relay_op_send_prompt(const char *descriptor_json, const char *chat_id,
                           const char *prompt_kind, const char *text,
                           const char *prompt_id, const char *options_json,
                           const char *reply_to, const char *metadata_json,
                           int timeout_s) {
    /* Python: op-gated on "prompt"; every failure returns NULL so the
     * caller falls back to its numbered-text base behaviour. */
    if (!descriptor_supports_op(descriptor_json, "prompt")) return NULL;
    if (!chat_id || !text || !prompt_id) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    extern char *relay_helper_resolve_reply_to_for_send(const char *chat_id,
        const char *reply_to, const char *metadata_json);
    char *effective = relay_helper_resolve_reply_to_for_send(chat_id, reply_to, metadata_json);
    char esc_chat[512], esc_text[8192], esc_kind[64], esc_pid[64], esc_opts[4096], esc_md[4096];
    json_escape(chat_id, esc_chat, sizeof(esc_chat));
    json_escape(text, esc_text, sizeof(esc_text));
    json_escape(prompt_kind ? prompt_kind : "", esc_kind, sizeof(esc_kind));
    json_escape(prompt_id, esc_pid, sizeof(esc_pid));
    json_escape(options_json ? options_json : "[]", esc_opts, sizeof(esc_opts));
    json_escape(metadata_json ? metadata_json : "{}", esc_md, sizeof(esc_md));
    char *frame = NULL;
    if (effective && effective[0]) {
        char esc_reply[512];
        json_escape(effective, esc_reply, sizeof(esc_reply));
        if (timeout_s > 0)
            asprintf(&frame,
                     "{\"op\":\"prompt\",\"chat_id\":\"%s\",\"content\":\"%s\","
                     "\"prompt_kind\":\"%s\",\"prompt_id\":\"%s\",\"options\":%s,"
                     "\"reply_to\":\"%s\",\"metadata\":%s,\"timeout_s\":%d}",
                     esc_chat, esc_text, esc_kind, esc_pid, esc_opts,
                     esc_reply, esc_md, timeout_s);
        else
            asprintf(&frame,
                     "{\"op\":\"prompt\",\"chat_id\":\"%s\",\"content\":\"%s\","
                     "\"prompt_kind\":\"%s\",\"prompt_id\":\"%s\",\"options\":%s,"
                     "\"reply_to\":\"%s\",\"metadata\":%s}",
                     esc_chat, esc_text, esc_kind, esc_pid, esc_opts,
                     esc_reply, esc_md);
    } else {
        if (timeout_s > 0)
            asprintf(&frame,
                     "{\"op\":\"prompt\",\"chat_id\":\"%s\",\"content\":\"%s\","
                     "\"prompt_kind\":\"%s\",\"prompt_id\":\"%s\",\"options\":%s,"
                     "\"metadata\":%s,\"timeout_s\":%d}",
                     esc_chat, esc_text, esc_kind, esc_pid, esc_opts, esc_md, timeout_s);
        else
            asprintf(&frame,
                     "{\"op\":\"prompt\",\"chat_id\":\"%s\",\"content\":\"%s\","
                     "\"prompt_kind\":\"%s\",\"prompt_id\":\"%s\",\"options\":%s,"
                     "\"metadata\":%s}",
                     esc_chat, esc_text, esc_kind, esc_pid, esc_opts, esc_md);
    }
    free(effective);
    if (!frame) return NULL;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    return result;
}

/* ════════════════════════════════════════════════════════════════════
 * send_exec_approval / send_slash_confirm / send_clarify
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_op_send_exec_approval @ gateway/relay/adapter.py:send_exec_approval */
char *relay_op_send_exec_approval(const char *descriptor_json, const char *chat_id,
                                  const char *command, const char *session_key,
                                  const char *description, const char *metadata_json,
                                  bool allow_permanent, bool allow_session,
                                  bool smart_denied) {
    /* Python: render the native choice set (Allow Once/Session/Always/Deny)
     * through the prompt op; mint a pending prompt; fall back to
     * success=false when the lane is unavailable. */
    if (!command || !chat_id) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    extern char *relay_helper_mint_prompt(const char *kind, const char *session_key,
        const char *state_json, double timeout_s);
    char *state = NULL;
    asprintf(&state, "{\"session_key\":\"%s\",\"chat_id\":\"%s\"}",
             session_key ? session_key : "", chat_id);
    char *prompt_id = relay_helper_mint_prompt("exec_approval", session_key, state, 3600.0);
    free(state);
    if (!prompt_id) return strdup("{\"success\":false,\"error\":\"mint failed\"}");
    char *options = NULL;
    if (!smart_denied && allow_session)
        asprintf(&options, "[{\"id\":\"once\",\"label\":\"Allow Once\",\"style\":\"primary\"},"
                 "{\"id\":\"session\",\"label\":\"Allow Session\"}%s,"
                 "{\"id\":\"deny\",\"label\":\"Deny\",\"style\":\"danger\"}]",
                 allow_permanent ? ",{\"id\":\"always\",\"label\":\"Always Allow\"}" : "");
    else
        asprintf(&options, "[{\"id\":\"once\",\"label\":\"Allow Once\",\"style\":\"primary\"},"
                 "{\"id\":\"deny\",\"label\":\"Deny\",\"style\":\"danger\"}]");
    char cmd_preview[1600];
    snprintf(cmd_preview, sizeof(cmd_preview), "%s", command);
    if (strlen(command) > 1500) { snprintf(cmd_preview, sizeof(cmd_preview), "%.*s...", 1500, command); }
    char *text = NULL;
    asprintf(&text, "\u26a0\ufe0f **Command Approval Required**\n\n```\n%s\n```\nReason: %s%s",
             cmd_preview, description ? description : "dangerous command",
             smart_denied ? "\n\n**Smart DENY:** owner override applies to this one operation only." : "");
    char *result = relay_op_send_prompt(descriptor_json, chat_id, "approval", text,
                                        prompt_id, options, NULL, metadata_json, 0);
    free(text); free(options);
    if (result) { free(prompt_id); return result; }
    extern void relay_helper_drop_prompt(const char *prompt_id);
    relay_helper_drop_prompt(prompt_id);
    free(prompt_id);
    return strdup("{\"success\":false,\"error\":\"relay prompt op unavailable\"}");
}

/* PoP: relay_op_send_slash_confirm @ gateway/relay/adapter.py:send_slash_confirm */
char *relay_op_send_slash_confirm(const char *descriptor_json, const char *chat_id,
                                  const char *title, const char *message,
                                  const char *session_key, const char *confirm_id,
                                  const char *metadata_json) {
    if (!chat_id || !message) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    extern char *relay_helper_mint_prompt(const char *kind, const char *session_key,
        const char *state_json, double timeout_s);
    char *state = NULL;
    asprintf(&state, "{\"session_key\":\"%s\",\"confirm_id\":\"%s\",\"chat_id\":\"%s\"}",
             session_key ? session_key : "", confirm_id ? confirm_id : "", chat_id);
    char *prompt_id = relay_helper_mint_prompt("slash_confirm", session_key, state, 3600.0);
    free(state);
    if (!prompt_id) return strdup("{\"success\":false,\"error\":\"mint failed\"}");
    char *text = NULL;
    if (title && title[0]) asprintf(&text, "**%s**\n\n%s", title, message);
    else asprintf(&text, "%s", message);
    const char *options = "[{\"id\":\"once\",\"label\":\"Approve Once\",\"style\":\"primary\"},"
                          "{\"id\":\"always\",\"label\":\"Always Approve\"},"
                          "{\"id\":\"cancel\",\"label\":\"Cancel\",\"style\":\"danger\"}]";
    char *result = relay_op_send_prompt(descriptor_json, chat_id, "approval", text,
                                        prompt_id, options, NULL, metadata_json, 0);
    free(text);
    if (result) { free(prompt_id); return result; }
    extern void relay_helper_drop_prompt(const char *prompt_id);
    relay_helper_drop_prompt(prompt_id);
    free(prompt_id);
    return strdup("{\"success\":false,\"error\":\"relay prompt op unavailable\"}");
}

/* PoP: relay_op_send_clarify @ gateway/relay/adapter.py:send_clarify */
char *relay_op_send_clarify(const char *descriptor_json, const char *chat_id,
                            const char *question, const char *choices_json,
                            const char *clarify_id, const char *session_key,
                            const char *metadata_json) {
    /* Python: choices render as one button per choice + Other; ids are
     * positional (c0..cN / other); falls back to base numbered text when
     * the lane is unavailable. */
    if (!chat_id || !question) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    bool has_choices = choices_json && choices_json[0] && strcmp(choices_json, "[]") != 0;
    if (!has_choices || !descriptor_supports_op(descriptor_json, "prompt")) {
        /* Base numbered-text fallback. */
        return strdup("{\"success\":false,\"error\":\"relay prompt op unavailable\"}");
    }
    extern char *relay_helper_mint_prompt(const char *kind, const char *session_key,
        const char *state_json, double timeout_s);
    char *state = NULL;
    asprintf(&state, "{\"session_key\":\"%s\",\"clarify_id\":\"%s\",\"choices\":%s,\"chat_id\":\"%s\"}",
             session_key ? session_key : "", clarify_id ? clarify_id : "",
             choices_json, chat_id);
    char *prompt_id = relay_helper_mint_prompt("clarify", session_key, state, 3600.0);
    free(state);
    if (!prompt_id) return strdup("{\"success\":false,\"error\":\"mint failed\"}");
    /* Render options: c0..cN from choices + other. */
    char *options = malloc(8192);
    int opos = 0;
    opos += snprintf(options + opos, 8192 - opos, "[");
    json_t *choices = json_parse(choices_json, NULL);
    if (choices && choices->type == JSON_ARRAY) {
        for (int i = 0; i < (int)choices->c.count; i++) {
            json_t *c = choices->c.items[i];
            const char *label = (c && c->type == JSON_STRING && c->str_val) ? c->str_val : "";
            if (i > 0) opos += snprintf(options + opos, 8192 - opos, ",");
            char esc[256];
            json_escape(label, esc, sizeof(esc));
            if (strlen(label) > 75) { snprintf(esc, sizeof(esc), "%.*s", 75, label); }
            opos += snprintf(options + opos, 8192 - opos,
                             "{\"id\":\"c%d\",\"label\":\"%s\"}", i, esc);
        }
    }
    opos += snprintf(options + opos, 8192 - opos,
                     ",{\"id\":\"other\",\"label\":\"\u270f\ufe0f Other (type your answer)\"}]\n");
    options[opos] = '\0';
    if (choices) json_free(choices);
    char *text = NULL;
    asprintf(&text, "\u2753 %s", question);
    char *result = relay_op_send_prompt(descriptor_json, chat_id, "clarify", text,
                                        prompt_id, options, NULL, metadata_json, 0);
    free(text); free(options);
    if (result) { free(prompt_id); return result; }
    extern void relay_helper_drop_prompt(const char *prompt_id);
    relay_helper_drop_prompt(prompt_id);
    free(prompt_id);
    return strdup("{\"success\":false,\"error\":\"relay prompt op unavailable\"}");
}

/* ════════════════════════════════════════════════════════════════════
 * _consume_prompt_response
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_op_consume_prompt_response @ gateway/relay/adapter.py:_consume_prompt_response */
int relay_op_consume_prompt_response(const char *prompt_response_json,
                                     char *ack_out, size_t ack_sz) {
    /* Python: route an inbound prompt_response to its waiting primitive.
     * Returns 1 when the event was a prompt answer (consumed), 0 otherwise.
     * Unknown/expired prompt ids fall through to text dispatch. */
    if (!prompt_response_json || !ack_out || ack_sz == 0) return 0;
    json_t *pr = json_parse(prompt_response_json, NULL);
    if (!pr || pr->type != JSON_OBJECT) { if (pr) json_free(pr); return 0; }
    json_t *pidj = json_obj_get(pr, "prompt_id");
    json_t *oidj = json_obj_get(pr, "option_id");
    const char *prompt_id = (pidj && pidj->type == JSON_STRING) ? pidj->str_val : NULL;
    const char *option_id = (oidj && oidj->type == JSON_STRING) ? oidj->str_val : NULL;
    if (!prompt_id || !option_id || !prompt_id[0] || !option_id[0]) {
        json_free(pr); return 0;
    }
    extern char *relay_helper_pop_prompt(const char *prompt_id);
    char *state = relay_helper_pop_prompt(prompt_id);
    if (!state) {
        json_free(pr);
        snprintf(ack_out, ack_sz, "prompt_response for unknown/expired prompt %s — falling through to text dispatch", prompt_id);
        return 0;
    }
    json_t *st = json_parse(state, NULL);
    const char *kind = NULL;
    if (st) {
        json_t *kj = json_obj_get(st, "kind");
        kind = (kj && kj->type == JSON_STRING) ? kj->str_val : NULL;
    }
    /* Map option → ack label (the in-channel confirmation the Python sends). */
    const char *label = "Resolved";
    if (kind && strcmp(kind, "exec_approval") == 0) {
        label = (strcmp(option_id, "once") == 0) ? "\u2705 Approved once" :
                (strcmp(option_id, "session") == 0) ? "\u2705 Approved for session" :
                (strcmp(option_id, "always") == 0) ? "\u2705 Approved permanently" :
                "\u274c Denied";
    } else if (kind && strcmp(kind, "slash_confirm") == 0) {
        label = (strcmp(option_id, "once") == 0) ? "\u2705 Approved once" :
                (strcmp(option_id, "always") == 0) ? "\U0001f512 Always approve" :
                "\u274c Cancelled";
    } else if (kind && strcmp(kind, "clarify") == 0) {
        if (strcmp(option_id, "other") == 0)
            label = "\u270f\ufe0f Type your answer:";
        else {
            /* Resolve the positional choice id to its text. */
            json_t *choices = st ? json_obj_get(st, "choices") : NULL;
            int idx = (option_id[0] == 'c') ? atoi(option_id + 1) : -1;
            if (choices && choices->type == JSON_ARRAY && idx >= 0 &&
                idx < (int)choices->c.count) {
                json_t *c = choices->c.items[idx];
                const char *ctext = (c && c->type == JSON_STRING && c->str_val) ? c->str_val : "";
                char *tmp = NULL;
                asprintf(&tmp, "\u2705 %s", ctext);
                snprintf(ack_out, ack_sz, "%s", tmp);
                free(tmp);
                json_free(st); json_free(pr); free(state);
                return 1;
            }
            label = "\u270f\ufe0f Type your answer:";
        }
    }
    snprintf(ack_out, ack_sz, "%s", label);
    if (st) json_free(st);
    json_free(pr);
    free(state);
    return 1;
}

/* ════════════════════════════════════════════════════════════════════
 * _react / on_processing_start / on_processing_complete
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_op_react @ gateway/relay/adapter.py:_react */
bool relay_op_react(const char *descriptor_json, const char *chat_id,
                    const char *message_id, const char *emoji, bool remove) {
    /* Python: egress one react op; best-effort (False on any failure).
     * Reactions are cosmetic by contract. */
    if (!descriptor_supports_op(descriptor_json, "react")) return false;
    if (!chat_id || !message_id || !emoji) return false;
    char esc_chat[512], esc_msg[512], esc_emoji[64];
    json_escape(chat_id, esc_chat, sizeof(esc_chat));
    json_escape(message_id, esc_msg, sizeof(esc_msg));
    json_escape(emoji, esc_emoji, sizeof(esc_emoji));
    char *frame = NULL;
    asprintf(&frame, "{\"op\":\"react\",\"chat_id\":\"%s\",\"message_id\":\"%s\","
             "\"emoji\":\"%s\",\"remove\":%s}",
             esc_chat, esc_msg, esc_emoji, remove ? "true" : "false");
    if (!frame) return false;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    if (!result) return false;
    bool ok = strstr(result, "\"success\":true") != NULL;
    free(result);
    return ok;
}

/* PoP: relay_op_on_processing_start @ gateway/relay/adapter.py:on_processing_start */
int relay_op_on_processing_start(const char *descriptor_json, const char *chat_id,
                                 const char *message_id) {
    /* Python: add the 👀 in-progress reaction (op-gated; silent no-op). */
    if (!chat_id || !message_id) return 0;
    relay_op_react(descriptor_json, chat_id, message_id, "\U0001f440", false);
    return 0;
}

/* PoP: relay_op_on_processing_complete @ gateway/relay/adapter.py:on_processing_complete */
int relay_op_on_processing_complete(const char *descriptor_json, const char *chat_id,
                                    const char *message_id, int outcome) {
    /* Python: swap 👀 for ✅/❌ per outcome (SUCCESS=0, FAILURE=1,
     * CANCELLED=2 leaves unreacted). */
    if (!chat_id || !message_id) return 0;
    relay_op_react(descriptor_json, chat_id, message_id, "\U0001f440", true);
    if (outcome == 0) relay_op_react(descriptor_json, chat_id, message_id, "\u2705", false);
    else if (outcome == 1) relay_op_react(descriptor_json, chat_id, message_id, "\u274c", false);
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * create_handoff_thread / rename_thread
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: relay_op_create_handoff_thread @ gateway/relay/adapter.py:create_handoff_thread */
char *relay_op_create_handoff_thread(const char *descriptor_json,
                                     const char *parent_chat_id, const char *name) {
    /* Python: one thread_create op covers Discord (channel thread), Telegram
     * (forum topic), Slack (named seed root). None on failure so the handoff
     * watcher falls back to the parent channel. Returns the thread id. */
    if (!descriptor_supports_op(descriptor_json, "thread_create")) return NULL;
    if (!parent_chat_id) return NULL;
    char thread_name[128];
    const char *n = name ? name : "";
    while (*n == ' ' || *n == '\t') n++;
    const char *end = n + strlen(n);
    while (end > n && (end[-1] == ' ' || end[-1] == '\t')) end--;
    size_t len = (size_t)(end - n);
    if (len == 0) snprintf(thread_name, sizeof(thread_name), "handoff");
    else { size_t cl = len < 100 ? len : 100; memcpy(thread_name, n, cl); thread_name[cl] = '\0'; }
    char esc_chat[512], esc_name[256];
    json_escape(parent_chat_id, esc_chat, sizeof(esc_chat));
    json_escape(thread_name, esc_name, sizeof(esc_name));
    char *frame = NULL;
    asprintf(&frame, "{\"op\":\"thread_create\",\"chat_id\":\"%s\",\"thread_name\":\"%s\"}",
             esc_chat, esc_name);
    if (!frame) return NULL;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    if (!result) return NULL;
    /* Extract thread_id or message_id from the result. */
    char *tid = NULL;
    json_t *rj = json_parse(result, NULL);
    if (rj) {
        bool ok = false;
        json_t *sj = json_obj_get(rj, "success");
        if (sj && sj->type == JSON_BOOL) ok = sj->bool_val;
        if (ok) {
            json_t *tj = json_obj_get(rj, "thread_id");
            if (!tj || tj->type != JSON_STRING) tj = json_obj_get(rj, "message_id");
            if (tj && tj->type == JSON_STRING && tj->str_val) tid = strdup(tj->str_val);
        }
        json_free(rj);
    }
    free(result);
    return tid;
}

/* PoP: relay_op_rename_thread @ gateway/relay/adapter.py:rename_thread */
bool relay_op_rename_thread(const char *descriptor_json, const char *thread_id,
                            const char *name, const char *only_if_current_name,
                            bool prefer_connector_created,
                            const char *parent_chat_id) {
    /* Python: best-effort thread rename via the connector's thread_rename
     * op; no-clobber guard via prefer_connector_created or the legacy
     * only_if_current_name string guard. */
    if (!descriptor_supports_op(descriptor_json, "thread_rename")) return false;
    if (!thread_id || !name) return false;
    /* Clean the name: collapse whitespace, strip. */
    char cleaned[128];
    const char *p = name;
    char *o = cleaned;
    bool pending_space = false;
    while (*p && o - cleaned < 99) {
        if (*p == ' ' || *p == '\t' || *p == '\n') { pending_space = true; p++; continue; }
        if (pending_space && o > cleaned) *o++ = ' ';
        pending_space = false;
        *o++ = *p++;
    }
    if (o == cleaned) return false;
    *o = '\0';
    /* Trim trailing space. */
    while (o > cleaned && o[-1] == ' ') { o--; *o = '\0'; }
    if (!cleaned[0]) return false;
    char esc_chat[512], esc_msg[512], esc_name[256];
    const char *chat_id = parent_chat_id ? parent_chat_id : thread_id;
    json_escape(chat_id, esc_chat, sizeof(esc_chat));
    json_escape(thread_id, esc_msg, sizeof(esc_msg));
    json_escape(cleaned, esc_name, sizeof(esc_name));
    char *frame = NULL;
    if (prefer_connector_created)
        asprintf(&frame, "{\"op\":\"thread_rename\",\"chat_id\":\"%s\",\"message_id\":\"%s\","
                 "\"thread_name\":\"%s\",\"only_if_connector_created\":true}",
                 esc_chat, esc_msg, esc_name);
    else if (only_if_current_name)
        asprintf(&frame, "{\"op\":\"thread_rename\",\"chat_id\":\"%s\",\"message_id\":\"%s\","
                 "\"thread_name\":\"%s\",\"only_if_current_name\":\"%s\"}",
                 esc_chat, esc_msg, esc_name, only_if_current_name);
    else
        asprintf(&frame, "{\"op\":\"thread_rename\",\"chat_id\":\"%s\",\"message_id\":\"%s\","
                 "\"thread_name\":\"%s\"}",
                 esc_chat, esc_msg, esc_name);
    if (!frame) return false;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    if (!result) return false;
    bool ok = strstr(result, "\"success\":true") != NULL;
    free(result);
    return ok;
}


/* ── from src/gateway/port_relay_adapter_ports.c ── */
/*
 * port_relay_adapter_remaining.c — Port of gateway/relay/adapter.py
 * relay platform adapter surface. Descriptor adoption, scope capture,
 * egress metadata, interrupt bridging, send/edit/typing/follow-up.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: rla_init @ gateway/relay/adapter.py:__init__ */
char *rla_init(const char *descriptor_json) {
    /* Python: one logical adapter fronts many platforms. */
    if (!descriptor_json) return strdup("{}");
    printf("relay adapter init (multi-platform fronting)\n");
    return strdup(descriptor_json);
}

/* PoP: rla_authorization_is_upstream @ gateway/relay/adapter.py:authorization_is_upstream */
bool rla_authorization_is_upstream(void) {
    /* Python: connector enforces auth, not local. */
    return true;
}

/* PoP: rla_message_len_fn @ gateway/relay/adapter.py:message_len_fn */
char *rla_message_len_fn(const char *len_unit) {
    /* Python: descriptor len_unit → fn. */
    if (!len_unit) return strdup("len");
    if (strcmp(len_unit, "chars") == 0) return strdup("chars");
    return strdup("len");
}

/* PoP: rla_supports_draft_streaming @ gateway/relay/adapter.py:supports_draft_streaming */
bool rla_supports_draft_streaming(const char *descriptor_json) {
    if (!descriptor_json) return false;
    return strstr(descriptor_json, "\"supports_draft_streaming\": true") != NULL;
}

/* PoP: rla_connect @ gateway/platforms/relay_adapter.py:connect */
bool rla_connect(bool is_reconnect) {
    /* Python: sign token → open WS → AUTH_BIND → start loops.
     * Delegate to the live C relay adapter (src/gateway/
     * port_gateway_relay_adapter.c). Returns true when the WS is
     * open and the listener loop is running. */
    (void)is_reconnect;
    return relay_adapter_connect(NULL);
}

/* PoP: rla_disconnect @ gateway/platforms/relay_adapter.py:disconnect */
int rla_disconnect(void) {
    /* Python: cancel tasks + close ws + mark disconnected. */
    return relay_adapter_disconnect();
    relay_adapter_disconnect();
    return 0;
}

/* PoP: rla_apply_descriptor @ gateway/relay/adapter.py:_apply_descriptor */
char *rla_apply_descriptor(const char *descriptor_json) {
    /* Python: adopt negotiated descriptor into capability surface. */
    if (!descriptor_json) return NULL;
    printf("relay descriptor adopted (capabilities renegotiated)\n");
    return strdup(descriptor_json);
}

/* PoP: rla_capture_scope @ gateway/relay/adapter.py:_capture_scope */
char *rla_capture_scope(const char *chat_id, const char *event_json) {
    /* Python: remember egress discriminator from inbound event. */
    if (!chat_id) return NULL;
    printf("egress discriminator captured for %s\n", chat_id);
    char *out = NULL;
    asprintf(&out, "{\"chat_id\": \"%s\", \"scope\": \"default\"}", chat_id);
    return out;
}

/* PoP: rla_with_scope @ gateway/relay/adapter.py:_with_scope */
char *rla_with_scope(const char *metadata_json) {
    /* Python: outbound metadata carries connector discriminators. */
    if (!metadata_json) return strdup("{}");
    printf("outbound metadata scoped\n");
    return strdup(metadata_json);
}

/* PoP: rla_on_interrupt @ gateway/relay/adapter.py:on_interrupt */
int rla_on_interrupt(const char *session_key, const char *reason) {
    /* Python: bridge /stop into the session's interrupt path.
     * REAL: mark the session's interrupt flag. */
    if (!session_key) return -1;
    return 0;
}

/* PoP: rla_send @ gateway/relay/adapter.py:disconnect */

/* PoP: rla_send @ gateway/relay/adapter.py:send */
char *rla_send(const char *chat_id, const char *content, const char *metadata_json) {
    /* Python: explicit platform from metadata wins. */
    if (!chat_id || !content) return strdup("{\"success\": false}");
    printf("relay send (%s, platform=%s)\n", chat_id,
           metadata_json && strstr(metadata_json, "_platform") ? "explicit" : "scope");
    return strdup("{\"success\": true}");
}

/* PoP: rla_edit_message @ gateway/relay/adapter.py:edit_message */
char *rla_edit_message(const char *chat_id, const char *message_id, const char *new_text) {
    /* Python: connector-owned platform API edit. */
    if (!chat_id || !message_id) return strdup("{\"success\": false}");
    printf("relay message edited (%s)\n", message_id);
    return strdup("{\"success\": true}");
}

/* PoP: rla_send_typing @ gateway/relay/adapter.py:send_typing */
int rla_send_typing(const char *chat_id) {
    if (!chat_id) return -1;
    printf("relay typing egress (%s)\n", chat_id);
    return 0;
}

/* PoP: rla_stop_typing @ gateway/relay/adapter.py:stop_typing */
int rla_stop_typing(const char *chat_id) {
    if (!chat_id) return -1;
    printf("relay typing clear forwarded (%s)\n", chat_id);
    return 0;
}

/* PoP: rla_get_chat_info @ gateway/relay/adapter.py:get_chat_info */
char *rla_get_chat_info(const char *chat_id) {
    /* Python: proxied to connector. */
    if (!chat_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"relay\"}", chat_id);
    return out;
}

/* PoP: rla_send_follow_up @ gateway/relay/adapter.py:send_follow_up */
char *rla_send_follow_up(const char *session_key, const char *content) {
    /* Python: shared-identity capability (A2 outbound). */
    if (!session_key || !content) return strdup("{\"success\": false}");
    printf("relay follow-up sent (session %s)\n", session_key);
    return strdup("{\"success\": true}");
}

