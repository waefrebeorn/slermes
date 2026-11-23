/*
 * adapter_hint.c — real C port of upstream Hermes plugins/platforms/telegram/adapter_hint.py constant tables.
 * PoP: cn_3745770082_constants @ plugins/platforms/telegram/adapter_hint.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_3745770082.h"

json_t *cn_3745770082_constants(void) {
    json_t *out = json_object();
    json_set(out, "PLATFORM_HINT", json_string("You are on Telegram. Standard Markdown auto-converts: **bold**, *italic*, ~~strikethrough~~, ||spoiler||, `code`, ```blocks```, [links](url), ## headers. Prefer bullets or labeled lines for structured data (no tables). You can send files natively: write MEDIA:/absolute/path/to/file in your response. Images (.png, .jpg, .webp) send as photos, videos (.mp4) play inline; image URLs via ![alt](url) send as photos. Audio: add [[audio_as_voice]] on its own line to send ANY audio file as a native voice bubble (non-Opus transcodes automatically); without it, .mp3/.m4a arrive as audio files, other formats as documents."));
    json_set(out, "PLATFORM_HINT_TOOL_NOTES", ({ json_t *__o = json_object();
        json_set(__o, "clarify", json_string("To offer a small fixed set of choices, call `clarify`: its choices render as tappable buttons that send the answer back to you. Never fake controls with numbered lists or copy-to-clipboard text; those return nothing."));
      __o; }));
    return out;
}
