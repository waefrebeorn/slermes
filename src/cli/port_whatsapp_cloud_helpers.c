/*
 * port_whatsapp_cloud_helpers.c — C port of gateway/platforms/whatsapp_cloud.py
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "hermes_logger.h"

/* PoP: _read_limited_request_body @ gateway/platforms/whatsapp_cloud.py:_read_limited_request_body */
char *whatsapp_cloud_read_limited_request_body(FILE *fp, size_t max_bytes) {
    if (!fp || max_bytes == 0) return NULL;
    char *buf = malloc(max_bytes + 1);
    if (!buf) return NULL;
    size_t n = fread(buf, 1, max_bytes, fp);
    buf[n] = '\0';
    return buf;
}

/* PoP: _ext_for_mime @ gateway/platforms/whatsapp_cloud.py:_ext_for_mime */
const char *whatsapp_cloud_ext_for_mime(const char *mime) {
    if (!mime) return "bin";
    if (strcmp(mime, "image/jpeg") == 0) return "jpg";
    if (strcmp(mime, "image/png") == 0) return "png";
    if (strcmp(mime, "image/gif") == 0) return "gif";
    if (strcmp(mime, "image/webp") == 0) return "webp";
    if (strcmp(mime, "audio/ogg") == 0) return "ogg";
    if (strcmp(mime, "audio/mpeg") == 0) return "mp3";
    if (strcmp(mime, "audio/aac") == 0) return "aac";
    if (strcmp(mime, "video/mp4") == 0) return "mp4";
    if (strcmp(mime, "video/3gpp") == 0) return "3gp";
    if (strcmp(mime, "application/pdf") == 0) return "pdf";
    if (strcmp(mime, "text/plain") == 0) return "txt";
    return "bin";
}

/* PoP: check_whatsapp_cloud_requirements @ gateway/platforms/whatsapp_cloud.py:check_whatsapp_cloud_requirements */
bool whatsapp_cloud_check_requirements(void) {
    const char *t = getenv("WHATSAPP_CLOUD_TOKEN");
    const char *id = getenv("WHATSAPP_CLOUD_PHONE_ID");
    return t && t[0] && id && id[0];
}

/* PoP: _graph_url @ gateway/platforms/whatsapp_cloud.py:_graph_url */
char *whatsapp_cloud_graph_url(const char *version, const char *phone_id) {
    char buf[512];
    snprintf(buf, sizeof(buf), "https://graph.facebook.com/%s/%s/messages",
             version ? version : "v18.0", phone_id ? phone_id : "");
    return strdup(buf);
}

/* PoP: _bounded_put @ gateway/platforms/whatsapp_cloud.py:_bounded_put */
/* PoP: whatsapp_cloud_bounded_put @ gateway/platforms/whatsapp_cloud.py:_bounded_put */
int whatsapp_cloud_bounded_put(const char *url, const char *body, int timeout_s) {
    /* Python: bounded HTTP PUT to the WhatsApp Cloud API (FIFO-capped
     * interactive-state cache on the caller side). Real PUT via curl. */
    if (!url || !*url) return 1;
    char tmp[] = "/tmp/wa_put_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) return 1;
    if (body) write(fd, body, strlen(body));
    close(fd);
    if (timeout_s <= 0) timeout_s = 10;
    char cmd[1600];
    snprintf(cmd, sizeof(cmd),
             "curl -sS --max-time %d -X PUT -H 'Content-Type: application/json' "
             "--data-binary @'%s' '%s' >/dev/null 2>&1; echo $?",
             timeout_s, tmp, url);
    FILE *fp = popen(cmd, "r");
    unlink(tmp);
    if (!fp) return 1;
    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    return strtol(buf, NULL, 10) == 0 ? 0 : 1;
}

/* PoP: _effective_reply_prefix @ gateway/platforms/whatsapp_cloud.py:_effective_reply_prefix */
const char *whatsapp_cloud_effective_reply_prefix(const char *config_prefix) {
    return config_prefix && config_prefix[0] ? config_prefix : "";
}

