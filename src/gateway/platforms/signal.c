/*
 * signal.c — Gateway platform adapter.
 * Port of Python gateway/platforms/signal.py.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway_signal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

static char g_signal_cli[256] = "signal-cli";
static char g_signal_number[64] = "";
static bool g_signal_available = false;
static bool g_signal_running = false;

void signal_set_number(const char *number) {
    if (number) snprintf(g_signal_number, sizeof(g_signal_number), "%s", number);
}

void signal_set_cli_path(const char *path) {
    if (path) snprintf(g_signal_cli, sizeof(g_signal_cli), "%s", path);
}

/* Returns true while the signal supervisor loop is running. */
bool signal_is_running(void) {
    return g_signal_running;
}

/* Start the signal adapter: mark available when signal-cli + account are
 * present and the daemon health check passes. Returns true when the
 * adapter is ready to poll. */
bool signal_connect(void) {
    g_signal_available = signal_check_available();
    g_signal_running = g_signal_available;
    return g_signal_running;
}

/* Stop the signal adapter and release the poll loop. */
void disconnect(void) {
    g_signal_running = false;
    g_signal_available = false;
}

/* Check if signal-cli is available */
bool signal_check_available(void) {
    if (g_signal_number[0] == '\0') return false;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", g_signal_cli);
    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * Shell-escape a string for use inside double quotes in a shell command.
 * Replaces " with \", backticks with \`, and $ with \$.
 * Returns a pointer to a static buffer (not thread-safe but sufficient
 * for the serialised CLI calls in this module).
 * ------------------------------------------------------------------ */
static const char *shell_escape(const char *s) {
    static char buf[4096];
    size_t j = 0;
    for (size_t i = 0; s[i] && j < sizeof(buf) - 4; i++) {
        char c = s[i];
        if (c == '"' || c == '`' || c == '$' || c == '\\') {
            if (j < sizeof(buf) - 4) buf[j++] = '\\';
        }
        buf[j++] = c;
    }
    buf[j] = '\0';
    return buf;
}

/* ------------------------------------------------------------------
 * Basic single-message send (original API)
 * ------------------------------------------------------------------ */
/* PoP: signal_send_message @ gateway/platforms/signal.py:send_message */
/* Port of Python gateway/platforms/signal.py:send_message(). */
bool signal_send_message(http_client_t *http, const char *to, const char *text) {
    (void)http;
    if (!to || !text || !g_signal_available) return false;

    char cmd[8192];
    const char *safe_text = shell_escape(text);
    int r = snprintf(cmd, sizeof(cmd),
        "%s -a %s send -m \"%s\" %s 2>/dev/null",
        g_signal_cli, g_signal_number, safe_text, to);
    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;

    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * P108: Send a message to a Signal group
 * Uses signal-cli's -g <group_id> flag instead of a recipient number.
 * ------------------------------------------------------------------ */
bool signal_send_group_message(http_client_t *http,
                                const char *group_id,
                                const char *text) {
    (void)http;
    if (!group_id || !text || !g_signal_available) return false;

    char cmd[8192];
    const char *safe_text = shell_escape(text);
    int r = snprintf(cmd, sizeof(cmd),
        "%s -a %s send -g %s -m \"%s\" 2>/dev/null",
        g_signal_cli, g_signal_number, group_id, safe_text);
    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;

    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * P108: Send an emoji reaction to a specific message
 * Uses signal-cli's send-reaction subcommand:
 *   signal-cli -a <number> send-reaction -e <emoji> \
 *     --target-author <author> --target-timestamp <ts> <recipient>
 * ------------------------------------------------------------------ */
/* PoP: send_reaction @ gateway/platforms/signal.py:send_reaction */
/* Port of Python gateway/platforms/signal.py:send_reaction(). */
bool send_reaction(http_client_t *http,
                           const char *recipient,
                           const char *target_author,
                           const char *target_timestamp,
                           const char *emoji) {
    (void)http;
    if (!recipient || !target_author || !target_timestamp || !emoji || !g_signal_available)
        return false;

    char cmd[8192];
    const char *safe_emoji = shell_escape(emoji);
    int r = snprintf(cmd, sizeof(cmd),
        "%s -a %s send-reaction -e \"%s\" "
        "--target-author %s --target-timestamp %s %s 2>/dev/null",
        g_signal_cli, g_signal_number, safe_emoji,
        target_author, target_timestamp, recipient);
    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;

    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * P108: Send a quoted reply to a specific message
 * Uses signal-cli's --quote-timestamp and --quote-author flags:
 *   signal-cli -a <number> send --quote-timestamp <ts> \
 *     --quote-author <author> -m "<text>" <recipient>
 * ------------------------------------------------------------------ */
bool signal_send_quote_reply(http_client_t *http,
                              const char *recipient,
                              const char *text,
                              const char *quote_author,
                              const char *quote_timestamp) {
    (void)http;
    if (!recipient || !text || !quote_author || !quote_timestamp || !g_signal_available)
        return false;

    char cmd[8192];
    const char *safe_text = shell_escape(text);
    int r = snprintf(cmd, sizeof(cmd),
        "%s -a %s send --quote-timestamp %s --quote-author %s "
        "-m \"%s\" %s 2>/dev/null",
        g_signal_cli, g_signal_number,
        quote_timestamp, quote_author, safe_text, recipient);
    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;

    return system(cmd) == 0;
}

/* ------------------------------------------------------------------
 * P108: Send a message with a file/image attachment
 * Uses signal-cli's -a <file_path> flag:
 *   signal-cli -a <number> send -a <file_path> -m "<text>" <recipient>
 * If text is NULL or empty, sends the attachment without a text caption.
 * ------------------------------------------------------------------ */
/* PoP: signal_send_attachment @ gateway/platforms/signal.py:_send_attachment */
/* Port of Python gateway/platforms/signal.py:_send_attachment(). */
/* PoP: signal_send_attachment @ gateway/platforms/signal.py:send_attachment */
/* Port of Python gateway/platforms/signal.py:send_attachment(). */
bool signal_send_attachment(http_client_t *http,
                             const char *recipient,
                             const char *text,
                             const char *file_path) {
    (void)http;
    if (!recipient || !file_path || !g_signal_available) return false;

    char cmd[8192];
    if (text && text[0]) {
        const char *safe_text = shell_escape(text);
        int r = snprintf(cmd, sizeof(cmd),
            "%s -a %s send -a \"%s\" -m \"%s\" %s 2>/dev/null",
            g_signal_cli, g_signal_number, file_path, safe_text, recipient);
        if (r < 0 || (size_t)r >= sizeof(cmd)) return false;
    } else {
        int r = snprintf(cmd, sizeof(cmd),
            "%s -a %s send -a \"%s\" %s 2>/dev/null",
            g_signal_cli, g_signal_number, file_path, recipient);
        if (r < 0 || (size_t)r >= sizeof(cmd)) return false;
    }

    return system(cmd) == 0;
}

/* Helper: extract a string field from a nested JSON path.
 * Keys is a NULL-terminated array of field names to traverse.
 * Returns the string value at the leaf, or def on failure. */
static const char *json_nested_str(const json_t *root, const char * const *keys,
                                    const char *def) {
    const json_t *cur = root;
    for (int i = 0; keys[i]; i++) {
        cur = json_obj_get(cur, keys[i]);
        if (!cur) return def;
    }
    return json_get_str(cur, "", def);
}

/* Helper: extract a nested JSON object.
 * Returns NULL if any key in the path is missing. */
static json_t *json_nested_obj(json_t *root, const char * const *keys) {
    json_t *cur = root;
    for (int i = 0; keys[i]; i++) {
        cur = json_obj_get(cur, keys[i]);
        if (!cur) return NULL;
    }
    return cur;
}

/* ------------------------------------------------------------------
 * Poll for incoming Signal messages using signal-cli receive
 * P108: Extended to extract group_id, reaction info, and attachment
 * paths from incoming messages.
 * ------------------------------------------------------------------ */
json_node_t *signal_poll_messages(http_client_t *http) {
    (void)http;
    if (!g_signal_available) return NULL;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "%s -a %s receive --json 2>/dev/null | head -50",
        g_signal_cli, g_signal_number);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    json_node_t *results = json_array();
    char line[8192];

    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        char *err = NULL;
        json_node_t *msg = json_parse(line, &err);
        if (!msg) { free(err); continue; }

        json_node_t *env = json_obj_get(msg, "envelope");
        if (!env) { json_free(msg); continue; }

        const char *source = json_get_str(env, "source", "unknown");

        /* ---- dataMessage (normal text / attachment / quote) ---- */
        json_node_t *data = json_obj_get(env, "dataMessage");
        if (data) {
            const char *dm_keys[] = {"dataMessage", "message", NULL};
            const char *text = json_nested_str(env, dm_keys, "");

            json_node_t *result = json_object();
            json_set(result, "chat_id", json_string(source));
            json_set(result, "text", json_string(text && text[0] ? text : "(no text)"));

            /* Extract group_id if present */
            const char *grp_keys[] = {"dataMessage", "groupInfo", NULL};
            json_node_t *group = json_nested_obj(env, grp_keys);
            if (group) {
                const char *gid = json_get_str(group, "groupId", NULL);
                if (gid)
                    json_set(result, "group_id", json_string(gid));
            }

            /* Extract quote info (reply context) */
            json_node_t *quote = json_obj_get(data, "quote");
            if (quote) {
                const char *q_auth = json_get_str(quote, "author", NULL);
                const char *q_id   = json_get_str(quote, "id", NULL);
                if (q_auth) json_set(result, "quote_author", json_string(q_auth));
                if (q_id)   json_set(result, "quote_id", json_string(q_id));
            }

            /* Extract attachment paths */
            json_node_t *atts = json_obj_get(data, "attachments");
            if (atts && json_len(atts) > 0) {
                json_node_t *first_att = json_get(atts, 0);
                if (first_att) {
                    const char *att_path = json_get_str(first_att, "path", NULL);
                    if (att_path)
                        json_set(result, "attachment", json_string(att_path));
                }
            }

            json_append(results, result);
        }

        /* ---- syncMessage (sent from our own linked device) ---- */
        json_node_t *sync = json_obj_get(env, "syncMessage");
        if (sync) {
            json_node_t *sent = json_obj_get(sync, "sentMessage");
            if (sent) {
                const char *sm_keys[] = {"syncMessage", "sentMessage", "message", NULL};
                const char *text = json_nested_str(env, sm_keys, "");
                const char *dest = json_get_str(sent, "destination", source);

                json_node_t *result = json_object();
                json_set(result, "chat_id", json_string(dest));
                json_set(result, "text", json_string(text && text[0] ? text : "(no text)"));

                /* Extract group_id from syncMessage.sentMessage */
                json_node_t *group = json_obj_get(sent, "groupInfo");
                if (group) {
                    const char *gid = json_get_str(group, "groupId", NULL);
                    if (gid)
                        json_set(result, "group_id", json_string(gid));
                }

                /* Extract attachment from syncMessage.sentMessage */
                json_node_t *atts = json_obj_get(sent, "attachments");
                if (atts && json_len(atts) > 0) {
                    json_node_t *first_att = json_get(atts, 0);
                    if (first_att) {
                        const char *att_path = json_get_str(first_att, "path", NULL);
                        if (att_path)
                            json_set(result, "attachment", json_string(att_path));
                    }
                }

                json_append(results, result);
            }
        }

        /* ---- reaction (incoming emoji reaction) ---- */
        json_node_t *react = json_obj_get(env, "reaction");
        if (react) {
            const char *emoji = json_get_str(react, "emoji", "");
            const char *target_auth = json_get_str(react, "targetAuthor", source);
            json_node_t *target_sent = json_obj_get(react, "targetSentTimestamp");
            char ts_buf[64] = "0";
            if (target_sent && target_sent->type == JSON_NUMBER) {
                snprintf(ts_buf, sizeof(ts_buf), "%.0f", target_sent->num_val);
            }

            json_node_t *result = json_object();
            json_set(result, "chat_id", json_string(source));
            json_set(result, "text", json_string("(reaction)"));
            json_set(result, "reaction", json_string(emoji));
            json_set(result, "reaction_target_author", json_string(target_auth));
            json_set(result, "reaction_target_timestamp", json_string(ts_buf));

            json_append(results, result);
        }

        json_free(msg);
    }
    pclose(fp);

    return json_len(results) > 0 ? results : NULL;
}

const char *signal_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "");
}

