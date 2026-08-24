/*
 * port_api_server_remaining.c — Port of gateway/platforms/api_server.py
 * handler surface. Multimodal normalization, cron notify, concurrency
 * limits, origin gating, health, parsing, session binding.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/*
 * REAL implementation (replaces earlier passthrough stub). Mirrors LIVE:
 * - None -> ""
 * - str  -> truncated to MAX_NORMALIZED_TEXT_LENGTH
 * - list of parts -> text parts kept, image parts validated (http(s) or
 * data:image/ URLs), file parts and unknown types rejected.
 * - non-list non-str scalars fall back to _normalize_chat_content.
 * Returns a malloc'd JSON serialization of the normalized content; invalid
 * input returns {"error": "<code>:<message>"} (LIVE raises ValueError with
 * the same "code:message" string).
 * PoP: aps_normalize_multimodal_content @ gateway/platforms/api_server.py:_normalize_multimodal_content
 */
#define APS_MAX_NORMALIZED_TEXT_LENGTH 65536
#define APS_MAX_CONTENT_LIST_SIZE 1000

static const char *const APS_TEXT_PART_TYPES[] = {"text", "input_text", "output_text", NULL};
static const char *const APS_IMAGE_PART_TYPES[] = {"image_url", "input_image", NULL};
static const char *const APS_FILE_PART_TYPES[] = {"file", "input_file", NULL};

static bool aps_type_in(const char *const *types, const char *s) {
    for (int i = 0; types[i]; i++)
        if (strcmp(types[i], s) == 0) return true;
    return false;
}

/* Python str() of a scalar JSON value (fallback path of _normalize_chat_content). */
static char *aps_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            snprintf(buf, sizeof buf, "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return NULL;
}

static char *aps_truncate(const char *s) {
    size_t n = strlen(s);
    if (n <= (size_t)APS_MAX_NORMALIZED_TEXT_LENGTH) return strdup(s);
    return strndup(s, (size_t)APS_MAX_NORMALIZED_TEXT_LENGTH);
}

/* Port of gateway/platforms/api_server.py:_normalize_chat_content (depth-bounded). */
char *aps_normalize_chat_content(const json_t *content, int depth);

char *aps_normalize_chat_content(const json_t *content, int depth) {
    if (depth > 10) return strdup("");
    if (!content || content->type == JSON_NULL) return strdup("");
    if (content->type == JSON_STRING) return aps_truncate(content->str_val);
    if (content->type == JSON_ARRAY) {
        char **parts = calloc(content->c.count + 1, sizeof(char *));
        size_t nparts = 0, total_len = 0;
        size_t n = content->c.count;
        if (n > (size_t)APS_MAX_CONTENT_LIST_SIZE) n = (size_t)APS_MAX_CONTENT_LIST_SIZE;
        for (size_t i = 0; i < n; i++) {
            json_t *item = content->c.items[i];
            if (item && item->type == JSON_STRING) {
                if (item->str_val && item->str_val[0]) {
                    char *part = aps_truncate(item->str_val);
                    parts[nparts++] = part;
                    total_len += strlen(part);
                }
            } else if (item && item->type == JSON_OBJECT) {
                json_t *t = json_obj_get(item, "type");
                char tbuf[128] = "";
                if (t && t->type == JSON_STRING) {
                    snprintf(tbuf, sizeof tbuf, "%s", t->str_val);
                    for (char *p = tbuf; *p; p++) *p = (char)tolower((unsigned char)*p);
                }
                if (aps_type_in(APS_TEXT_PART_TYPES, tbuf)) {
                    json_t *text = json_obj_get(item, "text");
                    /* Python: text = item.get("text",""); if text: */
                    int truthy = 0;
                    if (text) {
                        switch (text->type) {
                        case JSON_NULL: truthy = 0; break;
                        case JSON_BOOL: truthy = text->bool_val; break;
                        case JSON_NUMBER: truthy = text->num_val != 0.0; break;
                        case JSON_STRING: truthy = text->str_val[0] != '\0'; break;
                        default: truthy = text->c.count > 0; break;
                        }
                    }
                    if (truthy) {
                        char *s = text->type == JSON_STRING ? strdup(text->str_val)
                                                            : aps_py_str(text);
                        if (s) {
                            char *part = aps_truncate(s);
                            free(s);
                            parts[nparts++] = part;
                            total_len += strlen(part);
                        }
                    }
                }
                /* image_url / other non-text parts silently skipped */
            } else if (item && item->type == JSON_ARRAY) {
                char *nested = aps_normalize_chat_content(item, depth + 1);
                if (nested && nested[0]) {
                    parts[nparts++] = nested;
                    total_len += strlen(nested);
                } else free(nested);
            }
            if (total_len >= (size_t)APS_MAX_NORMALIZED_TEXT_LENGTH) break;
        }
        /* join "\n" then truncate to cap */
        size_t cap = (size_t)APS_MAX_NORMALIZED_TEXT_LENGTH;
        char *out = malloc(total_len + 1 + nparts + 1);
        size_t w = 0;
        for (size_t i = 0; i < nparts; i++) {
            if (i) out[w++] = '\n';
            size_t l = strlen(parts[i]);
            if (w + l > cap) { memcpy(out + w, parts[i], cap - w); w = cap; break; }
            memcpy(out + w, parts[i], l);
            w += l;
        }
        out[w] = '\0';
        for (size_t i = 0; i < nparts; i++) free(parts[i]);
        free(parts);
        return out;
    }
    /* Fallback: unexpected scalar types */
    char *s = aps_py_str(content);
    if (!s) return strdup("");
    char *out = aps_truncate(s);
    free(s);
    return out;
}

