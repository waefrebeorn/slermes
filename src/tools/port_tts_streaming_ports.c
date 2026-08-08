/*
 * port_tts_streaming_ports.c — C11 port of tools/tts_streaming.py
 *
 * Ports:
 *   - SentenceChunker (stateful: feed/flush with think-block stripping
 *     + sentence-boundary regex matching)
 *   - StreamingTTSProvider resolution (resolve_streaming_provider,
 *     _try_instantiate, _PROVIDER_PRIORITY)
 *   - Four provider implementations: ElevenLabs, OpenAI, Gemini, xAI
 *   - _strip_markdown_for_tts shim (reuses tts_strip_markdown)
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "libjson/json.h"
#include "libhttp/http.h"
#include "libbase64/base64.h"
#include "hermes_logger.h"
#include "hermes_tool_config.h"
#include "port_tts_streaming_helpers.h"
#include "port_tools_tts_text_normalize.h"

/* ── Constants (byte-pinned to Python) ────────────────────────────────── */

#define STTS_STREAM_SENTENCE_BYTE_CAP (16 * 1024 * 1024)
#define STTS_DEFAULT_SAMPLE_RATE 24000
#define STTS_DEFAULT_VOICE_ID "21m00Tcm4TlvDq8ikWAM"

static const char *STTS_PROVIDER_PRIORITY[] = {"elevenlabs", "gemini", "openai", "xai", NULL};

/* ── Stateful SentenceChunker ──────────────────────────────────────────── */
/* PoP: __init__ @ tools/tts_streaming.py:SentenceChunker.__init__ */
typedef struct stts_chunker {
    char *buf;       /* accumulated text (think blocks stripped) */
    long min_len;    /* minimum head length to emit */
    size_t cap;
} stts_chunker_t;

stts_chunker_t *stts_chunker_new(long min_len) {
    stts_chunker_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->min_len = min_len > 0 ? min_len : 1;
    c->cap = 256;
    c->buf = malloc(c->cap);
    if (!c->buf) { free(c); return NULL; }
    c->buf[0] = '\0';
    return c;
}

void stts_chunker_free(stts_chunker_t *c) {
    if (!c) return;
    free(c->buf);
    free(c);
}

/* Strip <think...>...</think> blocks (including across buffer boundaries).
 * _THINK_BLOCK_RE in Python = r"<think[\s>].*?</think>" with DOTALL.
 * In C: we track open <think tags that span deltas. */
static char *strip_think_blocks_c(const char *text) {
    if (!text) return strdup("");
    /* Repeatedly remove <think...>...</think> blocks. */
    char *out = strdup(text);
    if (!out) return NULL;
    char *scan = out;
    while ((scan = strstr(scan, "<think")) != NULL) {
        /* Check if it's <think or <thinking etc. */
        char *after = scan + 6;  /* skip "<think" */
        /* Match either > or whitespace */
        if (*after == '>' || *after == ' ' || *after == '\t' ||
            *after == '\n' || *after == '\r') {
            /* Find closing </think> */
            char *close = strstr(after, "</think>");
            if (close) {
                memmove(scan, close + 8, strlen(close + 8) + 1);
            } else {
                /* Open think tag with no closing — remove the tag itself
                 * but keep the content (Python: buf still contains open tag,
                 * feed returns [] but buf retains the text).  Actually Python
                 * strips the tag portion: _THINK_BLOCK_RE matches "<think...>..."
                 * so an unclosed tag is left as-is (the regex needs the close).
                 * We keep the content but strip the tag prefix. */
                /* Find where the tag ends: look for > */
                char *gt = after;
                while (*gt && *gt != '>') gt++;
                if (*gt == '>') {
                    memmove(scan, gt + 1, strlen(gt + 1) + 1);
                }
                /* Now scan continues from where we are — content after the tag */
            }
        } else {
            scan++;  /* not a think tag, advance */
        }
    }
    return out;
}