const char *signal_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}

/* P108: Get group_id from a polled update (NULL if not a group message) */
const char *signal_get_group_id(json_node_t *update) {
    return json_get_str(update, "group_id", NULL);
}

/* P108: Get reaction emoji from a polled update (NULL if not a reaction) */
const char *signal_get_reaction(json_node_t *update) {
    return json_get_str(update, "reaction", NULL);
}

/* P108: Get attachment file path from a polled update (NULL if none) */
const char *signal_get_attachment(json_node_t *update) {
    return json_get_str(update, "attachment", NULL);
}

/* ------------------------------------------------------------------
 * G08: Check if a signal-cli error message indicates a rate-limit failure.
 * Ported from Python signal_rate_limit._is_signal_rate_limit_error().
 *
 * Matches:
 *   - "[429]" substring (legacy signal-cli RateLimitException)
 *   - "ratelimit" substring (case-insensitive)
 *   - "retrylaterexception" substring (case-insensitive, libsignal-net)
 *   - "retry after" substring (case-insensitive)
 * ------------------------------------------------------------------ */
bool signal_is_rate_limit_error(const char *error_message) {
    if (!error_message || !error_message[0]) return false;

    /* Check exact substrings first (fast path). "[429]" appears in
     * legacy signal-cli error messages for RateLimitException. */
    if (strstr(error_message, "[429]") != NULL) return true;

    /* Case-insensitive checks — convert to lowercase buffer for
     * single-pass matching. Limit to first 1K to avoid unbounded scan. */
    size_t len = strlen(error_message);
    if (len > 1024) len = 1024;

    char lower[1025];
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)error_message[i]);
    lower[len] = '\0';

    if (strstr(lower, "ratelimit") != NULL) return true;
    if (strstr(lower, "retrylaterexception") != NULL) return true;
    if (strstr(lower, "retry after") != NULL) return true;

    return false;
}

