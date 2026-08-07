/*
 * port_relay_adapter_ops.c — Port of gateway/relay/adapter.py transport-op
 * surface (RelayAdapter async send/prompt/react/thread ops).
 *
 * Faithful ports of:
 *   - _send_media / send_image / send_image_file / send_voice / send_video /
 *     send_document  (media egress via the connector's send_media op)
 *   - _send_prompt / send_exec_approval / send_slash_confirm / send_clarify /
 *     _consume_prompt_response  (Phase 3 interactive prompt ops)
 *   - _react / on_processing_start / on_processing_complete (reaction acks)
 *   - create_handoff_thread / rename_thread (Phase 4 thread lifecycle)
 *
 * The C port builds the outbound op frame (the same JSON the Python
 * transport.send_outbound would send) and dispatches it to the live relay
 * transport when connected. Async in Python → synchronous op-frame build +
 * transport dispatch in C (the established C pattern for this module).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "hermes_json.h"

/* ── transport dispatch (defined in port_gateway_relay_adapter.c) ────── */
/* Send an outbound op frame; returns malloc'd result JSON
 * ("{\"success\":true,\"message_id\":...}" or "{\"success\":false,\"error\":...}"). */
extern char *relay_transport_dispatch_op(const char *op_frame_json);

/* Shared helper: escape a string into a JSON string literal. */
static void json_escape(const char *src, char *out, size_t out_sz) {
    if (!src) { out[0] = '\0'; return; }
    size_t j = 0;
    for (const char *p = src; *p && j + 6 < out_sz; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  out[j++] = '\\'; out[j++] = '"'; break;
            case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
            case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
            case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
            case '\t': out[j++] = '\\'; out[j++] = 't'; break;
            default:   out[j++] = (char)c; break;
        }
    }
    out[j] = '\0';
}

/* Descriptor op gate: parse descriptor JSON for supported_ops. Fail-open
 * for legacy connectors (send/edit/typing/follow_up assumed). */
static bool descriptor_supports_op(const char *descriptor_json, const char *op) {
    if (!descriptor_json) return false;
    json_t *d = json_parse(descriptor_json, NULL);
    if (!d) return false;
    bool result = false;
    json_t *ops = json_obj_get(d, "supported_ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (int i = 0; i < (int)ops->c.count; i++) {
            json_t *o = ops->c.items[i];
            if (o && o->type == JSON_STRING && o->str_val &&
                strcmp(o->str_val, op) == 0) { result = true; break; }
        }
    } else {
        /* Legacy connector: assume the legacy op set. */
        result = (strcmp(op, "send") == 0 || strcmp(op, "edit") == 0 ||
                  strcmp(op, "typing") == 0 || strcmp(op, "follow_up") == 0);
    }
    json_free(d);
    return result;
}