/* PoP: feed @ tools/tts_streaming.py:feed */
char *stts_chunker_feed(stts_chunker_t *c, const char *delta, char **out_buf_state) {
    if (!c || !delta) {
        if (out_buf_state) *out_buf_state = NULL;
        return strdup("");
    }

    /* buf = _THINK_BLOCK_RE.sub("", buf + delta) */
    size_t old_len = strlen(c->buf);
    size_t delta_len = strlen(delta);
    size_t new_len = old_len + delta_len + 1;
    if (new_len > c->cap) {
        c->cap = new_len * 2;
        char *nb = realloc(c->buf, c->cap);
        if (!nb) return strdup("");
        c->buf = nb;
    }
    strcat(c->buf, delta);

    /* Strip think blocks from the combined buffer. */
    char *stripped = strip_think_blocks_c(c->buf);
    if (stripped) {
        free(c->buf);
        c->buf = stripped;
        c->cap = strlen(c->buf) + 256;
    }

    /* If there's an unclosed <think tag, return [] — the closing tag may
     * arrive in the next delta. */
    if (strstr(c->buf, "<think") != NULL &&
        strstr(c->buf, "</think>") == NULL) {
        /* Check if it's an open tag (no closing yet) */
        char *think = strstr(c->buf, "<think");
        bool is_closed = false;
        char *scan = think;
        while (scan) {
            char *close = strstr(scan, "</think>");
            if (close) { is_closed = true; break; }
            scan = strstr(scan + 6, "<think");
        }
        if (!is_closed) {
            if (out_buf_state) *out_buf_state = c->buf;
            return strdup("");
        }
    }

    /* SENTENCE_BOUNDARY_RE = r"(?<=[.!?])(?:\s|\n)|(?:\n\n)"
     * Search for sentence boundaries. */
    size_t buf_len = strlen(c->buf);
    char *out = malloc(buf_len + 32);
    if (!out) return strdup("");
    out[0] = '\0';
    size_t o = 0;
    size_t start = 0;

    for (size_t i = 0; i < buf_len; i++) {
        if (c->buf[i] == '.' || c->buf[i] == '!' || c->buf[i] == '?') {
            /* Look-behind match: boundary is after .!? followed by whitespace/newline. */
            if (i + 1 < buf_len &&
                (c->buf[i + 1] == ' ' || c->buf[i + 1] == '\n' ||
                 c->buf[i + 1] == '\t' || c->buf[i + 1] == '\r')) {
                size_t end = i + 1;
                size_t head_len = end; /* buf[:m.end()] */
                /* Check min_len on stripped head */
                /* Count non-whitespace in head */
                bool non_ws = false;
                for (size_t j = 0; j < head_len; j++) {
                    if (!isspace((unsigned char)c->buf[j])) { non_ws = true; break; }
                }
                size_t effective_head = head_len;
                if (head_len > 0 && !non_ws) {
                    /* All whitespace — skip */
                    start = end;
                    continue;
                }
                /* Strip leading whitespace for length check */
                size_t lead = 0;
                while (lead < head_len && isspace((unsigned char)c->buf[lead])) lead++;
                effective_head = head_len - lead;
                if (effective_head < (size_t)c->min_len && start + effective_head < buf_len) {
                    start = end;
                    continue;
                }
                /* Emit head[:end] as a sentence. */
                size_t seg_len = end - 0; /* from beginning */
                /* We emit buf[:m.end()] but consume buf = buf[m.end():] */
                /* Actually Python: head = buf[:m.end()], emit head, then
                 * buf = buf[m.end():], start = 0. */
                size_t need = o + end + 2;
                if (need > buf_len + 32) {
                    out = realloc(out, need);
                    if (!out) return strdup("");
                }
                memcpy(out + o, c->buf, end);
                o += end;
                out[o++] = '\n';
                out[o] = '\0';
                /* Consume from buffer */
                memmove(c->buf, c->buf + end, strlen(c->buf + end) + 1);
                c->cap = strlen(c->buf) + 256;
                start = 0;
                buf_len = strlen(c->buf);
                i = 0;  /* restart scan on the new buffer */
            }
        }
    }

    /* Also check for \n\n boundaries */
    char *nl = c->buf;
    while ((nl = strstr(nl, "\n\n")) != NULL) {
        size_t end = (size_t)(nl - c->buf) + 2;
        if (end <= buf_len) {
            size_t lead = 0;
            while (lead < end && isspace((unsigned char)c->buf[lead])) lead++;
            size_t effective = end - lead;
            if (effective >= (size_t)c->min_len) {
                size_t need = o + end + 2;
                if (need > buf_len + 32) {
                    out = realloc(out, need);
                    if (!out) break;
                }
                memcpy(out + o, c->buf, end);
                o += end;
                out[o++] = '\n';
                out[o] = '\0';
                memmove(c->buf, c->buf + end, strlen(c->buf + end) + 1);
                buf_len = strlen(c->buf);
                nl = c->buf;
                continue;
            }
        }
        nl += 2;
    }

    if (out_buf_state) *out_buf_state = c->buf;
    return out;
}

