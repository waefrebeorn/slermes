/*
 * port_gateway_relay_descriptor.c — Port of Python gateway/relay/descriptor.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: from_platform_entry */
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