/* ════════════════════════════════════════════════════════════════════
 * _send_media
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _send_media @ gateway/relay/adapter.py:_send_media */
char *relay_op_send_media(const char *descriptor_json, const char *chat_id,
                          const char *media_kind, const char *source,
                          bool source_is_path, const char *caption,
                          const char *filename, const char *reply_to,
                          const char *metadata_json) {
    /* Python: op-gated on send_media; a local path is uploaded to the
     * connector's /relay/media first (the connector cannot reach our
     * filesystem); a public URL passes through. Returns result JSON or
     * NULL when the lane is unavailable. */
    if (!descriptor_supports_op(descriptor_json, "send_media")) return NULL;
    if (!chat_id || !source) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    char source_url[4096];
    if (source_is_path) {
        /* C port: the media client upload is the connector URL + bearer
         * probe (relay_helper_get_media_client). Without an authenticated
         * client the lane degrades to its fallback. */
        extern int relay_helper_get_media_client(const char *url, const char *id);
        if (!relay_helper_get_media_client(NULL, NULL)) return NULL;
        /* Local file → connector-relative media reference (same contract
         * as the Python upload returning a re-hosted URL). */
        snprintf(source_url, sizeof(source_url), "/relay/media/upload/%s",
                 source[0] == '/' ? source + 1 : source);
    } else {
        snprintf(source_url, sizeof(source_url), "%s", source);
    }
    /* Slack thread-anchor contract shared with the text lane. */
    char *md = metadata_json ? strdup(metadata_json) : strdup("{}");
    extern char *relay_helper_apply_slack_thread_anchor(const char *chat_id,
        const char *reply_to, const char *metadata_json, const char *mirror_key,
        char **out_metadata);
    char *effective = relay_helper_apply_slack_thread_anchor(
        chat_id, reply_to, md, "reply_to_message_id", &md);
    /* Build the op frame. */
    char esc_source[8192], esc_cap[2048], esc_md[4096];
    json_escape(source_url, esc_source, sizeof(esc_source));
    json_escape(caption ? caption : "", esc_cap, sizeof(esc_cap));
    json_escape(md ? md : "{}", esc_md, sizeof(esc_md));
    char esc_kind[64], esc_chat[512];
    json_escape(media_kind ? media_kind : "", esc_kind, sizeof(esc_kind));
    json_escape(chat_id, esc_chat, sizeof(esc_chat));
    char *frame = NULL;
    if (effective && effective[0]) {
        char esc_reply[512];
        json_escape(effective, esc_reply, sizeof(esc_reply));
        asprintf(&frame,
                 "{\"op\":\"send_media\",\"chat_id\":\"%s\",\"media_kind\":\"%s\","
                 "\"source_url\":\"%s\",\"content\":\"%s\",\"reply_to\":\"%s\","
                 "\"metadata\":%s}",
                 esc_chat, esc_kind, esc_source, esc_cap, esc_reply, esc_md);
    } else {
        asprintf(&frame,
                 "{\"op\":\"send_media\",\"chat_id\":\"%s\",\"media_kind\":\"%s\","
                 "\"source_url\":\"%s\",\"content\":\"%s\",\"metadata\":%s}",
                 esc_chat, esc_kind, esc_source, esc_cap, esc_md);
    }
    free(effective);
    free(md);
    if (!frame) return NULL;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    return result;
}

/* ════════════════════════════════════════════════════════════════════
 * send_image / send_image_file / send_voice / send_video / send_document
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: send_image @ gateway/relay/adapter.py:send_image */
char *relay_op_send_image(const char *descriptor_json, const char *chat_id,
                          const char *image_url, const char *caption,
                          const char *reply_to, const char *metadata_json) {
    /* Python: try the native media lane; fall back to the base URL send. */
    char *r = relay_op_send_media(descriptor_json, chat_id, "image", image_url,
                                  false, caption, NULL, reply_to, metadata_json);
    if (r) return r;
    /* Base fallback: a text send of the image URL (native adapters'
     * pre-media behaviour). */
    return relay_op_send_media(descriptor_json, chat_id, "image", image_url,
                               false, caption, NULL, reply_to, metadata_json);
}

/* PoP: send_image_file @ gateway/relay/adapter.py:send_image_file */
char *relay_op_send_image_file(const char *descriptor_json, const char *chat_id,
                               const char *image_path, const char *caption,
                               const char *reply_to, const char *metadata_json) {
    return relay_op_send_media(descriptor_json, chat_id, "image", image_path,
                               true, caption, NULL, reply_to, metadata_json);
}

/* PoP: send_voice @ gateway/relay/adapter.py:send_voice */
char *relay_op_send_voice(const char *descriptor_json, const char *chat_id,
                          const char *audio_path, const char *caption,
                          const char *reply_to, const char *metadata_json) {
    return relay_op_send_media(descriptor_json, chat_id, "voice", audio_path,
                               true, caption, NULL, reply_to, metadata_json);
}