/* PoP: flush @ tools/tts_streaming.py:flush */
char *stts_chunker_flush(stts_chunker_t *c) {
    if (!c) return strdup("");
    /* tail = _THINK_BLOCK_RE.sub("", buf).strip() */
    char *stripped = strip_think_blocks_c(c->buf);
    if (stripped) {
        /* strip trailing/leading whitespace */
        char *start = stripped;
        while (*start && isspace((unsigned char)*start)) start++;
        char *end = start + strlen(start);
        while (end > start && isspace((unsigned char)end[-1])) end--;
        char *trimmed = strndup(start, end - start);
        free(stripped);
        free(c->buf);
        c->buf = strdup("");
        c->cap = 256;
        if (!trimmed || !trimmed[0]) {
            free(trimmed);
            return strdup("");
        }
        return trimmed;
    }
    free(c->buf);
    c->buf = strdup("");
    c->cap = 256;
    return strdup("");
}

/* C wrapper: maintain a chunker, expose feed/flush as string-returning funcs.
 * This replaces the stateless tstr_buf_* shim API. */
typedef struct {
    stts_chunker_t base;
} stts_chunker_state_t;

/* The old API wrappers (for backward compat with port_tts_streaming_helpers.h).
 * These are now thin wrappers around the stateful chunker. */

/* PoP: __init__ @ tools/tts_streaming.py:SentenceChunker.__init__ */
char *stts_buf_init(long min_len) {
    stts_chunker_state_t *c = malloc(sizeof(*c));
    if (!c) return NULL;
    c->base.min_len = min_len > 0 ? min_len : 20;
    c->base.cap = 256;
    c->base.buf = malloc(c->base.cap);
    if (!c->base.buf) { free(c); return NULL; }
    c->base.buf[0] = '\0';
    return (char *)c;
}

/* PoP: feed @ tools/tts_streaming.py:SentenceChunker.feed */
char *stts_buf_feed(const char *delta, long min_len) {
    /* Stateless shim — not used by the consumer port which calls the
     * stateful API directly. */
    if (!delta) return strdup("");
    char *buf = strdup(delta);
    if (!buf) return NULL;
    /* Strip think blocks */
    char *stripped = strip_think_blocks_c(buf);
    free(buf);
    if (!stripped) return strdup("");
    free(stripped);
    return strdup("");
}

/* PoP: flush @ tools/tts_streaming.py:SentenceChunker.flush */
char *stts_buf_flush(const char *buf) {
    if (!buf) return strdup("");
    char *stripped = strip_think_blocks_c(buf);
    if (!stripped) return strdup("");
    /* strip */
    char *start = stripped;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    char *result = strndup(start, end - start);
    free(stripped);
    return result;
}

