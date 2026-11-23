/*
 * adapter_hint.c — real C port of upstream Hermes plugins/platforms/matrix/adapter_hint.py constant tables.
 * PoP: cn_291bf91deb_constants @ plugins/platforms/matrix/adapter_hint.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_291bf91deb.h"

json_t *cn_291bf91deb_constants(void) {
    json_t *out = json_object();
    json_set(out, "PLATFORM_HINT", json_string("You are in a Matrix room. Your markdown converts to HTML — bold, italic, code, headings, lists, blockquotes, and links render. Do NOT use tables (popular clients like Element X collapse them into run-on text — use '**Label:** value' lines or bullets), and avoid ||spoilers||, ~~strikethrough~~, and checkboxes (they appear as literal characters). Prefer [descriptive text](url) over bare URLs. You can send files natively: write MEDIA:/absolute/path/to/file in your response. Images send as inline photos, audio (.ogg, .mp3) as voice/audio messages, video (.mp4) inline, other files as attachments."));
    return out;
}
