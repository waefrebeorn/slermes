/*
 * adapter_hint.c — real C port of upstream Hermes plugins/platforms/discord/adapter_hint.py constant tables.
 * PoP: cn_2d1baa314e_constants @ plugins/platforms/discord/adapter_hint.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_2d1baa314e.h"

json_t *cn_2d1baa314e_constants(void) {
    json_t *out = json_object();
    json_set(out, "PLATFORM_HINT", json_string("You are in a Discord server or group chat communicating with your user. Discord renders standard markdown natively (bold, italic, code blocks, links); tables are NOT supported — use bullet lists or labeled lines. You can send media files natively: include MEDIA:/absolute/path/to/file in your response. Images (.png, .jpg, .webp) are sent as photo attachments, audio as file attachments. You can also include image URLs in markdown format ![alt](url) and they will be sent as attachments."));
    return out;
}