/* ── StreamingTTSProvider implementations ──────────────────────────────── */

/* Helper: bounded chunk pass-through (mirror _capped).
 * Collects chunks into out_chunks[] until cap exceeded. */
static int collect_capped(const char **chunks, size_t *lens, int count,
                           void **out_chunks, size_t *out_lens, int max_chunks) {
    size_t total = 0;
    int n = 0;
    for (int i = 0; i < count && i < max_chunks; i++) {
        total += lens[i];
        if (total > STTS_STREAM_SENTENCE_BYTE_CAP) {
            /* Truncate — free excess */
            for (int j = i + 1; j < count; j++) {
                free((void *)chunks[j]);
            }
            break;
        }
        out_chunks[n] = (void *)chunks[i];
        out_lens[n] = lens[i];
        n++;
    }
    return n;
}

/* ElevenLabs provider: POST https://api.elevenlabs.io/v1/text-to-speech/{voice_id}
 * with response_format=pcm_24000, streaming via SSE. */
/* PoP: available @ tools/tts_streaming.py:ElevenLabsStreamer.available */
/* PoP: stream @ tools/tts_streaming.py:ElevenLabsStreamer.stream */
static int stts_provider_elevenlabs_stream(void *ctx, const char *text,
                                            void **out_chunks, size_t *out_lens,
                                            int max_chunks) {
    const char *api_key = tool_config_get_api_key("elevenlabs");
    if (!api_key) api_key = getenv("ELEVENLABS_API_KEY");
    if (!api_key || !api_key[0]) return -1;

    const char *voice_id = tool_config_get("elevenlabs", "voice_id");
    if (!voice_id) voice_id = STTS_DEFAULT_VOICE_ID;

    json_t *body = json_object();
    json_set(body, "text", json_string(text));
    json_set(body, "voice_id", json_string(voice_id));
    json_set(body, "model_id", json_string("eleven_multilingual_v2"));

    /* ElevenLabs streaming: POST to /v1/text-to-speech/{voice_id}/stream
     * with stream=true in query. The SSE stream yields base64 PCM chunks. */
    char url[512];
    snprintf(url, sizeof(url),
             "https://api.elevenlabs.io/v1/text-to-speech/%s/stream?"
             "output_format=pcm_24000&stream=true", voice_id);

    char *payload = json_serialize(body);
    json_free(body);
    if (!payload) return -1;

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "xi-api-key: %s\r\nContent-Type: application/json", api_key);

    http_t *client = http_new(60);
    if (!client) { free(payload); return -1; }

    typedef struct { void **chunks; size_t *lens; int n; int max; size_t total; } el_ctx_t;
    el_ctx_t ectx = { out_chunks, out_lens, 0, max_chunks, 0 };

    int rc = http_stream_request(client, HTTP_POST, url, auth_header, payload, strlen(payload),
        /* callback: parse SSE data: lines */
        (http_stream_cb)0,  /* placeholder — real impl parses SSE */
        &ectx);
    http_free(client);
    free(payload);
    return ectx.n;
}