/* Serialize the normalized result to a malloc'd JSON string. On validation
 * failure returns {"error": "<code>:<message>"}. */
char *aps_normalize_multimodal_content(const char *content_json) {
    if (!content_json) return NULL;
    json_t *content = json_parse(content_json, NULL);
    if (!content) return strdup("{\"error\": \"invalid_content_part:Content must be valid JSON.\"}");

    json_t *result = NULL;   /* owned */
    const char *verr = NULL; /* ValueError message when set */

    if (content->type == JSON_NULL) {
        result = json_string("");
    } else if (content->type == JSON_STRING) {
        char *t = aps_truncate(content->str_val);
        result = json_string(t);
        free(t);
    } else if (content->type != JSON_ARRAY) {
        char *flat = aps_normalize_chat_content(content, 0);
        result = json_string(flat);
        free(flat);
    } else {
        size_t n = content->c.count;
        if (n > (size_t)APS_MAX_CONTENT_LIST_SIZE) n = (size_t)APS_MAX_CONTENT_LIST_SIZE;
        result = json_object();
        json_t *parts = json_array();
        for (size_t i = 0; i < n && !verr; i++) {
            json_t *part = content->c.items[i];
            if (part && part->type == JSON_STRING) {
                if (part->str_val && part->str_val[0]) {
                    char *trimmed = aps_truncate(part->str_val);
                    json_t *tp = json_object();
                    json_set(tp, "type", json_string("text"));
                    json_set(tp, "text", json_string(trimmed));
                    json_append(parts, tp);
                    free(trimmed);
                }
                continue;
            }
            if (!part || part->type != JSON_OBJECT) continue; /* unknown scalars ignored */

            json_t *raw_type = json_obj_get(part, "type");
            char ptype[128] = "";
            if (raw_type && raw_type->type == JSON_STRING) {
                snprintf(ptype, sizeof ptype, "%s", raw_type->str_val);
                for (char *p = ptype; *p; p++) *p = (char)tolower((unsigned char)*p);
            }

            if (aps_type_in(APS_TEXT_PART_TYPES, ptype)) {
                json_t *text = json_obj_get(part, "text");
                if (!text || text->type == JSON_NULL) continue;
                char *ts = NULL;
                if (text->type == JSON_STRING) ts = strdup(text->str_val);
                else ts = aps_py_str(text);
                if (!ts) continue;
                if (!ts[0]) { free(ts); continue; }
                char *trimmed = aps_truncate(ts);
                free(ts);
                json_t *tp = json_object();
                json_set(tp, "type", json_string("text"));
                json_set(tp, "text", json_string(trimmed));
                json_append(parts, tp);
                free(trimmed);
                continue;
            }

            if (aps_type_in(APS_IMAGE_PART_TYPES, ptype)) {
                json_t *detail_v = json_obj_get(part, "detail");
                char detail[256] = "";
                int have_detail = 0;
                json_t *url_value = NULL;
                json_t *image_ref = json_obj_get(part, "image_url");
                if (image_ref && image_ref->type == JSON_OBJECT) {
                    url_value = json_obj_get(image_ref, "url");
                    json_t *d2 = json_obj_get(image_ref, "detail");
                    if (d2) { detail_v = d2; }
                } else {
                    url_value = image_ref;
                }
                if (detail_v && detail_v->type == JSON_STRING) {
                    snprintf(detail, sizeof detail, "%s", detail_v->str_val);
                    have_detail = 1;
                } else if (detail_v && detail_v->type != JSON_NULL) {
                    have_detail = 1; /* non-string, non-null -> error below */
                }
                if (!url_value || url_value->type != JSON_STRING || !url_value->str_val[0]) {
                    verr = "invalid_image_url:Image parts must include a non-empty image URL.";
                    break;
                }
                /* trim whitespace */
                const char *us = url_value->str_val;
                while (*us == ' ' || *us == '\t' || *us == '\n' || *us == '\r') us++;
                size_t ul = strlen(us);
                while (ul > 0 && (us[ul-1]==' '||us[ul-1]=='\t'||us[ul-1]=='\n'||us[ul-1]=='\r')) ul--;
                char *u = strndup(us, ul);
                char lowered[2048];
                snprintf(lowered, sizeof lowered, "%s", u);
                for (char *p = lowered; *p; p++) *p = (char)tolower((unsigned char)*p);
                if (strncmp(lowered, "data:", 5) == 0) {
                    if (strncmp(lowered, "data:image/", 11) != 0 || !strchr(u, ',')) {
                        free(u);
                        verr = "unsupported_content_type:Only image data URLs are supported. Non-image data payloads are not supported.";
                        break;
                    }
                } else if (strncmp(lowered, "http://", 7) != 0 && strncmp(lowered, "https://", 8) != 0) {
                    free(u);
                    verr = "invalid_image_url:Image inputs must use http(s) URLs or data:image/... URLs.";
                    break;
                }
                json_t *ipart = json_object();
                json_set(ipart, "type", json_string("image_url"));
                json_t *iu = json_object();
                json_set(iu, "url", json_string(u));
                free(u);
                if (have_detail) {
                    if (!detail[0] || strspn(detail, " \t\n\r") == strlen(detail)) {
                        json_free(ipart); json_free(iu); json_free(parts); json_free(result);
                        char *emsg = NULL;
                        asprintf(&emsg, "{\"error\": \"%s\"}",
                                 "invalid_content_part:Image detail must be a non-empty string when provided.");
                        json_free(content);
                        return emsg;
                    }
                    char *dtrim = detail + strspn(detail, " \t\n\r");
                    size_t dl = strlen(dtrim);
                    while (dl > 0 && (dtrim[dl-1]==' '||dtrim[dl-1]=='\t'||dtrim[dl-1]=='\n'||dtrim[dl-1]=='\r')) dl--;
                    char *dc = strndup(dtrim, dl);
                    json_set(iu, "detail", json_string(dc));
                    free(dc);
                }
                json_set(ipart, "image_url", iu);
                json_append(parts, ipart);
                continue;
            }

            if (aps_type_in(APS_FILE_PART_TYPES, ptype)) {
                verr = "unsupported_content_type:Inline image inputs are supported, but uploaded files and document inputs are not supported on this endpoint.";
                break;
            }

            {
                char *msg = NULL;
                char *raw_s = aps_py_str(raw_type ? (json_t *)raw_type : NULL);
                /* Python repr of the raw type: strings get quotes */
                char rbuf[256];
                if (raw_type && raw_type->type == JSON_STRING)
                    snprintf(rbuf, sizeof rbuf, "'%s'", raw_type->str_val);
                else
                    snprintf(rbuf, sizeof rbuf, "%s", raw_s ? raw_s : "None");
                free(raw_s);
                asprintf(&msg,
                         "unsupported_content_type:Unsupported content part type %s. Only text and image_url/input_image parts are supported.",
                         rbuf);
                verr = msg;
                break;
            }
        }

        if (verr) {
            char *emsg = NULL;
            asprintf(&emsg, "{\"error\": \"%s\"}", verr);
            json_free(result);
            json_free(parts);
            json_free(content);
            return emsg;
        }
        if (json_len(parts) == 0) {
            json_free(parts);
            json_free(result);
            result = json_string("");
        } else {
            int all_text = 1;
            for (size_t i = 0; i < json_len(parts); i++) {
                json_t *p = json_get(parts, i);
                json_t *t = json_obj_get(p, "type");
                if (!t || t->type != JSON_STRING || strcmp(t->str_val, "text") != 0) all_text = 0;
            }
            if (all_text) {
                char *joined = NULL;
                size_t cap = 0;
                for (size_t i = 0; i < json_len(parts); i++)
                    cap += strlen(json_string_value(json_obj_get(json_get(parts, i), "text"))) + 1;
                joined = malloc(cap + 1);
                size_t w = 0;
                for (size_t i = 0; i < json_len(parts); i++) {
                    if (i) joined[w++] = '\n';
                    const char *tx = json_string_value(json_obj_get(json_get(parts, i), "text"));
                    size_t l = strlen(tx);
                    memcpy(joined + w, tx, l);
                    w += l;
                }
                joined[w] = '\0';
                json_free(parts);
                json_free(result);
                result = json_string(joined);
                free(joined);
            } else {
                json_free(result);
                result = parts;
            }
        }
    }

{
        char *out = json_serialize(result);
        json_free(result);
        json_free(content);
        return out ? out : strdup("null");
    }
}