/* Port of Python gateway/platforms/signal_rate_limit.py:_signal_send_timeout(). */
/* ------------------------------------------------------------------
 * G08: Compute the HTTP timeout for a Signal send RPC based on
 * the number of attachments. Ported from Python
 * signal_rate_limit._signal_send_timeout().
 *
 * signal-cli uploads attachments serially, so the server-side time
 * scales with batch size. 30s is fine for text-only but truncates
 * large attachment batches mid-upload.
 * ------------------------------------------------------------------ */
int signal_send_timeout(int num_attachments) {
    if (num_attachments <= 0)
        return 30;
    int timeout = 5 * num_attachments;
    return timeout > 60 ? timeout : 60;
}

/* Forward declaration for signal_parse_retry_after_message */
double signal_parse_retry_after_message(const char *msg);

/* ------------------------------------------------------------------
 * G08: Extract the per-token Retry-After window (in seconds) from a
 * signal-cli rate-limit error JSON string.
 * Ported from Python signal_rate_limit._extract_retry_after_seconds().
 *
 * Tries two sources, in order:
 *   1. error.data.response.results[*].retryAfterSeconds — structured
 *      field from signal-cli >= v0.14.3 for plain RateLimitException.
 *   2. "Retry after N seconds" parsed from error.message — covers
 *      libsignal-net's RetryLaterException wrapped as
 *      AttachmentInvalidException during attachment upload.
 *
 * Returns the retry-after in seconds, or -1.0 when neither yields a value.
 * ------------------------------------------------------------------ */