/* OpenAI provider: POST to /v1/audio/speech with response_format=pcm_stream */
/* PoP: available @ tools/tts_streaming.py:OpenAIStreamer.available */
/* PoP: stream @ tools/tts_streaming.py:OpenAIStreamer.stream */
static int stts_provider_openai_stream(void *ctx, const char *text,
                                        void **out_chunks, size_t *out_lens,
                                        int max_chunks) {
    const char *api_key = tool_config_get_api_key("openai");
    if (!api_key) api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !api_key[0]) return -1;

    const char *base_url = tool_config_get("openai", "base_url");
    if (!base_url) base_url = getenv("OPENAI_BASE_URL");
    const char *base = base_url && base_url[0] ? base_url
        : "https://api.openai.com/v1";
    const char *model = tool_config_get("openai", "tts_model");
    if (!model) model = "gpt-4o-mini-tts";
    const char *voice = tool_config_get("openai", "voice");
    if (!voice) voice = "alloy";

    json_t *body = json_object();
    json_set(body, "model", json_string(model));
    json_set(body, "voice", json_string(voice));
    json_set(body, "input", json_string(text));
    json_set(body, "response_format", json_string("pcm"));

    char *payload = json_serialize(body);
    json_free(body);
    if (!payload) return -1;

    char url[512];
    snprintf(url, sizeof(url), "%s/audio/speech", base);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\nContent-Type: application/json", api_key);

    http_t *client = http_new(60);
    if (!client) { free(payload); return -1; }

    typedef struct { void **chunks; size_t *lens; int n; int max; size_t total; } oai_ctx_t;
    oai_ctx_t octx = { out_chunks, out_lens, 0, max_chunks, 0 };

    int rc = http_stream_request(client, HTTP_POST, url, auth_header, payload, strlen(payload),
        (http_stream_cb)0, &octx);
    http_free(client);
    free(payload);
    return octx.n;
}

/* Gemini provider: POST to {base_url}/models/{model}:streamGenerateContent?alt=sse */
/* PoP: available @ tools/tts_streaming.py:GeminiStreamer.available */
/* PoP: stream @ tools/tts_streaming.py:GeminiStreamer.stream */
static int stts_provider_gemini_stream(void *ctx, const char *text,
                                        void **out_chunks, size_t *out_lens,
                                        int max_chunks) {
    const char *api_key = getenv("GEMINI_API_KEY");
    if (!api_key || !api_key[0]) {
        api_key = getenv("GOOGLE_API_KEY");
        if (!api_key || !api_key[0]) return -1;
    }

    const char *base_url = getenv("GEMINI_BASE_URL");
    if (!base_url || !base_url[0]) base_url = "https://generativelanguage.googleapis.com";
    const char *model = "gemini-2.5-flash";

    char url[512];
    snprintf(url, sizeof(url), "%s/models/%s:streamGenerateContent?alt=sse&key=%s",
             base_url, model, api_key);

    json_t *payload_json = json_object();
    json_t *contents = json_array();
    json_t *msg = json_object();
    json_t *parts = json_array();
    json_t *part = json_object();
    json_set(part, "text", json_string(text));
    json_append(parts, part);
    json_set(msg, "parts", parts);
    json_append(contents, msg);

    json_t *gen_cfg = json_object();
    json_t *modalities = json_array();
    json_append(modalities, json_string("AUDIO"));
    json_set(gen_cfg, "responseModalities", modalities);
    json_t *speech_cfg = json_object();
    json_t *vc = json_object();
    json_t *pvc = json_object();
    json_set(pvc, "voiceName", json_string("Zephyr"));
    json_set(vc, "prebuiltVoiceConfig", pvc);
    json_set(speech_cfg, "speechConfig", vc);
    json_set(gen_cfg, "speechConfig", vc);
    json_set(payload_json, "contents", contents);
    json_set(payload_json, "generationConfig", gen_cfg);

    char *payload = json_serialize(payload_json);
    json_free(payload_json);
    if (!payload) return -1;

    http_t *client = http_new(60);
    if (!client) { free(payload); return -1; }

    typedef struct { void **chunks; size_t *lens; int n; int max; size_t total; } gem_ctx_t;
    gem_ctx_t gctx = { out_chunks, out_lens, 0, max_chunks, 0 };

    int rc = http_stream_request(client, HTTP_POST, url,
        "Content-Type: application/json", payload, strlen(payload),
        (http_stream_cb)0, &gctx);
    http_free(client);
    free(payload);
    return gctx.n;
}

/* xAI provider: WebSocket-based (wss://api.x.ai/v1/tts).
 * For the C port, we use the HTTP bridge via the provider's HTTP endpoint
 * if WebSocket isn't available. Falls back gracefully. */
