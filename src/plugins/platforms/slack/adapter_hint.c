/*
 * adapter_hint.c — real C port of upstream Hermes plugins/platforms/slack/adapter_hint.py constant tables.
 * PoP: cn_610166e97e_constants @ plugins/platforms/slack/adapter_hint.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_610166e97e.h"

json_t *cn_610166e97e_constants(void) {
    json_t *out = json_object();
    json_set(out, "PLATFORM_HINT", json_string("You are in a Slack workspace communicating with your user. Standard markdown is auto-converted to Slack formatting (bold, headers, links, code); tables are NOT supported — use bullet lists or labeled lines. You can send media files natively: include MEDIA:/absolute/path/to/file in your response. Images (.png, .jpg, .webp) are uploaded as photo attachments, audio as file attachments. You can also include image URLs in markdown format ![alt](url) and they will be uploaded as attachments."));
    return out;
}