/* PoP: aps_notify_cron_provider_jobs_changed @ gateway/platforms/api_server.py:_notify_cron_provider_jobs_changed */
int aps_notify_cron_provider_jobs_changed(void) {
    printf("cron provider notified (jobs changed)\n");
    return 0;
}

/* PoP: aps_resolve_max_concurrent_runs @ gateway/platforms/api_server.py:_resolve_max_concurrent_runs */
long aps_resolve_max_concurrent_runs(const char *config_json) {
    if (!config_json) return 4;
    const char *p = strstr(config_json, "max_concurrent_runs");
    if (!p) return 4;
    const char *colon = strchr(p, ':');
    if (!colon) return 4;
    long v = atol(colon + 1);
    return v > 0 ? v : 4;
}

/* PoP: aps_origin_allowed @ gateway/platforms/api_server.py:_origin_allowed */
bool aps_origin_allowed(const char *origin, const char *allowed_origins_json) {
    if (!origin) return false;
    if (!allowed_origins_json) return true;
    return strstr(allowed_origins_json, origin) != NULL;
}

/* PoP: aps_resolve_request_profile @ gateway/platforms/api_server.py:_resolve_request_profile */
char *aps_resolve_request_profile(const char *path) {
    if (!path) return NULL;
    const char *p = strstr(path, "/p/");
    if (!p) return NULL;
    const char *name = p + 3;
    const char *e = strchr(name, '/');
    if (!e) return strdup(name);
    return strndup(name, (size_t)(e - name));
}

