/*
 * chat_completion_helpers_pure.c
 *
 * Four pure helpers from agent/chat_completion_helpers.py that had no PoP
 * credit anywhere in the tree:
 *
 *   agent_cch_image_part_chars              <- _image_part_chars
 *   agent_cch_payload_chars                 <- _payload_chars
 *   agent_cch_scale_stale_timeout_for_context <- _scale_stale_timeout_for_context
 *   agent_cch_bound_codex_stale_timeout     <- _bound_openai_codex_stale_timeout
 *
 * Kept in their own translation unit rather than appended to
 * src/agent/chat_completion_helpers.c: that object pulls in the whole agent
 * runtime (llm_client, auth store, message), which makes a minimal oracle
 * harness un-linkable. These four depend on nothing but libjson.
 */

#include "port_chat_completion_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: agent_cch_image_part_chars @ agent/chat_completion_helpers.py:_image_part_chars
 *      return image_cost * 4 + (len(text) if isinstance(text, str) else 0)
 *
 * The per-image price learned from provider usage, never the base64 length:
 * a native screenshot priced as text read as ~100K+ tokens and selected the
 * giant-conversation watchdog tiers.
 */
int agent_cch_image_part_chars(const json_t *part, int image_cost)
{
    if (!part || part->type != JSON_OBJECT)
        return image_cost * 4;
    const json_t *text = json_obj_get(part, "text");
    int n = (text && text->type == JSON_STRING && text->str_val)
                ? (int)strlen(text->str_val) : 0;
    return image_cost * 4 + n;
}

static const char *const CCH_IMAGE_PART_TYPES[] = {
    "image_url", "input_image", "image", NULL
};

/* PoP: agent_cch_payload_chars @ agent/chat_completion_helpers.py:_payload_chars */
int agent_cch_payload_chars(const json_t *value, int image_cost)
{
    if (!value || value->type == JSON_NULL)
        return 0;
    if (value->type == JSON_OBJECT) {
        const json_t *part_type = json_obj_get(value, "type");
        if (part_type && part_type->type == JSON_STRING && part_type->str_val) {
            int is_image = 0;
            for (int i = 0; CCH_IMAGE_PART_TYPES[i]; i++)
                if (strcmp(part_type->str_val, CCH_IMAGE_PART_TYPES[i]) == 0)
                    is_image = 1;
            /* Only scalar content-part types can match; a JSON-Schema node may
             * hold a sub-schema or a multi-type list under "type". */
            if (is_image
                && (json_obj_get(value, "image_url") || json_obj_get(value, "image")
                    || json_obj_get(value, "source") || json_obj_get(value, "file_id")))
                return agent_cch_image_part_chars(value, image_cost);
        }
        int total = 0;
        for (size_t i = 0; i < value->c.count; i++) {
            const char *k = value->c.keys[i];
            total += (int)strlen(k ? k : "") + 6
                     + agent_cch_payload_chars(value->c.items[i], image_cost);
        }
        return total;
    }
    if (value->type == JSON_ARRAY) {
        int total = 2 * (int)value->c.count;
        for (size_t i = 0; i < value->c.count; i++)
            total += agent_cch_payload_chars(value->c.items[i], image_cost);
        return total;
    }
    if (value->type == JSON_STRING)
        return value->str_val ? (int)strlen(value->str_val) : 0;
    if (value->type == JSON_BOOL)
        return value->bool_val ? 4 : 5;      /* len(str(True)) / len(str(False)) */
    if (value->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", value->num_val);
        return (int)strlen(buf);
    }
    return 0;
}

/* PoP: agent_cch_scale_stale_timeout_for_context @ agent/chat_completion_helpers.py:_scale_stale_timeout_for_context */
double agent_cch_scale_stale_timeout_for_context(double base, int est_tokens)
{
    if (est_tokens > 100000)
        return base > 300.0 ? base : 300.0;
    if (est_tokens > 50000)
        return base > 240.0 ? base : 240.0;
    return base;
}

/* PoP: agent_cch_bound_codex_stale_timeout @ agent/chat_completion_helpers.py:_bound_openai_codex_stale_timeout */
double agent_cch_bound_codex_stale_timeout(double stale_timeout, int est_tokens,
                                           double hard)
{
    double floor = 0.0;
    if (est_tokens > 100000)
        floor = 1200.0;
    else if (est_tokens > 50000)
        floor = 900.0;
    else if (est_tokens > 10000)
        floor = 600.0;
    if (floor > 0.0 && stale_timeout < floor)
        stale_timeout = floor;
    return (hard > 0.0 && stale_timeout > hard) ? hard : stale_timeout;
}
