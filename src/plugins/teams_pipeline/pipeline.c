/*
 * pipeline.c — C11 port of upstream plugins/teams_pipeline/pipeline.py
 * (gates w33_pipeline_540ecf_p1, w33_pipeline_540ecf_p2).
 *
 * PoP: w33_pipeline_540ecf_p1_config              @ plugins/teams_pipeline/pipeline.py:TeamsPipelineConfig.from_dict
 * PoP: w33_pipeline_540ecf_p1_organizer_user_id   @ plugins/teams_pipeline/pipeline.py:_organizer_user_id_from_payload
 * PoP: w33_pipeline_540ecf_p1_parse_resource      @ plugins/teams_pipeline/meetings.py:parse_graph_meeting_resource
 * PoP: w33_pipeline_540ecf_p1_transcript_like     @ plugins/teams_pipeline/meetings.py:looks_like_transcript_id
 * PoP: w33_pipeline_540ecf_p1_is_artifact         @ plugins/teams_pipeline/pipeline.py:_resource_data_id_is_artifact
 * PoP: w33_pipeline_540ecf_p1_meeting_ids         @ plugins/teams_pipeline/pipeline.py:_meeting_ids_from_notification
 * PoP: w33_pipeline_540ecf_p1_receipt_key         @ plugins/teams_pipeline/store.py:TeamsPipelineStore.build_notification_receipt_key
 * PoP: w33_pipeline_540ecf_p1_run_gate            @ plugins/teams_pipeline/pipeline.py:TeamsMeetingPipeline.run_notification
 * PoP: w33_pipeline_540ecf_p1_find_dedupe         @ plugins/teams_pipeline/pipeline.py:TeamsMeetingPipeline._find_job_by_dedupe_key
 *
 * PoP: w33_pipeline_540ecf_p2_rich_text           @ plugins/teams_pipeline/pipeline.py:_rich_text
 * PoP: w33_pipeline_540ecf_p2_bullets             @ plugins/teams_pipeline/pipeline.py:_bullets
 * PoP: w33_pipeline_540ecf_p2_sections            @ plugins/teams_pipeline/pipeline.py:_sections
 * PoP: w33_pipeline_540ecf_p2_notion_properties   @ plugins/teams_pipeline/pipeline.py:NotionWriter._build_properties
 * PoP: w33_pipeline_540ecf_p2_notion_blocks       @ plugins/teams_pipeline/pipeline.py:NotionWriter._build_blocks
 * PoP: w33_pipeline_540ecf_p2_linear_body         @ plugins/teams_pipeline/pipeline.py:LinearWriter.write_summary (issue body branch)
 * PoP: w33_pipeline_540ecf_p2_render_markdown     @ plugins/teams_pipeline/pipeline.py:_render_summary_markdown
 * PoP: w33_pipeline_540ecf_p2_summary_prompt      @ plugins/teams_pipeline/pipeline.py:_build_summary_prompt
 * PoP: w33_pipeline_540ecf_p2_parse_summary       @ plugins/teams_pipeline/pipeline.py:_parse_summary_json
 * PoP: w33_pipeline_540ecf_p2_heuristic_summary   @ plugins/teams_pipeline/pipeline.py:_heuristic_summary
 * PoP: w33_pipeline_540ecf_p2_participants        @ plugins/teams_pipeline/pipeline.py:_collect_participants
 * PoP: w33_pipeline_540ecf_p2_call_metrics        @ plugins/teams_pipeline/pipeline.py:_collect_call_metrics
 * PoP: w33_pipeline_540ecf_p2_payload_targets     @ plugins/teams_pipeline/pipeline.py:TeamsMeetingPipeline._generate_summary_payload (field shaping)
 * PoP: w33_pipeline_540ecf_p2_transcript_decision @ plugins/teams_pipeline/pipeline.py:TeamsMeetingPipeline._obtain_transcript
 * PoP: w33_pipeline_540ecf_p2_recording_name      @ plugins/teams_pipeline/pipeline.py:TeamsMeetingPipeline._transcribe_recording / _prepare_audio_path
 * PoP: w33_pipeline_540ecf_p2_stt_outcome         @ plugins/teams_pipeline/pipeline.py:TeamsMeetingPipeline._transcribe_recording (result classification)
 * PoP: w33_pipeline_540ecf_p2_sink_plan           @ plugins/teams_pipeline/pipeline.py:TeamsMeetingPipeline._write_sinks
 *
 * What is ported is exactly the part of the pipeline that is a pure function of
 * values: Graph resource-path parsing, notification -> (meeting id, organizer,
 * artifact ids), the dedupe/receipt key, the lifecycle gate, the transcript vs
 * recording->STT stage decision, the summary payload/markdown/block shaping and
 * the sink order. Everything that touches the world is a seam fed as fixture
 * input: httpx transports for the Notion/Linear sinks, the Graph client, the
 * LLM call, the STT thread, ffmpeg discovery, tempfile dirs and the store.
 * The STT/ffmpeg paths therefore report *decisions* (which path, which error
 * class) rather than performing the I/O.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w33_pipeline_540ecf_p1.h"
#include "noc_w33_pipeline_540ecf_p2.h"

/* ================================================================== */
/* Python value semantics                                              */
/* ================================================================== */

static char *pl_dup_n(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n && s) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *pl_dup(const char *s) { return pl_dup_n(s ? s : "", s ? strlen(s) : 0); }

static int pl_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int pl_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *pl_cstr(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return "";
    return v->str_val;
}

/* obj.get(key) that survives a non-object. */
static const json_t *pl_get(const json_t *o, const char *key) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, key) : NULL;
}

/* Python str() for the scalar shapes a Graph payload carries. */
static char *pl_str_of(const json_t *v) {
    if (pl_is_none(v)) return pl_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return pl_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        char tmp[64];
        double d = v->num_val;
        if (d == (double)(long long)d && d < 1e15 && d > -1e15)
            snprintf(tmp, sizeof tmp, "%.0f", d);
        else
            snprintf(tmp, sizeof tmp, "%.17g", d);
        return pl_dup(tmp);
    }
    case JSON_STRING: return pl_dup(pl_cstr(v));
    default: {
        char *s = json_dumps(v, 0);
        char *out = pl_dup(s ? s : "");
        free(s);
        return out;
    }
    }
}

/* Python str(v or ""): falsy -> "". */
static char *pl_str_or_empty(const json_t *v) { return pl_truthy(v) ? pl_str_of(v) : pl_dup(""); }

static int pl_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() / str.strip(chars). */
static char *pl_strip_chars(const char *s, const char *chars) {
    const char *start = s ? s : "";
    const char *end;
    if (!chars) {
        while (*start && pl_isspace((unsigned char)*start)) start++;
    } else {
        while (*start && strchr(chars, *start)) start++;
    }
    end = start + strlen(start);
    if (!chars) {
        while (end > start && pl_isspace((unsigned char)end[-1])) end--;
    } else {
        while (end > start && strchr(chars, end[-1])) end--;
    }
    return pl_dup_n(start, (size_t)(end - start));
}

static char *pl_strip(const char *s) { return pl_strip_chars(s, NULL); }

static char *pl_lower(const char *s) {
    char *o = pl_dup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return o;
}

static int pl_ci_at(const char *s, const char *lit) {
    for (; *lit; lit++, s++) {
        char a = *s, b = *lit;
        if (!a) return 0;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0;
    }
    return 1;
}

/* ISO-8859-free numeric view used by int(): trunc toward zero. */
static double pl_int_of(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (double)(long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) return (double)strtoll(v->str_val ? v->str_val : "", NULL, 10);
    return def;
}

/* ================================================================== */
/* Growable string buffer                                              */
/* ================================================================== */

typedef struct {
    char *buf;
    size_t len, cap;
    int oom;
} pl_sb;

static void sb_init(pl_sb *b) { b->buf = NULL; b->len = b->cap = 0; b->oom = 0; }

static void sb_reserve(pl_sb *b, size_t extra) {
    size_t need, cap;
    char *p;
    if (b->oom) return;
    need = b->len + extra + 1;
    if (need <= b->cap) return;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap *= 2;
    p = (char *)realloc(b->buf, cap);
    if (!p) { b->oom = 1; return; }
    b->buf = p;
    b->cap = cap;
}

static void sb_putn(pl_sb *b, const char *s, size_t n) {
    if (!n) return;
    sb_reserve(b, n);
    if (b->oom) return;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void sb_puts(pl_sb *b, const char *s) {
    if (s) sb_putn(b, s, strlen(s));
}

static void sb_putc(pl_sb *b, char c) { sb_putn(b, &c, 1); }

static void sb_printf(pl_sb *b, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof tmp) { sb_putn(b, tmp, (size_t)n); return; }
    {
        char *big = (char *)malloc((size_t)n + 1);
        if (!big) { b->oom = 1; return; }
        va_start(ap, fmt);
        vsnprintf(big, (size_t)n + 1, fmt, ap);
        va_end(ap);
        sb_putn(b, big, (size_t)n);
        free(big);
    }
}

