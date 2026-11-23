/*
 * extras.c — C port of upstream Hermes pm/extras.py (gate w5_extras_3292f9).
 *
 * PoP: w5_extras_3292f9_canonical @ pm/extras.py:_canonical
 * PoP: w5_extras_3292f9_install_hint @ pm/extras.py:install_hint
 * PoP: w5_extras_3292f9_anchors @ pm/extras.py:_anchors
 * PoP: w5_extras_3292f9_missing @ pm/extras.py:missing
 *
 * The ANCHORS table is mirrored verbatim. Multi-anchor values are stored with
 * '|' as the separator (no real anchor contains that character). Availability
 * probing and platform gating stay in Python; the gate injects the set of
 * importable anchors.
 */
#include "noc_w5_extras_3292f9.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name; const char *anchors; } extras_anchor_t;

static const extras_anchor_t ANCHORS[] = {
    {"anthropic", "anthropic"},
    {"bedrock", "boto3"},
    {"vertex", "google.auth"},
    {"azure-identity", "azure.identity"},
    {"exa", "exa_py"},
    {"firecrawl", "firecrawl"},
    {"parallel-web", "parallel"},
    {"ddgs", "ddgs"},
    {"otlp", "opentelemetry.sdk"},
    {"mistral", "mistralai"},
    {"edge-tts", "edge_tts"},
    {"neutts", "neutts"},
    {"kittentts", "kittentts|soundfile"},
    {"piper", "piper"},
    {"tts-premium", "elevenlabs"},
    {"voice", "faster_whisper"},
    {"stt-whisper", "faster_whisper"},
    {"audio-io", "sounddevice|numpy"},
    {"silk", "pilk"},
    {"wake", "pyopen_wakeword"},
    {"wake-openwakeword", "pyopen_wakeword"},
    {"wake-sherpa", "sherpa_onnx"},
    {"wake-porcupine", "pvporcupine"},
    {"fal", "fal_client"},
    {"messaging", "telegram"},
    {"telegram", "telegram"},
    {"discord", "discord"},
    {"slack", "slack_bolt"},
    {"matrix", "mautrix|asyncpg|aiosqlite|markdown|aiohttp_socks"},
    {"dingtalk", "dingtalk_stream"},
    {"feishu", "lark_oapi"},
    {"wecom", "defusedxml"},
    {"keychain", "keyring"},
    {"teams", "microsoft_teams.apps"},
    {"modal", "modal"},
    {"daytona", "daytona"},
    {"vercel", "vercel"},
    {"google", "googleapiclient|google.auth|google_auth_oauthlib.flow|google_auth_httplib2"},
    {"google-chat", "google.cloud.pubsub_v1"},
    {"google-meet", "playwright.sync_api|websockets"},
    {"youtube", "youtube_transcript_api"},
    {"acp", "acp"},
    {"web", "fastapi"},
    {"doc-extract", "anydoc"},
    {"documents", "docx|openpyxl|pptx|reportlab|pypdf"},
    {"documents-pdf", "pdfplumber|pypdfium2"},
    {"computer-use", "mcp"},
    {"trace-upload", "huggingface_hub"},
    {"vision", "PIL"},
};
#define N_ANCHORS ((int)(sizeof(ANCHORS) / sizeof(ANCHORS[0])))

static char *dupstr(const char *s) {
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* PoP: w5_extras_3292f9_canonical @ pm/extras.py:_canonical */
char *w5_extras_3292f9_canonical(const char *name) {
    if (!name) return dupstr("");
    char *out = (char *)malloc(strlen(name) + 1);
    if (!out) return NULL;
    size_t j = 0;
    int in_run = 0;
    for (const char *p = name; *p; p++) {
        char c = *p;
        if (c == '-' || c == '_' || c == '.') {
            if (!in_run) { out[j++] = '-'; in_run = 1; }
        } else {
            out[j++] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
            in_run = 0;
        }
    }
    out[j] = '\0';
    return out;
}

/* PoP: w5_extras_3292f9_install_hint @ pm/extras.py:install_hint */
char *w5_extras_3292f9_install_hint(const char *extra) {
    static const char prefix[] = "hermes pm install --extra ";
    const char *e = extra ? extra : "";
    char *out = (char *)malloc(sizeof(prefix) + strlen(e));
    if (!out) return NULL;
    memcpy(out, prefix, sizeof(prefix) - 1);
    memcpy(out + sizeof(prefix) - 1, e, strlen(e) + 1);
    return out;
}

static const extras_anchor_t *find_anchor(const char *extra) {
    for (int i = 0; i < N_ANCHORS; i++)
        if (!strcmp(ANCHORS[i].name, extra)) return &ANCHORS[i];
    return NULL;
}

/* PoP: w5_extras_3292f9_anchors @ pm/extras.py:_anchors */
char **w5_extras_3292f9_anchors(const char *extra, size_t *out_n) {
    const extras_anchor_t *ent = find_anchor(extra ? extra : "");
    if (!ent) {
        /* ANCHORS.get(extra, extra.replace("-", "_")) → one anchor */
        char **out = (char **)malloc(sizeof(char *));
        if (!out) { if (out_n) *out_n = 0; return NULL; }
        const char *e = extra ? extra : "";
        char *a = (char *)malloc(strlen(e) + 1);
        if (a) {
            for (size_t i = 0; e[i]; i++) a[i] = (e[i] == '-') ? '_' : e[i];
            a[strlen(e)] = '\0';
        }
        out[0] = a;
        if (out_n) *out_n = a ? 1 : 0;
        return out;
    }
    size_t n = 1;
    for (const char *p = ent->anchors; *p; p++) if (*p == '|') n++;
    char **out = (char **)malloc(n * sizeof(char *));
    if (!out) { if (out_n) *out_n = 0; return NULL; }
    size_t k = 0;
    const char *start = ent->anchors;
    for (const char *p = ent->anchors;; p++) {
        if (*p == '|' || *p == '\0') {
            size_t len = (size_t)(p - start);
            out[k] = (char *)malloc(len + 1);
            if (out[k]) { memcpy(out[k], start, len); out[k][len] = '\0'; k++; }
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    if (out_n) *out_n = k;
    return out;
}

static int in_importable(const char *const *set, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (set[i] && !strcmp(set[i], s)) return 1;
    return 0;
}

/* PoP: w5_extras_3292f9_missing @ pm/extras.py:missing */
char **w5_extras_3292f9_missing(const char *extra,
                                const char *const *importable, size_t nimp,
                                size_t *out_n) {
    size_t na = 0;
    char **all = w5_extras_3292f9_anchors(extra, &na);
    char **out = (char **)malloc((na + 1) * sizeof(char *));
    if (!out) { w5_extras_3292f9_strings_free(all, na); if (out_n) *out_n = 0; return NULL; }
    size_t k = 0;
    for (size_t i = 0; i < na; i++) {
        if (!in_importable(importable, nimp, all[i])) out[k++] = all[i];
        else free(all[i]);
    }
    free(all);
    if (out_n) *out_n = k;
    return out;
}

void w5_extras_3292f9_strings_free(char **arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) free(arr[i]);
    free(arr);
}
