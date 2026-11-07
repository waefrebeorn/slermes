/*
 * port_anthropic_message_convert.c — Port of Python:
 * agent/anthropic_message_convert.py (replay whitelists + block shaping).
 */

#include "port_anthropic_message_convert.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int is_blank(const char *text)
{
    if (!text) return 1;
    while (*text) {
        if (!isspace((unsigned char)*text)) return 0;
        text++;
    }
    return 1;
}

/* dict-valued cache_control of a block, or NULL */
static json_t *cache_control_of(json_t *block)
{
    if (!block || block->type != JSON_OBJECT) return NULL;
    json_t *cc = json_obj_get(block, "cache_control");
    return (cc && cc->type == JSON_OBJECT) ? cc : NULL;
}

/* {"type":"text","text":<text>} plus citations when it is a non-empty list */
static json_t *text_block(const char *text, json_t *citations)
{
    json_t *block = json_object();
    json_set(block, "type", json_string("text"));
    json_set(block, "text", json_string(text ? text : ""));
    if (citations && citations->type == JSON_ARRAY && citations->c.count > 0)
        json_set(block, "citations", json_copy(citations));
    return block;
}

/* PoP: amc_carry_cache_control @ agent/anthropic_message_convert.py:_carry_cache_control */
json_t *amc_carry_cache_control(const char *out_json, const char *block_json,
                                int copy_marker)
{
    json_t *out = out_json ? json_parse(out_json, NULL) : NULL;
    if (!out || out->type != JSON_OBJECT) {
        if (out) json_free(out);
        return NULL;
    }
    json_t *block = block_json ? json_parse(block_json, NULL) : NULL;
    /* Python's copy=False shares the marker object with the caller; in the C
     * ownership model json_set steals, so the marker is always copied. */
    (void)copy_marker;
    json_t *cc = cache_control_of(block);
    if (cc) json_set(out, "cache_control", json_copy(cc));
    if (block) json_free(block);
    return out;
}

/* PoP: amc_split_blank_text_blocks @ agent/anthropic_message_convert.py:_split_blank_text_blocks */
json_t *amc_split_blank_text_blocks(const char *blocks_json)
{
    json_t *result = json_object();
    json_t *kept = json_array();
    json_t *dropped = json_array();
    json_t *blocks = blocks_json ? json_parse(blocks_json, NULL) : NULL;

    json_t *relocated = NULL;
    if (blocks && blocks->type == JSON_ARRAY) {
        for (size_t i = 0; i < blocks->c.count; i++) {
            json_t *block = blocks->c.items[i];
            int blank = 0;
            if (block && block->type == JSON_OBJECT) {
                const char *type = json_get_str(block, "type", NULL);
                json_t *text = json_obj_get(block, "text");
                int text_blank = !(text && text->type == JSON_STRING &&
                                   !is_blank(text->str_val));
                blank = type && strcmp(type, "text") == 0 && text_blank;
            }
            if (blank) {
                json_append(dropped, json_number((double)i));
                json_t *cc = cache_control_of(block);
                if (cc) {
                    if (relocated) json_free(relocated);
                    relocated = json_copy(cc);
                }
            } else {
                json_append(kept, json_copy(block));
            }
        }
    }
    if (blocks) json_free(blocks);

    json_set(result, "kept", kept);
    json_set(result, "relocated", relocated ? relocated : json_null());
    json_set(result, "dropped", dropped);
    return result;
}

/* image/<subtype> of a data:image URL the API rejects, or NULL. */
static char *unsupported_media_type(const char *url)
{
    static const char *const SUPPORTED[] = {
        "image/jpeg", "image/png", "image/gif", "image/webp",
    };
    if (!url) return NULL;
    if (strncmp(url, "data:image/", 11) != 0) {
        /* header test is case-insensitive in Python (lower() on the header) */
        if (strncasecmp(url, "data:image/", 11) != 0) return NULL;
    }

    const char *start = url + 11;
    size_t n = strcspn(start, ";,");
    char subtype[64];
    if (n >= sizeof(subtype)) n = sizeof(subtype) - 1;
    memcpy(subtype, start, n);
    subtype[n] = '\0';
    /* strip whitespace */
    size_t s = 0, e = strlen(subtype);
    while (s < e && isspace((unsigned char)subtype[s])) s++;
    while (e > s && isspace((unsigned char)subtype[e - 1])) e--;
    memmove(subtype, subtype + s, e - s);
    subtype[e - s] = '\0';
    if (!subtype[0]) snprintf(subtype, sizeof(subtype), "unknown");
    for (char *q = subtype; *q; q++) *q = (char)tolower((unsigned char)*q);

    char media[80];
    if (strcmp(subtype, "jpg") == 0) snprintf(media, sizeof(media), "image/jpeg");
    else snprintf(media, sizeof(media), "image/%s", subtype);

    for (size_t i = 0; i < sizeof(SUPPORTED) / sizeof(SUPPORTED[0]); i++)
        if (strcmp(media, SUPPORTED[i]) == 0) return NULL;
    return dup_str(media);
}