static char *sb_take(pl_sb *b) {
    char *out;
    if (b->oom || !b->buf) {
        free(b->buf);
        b->buf = NULL;
        b->len = b->cap = 0;
        return pl_dup("");
    }
    out = b->buf;
    b->buf = NULL;
    b->len = b->cap = 0;
    return out;
}

/* ================================================================== */
/* urllib.parse.unquote                                                */
/* ================================================================== */

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char *pl_unquote(const char *s) {
    pl_sb b;
    const char *p = s ? s : "";
    sb_init(&b);
    while (*p) {
        if (p[0] == '%' && hexval((unsigned char)p[1]) >= 0 && hexval((unsigned char)p[2]) >= 0) {
            char c = (char)(hexval((unsigned char)p[1]) * 16 + hexval((unsigned char)p[2]));
            sb_putc(&b, c);
            p += 3;
        } else {
            sb_putc(&b, *p);
            p++;
        }
    }
    return sb_take(&b);
}

/* ================================================================== */
/* base64 (urlsafe then standard), as _decoded_id_hint tries them       */
/* ================================================================== */

static int b64_val(int c, int urlsafe) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (urlsafe) {
        if (c == '-') return 62;
        if (c == '_') return 63;
        if (c == '+') return 62;
        if (c == '/') return 63;
    } else {
        if (c == '+') return 62;
        if (c == '/') return 63;
    }
    return -1;
}

/* Python b64decode(validate=False): non-alphabet bytes (including '=') are
 * discarded; a residual length of 1 mod 4 is an error. */
