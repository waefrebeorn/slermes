/*
 * adapter_vault_code.c -- real C11 port of upstream Hermes
 * plugins/platforms/discord/adapter_vault_code.py: the Discord card for the
 * browser vault's one-time-code prompt.
 *
 * PoP: w20_adaptervau_8ec608_frame_kept @ plugins/platforms/discord/adapter_vault_code.py:_DropVaultCodeFrames.filter
 * PoP: w20_adaptervau_8ec608_truncate_utf16 @ gateway/platforms/base.py:_prefix_within_utf16_limit
 * PoP: w20_adaptervau_8ec608_check_auth @ plugins/platforms/discord/adapter_vault_code.py:VaultCodeView._check_auth
 * PoP: w20_adaptervau_8ec608_submit_plan @ plugins/platforms/discord/adapter_vault_code.py:VaultCodeView.submit_code
 * PoP: w20_adaptervau_8ec608_prompt_content @ plugins/platforms/discord/adapter_vault_code.py:DiscordVaultCodeMixin.send_vault_code_prompt
 *
 * discord.py (Modal/TextInput/Embed), the shared _HermesView base and the i18n
 * catalog stay upstream: the base admission answer, the submit callback's answer
 * and the translated body text are injected, and the card's own policy -- who
 * may open the modal, what a blank code does, which notice/footer key is
 * selected, and the UTF-16 field budget -- is reproduced here.
 */
#include "hermes_json.h"
#include "noc_w20_adaptervau_8ec608.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AVC608_INPUT_ID "hermes_vault_code_value"
#define AVC608_MODAL_TEXT_LIMIT 45
#define AVC608_HINT_LIMIT 300
#define AVC608_NOTICE_SUBMITTED "platform.shared.vault_code.submitted"
#define AVC608_NOTICE_ENDED "platform.shared.vault_code.ended"
#define AVC608_FOOTER_SUBMITTED "platform.shared.vault_code.submitted_footer"
#define AVC608_FOOTER_EXPIRED "platform.discord.prompt.expired_footer"

const char *w20_adaptervau_8ec608_input_id(void) {
    return AVC608_INPUT_ID;
}

int w20_adaptervau_8ec608_modal_text_limit(void) {
    return AVC608_MODAL_TEXT_LIMIT;
}

int w20_adaptervau_8ec608_hint_limit(void) {
    return AVC608_HINT_LIMIT;
}

/* --- UTF-8 helpers -------------------------------------------------------- */