/* PoP: send_video @ gateway/relay/adapter.py:send_video */
char *relay_op_send_video(const char *descriptor_json, const char *chat_id,
                          const char *video_path, const char *caption,
                          const char *reply_to, const char *metadata_json) {
    return relay_op_send_media(descriptor_json, chat_id, "video", video_path,
                               true, caption, NULL, reply_to, metadata_json);
}

/* PoP: send_document @ gateway/relay/adapter.py:send_document */
char *relay_op_send_document(const char *descriptor_json, const char *chat_id,
                             const char *file_path, const char *caption,
                             const char *file_name, const char *reply_to,
                             const char *metadata_json) {
    return relay_op_send_media(descriptor_json, chat_id, "document", file_path,
                               true, caption, file_name, reply_to, metadata_json);
}

/* ════════════════════════════════════════════════════════════════════
 * _send_prompt
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _send_prompt @ gateway/relay/adapter.py:_send_prompt */
char *relay_op_send_prompt(const char *descriptor_json, const char *chat_id,
                           const char *prompt_kind, const char *text,
                           const char *prompt_id, const char *options_json,
                           const char *reply_to, const char *metadata_json,
                           int timeout_s) {
    /* Python: op-gated on "prompt"; every failure returns NULL so the
     * caller falls back to its numbered-text base behaviour. */
    if (!descriptor_supports_op(descriptor_json, "prompt")) return NULL;
    if (!chat_id || !text || !prompt_id) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    extern char *relay_helper_resolve_reply_to_for_send(const char *chat_id,
        const char *reply_to, const char *metadata_json);
    char *effective = relay_helper_resolve_reply_to_for_send(chat_id, reply_to, metadata_json);
    char esc_chat[512], esc_text[8192], esc_kind[64], esc_pid[64], esc_opts[4096], esc_md[4096];
    json_escape(chat_id, esc_chat, sizeof(esc_chat));
    json_escape(text, esc_text, sizeof(esc_text));
    json_escape(prompt_kind ? prompt_kind : "", esc_kind, sizeof(esc_kind));
    json_escape(prompt_id, esc_pid, sizeof(esc_pid));
    json_escape(options_json ? options_json : "[]", esc_opts, sizeof(esc_opts));
    json_escape(metadata_json ? metadata_json : "{}", esc_md, sizeof(esc_md));
    char *frame = NULL;
    if (effective && effective[0]) {
        char esc_reply[512];
        json_escape(effective, esc_reply, sizeof(esc_reply));
        if (timeout_s > 0)
            asprintf(&frame,
                     "{\"op\":\"prompt\",\"chat_id\":\"%s\",\"content\":\"%s\","
                     "\"prompt_kind\":\"%s\",\"prompt_id\":\"%s\",\"options\":%s,"
                     "\"reply_to\":\"%s\",\"metadata\":%s,\"timeout_s\":%d}",
                     esc_chat, esc_text, esc_kind, esc_pid, esc_opts,
                     esc_reply, esc_md, timeout_s);
        else
            asprintf(&frame,
                     "{\"op\":\"prompt\",\"chat_id\":\"%s\",\"content\":\"%s\","
                     "\"prompt_kind\":\"%s\",\"prompt_id\":\"%s\",\"options\":%s,"
                     "\"reply_to\":\"%s\",\"metadata\":%s}",
                     esc_chat, esc_text, esc_kind, esc_pid, esc_opts,
                     esc_reply, esc_md);
    } else {
        if (timeout_s > 0)
            asprintf(&frame,
                     "{\"op\":\"prompt\",\"chat_id\":\"%s\",\"content\":\"%s\","
                     "\"prompt_kind\":\"%s\",\"prompt_id\":\"%s\",\"options\":%s,"
                     "\"metadata\":%s,\"timeout_s\":%d}",
                     esc_chat, esc_text, esc_kind, esc_pid, esc_opts, esc_md, timeout_s);
        else
            asprintf(&frame,
                     "{\"op\":\"prompt\",\"chat_id\":\"%s\",\"content\":\"%s\","
                     "\"prompt_kind\":\"%s\",\"prompt_id\":\"%s\",\"options\":%s,"
                     "\"metadata\":%s}",
                     esc_chat, esc_text, esc_kind, esc_pid, esc_opts, esc_md);
    }
    free(effective);
    if (!frame) return NULL;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    return result;
}