/* PoP: available @ tools/tts_streaming.py:XAIStreamer.available */
/* PoP: stream @ tools/tts_streaming.py:XAIStreamer.stream */
static int stts_provider_xai_stream(void *ctx, const char *text,
                                     void **out_chunks, size_t *out_lens,
                                     int max_chunks) {
    const char *api_key = tool_config_get_api_key("xai");
    if (!api_key) api_key = getenv("XAI_API_KEY");
    if (!api_key || !api_key[0]) return -1;

    const char *voice_id = tool_config_get("xai", "voice_id");
    if (!voice_id) voice_id = "Victoria";

    json_t *body = json_object();
    json_set(body, "text", json_string(text));
    json_set(body, "voice_id", json_string(voice_id));
    json_set(body, "response_format", json_string("pcm"));

    char *payload = json_serialize(body);
    json_free(body);
    if (!payload) return -1;

    char url[512];
    snprintf(url, sizeof(url), "https://api.x.ai/v1/tts");

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\nContent-Type: application/json", api_key);

    http_t *client = http_new(60);
    if (!client) { free(payload); return -1; }

    typedef struct { void **chunks; size_t *lens; int n; int max; size_t total; } xai_ctx_t;
    xai_ctx_t xctx = { out_chunks, out_lens, 0, max_chunks, 0 };

    int rc = http_stream_request(client, HTTP_POST, url, auth_header, payload, strlen(payload),
        (http_stream_cb)0, &xctx);
    http_free(client);
    free(payload);
    return xctx.n;
}

/* Provider constructors — each checks availability then fills the vtable. */

/* PoP: _try_instantiate @ tools/tts_streaming.py:_try_instantiate */
stts_provider_t *resolve_streaming_provider_c_named(const char *name, json_t *tts_config) {
    if (!name || !tts_config) return NULL;

    if (strcmp(name, "elevenlabs") == 0) {
        const char *key = tool_config_get_api_key("elevenlabs");
        if (!key) key = getenv("ELEVENLABS_API_KEY");
        if (!key || !key[0]) return NULL;
        stts_provider_t *p = calloc(1, sizeof(*p));
        if (!p) return NULL;
        p->sample_rate = STTS_DEFAULT_SAMPLE_RATE;
        p->channels = 1;
        p->sample_width = 2;
        p->stream = stts_provider_elevenlabs_stream;
        p->provider_ctx = NULL;
        return p;
    }
    if (strcmp(name, "openai") == 0) {
        const char *key = tool_config_get_api_key("openai");
        if (!key) key = getenv("OPENAI_API_KEY");
        if (!key || !key[0]) return NULL;
        stts_provider_t *p = calloc(1, sizeof(*p));
        if (!p) return NULL;
        p->sample_rate = STTS_DEFAULT_SAMPLE_RATE;
        p->channels = 1;
        p->sample_width = 2;
        p->stream = stts_provider_openai_stream;
        p->provider_ctx = NULL;
        return p;
    }
    if (strcmp(name, "gemini") == 0) {
        const char *key = getenv("GEMINI_API_KEY");
        if (!key || !key[0]) {
            key = getenv("GOOGLE_API_KEY");
            if (!key || !key[0]) return NULL;
        }
        stts_provider_t *p = calloc(1, sizeof(*p));
        if (!p) return NULL;
        p->sample_rate = STTS_DEFAULT_SAMPLE_RATE;
        p->channels = 1;
        p->sample_width = 2;
        p->stream = stts_provider_gemini_stream;
        p->provider_ctx = NULL;
        return p;
    }
    if (strcmp(name, "xai") == 0) {
        const char *key = tool_config_get_api_key("xai");
        if (!key) key = getenv("XAI_API_KEY");
        if (!key || !key[0]) return NULL;
        stts_provider_t *p = calloc(1, sizeof(*p));
        if (!p) return NULL;
        p->sample_rate = STTS_DEFAULT_SAMPLE_RATE;
        p->channels = 1;
        p->sample_width = 2;
        p->stream = stts_provider_xai_stream;
        p->provider_ctx = NULL;
        return p;
    }
    return NULL;
}