/* Length in bytes of the UTF-8 sequence starting at p (1 on malformed input). */
static size_t avc608_seq_len(const unsigned char *p) {
    unsigned char c = *p;
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* Code point at p and its byte length through *len. */
static unsigned long avc608_codepoint(const unsigned char *p, size_t *len) {
    unsigned char c = *p;
    size_t n = avc608_seq_len(p);
    unsigned long cp;
    size_t i;
    if (n == 1) { *len = 1; return (unsigned long)c; }
    cp = (unsigned long)(c & (0xFFu >> (n + 1)));
    for (i = 1; i < n; i++) {
        if ((p[i] & 0xC0) != 0x80) { *len = 1; return (unsigned long)c; }
        cp = (cp << 6) | (unsigned long)(p[i] & 0x3Fu);
    }
    *len = n;
    return cp;
}

static int avc608_utf16_len(unsigned long cp) {
    return cp > 0xFFFFUL ? 2 : 1;
}

/* --- _DropVaultCodeFrames.filter ------------------------------------------ */

int w20_adaptervau_8ec608_frame_kept(const char *message) {
    if (!message) return 0;
    /* True = keep: frames carrying the code input id are dropped. */
    return strstr(message, AVC608_INPUT_ID) != NULL ? 0 : 1;
}

/* --- _prefix_within_utf16_limit ------------------------------------------- */

json_t *w20_adaptervau_8ec608_truncate_utf16(const char *text, int limit) {
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    const char *start = (const char *)p;
    size_t offset = 0;
    int used = 0;
    if (limit < 0) limit = 0;
    while (*p) {
        size_t n = 0;
        unsigned long cp = avc608_codepoint(p, &n);
        int units = avc608_utf16_len(cp);
        if (used + units > limit) break;
        used += units;
        p += n;
        offset += n;
    }
    {
        char *slice = (char *)malloc(offset + 1);
        json_t *out;
        if (!slice) return json_string("");
        memcpy(slice, start, offset);
        slice[offset] = '\0';
        out = json_string(slice);
        free(slice);
        return out;
    }
}

/* --- VaultCodeView._check_auth -------------------------------------------- */

/* str(getattr(getattr(interaction, "user", None), "id", "")) */
static char *avc608_user_id_text(const json_t *value) {
    char buf[64];
    const char *text = "";
    if (value) {
        switch (value->type) {
        case JSON_STRING: text = value->str_val ? value->str_val : ""; break;
        case JSON_BOOL:   text = value->bool_val ? "True" : "False"; break;
        case JSON_NUMBER:
            if (value->num_val == (double)(long long)value->num_val) {
                snprintf(buf, sizeof(buf), "%lld", (long long)value->num_val);
            } else {
                snprintf(buf, sizeof(buf), "%.17g", value->num_val);
            }
            text = buf;
            break;
        case JSON_NULL:   text = "None"; break;
        default:          text = ""; break;
        }
    }
    {
        char *out = (char *)malloc(strlen(text) + 1);
        if (out) strcpy(out, text);
        return out;
    }
}

int w20_adaptervau_8ec608_check_auth(int admitted, const json_t *interaction_user_id,
                                     const char *user_id) {
    char *actual;
    int same;
    if (!admitted) return 0;                     /* super()._check_auth() refused */
    actual = avc608_user_id_text(interaction_user_id);
    same = (actual && user_id) ? (strcmp(actual, user_id) == 0) : 0;
    free(actual);
    return same;
}

/* --- submit_code ---------------------------------------------------------- */

/* Python str.strip()'s whitespace, ASCII side. */
static int avc608_is_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *avc608_strip(const char *text) {
    const char *start = text ? text : "";
    const char *end;
    size_t n;
    char *out;
    while (*start && avc608_is_space((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && avc608_is_space((unsigned char)end[-1])) end--;
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

json_t *w20_adaptervau_8ec608_submit_plan(const char *code, int admitted, int accepted) {
    json_t *out;
    char *stripped;

    if (!admitted) {
        /* The gate speaks (resolved / not-author); the card stays as it was. */
        out = json_object();
        json_set(out, "gated", json_bool(1));
        return out;
    }
    stripped = avc608_strip(code);
    if (!stripped || !stripped[0]) {
        /* Blank: nothing to enter and nothing ended; the button stays live. */
        free(stripped);
        out = json_object();
        json_set(out, "deferred", json_bool(1));
        return out;
    }
    free(stripped);
    out = json_object();
    json_set(out, "notice",
             json_string(accepted ? AVC608_NOTICE_SUBMITTED : AVC608_NOTICE_ENDED));
    json_set(out, "footer",
             json_string(accepted ? AVC608_FOOTER_SUBMITTED : AVC608_FOOTER_EXPIRED));
    json_set(out, "ephemeral", json_bool(1));
    json_set(out, "disabled", json_bool(1));
    json_set(out, "stopped", json_bool(1));
    json_set(out, "resolved", json_bool(1));
    return out;
}

/* --- send_vault_code_prompt's content ------------------------------------ */

/* First `limit` code points of text (Python's hint[:_HINT_LIMIT] is a code-point
 * slice, not a UTF-16 budget). */
static size_t avc608_prefix_bytes(const char *text, int limit) {
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    size_t offset = 0;
    int count = 0;
    while (*p && count < limit) {
        size_t n = avc608_seq_len(p);
        p += n;
        offset += n;
        count++;
    }
    return offset;
}

json_t *w20_adaptervau_8ec608_prompt_content(const char *body_text, const char *hint) {
    const char *body = body_text ? body_text : "";
    json_t *out;
    char *joined;
    size_t body_len, hint_len, total;
    if (!hint || !hint[0]) return json_string(body);
    body_len = strlen(body);
    hint_len = avc608_prefix_bytes(hint, AVC608_HINT_LIMIT);
    total = body_len + 1 + hint_len;
    joined = (char *)malloc(total + 1);
    if (!joined) return json_string(body);
    memcpy(joined, body, body_len);
    joined[body_len] = '\n';
    memcpy(joined + body_len + 1, hint, hint_len);
    joined[total] = '\0';
    out = json_string(joined);
    free(joined);
    return out;
}