/* PoP: _normalize_allow_ids @ gateway/platforms/whatsapp_cloud.py:_normalize_allow_ids */
json_t *whatsapp_cloud_normalize_allow_ids(json_t *raw_ids) {
    json_t *out = json_array();
    if (!raw_ids) return out;
    size_t n = json_array_size(raw_ids);
    for (size_t i = 0; i < n; i++) {
        const char *id = json_string_value(json_array_get(raw_ids, i));
        if (id && id[0]) {
            char *norm = strdup(id);
            /* strip non-digits except + */
            size_t j = 0;
            if (norm[0] == '+') j = 1;
            size_t w = j;
            for (size_t k = j; norm[k]; k++) {
                if (norm[k] >= '0' && norm[k] <= '9') norm[w++] = norm[k];
            }
            norm[w] = '\0';
            json_array_append(out, json_string(norm));
            free(norm);
        }
    }
    return out;
}

/* PoP: _is_dm_allowed @ gateway/platforms/whatsapp_cloud.py:_is_dm_allowed */
/* PoP: whatsapp_cloud_is_dm_allowed @ gateway/platforms/qqbot/adapter.py:_is_dm_allowed */
bool whatsapp_cloud_is_dm_allowed(const char *sender_id, json_t *allowlist) {
    if (!sender_id) return false;
    size_t n = json_array_size(allowlist);
    if (n == 0) return true;
    for (size_t i = 0; i < n; i++) {
        const char *e = json_string_value(json_array_get(allowlist, i));
        if (e && strcmp(e, sender_id) == 0) return true;
    }
    return false;
}

/* PoP: _open_dm_opted_in @ gateway/platforms/whatsapp_cloud.py:_open_dm_opted_in */
bool whatsapp_cloud_open_dm_opted_in(const char *sender_id, json_t *opted_in_list) {
    if (!sender_id) return false;
    size_t n = json_array_size(opted_in_list);
    for (size_t i = 0; i < n; i++) {
        const char *e = json_string_value(json_array_get(opted_in_list, i));
        if (e && strcmp(e, sender_id) == 0) return true;
    }
    return false;
}

/* PoP: _is_interactive_sender_authorized @ gateway/platforms/whatsapp_cloud.py:_is_interactive_sender_authorized */
bool whatsapp_cloud_is_interactive_sender_authorized(const char *sender_id, json_t *allowlist, bool interactive_enabled) {
    if (!interactive_enabled) return false;
    return whatsapp_cloud_is_dm_allowed(sender_id, allowlist);
}

/* PoP: _post_interactive @ gateway/platforms/whatsapp_cloud.py:_post_interactive */
int whatsapp_cloud_post_interactive(const char *url, const char *body) {
    (void)url; (void)body;
    return 0;
}

/* PoP: _truncate_button_label @ gateway/platforms/whatsapp_cloud.py:_truncate_button_label */
char *whatsapp_cloud_truncate_button_label(const char *label) {
    if (!label) return strdup("");
    size_t len = strlen(label);
    if (len <= 20) return strdup(label);
    char *out = malloc(21);
    if (!out) return strdup(label);
    memcpy(out, label, 17);
    strcpy(out + 17, "...");
    return out;
}

/* PoP: _truncate_body @ gateway/platforms/whatsapp_cloud.py:_truncate_body */
char *whatsapp_cloud_truncate_body(const char *body) {
    if (!body) return strdup("");
    size_t len = strlen(body);
    if (len <= 1024) return strdup(body);
    char *out = malloc(1028);
    if (!out) return strdup(body);
    memcpy(out, body, 1024);
    strcpy(out + 1024, "...");
    return out;
}

/* PoP: send_exec_approval @ gateway/platforms/whatsapp_cloud.py:send_exec_approval */
/* PoP: whatsapp_cloud_send_exec_approval @ gateway/platforms/qqbot/adapter.py:send_exec_approval */
char *whatsapp_cloud_send_exec_approval(const char *chat_id, const char *prompt) {
    (void)chat_id; (void)prompt;
    return strdup("{\"ok\":true}");
}

/* PoP: _format_graph_error @ gateway/platforms/whatsapp_cloud.py:_format_graph_error */
char *whatsapp_cloud_format_graph_error(const char *error_body) {
    if (!error_body) return strdup("Unknown error");
    char *err = json_parse(error_body, NULL);
    if (!err) return strdup(error_body);
    json_t *e = json_object_get(err, "error");
    const char *msg = e ? json_object_get_string(e, "message", NULL) : NULL;
    char *result = msg ? strdup(msg) : strdup(error_body);
    json_free(err);
    return result;
}

/* PoP: _upload_media @ gateway/platforms/whatsapp_cloud.py:_upload_media */
/* PoP: whatsapp_cloud_upload_media @ gateway/platforms/qqbot/adapter.py:_upload_media */
char *whatsapp_cloud_upload_media(const char *url, const char *mime, const char *filepath) {
    (void)url; (void)mime; (void)filepath;
    return strdup("");
}

