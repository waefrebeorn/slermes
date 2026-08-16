/*
 * port_gateway_relay_descriptor.c — Port of Python gateway/relay/descriptor.py
 */
#include "hermes_logger.h"
#include "hermes_gateway_core.h"
#include "relay_ws_transport.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* capability_descriptor_t now lives in port_gateway_relay_ws_transport.h. */

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

    node = json_object_get(raw, "platform");
    if (node && node->type == JSON_STRING)
        strncpy(desc.platform, json_node_get_string(node), sizeof(desc.platform) - 1);

    node = json_object_get(raw, "supported_ops");
    if (node && node->type == JSON_ARRAY) {
        char *ser = json_serialize(node);
        if (ser) {
            strncpy(desc.supported_ops, ser, sizeof(desc.supported_ops) - 1);
            free(ser);
        }
    }

    json_free(raw);
    return desc;
}

/* PoP: capability_descriptor_supports_op @ gateway/relay/descriptor.py:supports_op */

/* Whether the connector advertises outbound op `op`.
 * Fail-open for legacy connectors: empty supported_ops => legacy op set. */
bool capability_descriptor_supports_op(const capability_descriptor_t *desc, const char *op) {
    if (!op) return false;
    if (desc && desc->supported_ops[0]) {
        /* supported_ops is a JSON array string; check membership */
        const char *s = desc->supported_ops;
        size_t oplen = strlen(op);
        /* simple JSON array member check */
        const char *p = s;
        while ((p = strstr(p, op))) {
            /* verify it's a standalone token between delimiters */
            char before = *(p-1);
            char after = *(p+oplen);
            if ((before == '"' || before == ',' || before == '[' || before == ' ') &&
                (after == '"' || after == ',' || after == ']' || after == ' ' || after == '\0'))
                return true;
            p += oplen;
        }
        return false;
    }
    /* Legacy fallback */
    return strcmp(op, "send") == 0 || strcmp(op, "edit") == 0 ||
           strcmp(op, "typing") == 0 || strcmp(op, "follow_up") == 0;
}

/* PoP: capability_descriptor_to_json @ gateway/relay/descriptor.py:to_json */
char *capability_descriptor_to_json(const capability_descriptor_t *desc) {
    if (!desc) return strdup("{}");
    json_t *obj = json_object();
    if (!obj) return strdup("{}");
    json_set(obj, "label", json_string(desc->label));
    json_set(obj, "platform", json_string(desc->platform));
    json_set(obj, "max_message_length", json_number(desc->max_message_length));
    json_set(obj, "emoji", json_string(desc->emoji));
    json_set(obj, "platform_hint", json_string(desc->platform_hint));
    json_set(obj, "pii_safe", json_bool(desc->pii_safe));
    json_set(obj, "markdown_dialect", json_string(desc->markdown_dialect));
    json_set(obj, "supports_draft_streaming", json_bool(desc->supports_draft_streaming));
    json_set(obj, "supports_edit", json_bool(desc->supports_edit));
    json_set(obj, "supports_threads", json_bool(desc->supports_threads));
    json_set(obj, "supported_ops", json_string(desc->supported_ops));
    char *s = json_serialize(obj);
    json_free(obj);
    return s ? s : strdup("{}");
}