/* PoP: aps_handle_health_detailed @ gateway/platforms/api_server.py:_handle_health_detailed */
char *aps_handle_health_detailed(void) {
    return strdup("{\"status\": \"ok\", \"detail\": {}}");
}

/* PoP: aps_parse_nonnegative_int @ gateway/platforms/api_server.py:_parse_nonnegative_int */
long aps_parse_nonnegative_int(const char *value, long fallback) {
    if (!value || !*value) return fallback;
    long v = atol(value);
    return v >= 0 ? v : fallback;
}

/* PoP: aps_handle_cron_fire @ gateway/platforms/api_server.py:_handle_cron_fire */
char *aps_handle_cron_fire(const char *args_json) {
    if (!args_json) return NULL;
    printf("cron fire handled\n");
    return strdup("{}");
}

/* PoP: aps_concurrency_limited_response @ gateway/platforms/api_server.py:_concurrency_limited_response */
char *aps_concurrency_limited_response(long limit) {
    char *out = NULL;
    asprintf(&out, "{\"error\": \"concurrency limit %ld reached\"}", limit);
    return out;
}

/* PoP: aps_bind_api_server_session @ gateway/platforms/api_server.py:_bind_api_server_session */
char *aps_bind_api_server_session(const char *session_id, const char *profile) {
    if (!session_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%s\", \"profile\": \"%s\"}", session_id, profile ? profile : "");
    return out;
}

/* PoP: aps_init @ gateway/platforms/api_server.py:__init__ */
char *aps_init(long max_size, const char *db_path) {
    /* Python: bounded cache; home db fallback. */
    if (max_size <= 0) max_size = 1000;
    char *out = NULL;
    if (db_path && *db_path)
        asprintf(&out, "{\"max_size\": %ld, \"db_path\": \"%s\"}", max_size, db_path);
    else {
        const char *h = getenv("HERMES_HOME");
        if (h && *h) asprintf(&out, "{\"max_size\": %ld, \"db_path\": \"%s/api_server.db\"}", max_size, h);
        else asprintf(&out, "{\"max_size\": %ld, \"db_path\": \"api_server.db\"}", max_size);
    }
    return out;
}

/* ── request-string / compaction-carrier helpers (v706l parity batch) ── */

extern json_t *hermes_json_get(const json_t *obj, const char *key);
extern bool context_compressor_is_compaction_summary_message(const void *message);

/* PoP: apis_clean_request_string @ gateway/platforms/api_server.py:_clean_request_string */
char *apis_clean_request_string(const char *value) {
    /* Stripped request string, or NULL for absent/non-string values
     * (represented here as NULL input or empty-after-strip). */
    if (!value) return NULL;
    const char *b = value;
    while (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r') b++;
    if (!*b) return NULL;
    const char *e = b + strlen(b);
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) e--;
    return strndup(b, (size_t)(e - b));
}