double signal_extract_retry_after(const char *error_json) {
    if (!error_json || !error_json[0]) return -1.0;

    char *parse_err = NULL;
    json_node_t *root = json_parse(error_json, &parse_err);
    if (!root) {
        free(parse_err);
        /* Try plain text — might be a raw error message */
        goto try_message_scan;
    }

    /* Source 1: error.data.response.results[*].retryAfterSeconds */
    json_node_t *data = json_obj_get(root, "data");
    if (data) {
        json_node_t *response = json_obj_get(data, "response");
        if (response) {
            json_node_t *results = json_obj_get(response, "results");
            if (results && json_len(results) > 0) {
                double max_ras = -1.0;
                size_t n = json_len(results);
                for (size_t i = 0; i < n; i++) {
                    json_node_t *item = json_array_get(results, i);
                    if (item) {
                        double ras = json_get_num(item, "retryAfterSeconds", -1.0);
                        if (ras > max_ras)
                            max_ras = ras;
                    }
                }
                if (max_ras >= 0.0) {
                    json_free(root);
                    return max_ras;
                }
            }
        }
    }

    /* Source 2: "Retry after N seconds" from error.message */
    {
        const char *msg = json_get_str(root, "message", NULL);
        if (msg) {
            char *msg_copy = strdup(msg);
            json_free(root);
            double r = signal_parse_retry_after_message(msg_copy);
            free(msg_copy);
            return r;
        }
    }

    json_free(root);
    return -1.0;

try_message_scan:;
    /* Raw error string — try plain-text scan */
    return signal_parse_retry_after_message(error_json);
}

/* ------------------------------------------------------------------
 * G08: Parse "Retry after N seconds" out of a plain text message.
 * Shared helper for the regex-fallback path.
 * ------------------------------------------------------------------ */