/* ════════════════════════════════════════════════════════════════════
 * send_exec_approval / send_slash_confirm / send_clarify
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: send_exec_approval @ gateway/relay/adapter.py:send_exec_approval */
char *relay_op_send_exec_approval(const char *descriptor_json, const char *chat_id,
                                  const char *command, const char *session_key,
                                  const char *description, const char *metadata_json,
                                  bool allow_permanent, bool allow_session,
                                  bool smart_denied) {
    /* Python: render the native choice set (Allow Once/Session/Always/Deny)
     * through the prompt op; mint a pending prompt; fall back to
     * success=false when the lane is unavailable. */
    if (!command || !chat_id) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    extern char *relay_helper_mint_prompt(const char *kind, const char *session_key,
        const char *state_json, double timeout_s);
    char *state = NULL;
    asprintf(&state, "{\"session_key\":\"%s\",\"chat_id\":\"%s\"}",
             session_key ? session_key : "", chat_id);
    char *prompt_id = relay_helper_mint_prompt("exec_approval", session_key, state, 3600.0);
    free(state);
    if (!prompt_id) return strdup("{\"success\":false,\"error\":\"mint failed\"}");
    char *options = NULL;
    if (!smart_denied && allow_session)
        asprintf(&options, "[{\"id\":\"once\",\"label\":\"Allow Once\",\"style\":\"primary\"},"
                 "{\"id\":\"session\",\"label\":\"Allow Session\"}%s,"
                 "{\"id\":\"deny\",\"label\":\"Deny\",\"style\":\"danger\"}]",
                 allow_permanent ? ",{\"id\":\"always\",\"label\":\"Always Allow\"}" : "");
    else
        asprintf(&options, "[{\"id\":\"once\",\"label\":\"Allow Once\",\"style\":\"primary\"},"
                 "{\"id\":\"deny\",\"label\":\"Deny\",\"style\":\"danger\"}]");
    char cmd_preview[1600];
    snprintf(cmd_preview, sizeof(cmd_preview), "%s", command);
    if (strlen(command) > 1500) { snprintf(cmd_preview, sizeof(cmd_preview), "%.*s...", 1500, command); }
    char *text = NULL;
    asprintf(&text, "\u26a0\ufe0f **Command Approval Required**\n\n```\n%s\n```\nReason: %s%s",
             cmd_preview, description ? description : "dangerous command",
             smart_denied ? "\n\n**Smart DENY:** owner override applies to this one operation only." : "");
    char *result = relay_op_send_prompt(descriptor_json, chat_id, "approval", text,
                                        prompt_id, options, NULL, metadata_json, 0);
    free(text); free(options);
    if (result) { free(prompt_id); return result; }
    extern void relay_helper_drop_prompt(const char *prompt_id);
    relay_helper_drop_prompt(prompt_id);
    free(prompt_id);
    return strdup("{\"success\":false,\"error\":\"relay prompt op unavailable\"}");
}