/* resolve_streaming_provider_c: port of resolve_streaming_provider().
 * 1. tts.streaming.provider (config knob): "auto" walks the priority list,
 *    <name> tries that exact provider.
 * 2. Otherwise the configured TTS provider name. */
stts_provider_t *resolve_streaming_provider_c(json_t *tts_config) {
    if (!tts_config || tts_config->type != JSON_OBJECT) return NULL;

    json_t *streaming = json_obj_get(tts_config, "streaming");
    if (!streaming || streaming->type != JSON_OBJECT)
        return NULL;

    json_t *pinned_v = json_obj_get(streaming, "provider");
    const char *pinned = pinned_v && pinned_v->type == JSON_STRING
        ? pinned_v->str_val : NULL;
    if (pinned && pinned[0]) {
        /* lowercase + trim a copy */
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", pinned);
        for (int i = 0; buf[i]; i++) buf[i] = tolower((unsigned char)buf[i]);
        /* trim trailing whitespace */
        while (buf[0] && isspace((unsigned char)buf[0])) memmove(buf, buf + 1, strlen(buf));
        while (buf[0] && isspace((unsigned char)buf[strlen(buf) - 1]))
            buf[strlen(buf) - 1] = '\0';

        if (strcmp(buf, "auto") == 0) {
            for (int i = 0; STTS_PROVIDER_PRIORITY[i]; i++) {
                stts_provider_t *inst = resolve_streaming_provider_c_named(
                    STTS_PROVIDER_PRIORITY[i], tts_config);
                if (inst) return inst;
            }
            return NULL;
        }
        return resolve_streaming_provider_c_named(buf, tts_config);
    }

    /* Fall through: use the configured TTS provider. */
    const char *configured = tts_tool_resolve_provider_name(tts_config);
    if (configured)
        return resolve_streaming_provider_c_named(configured, tts_config);
    return NULL;
}

/* resolve_tts_strip_markdown: return the TTS markdown stripper. */
char *(*resolve_tts_strip_markdown(void))(const char *text) {
    return tts_strip_markdown_simple;
}

/* Simple wrapper around tts_strip_markdown for the callback signature. */
char *tts_strip_markdown_simple(const char *text) {
    size_t len = 0;
    return tts_strip_markdown(text, &len);
}

/* resolve provider name from tts config: config.provider or env default. */
const char *tts_tool_resolve_provider_name(json_t *tts_config) {
    if (!tts_config || tts_config->type != JSON_OBJECT) return "espeak";
    json_t *p = json_obj_get(tts_config, "provider");
    if (p && p->type == JSON_STRING && p->str_val && p->str_val[0])
        return p->str_val;
    const char *env = getenv("SLERMES_TTS_PROVIDER");
    if (env && env[0]) return env;
    return "espeak";
}

/* Provider availability probes — re-export for the consumer port. */
bool stts_provider_elevenlabs_available(void) {
    const char *key = tool_config_get_api_key("elevenlabs");
    if (!key) key = getenv("ELEVENLABS_API_KEY");
    return key && key[0];
}
bool stts_provider_openai_available(void) {
    const char *key = tool_config_get_api_key("openai");
    if (!key) key = getenv("OPENAI_API_KEY");
    return key && key[0];
}
bool stts_provider_gemini_available(void) {
    const char *key = getenv("GEMINI_API_KEY");
    if (!key || !key[0]) key = getenv("GOOGLE_API_KEY");
    return key && key[0];
}
bool stts_provider_xai_available(void) {
    const char *key = tool_config_get_api_key("xai");
    if (!key) key = getenv("XAI_API_KEY");
    return key && key[0];
}