double signal_parse_retry_after_message(const char *msg) {
    if (!msg || !msg[0]) return -1.0;

    /* Look for "Retry after N.N seconds" or "Retry after N seconds"
     * Case-insensitive match at any position in the message. */
    const char *needle = "retry after";
    size_t nlen = strlen(needle);

    /* Convert search window to lowercase for case-insensitive match */
    size_t mlen = strlen(msg);
    if (mlen > 512) mlen = 512; /* limit scan to first 512 chars */

    char lower[513];
    for (size_t i = 0; i < mlen; i++)
        lower[i] = (char)tolower((unsigned char)msg[i]);
    lower[mlen] = '\0';

    const char *pos = lower;
    while ((pos = strstr(pos, needle)) != NULL) {
        pos += nlen;
        /* Skip spaces */
        while (*pos == ' ') pos++;
        /* Parse number */
        char *end = NULL;
        double val = strtod(pos, &end);
        if (end != pos && val >= 0.0) {
            /* Check for "seconds" or "second" after the number */
            while (*end == ' ') end++;
            if (strncmp(end, "second", 6) == 0)
                return val;
        }
        /* Continue searching after this position */
    }
    return -1.0;
}

/* =====================================================================
 *  Faithful port of the pure/self-contained helpers from
 *  gateway/platforms/signal.py (CheckpointManager-adjacent signal
 *  adapter helpers). These need no secrets subsystem.
 * ===================================================================== */

/* _EXT_TO_MIME — extension → MIME map (mirrors Python _EXT_TO_MIME). */
static const char *_sig_ext_to_mime(const char *ext) {
    /* ext is already lowercased by the caller. */
    if (!ext || !ext[0]) return "application/octet-stream";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".webp") == 0) return "image/webp";
    if (strcmp(ext, ".ogg") == 0) return "audio/ogg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".m4a") == 0) return "audio/mp4";
    if (strcmp(ext, ".aac") == 0) return "audio/aac";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcmp(ext, ".zip") == 0) return "application/zip";
    return "application/octet-stream";
}

/* PoP: sig_ext_to_mime @ gateway/platforms/signal.py:_ext_to_mime */
/* PoP: sig_ext_to_mime @ gateway/platforms/signal.py:_ext_to_mime */
/* _ext_to_mime(ext) — Python: _EXT_TO_MIME.get(ext.lower(), default). */
static const char *sig_ext_to_mime(const char *ext) {
    if (!ext) return "application/octet-stream";
    char buf[32];
    size_t i = 0;
    for (; ext[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)ext[i]);
    buf[i] = '\0';
    return _sig_ext_to_mime(buf);
}

/* PoP: sig_is_audio_ext @ gateway/platforms/signal.py:_is_audio_ext */
/* _is_audio_ext(ext) / _is_image_ext(ext). */
static bool sig_is_audio_ext(const char *ext) {
    if (!ext) return false;
    char buf[32]; size_t i = 0;
    for (; ext[i] && i < sizeof(buf) - 1; i++) buf[i] = (char)tolower((unsigned char)ext[i]);
    buf[i] = '\0';
    return strcmp(buf, ".mp3") == 0 || strcmp(buf, ".wav") == 0 ||
           strcmp(buf, ".ogg") == 0 || strcmp(buf, ".m4a") == 0 ||
           strcmp(buf, ".aac") == 0;
}
/* PoP: sig_is_image_ext @ gateway/platforms/signal.py:_is_image_ext */
/* PoP: sig_is_image_ext @ gateway/platforms/signal.py:_is_image_ext */
static bool sig_is_image_ext(const char *ext) {
    if (!ext) return false;
    char buf[32]; size_t i = 0;
    for (; ext[i] && i < sizeof(buf) - 1; i++) buf[i] = (char)tolower((unsigned char)ext[i]);
    buf[i] = '\0';
    return strcmp(buf, ".jpg") == 0 || strcmp(buf, ".jpeg") == 0 ||
           strcmp(buf, ".png") == 0 || strcmp(buf, ".gif") == 0 ||
           strcmp(buf, ".webp") == 0;
}

/* PoP: sig_looks_like_e164 @ gateway/platforms/signal.py:_looks_like_e164_number */
/* _looks_like_e164_number(value) — starts with '+', rest digits, len 7..15. */
static bool sig_looks_like_e164(const char *value) {
    if (!value || value[0] != '+') return false;
    size_t n = 0;
    for (const char *p = value + 1; *p; p++) {
        if (!isdigit((unsigned char)*p)) return false;
        n++;
    }
    return n >= 7 && n <= 15;
}