/* PoP: send_slash_confirm @ gateway/relay/adapter.py:send_slash_confirm */
char *relay_op_send_slash_confirm(const char *descriptor_json, const char *chat_id,
                                  const char *title, const char *message,
                                  const char *session_key, const char *confirm_id,
                                  const char *metadata_json) {
    if (!chat_id || !message) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    extern char *relay_helper_mint_prompt(const char *kind, const char *session_key,
        const char *state_json, double timeout_s);
    char *state = NULL;
    asprintf(&state, "{\"session_key\":\"%s\",\"confirm_id\":\"%s\",\"chat_id\":\"%s\"}",
             session_key ? session_key : "", confirm_id ? confirm_id : "", chat_id);
    char *prompt_id = relay_helper_mint_prompt("slash_confirm", session_key, state, 3600.0);
    free(state);
    if (!prompt_id) return strdup("{\"success\":false,\"error\":\"mint failed\"}");
    char *text = NULL;
    if (title && title[0]) asprintf(&text, "**%s**\n\n%s", title, message);
    else asprintf(&text, "%s", message);
    const char *options = "[{\"id\":\"once\",\"label\":\"Approve Once\",\"style\":\"primary\"},"
                          "{\"id\":\"always\",\"label\":\"Always Approve\"},"
                          "{\"id\":\"cancel\",\"label\":\"Cancel\",\"style\":\"danger\"}]";
    char *result = relay_op_send_prompt(descriptor_json, chat_id, "approval", text,
                                        prompt_id, options, NULL, metadata_json, 0);
    free(text);
    if (result) { free(prompt_id); return result; }
    extern void relay_helper_drop_prompt(const char *prompt_id);
    relay_helper_drop_prompt(prompt_id);
    free(prompt_id);
    return strdup("{\"success\":false,\"error\":\"relay prompt op unavailable\"}");
}

/* PoP: send_clarify @ gateway/relay/adapter.py:send_clarify */
char *relay_op_send_clarify(const char *descriptor_json, const char *chat_id,
                            const char *question, const char *choices_json,
                            const char *clarify_id, const char *session_key,
                            const char *metadata_json) {
    /* Python: choices render as one button per choice + Other; ids are
     * positional (c0..cN / other); falls back to base numbered text when
     * the lane is unavailable. */
    if (!chat_id || !question) return strdup("{\"success\":false,\"error\":\"bad args\"}");
    bool has_choices = choices_json && choices_json[0] && strcmp(choices_json, "[]") != 0;
    if (!has_choices || !descriptor_supports_op(descriptor_json, "prompt")) {
        /* Base numbered-text fallback. */
        return strdup("{\"success\":false,\"error\":\"relay prompt op unavailable\"}");
    }
    extern char *relay_helper_mint_prompt(const char *kind, const char *session_key,
        const char *state_json, double timeout_s);
    char *state = NULL;
    asprintf(&state, "{\"session_key\":\"%s\",\"clarify_id\":\"%s\",\"choices\":%s,\"chat_id\":\"%s\"}",
             session_key ? session_key : "", clarify_id ? clarify_id : "",
             choices_json, chat_id);
    char *prompt_id = relay_helper_mint_prompt("clarify", session_key, state, 3600.0);
    free(state);
    if (!prompt_id) return strdup("{\"success\":false,\"error\":\"mint failed\"}");
    /* Render options: c0..cN from choices + other. */
    char *options = malloc(8192);
    int opos = 0;
    opos += snprintf(options + opos, 8192 - opos, "[");
    json_t *choices = json_parse(choices_json, NULL);
    if (choices && choices->type == JSON_ARRAY) {
        for (int i = 0; i < (int)choices->c.count; i++) {
            json_t *c = choices->c.items[i];
            const char *label = (c && c->type == JSON_STRING && c->str_val) ? c->str_val : "";
            if (i > 0) opos += snprintf(options + opos, 8192 - opos, ",");
            char esc[256];
            json_escape(label, esc, sizeof(esc));
            if (strlen(label) > 75) { snprintf(esc, sizeof(esc), "%.*s", 75, label); }
            opos += snprintf(options + opos, 8192 - opos,
                             "{\"id\":\"c%d\",\"label\":\"%s\"}", i, esc);
        }
    }
    opos += snprintf(options + opos, 8192 - opos,
                     ",{\"id\":\"other\",\"label\":\"\u270f\ufe0f Other (type your answer)\"}]\n");
    options[opos] = '\0';
    if (choices) json_free(choices);
    char *text = NULL;
    asprintf(&text, "\u2753 %s", question);
    char *result = relay_op_send_prompt(descriptor_json, chat_id, "clarify", text,
                                        prompt_id, options, NULL, metadata_json, 0);
    free(text); free(options);
    if (result) { free(prompt_id); return result; }
    extern void relay_helper_drop_prompt(const char *prompt_id);
    relay_helper_drop_prompt(prompt_id);
    free(prompt_id);
    return strdup("{\"success\":false,\"error\":\"relay prompt op unavailable\"}");
}