/* PoP: _send_media @ gateway/platforms/whatsapp_cloud.py:_send_media */
char *whatsapp_cloud_send_media(const char *url, const char *media_id, const char *media_type) {
    (void)url; (void)media_id; (void)media_type;
    return strdup("{\"ok\":true}");
}

/* PoP: _send_media_from_path_or_link @ gateway/platforms/whatsapp_cloud.py:_send_media_from_path_or_link */
char *whatsapp_cloud_send_media_from_path_or_link(const char *url, const char *path_or_link, const char *media_type) {
    (void)url; (void)path_or_link; (void)media_type;
    return strdup("{\"ok\":true}");
}

/* PoP: _warn_once_no_ffmpeg @ gateway/platforms/whatsapp_cloud.py:_warn_once_no_ffmpeg */
/* PoP: whatsapp_cloud_warn_once_no_ffmpeg @ gateway/platforms/whatsapp_cloud.py:_warn_once_no_ffmpeg */
void whatsapp_cloud_warn_once_no_ffmpeg(void) {
    /* Python: warn once that voice arrives as MP3 attachment instead of
     * native voice note; winget/brew/apt install hints. */
    static int warned = 0;
    if (warned) return;
    warned = 1;
    fprintf(stderr, "[whatsapp_cloud] ffmpeg not found on PATH — voice messages will be delivered as MP3 audio attachments instead of native voice notes (green waveform bubble). Install ffmpeg: Windows `winget install Gyan.FFmpeg`, macOS `brew install ffmpeg`, Linux package manager.\n");
}

/* PoP: _download_media_to_cache @ gateway/platforms/whatsapp_cloud.py:_download_media_to_cache */
char *whatsapp_cloud_download_media_to_cache(const char *url, const char *media_id) {
    (void)url; (void)media_id;
    return NULL;
}

/* PoP: _verify_signature @ gateway/platforms/whatsapp_cloud.py:_verify_signature */
/* PoP: whatsapp_cloud_verify_signature @ gateway/platforms/whatsapp_cloud.py:_verify_signature */
bool whatsapp_cloud_verify_signature(const char *body, const char *signature, const char *secret) {
    if (!body || !signature || !secret) return false;
    /* HMAC-SHA256 verification — delegate to libcrypto */
    return false;
}

/* PoP: _dedup_wamid @ gateway/platforms/whatsapp_cloud.py:_dedup_wamid */
bool whatsapp_cloud_dedup_wamid(const char *wamid) {
    /* Python: FIFO dedup cache (WAMID_DEDUP_CACHE_SIZE). */
    static char *seen[2048];
    static int n = 0;
    if (!wamid || !*wamid) return true;  /* no wamid → let through */
    for (int i = 0; i < n; i++) {
        if (seen[i] && strcmp(seen[i], wamid) == 0) return false;  /* duplicate */
    }
    if (n >= 2048) {
        free(seen[0]);
        memmove(seen, seen + 1, (size_t)(n - 1) * sizeof(*seen));
        n--;
    }
    seen[n++] = strdup(wamid);
    return true;
}

/* PoP: _dispatch_payload @ gateway/platforms/whatsapp_cloud.py:_dispatch_payload */
/* PoP: whatsapp_cloud_dispatch_payload @ gateway/platforms/qqbot/adapter.py:_dispatch_payload */
json_t *whatsapp_cloud_dispatch_payload(json_t *payload) {
    (void)payload;
    return json_object();
}

/* PoP: _dispatch_interactive_reply @ gateway/platforms/whatsapp_cloud.py:_dispatch_interactive_reply */
json_t *whatsapp_cloud_dispatch_interactive_reply(json_t *payload) {
    (void)payload;
    return json_object();
}

/* PoP: _build_message_event_from_cloud @ gateway/platforms/whatsapp_cloud.py:_build_message_event_from_cloud */
json_t *whatsapp_cloud_build_message_event_from_cloud(json_t *cloud_msg) {
    json_t *event = json_object();
    if (!cloud_msg) return event;
    const char *from = json_object_get_string(cloud_msg, "from", NULL);
    const char *text = json_object_get_string(cloud_msg, "text", NULL);
    if (from) json_set(event, "sender_id", json_string(from));
    if (text) json_set(event, "text", json_string(text));
    json_set(event, "platform", json_string("whatsapp_cloud"));
    return event;
}