/* PoP: amc_image_block_from_openai_url @ agent/anthropic_message_convert.py:_image_block_from_openai_url */
json_t *amc_image_block_from_openai_url(const char *url_in)
{
    const char *url = url_in ? url_in : "";
    while (*url && isspace((unsigned char)*url)) url++;
    size_t len = strlen(url);
    while (len > 0 && isspace((unsigned char)url[len - 1])) len--;
    char *trimmed = (char *)malloc(len + 1);
    if (!trimmed) return NULL;
    memcpy(trimmed, url, len);
    trimmed[len] = '\0';

    json_t *out = NULL;
    if (strncmp(trimmed, "data:", 5) != 0) {
        out = json_object();
        json_set(out, "type", json_string("image"));
        json_t *source = json_object();
        json_set(source, "type", json_string("url"));
        json_set(source, "url", json_string(trimmed));
        json_set(out, "source", source);
        free(trimmed);
        return out;
    }

    char *unsupported = unsupported_media_type(trimmed);
    if (unsupported) {
        char message[256];
        snprintf(message, sizeof(message),
                 "[image omitted: %s is not a supported image format]", unsupported);
        free(unsupported);
        out = text_block(message, NULL);
        free(trimmed);
        return out;
    }

    /* header, data = url.partition(",") */
    const char *comma = strchr(trimmed, ',');
    const char *data = comma ? comma + 1 : "";
    size_t header_len = comma ? (size_t)(comma - trimmed) : strlen(trimmed);
    char *header = (char *)malloc(header_len + 1);
    if (!header) { free(trimmed); return NULL; }
    memcpy(header, trimmed, header_len);
    header[header_len] = '\0';

    /* mime_part = header[len("data:"):].split(";", 1)[0].strip() */
    const char *mime_src = header + 5;
    size_t mime_len = strcspn(mime_src, ";");
    char mime[80];
    if (mime_len >= sizeof(mime)) mime_len = sizeof(mime) - 1;
    memcpy(mime, mime_src, mime_len);
    mime[mime_len] = '\0';
    size_t s = 0, e = strlen(mime);
    while (s < e && isspace((unsigned char)mime[s])) s++;
    while (e > s && isspace((unsigned char)mime[e - 1])) e--;
    memmove(mime, mime + s, e - s);
    mime[e - s] = '\0';

    char media[96];
    if (strncmp(mime, "image/", 6) == 0) snprintf(media, sizeof(media), "%s", mime);
    else snprintf(media, sizeof(media), "image/jpeg");
    if (strcasecmp(media, "image/jpg") == 0) snprintf(media, sizeof(media), "image/jpeg");

    out = json_object();
    json_set(out, "type", json_string("image"));
    json_t *source = json_object();
    json_set(source, "type", json_string("base64"));
    json_set(source, "media_type", json_string(media));
    json_set(source, "data", json_string(data));
    json_set(out, "source", source);

    free(header);
    free(trimmed);
    return out;
}

/* PoP: amc_safe_text @ agent/anthropic_message_convert.py:_safe_text */
char *amc_safe_text(const char *text)
{
    if (!text) return dup_str(AMC_EMPTY_TEXT_PLACEHOLDER);
    return is_blank(text) ? dup_str(AMC_EMPTY_TEXT_PLACEHOLDER) : dup_str(text);
}

/* PoP: amc_replay_text @ agent/anthropic_message_convert.py:_replay_text */
json_t *amc_replay_text(const char *block_json)
{
    json_t *block = block_json ? json_parse(block_json, NULL) : NULL;
    if (!block || block->type != JSON_OBJECT) {
        if (block) json_free(block);
        return NULL;
    }
    json_t *text = json_obj_get(block, "text");
    int blank = !(text && text->type == JSON_STRING && !is_blank(text->str_val));
    if (blank) { json_free(block); return NULL; }

    json_t *out = json_object();
    json_set(out, "type", json_string("text"));
    json_set(out, "text", json_string(text->str_val));
    json_t *citations = json_obj_get(block, "citations");
    if (citations && citations->type == JSON_ARRAY && citations->c.count > 0)
        json_set(out, "citations", json_copy(citations));
    json_t *cc = cache_control_of(block);
    if (cc) json_set(out, "cache_control", json_copy(cc));
    json_free(block);
    return out;
}

