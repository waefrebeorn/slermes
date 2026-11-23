/*
 * adapter_hint.c — real C port of upstream Hermes plugins/platforms/feishu/adapter_hint.py constant tables.
 * PoP: cn_79db0ebe9f_constants @ plugins/platforms/feishu/adapter_hint.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_79db0ebe9f.h"

json_t *cn_79db0ebe9f_constants(void) {
    json_t *out = json_object();
    json_set(out, "PLATFORM_HINT", json_string("You are in a Feishu (Lark) workspace communicating with your user. Feishu renders Markdown in messages — bold, italic, code blocks, and links are supported. You can send media files natively: include MEDIA:/absolute/path/to/file in your response. Images (.jpg, .png, .webp) are uploaded and displayed inline, audio files as native voice messages (non-Opus formats are transcoded automatically; without ffmpeg they fall back to file attachments), and other files as attachments."));
    return out;
}