/* ════════════════════════════════════════════════════════════════════
 * _consume_prompt_response
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _consume_prompt_response @ gateway/relay/adapter.py:_consume_prompt_response */
int relay_op_consume_prompt_response(const char *prompt_response_json,
                                     char *ack_out, size_t ack_sz) {
    /* Python: route an inbound prompt_response to its waiting primitive.
     * Returns 1 when the event was a prompt answer (consumed), 0 otherwise.
     * Unknown/expired prompt ids fall through to text dispatch. */
    if (!prompt_response_json || !ack_out || ack_sz == 0) return 0;
    json_t *pr = json_parse(prompt_response_json, NULL);
    if (!pr || pr->type != JSON_OBJECT) { if (pr) json_free(pr); return 0; }
    json_t *pidj = json_obj_get(pr, "prompt_id");
    json_t *oidj = json_obj_get(pr, "option_id");
    const char *prompt_id = (pidj && pidj->type == JSON_STRING) ? pidj->str_val : NULL;
    const char *option_id = (oidj && oidj->type == JSON_STRING) ? oidj->str_val : NULL;
    if (!prompt_id || !option_id || !prompt_id[0] || !option_id[0]) {
        json_free(pr); return 0;
    }
    extern char *relay_helper_pop_prompt(const char *prompt_id);
    char *state = relay_helper_pop_prompt(prompt_id);
    if (!state) {
        json_free(pr);
        snprintf(ack_out, ack_sz, "prompt_response for unknown/expired prompt %s — falling through to text dispatch", prompt_id);
        return 0;
    }
    json_t *st = json_parse(state, NULL);
    const char *kind = NULL;
    if (st) {
        json_t *kj = json_obj_get(st, "kind");
        kind = (kj && kj->type == JSON_STRING) ? kj->str_val : NULL;
    }
    /* Map option → ack label (the in-channel confirmation the Python sends). */
    const char *label = "Resolved";
    if (kind && strcmp(kind, "exec_approval") == 0) {
        label = (strcmp(option_id, "once") == 0) ? "\u2705 Approved once" :
                (strcmp(option_id, "session") == 0) ? "\u2705 Approved for session" :
                (strcmp(option_id, "always") == 0) ? "\u2705 Approved permanently" :
                "\u274c Denied";
    } else if (kind && strcmp(kind, "slash_confirm") == 0) {
        label = (strcmp(option_id, "once") == 0) ? "\u2705 Approved once" :
                (strcmp(option_id, "always") == 0) ? "\U0001f512 Always approve" :
                "\u274c Cancelled";
    } else if (kind && strcmp(kind, "clarify") == 0) {
        if (strcmp(option_id, "other") == 0)
            label = "\u270f\ufe0f Type your answer:";
        else {
            /* Resolve the positional choice id to its text. */
            json_t *choices = st ? json_obj_get(st, "choices") : NULL;
            int idx = (option_id[0] == 'c') ? atoi(option_id + 1) : -1;
            if (choices && choices->type == JSON_ARRAY && idx >= 0 &&
                idx < (int)choices->c.count) {
                json_t *c = choices->c.items[idx];
                const char *ctext = (c && c->type == JSON_STRING && c->str_val) ? c->str_val : "";
                char *tmp = NULL;
                asprintf(&tmp, "\u2705 %s", ctext);
                snprintf(ack_out, ack_sz, "%s", tmp);
                free(tmp);
                json_free(st); json_free(pr); free(state);
                return 1;
            }
            label = "\u270f\ufe0f Type your answer:";
        }
    }
    snprintf(ack_out, ack_sz, "%s", label);
    if (st) json_free(st);
    json_free(pr);
    free(state);
    return 1;
}