/* PoP: amc_replay_thinking @ agent/anthropic_message_convert.py:_replay_thinking */
json_t *amc_replay_thinking(const char *block_json)
{
    json_t *block = block_json ? json_parse(block_json, NULL) : NULL;
    if (!block || block->type != JSON_OBJECT) {
        if (block) json_free(block);
        return NULL;
    }
    json_t *out = json_object();
    json_set(out, "type", json_string("thinking"));
    const char *thinking = json_get_str(block, "thinking", "");
    json_set(out, "thinking", json_string(thinking ? thinking : ""));
    json_t *signature = json_obj_get(block, "signature");
    if (signature && json_type(signature) == JSON_STRING && signature->str_val &&
        signature->str_val[0])
        json_set(out, "signature", json_string(signature->str_val));
    json_free(block);
    return out;
}

/* PoP: amc_replay_redacted_thinking @ agent/anthropic_message_convert.py:_replay_redacted_thinking */
json_t *amc_replay_redacted_thinking(const char *block_json)
{
    json_t *block = block_json ? json_parse(block_json, NULL) : NULL;
    if (!block || block->type != JSON_OBJECT) {
        if (block) json_free(block);
        return NULL;
    }
    json_t *data = json_obj_get(block, "data");
    int present = 0;
    if (data && data->type == JSON_STRING) present = data->str_val && data->str_val[0];
    else if (data && data->type == JSON_NUMBER) present = data->num_val != 0.0;
    else if (data && data->type == JSON_ARRAY) present = data->c.count > 0;
    else if (data && data->type == JSON_OBJECT) present = data->c.count > 0;
    else if (data && data->type == JSON_BOOL) present = data->bool_val;

    json_t *out = NULL;
    if (present) {
        out = json_object();
        json_set(out, "type", json_string("redacted_thinking"));
        json_set(out, "data", json_copy(data));
    }
    json_free(block);
    return out;
}

/* PoP: amc_replay_tool_use @ agent/anthropic_message_convert.py:_replay_tool_use */
json_t *amc_replay_tool_use(const char *block_json)
{
    json_t *block = block_json ? json_parse(block_json, NULL) : NULL;
    if (!block || block->type != JSON_OBJECT) {
        if (block) json_free(block);
        return NULL;
    }
    /* _sanitize_tool_id: [^a-zA-Z0-9_-] -> "_", never empty */
    const char *raw_id = json_get_str(block, "id", "");
    char id_buf[256];
    size_t o = 0;
    for (const char *q = raw_id ? raw_id : ""; *q && o + 1 < sizeof(id_buf); q++) {
        unsigned char ch = (unsigned char)*q;
        int ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                 (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
        id_buf[o++] = ok ? (char)ch : '_';
    }
    id_buf[o] = '\0';
    if (o == 0) snprintf(id_buf, sizeof(id_buf), "tool_0");

    json_t *out = json_object();
    json_set(out, "type", json_string("tool_use"));
    json_set(out, "id", json_string(id_buf));
    const char *name = json_get_str(block, "name", "");
    json_set(out, "name", json_string(name ? name : ""));
    json_t *input = json_obj_get(block, "input");
    json_set(out, "input", input ? json_copy(input) : json_object());
    json_t *cc = cache_control_of(block);
    if (cc) json_set(out, "cache_control", json_copy(cc));
    json_free(block);
    return out;
}

/* PoP: amc_replay_image @ agent/anthropic_message_convert.py:_replay_image */
json_t *amc_replay_image(const char *block_json)
{
    json_t *block = block_json ? json_parse(block_json, NULL) : NULL;
    if (!block || block->type != JSON_OBJECT) {
        if (block) json_free(block);
        return NULL;
    }
    json_t *source = json_obj_get(block, "source");
    json_t *out = NULL;
    if (source && source->type == JSON_OBJECT) {
        out = json_object();
        json_set(out, "type", json_string("image"));
        json_set(out, "source", json_copy(source));
    }
    json_free(block);
    return out;
}

/* PoP: amc_sanitize_replay_block @ agent/anthropic_message_convert.py:_sanitize_replay_block */
json_t *amc_sanitize_replay_block(const char *block_json)
{
    json_t *block = block_json ? json_parse(block_json, NULL) : NULL;
    if (!block || block->type != JSON_OBJECT) {
        if (block) json_free(block);
        return NULL;
    }
    /* copy the type before freeing the document: json_get_str is borrowed */
    const char *borrowed = json_get_str(block, "type", NULL);
    char type[64];
    snprintf(type, sizeof(type), "%s", borrowed ? borrowed : "");
    json_free(block);
    if (!type[0]) return NULL;

    if (strcmp(type, "text") == 0) return amc_replay_text(block_json);
    if (strcmp(type, "thinking") == 0) return amc_replay_thinking(block_json);
    if (strcmp(type, "redacted_thinking") == 0) return amc_replay_redacted_thinking(block_json);
    if (strcmp(type, "tool_use") == 0) return amc_replay_tool_use(block_json);
    if (strcmp(type, "image") == 0) return amc_replay_image(block_json);
    return NULL;
}