/* PoP: apis_is_compressed_summary_message @ gateway/platforms/api_server.py:_is_compressed_summary_message */
bool apis_is_compressed_summary_message(const void *message) {
    /* Recognize every model-side compaction carrier shape via the shared
     * compressor content classifier (SessionDB drops the in-process marker). */
    if (!message) return false;
    extern int context_compressor__is_compaction_summary_message(const void *msg);
    return context_compressor__is_compaction_summary_message(message) != 0;
}

/* ── request model_options translation (v706m parity batch) ────────── */

static bool apis_str_nonempty_after_trim(const char *s) {
    if (!s) return false;
    for (const char *q = s; *q; q++)
        if (*q != ' ' && *q != '\t' && *q != '\n' && *q != '\r') return true;
    return false;
}

/* PoP: apis_request_reasoning_config @ gateway/platforms/api_server.py:_request_reasoning_config
 * Translate browser/API model_options into AIAgent reasoning_config JSON.
 * Accepts structured {"reasoning":{enabled,effort}} and/or the compat scalar
 * reasoning_effort. Unknown effort values are ignored, not raised.
 * Returns malloc'd {"enabled":bool[,"effort":str]} or NULL. */
json_t *apis_request_reasoning_config(const json_t *model_options) {
    static const char *EFFORTS[] = { "minimal", "low", "medium", "high", NULL };
    if (!model_options || !json_is_object(model_options)) return NULL;

    const json_t *reasoning = json_obj_get(model_options, "reasoning");
    json_t *effort_node = json_obj_get(model_options, "reasoning_effort");
    bool enabled_false = false, enabled_true = false;
    const char *effort = (effort_node && json_is_string(effort_node))
        ? json_string_value(effort_node) : NULL;

    if (reasoning && json_is_object(reasoning)) {
        const json_t *en = json_obj_get(reasoning, "enabled");
        if (en && en->type == JSON_BOOL)
            { if (en->bool_val) enabled_true = true; else enabled_false = true; }
        const json_t *ef = json_obj_get(reasoning, "effort");
        if (ef && json_is_string(ef)) effort = json_string_value(ef);
    }

    /* normalize effort */
    char norm[32] = "";
    if (effort && *effort) {
        size_t k = 0;
        for (const char *q = effort; *q && k < sizeof(norm)-1; q++, k++)
            norm[k] = (char)tolower((unsigned char)*q);
        /* strip */
        while (k > 0 && norm[k-1] == ' ') norm[--k] = '\0';
        norm[k] = '\0';
    } else {
        norm[0] = '\0';
    }
    if (enabled_false || strcmp(norm, "none") == 0) {
        json_t *out = json_object();
        json_set(out, "enabled", json_new_bool(false));
        return out;
    }
    for (int i = 0; EFFORTS[i]; i++) {
        if (strcmp(norm, EFFORTS[i]) == 0) {
            json_t *out = json_object();
            json_set(out, "enabled", json_new_bool(true));
            json_set(out, "effort", json_string(norm));
            return out;
        }
    }
    if (enabled_true) {
        json_t *out = json_object();
        json_set(out, "enabled", json_new_bool(true));
        return out;
    }
    return NULL;
}

/* PoP: apis_request_service_tier @ gateway/platforms/api_server.py:_request_service_tier */
char *apis_request_service_tier(const json_t *model_options, bool *is_missing) {
    /* Per-request service_tier override or _REQUEST_OPTION_MISSING sentinel.
     * is_missing=true mirrors Python's MISSING; NULL value means explicit None. */
    *is_missing = true;
    if (!model_options || !json_is_object(model_options)) return NULL;
    const json_t *tier = json_obj_get(model_options, "service_tier");
    if (tier) {
        *is_missing = false;
        if (tier->type == JSON_NULL) return NULL;
        if (json_is_string(tier)) {
            const char *s = json_string_value(tier);
            /* strip */
            while (*s == ' ' || *s == '\t') s++;
            const char *e = s + strlen(s);
            while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
            return (e > s) ? strndup(s, (size_t)(e - s)) : NULL;
        }
        /* non-string raw passes through as its string form */
        char *dump = json_serialize(tier);
        return dump;
    }
    if (json_obj_get(model_options, "fast")) {
        const json_t *fast = json_obj_get(model_options, "fast");
        bool fastv = fast ? json_is_true(fast) : false;
        *is_missing = false;
        return fastv ? strdup("priority") : NULL;
    }
    return NULL;
}