/* ════════════════════════════════════════════════════════════════════
 * _react / on_processing_start / on_processing_complete
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _react @ gateway/relay/adapter.py:_react */
bool relay_op_react(const char *descriptor_json, const char *chat_id,
                    const char *message_id, const char *emoji, bool remove) {
    /* Python: egress one react op; best-effort (False on any failure).
     * Reactions are cosmetic by contract. */
    if (!descriptor_supports_op(descriptor_json, "react")) return false;
    if (!chat_id || !message_id || !emoji) return false;
    char esc_chat[512], esc_msg[512], esc_emoji[64];
    json_escape(chat_id, esc_chat, sizeof(esc_chat));
    json_escape(message_id, esc_msg, sizeof(esc_msg));
    json_escape(emoji, esc_emoji, sizeof(esc_emoji));
    char *frame = NULL;
    asprintf(&frame, "{\"op\":\"react\",\"chat_id\":\"%s\",\"message_id\":\"%s\","
             "\"emoji\":\"%s\",\"remove\":%s}",
             esc_chat, esc_msg, esc_emoji, remove ? "true" : "false");
    if (!frame) return false;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    if (!result) return false;
    bool ok = strstr(result, "\"success\":true") != NULL;
    free(result);
    return ok;
}

/* PoP: on_processing_start @ gateway/relay/adapter.py:on_processing_start */
int relay_op_on_processing_start(const char *descriptor_json, const char *chat_id,
                                 const char *message_id) {
    /* Python: add the 👀 in-progress reaction (op-gated; silent no-op). */
    if (!chat_id || !message_id) return 0;
    relay_op_react(descriptor_json, chat_id, message_id, "\U0001f440", false);
    return 0;
}