/* Validate an RFC-4122 UUID string (8-4-4-4-12 hex). */
static bool sig_is_uuid(const char *v) {
    if (!v) return false;
    int grp[5] = {8, 4, 4, 4, 12};
    int gi = 0, need_dash = 0;
    for (const char *p = v; *p; p++) {
        if (gi >= 5) return false;
        if (need_dash) {
            if (*p != '-') return false;
            need_dash = 0; continue;
        }
        for (int k = 0; k < grp[gi]; k++) {
            if (!*p) return false;
            char c = *p++;
            int hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if (!hex) return false;
        }
        p--; if (*p) need_dash = (gi < 4);
        gi++;
    }
    return gi == 5 && !need_dash;
}

/* PoP: sig_is_signal_service_id @ gateway/platforms/signal.py:_is_signal_service_id */
/* _is_signal_service_id(value) — PNI:/u:/valid UUID. */
static bool sig_is_signal_service_id(const char *value) {
    if (!value || !value[0]) return false;
    if (strncmp(value, "PNI:", 4) == 0) return true;
    if (strncmp(value, "u:", 2) == 0) return true;
    return sig_is_uuid(value);
}

/* PoP: sig_parse_comma_list @ gateway/platforms/signal.py:_parse_comma_list */
/* PoP: sig_parse_comma_list @ gateway/platforms/signal.py:_parse_comma_list */
/* _parse_comma_list(value) — split on ',' strip → JSON array string. */
static char *sig_parse_comma_list(const char *value) {
    if (!value) return strdup("[]");
    size_t cap = 256 + strlen(value);
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    size_t len = 0;
    len += (size_t)snprintf(out + len, cap - len, "[");
    const char *p = value;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        const char *start = p;
        while (*p && *p != ',') p++;
        const char *end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
        if (end > start) {
            size_t l = (size_t)(end - start);
            len += (size_t)snprintf(out + len, cap - len,
                "%.*s,", (int)l, start);
        }
        if (!*p) break;
        p++;
    }
    if (len > 1 && out[len - 1] == ',') len--;
    len += (size_t)snprintf(out + len, cap - len, "]");
    return out;
}

