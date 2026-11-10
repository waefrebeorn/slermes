/*
 * port_image_token_cost.c — Port of agent/image_token_cost.py::count_images and
 * agent/model_metadata.py::_count_image_tokens (the pure counting core).
 */

#include "port_image_token_cost.h"
#include "port_anthropic_credentials.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python: _count_parts(parts, types) — count dict entries whose "type" is one
 * of types, only when parts is a list. */
static int count_parts_types(const json_t *parts, const char *const *types, int n_types)
{
    int c = 0;
    if (!parts || parts->type != JSON_ARRAY)
        return 0;
    for (size_t i = 0; i < parts->c.count; i++) {
        const json_t *part = parts->c.items[i];
        if (!part || part->type != JSON_OBJECT)
            continue;
        const char *type = json_get_str(part, "type", NULL);
        if (!type)
            continue;
        for (int t = 0; t < n_types; t++)
            if (strcmp(type, types[t]) == 0) {
                c++;
                break;
            }
    }
    return c;
}

static int in_set(const char *v, const char **set, int n)
{
    for (int i = 0; i < n; i++)
        if (strcmp(v, set[i]) == 0)
            return 1;
    return 0;
}

/* PoP: pic_count_image_tokens @ agent/model_metadata.py:_count_image_tokens */
int pic_count_image_tokens(const char *msg_json, int cost_per_image)
{
    json_t *msg = msg_json ? json_parse(msg_json, NULL) : NULL;
    if (!msg || msg->type != JSON_OBJECT) {
        if (msg)
            json_free(msg);
        return 0;
    }

    static const char *const image_types[] = { "image", "image_url", "input_image" };
    int count = 0;

    count += count_parts_types(json_obj_get(msg, "content"),
                               image_types,
                               sizeof(image_types) / sizeof(image_types[0]));

    static const char *const ansi_types[] = { "image" };
    count += count_parts_types(json_obj_get(msg, "_anthropic_content_blocks"),
                               ansi_types, 1);

    /* Multimodal tool results that have not been converted yet. */
    json_t *content = json_obj_get(msg, "content");
    if (content && content->type == JSON_OBJECT) {
        json_t *multimodal = json_obj_get(content, "_multimodal");
        int is_multimodal = 0;
        if (multimodal) {
            switch (multimodal->type) {
            case JSON_BOOL: is_multimodal = multimodal->bool_val; break;
            case JSON_NUMBER: is_multimodal = multimodal->num_val != 0.0; break;
            case JSON_STRING: is_multimodal = multimodal->str_val && multimodal->str_val[0]; break;
            default: is_multimodal = multimodal->c.count > 0; break;
            }
        }
        if (is_multimodal) {
            static const char *const mm_types[] = { "image", "image_url" };
            count += count_parts_types(json_obj_get(content, "content"),
                                       mm_types, 2);
        }
    }

    count += count_parts_types(json_obj_get(msg, "output"),
                               image_types,
                               sizeof(image_types) / sizeof(image_types[0]));

    json_free(msg);
    (void)in_set; (void)ansi_types; (void)image_types;
    return count * cost_per_image;
}

/* PoP: pic_count_images @ agent/image_token_cost.py:count_images */
int pic_count_images(const char *messages_json)
{
    json_t *msgs = messages_json ? json_parse(messages_json, NULL) : NULL;
    if (!msgs || msgs->type != JSON_ARRAY) {
        if (msgs)
            json_free(msgs);
        return 0;
    }
    int total = 0;
    for (size_t i = 0; i < msgs->c.count; i++) {
        json_t *m = msgs->c.items[i];
        if (!m || m->type != JSON_OBJECT)
            continue;
        char *ser = json_serialize(m);
        if (ser) {
            total += pic_count_image_tokens(ser, 1);
            free(ser);
        }
    }
    json_free(msgs);
    return total;
}

/* PoP: pic_learned_image_token_cost @ agent/image_token_cost.py:learned_image_token_cost */
char *pic_learned_image_token_cost(const char *model, const char *base_url,
                                   const char *table_json)
{
    /* _key(model, base_url) = f"{model or ''}@{base_url_hostname(base_url or '') or ''}" */
    char *host = anc_base_url_hostname(base_url ? base_url : "");
    json_t *table = table_json ? json_parse(table_json, NULL) : NULL;
    json_t *node = NULL;
    char *key = NULL;
    char *out;
    size_t host_len = host ? strlen(host) : 0;
    size_t model_len = model ? strlen(model) : 0;

    key = malloc(model_len + 1 + host_len + 1);
    if (!key) { free(host); if (table) json_free(table); return NULL; }
    memcpy(key, model ? model : "", model_len);
    key[model_len] = '@';
    memcpy(key + model_len + 1, host ? host : "", host_len);
    key[model_len + 1 + host_len] = '\0';

    if (table && table->type == JSON_OBJECT)
        node = json_obj_get(table, key);

    out = malloc(32);
    if (out) {
        if (node && node->type == JSON_NUMBER)
            sprintf(out, "%lld", (long long)node->num_val);
        else
            sprintf(out, "%d", PIC_DEFAULT_IMAGE_TOKEN_COST);
    }
    free(key);
    free(host);
    if (table) json_free(table);
    return out;
}