static unsigned char *b64_decode(const char *s, int urlsafe, size_t *outlen) {
    size_t n = 0, i = 0, o = 0;
    unsigned char *out;
    int vals[4];
    *outlen = 0;
    for (const char *p = s; *p; p++)
        if (b64_val((unsigned char)*p, urlsafe) >= 0) n++;
    if (n == 0) return NULL;
    if (n % 4 == 1) return NULL;
    out = (unsigned char *)malloc(n / 4 * 3 + 3);
    if (!out) return NULL;
    for (const char *p = s; *p; p++) {
        int v = b64_val((unsigned char)*p, urlsafe);
        if (v < 0) continue;
        vals[i++] = v;
        if (i == 4) {
            out[o++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
            out[o++] = (unsigned char)(((vals[1] & 15) << 4) | (vals[2] >> 2));
            out[o++] = (unsigned char)(((vals[2] & 3) << 6) | vals[3]);
            i = 0;
        }
    }
    if (i == 2) {
        out[o++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
    } else if (i == 3) {
        out[o++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
        out[o++] = (unsigned char)(((vals[1] & 15) << 4) | (vals[2] >> 2));
    }
    *outlen = o;
    return out;
}

/* _decoded_id_hint: best-effort base64 decode of a Graph id, lowercased. */
static char *pl_decoded_hint(const char *value) {
    char *stripped, *padded;
    unsigned char *raw = NULL;
    size_t n = 0, rl;
    char *out;
    size_t pad;

    stripped = pl_strip(value ? value : "");
    if (!stripped) return pl_dup("");
    if (strlen(stripped) < 16) { free(stripped); return pl_dup(""); }
    pad = (4 - (strlen(stripped) % 4)) % 4;
    padded = (char *)malloc(strlen(stripped) + pad + 1);
    if (!padded) { free(stripped); return pl_dup(""); }
    memcpy(padded, stripped, strlen(stripped));
    memset(padded + strlen(stripped), '=', pad);
    padded[strlen(stripped) + pad] = '\0';
    free(stripped);

    raw = b64_decode(padded, 1, &n);
    if (!raw) raw = b64_decode(padded, 0, &n);
    free(padded);
    if (!raw) return pl_dup("");
    rl = n;
    out = (char *)malloc(rl + 1);
    if (!out) { free(raw); return pl_dup(""); }
    for (size_t i = 0; i < rl; i++) {
        unsigned char c = raw[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
    }
    out[rl] = '\0';
    free(raw);
    return out;
}

/* looks_like_transcript_id(value, odata_type=...) */
static int pl_transcript_like(const char *value, const char *odata_type) {
    char *t = pl_lower(odata_type ? odata_type : "");
    int hit;
    if (!t) return 0;
    hit = strstr(t, "calltranscript") != NULL;
    free(t);
    if (hit) return 1;
    {
        char *v = pl_lower(value ? value : "");
        char *hint;
        if (!v) return 0;
        if (strstr(v, "transcript")) { free(v); return 1; }
        hint = pl_decoded_hint(value ? value : "");
        free(v);
        if (!hint) return 0;
        hit = strstr(hint, "transcript") != NULL;
        free(hint);
        return hit;
    }
}

/* ================================================================== */
/* parse_graph_meeting_resource                                        */
/* ================================================================== */

typedef struct {
    char *org;
    char *mid;
    char *tid;
    char *rid;
} pl_graph_ids;

static void graph_ids_free(pl_graph_ids *g) {
    free(g->org); free(g->mid); free(g->tid); free(g->rid);
    g->org = g->mid = g->tid = g->rid = NULL;
}

/* One capture group: quoted ('...') or slash /..., mirroring the regex groups. */
static int take_segment(const char **pp, const char *stop_slash, char **val) {
    const char *p = *pp;
    if (*p == '(' && p[1] == '\'') {
        const char *end = strchr(p + 2, '\'');
        size_t n;
        if (!end || end[1] != ')') return 0;
        n = (size_t)(end - (p + 2));
        if (n == 0) return 0;
        *val = pl_dup_n(p + 2, n);
        *pp = end + 2;
        return *val != NULL;
    }
    if (*p == '/') {
        const char *q = p + 1;
        while (*q && !strchr(stop_slash, *q)) q++;
        if (q == p + 1) return 0;
        *val = pl_dup_n(p + 1, (size_t)(q - (p + 1)));
        *pp = q;
        return *val != NULL;
    }
    return 0;
}

/* unquote(next(group)).strip() or None */
static char *finish_id(char *raw) {
    char *u, *s;
    if (!raw) return NULL;
    u = pl_unquote(raw);
    free(raw);
    if (!u) return NULL;
    s = pl_strip(u);
    free(u);
    if (!s) return NULL;
    if (!*s) { free(s); return NULL; }
    return s;
}

static const char *const PL_RESOURCE_SENTINELS[] = {
    "getalltranscripts", "getallrecordings", "transcripts", "recordings",
};

static int is_resource_sentinel(const char *id) {
    char *low = pl_lower(id);
    int hit = 0;
    if (!low) return 0;
    for (size_t i = 0; i < sizeof PL_RESOURCE_SENTINELS / sizeof *PL_RESOURCE_SENTINELS; i++)
        if (!strcmp(low, PL_RESOURCE_SENTINELS[i])) { hit = 1; break; }
    free(low);
    return hit;
}

static int find_users_meeting(const char *text, char **org, char **mid) {
    for (const char *p = text; *p; p++) {
        const char *q;
        char *o = NULL, *m = NULL;
        if (!(p == text || p[-1] == '/')) continue;
        if (!pl_ci_at(p, "users")) continue;
        q = p + 5;
        if (!take_segment(&q, "/'()", &o)) continue;
        if (!pl_ci_at(q, "/onlineMeetings")) { free(o); continue; }
        q += strlen("/onlineMeetings");
        if (!take_segment(&q, "/'?", &m)) { free(o); continue; }
        *org = o;
        *mid = m;
        return 1;
    }
    return 0;
}

static int find_comm_meeting(const char *text, char **mid) {
    static const char *const lit = "communications/onlineMeetings";
    for (const char *p = text; *p; p++) {
        const char *q;
        char *m = NULL;
        if (!(p == text || p[-1] == '/')) continue;
        if (!pl_ci_at(p, lit)) continue;
        q = p + strlen(lit);
        if (!take_segment(&q, "/'?", &m)) continue;
        *mid = m;
        return 1;
    }
    return 0;
}

static int find_artifact_id(const char *text, const char *lit, char **val) {
    for (const char *p = text; *p; p++) {
        const char *q;
        char *v = NULL;
        if (*p != '/') continue;
        if (!pl_ci_at(p, lit)) continue;
        q = p + strlen(lit);
        if (!take_segment(&q, "/'?", &v)) continue;
        *val = v;
        return 1;
    }
    return 0;
}

static void pl_parse_graph_resource(const char *resource, pl_graph_ids *out) {
    char *text, *tmp = NULL;
    memset(out, 0, sizeof *out);
    text = pl_strip(resource ? resource : "");
    if (!text) return;

    if (find_users_meeting(text, &tmp, &out->mid)) {
        out->org = finish_id(tmp);
        out->mid = finish_id(out->mid);
    } else if (find_comm_meeting(text, &out->mid)) {
        out->mid = finish_id(out->mid);
    }
    if (out->mid && is_resource_sentinel(out->mid)) {
        free(out->mid);
        out->mid = NULL;
    }
    tmp = NULL;
    if (find_artifact_id(text, "/transcripts", &tmp)) out->tid = finish_id(tmp);
    tmp = NULL;
    if (find_artifact_id(text, "/recordings", &tmp)) out->rid = finish_id(tmp);
    free(text);
}

/* ================================================================== */
/* sha256 (build_notification_receipt_key's canonical digest)          */
/* ================================================================== */

typedef struct {
    uint32_t h[8];
    uint64_t bits;
    unsigned char buf[64];
    size_t used;
} pl_sha256;

static const uint32_t PL_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(pl_sha256 *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR32(w[i - 15], 7) ^ ROR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR32(w[i - 2], 17) ^ ROR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t S0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        t1 = h + S1 + ch + PL_K[i] + w[i];
        t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha256_init(pl_sha256 *s) {
    static const uint32_t iv[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    memcpy(s->h, iv, sizeof iv);
    s->bits = 0;
    s->used = 0;
}

static void sha256_update(pl_sha256 *s, const void *data, size_t n) {
    const unsigned char *p = (const unsigned char *)data;
    s->bits += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - s->used;
        if (take > n) take = n;
        memcpy(s->buf + s->used, p, take);
        s->used += take;
        p += take;
        n -= take;
        if (s->used == 64) { sha256_block(s, s->buf); s->used = 0; }
    }
}

static void sha256_final(pl_sha256 *s, unsigned char out[32]) {
    uint64_t bits = s->bits;
    unsigned char pad = 0x80;
    sha256_update(s, &pad, 1);
    {
        unsigned char zero = 0;
        while (s->used != 56) sha256_update(s, &zero, 1);
    }
    {
        unsigned char len[8];
        for (int i = 0; i < 8; i++) len[i] = (unsigned char)(bits >> (56 - 8 * i));
        sha256_update(s, len, 8);
    }
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(s->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(s->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(s->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(s->h[i]);
    }
}

/* ================================================================== */
/* json.dumps(..., sort_keys=True, separators=(",", ":"))              */
/* ================================================================== */

static void canon_string(pl_sb *b, const char *s) {
    sb_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"':  sb_puts(b, "\\\""); break;
        case '\\': sb_puts(b, "\\\\"); break;
        case '\b': sb_puts(b, "\\b"); break;
        case '\f': sb_puts(b, "\\f"); break;
        case '\n': sb_puts(b, "\\n"); break;
        case '\r': sb_puts(b, "\\r"); break;
        case '\t': sb_puts(b, "\\t"); break;
        default:
            if (c < 0x20) {
                sb_printf(b, "\\u%04x", c);
            } else if (c < 0x80) {
                sb_putc(b, (char)c);
            } else {
                uint32_t cp;
                int extra;
                if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; extra = 1; }
                else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; extra = 2; }
                else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; extra = 3; }
                else { sb_printf(b, "\\u%04x", c); continue; }
                for (int i = 1; i <= extra; i++) {
                    if ((p[i] & 0xC0) != 0x80) { cp = c; extra = 0; break; }
                    cp = (cp << 6) | (uint32_t)(p[i] & 0x3F);
                }
                p += (size_t)extra;
                if (cp > 0xFFFF) {
                    uint32_t v = cp - 0x10000;
                    sb_printf(b, "\\u%04x\\u%04x", 0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
                } else {
                    sb_printf(b, "\\u%04x", cp);
                }
            }
        }
    }
    sb_putc(b, '"');
}

static void canon_number(pl_sb *b, double d) {
    char tmp[64];
    if (d == (double)(long long)d && d < 1e15 && d > -1e15)
        snprintf(tmp, sizeof tmp, "%.0f", d);
    else
        snprintf(tmp, sizeof tmp, "%.17g", d);
    sb_puts(b, tmp);
}

static int cmp_key(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void canon_node(pl_sb *b, const json_t *v) {
    if (!v) { sb_puts(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:   sb_puts(b, "null"); break;
    case JSON_BOOL:   sb_puts(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: canon_number(b, v->num_val); break;
    case JSON_STRING: canon_string(b, pl_cstr(v)); break;
    case JSON_ARRAY: {
        size_t n = json_len(v);
        sb_putc(b, '[');
        for (size_t i = 0; i < n; i++) {
            if (i) sb_putc(b, ',');
            canon_node(b, json_get(v, i));
        }
        sb_putc(b, ']');
        break;
    }
    case JSON_OBJECT: {
        size_t n = json_object_size(v);
        const char **keys = (const char **)malloc((n ? n : 1) * sizeof *keys);
        if (!keys) { sb_puts(b, "{}"); break; }
        for (size_t i = 0; i < n; i++) keys[i] = json_object_get_key_at(v, i);
        qsort(keys, n, sizeof *keys, cmp_key);
        sb_putc(b, '{');
        for (size_t i = 0; i < n; i++) {
            if (i) sb_putc(b, ',');
            canon_string(b, keys[i]);
            sb_putc(b, ':');
            canon_node(b, json_obj_get(v, keys[i]));
        }
        sb_putc(b, '}');
        free(keys);
        break;
    }
    }
}

/* ================================================================== */
/* p1: config                                                          */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p1_config(const json_t *payload) {
    static const struct { const char *key; int def; } flags[4] = {
        {"transcript_preferred", 1},
        {"transcript_required", 0},
        {"transcription_fallback", 1},
        {"ffmpeg_extract_audio", 1},
    };
    const json_t *data = (payload && payload->type == JSON_OBJECT) ? payload : NULL;
    const json_t *v, *tmp;
    json_t *out = json_object();

    for (size_t i = 0; i < 4; i++) {
        v = data ? json_obj_get(data, flags[i].key) : NULL;
        json_set(out, flags[i].key, json_bool(v ? pl_truthy(v) : flags[i].def));
    }

    v = data ? json_obj_get(data, "stt_model") : NULL;
    if (!pl_truthy(v) && data) v = json_obj_get(data, "sttModel");
    json_set(out, "stt_model", pl_truthy(v) ? json_copy(v) : json_null());

    v = data ? json_obj_get(data, "transcript_min_chars") : NULL;
    json_set(out, "transcript_min_chars", json_number(pl_int_of(v, 80.0)));

    tmp = data ? json_obj_get(data, "tmp_dir") : NULL;
    if (!pl_truthy(tmp) && data) tmp = json_obj_get(data, "tmpDir");
    if (pl_truthy(tmp)) {
        char *s = pl_str_of(tmp);
        json_set(out, "tmp_dir", json_string(s ? s : ""));
        free(s);
    } else {
        json_set(out, "tmp_dir", json_null());
    }

    json_set(out, "notion", (data && json_obj_get(data, "notion") && json_obj_get(data, "notion")->type != JSON_NULL)
                               ? json_copy(json_obj_get(data, "notion")) : json_null());
    json_set(out, "linear", (data && json_obj_get(data, "linear") && json_obj_get(data, "linear")->type != JSON_NULL)
                               ? json_copy(json_obj_get(data, "linear")) : json_null());
    tmp = data ? json_obj_get(data, "teams_delivery") : NULL;
    if (!pl_truthy(tmp) && data) tmp = json_obj_get(data, "teamsDelivery");
    json_set(out, "teams_delivery", pl_truthy(tmp) ? json_copy(tmp) : json_null());
    return out;
}

/* ================================================================== */
/* p1: organizer_user_id                                               */
/* ================================================================== */

static char *organizer_from_payload(const json_t *payload) {
    const json_t *organizer, *user, *identity, *id;
    char *out;

    organizer = pl_get(payload, "meetingOrganizer");
    if (!pl_truthy(organizer)) organizer = pl_get(payload, "organizer");
    if (organizer && organizer->type == JSON_OBJECT) {
        user = json_obj_get(organizer, "user");
        if (pl_is_none(user)) {
            identity = json_obj_get(organizer, "identity");
            if (identity && identity->type == JSON_OBJECT) user = json_obj_get(identity, "user");
        }
        if (user && user->type == JSON_OBJECT) {
            id = json_obj_get(user, "id");
            if (pl_truthy(id)) {
                char *raw = pl_str_of(id);
                out = pl_strip(raw);
                free(raw);
                if (out && !*out) { free(out); out = NULL; }
                return out;
            }
        }
    }
    {
        const json_t *t = pl_get(payload, "organizerUserId");
        if (!pl_truthy(t)) t = pl_get(payload, "organizer_user_id");
        if (!pl_truthy(t)) return NULL;
        {
            char *raw = pl_str_of(t);
            out = pl_strip(raw);
            free(raw);
            if (out && !*out) { free(out); out = NULL; }
            return out;
        }
    }
}

json_t *w33_pipeline_540ecf_p1_organizer_user_id(const json_t *payload) {
    char *id = organizer_from_payload(payload);
    json_t *out;
    if (!id) return json_null();
    out = json_string(id);
    free(id);
    return out;
}

/* ================================================================== */
/* p1: parse_resource                                                  */
/* ================================================================== */

static json_t *opt_string(const char *s) { return s ? json_string(s) : json_null(); }

json_t *w33_pipeline_540ecf_p1_parse_resource(const json_t *request) {
    const json_t *resource = (request && request->type == JSON_OBJECT) ? json_obj_get(request, "resource") : request;
    char *raw = pl_str_or_empty(resource);
    pl_graph_ids ids;
    json_t *out = json_object();

    pl_parse_graph_resource(raw, &ids);
    free(raw);
    json_set(out, "organizer_user_id", opt_string(ids.org));
    json_set(out, "meeting_id", opt_string(ids.mid));
    json_set(out, "transcript_id", opt_string(ids.tid));
    json_set(out, "recording_id", opt_string(ids.rid));
    graph_ids_free(&ids);
    return out;
}

/* ================================================================== */
/* p1: transcript_like                                                 */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p1_transcript_like(const json_t *request) {
    const json_t *v = pl_get(request, "value");
    char *value = pl_str_or_empty(v);
    const json_t *t = pl_get(request, "odata_type");
    char *odata = pl_str_or_empty(t);
    int hit = pl_transcript_like(value, odata);
    free(value);
    free(odata);
    return json_bool(hit);
}

/* ================================================================== */
/* p1: is_artifact                                                     */
/* ================================================================== */

/* _odata_field(resource_data, "odata.type") */
static char *resource_odata_type(const json_t *rd) {
    const json_t *v = pl_get(rd, "@odata.type");
    if (!pl_truthy(v)) v = pl_get(rd, "odata.type");
    return pl_str_or_empty(v);
}

static int resource_id_is_artifact(const json_t *notification, const json_t *rd) {
    char *odata = resource_odata_type(rd);
    char *low = pl_lower(odata);
    char *resource;
    int hit = 0;
    if (!low) { free(odata); return 0; }
    if (strstr(low, "calltranscript") || strstr(low, "callrecording")) hit = 1;
    free(low);
    if (!hit) {
        const json_t *rv = pl_get(notification, "resource");
        resource = pl_lower(pl_cstr(rv));
        if (resource) {
            if (strstr(resource, "getalltranscripts") || strstr(resource, "getallrecordings") ||
                strstr(resource, "/transcripts") || strstr(resource, "/recordings"))
                hit = 1;
            free(resource);
        }
    }
    if (!hit) {
        char *id = pl_str_or_empty(pl_get(rd, "id"));
        hit = pl_transcript_like(id, odata);
        free(id);
    }
    free(odata);
    return hit;
}

json_t *w33_pipeline_540ecf_p1_is_artifact(const json_t *request) {
    const json_t *notification = pl_get(request, "notification");
    const json_t *rd = pl_get(request, "resource_data");
    return json_bool(resource_id_is_artifact(notification, rd));
}

/* ================================================================== */
/* p1: receipt_key                                                     */
/* ================================================================== */

static char *notification_receipt_key(const json_t *notification) {
    const json_t *id = pl_get(notification, "id");
    pl_sb b;
    unsigned char digest[32];
    pl_sha256 sha;
    char hex[65];
    char *canon;

    if (pl_truthy(id)) {
        char *raw = pl_str_of(id);
        char *out = (char *)malloc(strlen(raw ? raw : "") + 4);
        if (!out) { free(raw); return pl_dup(""); }
        sprintf(out, "id:%s", raw ? raw : "");
        free(raw);
        return out;
    }
    sb_init(&b);
    canon_node(&b, notification);
    canon = sb_take(&b);
    sha256_init(&sha);
    sha256_update(&sha, canon, strlen(canon));
    sha256_final(&sha, digest);
    free(canon);
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", digest[i]);
    hex[64] = '\0';
    {
        char *out = (char *)malloc(7 + 64 + 1);
        if (!out) return pl_dup("");
        sprintf(out, "sha256:%s", hex);
        return out;
    }
}

json_t *w33_pipeline_540ecf_p1_receipt_key(const json_t *notification) {
    char *key = notification_receipt_key(notification);
    json_t *out = json_string(key ? key : "");
    free(key);
    return out;
}

/* ================================================================== */
/* p1: meeting_ids                                                     */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p1_meeting_ids(const json_t *notification) {
    const json_t *rd = pl_get(notification, "resourceData");
    const json_t *raws[3];
    pl_graph_ids parsed[3];
    char *first[4] = {NULL, NULL, NULL, NULL};
    char *odata, *organizer = NULL, *meeting_id = NULL, *transcript_id = NULL, *recording_id = NULL, *rd_id = NULL;
    json_t *out = json_object(), *extra = json_object();

    if (rd && rd->type != JSON_OBJECT) rd = NULL;
    memset(parsed, 0, sizeof parsed);

    raws[0] = pl_get(rd, "@odata.id");
    if (!pl_truthy(raws[0])) raws[0] = pl_get(rd, "odata.id");
    raws[1] = pl_get(notification, "resource");
    raws[2] = pl_get(rd, "transcriptContentUrl");
    for (int i = 0; i < 3; i++) {
        char *raw = pl_str_or_empty(raws[i]);
        pl_parse_graph_resource(raw, &parsed[i]);
        free(raw);
    }
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i < 3; i++) {
            char *v = (k == 0) ? parsed[i].org : (k == 1) ? parsed[i].mid : (k == 2) ? parsed[i].tid : parsed[i].rid;
            if (v) { first[k] = pl_dup(v); break; }
        }
    }
    odata = resource_odata_type(rd);

    organizer = first[0];
    if (!organizer) organizer = organizer_from_payload(rd);
    if (!organizer) organizer = organizer_from_payload(notification);

    meeting_id = first[1];
    if (!meeting_id) {
        const json_t *mv = pl_get(rd, "meetingId");
        if (!pl_truthy(mv)) mv = pl_get(notification, "meetingId");
        if (pl_truthy(mv)) {
            char *raw = pl_str_of(mv);
            char *s = pl_strip(raw);
            free(raw);
            if (s && *s) meeting_id = s;
            else free(s);
        }
    }
    transcript_id = first[2];
    recording_id = first[3];

    {
        const json_t *idv = pl_get(rd, "id");
        if (pl_truthy(idv)) {
            char *raw = pl_str_of(idv);
            char *s = pl_strip(raw);
            free(raw);
            if (s && *s) rd_id = s;
            else free(s);
        }
    }

    if (rd_id && !resource_id_is_artifact(notification, rd)) {
        if (!meeting_id) meeting_id = pl_dup(rd_id);
    } else if (rd_id && !transcript_id && pl_transcript_like(rd_id, odata)) {
        transcript_id = pl_dup(rd_id);
    }
    if (!meeting_id) {
        if (transcript_id) meeting_id = pl_dup(transcript_id);
        else if (recording_id) meeting_id = pl_dup(recording_id);
        else meeting_id = notification_receipt_key(notification);
    }

    json_set(out, "meeting_id", json_string(meeting_id ? meeting_id : ""));
    json_set(out, "organizer_user_id", opt_string(organizer));
    if (transcript_id) json_set(extra, "transcript_id", json_string(transcript_id));
    if (recording_id) json_set(extra, "recording_id", json_string(recording_id));
    json_set(out, "extra_metadata", extra);

    free(odata);
    free(rd_id);
    free(meeting_id);
    free(organizer);
    free(transcript_id);
    free(recording_id);
    /* first[] entries were handed off to the locals above; freeing them here
     * would be a double free. */
    for (int i = 0; i < 3; i++) graph_ids_free(&parsed[i]);
    return out;
}

/* ================================================================== */
/* p1: run_gate                                                        */
/* ================================================================== */

static int is_terminal_state(const char *s) {
    return !strcmp(s, "completed") || !strcmp(s, "failed") || !strcmp(s, "retry_scheduled");
}

static int is_active_state(const char *s) {
    static const char *const states[] = {
        "received", "resolving_meeting", "fetching_transcript", "downloading_recording",
        "transcribing_audio", "summarizing", "writing_notion", "writing_linear", "sending_teams",
    };
    for (size_t i = 0; i < sizeof states / sizeof *states; i++)
        if (!strcmp(s, states[i])) return 1;
    return 0;
}

json_t *w33_pipeline_540ecf_p1_run_gate(const json_t *job) {
    const json_t *sv = pl_get(job, "status");
    const char *status = pl_cstr(sv);
    if (is_terminal_state(status)) return json_string("skip");
    if (is_active_state(status) && strcmp(status, "received")) return json_string("skip");
    return json_string("run");
}

/* ================================================================== */
/* p1: find_dedupe                                                     */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p1_find_dedupe(const json_t *request) {
    const json_t *jobs = pl_get(request, "jobs");
    const json_t *kv = pl_get(request, "dedupe_key");
    char *key = pl_str_or_empty(kv);
    json_t *out = json_null();

    if (jobs && jobs->type == JSON_ARRAY && key) {
        size_t n = json_len(jobs);
        for (size_t i = 0; i < n && out->type == JSON_NULL; i++) {
            const json_t *payload = json_get(jobs, i);
            char *have;
            if (!payload || payload->type != JSON_OBJECT) continue;
            have = pl_str_or_empty(json_obj_get(payload, "dedupe_key"));
            if (have && !strcmp(have, key)) {
                char *id = pl_str_or_empty(json_obj_get(payload, "job_id"));
                out = json_string(id ? id : "");
                free(id);
            }
            free(have);
        }
    }
    free(key);
    return out;
}

/* ================================================================== */
/* p2: rich_text / bullets / sections                                  */
/* ================================================================== */

static json_t *rich_text_node(const char *content) {
    json_t *o = json_object(), *arr = json_array(), *item = json_object(), *text = json_object();
    json_set(text, "content", json_string(content ? content : ""));
    json_set(item, "text", text);
    json_append(arr, item);
    json_set(o, "rich_text", arr);
    return o;
}

json_t *w33_pipeline_540ecf_p2_rich_text(const json_t *request) {
    char *content = pl_str_or_empty(pl_get(request, "content"));
    json_t *out = rich_text_node(content);
    free(content);
    return out;
}

static char *bullets_of(const json_t *items) {
    pl_sb b;
    size_t n = (items && items->type == JSON_ARRAY) ? json_len(items) : 0;
    sb_init(&b);
    for (size_t i = 0; i < n; i++) {
        char *s = pl_str_of(json_get(items, i));
        if (i) sb_putc(&b, '\n');
        sb_putc(&b, '-');
        sb_putc(&b, ' ');
        sb_puts(&b, s ? s : "");
        free(s);
    }
    return sb_take(&b);
}

json_t *w33_pipeline_540ecf_p2_bullets(const json_t *items) {
    char *s = bullets_of(items);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

static const char *payload_summary(const json_t *payload) { return pl_cstr(pl_get(payload, "summary")); }

/* _sections(payload) -> [(heading, body), ...] */
static json_t *sections_node(const json_t *payload) {
    static const char *const headings[4] = {"Summary", "Key Decisions", "Action Items", "Risks"};
    static const char *const list_keys[3] = {"key_decisions", "action_items", "risks"};
    json_t *out = json_array();

    for (int i = 0; i < 4; i++) {
        json_t *pair = json_array();
        char *body;
        if (i == 0) {
            body = pl_dup(payload_summary(payload));
        } else {
            body = bullets_of(pl_get(payload, list_keys[i - 1]));
        }
        json_append(pair, json_string(headings[i]));
        json_append(pair, json_string(body ? body : ""));
        free(body);
        json_append(out, pair);
    }
    return out;
}

json_t *w33_pipeline_540ecf_p2_sections(const json_t *payload) { return sections_node(payload); }

/* ================================================================== */
/* p2: notion_properties / notion_blocks                               */
/* ================================================================== */

static char *payload_title(const json_t *payload) {
    const json_t *tv = pl_get(payload, "title");
    if (pl_truthy(tv)) return pl_str_of(tv);
    {
        char *id = pl_str_or_empty(pl_get(pl_get(payload, "meeting_ref"), "meeting_id"));
        pl_sb b;
        sb_init(&b);
        sb_puts(&b, "Meeting ");
        sb_puts(&b, id ? id : "");
        free(id);
        return sb_take(&b);
    }
}

json_t *w33_pipeline_540ecf_p2_notion_properties(const json_t *request) {
    const json_t *payload = pl_get(request, "payload");
    const json_t *config = pl_get(request, "config");
    const json_t *tp = pl_get(config, "title_property");
    const json_t *sp = pl_get(config, "summary_property");
    const json_t *mp = pl_get(config, "meeting_id_property");
    const char *title_key = pl_truthy(tp) ? pl_cstr(tp) : "Name";
    json_t *out = json_object();
    char *title = payload_title(payload);
    json_t *title_prop = json_object(), *title_arr = json_array(), *title_item = json_object(), *title_text = json_object();

    json_set(title_text, "content", json_string(title ? title : ""));
    json_set(title_item, "text", title_text);
    json_append(title_arr, title_item);
    json_set(title_prop, "title", title_arr);
    json_set(out, title_key, title_prop);
    free(title);

    if (pl_truthy(sp)) {
        char *summary = pl_dup(payload_summary(payload));
        json_t *rt;
        if (summary && strlen(summary) > W33_PIPELINE_540ECF_P2_SUMMARY_CLIP)
            summary[W33_PIPELINE_540ECF_P2_SUMMARY_CLIP] = '\0';
        rt = rich_text_node(summary ? summary : "");
        json_set(out, pl_cstr(sp), rt);
        free(summary);
    }
    if (pl_truthy(mp)) {
        char *id = pl_str_or_empty(pl_get(pl_get(payload, "meeting_ref"), "meeting_id"));
        json_t *rt = rich_text_node(id ? id : "");
        json_set(out, pl_cstr(mp), rt);
        free(id);
    }
    return out;
}

json_t *w33_pipeline_540ecf_p2_notion_blocks(const json_t *payload) {
    json_t *out = json_array();
    json_t *sections = sections_node(payload);
    size_t n = json_len(sections);

    for (size_t i = 0; i < n; i++) {
        const json_t *pair = json_get(sections, i);
        const char *heading = pl_cstr(json_get(pair, 0));
        const char *body = pl_cstr(json_get(pair, 1));
        json_t *h = json_object(), *p = json_object();
        json_set(h, "object", json_string("block"));
        json_set(h, "type", json_string("heading_2"));
        json_set(h, "heading_2", rich_text_node(heading));
        json_set(p, "object", json_string("block"));
        json_set(p, "type", json_string("paragraph"));
        json_set(p, "paragraph", rich_text_node((body && *body) ? body : "None"));
        json_append(out, h);
        json_append(out, p);
    }
    json_free(sections);
    return out;
}

/* ================================================================== */
/* p2: render_markdown                                                 */
/* ================================================================== */

static char *render_markdown(const json_t *payload) {
    pl_sb b;
    json_t *sections = sections_node(payload);
    size_t n = json_len(sections);
    char *title = payload_title(payload);

    sb_init(&b);
    sb_puts(&b, "# ");
    sb_puts(&b, title ? title : "");
    for (size_t i = 0; i < n; i++) {
        const json_t *pair = json_get(sections, i);
        const char *heading = pl_cstr(json_get(pair, 0));
        const char *body = pl_cstr(json_get(pair, 1));
        sb_puts(&b, "\n\n## ");
        sb_puts(&b, heading);
        sb_putc(&b, '\n');
        if (body && *body) {
            sb_puts(&b, body);
        } else if (!strcmp(heading, "Summary")) {
            sb_puts(&b, "No summary available.");
        } else {
            sb_puts(&b, "- None");
        }
    }
    sb_puts(&b, "\n\nConfidence: ");
    {
        const char *conf = pl_cstr(pl_get(payload, "confidence"));
        sb_puts(&b, (conf && *conf) ? conf : "unknown");
    }
    sb_putc(&b, '\n');
    sb_puts(&b, pl_cstr(pl_get(payload, "confidence_notes")));
    free(title);
    json_free(sections);
    {
        char *joined = sb_take(&b);
        char *stripped = pl_strip(joined);
        free(joined);
        return stripped;
    }
}

json_t *w33_pipeline_540ecf_p2_render_markdown(const json_t *request) {
    const json_t *payload = pl_get(request, "payload");
    char *s = render_markdown(payload);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* ================================================================== */
/* p2: linear_body                                                     */
/* ================================================================== */

static const char *const PL_LINEAR_UPDATE =
    "mutation($id: String!, $input: IssueUpdateInput!) { issueUpdate(id: $id, input: $input) "
    "{ success issue { id identifier url } } }";
static const char *const PL_LINEAR_CREATE =
    "mutation($input: IssueCreateInput!) { issueCreate(input: $input) "
    "{ success issue { id identifier url } } }";

json_t *w33_pipeline_540ecf_p2_linear_body(const json_t *request) {
    const json_t *payload = pl_get(request, "payload");
    const json_t *config = pl_get(request, "config");
    const json_t *existing = pl_get(request, "existing_record");
    const json_t *issue_id_v = pl_get(existing, "issue_id");
    const json_t *team_id_v = pl_get(config, "team_id");
    json_t *out = json_object(), *issue_input = json_object(), *body = json_object();
    char *title = NULL, *desc = NULL, *team_id = NULL;

    if (!pl_truthy(team_id_v)) {
        team_id_v = pl_get(config, "teamId");
    }
    team_id = pl_strip(pl_cstr(team_id_v));

    {
        const json_t *tv = pl_get(payload, "title");
        if (pl_truthy(tv)) {
            title = pl_str_of(tv);
        } else {
            char *id = pl_str_or_empty(pl_get(pl_get(payload, "meeting_ref"), "meeting_id"));
            pl_sb b;
            sb_init(&b);
            sb_puts(&b, "Meeting Summary: ");
            sb_puts(&b, id ? id : "");
            free(id);
            title = sb_take(&b);
        }
    }
    desc = render_markdown(payload);
    json_set(issue_input, "title", json_string(title ? title : ""));
    json_set(issue_input, "description", json_string(desc ? desc : ""));

    if (pl_truthy(issue_id_v)) {
        json_t *variables = json_object();
        json_set(body, "query", json_string(PL_LINEAR_UPDATE));
        json_set(variables, "id", json_copy(issue_id_v));
        json_set(variables, "input", json_copy(issue_input));
        json_set(body, "variables", variables);
        json_set(out, "mode", json_string("update"));
        json_set(out, "body", body);
        json_set(out, "error", json_null());
    } else if (!team_id || !*team_id) {
        json_set(out, "mode", json_string("error"));
        json_set(out, "body", json_null());
        json_set(out, "error", json_string("Linear sink requires team_id when creating a new issue."));
        json_free(body);
    } else {
        json_t *variables = json_object(), *input = json_object();
        json_set(input, "teamId", json_string(team_id));
        json_set(input, "title", json_string(title ? title : ""));
        json_set(input, "description", json_string(desc ? desc : ""));
        json_set(body, "query", json_string(PL_LINEAR_CREATE));
        json_set(variables, "input", input);
        json_set(body, "variables", variables);
        json_set(out, "mode", json_string("create"));
        json_set(out, "body", body);
        json_set(out, "error", json_null());
    }
    free(title);
    free(desc);
    free(team_id);
    json_free(issue_input);
    return out;
}

/* ================================================================== */
/* p2: summary_prompt                                                  */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p2_summary_prompt(const json_t *request) {
    const json_t *meeting_ref = pl_get(request, "meeting_ref");
    const json_t *artifacts = pl_get(request, "artifacts");
    char *transcript = pl_str_or_empty(pl_get(request, "transcript_text"));
    char *meeting_id = pl_str_or_empty(pl_get(meeting_ref, "meeting_id"));
    const json_t *subject = pl_get(pl_get(meeting_ref, "metadata"), "subject");
    pl_sb b;
    size_t n = (artifacts && artifacts->type == JSON_ARRAY) ? json_len(artifacts) : 0;

    sb_init(&b);
    sb_puts(&b, "Meeting ID: ");
    sb_puts(&b, meeting_id ? meeting_id : "");
    sb_puts(&b, "\nTitle: ");
    sb_puts(&b, pl_truthy(subject) ? pl_cstr(subject) : "Unknown");
    sb_puts(&b, "\nArtifacts:\n");
    if (n == 0) {
        sb_puts(&b, "- none");
    } else {
        for (size_t i = 0; i < n; i++) {
            const json_t *a = json_get(artifacts, i);
            char *id = pl_str_or_empty(pl_get(a, "artifact_id"));
            const char *type = pl_cstr(pl_get(a, "artifact_type"));
            const char *name = pl_cstr(pl_get(a, "display_name"));
            if (i) sb_putc(&b, '\n');
            sb_puts(&b, "- ");
            sb_puts(&b, type);
            sb_putc(&b, ':');
            sb_puts(&b, id ? id : "");
            sb_putc(&b, ':');
            sb_puts(&b, name ? name : "");
            free(id);
        }
    }
    sb_puts(&b, "\n\nTranscript:\n");
    if (transcript) {
        size_t len = strlen(transcript);
        if (len > W33_PIPELINE_540ECF_P2_TRANSCRIPT_CLIP) len = W33_PIPELINE_540ECF_P2_TRANSCRIPT_CLIP;
        sb_putn(&b, transcript, len);
    }
    free(transcript);
    free(meeting_id);
    {
        char *s = sb_take(&b);
        json_t *out = json_string(s ? s : "");
        free(s);
        return out;
    }
}

/* ================================================================== */
/* p2: heuristic_summary / parse_summary                               */
/* ================================================================== */

/* Python str.splitlines() over the ASCII/Unicode breaks a transcript carries. */
static char **split_lines(const char *text, size_t *count) {
    size_t cap = 8, n = 0;
    char **lines = (char **)malloc(cap * sizeof *lines);
    const char *p = text ? text : "";
    pl_sb cur;
    if (!lines) { *count = 0; return NULL; }
    sb_init(&cur);
    while (*p) {
        int brk = 0;
        size_t width = 1;
        if (*p == '\n') brk = 1;
        else if (*p == '\r') { brk = 1; if (p[1] == '\n') width = 2; }
        else if (*p == '\v' || *p == '\f') brk = 1;
        else if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0x85) { brk = 1; width = 2; }
        else if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x80 && (unsigned char)p[2] == 0xA8) { brk = 1; width = 3; }
        else if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x80 && (unsigned char)p[2] == 0xA9) { brk = 1; width = 3; }
        if (brk) {
            char *line = sb_take(&cur);
            if (n == cap) {
                char **grown = (char **)realloc(lines, cap * 2 * sizeof *lines);
                if (!grown) { free(line); break; }
                lines = grown;
                cap *= 2;
            }
            lines[n++] = line ? line : pl_dup("");
            sb_init(&cur);
            p += width;
        } else {
            sb_putc(&cur, *p);
            p++;
        }
    }
    if (cur.len) {
        char *line = sb_take(&cur);
        if (n == cap) {
            char **grown = (char **)realloc(lines, cap * 2 * sizeof *lines);
            if (!grown) { free(line); lines[n] = NULL; *count = n; return lines; }
            lines = grown;
            cap *= 2;
        }
        lines[n++] = line ? line : pl_dup("");
    } else {
        free(cur.buf);
    }
    *count = n;
    return lines;
}

static void free_lines(char **lines, size_t n) {
    if (!lines) return;
    for (size_t i = 0; i < n; i++) free(lines[i]);
    free(lines);
}

static int starts_with_any(const char *s, const char *const *prefixes, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (!strncmp(s, prefixes[i], strlen(prefixes[i]))) return 1;
    return 0;
}

static json_t *heuristic_summary(const char *transcript_text) {
    static const char *const action_prefixes[] = {"action:", "todo:", "next step:", "follow up:"};
    size_t n = 0, i;
    char **lines = split_lines(transcript_text, &n);
    char **clean = (char **)malloc((n ? n : 1) * sizeof *clean);
    size_t kept = 0;
    json_t *out = json_object(), *decisions = json_array(), *actions = json_array(), *risks = json_array();
    char *summary = NULL;

    if (!clean) { free_lines(lines, n); return out; }
    for (i = 0; i < n; i++) {
        char *stripped = pl_strip(lines[i]);
        if (stripped && *stripped) clean[kept++] = pl_strip_chars(lines[i], " -*\t");
        free(stripped);
    }
    {
        pl_sb b;
        sb_init(&b);
        for (i = 0; i < kept && i < 3; i++) {
            if (i) sb_putc(&b, ' ');
            sb_puts(&b, clean[i]);
        }
        summary = sb_take(&b);
        if (strlen(summary) > W33_PIPELINE_540ECF_P2_HEURISTIC_CLIP)
            summary[W33_PIPELINE_540ECF_P2_HEURISTIC_CLIP] = '\0';
        if (!*summary) {
            free(summary);
            summary = pl_dup("Transcript unavailable or too sparse for a confident summary.");
        }
    }
    json_set(out, "summary", json_string(summary));
    free(summary);

    for (i = 0; i < kept; i++) {
        const char *line = clean[i];
        char *low = pl_lower(line);
        if (!low) continue;
        if ((strstr(low, "decide") || strstr(low, "decision")) && json_len(decisions) < 6)
            json_append(decisions, json_string(line));
        if (starts_with_any(low, action_prefixes, sizeof action_prefixes / sizeof *action_prefixes) &&
            json_len(actions) < 8)
            json_append(actions, json_string(line));
        if ((strstr(low, "risk") || strstr(low, "blocker")) && json_len(risks) < 6)
            json_append(risks, json_string(line));
        free(low);
    }
    json_set(out, "key_decisions", decisions);
    json_set(out, "action_items", actions);
    json_set(out, "risks", risks);

    {
        char *trimmed = pl_strip(transcript_text ? transcript_text : "");
        size_t len = trimmed ? strlen(trimmed) : 0;
        json_set(out, "confidence", json_string(len < 300 ? "low" : "medium"));
        free(trimmed);
    }
    json_set(out, "confidence_notes",
             json_string("Generated with heuristic fallback because no LLM summary response was available."));

    for (i = 0; i < kept; i++) free(clean[i]);
    free(clean);
    free_lines(lines, n);
    return out;
}

json_t *w33_pipeline_540ecf_p2_heuristic_summary(const json_t *request) {
    char *text = pl_str_or_empty(pl_get(request, "transcript_text"));
    json_t *out = heuristic_summary(text);
    free(text);
    return out;
}

static json_t *clean_items(const json_t *values) {
    json_t *out = json_array();
    size_t n = (values && values->type == JSON_ARRAY) ? json_len(values) : 0;
    for (size_t i = 0; i < n; i++) {
        char *raw = pl_str_of(json_get(values, i));
        char *s = pl_strip(raw);
        free(raw);
        if (s && *s) json_append(out, json_string(s));
        free(s);
    }
    return out;
}

json_t *w33_pipeline_540ecf_p2_parse_summary(const json_t *request) {
    char *content = pl_str_or_empty(pl_get(request, "content"));
    char *text = pl_strip(content);
    json_t *out = json_object();

    free(content);
    if (!text || !*text) {
        free(text);
        return heuristic_summary("");
    }
    {
        const char *start = strchr(text, '{');
        const char *end = strrchr(text, '}');
        if (start && end && end > start) {
            char *inner = pl_dup_n(start, (size_t)(end - start + 1));
            free(text);
            text = inner;
        }
    }
    {
        json_t *payload = json_parse(text, NULL);
        char *s;
        free(text);
        if (!payload || payload->type != JSON_OBJECT) {
            json_free(payload);
            return heuristic_summary("");
        }
        s = pl_strip(pl_cstr(json_obj_get(payload, "summary")));
        json_set(out, "summary", json_string(s ? s : ""));
        free(s);
        json_set(out, "key_decisions", clean_items(json_obj_get(payload, "key_decisions")));
        json_set(out, "action_items", clean_items(json_obj_get(payload, "action_items")));
        json_set(out, "risks", clean_items(json_obj_get(payload, "risks")));
        {
            const json_t *cv = json_obj_get(payload, "confidence");
            char *raw = pl_str_or_empty(cv);
            char *v = pl_strip(raw);
            free(raw);
            if (!v || !*v) { free(v); v = pl_dup("medium"); }
            json_set(out, "confidence", json_string(v));
            free(v);
        }
        {
            char *raw = pl_str_or_empty(json_obj_get(payload, "confidence_notes"));
            char *v = pl_strip(raw);
            free(raw);
            json_set(out, "confidence_notes", json_string(v ? v : ""));
            free(v);
        }
        json_free(payload);
    }
    return out;
}

/* ================================================================== */
/* p2: participants / call_metrics                                     */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p2_participants(const json_t *meeting_ref) {
    const json_t *p = pl_get(pl_get(meeting_ref, "metadata"), "participants");
    json_t *out = json_array();
    size_t n;

    if (!pl_truthy(p) || p->type != JSON_ARRAY) return out;
    n = json_len(p);
    for (size_t i = 0; i < n; i++) {
        const json_t *item = json_get(p, i);
        const json_t *name = NULL;
        char *s;
        if (!item || item->type != JSON_OBJECT) continue;
        name = json_obj_get(item, "displayName");
        if (!pl_truthy(name)) {
            const json_t *identity = json_obj_get(item, "identity");
            if (identity && identity->type == JSON_OBJECT) {
                const json_t *user = json_obj_get(identity, "user");
                if (user && user->type == JSON_OBJECT) name = json_obj_get(user, "displayName");
            }
        }
        if (!pl_truthy(name)) continue;
        s = pl_str_of(name);
        if (s && *s) json_append(out, json_string(s));
        free(s);
    }
    return out;
}

json_t *w33_pipeline_540ecf_p2_call_metrics(const json_t *artifacts) {
    json_t *out = json_object();
    size_t n = (artifacts && artifacts->type == JSON_ARRAY) ? json_len(artifacts) : 0;

    for (size_t i = 0; i < n; i++) {
        const json_t *a = json_get(artifacts, i);
        const json_t *type = pl_get(a, "artifact_type");
        if (!type || strcmp(pl_cstr(type), "call_record")) continue;
        {
            const json_t *metrics = pl_get(pl_get(a, "metadata"), "metrics");
            size_t m = (metrics && metrics->type == JSON_OBJECT) ? json_object_size(metrics) : 0;
            for (size_t k = 0; k < m; k++) {
                const char *key = json_object_get_key_at(metrics, k);
                if (key) json_set(out, key, json_copy(json_object_get_at(metrics, k)));
            }
        }
    }
    json_set(out, "artifact_count", json_number((double)n));
    return out;
}

/* ================================================================== */
/* p2: payload_targets                                                 */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p2_payload_targets(const json_t *request) {
    const json_t *meeting_ref = pl_get(request, "meeting_ref");
    const json_t *metadata = pl_get(meeting_ref, "metadata");
    const json_t *subject = pl_get(metadata, "subject");
    const json_t *notion = pl_get(request, "notion");
    const json_t *linear = pl_get(request, "linear");
    const json_t *teams = pl_get(request, "teams_delivery");
    const json_t *channel = pl_get(teams, "channel_id");
    const json_t *chat = pl_get(teams, "chat_id");
    json_t *out = json_object();
    char *title;

    if (pl_truthy(subject)) {
        title = pl_str_of(subject);
    } else {
        char *id = pl_str_or_empty(pl_get(meeting_ref, "meeting_id"));
        pl_sb b;
        sb_init(&b);
        sb_puts(&b, "Meeting ");
        sb_puts(&b, id ? id : "");
        free(id);
        title = sb_take(&b);
    }
    json_set(out, "title", json_string(title ? title : ""));
    free(title);
    {
        const json_t *start = pl_get(metadata, "startDateTime");
        const json_t *end = pl_get(metadata, "endDateTime");
        json_set(out, "start_time", pl_truthy(start) ? json_copy(start) : json_null());
        json_set(out, "end_time", pl_truthy(end) ? json_copy(end) : json_null());
    }
    {
        const json_t *db = pl_get(notion, "database_id");
        json_set(out, "notion_target", pl_truthy(db) ? json_copy(db) : json_null());
    }
    {
        const json_t *team = pl_get(linear, "team_id");
        json_set(out, "linear_target", pl_truthy(team) ? json_copy(team) : json_null());
    }
    json_set(out, "teams_target", pl_truthy(channel) ? json_copy(channel) : (pl_truthy(chat) ? json_copy(chat) : json_null()));
    return out;
}

/* ================================================================== */
/* p2: transcript_decision                                             */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p2_transcript_decision(const json_t *request) {
    const json_t *config = pl_get(request, "config");
    int preferred = pl_truthy(pl_get(config, "transcript_preferred"));
    int required = pl_truthy(pl_get(config, "transcript_required"));
    int fallback = pl_truthy(pl_get(config, "transcription_fallback"));
    double min_chars = pl_int_of(pl_get(config, "transcript_min_chars"), 80.0);
    int has_artifact = pl_truthy(pl_get(request, "transcript_artifact"));
    char *text = pl_str_or_empty(pl_get(request, "transcript_text"));
    char *meeting_id = pl_str_or_empty(pl_get(request, "meeting_id"));
    double recordings = pl_int_of(pl_get(request, "recordings"), 0.0);
    const char *status = pl_cstr(pl_get(request, "status"));
    char status_buf[32];
    json_t *out = json_object();
    const char *strategy = NULL;
    const char *error_type = NULL;
    char *error_message = NULL;

    if (!*status) status = "received";
    snprintf(status_buf, sizeof status_buf, "%s", status);

    if (preferred) {
        snprintf(status_buf, sizeof status_buf, "fetching_transcript");
        if (has_artifact && text && *text) {
            char *trimmed = pl_strip(text);
            double len = trimmed ? (double)strlen(trimmed) : 0.0;
            free(trimmed);
            if (len < min_chars) { free(text); text = pl_dup(""); }
        }
    } else {
        /* fetch_preferred_transcript_text is never called, so no text exists. */
        free(text);
        text = pl_dup("");
    }
    if (text && *text) {
        strategy = "transcript_first";
        free(text);
        json_set(out, "status", json_string(status_buf));
        json_set(out, "strategy", json_string(strategy));
        json_set(out, "error_type", json_null());
        json_set(out, "error_message", json_null());
        free(meeting_id);
        return out;
    }
    free(text);
    if (required) {
        error_type = "TeamsPipelineRetryableError";
        {
            pl_sb b;
            sb_init(&b);
            sb_printf(&b, "Transcript unavailable for meeting %s.", meeting_id ? meeting_id : "");
            error_message = sb_take(&b);
        }
    } else if (!fallback) {
        error_type = "TeamsPipelineArtifactNotFoundError";
        {
            pl_sb b;
            sb_init(&b);
            sb_printf(&b, "No transcript available and transcription fallback disabled for %s.",
                      meeting_id ? meeting_id : "");
            error_message = sb_take(&b);
        }
    } else {
        snprintf(status_buf, sizeof status_buf, "downloading_recording");
        if (recordings < 1.0) {
            error_type = "TeamsPipelineRetryableError";
            {
                pl_sb b;
                sb_init(&b);
                sb_printf(&b, "Recording unavailable for meeting %s.", meeting_id ? meeting_id : "");
                error_message = sb_take(&b);
            }
        } else {
            strategy = "recording_stt_fallback";
        }
    }

    if (error_type) {
        /* run_job() catches the retryable family and persists retry_scheduled. */
        snprintf(status_buf, sizeof status_buf, "retry_scheduled");
    }
    json_set(out, "status", json_string(status_buf));
    json_set(out, "strategy", strategy ? json_string(strategy) : json_null());
    json_set(out, "error_type", error_type ? json_string(error_type) : json_null());
    json_set(out, "error_message", error_message ? json_string(error_message) : json_null());
    free(error_message);
    free(meeting_id);
    return out;
}

/* ================================================================== */
/* p2: recording_name                                                  */
/* ================================================================== */

/* Path(...).name: last component of a '/'- or '\'-separated path. */
static char *path_name(const char *path) {
    size_t len = path ? strlen(path) : 0;
    const char *start;
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) len--;
    if (len == 0) return pl_dup("");
    start = path + len;
    while (start > path && start[-1] != '/' && start[-1] != '\\') start--;
    if (start == path) {
        if (*path == '/' || *path == '\\') return pl_dup("");
        return pl_dup(path);
    }
    return pl_dup_n(start, (size_t)(path + len - start));
}

/* Path(x).suffix */
static char *path_suffix(const char *name) {
    const char *dot = strrchr(name ? name : "", '.');
    if (!dot || dot == (name ? name : "")) return pl_dup("");
    return pl_dup(dot);
}

static int is_audio_suffix(const char *suffix) {
    static const char *const suffixes[] = {".wav", ".mp3", ".m4a", ".ogg", ".flac", ".aac", ".webm"};
    char *low = pl_lower(suffix ? suffix : "");
    int hit = 0;
    if (!low) return 0;
    for (size_t i = 0; i < sizeof suffixes / sizeof *suffixes; i++)
        if (!strcmp(low, suffixes[i])) { hit = 1; break; }
    free(low);
    return hit;
}

json_t *w33_pipeline_540ecf_p2_recording_name(const json_t *request) {
    char *artifact_id = pl_str_or_empty(pl_get(request, "artifact_id"));
    const json_t *dn = pl_get(request, "display_name");
    int ffmpeg = pl_truthy(pl_get(request, "ffmpeg_extract_audio"));
    char *fallback_name;
    char *candidate = NULL, *name = NULL, *suffix = NULL, *audio = NULL;
    json_t *out = json_object();

    fallback_name = (char *)malloc(strlen(artifact_id ? artifact_id : "") + 5);
    if (fallback_name) sprintf(fallback_name, "%s.mp4", artifact_id ? artifact_id : "");

    if (pl_truthy(dn)) {
        char *raw = pl_str_of(dn);
        candidate = path_name(raw ? raw : "");
        free(raw);
    } else {
        candidate = pl_dup(fallback_name ? fallback_name : "");
    }
    if (!candidate || !*candidate || !strcmp(candidate, ".") || !strcmp(candidate, ".."))
        name = pl_dup(fallback_name ? fallback_name : "");
    else
        name = pl_dup(candidate);

    suffix = path_suffix(name);
    if (is_audio_suffix(suffix) || !ffmpeg) {
        audio = pl_dup(name);
        json_set(out, "needs_ffmpeg", json_bool(0));
    } else {
        size_t keep = strlen(name) - strlen(suffix ? suffix : "");
        audio = (char *)malloc(keep + 5);
        if (audio) sprintf(audio, "%.*s.wav", (int)keep, name);
        json_set(out, "needs_ffmpeg", json_bool(1));
    }
    json_set(out, "path", json_string(name ? name : ""));
    json_set(out, "audio_path", json_string(audio ? audio : ""));
    json_set(out, "suffix", json_string(suffix ? suffix : ""));

    free(artifact_id);
    free(fallback_name);
    free(candidate);
    free(name);
    free(suffix);
    free(audio);
    return out;
}

/* ================================================================== */
/* p2: stt_outcome                                                     */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p2_stt_outcome(const json_t *result) {
    json_t *out = json_object();
    if (!pl_truthy(pl_get(result, "success"))) {
        const json_t *ev = pl_get(result, "error");
        char *msg = pl_str_or_empty(ev);
        if (!msg || !*msg) { free(msg); msg = pl_dup("Unknown STT failure"); }
        json_set(out, "outcome", json_string("retry_scheduled"));
        json_set(out, "transcript", json_null());
        json_set(out, "error_type", json_string("TeamsPipelineRetryableError"));
        json_set(out, "error_message", json_string(msg));
        free(msg);
        return out;
    }
    {
        char *raw = pl_str_or_empty(pl_get(result, "transcript"));
        char *trimmed = pl_strip(raw);
        free(raw);
        if (!trimmed || !*trimmed) {
            json_set(out, "outcome", json_string("retry_scheduled"));
            json_set(out, "transcript", json_null());
            json_set(out, "error_type", json_string("TeamsPipelineRetryableError"));
            json_set(out, "error_message", json_string("STT returned an empty transcript."));
            free(trimmed);
            return out;
        }
        json_set(out, "outcome", json_string("transcript"));
        json_set(out, "transcript", json_string(trimmed));
        json_set(out, "error_type", json_null());
        json_set(out, "error_message", json_null());
        free(trimmed);
    }
    return out;
}

/* ================================================================== */
/* p2: sink_plan                                                       */
/* ================================================================== */

json_t *w33_pipeline_540ecf_p2_sink_plan(const json_t *request) {
    static const struct { const char *name; const char *status; const char *config_key; const char *writer_key; } sinks[3] = {
        {"notion", "writing_notion", "notion", "have_notion_writer"},
        {"linear", "writing_linear", "linear", "have_linear_writer"},
        {"teams", "sending_teams", "teams_delivery", "have_teams_sender"},
    };
    char *meeting_id = pl_str_or_empty(pl_get(request, "meeting_id"));
    json_t *out = json_array();

    for (size_t i = 0; i < 3; i++) {
        const json_t *config = pl_get(request, sinks[i].config_key);
        const json_t *enabled = pl_get(config, "enabled");
        const json_t *writer = pl_get(request, sinks[i].writer_key);
        json_t *step;
        pl_sb key;
        if (!pl_truthy(config) || !pl_truthy(enabled) || !pl_truthy(writer)) continue;
        step = json_object();
        json_set(step, "name", json_string(sinks[i].name));
        json_set(step, "status", json_string(sinks[i].status));
        sb_init(&key);
        sb_puts(&key, sinks[i].name);
        sb_putc(&key, ':');
        sb_puts(&key, meeting_id ? meeting_id : "");
        {
            char *k = sb_take(&key);
            json_set(step, "sink_key", json_string(k ? k : ""));
            free(k);
        }
        json_append(out, step);
    }
    free(meeting_id);
    return out;
}