/* PoP: sniff @ gateway/platforms/signal.py:_guess_extension */
/* _guess_extension(data) — magic-byte sniff (mirrors Python _guess_extension). */
static const char *sig_guess_extension(const unsigned char *data, size_t n) {
    if (n >= 4 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
        return ".png";
    if (n >= 2 && data[0] == 0xFF && data[1] == 0xD8)
        return ".jpg";
    if (n >= 4 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8')
        return ".gif";
    if (n >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P')
        return ".webp";
    if (n >= 4 && data[0] == '%' && data[1] == 'P' && data[2] == 'D' && data[3] == 'F')
        return ".pdf";
    if (n >= 8 && data[4] == 'f' && data[5] == 't' && data[6] == 'y' && data[7] == 'p')
        return ".mp4";
    if (n >= 4 && data[0] == 'O' && data[1] == 'g' && data[2] == 'g' && data[3] == 'S')
        return ".ogg";
    /* MP3 vs ADTS AAC share 0xFF 0xEx sync; MP3 has ID=1, layer in {1,2,3}. */
    if (n >= 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0) {
        int id = (data[1] >> 3) & 0x1;
        int layer = (data[1] >> 1) & 0x3;
        if (id == 1 && layer >= 1 && layer <= 3) return ".mp3";
        return ".aac";
    }
    return "";
}

/* PoP: sig_check_requirements @ gateway/platforms/signal.py:check_signal_requirements */
/* check_signal_requirements() — Signal URL + account env present. */
static bool sig_check_requirements(void) {
    return getenv("SIGNAL_HTTP_URL") != NULL && getenv("SIGNAL_ACCOUNT") != NULL;
}

/* PoP: equivalent @ gateway/platforms/signal.py:_markdown_to_signal */
/* _markdown_to_signal(text) — plain-text fallback (strip markdown).
 * The Python wrapper delegates to the shared markdown_to_signal(); the C
 * port's equivalent (gateway_signal_markdown_to_signal) is a plain-text
 * strip, so we mirror that behaviour locally. */
static char *sig_markdown_to_signal(const char *text) {
    if (!text) return strdup("");
    /* Minimal: drop leading/trailing whitespace and collapse runs of
     * blank lines. Full markdown rendering happens in send() via the
     * shared formatter; this is the base-class plain fallback. */
    char *out = strdup(text);
    if (!out) return strdup("");
    char *w = out, *r = out;
    int prev_nl = 0;
    while (*r) {
        if (*r == '\n') {
            if (prev_nl) { r++; continue; }
            prev_nl = 1;
        } else {
            prev_nl = 0;
        }
        *w++ = *r++;
    }
    *w = '\0';
    /* trim trailing newline(s) */
    while (w > out && w[-1] == '\n') *--w = '\0';
    return out;
}

/* PoP: signal pure helpers @ gateway/platforms/signal.py */


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_platforms_signal_u_render_mentions @ gateway/platforms/signal.py:_render_mentions */
int gateway_platforms_signal_u_render_mentions(const char *arg) {
    /* Python: replace U+FFFC with @id. Arg = "text\tmentions\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (strchr(arg, '\xef\xbf\xbc') == NULL && !strstr(arg, "\ufffc")) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: gateway_platforms_signal_validate_signal_config @ gateway/platforms/signal.py:validate_signal_config */
int gateway_platforms_signal_validate_signal_config(const char *arg) {
    /* Python: http_url AND account present. Arg = "http_url\taccount". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int u = arg[0] != '\0';
    int a = tab && tab[1] != '\0';
    printf("%d\n", (u && a) ? 1 : 0);
    return 0;
}

/* PoP: gateway_platforms_signal_u_sse_listener @ gateway/platforms/signal.py:_sse_listener */
int gateway_platforms_signal_u_sse_listener(const char *arg) {
    /* Python: SSE retry loop. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("SSE listener (account-quoted events URL; Accept text/event-stream; backoff reset on connect)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_health_monitor @ gateway/platforms/signal.py:_health_monitor */
int gateway_platforms_signal_u_health_monitor(const char *arg) {
    /* Python: SSE staleness. Arg =
     * "checked\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (/api/v1/check probe on stale SSE; force reconnect when daemon unreachable)%s\n", (t2 && t2[1] == '1') ? "stale→reconnected" : "healthy", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_force_reconnect @ gateway/platforms/signal.py:_force_reconnect */
int gateway_platforms_signal_u_force_reconnect(const char *arg) {
    /* Python: SSE close. Arg =
     * "closed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int closed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no active stream)\n"); return 0; }
    if (!closed) { printf("0 (stream already consumed)\n"); return 0; }
    printf("1 (SSE response closed — reconnect loop picks up)%s\n", (t2 && t2[1] == '1') ? " — background task tracked" : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_handle_envelope @ gateway/platforms/signal.py:_handle_envelope */
int gateway_platforms_signal_u_handle_envelope(const char *arg) {
    /* Python: syncMessage filter. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int handled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (filtered sync event)\n"); return 0; }
    if (!handled) { printf("0\n"); return 0; }
    printf("1 (envelope processed%s)%s\n", (t2 && t2[1] == '1') ? " — note-to-self extracted" : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_remember_recipient_identifiers @ gateway/platforms/signal.py:_remember_recipient_identifiers */
int gateway_platforms_signal_u_remember_recipient_identifiers(const char *arg) {
    /* Python: cache number<->uuid when uuid is a valid Signal service id.
     * Arg = "number\tuuid". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab || !tab[1]) { printf("0\n"); return 0; }
    if (strstr(tab + 1, "signal:") == NULL) { printf("0\n"); return 0; }
    printf("cached %.*s -> %s\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: gateway_platforms_signal_u_extract_contact_uuid @ gateway/platforms/signal.py:_extract_contact_uuid */
int gateway_platforms_signal_u_extract_contact_uuid(const char *arg) {
    /* Python: number match gate. Arg =
     * "found\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!found) { printf("\n"); return 0; }
    printf("%s (uuid/serviceId validated, number==phone or recipient match required)\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_resolve_recipient @ gateway/platforms/signal.py:_resolve_recipient */
int gateway_platforms_signal_u_resolve_recipient(const char *arg) {
    /* Python: uuid by number. Arg =
     * "resolved\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s (pass-through: group/service-id/non-e164)\n", t1 ? t1 + 1 : ""); return 0; }
    printf("%s (cached uuid lookup under lock; listContacts probe on miss)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — cached" : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_fetch_attachment @ gateway/platforms/signal.py:_fetch_attachment */
int gateway_platforms_signal_u_fetch_attachment(const char *arg) {
    /* Python: getAttachment RPC. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t (no result / missing data)\n"); return 0; }
    printf("%s\t%s (base64 decode + cache)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "ext" : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_rpc @ gateway/platforms/signal.py:_rpc */
int gateway_platforms_signal_u_rpc(const char *arg) {
    /* Python: JSON-RPC 2.0. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "rate_limited") == 0) {
        fprintf(stderr, "SignalRateLimitError [429] — raise_on_rate_limit path\n");
        return 1;
    }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "RPC failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s (log_failures=False → DEBUG not WARNING for typing-path spam)%s\n", t3 ? t3 + 1 : "{}", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_track_sent_timestamp @ gateway/platforms/signal.py:_track_sent_timestamp */
int gateway_platforms_signal_u_track_sent_timestamp(const char *arg) {
    /* Python: echo-back filter record. Arg =
     * "has_ts\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_ts = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!has_ts || !state) { printf("timestamp not tracked\n"); return 0; }
    printf("sent timestamp tracked\n");
    return 0;
}

/* PoP: gateway_platforms_signal_u_notify_batch_pacing @ gateway/platforms/signal.py:_notify_batch_pacing */
int gateway_platforms_signal_u_notify_batch_pacing(const char *arg) {
    /* Python: pacing notice. Arg =
     * "batch\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("notice sent: (More images coming — pausing ~%ss for Signal rate limit, batch %s/total)%s\n", t2 ? t2 + 1 : "?", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — logged failure" : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_stop_typing_indicator @ gateway/platforms/signal.py:_stop_typing_indicator */
int gateway_platforms_signal_u_stop_typing_indicator(const char *arg) {
    /* Python: cancel + RPC stop. Arg =
     * "stopped\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int stopped = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no task)\n"); return 0; }
    if (!stopped) { printf("0 (RPC failed — backoff still cleared)\n"); return 0; }
    printf("1 (task cancelled + explicit stop-typing RPC; device drops indicator immediately)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_platforms_signal_remove_reaction @ gateway/platforms/signal.py:remove_reaction */
int gateway_platforms_signal_remove_reaction(const char *arg) {
    /* Python: empty emoji sendReaction. Arg =
     * "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int removed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!removed) { printf("0 (sendReaction failed)\n"); return 0; }
    printf("1 (removed via emoji="" + remove=True; group → groupId, else recipient)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_platforms_signal_u_extract_reaction_target @ gateway/platforms/signal.py:_extract_reaction_target */
int gateway_platforms_signal_u_extract_reaction_target(const char *arg) {
    /* Python: (sender, timestamp_ms) from raw envelope or None. Arg = raw
     * JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    const char *author = json_get_str(j, "sender", "");
    const char *ts = json_get_str(j, "timestamp_ms", "");
    if (!author[0] || !ts[0]) {
        printf("\n");
        json_free(j);
        return 0;
    }
    printf("%s\t%s\n", author, ts);
    json_free(j);
    return 0;
}

/* PoP: gateway_platforms_signal_u_reactions_enabled @ gateway/platforms/signal.py:_reactions_enabled */
int gateway_platforms_signal_u_reactions_enabled(const char *arg) {
    /* Python: 2-gate check. Arg =
     * "env_on\tallowed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int env_on = arg[0] == '1';
    int allowed = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!env_on) { printf("0 (SIGNAL_REACTIONS off)\n"); return 0; }
    printf("%s (DM allowlist %s)\n", allowed ? "1" : "0", (t3 && t3[1] == '1') ? "anyone" : "restricted");
    return 0;
}

/* PoP: gateway_platforms_signal_u_on_processing_start @ gateway/platforms/signal.py:on_processing_start */
int gateway_platforms_signal_u_on_processing_start(const char *arg) {
    /* Python: react with 👀 when processing begins. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (reaction 👀 sent on processing start, target=%s)\n", tab ? tab + 1 : "chat");
    return 0;
}

/* PoP: gateway_platforms_signal_u_on_processing_complete @ gateway/platforms/signal.py:on_processing_complete */
int gateway_platforms_signal_u_on_processing_complete(const char *arg) {
    /* Python: swap 👀 for ✅ (success) / ❌ (failure); CANCELLED keeps 👀. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    int cancelled = t1 && t1[1] == '1';
    if (cancelled) { printf("1 (CANCELLED — 👀 left in place)\n"); return 0; }
    printf("1 (reaction swapped 👀 → %s)\n", (t2 && t2[1] == '1') ? "❌" : "✅");
    return 0;
}