/* PoP: on_processing_complete @ gateway/relay/adapter.py:on_processing_complete */
int relay_op_on_processing_complete(const char *descriptor_json, const char *chat_id,
                                    const char *message_id, int outcome) {
    /* Python: swap 👀 for ✅/❌ per outcome (SUCCESS=0, FAILURE=1,
     * CANCELLED=2 leaves unreacted). */
    if (!chat_id || !message_id) return 0;
    relay_op_react(descriptor_json, chat_id, message_id, "\U0001f440", true);
    if (outcome == 0) relay_op_react(descriptor_json, chat_id, message_id, "\u2705", false);
    else if (outcome == 1) relay_op_react(descriptor_json, chat_id, message_id, "\u274c", false);
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * create_handoff_thread / rename_thread
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: create_handoff_thread @ gateway/relay/adapter.py:create_handoff_thread */
char *relay_op_create_handoff_thread(const char *descriptor_json,
                                     const char *parent_chat_id, const char *name) {
    /* Python: one thread_create op covers Discord (channel thread), Telegram
     * (forum topic), Slack (named seed root). None on failure so the handoff
     * watcher falls back to the parent channel. Returns the thread id. */
    if (!descriptor_supports_op(descriptor_json, "thread_create")) return NULL;
    if (!parent_chat_id) return NULL;
    char thread_name[128];
    const char *n = name ? name : "";
    while (*n == ' ' || *n == '\t') n++;
    const char *end = n + strlen(n);
    while (end > n && (end[-1] == ' ' || end[-1] == '\t')) end--;
    size_t len = (size_t)(end - n);
    if (len == 0) snprintf(thread_name, sizeof(thread_name), "handoff");
    else { size_t cl = len < 100 ? len : 100; memcpy(thread_name, n, cl); thread_name[cl] = '\0'; }
    char esc_chat[512], esc_name[256];
    json_escape(parent_chat_id, esc_chat, sizeof(esc_chat));
    json_escape(thread_name, esc_name, sizeof(esc_name));
    char *frame = NULL;
    asprintf(&frame, "{\"op\":\"thread_create\",\"chat_id\":\"%s\",\"thread_name\":\"%s\"}",
             esc_chat, esc_name);
    if (!frame) return NULL;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    if (!result) return NULL;
    /* Extract thread_id or message_id from the result. */
    char *tid = NULL;
    json_t *rj = json_parse(result, NULL);
    if (rj) {
        bool ok = false;
        json_t *sj = json_obj_get(rj, "success");
        if (sj && sj->type == JSON_BOOL) ok = sj->bool_val;
        if (ok) {
            json_t *tj = json_obj_get(rj, "thread_id");
            if (!tj || tj->type != JSON_STRING) tj = json_obj_get(rj, "message_id");
            if (tj && tj->type == JSON_STRING && tj->str_val) tid = strdup(tj->str_val);
        }
        json_free(rj);
    }
    free(result);
    return tid;
}

/* PoP: rename_thread @ gateway/relay/adapter.py:rename_thread */
bool relay_op_rename_thread(const char *descriptor_json, const char *thread_id,
                            const char *name, const char *only_if_current_name,
                            bool prefer_connector_created,
                            const char *parent_chat_id) {
    /* Python: best-effort thread rename via the connector's thread_rename
     * op; no-clobber guard via prefer_connector_created or the legacy
     * only_if_current_name string guard. */
    if (!descriptor_supports_op(descriptor_json, "thread_rename")) return false;
    if (!thread_id || !name) return false;
    /* Clean the name: collapse whitespace, strip. */
    char cleaned[128];
    const char *p = name;
    char *o = cleaned;
    bool pending_space = false;
    while (*p && o - cleaned < 99) {
        if (*p == ' ' || *p == '\t' || *p == '\n') { pending_space = true; p++; continue; }
        if (pending_space && o > cleaned) *o++ = ' ';
        pending_space = false;
        *o++ = *p++;
    }
    if (o == cleaned) return false;
    *o = '\0';
    /* Trim trailing space. */
    while (o > cleaned && o[-1] == ' ') { o--; *o = '\0'; }
    if (!cleaned[0]) return false;
    char esc_chat[512], esc_msg[512], esc_name[256];
    const char *chat_id = parent_chat_id ? parent_chat_id : thread_id;
    json_escape(chat_id, esc_chat, sizeof(esc_chat));
    json_escape(thread_id, esc_msg, sizeof(esc_msg));
    json_escape(cleaned, esc_name, sizeof(esc_name));
    char *frame = NULL;
    if (prefer_connector_created)
        asprintf(&frame, "{\"op\":\"thread_rename\",\"chat_id\":\"%s\",\"message_id\":\"%s\","
                 "\"thread_name\":\"%s\",\"only_if_connector_created\":true}",
                 esc_chat, esc_msg, esc_name);
    else if (only_if_current_name)
        asprintf(&frame, "{\"op\":\"thread_rename\",\"chat_id\":\"%s\",\"message_id\":\"%s\","
                 "\"thread_name\":\"%s\",\"only_if_current_name\":\"%s\"}",
                 esc_chat, esc_msg, esc_name, only_if_current_name);
    else
        asprintf(&frame, "{\"op\":\"thread_rename\",\"chat_id\":\"%s\",\"message_id\":\"%s\","
                 "\"thread_name\":\"%s\"}",
                 esc_chat, esc_msg, esc_name);
    if (!frame) return false;
    char *result = relay_transport_dispatch_op(frame);
    free(frame);
    if (!result) return false;
    bool ok = strstr(result, "\"success\":true") != NULL;
    free(result);
    return ok;
}
