/*
 * port_gateway_relay_descriptor.c — Port of Python gateway/relay/descriptor.py
 */
#include <stdio.h>
#include "hermes_gateway_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

typedef struct {
    char label[256];
    int max_message_length;
    char emoji[64];
    char platform_hint[1024];
    bool pii_safe;
    char markdown_dialect[64];
    bool supports_draft_streaming;
    bool supports_edit;
    bool supports_threads;
} capability_descriptor_t;

/* PoP: capability_descriptor_from_platform_entry @ gateway/relay/descriptor.py:from_platform_entry */
capability_descriptor_t capability_descriptor_from_platform_entry(
    const char *entry_json, const char *len_unit,
    bool supports_draft, bool supports_edit,
    bool supports_threads, const char *md_dialect) {
    
    capability_descriptor_t desc = {0};
    strncpy(desc.markdown_dialect, md_dialect ? md_dialect : "plain", 63);
    desc.supports_draft_streaming = supports_draft;
    desc.supports_edit = supports_edit;
    desc.supports_threads = supports_threads;
    
    if (!entry_json) return desc;
    
    /* Extract fields from JSON */
    const char *label = strstr(entry_json, "\"label\"");
    if (label) {
        const char *val = strchr(label + 7, '"');
        if (val) {
            val++;
            size_t i = 0;
            while (*val && *val != '"' && i < 255) desc.label[i++] = *val++;
            desc.label[i] = '\0';
        }
    }
    
    return desc;
}

/* PoP: capability_descriptor_from_json @ gateway/relay/descriptor.py:from_json */
capability_descriptor_t capability_descriptor_from_json(const char *data)
{
    /* Deserialize from a handshake JSON string.
     * Unknown keys are ignored (forward-compat).
     * Port of Python gateway/relay/descriptor.py:CapabilityDescriptor.from_json(). */
    capability_descriptor_t desc = {0};
    if (!data || !*data) return desc;

    json_t *raw = json_parse(data, NULL);
    if (!raw || raw->type != JSON_OBJECT) {
        if (raw) json_free(raw);
        return desc;
    }

    json_t *node;
    node = json_object_get(raw, "label");
    if (node && node->type == JSON_STRING)
        strncpy(desc.label, json_node_get_string(node), sizeof(desc.label) - 1);

    node = json_object_get(raw, "max_message_length");
    if (node && node->type == JSON_NUMBER)
        desc.max_message_length = (int)json_node_get_double(node);

    node = json_object_get(raw, "emoji");
    if (node && node->type == JSON_STRING)
        strncpy(desc.emoji, json_node_get_string(node), sizeof(desc.emoji) - 1);

    node = json_object_get(raw, "platform_hint");
    if (node && node->type == JSON_STRING)
        strncpy(desc.platform_hint, json_node_get_string(node), sizeof(desc.platform_hint) - 1);

    node = json_object_get(raw, "pii_safe");
    if (node && node->type == JSON_BOOL)
        desc.pii_safe = node->bool_val;

    node = json_object_get(raw, "markdown_dialect");
    if (node && node->type == JSON_STRING)
        strncpy(desc.markdown_dialect, json_node_get_string(node), sizeof(desc.markdown_dialect) - 1);

    node = json_object_get(raw, "supports_draft_streaming");
    if (node && node->type == JSON_BOOL)
        desc.supports_draft_streaming = node->bool_val;

    node = json_object_get(raw, "supports_edit");
    if (node && node->type == JSON_BOOL)
        desc.supports_edit = node->bool_val;

    node = json_object_get(raw, "supports_threads");
    if (node && node->type == JSON_BOOL)
        desc.supports_threads = node->bool_val;

    json_free(raw);
    return desc;
}
